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
    ApngOpenResult open(const QByteArray& inputData)
    {
        close();

        const RustApngOpenResult openResult
            = rustOpenApngAnimationReader(*reader, Bridge::rustBytes(inputData));
        switch (openResult.status) {
        case RustApngOpenStatus::NotApng:
            return {};
        case RustApngOpenStatus::Error:
            close();
            return errorResult(apngDecodeErrorString());
        case RustApngOpenStatus::Success:
            break;
        }

        if (!canRepresentCanvas(openResult.canvas_width, openResult.canvas_height)
            || !composer.initialize(QSize(static_cast<int>(openResult.canvas_width),
                                        static_cast<int>(openResult.canvas_height)),
                static_cast<std::size_t>(openResult.canvas_width)
                    * ApngRgbaBuffer::bytesPerPixel)) {
            close();
            return errorResult(apngDecodeErrorString());
        }

        AnimationFrameReadResult firstFrame = readNextFrame();
        if (!firstFrame || !firstFrame->has_value()) {
            close();
            return errorResult(firstFrame ? apngDecodeErrorString() : firstFrame.error());
        }

        AnimationFrame decodedFirstFrame = std::move(**firstFrame);
        ApngOpenResult result;
        result.status = ApngOpenStatus::Success;
        result.firstFrame = std::move(decodedFirstFrame.image);
        result.firstFrameDelay = decodedFirstFrame.delay;
        result.loopCount = openResult.loop_count;
        result.frameCount = openResult.frame_count;
        return result;
    }

    AnimationFrameReadResult readNextFrame()
    {
        const RustApngFrameResult frame = rustReadApngAnimationFrame(*reader);
        switch (frame.status) {
        case RustApngReadStatus::End:
            return std::optional<AnimationFrame>();
        case RustApngReadStatus::Error:
            close();
            return std::unexpected(apngDecodeErrorString());
        case RustApngReadStatus::Frame:
            break;
        }

        const ApngFrameControl control = frameControlFromRust(frame);
        if (!composer.setFrameBytes(
                control, std::span(frame.pixels.data(), frame.pixels.size()), frame.row_bytes)) {
            close();
            return std::unexpected(apngDecodeErrorString());
        }

        std::optional<QImage> image = composer.composeFrame(control);
        if (!image.has_value()) {
            close();
            return std::unexpected(apngDecodeErrorString());
        }

        return std::optional<AnimationFrame>(AnimationFrame {
            std::move(*image),
            apngFrameDelay(frame.delay_num, frame.delay_den),
        });
    }

    [[nodiscard]] bool hasMoreFrames() const
    {
        return rustApngAnimationReaderHasMoreFrames(*reader);
    }

    void close()
    {
        reader = rustNewApngAnimationReader();
        composer.clear();
    }

private:
    rust::Box<RustApngAnimationReader> reader = rustNewApngAnimationReader();
    ApngFrameComposer composer;
};

ApngAnimationReader::ApngAnimationReader()
    : d(std::make_unique<Private>())
{
}

ApngAnimationReader::~ApngAnimationReader() = default;

ApngOpenResult ApngAnimationReader::open(const QByteArray& data) { return d->open(data); }

AnimationFrameReadResult ApngAnimationReader::readNextFrame() { return d->readNextFrame(); }

bool ApngAnimationReader::hasMoreFrames() const { return d->hasMoreFrames(); }
}
