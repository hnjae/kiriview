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

struct ApngWorkspacePlan
{
    qsizetype canvasByteCount = 0;
    std::size_t decoderInternalByteLimit = 0;
    qsizetype worstCaseByteCount = 0;
};

constexpr std::size_t apngDecoderInternalByteLimit = std::size_t { 64 } * 1024 * 1024;

std::optional<ApngWorkspacePlan> apngWorkspacePlan(quint32 width, quint32 height,
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

    return ApngWorkspacePlan {
        static_cast<qsizetype>(canvasByteCount),
        apngDecoderInternalByteLimit,
        static_cast<qsizetype>(worstCaseByteCount),
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
        close();
        m_lastReadResourceLimitExceeded = false;

        const RustApngProbeResult probeResult
            = rustProbeApngAnimation(Bridge::rustBytes(inputData), apngDecoderInternalByteLimit);
        switch (probeResult.status) {
        case RustApngProbeStatus::NotApng:
            return {};
        case RustApngProbeStatus::Error:
            close();
            return errorResult(apngDecodeErrorString());
        case RustApngProbeStatus::ResourceLimitExceeded:
            close();
            return resourceLimitResult();
        case RustApngProbeStatus::Success:
            break;
        }

        const std::optional<ApngWorkspacePlan> workspacePlan
            = apngWorkspacePlan(probeResult.canvas_width, probeResult.canvas_height,
                inputData.size(), probeResult.decoder_workspace_byte_count);
        if (!canRepresentCanvas(probeResult.canvas_width, probeResult.canvas_height)
            || !workspacePlan.has_value() || !tryReserveWorkspace(*workspacePlan)) {
            close();
            return resourceLimitResult();
        }

        const RustApngOpenResult openResult = rustOpenApngAnimationReader(
            *reader, Bridge::rustBytes(inputData), workspacePlan->decoderInternalByteLimit);
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
        if (openResult.canvas_width != probeResult.canvas_width
            || openResult.canvas_height != probeResult.canvas_height) {
            close();
            return errorResult(apngDecodeErrorString());
        }

        bool initialized = false;
        try {
            initialized = composer.initialize(QSize(static_cast<int>(openResult.canvas_width),
                                                  static_cast<int>(openResult.canvas_height)),
                static_cast<std::size_t>(workspacePlan->canvasByteCount)
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

    AnimationFrameReadResult readNextFrame()
    {
        m_lastReadResourceLimitExceeded = false;
        try {
            return readNextFrameImpl();
        } catch (const std::bad_alloc&) {
            m_lastReadResourceLimitExceeded = true;
            close();
            return std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic());
        }
    }

    AnimationFrameReadResult readNextFrameImpl()
    {
        struct FrameReadAttempt
        {
            AnimationFrameReadResult result;
            bool terminate = false;
            bool resourceLimitExceeded = false;
        };

        FrameReadAttempt attempt = [this]() -> FrameReadAttempt {
            if (!rustApngAnimationReaderHasMoreFrames(*reader)) {
                return { std::optional<AnimationFrame>(), true, false };
            }

            ImageDecodeWorkspaceLease outputLease = m_workspaceBudget->startLeaseForOperation(
                m_transientWorkspaceLease.reservedByteCount());
            if (!outputLease.tryReserve(static_cast<qsizetype>(m_canvasByteCount))) {
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
    bool tryReserveWorkspace(const ApngWorkspacePlan& plan)
    {
        m_transientWorkspaceLease = m_workspaceBudget->startLease();
        if (!m_transientWorkspaceLease.isManaged()) {
            return false;
        }

        if (!m_transientWorkspaceLease.tryReserve(plan.worstCaseByteCount)) {
            return false;
        }

        m_canvasByteCount = static_cast<std::size_t>(plan.canvasByteCount);
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

AnimationFrameReadResult ApngAnimationReader::readNextFrame() { return d->readNextFrame(); }

bool ApngAnimationReader::hasMoreFrames() const { return d->hasMoreFrames(); }

bool ApngAnimationReader::lastReadResourceLimitExceeded() const
{
    return d->lastReadResourceLimitExceeded();
}

}
