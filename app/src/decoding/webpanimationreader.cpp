// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "webpanimationreader.h"

#include "animationtiming.h"
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
    return QStringLiteral("WebP decoder rejected the animation data");
}

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

kiriview::WebPAnimationOpenResult resourceLimitOpenResult()
{
    kiriview::WebPAnimationOpenResult result;
    result.status = kiriview::WebPAnimationOpenStatus::ResourceLimitExceeded;
    result.errorString = kiriview::imageDecodeWorkspaceResourceLimitDiagnostic();
    return result;
}

WebPData webpDataFor(const QByteArray& data)
{
    return WebPData {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- libwebp byte API.
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
        bytes, size.width(), size.height(), bytesPerLine, QImage::Format_RGBA8888_Premultiplied);
    if (borrowedImage.isNull()) {
        return std::nullopt;
    }

    QImage output = borrowedImage.copy();
    if (output.isNull()) {
        return std::nullopt;
    }
    output.setColorSpace(QColorSpace(QColorSpace::SRgb));
    return output;
}

std::optional<qsizetype> webpAnimationTransientWorkspaceByteCount(
    QSize canvasSize, qsizetype inputByteCount)
{
    if (canvasSize.isEmpty() || inputByteCount < 0) {
        return std::nullopt;
    }

    constexpr quint64 maximum = static_cast<quint64>(std::numeric_limits<qsizetype>::max());
    const auto checkedAdd = [](quint64 left, quint64 right) -> std::optional<quint64> {
        return right <= maximum - left ? std::optional<quint64>(left + right) : std::nullopt;
    };
    const auto checkedMultiply = [](quint64 left, quint64 right) -> std::optional<quint64> {
        return left == 0 || right <= maximum / left ? std::optional<quint64>(left * right)
                                                    : std::nullopt;
    };

    const quint64 width = static_cast<quint64>(canvasSize.width());
    const quint64 height = static_cast<quint64>(canvasSize.height());
    const std::optional<quint64> pixelCount = checkedMultiply(width, height);
    const std::optional<quint64> huffmanPositionCount
        = checkedMultiply((width + 3) / 4, (height + 3) / 4);
    if (!pixelCount.has_value() || !huffmanPositionCount.has_value()) {
        return std::nullopt;
    }

    // Pinned libwebp 1.6.0, threads disabled, no scaling:
    // - AnimDecoder owns two 4P canvases.
    // - VP8 plus compressed ALPH/VP8L peaks below 20P + 256W + 1 MiB.
    // - VP8L can retain two 5004-entry Huffman segments and one 568-byte
    //   HTreeGroup per group on 64-bit targets: 40,600G.
    // - G is bounded by the larger of the <=1000 direct-index case and the
    //   4x4-subsampled meta-Huffman image, capped by its 16-bit group index.
    // - Demux owns fixed nodes that are bounded by 32 bytes per source byte.
    const quint64 groupCount = std::max(std::min(*pixelCount, quint64 { 1000 }),
        std::min(*huffmanPositionCount, quint64 { 65536 }));
    const std::optional<quint64> canvasAndCodecBytes = checkedMultiply(*pixelCount, 28);
    const std::optional<quint64> rowBytes = checkedMultiply(width, 256);
    const std::optional<quint64> huffmanBytes = checkedMultiply(groupCount, 40600);
    const std::optional<quint64> demuxBytes
        = checkedMultiply(static_cast<quint64>(inputByteCount), 32);
    if (!canvasAndCodecBytes.has_value() || !rowBytes.has_value() || !huffmanBytes.has_value()
        || !demuxBytes.has_value()) {
        return std::nullopt;
    }

    std::optional<quint64> total = checkedAdd(*canvasAndCodecBytes, *rowBytes);
    total = total.has_value() ? checkedAdd(*total, *huffmanBytes) : std::nullopt;
    total = total.has_value() ? checkedAdd(*total, *demuxBytes) : std::nullopt;
    constexpr quint64 fixedByteCount = (quint64 { 1 } * 1024 * 1024) + 4096;
    total = total.has_value() ? checkedAdd(*total, fixedByteCount) : std::nullopt;
    return total.has_value() ? std::optional<qsizetype>(static_cast<qsizetype>(*total))
                             : std::nullopt;
}
}

namespace kiriview {
bool webPAnimationWorkspaceModelSupports(WebPAnimationLibraryVersions versions)
{
    // The allocation proof below is source-derived for this exact release.
    // Fail closed before parsing if either linked runtime changes.
    return versions.decoder == auditedWebPAnimationLibraryVersion
        && versions.demux == auditedWebPAnimationLibraryVersion;
}

WebPAnimationLibraryVersions currentWebPAnimationLibraryVersions()
{
    return WebPAnimationLibraryVersions {
        WebPGetDecoderVersion(),
        WebPGetDemuxVersion(),
    };
}

WebPAnimationWorkspacePlanResult planWebPAnimationOpen(
    const QByteArray& data, WebPAnimationLibraryVersions versions)
{
    if (data.isEmpty()) {
        return {};
    }
    if (!webPAnimationWorkspaceModelSupports(versions)) {
        return { WebPAnimationOpenStatus::ResourceLimitExceeded, {},
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }

    const WebPData input = webpDataFor(data);
    WebPBitstreamFeatures features {};
    if (WebPGetFeatures(input.bytes, input.size, &features) != VP8_STATUS_OK) {
        return {};
    }
    if (features.has_animation == 0) {
        return { WebPAnimationOpenStatus::NotAnimation, {}, {} };
    }
    if (features.width <= 0 || features.height <= 0) {
        return { WebPAnimationOpenStatus::Error, {}, webpAnimationDecodeErrorString() };
    }

    const QSize canvasSize(features.width, features.height);
    const std::optional<qsizetype> outputByteCount
        = checkedImageDecodeWorkspaceByteCount(canvasSize, 4, 1);
    const std::optional<qsizetype> transientByteCount
        = webpAnimationTransientWorkspaceByteCount(canvasSize, data.size());
    if (!outputByteCount.has_value() || !transientByteCount.has_value()) {
        return { WebPAnimationOpenStatus::ResourceLimitExceeded, {},
            imageDecodeWorkspaceResourceLimitDiagnostic() };
    }
    return { WebPAnimationOpenStatus::Success,
        WebPAnimationWorkspacePlan { canvasSize, *transientByteCount, *outputByteCount }, {} };
}

class WebPAnimationReader::Private
{
public:
    Private(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        WebPAnimationLibraryVersions libraryVersions)
        : m_workspaceBudget(workspaceBudget != nullptr ? std::move(workspaceBudget)
                                                       : defaultImageDecodeWorkspaceBudget())
        , m_libraryVersions(libraryVersions)
    {
    }

    ~Private() { reset(); }
    Private(const Private&) = delete;
    Private& operator=(const Private&) = delete;
    Private(Private&&) = delete;
    Private& operator=(Private&&) = delete;

    WebPAnimationOpenResult open(QByteArray inputData)
    {
        const WebPAnimationWorkspacePlanResult planning
            = planWebPAnimationOpen(inputData, m_libraryVersions);
        if (planning.status != WebPAnimationOpenStatus::Success) {
            reset();
            lastResourceLimitExceeded
                = planning.status == WebPAnimationOpenStatus::ResourceLimitExceeded;
            WebPAnimationOpenResult result;
            result.status = planning.status;
            result.errorString = planning.errorString;
            return result;
        }
        return open(std::move(inputData), planning.plan);
    }

    WebPAnimationOpenResult open(
        QByteArray inputData, const WebPAnimationWorkspacePlan& workspacePlan)
    {
        reset();
        lastResourceLimitExceeded = false;
        if (workspacePlan.canvasSize.isEmpty() || workspacePlan.transientByteCount <= 0
            || workspacePlan.firstFrameOutputByteCount <= 0
            || !tryReserveWorkspace(workspacePlan.transientByteCount)) {
            reset();
            lastResourceLimitExceeded = true;
            return resourceLimitOpenResult();
        }
        canvasSize = workspacePlan.canvasSize;
        outputByteCount = workspacePlan.firstFrameOutputByteCount;

        data = std::move(inputData);
        WebPData webpData = webpDataFor(data);
        WebPAnimDecoderOptions options;
        if (WebPAnimDecoderOptionsInit(&options) == 0) {
            reset();
            return errorOpenResult(webpAnimationDecodeErrorString());
        }
        options.color_mode = MODE_rgbA;
        options.use_threads = 0;

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

        const QSize decoderCanvasSize(
            static_cast<int>(info.canvas_width), static_cast<int>(info.canvas_height));
        if (decoderCanvasSize != canvasSize) {
            reset();
            return errorOpenResult(webpAnimationDecodeErrorString());
        }
        loopCount = animationLoopCountForPlayCount(info.loop_count);

        AnimationFrameReadResult firstFrame = readNextFrame();
        if (!firstFrame || !firstFrame->has_value()) {
            const bool resourceLimitExceeded = lastResourceLimitExceeded;
            reset();
            if (resourceLimitExceeded) {
                lastResourceLimitExceeded = true;
                return resourceLimitOpenResult();
            }
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
        result.workspaceHold = std::move(decodedFirstFrame.workspaceHold);
        return result;
    }

    AnimationFrameReadResult readNextFrame() { return readNextFrame({}); }

    AnimationFrameReadResult readNextFrame(
        const std::shared_ptr<ImageDecodeWorkspaceBudget>& outputWorkspaceBudget)
    {
        if (decoder == nullptr || !hasMoreFrames()) {
            return std::optional<AnimationFrame>();
        }

        const std::shared_ptr<ImageDecodeWorkspaceBudget>& budget
            = outputWorkspaceBudget != nullptr ? outputWorkspaceBudget : m_workspaceBudget;
        ImageDecodeWorkspaceLease outputLease = ImageDecodeWorkspaceDetail::startLeaseForOperation(
            *budget, transientWorkspaceLease.reservedByteCount());
        if (!ImageDecodeWorkspaceDetail::tryReserve(outputLease, outputByteCount)) {
            reset();
            lastResourceLimitExceeded = true;
            return std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic());
        }

        std::uint8_t* bytes = nullptr;
        int timestamp = 0;
        if (WebPAnimDecoderGetNext(decoder.get(), &bytes, &timestamp) == 0) {
            return std::unexpected(webpAnimationDecodeErrorString());
        }

        std::optional<QImage> image = imageFromRgbaFrame(bytes, canvasSize);
        if (!image.has_value()) {
            reset();
            lastResourceLimitExceeded = true;
            return std::unexpected(imageDecodeWorkspaceResourceLimitDiagnostic());
        }

        const int delay = frameTimestamp < 0 ? timestamp : std::max(0, timestamp - frameTimestamp);
        frameTimestamp = timestamp;
        return std::optional<AnimationFrame>(
            AnimationFrame { std::move(*image), delay, outputLease.sharedHold() });
    }

    [[nodiscard]] bool hasMoreFrames() const
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
        outputByteCount = 0;
        transientWorkspaceLease = {};
    }

    [[nodiscard]] bool lastReadResourceLimitExceeded() const { return lastResourceLimitExceeded; }

private:
    bool tryReserveWorkspace(qsizetype byteCount)
    {
        transientWorkspaceLease = ImageDecodeWorkspaceDetail::startLease(*m_workspaceBudget);
        return ImageDecodeWorkspaceDetail::tryReserve(transientWorkspaceLease, byteCount);
    }

    std::shared_ptr<ImageDecodeWorkspaceBudget> m_workspaceBudget;
    WebPAnimationLibraryVersions m_libraryVersions;
    ImageDecodeWorkspaceLease transientWorkspaceLease;
    qsizetype outputByteCount = 0;
    QByteArray data;
    WebPAnimDecoderPtr decoder;
    QSize canvasSize;
    int frameTimestamp = -1;
    int loopCount = 0;
    bool lastResourceLimitExceeded = false;
};

WebPAnimationReader::WebPAnimationReader()
    : WebPAnimationReader(defaultImageDecodeWorkspaceBudget())
{
}

WebPAnimationReader::WebPAnimationReader(
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget)
    : WebPAnimationReader(std::move(workspaceBudget), currentWebPAnimationLibraryVersions())
{
}

WebPAnimationReader::WebPAnimationReader(
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    WebPAnimationLibraryVersions libraryVersions)
    : d(std::make_unique<Private>(std::move(workspaceBudget), libraryVersions))
{
}

WebPAnimationReader::~WebPAnimationReader() = default;

WebPAnimationReader::WebPAnimationReader(WebPAnimationReader&&) noexcept = default;

WebPAnimationReader& WebPAnimationReader::operator=(WebPAnimationReader&&) noexcept = default;

WebPAnimationOpenResult WebPAnimationReader::open(QByteArray data)
{
    return d->open(std::move(data));
}

WebPAnimationOpenResult WebPAnimationReader::open(
    QByteArray data, const WebPAnimationWorkspacePlan& plan)
{
    return d->open(std::move(data), plan);
}

AnimationFrameReadResult WebPAnimationReader::readNextFrame() { return d->readNextFrame(); }

AnimationFrameReadResult WebPAnimationReader::readNextFrame(
    const std::shared_ptr<ImageDecodeWorkspaceBudget>& outputWorkspaceBudget)
{
    return d->readNextFrame(outputWorkspaceBudget);
}

bool WebPAnimationReader::hasMoreFrames() const { return d->hasMoreFrames(); }

bool WebPAnimationReader::lastReadResourceLimitExceeded() const
{
    return d->lastReadResourceLimitExceeded();
}

void WebPAnimationReader::close() { (*d).reset(); }
}
