// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageinputclassification.h"

#include "decoding/heifcontainer.h"
#include "format/supportedmediaformats.h"

#include <QByteArrayView>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <optional>

namespace {
constexpr QByteArrayView pngSignature("\x89PNG\r\n\x1a\n", 8);
constexpr QByteArrayView jpeg2000Signature("\0\0\0\x0cjP  \r\n\x87\n", 12);
constexpr QByteArrayView jxlCodestreamSignature("\xff\x0a", 2);
constexpr QByteArrayView jxlContainerSignature("\0\0\0\x0cJXL \r\n\x87\n", 12);

enum class TiffByteOrder {
    LittleEndian,
    BigEndian,
};

template <typename Integer>
std::optional<Integer> readInteger(QByteArrayView data, qsizetype offset, QSysInfo::Endian order)
{
    if (offset < 0 || offset > data.size() - qsizetype(sizeof(Integer))) {
        return std::nullopt;
    }
    Integer value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return order == QSysInfo::BigEndian ? qFromBigEndian(value) : qFromLittleEndian(value);
}

std::optional<quint32> readBigEndianU32(QByteArrayView data, qsizetype offset)
{
    return readInteger<quint32>(data, offset, QSysInfo::BigEndian);
}

kiriview::ImageInputClassification classification(kiriview::ImageInputKind kind)
{
    return { kind, kiriview::QtRasterFormat::None,
        kind == kiriview::ImageInputKind::HeifFamily
            ? kiriview::ImageDecodeDataSource::AvifCompatible
            : kiriview::ImageDecodeDataSource::Original };
}

kiriview::ImageInputClassification qtRaster(kiriview::QtRasterFormat format)
{
    return { kiriview::ImageInputKind::QtRaster, format,
        kiriview::ImageDecodeDataSource::Original };
}

QString fileNameExtension(const QString& name)
{
    const qsizetype dot = name.lastIndexOf(u'.');
    return dot <= 0 || dot == name.size() - 1 ? QString() : name.sliced(dot + 1).toLower();
}

bool pngContainsAnimationControl(QByteArrayView data)
{
    qsizetype offset = pngSignature.size();
    while (offset >= 0 && offset <= data.size() - 12) {
        const std::optional<quint32> length = readBigEndianU32(data, offset);
        if (!length.has_value()) {
            return false;
        }
        const QByteArrayView kind = data.sliced(offset + 4, 4);
        if (kind == QByteArrayView("acTL", 4)) {
            return true;
        }
        if (kind == QByteArrayView("IDAT", 4) || kind == QByteArrayView("IEND", 4)
            || *length > quint32(std::numeric_limits<qsizetype>::max() - offset - 12)) {
            return false;
        }
        offset += 12 + *length;
    }
    return false;
}

bool isJpeg2000Brand(QByteArrayView brand)
{
    return brand == QByteArrayView("jp2 ", 4) || brand == QByteArrayView("jpx ", 4)
        || brand == QByteArrayView("jpm ", 4);
}

std::optional<kiriview::ImageInputClassification> bmffClassification(QByteArrayView data)
{
    constexpr qsizetype majorBrandOffset = 8;
    constexpr qsizetype compatibleBrandsOffset = 16;
    if (data.size() < compatibleBrandsOffset || data.sliced(4, 4) != QByteArrayView("ftyp", 4)) {
        return std::nullopt;
    }
    const std::optional<quint32> encodedSize = readBigEndianU32(data, 0);
    if (!encodedSize.has_value() || *encodedSize < compatibleBrandsOffset
        || *encodedSize > quint32(data.size())) {
        return std::nullopt;
    }
    bool raw = false;
    bool heif = false;
    bool jxl = false;
    bool jp2 = false;
    const auto record = [&](QByteArrayView brand) {
        if (brand == QByteArrayView("crx ", 4) || brand == QByteArrayView("cr3 ", 4)) {
            raw = true;
        } else if (kiriview::heifBrandKind(brand) != kiriview::HeifBrandKind::Unknown) {
            heif = true;
        } else if (brand == QByteArrayView("jxl ", 4)) {
            jxl = true;
        } else if (isJpeg2000Brand(brand)) {
            jp2 = true;
        }
    };
    record(data.sliced(majorBrandOffset, 4));
    for (qsizetype offset = compatibleBrandsOffset; offset + 4 <= *encodedSize; offset += 4) {
        record(data.sliced(offset, 4));
    }
    if (raw) {
        return classification(kiriview::ImageInputKind::Raw);
    }
    if (heif) {
        return classification(kiriview::ImageInputKind::HeifFamily);
    }
    if (jxl) {
        return qtRaster(kiriview::QtRasterFormat::Jxl);
    }
    if (jp2) {
        return qtRaster(kiriview::QtRasterFormat::Jp2);
    }
    return std::nullopt;
}

std::optional<TiffByteOrder> tiffByteOrder(QByteArrayView data)
{
    if (data.startsWith(QByteArrayView("II*\0", 4))) {
        return TiffByteOrder::LittleEndian;
    }
    if (data.startsWith(QByteArrayView("MM\0*", 4))) {
        return TiffByteOrder::BigEndian;
    }
    return std::nullopt;
}

template <typename Integer>
std::optional<Integer> readTiffInteger(QByteArrayView data, qsizetype offset, TiffByteOrder order)
{
    return readInteger<Integer>(data, offset,
        order == TiffByteOrder::BigEndian ? QSysInfo::BigEndian : QSysInfo::LittleEndian);
}

bool isLikelyTiffRawImage(QByteArrayView data)
{
    constexpr quint16 photometricTag = 262;
    constexpr quint16 dngVersionTag = 50706;
    constexpr quint16 shortType = 3;
    constexpr quint16 cfaPhotometric = 32803;
    const std::optional<TiffByteOrder> order = tiffByteOrder(data);
    if (!order.has_value()) {
        return false;
    }
    static const std::array<QByteArray, 4> rawMarkers { QByteArray::fromHex("06010300010000002380"),
        QByteArray::fromHex("01060003000000018023"), QByteArray::fromHex("12c6010004000000"),
        QByteArray::fromHex("c612000100000004") };
    if (std::ranges::any_of(
            rawMarkers, [data](const QByteArray& marker) { return data.indexOf(marker) >= 0; })) {
        return true;
    }
    const std::optional<quint32> ifdOffset = readTiffInteger<quint32>(data, 4, *order);
    if (!ifdOffset.has_value() || *ifdOffset > quint32(std::numeric_limits<qsizetype>::max())) {
        return false;
    }
    const qsizetype base = *ifdOffset;
    const std::optional<quint16> count = readTiffInteger<quint16>(data, base, *order);
    if (!count.has_value() || base > data.size() - 2
        || *count > quint16((data.size() - base - 2) / 12)) {
        return false;
    }
    for (quint16 index = 0; index < *count; ++index) {
        const qsizetype entry = base + 2 + qsizetype(index) * 12;
        const std::optional<quint16> tag = readTiffInteger<quint16>(data, entry, *order);
        if (tag == dngVersionTag) {
            return true;
        }
        if (tag != photometricTag
            || readTiffInteger<quint16>(data, entry + 2, *order) != shortType) {
            continue;
        }
        const std::optional<quint32> valueCount = readTiffInteger<quint32>(data, entry + 4, *order);
        if (!valueCount.has_value() || *valueCount == 0) {
            continue;
        }
        qsizetype valueOffset = entry + 8;
        if (*valueCount > 2) {
            const std::optional<quint32> offset = readTiffInteger<quint32>(data, entry + 8, *order);
            if (!offset.has_value()) {
                continue;
            }
            valueOffset = *offset;
        }
        if (readTiffInteger<quint16>(data, valueOffset, *order) == cfaPhotometric) {
            return true;
        }
    }
    return false;
}

bool looksLikeSvg(QByteArrayView data)
{
    QByteArray prefix(data.first(std::min<qsizetype>(data.size(), 4096)).toByteArray());
    if (prefix.startsWith("\xef\xbb\xbf")) {
        prefix.remove(0, 3);
    }
    while (!prefix.isEmpty() && std::isspace(static_cast<unsigned char>(prefix.front()))) {
        prefix.remove(0, 1);
    }
    if (prefix.startsWith("<svg") || prefix.startsWith("<SVG")) {
        return true;
    }
    const QByteArray lower = prefix.toLower();
    return (lower.startsWith("<?xml") || lower.startsWith("<!--") || lower.startsWith("<!doctype"))
        && lower.contains("<svg");
}
}

namespace kiriview {
ImageInputClassification classifyImageInput(const QByteArray& data, const QString& fileName)
{
    const QByteArrayView bytes(data);
    if (bytes.startsWith(pngSignature)) {
        return pngContainsAnimationControl(bytes) ? classification(ImageInputKind::Apng)
                                                  : qtRaster(QtRasterFormat::Png);
    }
    if (bytes.startsWith(QByteArrayView("\xff\xd8\xff", 3))) {
        return qtRaster(QtRasterFormat::Jpeg);
    }
    if (bytes.startsWith("GIF87a") || bytes.startsWith("GIF89a")) {
        return qtRaster(QtRasterFormat::Gif);
    }
    if (bytes.size() >= 12 && bytes.startsWith("RIFF") && bytes.sliced(8, 4) == "WEBP") {
        return qtRaster(QtRasterFormat::Webp);
    }
    if (bytes.startsWith("BM")) {
        return qtRaster(QtRasterFormat::Bmp);
    }
    if (bytes.startsWith(jxlCodestreamSignature) || bytes.startsWith(jxlContainerSignature)) {
        return qtRaster(QtRasterFormat::Jxl);
    }
    if (bytes.startsWith(jpeg2000Signature)) {
        return qtRaster(QtRasterFormat::Jp2);
    }
    const QString extension = fileNameExtension(fileName);
    if (tiffByteOrder(bytes).has_value()) {
        return SupportedMediaFormats::isRawImageExtension(extension) || isLikelyTiffRawImage(bytes)
            ? classification(ImageInputKind::Raw)
            : qtRaster(QtRasterFormat::Tiff);
    }
    if (const auto bmff = bmffClassification(bytes)) {
        return *bmff;
    }
    if (extension == QStringLiteral("svg") || looksLikeSvg(bytes)) {
        return classification(ImageInputKind::Svg);
    }
    if (SupportedMediaFormats::isRawImageExtension(extension)) {
        return classification(ImageInputKind::Raw);
    }
    return {};
}

QByteArray qtImageReaderFormat(QtRasterFormat format)
{
    switch (format) {
    case QtRasterFormat::Png:
        return QByteArrayLiteral("png");
    case QtRasterFormat::Jpeg:
        return QByteArrayLiteral("jpeg");
    case QtRasterFormat::Gif:
        return QByteArrayLiteral("gif");
    case QtRasterFormat::Webp:
        return QByteArrayLiteral("webp");
    case QtRasterFormat::Bmp:
        return QByteArrayLiteral("bmp");
    case QtRasterFormat::Tiff:
        return QByteArrayLiteral("tiff");
    case QtRasterFormat::Jxl:
        return QByteArrayLiteral("jxl");
    case QtRasterFormat::Jp2:
        return QByteArrayLiteral("jp2");
    case QtRasterFormat::None:
        return {};
    }
    return {};
}
}
