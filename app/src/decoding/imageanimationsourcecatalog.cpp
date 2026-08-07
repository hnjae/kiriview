// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageanimationsourcecatalog.h"

#include "animationtiming.h"
#include "heifsequencereader.h"
#include "jxlanimationreader.h"
#include "webpanimationreader.h"

#include <ImageViewport/imagesequence.h>
#include <webp/demux.h>

#include <QByteArrayView>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>

namespace {
using CatalogResult = kiriview::ImageAnimationSourceCatalogResult;

QString catalogError() { return QStringLiteral("animation source catalog is invalid"); }

CatalogResult failedCatalog(QString errorString = catalogError(),
    kiriview::ImageAnimationSourceCatalogFailureCause cause
    = kiriview::ImageAnimationSourceCatalogFailureCause::InvalidSource)
{
    return std::unexpected(
        kiriview::ImageAnimationSourceCatalogFailure { std::move(errorString), cause });
}

CatalogResult resourceLimitCatalog()
{
    return failedCatalog(kiriview::imageDecodeWorkspaceResourceLimitDiagnostic(),
        kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded);
}

bool hasBytes(const QByteArray& data, qsizetype offset, qsizetype count)
{
    return offset >= 0 && count >= 0 && offset <= data.size() && count <= data.size() - offset;
}

std::uint16_t readBigEndian16(const QByteArray& data, qsizetype offset)
{
    return (static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset])) << 8)
        | static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset + 1]));
}

std::uint16_t readLittleEndian16(const QByteArray& data, qsizetype offset)
{
    return static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset]))
        | (static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset + 1])) << 8);
}

std::uint32_t readBigEndian32(const QByteArray& data, qsizetype offset)
{
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset])) << 24)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 8)
        | static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 3]));
}

int normalizedDelay(int delay) { return kiriview::normalizedAnimationFrameDelay(delay); }

int maximumFrameCount() { return ImageSequenceLimits::maximumFrameCount(); }

bool skipGifSubBlocks(const QByteArray& data, qsizetype* offset)
{
    while (hasBytes(data, *offset, 1)) {
        const qsizetype blockSize = static_cast<unsigned char>(data[*offset]);
        ++*offset;
        if (blockSize == 0) {
            return true;
        }
        if (!hasBytes(data, *offset, blockSize)) {
            return false;
        }
        *offset += blockSize;
    }
    return false;
}

CatalogResult gifCatalog(const kiriview::ReaderAnimationPlaybackRequest& request)
{
    const QByteArray& data = request.data;
    if (request.format.toLower() != QByteArrayLiteral("gif") || !hasBytes(data, 0, 13)
        || (data.first(6) != QByteArrayLiteral("GIF87a")
            && data.first(6) != QByteArrayLiteral("GIF89a"))) {
        return failedCatalog();
    }

    const int width = readLittleEndian16(data, 6);
    const int height = readLittleEndian16(data, 8);
    if (width <= 0 || height <= 0) {
        return failedCatalog();
    }

    qsizetype offset = 13;
    const unsigned char logicalPacked = static_cast<unsigned char>(data[10]);
    if ((logicalPacked & 0x80U) != 0U) {
        const qsizetype tableBytes
            = 3 * (qsizetype(1) << (static_cast<int>(logicalPacked & 0x07U) + 1));
        if (!hasBytes(data, offset, tableBytes)) {
            return failedCatalog();
        }
        offset += tableBytes;
    }

    QVector<int> durations;
    int pendingDelay = 0;
    int repeatCount = 0;
    while (hasBytes(data, offset, 1)) {
        const unsigned char marker = static_cast<unsigned char>(data[offset++]);
        if (marker == 0x3bU) {
            break;
        }
        if (marker == 0x2cU) {
            if (!hasBytes(data, offset, 9)) {
                return failedCatalog();
            }
            const unsigned char imagePacked = static_cast<unsigned char>(data[offset + 8]);
            offset += 9;
            if ((imagePacked & 0x80U) != 0U) {
                const qsizetype tableBytes
                    = 3 * (qsizetype(1) << (static_cast<int>(imagePacked & 0x07U) + 1));
                if (!hasBytes(data, offset, tableBytes)) {
                    return failedCatalog();
                }
                offset += tableBytes;
            }
            if (!hasBytes(data, offset, 1)) {
                return failedCatalog();
            }
            ++offset;
            if (!skipGifSubBlocks(data, &offset)) {
                return failedCatalog();
            }
            if (durations.size() >= maximumFrameCount()) {
                return resourceLimitCatalog();
            }
            durations.append(normalizedDelay(pendingDelay));
            pendingDelay = 0;
            continue;
        }
        if (marker != 0x21U || !hasBytes(data, offset, 1)) {
            return failedCatalog();
        }

        const unsigned char label = static_cast<unsigned char>(data[offset++]);
        if (label == 0xf9U) {
            if (!hasBytes(data, offset, 6) || static_cast<unsigned char>(data[offset]) != 4U
                || static_cast<unsigned char>(data[offset + 5]) != 0U) {
                return failedCatalog();
            }
            pendingDelay = static_cast<int>(readLittleEndian16(data, offset + 2)) * 10;
            offset += 6;
            continue;
        }
        if (label == 0xffU) {
            if (!hasBytes(data, offset, 1)) {
                return failedCatalog();
            }
            const qsizetype identifierSize = static_cast<unsigned char>(data[offset++]);
            if (!hasBytes(data, offset, identifierSize)) {
                return failedCatalog();
            }
            const QByteArray identifier = data.mid(offset, identifierSize);
            offset += identifierSize;

            bool firstSubBlock = true;
            bool terminated = false;
            while (hasBytes(data, offset, 1)) {
                const qsizetype blockSize = static_cast<unsigned char>(data[offset++]);
                if (blockSize == 0) {
                    terminated = true;
                    break;
                }
                if (!hasBytes(data, offset, blockSize)) {
                    return failedCatalog();
                }
                if (firstSubBlock
                    && (identifier == QByteArrayLiteral("NETSCAPE2.0")
                        || identifier == QByteArrayLiteral("ANIMEXTS1.0"))
                    && blockSize >= 3 && static_cast<unsigned char>(data[offset]) == 1U) {
                    const std::uint16_t loops = readLittleEndian16(data, offset + 1);
                    repeatCount = loops == 0 ? -1 : static_cast<int>(loops);
                }
                firstSubBlock = false;
                offset += blockSize;
            }
            if (!terminated) {
                return failedCatalog();
            }
            continue;
        }
        if (!skipGifSubBlocks(data, &offset)) {
            return failedCatalog();
        }
    }

    kiriview::ImageAnimationSourceCatalog catalog {
        QSize(width, height),
        std::move(durations),
        repeatCount,
    };
    return catalog.isValid() ? CatalogResult(std::move(catalog)) : failedCatalog();
}

CatalogResult apngCatalog(const kiriview::ApngAnimationPlaybackRequest& request)
{
    static constexpr unsigned char signature[] {
        0x89U,
        'P',
        'N',
        'G',
        0x0dU,
        0x0aU,
        0x1aU,
        0x0aU,
    };
    const QByteArray& data = request.data;
    if (!hasBytes(data, 0, static_cast<qsizetype>(std::size(signature)))
        || std::memcmp(data.constData(), signature, std::size(signature)) != 0) {
        return failedCatalog();
    }

    QSize logicalSize;
    QVector<int> durations;
    std::optional<std::uint32_t> declaredFrameCount;
    int repeatCount = 0;
    qsizetype offset = static_cast<qsizetype>(std::size(signature));
    while (hasBytes(data, offset, 12)) {
        const std::uint32_t payloadSize = readBigEndian32(data, offset);
        if (payloadSize > static_cast<std::uint32_t>(std::numeric_limits<qsizetype>::max())
            || !hasBytes(data, offset + 8, static_cast<qsizetype>(payloadSize) + 4)) {
            return failedCatalog();
        }
        const qsizetype payloadOffset = offset + 8;
        const QByteArrayView type(data.constData() + offset + 4, 4);
        if (type == QByteArrayView("IHDR", 4)) {
            if (payloadSize != 13) {
                return failedCatalog();
            }
            const std::uint32_t width = readBigEndian32(data, payloadOffset);
            const std::uint32_t height = readBigEndian32(data, payloadOffset + 4);
            if (width == 0 || height == 0
                || width > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
                || height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
                return failedCatalog();
            }
            logicalSize = QSize(static_cast<int>(width), static_cast<int>(height));
        } else if (type == QByteArrayView("acTL", 4)) {
            if (payloadSize != 8) {
                return failedCatalog();
            }
            declaredFrameCount = readBigEndian32(data, payloadOffset);
            if (std::cmp_greater(*declaredFrameCount, maximumFrameCount())) {
                return failedCatalog();
            }
            repeatCount = kiriview::animationLoopCountForPlayCount(
                readBigEndian32(data, payloadOffset + 4));
        } else if (type == QByteArrayView("fcTL", 4)) {
            if (payloadSize != 26 || durations.size() >= maximumFrameCount()) {
                return failedCatalog();
            }
            const int delay = kiriview::apngFrameDelay(readBigEndian16(data, payloadOffset + 20),
                readBigEndian16(data, payloadOffset + 22));
            durations.append(normalizedDelay(delay));
        }
        offset = payloadOffset + static_cast<qsizetype>(payloadSize) + 4;
    }

    kiriview::ImageAnimationSourceCatalog catalog {
        logicalSize,
        std::move(durations),
        repeatCount,
    };
    if (!declaredFrameCount.has_value()
        || std::cmp_not_equal(*declaredFrameCount, catalog.frameDurations.size())
        || !catalog.isValid()) {
        return failedCatalog();
    }
    return catalog;
}

struct WebPDemuxDeleter
{
    void operator()(WebPDemuxer* demuxer) const
    {
        if (demuxer != nullptr) {
            WebPDemuxDelete(demuxer);
        }
    }
};

CatalogResult webpCatalog(const kiriview::WebPAnimationPlaybackRequest& request)
{
    if (!kiriview::webPAnimationWorkspaceModelSupports(
            kiriview::currentWebPAnimationLibraryVersions())) {
        return resourceLimitCatalog();
    }

    // libwebp 1.6.0's demuxer retains only one fixed-size Frame or Chunk node
    // per distinct RIFF chunk and borrows all payload bytes. Every such node
    // consumes at least an eight-byte chunk header, so 32 bytes per input byte
    // plus fixed parser state is a conservative bound for its allocation tree.
    constexpr qsizetype fixedParserByteCount = 4096;
    constexpr qsizetype parserBytesPerInputByte = 32;
    constexpr qsizetype maximum = std::numeric_limits<qsizetype>::max();
    if (request.data.size() > (maximum - fixedParserByteCount) / parserBytesPerInputByte) {
        return resourceLimitCatalog();
    }
    const qsizetype parserByteCount
        = fixedParserByteCount + (request.data.size() * parserBytesPerInputByte);
    const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget
        = request.workspaceBudget != nullptr ? request.workspaceBudget
                                             : kiriview::defaultImageDecodeWorkspaceBudget();
    kiriview::ImageDecodeWorkspaceLease workspaceLease = workspaceBudget->startLease();
    if (!workspaceLease.tryReserve(parserByteCount)) {
        return resourceLimitCatalog();
    }

    const WebPData webpData {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- WebP byte API.
        reinterpret_cast<const std::uint8_t*>(request.data.constData()),
        static_cast<std::size_t>(request.data.size()),
    };
    const std::unique_ptr<WebPDemuxer, WebPDemuxDeleter> demuxer(WebPDemux(&webpData));
    if (demuxer == nullptr) {
        return failedCatalog();
    }

    const std::uint32_t width = WebPDemuxGetI(demuxer.get(), WEBP_FF_CANVAS_WIDTH);
    const std::uint32_t height = WebPDemuxGetI(demuxer.get(), WEBP_FF_CANVAS_HEIGHT);
    const std::uint32_t frameCount = WebPDemuxGetI(demuxer.get(), WEBP_FF_FRAME_COUNT);
    if (width == 0 || height == 0
        || width > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || std::cmp_greater(frameCount, maximumFrameCount())) {
        return failedCatalog();
    }

    WebPIterator iterator;
    if (WebPDemuxGetFrame(demuxer.get(), 1, &iterator) == 0) {
        return failedCatalog();
    }
    QVector<int> durations;
    do {
        durations.append(normalizedDelay(iterator.duration));
    } while (durations.size() < maximumFrameCount() && WebPDemuxNextFrame(&iterator) != 0);
    WebPDemuxReleaseIterator(&iterator);

    kiriview::ImageAnimationSourceCatalog catalog {
        QSize(static_cast<int>(width), static_cast<int>(height)),
        std::move(durations),
        kiriview::animationLoopCountForPlayCount(WebPDemuxGetI(demuxer.get(), WEBP_FF_LOOP_COUNT)),
    };
    if (std::cmp_not_equal(frameCount, catalog.frameDurations.size()) || !catalog.isValid()) {
        return failedCatalog();
    }
    return catalog;
}

CatalogResult jxlCatalog(const kiriview::JxlAnimationPlaybackRequest& request)
{
    kiriview::JxlAnimationReader reader(request.workspaceBudget);
    return reader.readSourceCatalog(request.data);
}

CatalogResult heifCatalog(const kiriview::HeifSequenceAnimationPlaybackRequest& request)
{
    kiriview::HeifSequenceReader reader(request.workspaceBudget);
    const kiriview::HeifSequenceOpenResult opened = reader.open(request.data);
    if (opened.status != kiriview::HeifSequenceOpenStatus::Success) {
        return opened.status == kiriview::HeifSequenceOpenStatus::ResourceLimitExceeded
            ? resourceLimitCatalog()
            : failedCatalog(opened.errorString.isEmpty() ? catalogError() : opened.errorString);
    }

    kiriview::AnimationFrameReadResult firstFrame = reader.readNextFrame();
    if (!firstFrame.has_value()) {
        return reader.lastReadResourceLimitExceeded()
            ? resourceLimitCatalog()
            : failedCatalog(std::move(firstFrame.error()));
    }
    if (!firstFrame->has_value()) {
        return failedCatalog();
    }
    return kiriview::readHeifSequenceAnimationSourceCatalog(
        reader, **firstFrame, opened.repeatCount);
}

CatalogResult catalogFor(std::monostate) { return failedCatalog(); }

CatalogResult catalogFor(const kiriview::ReaderAnimationPlaybackRequest& request)
{
    return gifCatalog(request);
}

CatalogResult catalogFor(const kiriview::ApngAnimationPlaybackRequest& request)
{
    return apngCatalog(request);
}

CatalogResult catalogFor(const kiriview::WebPAnimationPlaybackRequest& request)
{
    return webpCatalog(request);
}

CatalogResult catalogFor(const kiriview::JxlAnimationPlaybackRequest& request)
{
    return jxlCatalog(request);
}

CatalogResult catalogFor(const kiriview::HeifSequenceAnimationPlaybackRequest& request)
{
    return heifCatalog(request);
}
}

namespace kiriview {
bool ImageAnimationSourceCatalog::isValid() const
{
    return !logicalSize.isEmpty() && frameDurations.size() >= 2
        && frameDurations.size() <= ImageSequenceLimits::maximumFrameCount() && repeatCount >= -1
        && std::ranges::all_of(frameDurations, [](int duration) { return duration > 0; });
}

ImageAnimationSourceCatalogResult readImageAnimationSourceCatalog(
    const ImageAnimationPlaybackRequest& request)
{
    return std::visit([](const auto& payload) { return catalogFor(payload); }, request.payload);
}

ImageAnimationSourceCatalogResult readHeifSequenceAnimationSourceCatalog(
    HeifSequenceReader& reader, const AnimationFrame& firstFrame, int repeatCount)
{
    if (firstFrame.image.isNull()) {
        return failedCatalog();
    }

    const QSize logicalSize = firstFrame.image.size();
    QVector<int> durations {
        normalizedDelay(firstFrame.delay),
    };
    while (durations.size() < maximumFrameCount()) {
        AnimationFrameReadResult frame = reader.readNextFrame();
        if (!frame.has_value()) {
            return reader.lastReadResourceLimitExceeded() ? resourceLimitCatalog()
                                                          : failedCatalog(std::move(frame.error()));
        }
        if (!frame->has_value()) {
            break;
        }
        if ((**frame).image.isNull() || (**frame).image.size() != logicalSize) {
            return failedCatalog();
        }
        durations.append(normalizedDelay((**frame).delay));
    }
    if (durations.size() == maximumFrameCount()) {
        AnimationFrameReadResult extra = reader.readNextFrame();
        if (!extra.has_value() || extra->has_value()) {
            return !extra.has_value() && reader.lastReadResourceLimitExceeded()
                ? resourceLimitCatalog()
                : failedCatalog(extra.has_value() ? catalogError() : std::move(extra.error()));
        }
    }

    ImageAnimationSourceCatalog catalog {
        logicalSize,
        std::move(durations),
        repeatCount,
    };
    return catalog.isValid() ? ImageAnimationSourceCatalogResult(std::move(catalog))
                             : failedCatalog();
}
}
