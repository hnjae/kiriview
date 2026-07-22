// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "webpanimationreader.h"

#include "animationtiming.h"
#include "localization/imageerrortext.h"
#include "rendering/imagerendering.h"

#include <webp/decode.h>
#include <webp/demux.h>

#include <QColorSpace>
#include <QSize>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace {
struct WebPAnimDecoderDeleter
{
    void operator()(WebPAnimDecoder* decoder) const
    {
        if (decoder != nullptr) {
            WebPAnimDecoderDelete(decoder);
        }
    }
};

using WebPAnimDecoderPtr = std::unique_ptr<WebPAnimDecoder, WebPAnimDecoderDeleter>;

QString webpAnimationDecodeErrorString()
{
    return kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation);
}

kiriview::WebPAnimationOpenResult notWebPResult() { return {}; }

kiriview::WebPAnimationOpenResult notAnimationResult()
{
    kiriview::WebPAnimationOpenResult result;
    result.status = kiriview::WebPAnimationOpenStatus::NotAnimation;
    return result;
}

kiriview::WebPAnimationOpenResult errorOpenResult(QString errorString)
{
    kiriview::WebPAnimationOpenResult result;
    result.status = kiriview::WebPAnimationOpenStatus::Error;
    result.errorString = std::move(errorString);
    return result;
}

WebPData webpDataFor(const QByteArray& data)
{
    return WebPData {
        reinterpret_cast<const std::uint8_t*>(data.constData()),
        static_cast<std::size_t>(data.size()),
    };
}

std::optional<QImage> imageFromRgbaFrame(const std::uint8_t* bytes, QSize size)
{
    if (bytes == nullptr || size.isEmpty()) {
        return std::nullopt;
    }
    const auto width = static_cast<std::size_t>(size.width());
    if (width > static_cast<std::size_t>(std::numeric_limits<qsizetype>::max()) / 4U) {
        return std::nullopt;
    }

    const qsizetype bytesPerLine = static_cast<qsizetype>(width) * 4;
    const QImage borrowedImage(
        bytes, size.width(), size.height(), bytesPerLine, QImage::Format_RGBA8888);
    if (borrowedImage.isNull()) {
        return std::nullopt;
    }

    QImage image = borrowedImage.copy();
    image.setColorSpace(QColorSpace(QColorSpace::SRgb));
    return kiriview::displayReadyImage(image);
}
}

namespace kiriview {
class WebPAnimationReader::Private
{
public:
    WebPAnimationOpenResult open(QByteArray inputData)
    {
        reset();
        if (inputData.isEmpty()) {
            return notWebPResult();
        }

        int width = 0;
        int height = 0;
        if (WebPGetInfo(reinterpret_cast<const std::uint8_t*>(inputData.constData()),
                static_cast<std::size_t>(inputData.size()), &width, &height)
            == 0) {
            return notWebPResult();
        }
        if (width <= 0 || height <= 0) {
            return errorOpenResult(webpAnimationDecodeErrorString());
        }

        data = std::move(inputData);
        WebPData webpData = webpDataFor(data);
        WebPAnimDecoderOptions options;
        if (WebPAnimDecoderOptionsInit(&options) == 0) {
            reset();
            return errorOpenResult(webpAnimationDecodeErrorString());
        }
        options.color_mode = MODE_RGBA;
        options.use_threads = 1;

        decoder = WebPAnimDecoderPtr(WebPAnimDecoderNew(&webpData, &options));
        if (decoder == nullptr) {
            reset();
            return errorOpenResult(webpAnimationDecodeErrorString());
        }

        WebPAnimInfo info;
        if (WebPAnimDecoderGetInfo(decoder.get(), &info) == 0) {
            reset();
            return errorOpenResult(webpAnimationDecodeErrorString());
        }
        if (info.frame_count < 2) {
            reset();
            return notAnimationResult();
        }
        if (info.canvas_width == 0 || info.canvas_height == 0
            || info.canvas_width > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
            || info.canvas_height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
            reset();
            return errorOpenResult(webpAnimationDecodeErrorString());
        }

        canvasSize
            = QSize(static_cast<int>(info.canvas_width), static_cast<int>(info.canvas_height));
        loopCount = animationLoopCountForPlayCount(info.loop_count);

        AnimationFrameReadResult firstFrame = readNextFrame();
        if (!firstFrame || !firstFrame->has_value()) {
            reset();
            return errorOpenResult(
                firstFrame ? webpAnimationDecodeErrorString() : firstFrame.error());
        }

        AnimationFrame decodedFirstFrame = std::move(**firstFrame);
        WebPAnimationOpenResult result;
        result.status = WebPAnimationOpenStatus::Success;
        result.firstFrame = std::move(decodedFirstFrame.image);
        result.firstFrameDelay = decodedFirstFrame.delay;
        result.loopCount = loopCount;
        result.sourceHasMoreFrames = hasMoreFrames();
        return result;
    }

    AnimationFrameReadResult readNextFrame()
    {
        if (decoder == nullptr || !hasMoreFrames()) {
            return std::optional<AnimationFrame>();
        }

        std::uint8_t* bytes = nullptr;
        int timestamp = 0;
        if (WebPAnimDecoderGetNext(decoder.get(), &bytes, &timestamp) == 0) {
            return std::unexpected(webpAnimationDecodeErrorString());
        }

        std::optional<QImage> image = imageFromRgbaFrame(bytes, canvasSize);
        if (!image.has_value()) {
            return std::unexpected(webpAnimationDecodeErrorString());
        }

        const int delay = frameTimestamp < 0 ? timestamp : std::max(0, timestamp - frameTimestamp);
        frameTimestamp = timestamp;
        return std::optional<AnimationFrame>(AnimationFrame { std::move(*image), delay });
    }

    bool hasMoreFrames() const
    {
        return decoder != nullptr && WebPAnimDecoderHasMoreFrames(decoder.get()) != 0;
    }

    void reset()
    {
        decoder.reset();
        data.clear();
        canvasSize = {};
        frameTimestamp = -1;
        loopCount = 0;
    }

private:
    QByteArray data;
    WebPAnimDecoderPtr decoder;
    QSize canvasSize;
    int frameTimestamp = -1;
    int loopCount = 0;
};

WebPAnimationReader::WebPAnimationReader()
    : d(std::make_unique<Private>())
{
}

WebPAnimationReader::~WebPAnimationReader() = default;

WebPAnimationReader::WebPAnimationReader(WebPAnimationReader&&) noexcept = default;

WebPAnimationReader& WebPAnimationReader::operator=(WebPAnimationReader&&) noexcept = default;

WebPAnimationOpenResult WebPAnimationReader::open(QByteArray data)
{
    return d->open(std::move(data));
}

AnimationFrameReadResult WebPAnimationReader::readNextFrame() { return d->readNextFrame(); }

bool WebPAnimationReader::hasMoreFrames() const { return d->hasMoreFrames(); }

void WebPAnimationReader::close() { d->reset(); }
}
