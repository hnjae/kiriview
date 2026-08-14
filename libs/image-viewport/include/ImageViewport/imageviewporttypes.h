/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <QtCore/QDebug>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QSizeF>
#include <QtQmlIntegration/qqmlintegration.h>

class ImageViewportRevisionsSnapshot;
class ImageViewportDisplaySnapshot;
class ImageViewportRequestSnapshot;
class ImageViewportRoleRequestSnapshot;
class ImageViewportRoleDisplaySnapshot;
class ImageSequenceProviderFailureHandle;

namespace ImageSequenceEnums {
Q_NAMESPACE

enum class AuthoredAnimationLoopMode {
    Unavailable,
    PlayOnce,
    Finite,
    Infinite,
};
Q_ENUM_NS(AuthoredAnimationLoopMode)
}

namespace ImageViewportEnums {
Q_NAMESPACE

enum class PageRole {
    Primary,
    Secondary,
};
Q_ENUM_NS(PageRole)

enum class CapabilitySupport {
    Unavailable,
    False,
    True,
};
Q_ENUM_NS(CapabilitySupport)

enum class QualityPreference {
    Default,
    FastFirstDisplay,
    BalancedDetail,
    ExactDetail,
};
Q_ENUM_NS(QualityPreference)

enum class ExactnessPreference {
    Default,
    AllowInexact,
    PreferExact,
    RequireExact,
};
Q_ENUM_NS(ExactnessPreference)

enum class PayloadQuality {
    Unknown,
    Preview,
    FirstDisplay,
    BoundedDetail,
    Exact,
};
Q_ENUM_NS(PayloadQuality)

enum class PayloadExactness {
    Unknown,
    NotExact,
    ExactForSource,
};
Q_ENUM_NS(PayloadExactness)

enum class SpreadDirection {
    LeftToRight,
    RightToLeft,
};
Q_ENUM_NS(SpreadDirection)

enum class FitMode {
    Contain,
    FitWidth,
    FitHeight,
    Manual,
};
Q_ENUM_NS(FitMode)

enum class ContentAnchor {
    Start,
    End,
};
Q_ENUM_NS(ContentAnchor)

enum class RequestStatus {
    NoRequest,
    Loading,
    Ready,
    Unsupported,
    Error,
};
Q_ENUM_NS(RequestStatus)

enum class RequestReason {
    NoRequest,
    ProviderWaiting,
    RequestQueued,
    UploadPending,
    RenderWaiting,
    Ready,
    UnsupportedRequest,
    InvalidRequest,
    ProviderFailure,
    PayloadRejection,
    RenderFailure,
};
Q_ENUM_NS(RequestReason)

enum class CommandReason {
    NoCommand,
    IgnoredNoRequest,
    InvalidRequest,
    UnsupportedRequest,
};
Q_ENUM_NS(CommandReason)

enum class DisplayStatus {
    Empty,
    Ready,
    Retained,
};
Q_ENUM_NS(DisplayStatus)

enum class DisplayPhase {
    NoPresentation,
    PreviousActive,
    TransitioningPlaceholder,
    CommittedActive,
};
Q_ENUM_NS(DisplayPhase)

enum class PlaybackPhase {
    Stopped,
    Playing,
    Waiting,
    Paused,
};
Q_ENUM_NS(PlaybackPhase)

enum class CommandOutcome {
    Accepted,
    Invalid,
    Unsupported,
    IgnoredNoRequest,
};
Q_ENUM_NS(CommandOutcome)

enum class BackgroundMode {
    Transparent,
    SolidColor,
    Checkerboard,
};
Q_ENUM_NS(BackgroundMode)

enum class CoordinateSpace {
    Item,
    DisplayedSpread,
    DisplayedPage,
};
Q_ENUM_NS(CoordinateSpace)

enum class SequenceProviderFailureCause {
    Unavailable,
    SourceAccess,
    Decode,
    ResourceExhausted,
    ProviderInternal,
};
Q_ENUM_NS(SequenceProviderFailureCause)

enum class FailureContext {
    Unavailable,
    CurrentRequest,
    RestoredTransition,
};
Q_ENUM_NS(FailureContext)

enum class FailureScope {
    Unavailable,
    Generation,
    DisplayRequest,
};
Q_ENUM_NS(FailureScope)
}

using ImageViewportPageRole = ImageViewportEnums::PageRole;
using ImageViewportCapabilitySupport = ImageViewportEnums::CapabilitySupport;
using ImageViewportQualityPreference = ImageViewportEnums::QualityPreference;
using ImageViewportExactnessPreference = ImageViewportEnums::ExactnessPreference;
using ImageViewportPayloadQuality = ImageViewportEnums::PayloadQuality;
using ImageViewportPayloadExactness = ImageViewportEnums::PayloadExactness;
using ImageViewportSpreadDirection = ImageViewportEnums::SpreadDirection;
using ImageViewportFitMode = ImageViewportEnums::FitMode;
using ImageViewportContentAnchor = ImageViewportEnums::ContentAnchor;
using ImageViewportRequestStatus = ImageViewportEnums::RequestStatus;
using ImageViewportRequestReason = ImageViewportEnums::RequestReason;
using ImageViewportCommandReason = ImageViewportEnums::CommandReason;
using ImageViewportDisplayStatus = ImageViewportEnums::DisplayStatus;
using ImageViewportDisplayPhase = ImageViewportEnums::DisplayPhase;
using ImageViewportPlaybackPhase = ImageViewportEnums::PlaybackPhase;
using ImageViewportCommandOutcome = ImageViewportEnums::CommandOutcome;
using ImageViewportBackgroundMode = ImageViewportEnums::BackgroundMode;
using ImageViewportCoordinateSpace = ImageViewportEnums::CoordinateSpace;
using ImageSequenceProviderFailureCause = ImageViewportEnums::SequenceProviderFailureCause;
using ImageViewportFailureContext = ImageViewportEnums::FailureContext;
using ImageViewportFailureScope = ImageViewportEnums::FailureScope;
using ImageSequenceAuthoredAnimationLoopMode = ImageSequenceEnums::AuthoredAnimationLoopMode;

class ImageSequenceAuthoredAnimationFacts
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceAuthoredAnimationFacts)
    Q_PROPERTY(bool autoplay READ autoplay CONSTANT)
    Q_PROPERTY(ImageSequenceAuthoredAnimationLoopMode loopMode READ loopMode CONSTANT)
    Q_PROPERTY(int loopCount READ loopCount CONSTANT)

public:
    ImageSequenceAuthoredAnimationFacts() = default;
    static ImageSequenceAuthoredAnimationFacts finiteLoop(int loopCount);
    static ImageSequenceAuthoredAnimationFacts infiniteLoop();

    [[nodiscard]] bool autoplay() const;
    void setAutoplay(bool autoplay);
    [[nodiscard]] ImageSequenceAuthoredAnimationLoopMode loopMode() const;
    [[nodiscard]] int loopCount() const;
    bool setFiniteLoopCount(int loopCount);
    [[nodiscard]] bool isValid() const;

private:
    bool m_autoplay = false;
    ImageSequenceAuthoredAnimationLoopMode m_loopMode
        = ImageSequenceAuthoredAnimationLoopMode::PlayOnce;
    int m_loopCount = 1;
};

class ImageSequenceProviderFailureReference
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceProviderFailureReference)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageSequenceProviderFailureReference() = default;

    [[nodiscard]] bool isValid() const { return m_value != 0; }
    Q_INVOKABLE [[nodiscard]] bool equals(ImageSequenceProviderFailureReference other) const
    {
        return m_value == other.m_value;
    }

    friend bool operator==(
        ImageSequenceProviderFailureReference lhs, ImageSequenceProviderFailureReference rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

private:
    explicit ImageSequenceProviderFailureReference(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageSequenceProviderFailureHandle;
};

class ImageViewportRange
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRange)
    Q_PROPERTY(int minimum READ minimum CONSTANT)
    Q_PROPERTY(int maximum READ maximum CONSTANT)

public:
    ImageViewportRange() = default;
    ImageViewportRange(int minimum, int maximum)
        : m_minimum(minimum)
        , m_maximum(maximum)
    {
    }

    [[nodiscard]] int minimum() const { return m_minimum; }
    [[nodiscard]] int maximum() const { return m_maximum; }

    friend bool operator==(ImageViewportRange lhs, ImageViewportRange rhs)
    {
        return lhs.m_minimum == rhs.m_minimum && lhs.m_maximum == rhs.m_maximum;
    }

private:
    int m_minimum = -1;
    int m_maximum = -1;
};

class ImageViewportRevisionToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRevisionToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportRevisionToken() = default;

    [[nodiscard]] bool isValid() const { return m_value != 0; }
    Q_INVOKABLE [[nodiscard]] bool equals(ImageViewportRevisionToken other) const
    {
        return m_value == other.m_value;
    }

    friend bool operator==(ImageViewportRevisionToken lhs, ImageViewportRevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

private:
    explicit ImageViewportRevisionToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageViewportRevisionsSnapshot;
    friend class ImageViewportDisplaySnapshot;
    friend class ImageViewportTypesPrivateAccess;
};

class ImageViewportPresentationTargetGenerationToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationTargetGenerationToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportPresentationTargetGenerationToken() = default;

    [[nodiscard]] bool isValid() const { return m_value != 0; }
    Q_INVOKABLE [[nodiscard]] bool equals(
        ImageViewportPresentationTargetGenerationToken other) const
    {
        return m_value == other.m_value;
    }

    friend bool operator==(ImageViewportPresentationTargetGenerationToken lhs,
        ImageViewportPresentationTargetGenerationToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

private:
    explicit ImageViewportPresentationTargetGenerationToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageViewportRequestSnapshot;
    friend class ImageViewportDisplaySnapshot;
    friend class ImageViewportRoleRequestSnapshot;
    friend class ImageViewportTypesPrivateAccess;
};

class ImageViewportDemandRevisionToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportDemandRevisionToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportDemandRevisionToken() = default;

    [[nodiscard]] bool isValid() const { return m_value != 0; }
    Q_INVOKABLE [[nodiscard]] bool equals(ImageViewportDemandRevisionToken other) const
    {
        return m_value == other.m_value;
    }

    friend bool operator==(
        ImageViewportDemandRevisionToken lhs, ImageViewportDemandRevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

private:
    explicit ImageViewportDemandRevisionToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageViewportRoleRequestSnapshot;
    friend class ImageViewportRoleDisplaySnapshot;
    friend class ImageViewportTypesPrivateAccess;
};

class ImageViewportAllocationGenerationToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportAllocationGenerationToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportAllocationGenerationToken() = default;

    [[nodiscard]] bool isValid() const { return m_value != 0; }
    Q_INVOKABLE [[nodiscard]] bool equals(ImageViewportAllocationGenerationToken other) const
    {
        return m_value == other.m_value;
    }

    friend bool operator==(
        ImageViewportAllocationGenerationToken lhs, ImageViewportAllocationGenerationToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }

private:
    explicit ImageViewportAllocationGenerationToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageViewportTypesPrivateAccess;
};

class ImageViewportRoleSet
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleSet)
    Q_PROPERTY(bool primary READ primary CONSTANT)
    Q_PROPERTY(bool secondary READ secondary CONSTANT)

public:
    ImageViewportRoleSet() = default;
    ImageViewportRoleSet(bool primary, bool secondary)
        : m_primary(primary)
        , m_secondary(secondary)
    {
    }

    [[nodiscard]] bool primary() const { return m_primary; }
    [[nodiscard]] bool secondary() const { return m_secondary; }

    friend bool operator==(ImageViewportRoleSet lhs, ImageViewportRoleSet rhs)
    {
        return lhs.m_primary == rhs.m_primary && lhs.m_secondary == rhs.m_secondary;
    }

private:
    bool m_primary = false;
    bool m_secondary = false;
};

inline QDebug operator<<(QDebug debug, ImageViewportRange range)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "ImageViewportRange(minimum=" << range.minimum()
                    << ", maximum=" << range.maximum() << ")";
    return debug;
}

Q_DECLARE_METATYPE(ImageViewportRange)
Q_DECLARE_METATYPE(ImageViewportRevisionToken)
Q_DECLARE_METATYPE(ImageViewportPresentationTargetGenerationToken)
Q_DECLARE_METATYPE(ImageViewportDemandRevisionToken)
Q_DECLARE_METATYPE(ImageViewportAllocationGenerationToken)
Q_DECLARE_METATYPE(ImageViewportRoleSet)
Q_DECLARE_METATYPE(ImageSequenceProviderFailureReference)
Q_DECLARE_METATYPE(ImageSequenceAuthoredAnimationFacts)
