// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heifsequencereader.h"

#include "animationtiming.h"
#include "heifcontainer.h"
#include "heifsupport.h"
#include "localization/imageerrortext.h"

#include <libheif/heif_sequences.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace {
int animationRepeatCount(std::uint32_t repetitions)
{
    if (repetitions == heif_sequence_track_number_of_repetitions_infinite) {
        return -1;
    }
    if (repetitions <= 1) {
        return 0;
    }
    return static_cast<int>(
        std::min(repetitions - 1, static_cast<std::uint32_t>(std::numeric_limits<int>::max())));
}

struct HeifSequenceWorkspacePlan
{
    qsizetype transientByteCount = 0;
    qsizetype outputByteCount = 0;
    std::uint64_t decoderByteLimit = 0;
    std::uint64_t pixelLimit = 0;
};

constexpr qsizetype minimumHeifDecoderByteLimit = qsizetype { 8 } * 1024 * 1024;

std::optional<HeifSequenceWorkspacePlan> heifSequenceWorkspacePlan(QSize imageSize)
{
    const std::optional<qsizetype> outputByteCount
        = kiriview::checkedImageDecodeWorkspaceByteCount(imageSize, 4, 1);
    const std::optional<qsizetype> decoderCanvasByteCount
        = kiriview::checkedImageDecodeWorkspaceByteCount(imageSize, 4, 8);
    if (!outputByteCount.has_value() || !decoderCanvasByteCount.has_value()) {
        return std::nullopt;
    }

    const qsizetype decoderByteLimit
        = std::max(minimumHeifDecoderByteLimit, *decoderCanvasByteCount);
    constexpr qsizetype maximum = std::numeric_limits<qsizetype>::max();
    if (*outputByteCount > maximum - decoderByteLimit) {
        return std::nullopt;
    }
    return HeifSequenceWorkspacePlan {
        decoderByteLimit + *outputByteCount,
        *outputByteCount,
        static_cast<std::uint64_t>(decoderByteLimit),
        static_cast<std::uint64_t>(imageSize.width())
            * static_cast<std::uint64_t>(imageSize.height()),
    };
}

bool isHeifResourceFailure(heif_error error)
{
    return error.code == heif_error_Memory_allocation_error
        || error.subcode == heif_suberror_Security_limit_exceeded;
}
}

namespace kiriview {
QString heifSequenceDecodeErrorString()
{
    return imageErrorText(ImageErrorTextId::DecodeHeifSequence);
}

class HeifSequenceReader::Private
{
public:
    explicit Private(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
        : workspaceBudget(workspaceBudget != nullptr ? std::move(workspaceBudget)
                                                     : defaultImageDecodeWorkspaceBudget())
    {
    }
    ~Private() { reset(); }
    Q_DISABLE_COPY_MOVE(Private)

    void reset()
    {
        options.reset();
        track = HeifTrack();
        context.reset();
        data.clear();
        timescale = 0;
        outputByteCount = 0;
        inheritedMaximumTotalMemory = 0;
        transientWorkspaceLease = {};
    }

    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget;
    ImageDecodeWorkspaceLease transientWorkspaceLease;
    qsizetype outputByteCount = 0;
    std::uint64_t inheritedMaximumTotalMemory = 0;
    bool lastResourceLimitExceeded = false;
    QByteArray data;
    std::optional<HeifContext> context;
    HeifTrack track;
    std::optional<HeifDecodingOptions> options;
    uint32_t timescale = 0;
};

HeifSequenceReader::HeifSequenceReader()
    : HeifSequenceReader(defaultImageDecodeWorkspaceBudget())
{
}

HeifSequenceReader::HeifSequenceReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
    : d(std::make_unique<Private>(std::move(workspaceBudget)))
{
}

HeifSequenceReader::~HeifSequenceReader() = default;

HeifSequenceReader::HeifSequenceReader(HeifSequenceReader&&) noexcept = default;

HeifSequenceReader& HeifSequenceReader::operator=(HeifSequenceReader&&) noexcept = default;

HeifSequenceOpenResult HeifSequenceReader::open(QByteArray data)
{
    close();
    d->lastResourceLimitExceeded = false;

    if (!isLikelyHeifContainer(data)) {
        return { HeifSequenceOpenStatus::NotHeif, {} };
    }

    d->transientWorkspaceLease = d->workspaceBudget->startLease();
    if (!d->transientWorkspaceLease.tryReserve(minimumHeifDecoderByteLimit)) {
        close();
        d->lastResourceLimitExceeded = true;
        return { HeifSequenceOpenStatus::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }

    d->data = std::move(data);
    QString errorString;
    bool contextResourceLimitExceeded = false;
    HeifContextInheritedLimits inheritedLimits;
    d->context = openHeifContext(d->data, &errorString,
        HeifContextOpenLimits { static_cast<std::uint64_t>(minimumHeifDecoderByteLimit) },
        &contextResourceLimitExceeded, &inheritedLimits);
    if (!d->context.has_value()) {
        close();
        d->lastResourceLimitExceeded = contextResourceLimitExceeded;
        return { contextResourceLimitExceeded ? HeifSequenceOpenStatus::ResourceLimitExceeded
                                              : HeifSequenceOpenStatus::Error,
            contextResourceLimitExceeded ? imageDecodeWorkspaceResourceLimitDiagnostic()
                                         : errorString };
    }
    d->inheritedMaximumTotalMemory = inheritedLimits.maximumTotalMemory;

    if (!heif_context_has_sequence(d->context->get())) {
        close();
        return { HeifSequenceOpenStatus::NotSequence, {} };
    }

    d->track = HeifTrack(heif_context_get_track(d->context->get(), 0));
    if (d->track.get() == nullptr) {
        close();
        return { HeifSequenceOpenStatus::Error,
            imageErrorText(ImageErrorTextId::HeifSequenceTrackMissing) };
    }

    if (heif_track_get_track_handler_type(d->track.get()) != heif_track_type_image_sequence) {
        close();
        return { HeifSequenceOpenStatus::NotSequence, {} };
    }

    std::uint16_t width = 0;
    std::uint16_t height = 0;
    const heif_error resolutionError
        = heif_track_get_image_resolution(d->track.get(), &width, &height);
    if (resolutionError.code != heif_error_Ok) {
        close();
        return { HeifSequenceOpenStatus::Error,
            heifErrorString(imageErrorActionText(ImageErrorActionTextId::DecodeHeifSequence),
                resolutionError) };
    }
    const std::optional<HeifSequenceWorkspacePlan> workspacePlan
        = heifSequenceWorkspacePlan(QSize(width, height));
    if (!workspacePlan.has_value()
        || workspacePlan->transientByteCount < minimumHeifDecoderByteLimit
        || !d->transientWorkspaceLease.tryReserve(
            workspacePlan->transientByteCount - minimumHeifDecoderByteLimit)) {
        close();
        d->lastResourceLimitExceeded = true;
        return { HeifSequenceOpenStatus::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    d->outputByteCount = workspacePlan->outputByteCount;

    heif_security_limits limits = *heif_context_get_security_limits(d->context->get());
    limits.max_image_size_pixels = limits.max_image_size_pixels == 0
        ? workspacePlan->pixelLimit
        : std::min(limits.max_image_size_pixels, workspacePlan->pixelLimit);
    limits.max_total_memory = d->inheritedMaximumTotalMemory == 0
        ? workspacePlan->decoderByteLimit
        : std::min(d->inheritedMaximumTotalMemory, workspacePlan->decoderByteLimit);
    const heif_error limitsError = heif_context_set_security_limits(d->context->get(), &limits);
    if (limitsError.code != heif_error_Ok) {
        const bool resourceLimitExceeded = isHeifResourceFailure(limitsError);
        close();
        d->lastResourceLimitExceeded = resourceLimitExceeded;
        return { resourceLimitExceeded ? HeifSequenceOpenStatus::ResourceLimitExceeded
                                       : HeifSequenceOpenStatus::Error,
            resourceLimitExceeded
                ? imageDecodeWorkspaceResourceLimitDiagnostic()
                : heifErrorString(imageErrorActionText(ImageErrorActionTextId::DecodeHeifSequence),
                      limitsError) };
    }

    d->options.emplace();
    if (d->options->get() == nullptr) {
        close();
        d->lastResourceLimitExceeded = true;
        return { HeifSequenceOpenStatus::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    d->options->setIgnoreSequenceEditList(true);
    d->timescale = heif_track_get_timescale(d->track.get());

    return { HeifSequenceOpenStatus::Success, {},
        animationRepeatCount(heif_track_get_number_of_repetitions(d->track.get())) };
}

AnimationFrameReadResult HeifSequenceReader::readNextFrame()
{
    d->lastResourceLimitExceeded = false;
    if (d->track.get() == nullptr) {
        return std::unexpected(imageErrorText(ImageErrorTextId::HeifSequenceTrackMissing));
    }

    ImageDecodeWorkspaceLease outputLease = d->workspaceBudget->startLeaseForOperation(
        d->transientWorkspaceLease.reservedByteCount());
    if (!outputLease.tryReserve(d->outputByteCount)) {
        close();
        d->lastResourceLimitExceeded = true;
        return std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic());
    }

    HeifImage heifImage;
    const heif_error error = heif_track_decode_next_image(d->track.get(), heifImage.out(),
        heif_colorspace_RGB, heif_chroma_interleaved_RGBA, d->options->get());
    if (error.code == heif_error_End_of_sequence) {
        return std::optional<AnimationFrame>();
    }
    if (error.code != heif_error_Ok) {
        if (isHeifResourceFailure(error)) {
            heifImage = HeifImage();
            close();
            d->lastResourceLimitExceeded = true;
            return std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic());
        }
        return std::unexpected(heifErrorString(
            imageErrorActionText(ImageErrorActionTextId::DecodeHeifSequence), error));
    }

    QString conversionError;
    HeifImageConversionFailureCause conversionFailureCause
        = HeifImageConversionFailureCause::Invalid;
    std::optional<QImage> image
        = qImageFromHeifImage(heifImage.get(), &conversionError, &conversionFailureCause);
    if (!image.has_value()) {
        const bool resourceLimitExceeded
            = conversionFailureCause == HeifImageConversionFailureCause::ResourceLimitExceeded;
        if (resourceLimitExceeded) {
            heifImage = HeifImage();
            close();
        }
        d->lastResourceLimitExceeded = resourceLimitExceeded;
        return std::unexpected(conversionError);
    }

    return std::optional<AnimationFrame>(AnimationFrame {
        std::move(*image),
        heifFrameDelay(heif_image_get_duration(heifImage.get()), d->timescale),
        outputLease.sharedHold(),
    });
}

bool HeifSequenceReader::lastReadResourceLimitExceeded() const
{
    return d->lastResourceLimitExceeded;
}

void HeifSequenceReader::close()
{
    if (d != nullptr) {
        (*d).reset();
    }
}
}
