// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "heifsequencereader.h"

#include "animationtiming.h"
#include "cache/imagebyteaccounting.h"
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

std::optional<kiriview::HeifSequenceWorkspacePlan> heifSequenceWorkspacePlanForSize(QSize imageSize)
{
    const std::optional<qsizetype> outputByteCount
        = kiriview::checkedImageDecodeWorkspaceByteCount(imageSize, 4, 1);
    const std::optional<qsizetype> decoderCanvasByteCount
        = kiriview::checkedImageDecodeWorkspaceByteCount(imageSize, 4, 8);
    if (!outputByteCount.has_value() || !decoderCanvasByteCount.has_value()) {
        return std::nullopt;
    }

    const qsizetype decoderByteLimit
        = std::max(kiriview::heifSequenceProbeWorkspaceByteCount, *decoderCanvasByteCount);
    constexpr qsizetype maximum = std::numeric_limits<qsizetype>::max();
    if (*outputByteCount > maximum - decoderByteLimit) {
        return std::nullopt;
    }
    return kiriview::HeifSequenceWorkspacePlan {
        imageSize,
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

kiriview::HeifSequenceWorkspacePlanResult inspectHeifSequence(const QByteArray& data)
{
    if (!kiriview::isLikelyHeifContainer(data)) {
        return { kiriview::HeifSequenceOpenStatus::NotHeif, {}, {} };
    }

    QString errorString;
    bool contextResourceLimitExceeded = false;
    std::optional<kiriview::HeifContext> context = kiriview::openHeifContext(data, &errorString,
        kiriview::HeifContextOpenLimits {
            static_cast<std::uint64_t>(kiriview::heifSequenceProbeWorkspaceByteCount),
        },
        &contextResourceLimitExceeded);
    if (!context.has_value()) {
        return { contextResourceLimitExceeded
                ? kiriview::HeifSequenceOpenStatus::ResourceLimitExceeded
                : kiriview::HeifSequenceOpenStatus::Error,
            {},
            contextResourceLimitExceeded ? kiriview::imageDecodeWorkspaceResourceLimitDiagnostic()
                                         : std::move(errorString) };
    }
    if (!heif_context_has_sequence(context->get())) {
        return { kiriview::HeifSequenceOpenStatus::NotSequence, {}, {} };
    }

    kiriview::HeifTrack track(heif_context_get_track(context->get(), 0));
    if (track.get() == nullptr) {
        return { kiriview::HeifSequenceOpenStatus::Error, {},
            kiriview::imageErrorText(kiriview::ImageErrorTextId::HeifSequenceTrackMissing) };
    }
    if (heif_track_get_track_handler_type(track.get()) != heif_track_type_image_sequence) {
        return { kiriview::HeifSequenceOpenStatus::NotSequence, {}, {} };
    }

    std::uint16_t width = 0;
    std::uint16_t height = 0;
    const heif_error resolutionError
        = heif_track_get_image_resolution(track.get(), &width, &height);
    if (resolutionError.code != heif_error_Ok) {
        return { kiriview::HeifSequenceOpenStatus::Error, {},
            kiriview::heifErrorString(kiriview::imageErrorActionText(
                                          kiriview::ImageErrorActionTextId::DecodeHeifSequence),
                resolutionError) };
    }

    const std::optional<kiriview::HeifSequenceWorkspacePlan> plan
        = heifSequenceWorkspacePlanForSize(QSize(width, height));
    if (!plan.has_value()) {
        return { kiriview::HeifSequenceOpenStatus::ResourceLimitExceeded, {},
            kiriview::imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    return { kiriview::HeifSequenceOpenStatus::Success, *plan, {} };
}
}

namespace kiriview {
std::optional<HeifSequenceWorkspacePlan> heifSequenceWorkspacePlan(QSize imageSize)
{
    return heifSequenceWorkspacePlanForSize(imageSize);
}

QString heifSequenceDecodeErrorString()
{
    return imageErrorText(ImageErrorTextId::DecodeHeifSequence);
}

class HeifSequenceReader::Private
{
public:
    explicit Private(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        qsizetype perOperationBaselineByteCount)
        : workspaceBudget(workspaceBudget != nullptr ? std::move(workspaceBudget)
                                                     : defaultImageDecodeWorkspaceBudget())
        , perOperationBaselineByteCount(perOperationBaselineByteCount)
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
    qsizetype perOperationBaselineByteCount = 0;
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
    : HeifSequenceReader(defaultImageDecodeWorkspaceBudget(), 0)
{
}

HeifSequenceReader::HeifSequenceReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    qsizetype perOperationBaselineByteCount)
    : d(std::make_unique<Private>(std::move(workspaceBudget), perOperationBaselineByteCount))
{
}

HeifSequenceReader::~HeifSequenceReader() = default;

HeifSequenceReader::HeifSequenceReader(HeifSequenceReader&&) noexcept = default;

HeifSequenceReader& HeifSequenceReader::operator=(HeifSequenceReader&&) noexcept = default;

HeifSequenceWorkspacePlanResult planHeifSequenceOpen(const QByteArray& data,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    qsizetype perOperationBaselineByteCount)
{
    if (workspaceBudget == nullptr) {
        workspaceBudget = defaultImageDecodeWorkspaceBudget();
    }
    ImageDecodeWorkspaceLease probeWorkspace = ImageDecodeWorkspaceDetail::startLeaseForOperation(
        *workspaceBudget, perOperationBaselineByteCount);
    if (!ImageDecodeWorkspaceDetail::tryReserve(
            probeWorkspace, heifSequenceProbeWorkspaceByteCount)) {
        return { HeifSequenceOpenStatus::ResourceLimitExceeded, {},
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    return inspectHeifSequence(data);
}

HeifSequenceOpenResult HeifSequenceReader::open(QByteArray data)
{
    const HeifSequenceWorkspacePlanResult planning
        = planHeifSequenceOpen(data, d->workspaceBudget, d->perOperationBaselineByteCount);
    if (planning.status != HeifSequenceOpenStatus::Success) {
        close();
        d->lastResourceLimitExceeded
            = planning.status == HeifSequenceOpenStatus::ResourceLimitExceeded;
        return { planning.status, planning.errorString };
    }
    return open(std::move(data), planning.plan);
}

HeifSequenceOpenResult HeifSequenceReader::open(
    QByteArray data, const HeifSequenceWorkspacePlan& plan)
{
    close();
    d->lastResourceLimitExceeded = false;
    const std::optional<HeifSequenceWorkspacePlan> validatedPlan
        = heifSequenceWorkspacePlan(plan.imageSize);
    if (!validatedPlan.has_value() || validatedPlan->transientByteCount != plan.transientByteCount
        || validatedPlan->outputByteCount != plan.outputByteCount
        || validatedPlan->decoderByteLimit != plan.decoderByteLimit
        || validatedPlan->pixelLimit != plan.pixelLimit) {
        d->lastResourceLimitExceeded = true;
        return { HeifSequenceOpenStatus::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }

    d->transientWorkspaceLease = ImageDecodeWorkspaceDetail::startLeaseForOperation(
        *d->workspaceBudget, d->perOperationBaselineByteCount);
    if (!ImageDecodeWorkspaceDetail::tryReserve(
            d->transientWorkspaceLease, plan.transientByteCount)) {
        close();
        d->lastResourceLimitExceeded = true;
        return { HeifSequenceOpenStatus::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }

    d->data = std::move(data);
    QString errorString;
    bool contextResourceLimitExceeded = false;
    HeifContextInheritedLimits inheritedLimits;
    d->context
        = openHeifContext(d->data, &errorString, HeifContextOpenLimits { plan.decoderByteLimit },
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
    if (QSize(width, height) != plan.imageSize) {
        close();
        d->lastResourceLimitExceeded = true;
        return { HeifSequenceOpenStatus::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    d->outputByteCount = plan.outputByteCount;

    heif_security_limits limits = *heif_context_get_security_limits(d->context->get());
    limits.max_image_size_pixels = limits.max_image_size_pixels == 0
        ? plan.pixelLimit
        : std::min(limits.max_image_size_pixels, plan.pixelLimit);
    limits.max_total_memory = d->inheritedMaximumTotalMemory == 0
        ? plan.decoderByteLimit
        : std::min(d->inheritedMaximumTotalMemory, plan.decoderByteLimit);
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

AnimationFrameReadResult HeifSequenceReader::readNextFrame() { return readNextFrame({}); }

AnimationFrameReadResult HeifSequenceReader::readNextFrame(
    const std::shared_ptr<ImageDecodeWorkspaceBudget>& outputWorkspaceBudget)
{
    d->lastResourceLimitExceeded = false;
    if (d->track.get() == nullptr) {
        return std::unexpected(imageErrorText(ImageErrorTextId::HeifSequenceTrackMissing));
    }

    const qsizetype outputBaselineByteCount = saturatedQtByteSum(
        d->perOperationBaselineByteCount, d->transientWorkspaceLease.reservedByteCount());
    const std::shared_ptr<ImageDecodeWorkspaceBudget>& budget
        = outputWorkspaceBudget != nullptr ? outputWorkspaceBudget : d->workspaceBudget;
    ImageDecodeWorkspaceLease outputLease
        = ImageDecodeWorkspaceDetail::startLeaseForOperation(*budget, outputBaselineByteCount);
    if (!ImageDecodeWorkspaceDetail::tryReserve(outputLease, d->outputByteCount)) {
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
