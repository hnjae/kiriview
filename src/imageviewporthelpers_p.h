#pragma once

#include "imageviewport.h"

#include <QtCore/QRegularExpression>

#include <cmath>

namespace ImageViewportInternal {

constexpr int minimumMaximumLogicalSide = 8192;
constexpr qint64 minimumMaximumPixelsPerFrame = 67108864LL;
constexpr qint64 minimumMaximumPayloadBytesPerFrame = 268435456LL;
constexpr int minimumMaximumTimedListFrameCount = 10000;
constexpr int minimumMaximumDuration = 86400000;
constexpr int minimumMaximumDiagnosticStringLength = 4096;

inline bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

inline bool isPositiveFiniteInteger(double value)
{
    return std::isfinite(value) && value > 0.0 && std::trunc(value) == value;
}

inline bool isAdmittedLogicalSizeComponent(double value, int maximum)
{
    return isPositiveFiniteInteger(value) && value <= static_cast<double>(maximum);
}

inline bool isFinitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

inline QString frameLimitViolation(const ImageFrame &frame)
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

inline QString providerMetadataLimitViolation(const ImageSequenceProviderMetadata &metadata)
{
    if (!metadata.isSpecified()) {
        return {};
    }
    if (!metadata.isValid()) {
        return QStringLiteral("provider metadata is invalid");
    }

    const QSizeF size = metadata.logicalSize();
    if (!isPositiveFiniteInteger(size.width()) || !isPositiveFiniteInteger(size.height())) {
        return QStringLiteral("provider metadata is invalid");
    }
    if (size.width() > ImageSequenceLimits::maximumLogicalWidth()) {
        return QStringLiteral("provider metadata logical width exceeds maximumLogicalWidth");
    }
    if (size.height() > ImageSequenceLimits::maximumLogicalHeight()) {
        return QStringLiteral("provider metadata logical height exceeds maximumLogicalHeight");
    }

    const qint64 width = static_cast<qint64>(size.width());
    const qint64 height = static_cast<qint64>(size.height());
    if (width * height > ImageSequenceLimits::maximumPixelsPerFrame()) {
        return QStringLiteral("provider metadata logical size exceeds maximumPixelsPerFrame");
    }

    if (!metadata.isTimedFrameList()) {
        return {};
    }

    const QVector<int> durations = metadata.frameDurations();
    if (durations.size() > ImageSequenceLimits::maximumTimedListFrameCount()) {
        return QStringLiteral("provider metadata frame count exceeds maximumTimedListFrameCount");
    }

    qint64 totalDuration = 0;
    for (int duration : durations) {
        if (duration <= 0) {
            return QStringLiteral("provider metadata frame duration must be positive");
        }
        if (duration > ImageSequenceLimits::maximumFrameDuration()) {
            return QStringLiteral("provider metadata frame duration exceeds maximumFrameDuration");
        }
        totalDuration += duration;
        if (totalDuration > ImageSequenceLimits::maximumTotalSequenceDuration()) {
            return QStringLiteral("provider metadata total duration exceeds maximumTotalSequenceDuration");
        }
    }

    return {};
}

inline ImageViewport::TriState capabilitySupportToTriState(ImageSequenceProviderCapabilitySupport support)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::DeclaredFalse:
    case ImageSequenceProviderCapabilitySupport::KnownFalse:
        return ImageViewport::TriState::False;
    case ImageSequenceProviderCapabilitySupport::DeclaredTrue:
    case ImageSequenceProviderCapabilitySupport::KnownTrue:
        return ImageViewport::TriState::True;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return ImageViewport::TriState::Unavailable;
    }

    return ImageViewport::TriState::Unavailable;
}

inline bool providerCapabilityContradictsMetadata(ImageSequenceProviderCapabilitySupport support, bool metadataCapability)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::DeclaredFalse:
    case ImageSequenceProviderCapabilitySupport::KnownFalse:
        return metadataCapability;
    case ImageSequenceProviderCapabilitySupport::DeclaredTrue:
    case ImageSequenceProviderCapabilitySupport::KnownTrue:
        return !metadataCapability;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return false;
    }

    return false;
}

inline bool providerCapabilityKnownFalse(ImageSequenceProviderCapabilitySupport support)
{
    return support == ImageSequenceProviderCapabilitySupport::DeclaredFalse
        || support == ImageSequenceProviderCapabilitySupport::KnownFalse;
}

inline bool isValidFillMode(ImageViewport::FillMode mode)
{
    switch (mode) {
    case ImageViewport::FillMode::Contain:
    case ImageViewport::FillMode::Cover:
    case ImageViewport::FillMode::Stretch:
    case ImageViewport::FillMode::Center:
        return true;
    }

    return false;
}

inline bool isValidHorizontalAlignment(ImageViewport::HorizontalAlignment alignment)
{
    switch (alignment) {
    case ImageViewport::HorizontalAlignment::AlignLeft:
    case ImageViewport::HorizontalAlignment::AlignHCenter:
    case ImageViewport::HorizontalAlignment::AlignRight:
        return true;
    }

    return false;
}

inline bool isValidVerticalAlignment(ImageViewport::VerticalAlignment alignment)
{
    switch (alignment) {
    case ImageViewport::VerticalAlignment::AlignTop:
    case ImageViewport::VerticalAlignment::AlignVCenter:
    case ImageViewport::VerticalAlignment::AlignBottom:
        return true;
    }

    return false;
}

inline bool isValidBackgroundMode(ImageViewport::BackgroundMode mode)
{
    switch (mode) {
    case ImageViewport::BackgroundMode::Transparent:
    case ImageViewport::BackgroundMode::SolidColor:
    case ImageViewport::BackgroundMode::Checkerboard:
        return true;
    }

    return false;
}

inline QString redactDiagnosticDetails(QString diagnostic)
{
    static const QRegularExpression credentialPattern(
        QStringLiteral("\\b(?:password|passwd|pwd|token|api[_-]?key|secret)\\s*[:=]\\s*\\S+"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression urlPattern(QStringLiteral("\\b[A-Za-z][A-Za-z0-9+.-]*://\\S+"));
    static const QRegularExpression windowsPathPattern(QStringLiteral("\\b[A-Za-z]:[\\\\/][^\\s]+"));
    static const QRegularExpression unixPathPattern(QStringLiteral("(?<!\\w)/(?:[^\\s/]+/)+[^\\s]+"));

    diagnostic.replace(credentialPattern, QStringLiteral("[redacted-credential]"));
    diagnostic.replace(urlPattern, QStringLiteral("[redacted-url]"));
    diagnostic.replace(windowsPathPattern, QStringLiteral("[redacted-path]"));
    diagnostic.replace(unixPathPattern, QStringLiteral("[redacted-path]"));
    return diagnostic;
}

}
