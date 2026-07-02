#pragma once

#include "imageviewport.h"

#include <QtCore/QRegularExpression>

#include <cmath>

namespace ImageViewportInternal {

enum class ContentPlacementMode {
    Contain,
    Cover,
    Stretch,
    Center,
};

enum class ContentHorizontalPlacement {
    AlignLeft,
    AlignHCenter,
    AlignRight,
};

enum class ContentVerticalPlacement {
    AlignTop,
    AlignVCenter,
    AlignBottom,
};

constexpr int minimumMaximumLogicalSide = 8192;
constexpr qint64 minimumMaximumPixelsPerFrame = 67108864LL;
constexpr qint64 minimumMaximumPayloadBytesPerFrame = 268435456LL;
constexpr int minimumMaximumTimedListFrameCount = 10000;
constexpr int minimumMaximumDuration = 86400000;
constexpr int minimumMaximumDiagnosticStringLength = 4096;

inline bool isFinitePositive(double value) { return std::isfinite(value) && value > 0.0; }

inline bool isPositiveFiniteInteger(double value)
{
    return std::isfinite(value) && value > 0.0 && std::trunc(value) == value;
}

inline bool isAdmittedLogicalSizeComponent(double value, int maximum)
{
    return isPositiveFiniteInteger(value) && value <= static_cast<double>(maximum);
}

inline bool isFinitePoint(QPointF point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

inline bool rectsExactlyEqual(const QRectF& left, const QRectF& right)
{
    return left.x() == right.x() && left.y() == right.y() && left.width() == right.width()
        && left.height() == right.height();
}

inline bool rectsDifferExactly(const QRectF& left, const QRectF& right)
{
    return !rectsExactlyEqual(left, right);
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

inline ImageViewport::TriState capabilitySupportToTriState(
    ImageSequenceProviderCapabilitySupport support)
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

inline bool providerCapabilityContradictsMetadata(
    ImageSequenceProviderCapabilitySupport support, bool metadataCapability)
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

inline bool providerCapabilityKnownTrue(ImageSequenceProviderCapabilitySupport support)
{
    return support == ImageSequenceProviderCapabilitySupport::DeclaredTrue
        || support == ImageSequenceProviderCapabilitySupport::KnownTrue;
}

inline bool providerResolvedCapability(
    ImageSequenceProviderCapabilitySupport support, bool defaultSupport)
{
    if (providerCapabilityKnownFalse(support)) {
        return false;
    }
    if (providerCapabilityKnownTrue(support)) {
        return true;
    }
    return defaultSupport;
}

inline bool providerFactsContradictCapabilities(const ImageSequenceProviderKnownFacts& facts,
    ImageSequenceProviderCapabilitySupport timedPlaybackSupport,
    ImageSequenceProviderCapabilitySupport,
    ImageSequenceProviderCapabilitySupport positionSeekSupport)
{
    if (!facts.isSpecified() || facts.isLogicalSizeOnly()) {
        return false;
    }

    const bool timedFacts = facts.isTimedFrameCount() || facts.isTimedFrameList();
    return providerCapabilityKnownTrue(timedPlaybackSupport) && !timedFacts
        || providerCapabilityKnownTrue(positionSeekSupport) && !timedFacts;
}

inline bool providerFactsContradictMetadata(
    const ImageSequenceProviderKnownFacts& facts, const ImageSequenceProviderMetadata& metadata)
{
    if (!facts.isSpecified()) {
        return false;
    }
    if (metadata.logicalSize() != facts.logicalSize()) {
        return true;
    }
    if (facts.isLogicalSizeOnly()) {
        return false;
    }
    if (facts.isStill()) {
        return !metadata.isStill();
    }
    if (facts.isTimedFrameCount()) {
        return !metadata.isTimedFrameList()
            || metadata.frameDurations().size() != facts.frameCount();
    }
    if (facts.isTimedFrameList()) {
        return !metadata.isTimedFrameList() || metadata.frameDurations() != facts.frameDurations();
    }
    return false;
}

inline bool isValidContentPlacementMode(ContentPlacementMode mode)
{
    switch (mode) {
    case ContentPlacementMode::Contain:
    case ContentPlacementMode::Cover:
    case ContentPlacementMode::Stretch:
    case ContentPlacementMode::Center:
        return true;
    }

    return false;
}

inline bool isValidFitMode(ImageViewport::FitMode mode)
{
    switch (mode) {
    case ImageViewport::FitMode::Contain:
    case ImageViewport::FitMode::FitWidth:
    case ImageViewport::FitMode::FitHeight:
    case ImageViewport::FitMode::Manual:
        return true;
    }

    return false;
}

inline bool isValidSpreadDirection(ImageViewport::SpreadDirection direction)
{
    switch (direction) {
    case ImageViewport::SpreadDirection::LeftToRight:
    case ImageViewport::SpreadDirection::RightToLeft:
        return true;
    }

    return false;
}

inline bool isValidPageRole(ImageViewport::PageRole role)
{
    switch (role) {
    case ImageViewport::PageRole::Primary:
    case ImageViewport::PageRole::Secondary:
        return true;
    }

    return false;
}

inline bool isValidContentHorizontalPlacement(ContentHorizontalPlacement alignment)
{
    switch (alignment) {
    case ContentHorizontalPlacement::AlignLeft:
    case ContentHorizontalPlacement::AlignHCenter:
    case ContentHorizontalPlacement::AlignRight:
        return true;
    }

    return false;
}

inline bool isValidContentVerticalPlacement(ContentVerticalPlacement alignment)
{
    switch (alignment) {
    case ContentVerticalPlacement::AlignTop:
    case ContentVerticalPlacement::AlignVCenter:
    case ContentVerticalPlacement::AlignBottom:
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
    static const QRegularExpression windowsPathPattern(
        QStringLiteral("\\b[A-Za-z]:[\\\\/][^\\s]+"));
    static const QRegularExpression unixPathPattern(
        QStringLiteral("(?<!\\w)/(?:[^\\s/]+/)+[^\\s]+"));

    diagnostic.replace(credentialPattern, QStringLiteral("[redacted-credential]"));
    diagnostic.replace(urlPattern, QStringLiteral("[redacted-url]"));
    diagnostic.replace(windowsPathPattern, QStringLiteral("[redacted-path]"));
    diagnostic.replace(unixPathPattern, QStringLiteral("[redacted-path]"));
    return diagnostic;
}

inline QString plainTextDiagnostic(QString diagnostic)
{
    static const QRegularExpression markupPattern(QStringLiteral("<[^>]*>"));
    diagnostic.replace(markupPattern, QStringLiteral(" "));

    QString plain;
    plain.reserve(diagnostic.size());
    bool pendingSpace = false;
    for (QChar character : diagnostic) {
        const ushort codeUnit = character.unicode();
        if (character.isSpace() || codeUnit < 0x20 || codeUnit == 0x7f) {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !plain.isEmpty()) {
            plain += QLatin1Char(' ');
        }
        plain += character;
        pendingSpace = false;
    }
    return plain.trimmed();
}

}
