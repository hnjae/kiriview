#pragma once

#include "imageviewport.h"

#include <cmath>

namespace ImageViewportInternal {

constexpr int minimumMaximumLogicalSide = 8192;
constexpr qint64 minimumMaximumPixelsPerFrame = 67108864LL;
constexpr int minimumMaximumPayloadRasterSide = 8192;
constexpr qint64 minimumMaximumPayloadBytesPerFrame = 268435456LL;
constexpr int minimumMaximumTimedListFrameCount = 10000;
constexpr int minimumMaximumDuration = 86400000;
constexpr int minimumMaximumDiagnosticStringLength = 4096;
constexpr int minimumMaximumFormatIdentifierLength = 256;

inline bool isPositiveFiniteInteger(double value)
{
    return std::isfinite(value) && value > 0.0 && std::trunc(value) == value;
}

inline bool isAdmittedLogicalSizeComponent(double value, int maximum)
{
    return isPositiveFiniteInteger(value) && value <= static_cast<double>(maximum);
}

inline QString frameLimitViolation(const ImageFrame& frame)
{
    const QSizeF size = frame.sourceLogicalSize();
    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width > ImageSequenceLimits::maximumSourceLogicalWidth()) {
        return QStringLiteral("ImageFrame exceeds maximumSourceLogicalWidth");
    }
    if (height > ImageSequenceLimits::maximumSourceLogicalHeight()) {
        return QStringLiteral("ImageFrame exceeds maximumSourceLogicalHeight");
    }
    if (width * height > ImageSequenceLimits::maximumSourceLogicalPixels()) {
        return QStringLiteral("ImageFrame exceeds maximumSourceLogicalPixels");
    }
    if (frame.payloadRasterSize().width() > ImageSequenceLimits::maximumPayloadRasterWidth()) {
        return QStringLiteral("ImageFrame exceeds maximumPayloadRasterWidth");
    }
    if (frame.payloadRasterSize().height() > ImageSequenceLimits::maximumPayloadRasterHeight()) {
        return QStringLiteral("ImageFrame exceeds maximumPayloadRasterHeight");
    }
    if (frame.payloadByteSize() <= 0) {
        return QStringLiteral("ImageFrame payload byte size is invalid");
    }
    if (frame.payloadByteSize() > ImageSequenceLimits::maximumPayloadBytes()) {
        return QStringLiteral("ImageFrame exceeds maximumPayloadBytes");
    }
    if (frame.formatIdentifier().toUcs4().size()
        > ImageSequenceLimits::maximumFormatIdentifierCharacters()) {
        return QStringLiteral("ImageFrame exceeds maximumFormatIdentifierCharacters");
    }

    return {};
}

}
