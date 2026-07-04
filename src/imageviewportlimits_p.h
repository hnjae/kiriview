#pragma once

#include "imageviewport.h"

#include <cmath>

namespace ImageViewportInternal {

constexpr int minimumMaximumLogicalSide = 8192;
constexpr qint64 minimumMaximumPixelsPerFrame = 67108864LL;
constexpr qint64 minimumMaximumPayloadBytesPerFrame = 268435456LL;
constexpr int minimumMaximumTimedListFrameCount = 10000;
constexpr int minimumMaximumDuration = 86400000;
constexpr int minimumMaximumDiagnosticStringLength = 4096;

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
    const QSizeF size = frame.logicalSize();
    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width > ImageSequenceLimits::maximumLogicalWidth()) {
        return QStringLiteral("ImageFrame exceeds maximumLogicalWidth");
    }
    if (height > ImageSequenceLimits::maximumLogicalHeight()) {
        return QStringLiteral("ImageFrame exceeds maximumLogicalHeight");
    }
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return QStringLiteral("ImageFrame exceeds maximumPixelsPerFrame");
    }
    if (frame.payloadByteSize() <= 0) {
        return QStringLiteral("ImageFrame payload byte size is invalid");
    }
    if (frame.payloadByteSize() > ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
        return QStringLiteral("ImageFrame exceeds maximumPayloadBytesPerFrame");
    }

    return {};
}

}
