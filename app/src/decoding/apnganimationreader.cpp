// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "apnganimationreader.h"

#include "animationtiming.h"
#include "apngframecomposer.h"
#include "bridge/rustqtconversion.h"
#include "kiriview/src/support/apnganimationreader.cxx.h"
#include "localization/imageerrortext.h"

#include <QSize>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <utility>

namespace {
QString apngDecodeErrorString()
{
    return kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeApngAnimation);
}

kiriview::ApngOpenResult errorResult(QString errorString)
{
    kiriview::ApngOpenResult result;
    result.status = kiriview::ApngOpenStatus::Error;
    result.errorString = std::move(errorString);
    return result;
}

kiriview::ApngOpenResult resourceLimitResult()
{
    kiriview::ApngOpenResult result;
    result.status = kiriview::ApngOpenStatus::ResourceLimitExceeded;
    result.errorString = kiriview::imageDecodeWorkspaceResourceLimitDiagnostic();
    return result;
}

constexpr std::size_t apngDecoderInternalByteLimit = std::size_t { 64 } * 1024 * 1024;

std::optional<kiriview::ApngAnimationWorkspacePlan> apngWorkspacePlan(quint32 width, quint32 height,
    qsizetype inputByteCount, std::size_t decoderInternalByteReservation)
{
    constexpr quint64 bytesPerPixel = kiriview::ApngRgbaBuffer::bytesPerPixel;
    // Composer canvas and frame, Rust output, and dispose-previous backup. The
    // returned QImage is admitted independently for every frame.
    constexpr quint64 worstCaseCanvasCount = 4;
    constexpr quint64 byteLimit = static_cast<quint64>(std::numeric_limits<qsizetype>::max());

    if (inputByteCount < 0 || width == 0 || height == 0
        || static_cast<quint64>(width) > byteLimit / static_cast<quint64>(height)) {
        return std::nullopt;
    }
    const quint64 pixelCount = static_cast<quint64>(width) * static_cast<quint64>(height);
    if (pixelCount > byteLimit / bytesPerPixel) {
        return std::nullopt;
    }
    const quint64 canvasByteCount = pixelCount * bytesPerPixel;
    if (canvasByteCount > byteLimit / worstCaseCanvasCount) {
        return std::nullopt;
    }
    quint64 worstCaseByteCount = canvasByteCount * worstCaseCanvasCount;
    const quint64 inputBytes = static_cast<quint64>(inputByteCount);
    if (inputBytes > byteLimit - worstCaseByteCount) {
        return std::nullopt;
    }
    worstCaseByteCount += inputBytes;
    const quint64 decoderInternalBytes = static_cast<quint64>(decoderInternalByteReservation);
    if (decoderInternalBytes > byteLimit - worstCaseByteCount) {
        return std::nullopt;
    }
    worstCaseByteCount += decoderInternalBytes;

    return kiriview::ApngAnimationWorkspacePlan {
        QSize(static_cast<int>(width), static_cast<int>(height)),
        static_cast<qsizetype>(worstCaseByteCount),
        static_cast<qsizetype>(canvasByteCount),
        apngDecoderInternalByteLimit,
    };
}

kiriview::ApngFrameDisposeOp disposeOpFromRust(kiriview::RustApngDisposeOp disposeOp)
{
    switch (disposeOp) {
    case kiriview::RustApngDisposeOp::Background:
        return kiriview::ApngFrameDisposeOp::Background;
    case kiriview::RustApngDisposeOp::Previous:
        return kiriview::ApngFrameDisposeOp::Previous;
    case kiriview::RustApngDisposeOp::None:
    default:
        return kiriview::ApngFrameDisposeOp::None;
    }
}

kiriview::ApngFrameBlendOp blendOpFromRust(kiriview::RustApngBlendOp blendOp)
{
    return blendOp == kiriview::RustApngBlendOp::Over ? kiriview::ApngFrameBlendOp::Over
                                                      : kiriview::ApngFrameBlendOp::Source;
}

kiriview::ApngFrameControl frameControlFromRust(const kiriview::RustApngFrameResult& frame)
{
    return kiriview::ApngFrameControl {
        frame.width,
        frame.height,
        frame.x_offset,
        frame.y_offset,
        disposeOpFromRust(frame.dispose_op),
        blendOpFromRust(frame.blend_op),
    };
}

bool canRepresentCanvas(quint32 width, quint32 height)
{
    return width > 0 && height > 0 && width <= static_cast<quint32>(std::numeric_limits<int>::max())
        && height <= static_cast<quint32>(std::numeric_limits<int>::max());
}
}

namespace kiriview {
ApngAnimationWorkspacePlanResult planApngAnimationOpen(const QByteArray& data)
{
    const RustApngProbeResult probeResult
        = rustProbeApngAnimation(Bridge::rustBytes(data), apngDecoderInternalByteLimit);
    switch (probeResult.status) {
    case RustApngProbeStatus::NotApng:
        return {};
    case RustApngProbeStatus::Error:
        return { ApngOpenStatus::Error, {}, apngDecodeErrorString() };
    case RustApngProbeStatus::ResourceLimitExceeded:
        return { ApngOpenStatus::ResourceLimitExceeded, {},
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    case RustApngProbeStatus::Success:
        break;
    }

    if (!canRepresentCanvas(probeResult.canvas_width, probeResult.canvas_height)) {
        return { ApngOpenStatus::ResourceLimitExceeded, {},
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    const std::optional<ApngAnimationWorkspacePlan> plan
        = apngWorkspacePlan(probeResult.canvas_width, probeResult.canvas_height, data.size(),
            probeResult.decoder_workspace_byte_count);
    if (!plan.has_value()) {
        return { ApngOpenStatus::ResourceLimitExceeded, {},
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    return { ApngOpenStatus::Success, *plan, {} };
}

class ApngAnimationReader::Private
{
public:
    explicit Private(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
        : m_workspaceBudget(workspaceBudget != nullptr ? std::move(workspaceBudget)
                                                       : defaultImageDecodeWorkspaceBudget())
    {
    }

    ~Private() { close(); }
    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;

    ApngOpenResult open(const QByteArray& inputData)
    {
        const ApngAnimationWorkspacePlanResult planning = planApngAnimationOpen(inputData);
        if (planning.status != ApngOpenStatus::Success) {
            close();
            m_lastReadResourceLimitExceeded
                = planning.status == ApngOpenStatus::ResourceLimitExceeded;
            ApngOpenResult result;
            result.status = planning.status;
            result.errorString = planning.errorString;
            return result;
        }
        return open(inputData, planning.plan);
    }

    ApngOpenResult open(
        const QByteArray& inputData, const ApngAnimationWorkspacePlan& workspacePlan)
    {
        close();
        m_lastReadResourceLimitExceeded = false;

        if (workspacePlan.canvasSize.isEmpty() || workspacePlan.transientByteCount <= 0
            || workspacePlan.firstFrameOutputByteCount <= 0
            || workspacePlan.decoderInternalByteLimit == 0 || !tryReserveWorkspace(workspacePlan)) {
            close();
            return resourceLimitResult();
        }

        const RustApngOpenResult openResult = rustOpenApngAnimationReader(
            *reader, Bridge::rustBytes(inputData), workspacePlan.decoderInternalByteLimit);
        switch (openResult.status) {
        case RustApngOpenStatus::NotApng:
        case RustApngOpenStatus::Error:
            close();
            return errorResult(apngDecodeErrorString());
        case RustApngOpenStatus::ResourceLimitExceeded:
            close();
            return resourceLimitResult();
        case RustApngOpenStatus::Success:
            break;
        }
        if (std::cmp_not_equal(openResult.canvas_width, workspacePlan.canvasSize.width())
            || std::cmp_not_equal(openResult.canvas_height, workspacePlan.canvasSize.height())) {
            close();
            return errorResult(apngDecodeErrorString());
        }

        bool initialized = false;
        try {
            initialized = composer.initialize(QSize(static_cast<int>(openResult.canvas_width),
                                                  static_cast<int>(openResult.canvas_height)),
                static_cast<std::size_t>(workspacePlan.firstFrameOutputByteCount)
                    / static_cast<std::size_t>(openResult.canvas_height));
        } catch (const std::bad_alloc&) {
            close();
            return resourceLimitResult();
        }
        if (!initialized) {
            close();
            return errorResult(apngDecodeErrorString());
        }

        AnimationFrameReadResult firstFrame = readNextFrame();
        if (!firstFrame || !firstFrame->has_value()) {
            const bool resourceLimitExceeded = m_lastReadResourceLimitExceeded;
            QString errorString = firstFrame ? apngDecodeErrorString() : firstFrame.error();
            close();
            return resourceLimitExceeded ? resourceLimitResult()
                                         : errorResult(std::move(errorString));
        }

        AnimationFrame decodedFirstFrame = std::move(**firstFrame);
        ApngOpenResult result;
        result.status = ApngOpenStatus::Success;
        result.workspaceHold = std::move(decodedFirstFrame.workspaceHold);
        result.firstFrame = std::move(decodedFirstFrame.image);
        result.firstFrameDelay = decodedFirstFrame.delay;
        result.loopCount = openResult.loop_count;
        result.frameCount = openResult.frame_count;
        return result;
    }

    AnimationFrameReadResult readNextFrame() { return readNextFrame({}); }

    AnimationFrameReadResult readNextFrame(
        std::shared_ptr<ImageDecodeWorkspaceBudget> outputWorkspaceBudget)
    {
        m_lastReadResourceLimitExceeded = false;
        try {
            return readNextFrameImpl(std::move(outputWorkspaceBudget));
        } catch (const std::bad_alloc&) {
            m_lastReadResourceLimitExceeded = true;
            close();
            return std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic());
        }
    }

    AnimationFrameReadResult readNextFrameImpl(
        std::shared_ptr<ImageDecodeWorkspaceBudget> outputWorkspaceBudget)
    {
        struct FrameReadAttempt
        {
            AnimationFrameReadResult result;
            bool terminate = false;
            bool resourceLimitExceeded = false;
        };

        FrameReadAttempt attempt = [this, &outputWorkspaceBudget]() -> FrameReadAttempt {
            if (!rustApngAnimationReaderHasMoreFrames(*reader)) {
                return { std::optional<AnimationFrame>(), true, false };
            }

            const std::shared_ptr<ImageDecodeWorkspaceBudget>& budget
                = outputWorkspaceBudget != nullptr ? outputWorkspaceBudget : m_workspaceBudget;
            ImageDecodeWorkspaceLease outputLease
                = ImageDecodeWorkspaceDetail::startLeaseForOperation(
                    *budget, m_transientWorkspaceLease.reservedByteCount());
            if (!ImageDecodeWorkspaceDetail::tryReserve(
                    outputLease, static_cast<qsizetype>(m_canvasByteCount))) {
                return { std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic()), true,
                    true };
            }

            const RustApngFrameResult frame
                = rustReadApngAnimationFrame(*reader, m_canvasByteCount);
            switch (frame.status) {
            case RustApngReadStatus::End:
                return { std::optional<AnimationFrame>(), true, false };
            case RustApngReadStatus::ResourceLimitExceeded:
                return { std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic()), true,
                    true };
            case RustApngReadStatus::Error:
                return { std::unexpected(apngDecodeErrorString()), true, false };
            case RustApngReadStatus::Frame:
                break;
            }

            const ApngFrameControl control = frameControlFromRust(frame);
            if (!composer.setFrameBytes(control,
                    std::span(frame.pixels.data(), frame.pixels.size()), frame.row_bytes)) {
                return { std::unexpected(apngDecodeErrorString()), true, false };
            }

            std::optional<QImage> image = composer.composeFrame(control);
            if (!image.has_value()) {
                return { std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic()), true,
                    true };
            }

            return { std::optional<AnimationFrame>(AnimationFrame {
                         std::move(*image),
                         apngFrameDelay(frame.delay_num, frame.delay_den),
                         outputLease.sharedHold(),
                     }),
                false, false };
        }();

        if (attempt.terminate) {
            m_lastReadResourceLimitExceeded = attempt.resourceLimitExceeded;
            close();
        }
        return std::move(attempt.result);
    }

    [[nodiscard]] bool hasMoreFrames() const
    {
        return rustApngAnimationReaderHasMoreFrames(*reader);
    }

    [[nodiscard]] bool lastReadResourceLimitExceeded() const
    {
        return m_lastReadResourceLimitExceeded;
    }

    void close()
    {
        composer.clear();
        reader = rustNewApngAnimationReader();
        m_canvasByteCount = 0;
        m_transientWorkspaceLease = {};
    }

private:
    bool tryReserveWorkspace(const ApngAnimationWorkspacePlan& plan)
    {
        m_transientWorkspaceLease = ImageDecodeWorkspaceDetail::startLease(*m_workspaceBudget);
        if (!m_transientWorkspaceLease.isManaged()) {
            return false;
        }

        if (!ImageDecodeWorkspaceDetail::tryReserve(
                m_transientWorkspaceLease, plan.transientByteCount)) {
            return false;
        }

        m_canvasByteCount = static_cast<std::size_t>(plan.firstFrameOutputByteCount);
        return true;
    }

    std::shared_ptr<ImageDecodeWorkspaceBudget> m_workspaceBudget;
    ImageDecodeWorkspaceLease m_transientWorkspaceLease;
    rust::Box<RustApngAnimationReader> reader = rustNewApngAnimationReader();
    ApngFrameComposer composer;
    std::size_t m_canvasByteCount = 0;
    bool m_lastReadResourceLimitExceeded = false;
};

ApngAnimationReader::ApngAnimationReader()
    : ApngAnimationReader(defaultImageDecodeWorkspaceBudget())
{
}

ApngAnimationReader::ApngAnimationReader(
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
    : d(std::make_unique<Private>(std::move(workspaceBudget)))
{
}

ApngAnimationReader::~ApngAnimationReader() = default;

ApngOpenResult ApngAnimationReader::open(const QByteArray& data) { return d->open(data); }

ApngOpenResult ApngAnimationReader::open(
    const QByteArray& data, const ApngAnimationWorkspacePlan& plan)
{
    return d->open(data, plan);
}

AnimationFrameReadResult ApngAnimationReader::readNextFrame() { return d->readNextFrame(); }

AnimationFrameReadResult ApngAnimationReader::readNextFrame(
    std::shared_ptr<ImageDecodeWorkspaceBudget> outputWorkspaceBudget)
{
    return d->readNextFrame(std::move(outputWorkspaceBudget));
}

bool ApngAnimationReader::hasMoreFrames() const { return d->hasMoreFrames(); }

bool ApngAnimationReader::lastReadResourceLimitExceeded() const
{
    return d->lastReadResourceLimitExceeded();
}

}
