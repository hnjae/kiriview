/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imagesequence.h>

#include <cmath>
#include <limits>

namespace ImageViewportInternal {

struct ViewportDisplayLimits
{
    static constexpr double minimumManualZoomPercent() { return 10.0; }
    static constexpr double manualZoomStepFactor() { return 1.0905077326652577; }
    static constexpr double maximumPageGap() { return 8192.0; }
    static constexpr double minimumCheckerboardCellSize() { return 1.0; }
    static constexpr double maximumCheckerboardCellSize() { return 256.0; }
};

constexpr int minimumMaximumLogicalSide = std::numeric_limits<int>::max();
constexpr qint64 minimumMaximumPixelsPerFrame
    = qint64(std::numeric_limits<int>::max()) * qint64(std::numeric_limits<int>::max());
constexpr int minimumMaximumPayloadRasterSide = 16384;
constexpr qint64 minimumMaximumPayloadBytesPerFrame = qint64(512) * 1024 * 1024;
constexpr int minimumMaximumTimedListFrameCount = 10000;
constexpr int minimumMaximumDuration = 86400000;
constexpr int minimumMaximumDiagnosticStringLength = 4096;
constexpr int minimumMaximumFormatIdentifierLength = 256;

inline bool isPositiveFiniteInteger(double value)
{
    return std::isfinite(value) && value > 0.0 && std::trunc(value) == value;
}

inline bool logicalPixelCountExceedsLimit(qint64 width, qint64 height)
{
    const qint64 maximum = ImageSequenceLimits::maximumSourceLogicalPixels();
    return width <= 0 || height <= 0 || maximum <= 0 || width > maximum / height;
}

inline QString frameLimitViolation(const ImageFrame& frame)
{
    const QSizeF size = frame.sourceLogicalSize();
    if (!isPositiveFiniteInteger(size.width()) || !isPositiveFiniteInteger(size.height())) {
        return QStringLiteral("ImageFrame source logical size is invalid");
    }
    if (size.width() > ImageSequenceLimits::maximumSourceLogicalWidth()) {
        return QStringLiteral("ImageFrame exceeds maximumSourceLogicalWidth");
    }
    if (size.height() > ImageSequenceLimits::maximumSourceLogicalHeight()) {
        return QStringLiteral("ImageFrame exceeds maximumSourceLogicalHeight");
    }
    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (logicalPixelCountExceedsLimit(width, height)) {
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
