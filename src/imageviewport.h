#pragma once

#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

class ImageSequenceProviderDescriptor;
class ImageSequenceProviderDisplayDemand;
class ImageSequenceProviderEvent;
class ImageSequenceProviderFrameEnvelope;
class ImageSequenceProviderMetadata;
class ImageSequenceProviderRequest;
class ImageViewportPrivate;
class ImageViewportCommandResult;
class ImageViewportCoordinateInput;
class ImageViewportCoordinateResult;
class ImageViewportDiagnosticsSnapshot;
class ImageViewportDisplaySnapshot;
class ImageViewportPresentationTarget;
class ImageViewportPresentationCommand;
class ImageViewportPresentationSnapshot;
class ImageViewportRange;
class ImageViewportRequestSnapshot;
class ImageViewportRevisionsSnapshot;
class ImageViewportRoleDisplaySnapshot;
class ImageViewportRoleGeometrySnapshot;
class ImageViewportRoleMetadataSnapshot;
class ImageViewportRoleRequestSnapshot;
class ImageViewportRoleSnapshot;
class ImageViewportStateSnapshot;
class RevisionToken;
class PresentationTargetTransitionPolicy;
class TimingIntervals;

namespace ImageViewportInternal {
class ImageFramePrivateAccess;
class ImageSequenceData;
class ImageSequencePrivateAccess;
class ProviderRequestTokenPrivateAccess;
class RevisionTokenPrivateAccess;
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
}

using ImageViewportPageRole = ImageViewportEnums::PageRole;
using ImageViewportCapabilitySupport = ImageViewportEnums::CapabilitySupport;
using ImageViewportQualityPreference = ImageViewportEnums::QualityPreference;
using ImageViewportExactnessPreference = ImageViewportEnums::ExactnessPreference;
using ImageViewportPayloadQuality = ImageViewportEnums::PayloadQuality;
using ImageViewportPayloadExactness = ImageViewportEnums::PayloadExactness;

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

using ImageSequenceAuthoredAnimationLoopMode = ImageSequenceEnums::AuthoredAnimationLoopMode;

enum class ImageSequenceProviderThreadingContract {
    AffinityBound,
    ThreadSafe,
};

class ImageSequenceAuthoredAnimationFacts
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceAuthoredAnimationFacts)
    Q_PROPERTY(bool autoplay READ autoplay CONSTANT)
    Q_PROPERTY(bool progressiveAnimationReadiness READ progressiveAnimationReadiness CONSTANT)
    Q_PROPERTY(ImageSequenceAuthoredAnimationLoopMode loopMode READ loopMode CONSTANT)
    Q_PROPERTY(int loopCount READ loopCount CONSTANT)

public:
    ImageSequenceAuthoredAnimationFacts() = default;
    static ImageSequenceAuthoredAnimationFacts finiteLoop(int loopCount);
    static ImageSequenceAuthoredAnimationFacts infiniteLoop();

    bool autoplay() const;
    void setAutoplay(bool autoplay);
    bool progressiveAnimationReadiness() const;
    void setProgressiveAnimationReadiness(bool progressiveAnimationReadiness);
    ImageSequenceAuthoredAnimationLoopMode loopMode() const;
    int loopCount() const;
    bool setFiniteLoopCount(int loopCount);
    bool isValid() const;

private:
    bool m_autoplay = false;
    bool m_progressiveAnimationReadiness = false;
    ImageSequenceAuthoredAnimationLoopMode m_loopMode
        = ImageSequenceAuthoredAnimationLoopMode::PlayOnce;
    int m_loopCount = 1;
};

class ImageSequence : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use ImageSequenceFactory to create sequence handles")

public:
    ~ImageSequence() override;

private:
    explicit ImageSequence(
        std::unique_ptr<ImageViewportInternal::ImageSequenceData> data, QObject* parent = nullptr);

    std::unique_ptr<ImageViewportInternal::ImageSequenceData> d;

    friend class ImageViewportInternal::ImageSequencePrivateAccess;
};

class ImageFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageFrame objects are created by C++ helpers or provider adapters")
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(qint64 payloadByteSize READ payloadByteSize CONSTANT)
    Q_PROPERTY(QSizeF payloadRasterSize READ payloadRasterSize CONSTANT)
    Q_PROPERTY(QSizeF sourceToPayloadScale READ sourceToPayloadScale CONSTANT)
    Q_PROPERTY(ImageViewportPayloadQuality quality READ quality CONSTANT)
    Q_PROPERTY(ImageViewportPayloadExactness exactness READ exactness CONSTANT)
    Q_PROPERTY(bool hasAlpha READ hasAlpha CONSTANT)
    Q_PROPERTY(OrientationPolicy orientationPolicy READ orientationPolicy CONSTANT)
    Q_PROPERTY(QString formatIdentifier READ formatIdentifier CONSTANT)

public:
    enum class OrientationPolicy {
        Identity,
        MirrorHorizontally,
        MirrorVertically,
        Rotate180,
        Rotate90,
        MirrorHorizontallyAndRotate90,
        MirrorVerticallyAndRotate90,
        Rotate270,
    };
    Q_ENUM(OrientationPolicy)

    explicit ImageFrame(QObject* parent = nullptr);
    explicit ImageFrame(const QImage& image, QObject* parent = nullptr);
    ImageFrame(const QImage& image, OrientationPolicy orientationPolicy, QObject* parent = nullptr);
    ImageFrame(const QImage& image, QSizeF sourceLogicalSize, QSizeF payloadRasterSize,
        QSizeF sourceToPayloadScale, qint64 payloadByteSize, ImageViewportPayloadQuality quality,
        ImageViewportPayloadExactness exactness, bool hasAlpha, OrientationPolicy orientationPolicy,
        QString formatIdentifier, QObject* parent = nullptr);

    bool isValid() const;
    QSizeF sourceLogicalSize() const;
    qint64 payloadByteSize() const;
    QSizeF payloadRasterSize() const;
    QSizeF sourceToPayloadScale() const;
    ImageViewportPayloadQuality quality() const;
    ImageViewportPayloadExactness exactness() const;
    bool hasAlpha() const;
    OrientationPolicy orientationPolicy() const;
    QString formatIdentifier() const;

private:
    ImageFrame(const QImage& image, qsizetype payloadByteSizeOverride, QObject* parent = nullptr);
    const QImage& imagePayload() const;

    QImage m_image;
    QSizeF m_logicalSize;
    qint64 m_payloadByteSize = 0;
    QSizeF m_payloadRasterSize;
    QSizeF m_sourceToPayloadScale;
    ImageViewportPayloadQuality m_quality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness m_exactness = ImageViewportPayloadExactness::Unknown;
    bool m_hasAlpha = false;
    OrientationPolicy m_orientationPolicy = OrientationPolicy::Identity;
    QString m_formatIdentifier;

    friend class ImageSequenceFactory;
    friend class TimedImageFrame;
    friend class TimedImageFrameList;
    friend class ImageViewport;
    friend class ImageViewportInternal::ImageFramePrivateAccess;
};

class TimedImageFrame
{
    Q_GADGET
    QML_VALUE_TYPE(timedImageFrame)
    Q_PROPERTY(ImageFrame* frame READ frame CONSTANT)
    Q_PROPERTY(int startPosition READ startPosition CONSTANT)
    Q_PROPERTY(int duration READ duration CONSTANT)

public:
    TimedImageFrame() = default;
    TimedImageFrame(ImageFrame* frame, int startPosition, int duration);

    ImageFrame* frame() const;
    int startPosition() const;
    int duration() const;
    bool isValid() const;

private:
    std::shared_ptr<ImageFrame> m_frame;
    int m_startPosition = -1;
    int m_duration = -1;
};

class ImageSequenceProviderFrameHandle : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageSequenceProviderFrameHandle objects are created by provider adapters")

public:
    using ReleaseCallback = std::function<void(ImageFrame*)>;

    explicit ImageSequenceProviderFrameHandle(
        std::unique_ptr<ImageFrame> frame, QObject* parent = nullptr);
    ImageSequenceProviderFrameHandle(
        ImageFrame* frame, ReleaseCallback releaseFrame, QObject* parent = nullptr);
    ~ImageSequenceProviderFrameHandle() override;

    ImageFrame* frame() const;
    void release();

private:
    ImageFrame* m_frame = nullptr;
    ReleaseCallback m_releaseFrame;
    bool m_released = false;
};

class TimedImageFrameList : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QList<TimedImageFrame> frames READ frames NOTIFY countChanged)
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY animationFactsChanged)
    Q_PROPERTY(
        ImageSequenceAuthoredAnimationLoopMode loopMode READ loopMode NOTIFY animationFactsChanged)
    Q_PROPERTY(int loopCount READ loopCount NOTIFY animationFactsChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)

public:
    explicit TimedImageFrameList(QObject* parent = nullptr);

    int count() const;
    QList<TimedImageFrame> frames() const;
    QString errorString() const;
    bool autoplay() const;
    void setAutoplay(bool autoplay);
    ImageSequenceAuthoredAnimationLoopMode loopMode() const;
    int loopCount() const;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts() const;
    void setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    bool appendFrame(const QImage& image, int durationMilliseconds);
    Q_INVOKABLE bool appendFrame(ImageFrame* frame, int durationMilliseconds);
    Q_INVOKABLE bool appendFrame(const TimedImageFrame& frame);
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void animationFactsChanged();
    void diagnosticsChanged();

private:
    bool isValid() const;
    QSizeF logicalSize() const;
    QVector<int> frameDurations() const;
    QVector<QImage> frameImages() const;
    int totalDuration() const;
    void setErrorString(const QString& errorString);

    QSizeF m_logicalSize;
    QVector<int> m_frameDurations;
    QVector<QImage> m_images;
    QList<TimedImageFrame> m_frames;
    ImageSequenceAuthoredAnimationFacts m_authoredAnimationFacts;
    QString m_errorString;

    friend class ImageSequenceFactory;
};

class ImageSequenceProviderAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use a concrete provider adapter supplied by C++ or module helpers")

public:
    explicit ImageSequenceProviderAdapter(QObject* parent = nullptr);
    virtual ImageSequenceProviderDescriptor descriptor() const = 0;
};

class ImageSequenceProviderRequestToken
{
public:
    ImageSequenceProviderRequestToken() = default;

    bool isValid() const;

    friend bool operator==(
        ImageSequenceProviderRequestToken left, ImageSequenceProviderRequestToken right)
    {
        return left.m_id == right.m_id;
    }

    friend bool operator!=(
        ImageSequenceProviderRequestToken left, ImageSequenceProviderRequestToken right)
    {
        return !(left == right);
    }

private:
    explicit ImageSequenceProviderRequestToken(quint64 id);

    quint64 m_id = 0;

    friend class ImageViewportInternal::ProviderRequestTokenPrivateAccess;
};

class ImageSequenceProviderMetadata
{
public:
    enum class Kind {
        Invalid,
        Still,
        FixedDurationFrames,
        TimedFrameList,
    };

    ImageSequenceProviderMetadata() = default;
    static ImageSequenceProviderMetadata still(QSizeF logicalSize);
    static ImageSequenceProviderMetadata fixedDurationFrames(
        QSizeF logicalSize, int frameCount, int frameDuration);
    static ImageSequenceProviderMetadata timedFrameList(
        QSizeF logicalSize, QVector<int> frameDurations);
    static ImageSequenceProviderMetadata withSourceLogicalSize(QSizeF sourceLogicalSize);
    static ImageSequenceProviderMetadata timedFrameCount(QSizeF logicalSize, int frameCount);

    bool isSpecified() const;
    bool hasCompleteModel() const;
    bool isValid() const;
    bool isStill() const;
    bool isTimedFrameList() const;
    QSizeF sourceLogicalSize() const;
    int frameCount() const;
    int totalDuration() const;
    ImageViewportRange frameSeekBounds() const;
    ImageViewportRange positionSeekBounds() const;
    QVector<int> frameDurations() const;
    bool hasAuthoredAnimationFacts() const;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts() const;
    void setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    void setTimedPlaybackSupport(ImageViewportCapabilitySupport support);
    void setFrameSeekSupport(ImageViewportCapabilitySupport support);
    void setPositionSeekSupport(ImageViewportCapabilitySupport support);
    ImageViewportCapabilitySupport timedPlaybackSupport() const;
    ImageViewportCapabilitySupport frameSeekSupport() const;
    ImageViewportCapabilitySupport positionSeekSupport() const;
    ImageViewportCapabilitySupport autoplay() const;
    ImageSequenceAuthoredAnimationLoopMode authoredLoopMode() const;
    int authoredLoopCount() const;

private:
    Kind m_kind = Kind::Invalid;
    QSizeF m_logicalSize;
    QVector<int> m_frameDurations;
    bool m_hasAuthoredAnimationFacts = false;
    ImageSequenceAuthoredAnimationFacts m_authoredAnimationFacts;
    std::optional<bool> m_timedPlaybackSupport;
    std::optional<bool> m_frameSeekSupport;
    std::optional<bool> m_positionSeekSupport;
    int m_constructionFrameCount = -1;
};

class ImageSequenceProviderFrameMetadata
{
public:
    enum class Kind {
        Invalid,
        Still,
        TimedFrame,
    };

    ImageSequenceProviderFrameMetadata() = default;
    static ImageSequenceProviderFrameMetadata stillFrame();
    static ImageSequenceProviderFrameMetadata timedFrame(
        int frame, int frameStartPosition, int frameDuration = -1);

    bool isValid() const;
    bool isStillFrame() const;
    bool isTimedFrame() const;
    int frame() const;
    int frameStartPosition() const;
    int frameDuration() const;

private:
    Kind m_kind = Kind::Invalid;
    int m_frame = -1;
    int m_frameStartPosition = -1;
    int m_frameDuration = -1;
};

class ImageSequenceProviderSession : public QObject
{
    Q_OBJECT

public:
    enum class UnsupportedCause {
        UnsupportedRequest,
        PayloadRejection,
    };
    Q_ENUM(UnsupportedCause)

    explicit ImageSequenceProviderSession(QObject* parent = nullptr);
    ~ImageSequenceProviderSession() override = default;

    virtual void request(const ImageSequenceProviderRequest& request) = 0;

signals:
    void providerEvent(const ImageSequenceProviderEvent& event);
};

enum class ImageSequenceProviderSessionFactoryOutcome {
    Created,
    Failed,
};

class ImageSequenceProviderSessionFactoryResult
{
public:
    ImageSequenceProviderSessionFactoryResult() = default;
    static ImageSequenceProviderSessionFactoryResult created(ImageSequenceProviderSession* session);
    static ImageSequenceProviderSessionFactoryResult failed(QString diagnostic = {});

    ImageSequenceProviderSessionFactoryOutcome outcome() const;
    ImageSequenceProviderSession* session() const;
    QString diagnostic() const;

private:
    ImageSequenceProviderSessionFactoryOutcome m_outcome
        = ImageSequenceProviderSessionFactoryOutcome::Failed;
    QPointer<ImageSequenceProviderSession> m_session;
    QString m_diagnostic;
};

namespace ImageSequenceFactoryEnums {
Q_NAMESPACE

enum class FactoryOutcome {
    Created,
    Rejected,
};
Q_ENUM_NS(FactoryOutcome)

enum class FactoryReason {
    NoError,
    InvalidFrame,
    InvalidTiming,
    InvalidAnimationMetadata,
    InvalidProviderDescriptor,
    LimitExceeded,
};
Q_ENUM_NS(FactoryReason)
}

using ImageSequenceFactoryOutcome = ImageSequenceFactoryEnums::FactoryOutcome;
using ImageSequenceFactoryReason = ImageSequenceFactoryEnums::FactoryReason;

class ImageSequenceFactoryResult : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageSequenceFactoryResult objects are returned by ImageSequenceFactory")
    QML_EXTENDED_NAMESPACE(ImageSequenceFactoryEnums)
    Q_PROPERTY(ImageSequence* sequence READ sequence CONSTANT)
    Q_PROPERTY(ImageSequenceFactoryOutcome outcome READ outcome CONSTANT)
    Q_PROPERTY(ImageSequenceFactoryReason reason READ reason CONSTANT)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)

public:
    explicit ImageSequenceFactoryResult(ImageSequence* sequence,
        ImageSequenceFactoryOutcome outcome, ImageSequenceFactoryReason reason,
        QString errorString = {}, QObject* parent = nullptr);

    ImageSequence* sequence() const;
    ImageSequenceFactoryOutcome outcome() const;
    ImageSequenceFactoryReason reason() const;
    QString errorString() const;

private:
    friend class ImageSequenceFactory;

    explicit ImageSequenceFactoryResult(std::shared_ptr<ImageSequence> sequence,
        ImageSequenceFactoryOutcome outcome, ImageSequenceFactoryReason reason,
        QString errorString = {}, QObject* parent = nullptr);

    QPointer<ImageSequence> m_sequence;
    std::shared_ptr<ImageSequence> m_sequenceOwner;
    ImageSequenceFactoryOutcome m_outcome = ImageSequenceFactoryOutcome::Rejected;
    ImageSequenceFactoryReason m_reason = ImageSequenceFactoryReason::InvalidFrame;
    QString m_errorString;
};

class ImageSequenceFactory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit ImageSequenceFactory(QObject* parent = nullptr);

    ImageSequenceFactoryResult* fromFrame(const QImage& image);
    ImageSequenceFactoryResult* fromTimedFrameList(
        const QVector<QImage>& images, const QVector<int>& durationsMilliseconds);
    Q_INVOKABLE ImageSequenceFactoryResult* fromFrame(ImageFrame* frame);
    Q_INVOKABLE ImageSequenceFactoryResult* fromTimedFrameList(TimedImageFrameList* list);
    Q_INVOKABLE ImageSequenceFactoryResult* fromProvider(ImageSequenceProviderAdapter* adapter);
};

class ImageSequenceLimits : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int maximumSourceLogicalWidth READ getMaximumSourceLogicalWidth CONSTANT)
    Q_PROPERTY(int maximumSourceLogicalHeight READ getMaximumSourceLogicalHeight CONSTANT)
    Q_PROPERTY(qint64 maximumSourceLogicalPixels READ getMaximumSourceLogicalPixels CONSTANT)
    Q_PROPERTY(int maximumPayloadRasterWidth READ getMaximumPayloadRasterWidth CONSTANT)
    Q_PROPERTY(int maximumPayloadRasterHeight READ getMaximumPayloadRasterHeight CONSTANT)
    Q_PROPERTY(qint64 maximumPayloadBytes READ getMaximumPayloadBytes CONSTANT)
    Q_PROPERTY(int maximumFrameCount READ getMaximumFrameCount CONSTANT)
    Q_PROPERTY(
        int maximumFrameDurationMilliseconds READ getMaximumFrameDurationMilliseconds CONSTANT)
    Q_PROPERTY(
        int maximumTotalDurationMilliseconds READ getMaximumTotalDurationMilliseconds CONSTANT)
    Q_PROPERTY(int maximumDiagnosticCharacters READ getMaximumDiagnosticCharacters CONSTANT)
    Q_PROPERTY(
        int maximumFormatIdentifierCharacters READ getMaximumFormatIdentifierCharacters CONSTANT)

public:
    explicit ImageSequenceLimits(QObject* parent = nullptr);

    int getMaximumSourceLogicalWidth() const;
    int getMaximumSourceLogicalHeight() const;
    qint64 getMaximumSourceLogicalPixels() const;
    int getMaximumPayloadRasterWidth() const;
    int getMaximumPayloadRasterHeight() const;
    qint64 getMaximumPayloadBytes() const;
    int getMaximumFrameCount() const;
    int getMaximumFrameDurationMilliseconds() const;
    int getMaximumTotalDurationMilliseconds() const;
    int getMaximumDiagnosticCharacters() const;
    int getMaximumFormatIdentifierCharacters() const;

    static int maximumSourceLogicalWidth();
    static int maximumSourceLogicalHeight();
    static qint64 maximumSourceLogicalPixels();
    static int maximumPayloadRasterWidth();
    static int maximumPayloadRasterHeight();
    static qint64 maximumPayloadBytes();
    static int maximumFrameCount();
    static int maximumFrameDurationMilliseconds();
    static int maximumTotalDurationMilliseconds();
    static int maximumDiagnosticCharacters();
    static int maximumFormatIdentifierCharacters();
};

class ImageViewportDisplayLimits : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(double maximumManualZoomPercent READ getMaximumManualZoomPercent CONSTANT)

public:
    explicit ImageViewportDisplayLimits(QObject* parent = nullptr);

    double getMaximumManualZoomPercent() const;

    static double maximumManualZoomPercent();
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

    int minimum() const { return m_minimum; }
    int maximum() const { return m_maximum; }

    friend bool operator==(ImageViewportRange lhs, ImageViewportRange rhs)
    {
        return lhs.m_minimum == rhs.m_minimum && lhs.m_maximum == rhs.m_maximum;
    }
    friend bool operator!=(ImageViewportRange lhs, ImageViewportRange rhs) { return !(lhs == rhs); }

private:
    int m_minimum = -1;
    int m_maximum = -1;
};

class RevisionToken
{
    Q_GADGET
    QML_VALUE_TYPE(revisionToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    RevisionToken() = default;

    bool isValid() const { return m_value != 0; }

    friend bool operator==(RevisionToken lhs, RevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }
    friend bool operator!=(RevisionToken lhs, RevisionToken rhs) { return !(lhs == rhs); }

private:
    explicit RevisionToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend class ImageViewportInternal::RevisionTokenPrivateAccess;
};

class ImageViewportRevisionToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRevisionToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportRevisionToken() = default;

    bool isValid() const { return m_value != 0; }

    friend bool operator==(ImageViewportRevisionToken lhs, ImageViewportRevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }
    friend bool operator!=(ImageViewportRevisionToken lhs, ImageViewportRevisionToken rhs)
    {
        return !(lhs == rhs);
    }

private:
    explicit ImageViewportRevisionToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend ImageViewportPrivate;
    friend class ImageViewportRevisionsSnapshot;
    friend class ImageViewportDisplaySnapshot;
    friend class ImageViewportInternal::RevisionTokenPrivateAccess;
};

class ImageViewportPresentationTargetGenerationToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationTargetGenerationToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportPresentationTargetGenerationToken() = default;

    bool isValid() const { return m_value != 0; }

    friend bool operator==(ImageViewportPresentationTargetGenerationToken lhs,
        ImageViewportPresentationTargetGenerationToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }
    friend bool operator!=(ImageViewportPresentationTargetGenerationToken lhs,
        ImageViewportPresentationTargetGenerationToken rhs)
    {
        return !(lhs == rhs);
    }

private:
    explicit ImageViewportPresentationTargetGenerationToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend ImageViewportPrivate;
    friend class ImageViewportRequestSnapshot;
    friend class ImageViewportDisplaySnapshot;
    friend class ImageViewportRoleRequestSnapshot;
    friend class ImageViewportInternal::RevisionTokenPrivateAccess;
};

class ImageViewportDemandRevisionToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportDemandRevisionToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportDemandRevisionToken() = default;

    bool isValid() const { return m_value != 0; }

    friend bool operator==(
        ImageViewportDemandRevisionToken lhs, ImageViewportDemandRevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }
    friend bool operator!=(
        ImageViewportDemandRevisionToken lhs, ImageViewportDemandRevisionToken rhs)
    {
        return !(lhs == rhs);
    }

private:
    explicit ImageViewportDemandRevisionToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend ImageViewportPrivate;
    friend class ImageViewportRoleRequestSnapshot;
    friend class ImageViewportRoleDisplaySnapshot;
    friend class ImageViewportInternal::RevisionTokenPrivateAccess;
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

    bool primary() const { return m_primary; }
    bool secondary() const { return m_secondary; }

    friend bool operator==(ImageViewportRoleSet lhs, ImageViewportRoleSet rhs)
    {
        return lhs.m_primary == rhs.m_primary && lhs.m_secondary == rhs.m_secondary;
    }
    friend bool operator!=(ImageViewportRoleSet lhs, ImageViewportRoleSet rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_primary = false;
    bool m_secondary = false;
};

class ImageViewportPresentationTarget
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationTarget)
    Q_PROPERTY(ImageSequence* primary READ primary WRITE setPrimary)
    Q_PROPERTY(ImageSequence* secondary READ secondary WRITE setSecondary)
    Q_PROPERTY(bool clear READ isClear CONSTANT)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportPresentationTarget() = default;
    explicit ImageViewportPresentationTarget(ImageSequence* primary)
        : m_primary(primary)
    {
    }
    ImageViewportPresentationTarget(ImageSequence* primary, ImageSequence* secondary)
        : m_primary(primary)
        , m_secondary(secondary)
    {
    }

    static ImageViewportPresentationTarget clear() { return {}; }

    ImageSequence* primary() const { return m_primary; }
    void setPrimary(ImageSequence* primary) { m_primary = primary; }
    ImageSequence* secondary() const { return m_secondary; }
    void setSecondary(ImageSequence* secondary) { m_secondary = secondary; }
    bool isClear() const { return !m_primary && !m_secondary; }
    bool isValid() const { return m_primary || !m_secondary; }

    friend bool operator==(
        const ImageViewportPresentationTarget& lhs, const ImageViewportPresentationTarget& rhs)
    {
        return lhs.m_primary == rhs.m_primary && lhs.m_secondary == rhs.m_secondary;
    }
    friend bool operator!=(
        const ImageViewportPresentationTarget& lhs, const ImageViewportPresentationTarget& rhs)
    {
        return !(lhs == rhs);
    }

private:
    QPointer<ImageSequence> m_primary;
    QPointer<ImageSequence> m_secondary;
};

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_ELEMENT
    QML_EXTENDED_NAMESPACE(ImageViewportEnums)
    Q_PROPERTY(ImageViewportStateSnapshot state READ state NOTIFY stateChanged)

public:
    enum class SpreadDirection {
        LeftToRight,
        RightToLeft,
    };
    Q_ENUM(SpreadDirection)

    enum class FitMode {
        Contain,
        FitWidth,
        FitHeight,
        Manual,
    };
    Q_ENUM(FitMode)

    enum class ScanDirection {
        Start,
        Previous,
        Next,
        End,
    };
    Q_ENUM(ScanDirection)

    enum class RequestStatus {
        NoRequest,
        Loading,
        Ready,
        Unsupported,
        Error,
    };
    Q_ENUM(RequestStatus)

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
    Q_ENUM(RequestReason)

    enum class CommandReason {
        NoCommand,
        IgnoredNoRequest,
        InvalidRequest,
        UnsupportedRequest,
    };
    Q_ENUM(CommandReason)

    enum class DisplayStatus {
        Empty,
        Ready,
        Retained,
    };
    Q_ENUM(DisplayStatus)

    enum class DisplayPhase {
        NoPresentation,
        PreviousActive,
        TransitioningPlaceholder,
        CommittedActive,
    };
    Q_ENUM(DisplayPhase)

    enum class PlaybackPhase {
        Stopped,
        Playing,
        Waiting,
        Paused,
    };
    Q_ENUM(PlaybackPhase)

    enum class CommandOutcome {
        Accepted,
        Invalid,
        Unsupported,
        IgnoredNoRequest,
    };
    Q_ENUM(CommandOutcome)

    enum class BackgroundMode {
        Transparent,
        SolidColor,
        Checkerboard,
    };
    Q_ENUM(BackgroundMode)

    enum class CoordinateSpace {
        Item,
        DisplayedSpread,
        DisplayedPage,
    };
    Q_ENUM(CoordinateSpace)

    explicit ImageViewport(QQuickItem* parent = nullptr);
    ~ImageViewport() override;

    ImageViewportStateSnapshot state() const;

    Q_INVOKABLE ImageViewportCommandResult clear();
    Q_INVOKABLE ImageViewportCommandResult play(ImageViewportPageRole role);
    Q_INVOKABLE ImageViewportCommandResult pause(ImageViewportPageRole role);
    Q_INVOKABLE ImageViewportCommandResult stop(ImageViewportPageRole role);
    Q_INVOKABLE ImageViewportCommandResult seek(ImageViewportPageRole role, int frame);
    Q_INVOKABLE ImageViewportCommandResult seekToPosition(
        ImageViewportPageRole role, int milliseconds);
    Q_INVOKABLE ImageViewportCommandResult setPresentationTarget(
        ImageViewportPresentationTarget presentationTarget,
        PresentationTargetTransitionPolicy policy);
    Q_INVOKABLE ImageViewportCommandResult resetView();
    Q_INVOKABLE ImageViewportCommandResult setPresentation(
        ImageViewportPresentationCommand command);
    Q_INVOKABLE ImageViewportCoordinateResult mapPoint(ImageViewportCoordinateInput input) const;
    Q_INVOKABLE bool containsPoint(ImageViewportCoordinateInput input) const;

signals:
    void stateChanged(); // clazy:exclude=overloaded-signal

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;

private:
    friend ImageViewportPrivate;

    std::unique_ptr<ImageViewportPrivate> d;
};

class ImageViewportPresentationCommand
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationCommand)
    Q_PROPERTY(bool resetView READ resetView WRITE setResetView)
    Q_PROPERTY(bool fitModeSet READ hasFitMode CONSTANT)
    Q_PROPERTY(ImageViewport::FitMode fitMode READ fitMode WRITE setFitMode)
    Q_PROPERTY(bool manualZoomPercentSet READ hasManualZoomPercent CONSTANT)
    Q_PROPERTY(double manualZoomPercent READ manualZoomPercent WRITE setManualZoomPercent)
    Q_PROPERTY(bool zoomStepDeltaSet READ hasZoomStepDelta CONSTANT)
    Q_PROPERTY(int zoomStepDelta READ zoomStepDelta WRITE setZoomStepDelta)
    Q_PROPERTY(bool contentPositionSet READ hasContentPosition CONSTANT)
    Q_PROPERTY(QPointF contentPosition READ contentPosition WRITE setContentPosition)
    Q_PROPERTY(bool panDeltaSet READ hasPanDelta CONSTANT)
    Q_PROPERTY(QPointF panDelta READ panDelta WRITE setPanDelta)
    Q_PROPERTY(bool scanDirectionSet READ hasScanDirection CONSTANT)
    Q_PROPERTY(ImageViewport::ScanDirection scanDirection READ scanDirection WRITE setScanDirection)
    Q_PROPERTY(bool rotationDegreesSet READ hasRotationDegrees CONSTANT)
    Q_PROPERTY(int rotationDegrees READ rotationDegrees WRITE setRotationDegrees)
    Q_PROPERTY(bool mirrorHorizontallySet READ hasMirrorHorizontally CONSTANT)
    Q_PROPERTY(bool mirrorHorizontally READ mirrorHorizontally WRITE setMirrorHorizontally)
    Q_PROPERTY(bool mirrorVerticallySet READ hasMirrorVertically CONSTANT)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically WRITE setMirrorVertically)
    Q_PROPERTY(bool spreadDirectionSet READ hasSpreadDirection CONSTANT)
    Q_PROPERTY(ImageViewport::SpreadDirection spreadDirection READ spreadDirection WRITE
            setSpreadDirection)
    Q_PROPERTY(bool pageGapSet READ hasPageGap CONSTANT)
    Q_PROPERTY(double pageGap READ pageGap WRITE setPageGap)
    Q_PROPERTY(bool backgroundModeSet READ hasBackgroundMode CONSTANT)
    Q_PROPERTY(
        ImageViewport::BackgroundMode backgroundMode READ backgroundMode WRITE setBackgroundMode)
    Q_PROPERTY(bool backgroundColorSet READ hasBackgroundColor CONSTANT)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(bool smoothingSet READ hasSmoothing CONSTANT)
    Q_PROPERTY(bool smoothing READ smoothing WRITE setSmoothing)
    Q_PROPERTY(bool mipmapSet READ hasMipmap CONSTANT)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap)
    Q_PROPERTY(bool loopingSet READ hasLooping CONSTANT)
    Q_PROPERTY(bool looping READ looping WRITE setLooping)
    Q_PROPERTY(bool qualityPreferenceSet READ hasQualityPreference CONSTANT)
    Q_PROPERTY(ImageViewportQualityPreference qualityPreference READ qualityPreference WRITE
            setQualityPreference)
    Q_PROPERTY(bool exactnessPreferenceSet READ hasExactnessPreference CONSTANT)
    Q_PROPERTY(ImageViewportExactnessPreference exactnessPreference READ exactnessPreference WRITE
            setExactnessPreference)

public:
    ImageViewportPresentationCommand() = default;

    static ImageViewportPresentationCommand resetViewCommand()
    {
        ImageViewportPresentationCommand command;
        command.setResetView(true);
        return command;
    }

    bool resetView() const { return m_resetView; }
    void setResetView(bool reset) { m_resetView = reset; }
    bool hasFitMode() const { return m_hasFitMode; }
    ImageViewport::FitMode fitMode() const { return m_fitMode; }
    void setFitMode(ImageViewport::FitMode mode)
    {
        m_fitMode = mode;
        m_hasFitMode = true;
    }
    bool hasManualZoomPercent() const { return m_hasManualZoomPercent; }
    double manualZoomPercent() const { return m_manualZoomPercent; }
    void setManualZoomPercent(double percent)
    {
        m_manualZoomPercent = percent;
        m_hasManualZoomPercent = true;
    }
    bool hasZoomStepDelta() const { return m_hasZoomStepDelta; }
    int zoomStepDelta() const { return m_zoomStepDelta; }
    void setZoomStepDelta(int delta)
    {
        m_zoomStepDelta = delta;
        m_hasZoomStepDelta = true;
    }
    bool hasContentPosition() const { return m_hasContentPosition; }
    QPointF contentPosition() const { return m_contentPosition; }
    void setContentPosition(QPointF position)
    {
        m_contentPosition = position;
        m_hasContentPosition = true;
    }
    bool hasPanDelta() const { return m_hasPanDelta; }
    QPointF panDelta() const { return m_panDelta; }
    void setPanDelta(QPointF delta)
    {
        m_panDelta = delta;
        m_hasPanDelta = true;
    }
    bool hasScanDirection() const { return m_hasScanDirection; }
    ImageViewport::ScanDirection scanDirection() const { return m_scanDirection; }
    void setScanDirection(ImageViewport::ScanDirection direction)
    {
        m_scanDirection = direction;
        m_hasScanDirection = true;
    }
    bool hasRotationDegrees() const { return m_hasRotationDegrees; }
    int rotationDegrees() const { return m_rotationDegrees; }
    void setRotationDegrees(int degrees)
    {
        m_rotationDegrees = degrees;
        m_hasRotationDegrees = true;
    }
    bool hasMirrorHorizontally() const { return m_hasMirrorHorizontally; }
    bool mirrorHorizontally() const { return m_mirrorHorizontally; }
    void setMirrorHorizontally(bool mirror)
    {
        m_mirrorHorizontally = mirror;
        m_hasMirrorHorizontally = true;
    }
    bool hasMirrorVertically() const { return m_hasMirrorVertically; }
    bool mirrorVertically() const { return m_mirrorVertically; }
    void setMirrorVertically(bool mirror)
    {
        m_mirrorVertically = mirror;
        m_hasMirrorVertically = true;
    }
    bool hasSpreadDirection() const { return m_hasSpreadDirection; }
    ImageViewport::SpreadDirection spreadDirection() const { return m_spreadDirection; }
    void setSpreadDirection(ImageViewport::SpreadDirection direction)
    {
        m_spreadDirection = direction;
        m_hasSpreadDirection = true;
    }
    bool hasPageGap() const { return m_hasPageGap; }
    double pageGap() const { return m_pageGap; }
    void setPageGap(double gap)
    {
        m_pageGap = gap;
        m_hasPageGap = true;
    }
    bool hasBackgroundMode() const { return m_hasBackgroundMode; }
    ImageViewport::BackgroundMode backgroundMode() const { return m_backgroundMode; }
    void setBackgroundMode(ImageViewport::BackgroundMode mode)
    {
        m_backgroundMode = mode;
        m_hasBackgroundMode = true;
    }
    bool hasBackgroundColor() const { return m_hasBackgroundColor; }
    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor& color)
    {
        m_backgroundColor = color;
        m_hasBackgroundColor = true;
    }
    bool hasSmoothing() const { return m_hasSmoothing; }
    bool smoothing() const { return m_smoothing; }
    void setSmoothing(bool smoothing)
    {
        m_smoothing = smoothing;
        m_hasSmoothing = true;
    }
    bool hasMipmap() const { return m_hasMipmap; }
    bool mipmap() const { return m_mipmap; }
    void setMipmap(bool mipmap)
    {
        m_mipmap = mipmap;
        m_hasMipmap = true;
    }
    bool hasLooping() const { return m_hasLooping; }
    bool looping() const { return m_looping; }
    void setLooping(bool looping)
    {
        m_looping = looping;
        m_hasLooping = true;
    }
    bool hasQualityPreference() const { return m_hasQualityPreference; }
    ImageViewportQualityPreference qualityPreference() const { return m_qualityPreference; }
    void setQualityPreference(ImageViewportQualityPreference preference)
    {
        m_qualityPreference = preference;
        m_hasQualityPreference = true;
    }
    bool hasExactnessPreference() const { return m_hasExactnessPreference; }
    ImageViewportExactnessPreference exactnessPreference() const { return m_exactnessPreference; }
    void setExactnessPreference(ImageViewportExactnessPreference preference)
    {
        m_exactnessPreference = preference;
        m_hasExactnessPreference = true;
    }

private:
    bool m_resetView = false;
    bool m_hasFitMode = false;
    ImageViewport::FitMode m_fitMode = ImageViewport::FitMode::Contain;
    bool m_hasManualZoomPercent = false;
    double m_manualZoomPercent = 100.0;
    bool m_hasZoomStepDelta = false;
    int m_zoomStepDelta = 0;
    bool m_hasContentPosition = false;
    QPointF m_contentPosition;
    bool m_hasPanDelta = false;
    QPointF m_panDelta;
    bool m_hasScanDirection = false;
    ImageViewport::ScanDirection m_scanDirection = ImageViewport::ScanDirection::Start;
    bool m_hasRotationDegrees = false;
    int m_rotationDegrees = 0;
    bool m_hasMirrorHorizontally = false;
    bool m_mirrorHorizontally = false;
    bool m_hasMirrorVertically = false;
    bool m_mirrorVertically = false;
    bool m_hasSpreadDirection = false;
    ImageViewport::SpreadDirection m_spreadDirection = ImageViewport::SpreadDirection::LeftToRight;
    bool m_hasPageGap = false;
    double m_pageGap = 0.0;
    bool m_hasBackgroundMode = false;
    ImageViewport::BackgroundMode m_backgroundMode = ImageViewport::BackgroundMode::Transparent;
    bool m_hasBackgroundColor = false;
    QColor m_backgroundColor = Qt::transparent;
    bool m_hasSmoothing = false;
    bool m_smoothing = true;
    bool m_hasMipmap = false;
    bool m_mipmap = false;
    bool m_hasLooping = false;
    bool m_looping = false;
    bool m_hasQualityPreference = false;
    ImageViewportQualityPreference m_qualityPreference = ImageViewportQualityPreference::Default;
    bool m_hasExactnessPreference = false;
    ImageViewportExactnessPreference m_exactnessPreference
        = ImageViewportExactnessPreference::Default;
};

enum class ImageSequenceProviderRequestKind {
    Metadata,
    Frame,
    Position,
    Playback,
    Cancel,
    Close,
};

enum class ImageSequenceProviderEventKind {
    MetadataReady,
    FrameReady,
    Waiting,
    Progress,
    EndOfSequence,
    Unsupported,
    Cancelled,
    Failed,
};

enum class ImageSequenceProviderUnsupportedCause {
    UnsupportedRequest,
    PayloadRejection,
};

class ImageSequenceProviderFrameEnvelope
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceProviderFrameEnvelope)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(
        ImageViewportDemandRevisionToken demandRevision READ demandRevision WRITE setDemandRevision)
    Q_PROPERTY(int frame READ frame WRITE setFrame)
    Q_PROPERTY(int frameStartPosition READ frameStartPosition WRITE setFrameStartPosition)
    Q_PROPERTY(int frameDuration READ frameDuration WRITE setFrameDuration)
public:
    ImageSequenceProviderFrameEnvelope() = default;

    bool isValid() const;
    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }
    void setDemandRevision(ImageViewportDemandRevisionToken revision)
    {
        m_demandRevision = revision;
    }
    int frame() const { return m_frame; }
    void setFrame(int frame) { m_frame = frame; }
    int frameStartPosition() const { return m_frameStartPosition; }
    void setFrameStartPosition(int position) { m_frameStartPosition = position; }
    int frameDuration() const { return m_frameDuration; }
    void setFrameDuration(int duration) { m_frameDuration = duration; }
    friend bool operator==(const ImageSequenceProviderFrameEnvelope& lhs,
        const ImageSequenceProviderFrameEnvelope& rhs)
    {
        return lhs.m_demandRevision == rhs.m_demandRevision && lhs.m_frame == rhs.m_frame
            && lhs.m_frameStartPosition == rhs.m_frameStartPosition
            && lhs.m_frameDuration == rhs.m_frameDuration;
    }
    friend bool operator!=(const ImageSequenceProviderFrameEnvelope& lhs,
        const ImageSequenceProviderFrameEnvelope& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportDemandRevisionToken m_demandRevision;
    int m_frame = 0;
    int m_frameStartPosition = -1;
    int m_frameDuration = -1;
};

class ImageSequenceProviderDisplayDemand
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceProviderDisplayDemand)
    Q_PROPERTY(
        ImageViewportDemandRevisionToken demandRevision READ demandRevision WRITE setDemandRevision)
    Q_PROPERTY(
        ImageViewportRevisionToken requestRevision READ requestRevision WRITE setRequestRevision)
    Q_PROPERTY(ImageViewportRevisionToken presentationRevision READ presentationRevision WRITE
            setPresentationRevision)
    Q_PROPERTY(ImageViewportPageRole role READ role WRITE setRole)
    Q_PROPERTY(int resolvedFrame READ resolvedFrame WRITE setResolvedFrame)
    Q_PROPERTY(int requestedPosition READ requestedPosition WRITE setRequestedPosition)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize WRITE setSourceLogicalSize)
    Q_PROPERTY(QRectF visibleSourceRect READ visibleSourceRect WRITE setVisibleSourceRect)
    Q_PROPERTY(QSizeF targetDisplaySizePixels READ targetDisplaySizePixels WRITE
            setTargetDisplaySizePixels)
    Q_PROPERTY(double effectiveDevicePixelRatio READ effectiveDevicePixelRatio WRITE
            setEffectiveDevicePixelRatio)
    Q_PROPERTY(int rotationDegrees READ rotationDegrees WRITE setRotationDegrees)
    Q_PROPERTY(bool mirrorHorizontally READ mirrorHorizontally WRITE setMirrorHorizontally)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically WRITE setMirrorVertically)
    Q_PROPERTY(ImageViewportQualityPreference qualityPreference READ qualityPreference WRITE
            setQualityPreference)
    Q_PROPERTY(ImageViewportExactnessPreference exactnessPreference READ exactnessPreference WRITE
            setExactnessPreference)
    Q_PROPERTY(qint64 maximumTextureSize READ maximumTextureSize WRITE setMaximumTextureSize)
    Q_PROPERTY(qint64 maximumPayloadBytes READ maximumPayloadBytes WRITE setMaximumPayloadBytes)
    Q_PROPERTY(qint64 displayByteBudget READ displayByteBudget WRITE setDisplayByteBudget)
    Q_PROPERTY(ImageViewportPresentationTargetGenerationToken allocationGeneration READ
            allocationGeneration WRITE setAllocationGeneration)
    Q_PROPERTY(ImageViewportPayloadQuality currentPayloadQuality READ currentPayloadQuality WRITE
            setCurrentPayloadQuality)
    Q_PROPERTY(ImageViewportPayloadExactness currentPayloadExactness READ currentPayloadExactness
            WRITE setCurrentPayloadExactness)
    Q_PROPERTY(QSizeF currentPayloadRasterSize READ currentPayloadRasterSize WRITE
            setCurrentPayloadRasterSize)
    Q_PROPERTY(QSizeF currentSourceToPayloadScale READ currentSourceToPayloadScale WRITE
            setCurrentSourceToPayloadScale)

public:
    ImageSequenceProviderDisplayDemand() = default;

    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }
    void setDemandRevision(ImageViewportDemandRevisionToken revision)
    {
        m_demandRevision = revision;
    }
    ImageViewportRevisionToken requestRevision() const { return m_requestRevision; }
    void setRequestRevision(ImageViewportRevisionToken revision) { m_requestRevision = revision; }
    ImageViewportRevisionToken presentationRevision() const { return m_presentationRevision; }
    void setPresentationRevision(ImageViewportRevisionToken revision)
    {
        m_presentationRevision = revision;
    }
    ImageViewportPageRole role() const { return m_role; }
    void setRole(ImageViewportPageRole role) { m_role = role; }
    int resolvedFrame() const { return m_resolvedFrame; }
    void setResolvedFrame(int frame) { m_resolvedFrame = frame; }
    int requestedPosition() const { return m_requestedPosition; }
    void setRequestedPosition(int position) { m_requestedPosition = position; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    void setSourceLogicalSize(QSizeF size) { m_sourceLogicalSize = size; }
    QRectF visibleSourceRect() const { return m_visibleSourceRect; }
    void setVisibleSourceRect(QRectF rect) { m_visibleSourceRect = rect; }
    QSizeF targetDisplaySizePixels() const { return m_targetDisplaySizePixels; }
    void setTargetDisplaySizePixels(QSizeF size) { m_targetDisplaySizePixels = size; }
    double effectiveDevicePixelRatio() const { return m_effectiveDevicePixelRatio; }
    void setEffectiveDevicePixelRatio(double ratio) { m_effectiveDevicePixelRatio = ratio; }
    int rotationDegrees() const { return m_rotationDegrees; }
    void setRotationDegrees(int degrees) { m_rotationDegrees = degrees; }
    bool mirrorHorizontally() const { return m_mirrorHorizontally; }
    void setMirrorHorizontally(bool mirror) { m_mirrorHorizontally = mirror; }
    bool mirrorVertically() const { return m_mirrorVertically; }
    void setMirrorVertically(bool mirror) { m_mirrorVertically = mirror; }
    ImageViewportQualityPreference qualityPreference() const { return m_qualityPreference; }
    void setQualityPreference(ImageViewportQualityPreference preference)
    {
        m_qualityPreference = preference;
    }
    ImageViewportExactnessPreference exactnessPreference() const { return m_exactnessPreference; }
    void setExactnessPreference(ImageViewportExactnessPreference preference)
    {
        m_exactnessPreference = preference;
    }
    qint64 maximumTextureSize() const { return m_maximumTextureSize; }
    void setMaximumTextureSize(qint64 size) { m_maximumTextureSize = size; }
    qint64 maximumPayloadBytes() const { return m_maximumPayloadBytes; }
    void setMaximumPayloadBytes(qint64 bytes) { m_maximumPayloadBytes = bytes; }
    qint64 displayByteBudget() const { return m_displayByteBudget; }
    void setDisplayByteBudget(qint64 bytes) { m_displayByteBudget = bytes; }
    ImageViewportPresentationTargetGenerationToken allocationGeneration() const
    {
        return m_allocationGeneration;
    }
    void setAllocationGeneration(ImageViewportPresentationTargetGenerationToken generation)
    {
        m_allocationGeneration = generation;
    }
    ImageViewportPayloadQuality currentPayloadQuality() const { return m_currentPayloadQuality; }
    void setCurrentPayloadQuality(ImageViewportPayloadQuality quality)
    {
        m_currentPayloadQuality = quality;
    }
    ImageViewportPayloadExactness currentPayloadExactness() const
    {
        return m_currentPayloadExactness;
    }
    void setCurrentPayloadExactness(ImageViewportPayloadExactness exactness)
    {
        m_currentPayloadExactness = exactness;
    }
    QSizeF currentPayloadRasterSize() const { return m_currentPayloadRasterSize; }
    void setCurrentPayloadRasterSize(QSizeF size) { m_currentPayloadRasterSize = size; }
    QSizeF currentSourceToPayloadScale() const { return m_currentSourceToPayloadScale; }
    void setCurrentSourceToPayloadScale(QSizeF scale) { m_currentSourceToPayloadScale = scale; }

private:
    ImageViewportDemandRevisionToken m_demandRevision;
    ImageViewportRevisionToken m_requestRevision;
    ImageViewportRevisionToken m_presentationRevision;
    ImageViewportPageRole m_role = ImageViewportPageRole::Primary;
    int m_resolvedFrame = -1;
    int m_requestedPosition = -1;
    QSizeF m_sourceLogicalSize;
    QRectF m_visibleSourceRect;
    QSizeF m_targetDisplaySizePixels;
    double m_effectiveDevicePixelRatio = 0.0;
    int m_rotationDegrees = 0;
    bool m_mirrorHorizontally = false;
    bool m_mirrorVertically = false;
    ImageViewportQualityPreference m_qualityPreference = ImageViewportQualityPreference::Default;
    ImageViewportExactnessPreference m_exactnessPreference
        = ImageViewportExactnessPreference::Default;
    qint64 m_maximumTextureSize = -1;
    qint64 m_maximumPayloadBytes = -1;
    qint64 m_displayByteBudget = -1;
    ImageViewportPresentationTargetGenerationToken m_allocationGeneration;
    ImageViewportPayloadQuality m_currentPayloadQuality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness m_currentPayloadExactness
        = ImageViewportPayloadExactness::Unknown;
    QSizeF m_currentPayloadRasterSize;
    QSizeF m_currentSourceToPayloadScale;
};

class ImageSequenceProviderRequest
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceProviderRequest)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(ImageSequenceProviderRequestKind kind READ kind CONSTANT)
    Q_PROPERTY(ImageSequenceProviderRequestToken token READ token CONSTANT)
    Q_PROPERTY(ImageViewportPageRole role READ role CONSTANT)
    Q_PROPERTY(int frame READ frame CONSTANT)
    Q_PROPERTY(int requestedPosition READ requestedPosition CONSTANT)
    Q_PROPERTY(int resolvedFrame READ resolvedFrame CONSTANT)
    Q_PROPERTY(ImageSequenceProviderDisplayDemand demand READ demand CONSTANT)

public:
    ImageSequenceProviderRequest() = default;
    static ImageSequenceProviderRequest metadata(ImageSequenceProviderRequestToken token);
    static ImageSequenceProviderRequest frame( // clazy:exclude=qproperty-type-mismatch
        ImageSequenceProviderRequestToken token, ImageViewportPageRole role, int frame,
        ImageSequenceProviderDisplayDemand demand);
    static ImageSequenceProviderRequest position(ImageSequenceProviderRequestToken token,
        ImageViewportPageRole role, int requestedPosition, int resolvedFrame,
        ImageSequenceProviderDisplayDemand demand);
    static ImageSequenceProviderRequest playback(ImageSequenceProviderRequestToken token,
        ImageViewportPageRole role, int frame, int position,
        ImageSequenceProviderDisplayDemand demand);
    static ImageSequenceProviderRequest cancel(QVector<ImageSequenceProviderRequestToken> tokens);
    static ImageSequenceProviderRequest close();

    bool isValid() const;
    ImageSequenceProviderRequestKind kind() const { return m_kind; }
    ImageSequenceProviderRequestToken token() const { return m_token; }
    ImageViewportPageRole role() const { return m_role; }
    int frame() const { return m_frame; }
    int requestedPosition() const { return m_requestedPosition; }
    int resolvedFrame() const { return m_resolvedFrame; }
    ImageSequenceProviderDisplayDemand demand() const { return m_demand; }
    QVector<ImageSequenceProviderRequestToken> tokens() const { return m_tokens; }

private:
    ImageSequenceProviderRequestKind m_kind = ImageSequenceProviderRequestKind::Metadata;
    ImageSequenceProviderRequestToken m_token;
    ImageViewportPageRole m_role = ImageViewportPageRole::Primary;
    int m_frame = -1;
    int m_requestedPosition = -1;
    int m_resolvedFrame = -1;
    ImageSequenceProviderDisplayDemand m_demand;
    QVector<ImageSequenceProviderRequestToken> m_tokens;
};

class ImageSequenceProviderEvent
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceProviderEvent)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(ImageSequenceProviderEventKind kind READ kind CONSTANT)
    Q_PROPERTY(ImageSequenceProviderRequestToken token READ token CONSTANT)
    Q_PROPERTY(QString diagnostic READ diagnostic CONSTANT)
    Q_PROPERTY(ImageSequenceProviderMetadata metadata READ metadata CONSTANT)
    Q_PROPERTY(ImageSequenceProviderFrameHandle* frameHandle READ frameHandle CONSTANT)
    Q_PROPERTY(ImageSequenceProviderFrameEnvelope frameEnvelope READ frameEnvelope CONSTANT)
    Q_PROPERTY(double progress READ progress CONSTANT)
    Q_PROPERTY(
        ImageSequenceProviderUnsupportedCause unsupportedCause READ unsupportedCause CONSTANT)

public:
    ImageSequenceProviderEvent() = default;
    static ImageSequenceProviderEvent metadataReady(
        ImageSequenceProviderRequestToken token, ImageSequenceProviderMetadata metadata);
    static ImageSequenceProviderEvent frameReady(ImageSequenceProviderRequestToken token,
        ImageSequenceProviderFrameHandle* frameHandle,
        ImageSequenceProviderFrameEnvelope frameEnvelope);
    static ImageSequenceProviderEvent waiting(ImageSequenceProviderRequestToken token);
    static ImageSequenceProviderEvent progress( // clazy:exclude=qproperty-type-mismatch
        ImageSequenceProviderRequestToken token, double progress);
    static ImageSequenceProviderEvent endOfSequence(ImageSequenceProviderRequestToken token);
    static ImageSequenceProviderEvent unsupported(ImageSequenceProviderRequestToken token,
        ImageSequenceProviderUnsupportedCause cause, QString diagnostic = {});
    static ImageSequenceProviderEvent cancelled(
        ImageSequenceProviderRequestToken token, QString diagnostic = {});
    static ImageSequenceProviderEvent failed(
        ImageSequenceProviderRequestToken token, QString diagnostic = {});

    bool isValid() const;
    ImageSequenceProviderEventKind kind() const { return m_kind; }
    ImageSequenceProviderRequestToken token() const { return m_token; }
    QString diagnostic() const { return m_diagnostic; }
    ImageSequenceProviderMetadata metadata() const { return m_metadata; }
    ImageSequenceProviderFrameHandle* frameHandle() const { return m_frameHandle; }
    ImageSequenceProviderFrameEnvelope frameEnvelope() const { return m_frameEnvelope; }
    double progress() const { return m_progress; }
    ImageSequenceProviderUnsupportedCause unsupportedCause() const { return m_unsupportedCause; }

private:
    ImageSequenceProviderEventKind m_kind = ImageSequenceProviderEventKind::Failed;
    ImageSequenceProviderRequestToken m_token;
    QString m_diagnostic;
    ImageSequenceProviderMetadata m_metadata;
    QPointer<ImageSequenceProviderFrameHandle> m_frameHandle;
    ImageSequenceProviderFrameEnvelope m_frameEnvelope;
    double m_progress = 0.0;
    ImageSequenceProviderUnsupportedCause m_unsupportedCause
        = ImageSequenceProviderUnsupportedCause::PayloadRejection;
};

class ImageSequenceProviderDescriptor
{
    Q_GADGET

public:
    using SessionFactory = std::function<ImageSequenceProviderSessionFactoryResult()>;

    ImageSequenceProviderDescriptor() = default;
    ImageSequenceProviderDescriptor(ImageSequenceProviderMetadata constructionMetadata,
        ImageSequenceProviderThreadingContract threadingContract, SessionFactory sessionFactory);

    bool isValid() const;
    ImageSequenceProviderMetadata constructionMetadata() const { return m_constructionMetadata; }
    ImageSequenceProviderThreadingContract threadingContract() const { return m_threadingContract; }
    SessionFactory sessionFactory() const { return m_sessionFactory; }

private:
    ImageSequenceProviderMetadata m_constructionMetadata;
    ImageSequenceProviderThreadingContract m_threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
    SessionFactory m_sessionFactory;
};

using ImageSequenceProviderSessionFactory = ImageSequenceProviderDescriptor::SessionFactory;

class PresentationTargetTransitionPolicy
{
    Q_GADGET
    QML_VALUE_TYPE(presentationTargetTransitionPolicy)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(
        DisplayTransition displayTransition READ displayTransition WRITE setDisplayTransition)
    Q_PROPERTY(ZoomTransition zoomTransition READ zoomTransition WRITE setZoomTransition)
    Q_PROPERTY(ContentPositionTransition contentPositionTransition READ contentPositionTransition
            WRITE setContentPositionTransition)
    Q_PROPERTY(
        RotationTransition rotationTransition READ rotationTransition WRITE setRotationTransition)
    Q_PROPERTY(MirrorTransition mirrorTransition READ mirrorTransition WRITE setMirrorTransition)
    Q_PROPERTY(
        FitModeTransition fitModeTransition READ fitModeTransition WRITE setFitModeTransition)
    Q_PROPERTY(ImageViewport::FitMode fitMode READ fitMode WRITE setFitMode)
    Q_PROPERTY(SpreadDirectionTransition spreadDirectionTransition READ spreadDirectionTransition
            WRITE setSpreadDirectionTransition)
    Q_PROPERTY(ImageViewport::SpreadDirection spreadDirection READ spreadDirection WRITE
            setSpreadDirection)
    Q_PROPERTY(
        PageGapTransition pageGapTransition READ pageGapTransition WRITE setPageGapTransition)
    Q_PROPERTY(double pageGap READ pageGap WRITE setPageGap)
    Q_PROPERTY(
        ReplacementIntent replacementIntent READ replacementIntent WRITE setReplacementIntent)

public:
    enum class DisplayTransition {
        RetainPrevious,
        ClearBeforeLoad,
    };
    Q_ENUM(DisplayTransition)

    enum class ZoomTransition {
        Preserve,
        ResetToContain,
        PreserveManualPercent,
    };
    Q_ENUM(ZoomTransition)

    enum class ContentPositionTransition {
        Preserve,
        Clamp,
        ScanStart,
        ScanEnd,
    };
    Q_ENUM(ContentPositionTransition)

    enum class RotationTransition {
        Preserve,
        Reset,
    };
    Q_ENUM(RotationTransition)

    enum class MirrorTransition {
        Preserve,
        Reset,
    };
    Q_ENUM(MirrorTransition)

    enum class FitModeTransition {
        Preserve,
        SetExplicit,
    };
    Q_ENUM(FitModeTransition)

    enum class SpreadDirectionTransition {
        Preserve,
        SetExplicit,
    };
    Q_ENUM(SpreadDirectionTransition)

    enum class PageGapTransition {
        Preserve,
        SetExplicit,
    };
    Q_ENUM(PageGapTransition)

    enum class ReplacementIntent {
        NewTarget,
        SameTargetRefinement,
    };
    Q_ENUM(ReplacementIntent)

    PresentationTargetTransitionPolicy() = default;

    DisplayTransition displayTransition() const { return m_displayTransition; }
    void setDisplayTransition(DisplayTransition transition) { m_displayTransition = transition; }
    ZoomTransition zoomTransition() const { return m_zoomTransition; }
    void setZoomTransition(ZoomTransition transition) { m_zoomTransition = transition; }
    ContentPositionTransition contentPositionTransition() const
    {
        return m_contentPositionTransition;
    }
    void setContentPositionTransition(ContentPositionTransition transition)
    {
        m_contentPositionTransition = transition;
    }
    RotationTransition rotationTransition() const { return m_rotationTransition; }
    void setRotationTransition(RotationTransition transition) { m_rotationTransition = transition; }
    MirrorTransition mirrorTransition() const { return m_mirrorTransition; }
    void setMirrorTransition(MirrorTransition transition) { m_mirrorTransition = transition; }
    FitModeTransition fitModeTransition() const { return m_fitModeTransition; }
    void setFitModeTransition(FitModeTransition transition) { m_fitModeTransition = transition; }
    ImageViewport::FitMode fitMode() const { return m_fitMode; }
    void setFitMode(ImageViewport::FitMode mode)
    {
        m_fitMode = mode;
        m_fitModeSet = true;
    }
    SpreadDirectionTransition spreadDirectionTransition() const
    {
        return m_spreadDirectionTransition;
    }
    void setSpreadDirectionTransition(SpreadDirectionTransition transition)
    {
        m_spreadDirectionTransition = transition;
    }
    ImageViewport::SpreadDirection spreadDirection() const { return m_spreadDirection; }
    void setSpreadDirection(ImageViewport::SpreadDirection direction)
    {
        m_spreadDirection = direction;
        m_spreadDirectionSet = true;
    }
    PageGapTransition pageGapTransition() const { return m_pageGapTransition; }
    void setPageGapTransition(PageGapTransition transition) { m_pageGapTransition = transition; }
    double pageGap() const { return m_pageGap; }
    void setPageGap(double gap)
    {
        m_pageGap = gap;
        m_pageGapSet = true;
    }
    ReplacementIntent replacementIntent() const { return m_replacementIntent; }
    void setReplacementIntent(ReplacementIntent intent) { m_replacementIntent = intent; }

    bool hasExplicitFitMode() const { return m_fitModeSet; }
    bool hasExplicitSpreadDirection() const { return m_spreadDirectionSet; }
    bool hasExplicitPageGap() const { return m_pageGapSet; }
    bool isValid() const;

    friend bool operator==(
        PresentationTargetTransitionPolicy lhs, PresentationTargetTransitionPolicy rhs)
    {
        return lhs.m_displayTransition == rhs.m_displayTransition
            && lhs.m_zoomTransition == rhs.m_zoomTransition
            && lhs.m_contentPositionTransition == rhs.m_contentPositionTransition
            && lhs.m_rotationTransition == rhs.m_rotationTransition
            && lhs.m_mirrorTransition == rhs.m_mirrorTransition
            && lhs.m_fitModeTransition == rhs.m_fitModeTransition && lhs.m_fitMode == rhs.m_fitMode
            && lhs.m_fitModeSet == rhs.m_fitModeSet
            && lhs.m_spreadDirectionTransition == rhs.m_spreadDirectionTransition
            && lhs.m_spreadDirection == rhs.m_spreadDirection
            && lhs.m_spreadDirectionSet == rhs.m_spreadDirectionSet
            && lhs.m_pageGapTransition == rhs.m_pageGapTransition && lhs.m_pageGap == rhs.m_pageGap
            && lhs.m_pageGapSet == rhs.m_pageGapSet
            && lhs.m_replacementIntent == rhs.m_replacementIntent;
    }
    friend bool operator!=(
        PresentationTargetTransitionPolicy lhs, PresentationTargetTransitionPolicy rhs)
    {
        return !(lhs == rhs);
    }

private:
    DisplayTransition m_displayTransition = DisplayTransition::RetainPrevious;
    ZoomTransition m_zoomTransition = ZoomTransition::Preserve;
    ContentPositionTransition m_contentPositionTransition = ContentPositionTransition::Clamp;
    RotationTransition m_rotationTransition = RotationTransition::Preserve;
    MirrorTransition m_mirrorTransition = MirrorTransition::Preserve;
    FitModeTransition m_fitModeTransition = FitModeTransition::Preserve;
    ImageViewport::FitMode m_fitMode = ImageViewport::FitMode::Contain;
    bool m_fitModeSet = false;
    SpreadDirectionTransition m_spreadDirectionTransition = SpreadDirectionTransition::Preserve;
    ImageViewport::SpreadDirection m_spreadDirection = ImageViewport::SpreadDirection::LeftToRight;
    bool m_spreadDirectionSet = false;
    PageGapTransition m_pageGapTransition = PageGapTransition::Preserve;
    double m_pageGap = 0.0;
    bool m_pageGapSet = false;
    ReplacementIntent m_replacementIntent = ReplacementIntent::NewTarget;
};

class ImageViewportRequestSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRequestSnapshot)
    Q_PROPERTY(ImageViewport::RequestStatus status READ status CONSTANT)
    Q_PROPERTY(ImageViewport::RequestReason reason READ reason CONSTANT)
    Q_PROPERTY(ImageViewport::PlaybackPhase playbackPhase READ playbackPhase CONSTANT)
    Q_PROPERTY(ImageViewportPresentationTargetGenerationToken acceptedPresentationTargetGeneration
            READ acceptedPresentationTargetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet acceptedRoleSet READ acceptedRoleSet CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet targetRoleSet READ targetRoleSet CONSTANT)
    Q_PROPERTY(QVariant activeRole READ activeRole CONSTANT)
    Q_PROPERTY(QVariant playbackRole READ playbackRole CONSTANT)

public:
    ImageViewportRequestSnapshot() = default;
    ImageViewportRequestSnapshot(ImageViewport::RequestStatus status,
        ImageViewport::RequestReason reason, ImageViewport::PlaybackPhase playbackPhase,
        ImageViewportPresentationTargetGenerationToken acceptedPresentationTargetGeneration,
        ImageViewportRoleSet acceptedRoleSet, ImageViewportRoleSet targetRoleSet,
        QVariant activeRole, QVariant playbackRole)
        : m_status(status)
        , m_reason(reason)
        , m_playbackPhase(playbackPhase)
        , m_acceptedPresentationTargetGeneration(acceptedPresentationTargetGeneration)
        , m_acceptedRoleSet(acceptedRoleSet)
        , m_targetRoleSet(targetRoleSet)
        , m_activeRole(std::move(activeRole))
        , m_playbackRole(std::move(playbackRole))
    {
    }

    ImageViewport::RequestStatus status() const { return m_status; }
    ImageViewport::RequestReason reason() const { return m_reason; }
    ImageViewport::PlaybackPhase playbackPhase() const { return m_playbackPhase; }
    ImageViewportPresentationTargetGenerationToken acceptedPresentationTargetGeneration() const
    {
        return m_acceptedPresentationTargetGeneration;
    }
    ImageViewportRoleSet acceptedRoleSet() const { return m_acceptedRoleSet; }
    ImageViewportRoleSet targetRoleSet() const { return m_targetRoleSet; }
    QVariant activeRole() const { return m_activeRole; }
    QVariant playbackRole() const { return m_playbackRole; }

    friend bool operator==(
        const ImageViewportRequestSnapshot& lhs, const ImageViewportRequestSnapshot& rhs)
    {
        return lhs.m_status == rhs.m_status && lhs.m_reason == rhs.m_reason
            && lhs.m_playbackPhase == rhs.m_playbackPhase
            && lhs.m_acceptedPresentationTargetGeneration
            == rhs.m_acceptedPresentationTargetGeneration
            && lhs.m_acceptedRoleSet == rhs.m_acceptedRoleSet
            && lhs.m_targetRoleSet == rhs.m_targetRoleSet && lhs.m_activeRole == rhs.m_activeRole
            && lhs.m_playbackRole == rhs.m_playbackRole;
    }
    friend bool operator!=(
        const ImageViewportRequestSnapshot& lhs, const ImageViewportRequestSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewport::RequestStatus m_status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason m_reason = ImageViewport::RequestReason::NoRequest;
    ImageViewport::PlaybackPhase m_playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    ImageViewportPresentationTargetGenerationToken m_acceptedPresentationTargetGeneration;
    ImageViewportRoleSet m_acceptedRoleSet;
    ImageViewportRoleSet m_targetRoleSet;
    QVariant m_activeRole;
    QVariant m_playbackRole;
};

class ImageViewportDisplaySnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportDisplaySnapshot)
    Q_PROPERTY(ImageViewport::DisplayStatus status READ status CONSTANT)
    Q_PROPERTY(ImageViewport::DisplayPhase phase READ phase CONSTANT)
    Q_PROPERTY(ImageViewportPresentationTargetGenerationToken displayedPresentationTargetGeneration
            READ displayedPresentationTargetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet displayedRoleSet READ displayedRoleSet CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet targetRoleSet READ targetRoleSet CONSTANT)
    Q_PROPERTY(
        bool belongsToAcceptedPresentationTarget READ belongsToAcceptedPresentationTarget CONSTANT)
    Q_PROPERTY(bool retained READ retained CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken displayedPresentationRevision READ
            displayedPresentationRevision CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken targetPresentationRevision READ targetPresentationRevision
            CONSTANT)
    Q_PROPERTY(QSizeF spreadSize READ spreadSize CONSTANT)
    Q_PROPERTY(QRectF contentRect READ contentRect CONSTANT)
    Q_PROPERTY(QSizeF contentSize READ contentSize CONSTANT)
    Q_PROPERTY(QPointF contentPosition READ contentPosition CONSTANT)
    Q_PROPERTY(QPointF maximumContentPosition READ maximumContentPosition CONSTANT)
    Q_PROPERTY(QRectF visibleSpreadRect READ visibleSpreadRect CONSTANT)
    Q_PROPERTY(bool horizontalPannable READ horizontalPannable CONSTANT)
    Q_PROPERTY(bool verticalPannable READ verticalPannable CONSTANT)

public:
    ImageViewportDisplaySnapshot() = default;
    ImageViewportDisplaySnapshot(ImageViewport::DisplayStatus status,
        ImageViewport::DisplayPhase phase,
        ImageViewportPresentationTargetGenerationToken displayedPresentationTargetGeneration,
        ImageViewportRoleSet displayedRoleSet, ImageViewportRoleSet targetRoleSet,
        bool belongsToAcceptedPresentationTarget, bool retained,
        ImageViewportRevisionToken displayedPresentationRevision,
        ImageViewportRevisionToken targetPresentationRevision, QSizeF spreadSize,
        QRectF contentRect, QSizeF contentSize, QPointF contentPosition,
        QPointF maximumContentPosition, QRectF visibleSpreadRect, bool horizontalPannable,
        bool verticalPannable)
        : m_status(status)
        , m_phase(phase)
        , m_displayedPresentationTargetGeneration(displayedPresentationTargetGeneration)
        , m_displayedRoleSet(displayedRoleSet)
        , m_targetRoleSet(targetRoleSet)
        , m_belongsToAcceptedPresentationTarget(belongsToAcceptedPresentationTarget)
        , m_retained(retained)
        , m_displayedPresentationRevision(displayedPresentationRevision)
        , m_targetPresentationRevision(targetPresentationRevision)
        , m_spreadSize(spreadSize)
        , m_contentRect(contentRect)
        , m_contentSize(contentSize)
        , m_contentPosition(contentPosition)
        , m_maximumContentPosition(maximumContentPosition)
        , m_visibleSpreadRect(visibleSpreadRect)
        , m_horizontalPannable(horizontalPannable)
        , m_verticalPannable(verticalPannable)
    {
    }

    ImageViewport::DisplayStatus status() const { return m_status; }
    ImageViewport::DisplayPhase phase() const { return m_phase; }
    ImageViewportPresentationTargetGenerationToken displayedPresentationTargetGeneration() const
    {
        return m_displayedPresentationTargetGeneration;
    }
    ImageViewportRoleSet displayedRoleSet() const { return m_displayedRoleSet; }
    ImageViewportRoleSet targetRoleSet() const { return m_targetRoleSet; }
    bool belongsToAcceptedPresentationTarget() const
    {
        return m_belongsToAcceptedPresentationTarget;
    }
    bool retained() const { return m_retained; }
    ImageViewportRevisionToken displayedPresentationRevision() const
    {
        return m_displayedPresentationRevision;
    }
    ImageViewportRevisionToken targetPresentationRevision() const
    {
        return m_targetPresentationRevision;
    }
    QSizeF spreadSize() const { return m_spreadSize; }
    QRectF contentRect() const { return m_contentRect; }
    QSizeF contentSize() const { return m_contentSize; }
    QPointF contentPosition() const { return m_contentPosition; }
    QPointF maximumContentPosition() const { return m_maximumContentPosition; }
    QRectF visibleSpreadRect() const { return m_visibleSpreadRect; }
    bool horizontalPannable() const { return m_horizontalPannable; }
    bool verticalPannable() const { return m_verticalPannable; }

    friend bool operator==(
        const ImageViewportDisplaySnapshot& lhs, const ImageViewportDisplaySnapshot& rhs)
    {
        return lhs.m_status == rhs.m_status && lhs.m_phase == rhs.m_phase
            && lhs.m_displayedPresentationTargetGeneration
            == rhs.m_displayedPresentationTargetGeneration
            && lhs.m_displayedRoleSet == rhs.m_displayedRoleSet
            && lhs.m_targetRoleSet == rhs.m_targetRoleSet
            && lhs.m_belongsToAcceptedPresentationTarget
            == rhs.m_belongsToAcceptedPresentationTarget
            && lhs.m_retained == rhs.m_retained
            && lhs.m_displayedPresentationRevision == rhs.m_displayedPresentationRevision
            && lhs.m_targetPresentationRevision == rhs.m_targetPresentationRevision
            && lhs.m_spreadSize == rhs.m_spreadSize && lhs.m_contentRect == rhs.m_contentRect
            && lhs.m_contentSize == rhs.m_contentSize
            && lhs.m_contentPosition == rhs.m_contentPosition
            && lhs.m_maximumContentPosition == rhs.m_maximumContentPosition
            && lhs.m_visibleSpreadRect == rhs.m_visibleSpreadRect
            && lhs.m_horizontalPannable == rhs.m_horizontalPannable
            && lhs.m_verticalPannable == rhs.m_verticalPannable;
    }
    friend bool operator!=(
        const ImageViewportDisplaySnapshot& lhs, const ImageViewportDisplaySnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewport::DisplayStatus m_status = ImageViewport::DisplayStatus::Empty;
    ImageViewport::DisplayPhase m_phase = ImageViewport::DisplayPhase::NoPresentation;
    ImageViewportPresentationTargetGenerationToken m_displayedPresentationTargetGeneration;
    ImageViewportRoleSet m_displayedRoleSet;
    ImageViewportRoleSet m_targetRoleSet;
    bool m_belongsToAcceptedPresentationTarget = false;
    bool m_retained = false;
    ImageViewportRevisionToken m_displayedPresentationRevision;
    ImageViewportRevisionToken m_targetPresentationRevision;
    QSizeF m_spreadSize;
    QRectF m_contentRect;
    QSizeF m_contentSize;
    QPointF m_contentPosition;
    QPointF m_maximumContentPosition;
    QRectF m_visibleSpreadRect;
    bool m_horizontalPannable = false;
    bool m_verticalPannable = false;
};

class ImageViewportPresentationSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPresentationSnapshot)
    Q_PROPERTY(ImageViewport::FitMode fitMode READ fitMode CONSTANT)
    Q_PROPERTY(double zoomPercent READ zoomPercent CONSTANT)
    Q_PROPERTY(double minimumManualZoomPercent READ minimumManualZoomPercent CONSTANT)
    Q_PROPERTY(double maximumManualZoomPercent READ maximumManualZoomPercent CONSTANT)
    Q_PROPERTY(double manualZoomStepFactor READ manualZoomStepFactor CONSTANT)
    Q_PROPERTY(int rotationDegrees READ rotationDegrees CONSTANT)
    Q_PROPERTY(bool mirrorHorizontally READ mirrorHorizontally CONSTANT)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically CONSTANT)
    Q_PROPERTY(ImageViewport::SpreadDirection spreadDirection READ spreadDirection CONSTANT)
    Q_PROPERTY(double pageGap READ pageGap CONSTANT)
    Q_PROPERTY(ImageViewport::BackgroundMode backgroundMode READ backgroundMode CONSTANT)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor CONSTANT)
    Q_PROPERTY(bool smoothing READ smoothing CONSTANT)
    Q_PROPERTY(bool mipmap READ mipmap CONSTANT)
    Q_PROPERTY(bool looping READ looping CONSTANT)
    Q_PROPERTY(ImageViewportQualityPreference qualityPreference READ qualityPreference CONSTANT)
    Q_PROPERTY(
        ImageViewportExactnessPreference exactnessPreference READ exactnessPreference CONSTANT)

public:
    ImageViewportPresentationSnapshot() = default;
    ImageViewportPresentationSnapshot(ImageViewport::FitMode fitMode, double zoomPercent,
        double minimumManualZoomPercent, double maximumManualZoomPercent,
        double manualZoomStepFactor, int rotationDegrees, bool mirrorHorizontally,
        bool mirrorVertically, ImageViewport::SpreadDirection spreadDirection, double pageGap,
        ImageViewport::BackgroundMode backgroundMode, QColor backgroundColor, bool smoothing,
        bool mipmap, bool looping, ImageViewportQualityPreference qualityPreference,
        ImageViewportExactnessPreference exactnessPreference)
        : m_fitMode(fitMode)
        , m_zoomPercent(zoomPercent)
        , m_minimumManualZoomPercent(minimumManualZoomPercent)
        , m_maximumManualZoomPercent(maximumManualZoomPercent)
        , m_manualZoomStepFactor(manualZoomStepFactor)
        , m_rotationDegrees(rotationDegrees)
        , m_mirrorHorizontally(mirrorHorizontally)
        , m_mirrorVertically(mirrorVertically)
        , m_spreadDirection(spreadDirection)
        , m_pageGap(pageGap)
        , m_backgroundMode(backgroundMode)
        , m_backgroundColor(backgroundColor)
        , m_smoothing(smoothing)
        , m_mipmap(mipmap)
        , m_looping(looping)
        , m_qualityPreference(qualityPreference)
        , m_exactnessPreference(exactnessPreference)
    {
    }

    ImageViewport::FitMode fitMode() const { return m_fitMode; }
    double zoomPercent() const { return m_zoomPercent; }
    double minimumManualZoomPercent() const { return m_minimumManualZoomPercent; }
    double maximumManualZoomPercent() const { return m_maximumManualZoomPercent; }
    double manualZoomStepFactor() const { return m_manualZoomStepFactor; }
    int rotationDegrees() const { return m_rotationDegrees; }
    bool mirrorHorizontally() const { return m_mirrorHorizontally; }
    bool mirrorVertically() const { return m_mirrorVertically; }
    ImageViewport::SpreadDirection spreadDirection() const { return m_spreadDirection; }
    double pageGap() const { return m_pageGap; }
    ImageViewport::BackgroundMode backgroundMode() const { return m_backgroundMode; }
    QColor backgroundColor() const { return m_backgroundColor; }
    bool smoothing() const { return m_smoothing; }
    bool mipmap() const { return m_mipmap; }
    bool looping() const { return m_looping; }
    ImageViewportQualityPreference qualityPreference() const { return m_qualityPreference; }
    ImageViewportExactnessPreference exactnessPreference() const { return m_exactnessPreference; }

    friend bool operator==(
        const ImageViewportPresentationSnapshot& lhs, const ImageViewportPresentationSnapshot& rhs)
    {
        return lhs.m_fitMode == rhs.m_fitMode && lhs.m_zoomPercent == rhs.m_zoomPercent
            && lhs.m_minimumManualZoomPercent == rhs.m_minimumManualZoomPercent
            && lhs.m_maximumManualZoomPercent == rhs.m_maximumManualZoomPercent
            && lhs.m_manualZoomStepFactor == rhs.m_manualZoomStepFactor
            && lhs.m_rotationDegrees == rhs.m_rotationDegrees
            && lhs.m_mirrorHorizontally == rhs.m_mirrorHorizontally
            && lhs.m_mirrorVertically == rhs.m_mirrorVertically
            && lhs.m_spreadDirection == rhs.m_spreadDirection && lhs.m_pageGap == rhs.m_pageGap
            && lhs.m_backgroundMode == rhs.m_backgroundMode
            && lhs.m_backgroundColor == rhs.m_backgroundColor && lhs.m_smoothing == rhs.m_smoothing
            && lhs.m_mipmap == rhs.m_mipmap && lhs.m_looping == rhs.m_looping
            && lhs.m_qualityPreference == rhs.m_qualityPreference
            && lhs.m_exactnessPreference == rhs.m_exactnessPreference;
    }
    friend bool operator!=(
        const ImageViewportPresentationSnapshot& lhs, const ImageViewportPresentationSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewport::FitMode m_fitMode = ImageViewport::FitMode::Contain;
    double m_zoomPercent = 100.0;
    double m_minimumManualZoomPercent = 0.0;
    double m_maximumManualZoomPercent = 0.0;
    double m_manualZoomStepFactor = 1.0;
    int m_rotationDegrees = 0;
    bool m_mirrorHorizontally = false;
    bool m_mirrorVertically = false;
    ImageViewport::SpreadDirection m_spreadDirection = ImageViewport::SpreadDirection::LeftToRight;
    double m_pageGap = 0.0;
    ImageViewport::BackgroundMode m_backgroundMode = ImageViewport::BackgroundMode::Transparent;
    QColor m_backgroundColor = Qt::transparent;
    bool m_smoothing = true;
    bool m_mipmap = false;
    bool m_looping = false;
    ImageViewportQualityPreference m_qualityPreference = ImageViewportQualityPreference::Default;
    ImageViewportExactnessPreference m_exactnessPreference
        = ImageViewportExactnessPreference::Default;
};

class ImageViewportRoleRequestSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleRequestSnapshot)
    Q_PROPERTY(
        bool belongsToAcceptedPresentationTarget READ belongsToAcceptedPresentationTarget CONSTANT)
    Q_PROPERTY(ImageViewportPresentationTargetGenerationToken presentationTargetGeneration READ
            presentationTargetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportPageRole role READ role CONSTANT)
    Q_PROPERTY(int frame READ frame CONSTANT)
    Q_PROPERTY(int position READ position CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(ImageViewportDemandRevisionToken demandRevision READ demandRevision CONSTANT)

public:
    ImageViewportRoleRequestSnapshot() = default;
    ImageViewportRoleRequestSnapshot(bool belongsToAcceptedPresentationTarget,
        ImageViewportPresentationTargetGenerationToken presentationTargetGeneration,
        ImageViewportPageRole role, int frame, int position, QSizeF sourceLogicalSize,
        ImageViewportDemandRevisionToken demandRevision)
        : m_belongsToAcceptedPresentationTarget(belongsToAcceptedPresentationTarget)
        , m_presentationTargetGeneration(presentationTargetGeneration)
        , m_role(role)
        , m_frame(frame)
        , m_position(position)
        , m_sourceLogicalSize(sourceLogicalSize)
        , m_demandRevision(demandRevision)
    {
    }

    bool belongsToAcceptedPresentationTarget() const
    {
        return m_belongsToAcceptedPresentationTarget;
    }
    ImageViewportPresentationTargetGenerationToken presentationTargetGeneration() const
    {
        return m_presentationTargetGeneration;
    }
    ImageViewportPageRole role() const { return m_role; }
    int frame() const { return m_frame; }
    int position() const { return m_position; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }

    friend bool operator==(
        const ImageViewportRoleRequestSnapshot& lhs, const ImageViewportRoleRequestSnapshot& rhs)
    {
        return lhs.m_belongsToAcceptedPresentationTarget
            == rhs.m_belongsToAcceptedPresentationTarget
            && lhs.m_presentationTargetGeneration == rhs.m_presentationTargetGeneration
            && lhs.m_role == rhs.m_role && lhs.m_frame == rhs.m_frame
            && lhs.m_position == rhs.m_position
            && lhs.m_sourceLogicalSize == rhs.m_sourceLogicalSize
            && lhs.m_demandRevision == rhs.m_demandRevision;
    }
    friend bool operator!=(
        const ImageViewportRoleRequestSnapshot& lhs, const ImageViewportRoleRequestSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_belongsToAcceptedPresentationTarget = false;
    ImageViewportPresentationTargetGenerationToken m_presentationTargetGeneration;
    ImageViewportPageRole m_role = ImageViewportPageRole::Primary;
    int m_frame = -1;
    int m_position = -1;
    QSizeF m_sourceLogicalSize;
    ImageViewportDemandRevisionToken m_demandRevision;
};

class ImageViewportRoleDisplaySnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleDisplaySnapshot)
    Q_PROPERTY(
        bool belongsToAcceptedPresentationTarget READ belongsToAcceptedPresentationTarget CONSTANT)
    Q_PROPERTY(bool retained READ retained CONSTANT)
    Q_PROPERTY(int frame READ frame CONSTANT)
    Q_PROPERTY(int position READ position CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(QSizeF payloadRasterSize READ payloadRasterSize CONSTANT)
    Q_PROPERTY(QSizeF sourceToPayloadScale READ sourceToPayloadScale CONSTANT)
    Q_PROPERTY(ImageViewportPayloadQuality quality READ quality CONSTANT)
    Q_PROPERTY(ImageViewportPayloadExactness exactness READ exactness CONSTANT)
    Q_PROPERTY(bool currentForDemand READ currentForDemand CONSTANT)
    Q_PROPERTY(ImageViewportDemandRevisionToken demandRevision READ demandRevision CONSTANT)

public:
    ImageViewportRoleDisplaySnapshot() = default;
    ImageViewportRoleDisplaySnapshot(bool belongsToAcceptedPresentationTarget, bool retained,
        int frame, int position, QSizeF sourceLogicalSize, QSizeF payloadRasterSize,
        QSizeF sourceToPayloadScale, ImageViewportPayloadQuality quality,
        ImageViewportPayloadExactness exactness, bool currentForDemand,
        ImageViewportDemandRevisionToken demandRevision)
        : m_belongsToAcceptedPresentationTarget(belongsToAcceptedPresentationTarget)
        , m_retained(retained)
        , m_frame(frame)
        , m_position(position)
        , m_sourceLogicalSize(sourceLogicalSize)
        , m_payloadRasterSize(payloadRasterSize)
        , m_sourceToPayloadScale(sourceToPayloadScale)
        , m_quality(quality)
        , m_exactness(exactness)
        , m_currentForDemand(currentForDemand)
        , m_demandRevision(demandRevision)
    {
    }

    bool belongsToAcceptedPresentationTarget() const
    {
        return m_belongsToAcceptedPresentationTarget;
    }
    bool retained() const { return m_retained; }
    int frame() const { return m_frame; }
    int position() const { return m_position; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    QSizeF payloadRasterSize() const { return m_payloadRasterSize; }
    QSizeF sourceToPayloadScale() const { return m_sourceToPayloadScale; }
    ImageViewportPayloadQuality quality() const { return m_quality; }
    ImageViewportPayloadExactness exactness() const { return m_exactness; }
    bool currentForDemand() const { return m_currentForDemand; }
    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }

    friend bool operator==(
        const ImageViewportRoleDisplaySnapshot& lhs, const ImageViewportRoleDisplaySnapshot& rhs)
    {
        return lhs.m_belongsToAcceptedPresentationTarget
            == rhs.m_belongsToAcceptedPresentationTarget
            && lhs.m_retained == rhs.m_retained && lhs.m_frame == rhs.m_frame
            && lhs.m_position == rhs.m_position
            && lhs.m_sourceLogicalSize == rhs.m_sourceLogicalSize
            && lhs.m_payloadRasterSize == rhs.m_payloadRasterSize
            && lhs.m_sourceToPayloadScale == rhs.m_sourceToPayloadScale
            && lhs.m_quality == rhs.m_quality && lhs.m_exactness == rhs.m_exactness
            && lhs.m_currentForDemand == rhs.m_currentForDemand
            && lhs.m_demandRevision == rhs.m_demandRevision;
    }
    friend bool operator!=(
        const ImageViewportRoleDisplaySnapshot& lhs, const ImageViewportRoleDisplaySnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_belongsToAcceptedPresentationTarget = false;
    bool m_retained = false;
    int m_frame = -1;
    int m_position = -1;
    QSizeF m_sourceLogicalSize;
    QSizeF m_payloadRasterSize;
    QSizeF m_sourceToPayloadScale;
    ImageViewportPayloadQuality m_quality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness m_exactness = ImageViewportPayloadExactness::Unknown;
    bool m_currentForDemand = false;
    ImageViewportDemandRevisionToken m_demandRevision;
};

class ImageViewportRoleMetadataSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleMetadataSnapshot)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(int frameCount READ frameCount CONSTANT)
    Q_PROPERTY(int totalDuration READ totalDuration CONSTANT)
    Q_PROPERTY(ImageViewportRange frameSeekBounds READ frameSeekBounds CONSTANT)
    Q_PROPERTY(ImageViewportRange positionSeekBounds READ positionSeekBounds CONSTANT)
    Q_PROPERTY(ImageViewportCapabilitySupport frameSeekSupport READ frameSeekSupport CONSTANT)
    Q_PROPERTY(ImageViewportCapabilitySupport positionSeekSupport READ positionSeekSupport CONSTANT)
    Q_PROPERTY(
        ImageViewportCapabilitySupport timedPlaybackSupport READ timedPlaybackSupport CONSTANT)
    Q_PROPERTY(bool autoplay READ autoplay CONSTANT)
    Q_PROPERTY(bool progressiveAnimationReadiness READ progressiveAnimationReadiness CONSTANT)
    Q_PROPERTY(ImageSequenceAuthoredAnimationLoopMode loopMode READ loopMode CONSTANT)
    Q_PROPERTY(int loopCount READ loopCount CONSTANT)

public:
    ImageViewportRoleMetadataSnapshot() = default;
    ImageViewportRoleMetadataSnapshot(bool available, QSizeF sourceLogicalSize, int frameCount,
        int totalDuration, ImageViewportRange frameSeekBounds,
        ImageViewportRange positionSeekBounds, ImageViewportCapabilitySupport frameSeekSupport,
        ImageViewportCapabilitySupport positionSeekSupport,
        ImageViewportCapabilitySupport timedPlaybackSupport, bool autoplay,
        bool progressiveAnimationReadiness, ImageSequenceAuthoredAnimationLoopMode loopMode,
        int loopCount)
        : m_available(available)
        , m_sourceLogicalSize(sourceLogicalSize)
        , m_frameCount(frameCount)
        , m_totalDuration(totalDuration)
        , m_frameSeekBounds(frameSeekBounds)
        , m_positionSeekBounds(positionSeekBounds)
        , m_frameSeekSupport(frameSeekSupport)
        , m_positionSeekSupport(positionSeekSupport)
        , m_timedPlaybackSupport(timedPlaybackSupport)
        , m_autoplay(autoplay)
        , m_progressiveAnimationReadiness(progressiveAnimationReadiness)
        , m_loopMode(loopMode)
        , m_loopCount(loopCount)
    {
    }

    bool available() const { return m_available; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    int frameCount() const { return m_frameCount; }
    int totalDuration() const { return m_totalDuration; }
    ImageViewportRange frameSeekBounds() const { return m_frameSeekBounds; }
    ImageViewportRange positionSeekBounds() const { return m_positionSeekBounds; }
    ImageViewportCapabilitySupport frameSeekSupport() const { return m_frameSeekSupport; }
    ImageViewportCapabilitySupport positionSeekSupport() const { return m_positionSeekSupport; }
    ImageViewportCapabilitySupport timedPlaybackSupport() const { return m_timedPlaybackSupport; }
    bool autoplay() const { return m_autoplay; }
    bool progressiveAnimationReadiness() const { return m_progressiveAnimationReadiness; }
    ImageSequenceAuthoredAnimationLoopMode loopMode() const { return m_loopMode; }
    int loopCount() const { return m_loopCount; }

    friend bool operator==(
        const ImageViewportRoleMetadataSnapshot& lhs, const ImageViewportRoleMetadataSnapshot& rhs)
    {
        return lhs.m_available == rhs.m_available
            && lhs.m_sourceLogicalSize == rhs.m_sourceLogicalSize
            && lhs.m_frameCount == rhs.m_frameCount && lhs.m_totalDuration == rhs.m_totalDuration
            && lhs.m_frameSeekBounds == rhs.m_frameSeekBounds
            && lhs.m_positionSeekBounds == rhs.m_positionSeekBounds
            && lhs.m_frameSeekSupport == rhs.m_frameSeekSupport
            && lhs.m_positionSeekSupport == rhs.m_positionSeekSupport
            && lhs.m_timedPlaybackSupport == rhs.m_timedPlaybackSupport
            && lhs.m_autoplay == rhs.m_autoplay
            && lhs.m_progressiveAnimationReadiness == rhs.m_progressiveAnimationReadiness
            && lhs.m_loopMode == rhs.m_loopMode && lhs.m_loopCount == rhs.m_loopCount;
    }
    friend bool operator!=(
        const ImageViewportRoleMetadataSnapshot& lhs, const ImageViewportRoleMetadataSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_available = false;
    QSizeF m_sourceLogicalSize;
    int m_frameCount = -1;
    int m_totalDuration = -1;
    ImageViewportRange m_frameSeekBounds;
    ImageViewportRange m_positionSeekBounds;
    ImageViewportCapabilitySupport m_frameSeekSupport = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport m_positionSeekSupport
        = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport m_timedPlaybackSupport
        = ImageViewportCapabilitySupport::Unavailable;
    bool m_autoplay = false;
    bool m_progressiveAnimationReadiness = false;
    ImageSequenceAuthoredAnimationLoopMode m_loopMode
        = ImageSequenceAuthoredAnimationLoopMode::PlayOnce;
    int m_loopCount = -1;
};

class ImageViewportRoleGeometrySnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleGeometrySnapshot)
    Q_PROPERTY(QRectF acceptedPageRect READ acceptedPageRect CONSTANT)
    Q_PROPERTY(QRectF acceptedItemRect READ acceptedItemRect CONSTANT)
    Q_PROPERTY(QRectF acceptedVisiblePageRect READ acceptedVisiblePageRect CONSTANT)
    Q_PROPERTY(QRectF displayedPageRect READ displayedPageRect CONSTANT)
    Q_PROPERTY(QRectF displayedItemRect READ displayedItemRect CONSTANT)
    Q_PROPERTY(QRectF displayedVisiblePageRect READ displayedVisiblePageRect CONSTANT)

public:
    ImageViewportRoleGeometrySnapshot() = default;
    ImageViewportRoleGeometrySnapshot(QRectF acceptedPageRect, QRectF acceptedItemRect,
        QRectF acceptedVisiblePageRect, QRectF displayedPageRect, QRectF displayedItemRect,
        QRectF displayedVisiblePageRect)
        : m_acceptedPageRect(acceptedPageRect)
        , m_acceptedItemRect(acceptedItemRect)
        , m_acceptedVisiblePageRect(acceptedVisiblePageRect)
        , m_displayedPageRect(displayedPageRect)
        , m_displayedItemRect(displayedItemRect)
        , m_displayedVisiblePageRect(displayedVisiblePageRect)
    {
    }

    QRectF acceptedPageRect() const { return m_acceptedPageRect; }
    QRectF acceptedItemRect() const { return m_acceptedItemRect; }
    QRectF acceptedVisiblePageRect() const { return m_acceptedVisiblePageRect; }
    QRectF displayedPageRect() const { return m_displayedPageRect; }
    QRectF displayedItemRect() const { return m_displayedItemRect; }
    QRectF displayedVisiblePageRect() const { return m_displayedVisiblePageRect; }

    friend bool operator==(
        const ImageViewportRoleGeometrySnapshot& lhs, const ImageViewportRoleGeometrySnapshot& rhs)
    {
        return lhs.m_acceptedPageRect == rhs.m_acceptedPageRect
            && lhs.m_acceptedItemRect == rhs.m_acceptedItemRect
            && lhs.m_acceptedVisiblePageRect == rhs.m_acceptedVisiblePageRect
            && lhs.m_displayedPageRect == rhs.m_displayedPageRect
            && lhs.m_displayedItemRect == rhs.m_displayedItemRect
            && lhs.m_displayedVisiblePageRect == rhs.m_displayedVisiblePageRect;
    }
    friend bool operator!=(
        const ImageViewportRoleGeometrySnapshot& lhs, const ImageViewportRoleGeometrySnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    QRectF m_acceptedPageRect;
    QRectF m_acceptedItemRect;
    QRectF m_acceptedVisiblePageRect;
    QRectF m_displayedPageRect;
    QRectF m_displayedItemRect;
    QRectF m_displayedVisiblePageRect;
};

class ImageViewportRoleSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleSnapshot)
    Q_PROPERTY(bool present READ present CONSTANT)
    Q_PROPERTY(ImageSequence* sequence READ sequence CONSTANT)
    Q_PROPERTY(ImageViewportRoleRequestSnapshot request READ request CONSTANT)
    Q_PROPERTY(ImageViewportRoleDisplaySnapshot display READ display CONSTANT)
    Q_PROPERTY(ImageViewportRoleMetadataSnapshot metadata READ metadata CONSTANT)
    Q_PROPERTY(ImageViewportRoleGeometrySnapshot geometry READ geometry CONSTANT)

public:
    ImageViewportRoleSnapshot() = default;
    ImageViewportRoleSnapshot(bool present, ImageSequence* sequence,
        ImageViewportRoleRequestSnapshot request, ImageViewportRoleDisplaySnapshot display,
        ImageViewportRoleMetadataSnapshot metadata, ImageViewportRoleGeometrySnapshot geometry)
        : m_present(present)
        , m_sequence(sequence)
        , m_request(request)
        , m_display(display)
        , m_metadata(metadata)
        , m_geometry(geometry)
    {
    }

    bool present() const { return m_present; }
    ImageSequence* sequence() const { return m_sequence; }
    ImageViewportRoleRequestSnapshot request() const { return m_request; }
    ImageViewportRoleDisplaySnapshot display() const { return m_display; }
    ImageViewportRoleMetadataSnapshot metadata() const { return m_metadata; }
    ImageViewportRoleGeometrySnapshot geometry() const { return m_geometry; }

    friend bool operator==(
        const ImageViewportRoleSnapshot& lhs, const ImageViewportRoleSnapshot& rhs)
    {
        return lhs.m_present == rhs.m_present && lhs.m_sequence == rhs.m_sequence
            && lhs.m_request == rhs.m_request && lhs.m_display == rhs.m_display
            && lhs.m_metadata == rhs.m_metadata && lhs.m_geometry == rhs.m_geometry;
    }
    friend bool operator!=(
        const ImageViewportRoleSnapshot& lhs, const ImageViewportRoleSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_present = false;
    QPointer<ImageSequence> m_sequence;
    ImageViewportRoleRequestSnapshot m_request;
    ImageViewportRoleDisplaySnapshot m_display;
    ImageViewportRoleMetadataSnapshot m_metadata;
    ImageViewportRoleGeometrySnapshot m_geometry;
};

class ImageViewportDiagnosticsSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportDiagnosticsSnapshot)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)
    Q_PROPERTY(QString warningString READ warningString CONSTANT)
    Q_PROPERTY(ImageViewport::CommandReason commandReason READ commandReason CONSTANT)

public:
    ImageViewportDiagnosticsSnapshot() = default;
    ImageViewportDiagnosticsSnapshot(
        QString errorString, QString warningString, ImageViewport::CommandReason commandReason)
        : m_errorString(std::move(errorString))
        , m_warningString(std::move(warningString))
        , m_commandReason(commandReason)
    {
    }

    QString errorString() const { return m_errorString; }
    QString warningString() const { return m_warningString; }
    ImageViewport::CommandReason commandReason() const { return m_commandReason; }

    friend bool operator==(
        const ImageViewportDiagnosticsSnapshot& lhs, const ImageViewportDiagnosticsSnapshot& rhs)
    {
        return lhs.m_errorString == rhs.m_errorString && lhs.m_warningString == rhs.m_warningString
            && lhs.m_commandReason == rhs.m_commandReason;
    }
    friend bool operator!=(
        const ImageViewportDiagnosticsSnapshot& lhs, const ImageViewportDiagnosticsSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    QString m_errorString;
    QString m_warningString;
    ImageViewport::CommandReason m_commandReason = ImageViewport::CommandReason::NoCommand;
};

class ImageViewportRevisionsSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRevisionsSnapshot)
    Q_PROPERTY(ImageViewportRevisionToken request READ request CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken display READ display CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken presentation READ presentation CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken command READ command CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken snapshot READ snapshot CONSTANT)

public:
    ImageViewportRevisionsSnapshot() = default;
    ImageViewportRevisionsSnapshot(ImageViewportRevisionToken request,
        ImageViewportRevisionToken display, ImageViewportRevisionToken presentation,
        ImageViewportRevisionToken command, ImageViewportRevisionToken snapshot)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
        , m_command(command)
        , m_snapshot(snapshot)
    {
    }

    ImageViewportRevisionToken request() const { return m_request; }
    ImageViewportRevisionToken display() const { return m_display; }
    ImageViewportRevisionToken presentation() const { return m_presentation; }
    ImageViewportRevisionToken command() const { return m_command; }
    ImageViewportRevisionToken snapshot() const { return m_snapshot; }

    friend bool operator==(
        const ImageViewportRevisionsSnapshot& lhs, const ImageViewportRevisionsSnapshot& rhs)
    {
        return lhs.m_request == rhs.m_request && lhs.m_display == rhs.m_display
            && lhs.m_presentation == rhs.m_presentation && lhs.m_command == rhs.m_command
            && lhs.m_snapshot == rhs.m_snapshot;
    }
    friend bool operator!=(
        const ImageViewportRevisionsSnapshot& lhs, const ImageViewportRevisionsSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportRevisionToken m_request;
    ImageViewportRevisionToken m_display;
    ImageViewportRevisionToken m_presentation;
    ImageViewportRevisionToken m_command;
    ImageViewportRevisionToken m_snapshot;
};

class ImageViewportStateSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportStateSnapshot)
    Q_PROPERTY(ImageViewportRequestSnapshot request READ request CONSTANT)
    Q_PROPERTY(ImageViewportDisplaySnapshot display READ display CONSTANT)
    Q_PROPERTY(ImageViewportPresentationSnapshot presentation READ presentation CONSTANT)
    Q_PROPERTY(ImageViewportRoleSnapshot primary READ primary CONSTANT)
    Q_PROPERTY(ImageViewportRoleSnapshot secondary READ secondary CONSTANT)
    Q_PROPERTY(ImageViewportDiagnosticsSnapshot diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(ImageViewportRevisionsSnapshot revisions READ revisions CONSTANT)

public:
    ImageViewportStateSnapshot() = default;
    ImageViewportStateSnapshot(ImageViewportRequestSnapshot request,
        ImageViewportDisplaySnapshot display, ImageViewportPresentationSnapshot presentation,
        ImageViewportRoleSnapshot primary, ImageViewportRoleSnapshot secondary,
        ImageViewportDiagnosticsSnapshot diagnostics, ImageViewportRevisionsSnapshot revisions)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
        , m_primary(primary)
        , m_secondary(secondary)
        , m_diagnostics(diagnostics)
        , m_revisions(revisions)
    {
    }

    ImageViewportRequestSnapshot request() const { return m_request; }
    ImageViewportDisplaySnapshot display() const { return m_display; }
    ImageViewportPresentationSnapshot presentation() const { return m_presentation; }
    ImageViewportRoleSnapshot primary() const { return m_primary; }
    ImageViewportRoleSnapshot secondary() const { return m_secondary; }
    ImageViewportDiagnosticsSnapshot diagnostics() const { return m_diagnostics; }
    ImageViewportRevisionsSnapshot revisions() const { return m_revisions; }

    friend bool operator==(
        const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
    {
        return lhs.m_request == rhs.m_request && lhs.m_display == rhs.m_display
            && lhs.m_presentation == rhs.m_presentation && lhs.m_primary == rhs.m_primary
            && lhs.m_secondary == rhs.m_secondary && lhs.m_diagnostics == rhs.m_diagnostics
            && lhs.m_revisions == rhs.m_revisions;
    }
    friend bool operator!=(
        const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewportRequestSnapshot m_request;
    ImageViewportDisplaySnapshot m_display;
    ImageViewportPresentationSnapshot m_presentation;
    ImageViewportRoleSnapshot m_primary;
    ImageViewportRoleSnapshot m_secondary;
    ImageViewportDiagnosticsSnapshot m_diagnostics;
    ImageViewportRevisionsSnapshot m_revisions;
};

class ImageViewportCommandResult
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportCommandResult)
    Q_PROPERTY(ImageViewport::CommandOutcome outcome READ outcome CONSTANT)
    Q_PROPERTY(ImageViewport::CommandReason reason READ reason CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken commandRevision READ commandRevision CONSTANT)
    Q_PROPERTY(ImageViewportRevisionToken snapshotRevision READ snapshotRevision CONSTANT)

public:
    ImageViewportCommandResult() = default;
    ImageViewportCommandResult(ImageViewport::CommandOutcome outcome,
        ImageViewport::CommandReason reason, ImageViewportRevisionToken commandRevision,
        ImageViewportRevisionToken snapshotRevision)
        : m_outcome(outcome)
        , m_reason(reason)
        , m_commandRevision(commandRevision)
        , m_snapshotRevision(snapshotRevision)
    {
    }

    ImageViewport::CommandOutcome outcome() const { return m_outcome; }
    ImageViewport::CommandReason reason() const { return m_reason; }
    ImageViewportRevisionToken commandRevision() const { return m_commandRevision; }
    ImageViewportRevisionToken snapshotRevision() const { return m_snapshotRevision; }

    friend bool operator==(
        const ImageViewportCommandResult& lhs, const ImageViewportCommandResult& rhs)
    {
        return lhs.m_outcome == rhs.m_outcome && lhs.m_reason == rhs.m_reason
            && lhs.m_commandRevision == rhs.m_commandRevision
            && lhs.m_snapshotRevision == rhs.m_snapshotRevision;
    }
    friend bool operator!=(
        const ImageViewportCommandResult& lhs, const ImageViewportCommandResult& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewport::CommandOutcome m_outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewport::CommandReason m_reason = ImageViewport::CommandReason::NoCommand;
    ImageViewportRevisionToken m_commandRevision;
    ImageViewportRevisionToken m_snapshotRevision;
};

class ImageViewportCoordinateInput
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportCoordinateInput)
    QML_STRUCTURED_VALUE
    Q_PROPERTY(ImageViewport::CoordinateSpace sourceSpace READ sourceSpace WRITE setSourceSpace)
    Q_PROPERTY(ImageViewport::CoordinateSpace targetSpace READ targetSpace WRITE setTargetSpace)
    Q_PROPERTY(QVariant role READ role WRITE setRole)
    Q_PROPERTY(QPointF point READ point WRITE setPoint)

public:
    ImageViewportCoordinateInput() = default;

    ImageViewport::CoordinateSpace sourceSpace() const { return m_sourceSpace; }
    void setSourceSpace(ImageViewport::CoordinateSpace sourceSpace) { m_sourceSpace = sourceSpace; }
    ImageViewport::CoordinateSpace targetSpace() const { return m_targetSpace; }
    void setTargetSpace(ImageViewport::CoordinateSpace targetSpace) { m_targetSpace = targetSpace; }
    QVariant role() const { return m_role; }
    void setRole(QVariant role) { m_role = std::move(role); }
    QPointF point() const { return m_point; }
    void setPoint(QPointF point) { m_point = point; }

    friend bool operator==(
        const ImageViewportCoordinateInput& lhs, const ImageViewportCoordinateInput& rhs)
    {
        return lhs.m_sourceSpace == rhs.m_sourceSpace && lhs.m_targetSpace == rhs.m_targetSpace
            && lhs.m_role == rhs.m_role && lhs.m_point == rhs.m_point;
    }
    friend bool operator!=(
        const ImageViewportCoordinateInput& lhs, const ImageViewportCoordinateInput& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewport::CoordinateSpace m_sourceSpace = ImageViewport::CoordinateSpace::Item;
    ImageViewport::CoordinateSpace m_targetSpace = ImageViewport::CoordinateSpace::DisplayedSpread;
    QVariant m_role;
    QPointF m_point;
};

class ImageViewportCoordinateResult
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportCoordinateResult)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(QPointF point READ point CONSTANT)
    Q_PROPERTY(ImageViewport::CoordinateSpace space READ space CONSTANT)
    Q_PROPERTY(QVariant role READ role CONSTANT)

public:
    ImageViewportCoordinateResult() = default;
    ImageViewportCoordinateResult(
        bool valid, QPointF point, ImageViewport::CoordinateSpace space, QVariant role = {})
        : m_valid(valid)
        , m_point(point)
        , m_space(space)
        , m_role(std::move(role))
    {
    }

    bool isValid() const { return m_valid; }
    QPointF point() const { return m_point; }
    ImageViewport::CoordinateSpace space() const { return m_space; }
    QVariant role() const { return m_role; }

    friend bool operator==(
        const ImageViewportCoordinateResult& lhs, const ImageViewportCoordinateResult& rhs)
    {
        return lhs.m_valid == rhs.m_valid && lhs.m_point == rhs.m_point
            && lhs.m_space == rhs.m_space && lhs.m_role == rhs.m_role;
    }
    friend bool operator!=(
        const ImageViewportCoordinateResult& lhs, const ImageViewportCoordinateResult& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_valid = false;
    QPointF m_point;
    ImageViewport::CoordinateSpace m_space = ImageViewport::CoordinateSpace::Item;
    QVariant m_role;
};

inline QDebug operator<<(QDebug debug, ImageViewportRange range)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "ImageViewportRange(" << range.minimum() << ", " << range.maximum() << ")";
    return debug;
}

inline QDebug operator<<(QDebug debug, RevisionToken token)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "RevisionToken(valid=" << token.isValid() << ")";
    return debug;
}

Q_DECLARE_METATYPE(ImageSequenceProviderRequestToken)
Q_DECLARE_METATYPE(ImageSequenceAuthoredAnimationFacts)
Q_DECLARE_METATYPE(ImageSequenceProviderMetadata)
Q_DECLARE_METATYPE(ImageSequenceProviderFrameMetadata)
Q_DECLARE_METATYPE(ImageSequenceProviderRequestKind)
Q_DECLARE_METATYPE(ImageSequenceProviderEventKind)
Q_DECLARE_METATYPE(ImageSequenceProviderUnsupportedCause)
Q_DECLARE_METATYPE(ImageSequenceProviderFrameEnvelope)
Q_DECLARE_METATYPE(ImageSequenceProviderDisplayDemand)
Q_DECLARE_METATYPE(ImageSequenceProviderRequest)
Q_DECLARE_METATYPE(ImageSequenceProviderEvent)
Q_DECLARE_METATYPE(ImageSequenceProviderDescriptor)
Q_DECLARE_METATYPE(ImageViewportRange)
Q_DECLARE_METATYPE(RevisionToken)
Q_DECLARE_METATYPE(ImageViewportRevisionToken)
Q_DECLARE_METATYPE(ImageViewportPresentationTargetGenerationToken)
Q_DECLARE_METATYPE(ImageViewportDemandRevisionToken)
Q_DECLARE_METATYPE(ImageViewportRoleSet)
Q_DECLARE_METATYPE(ImageViewportPresentationTarget)
Q_DECLARE_METATYPE(ImageViewportPresentationCommand)
Q_DECLARE_METATYPE(ImageViewportRequestSnapshot)
Q_DECLARE_METATYPE(ImageViewportDisplaySnapshot)
Q_DECLARE_METATYPE(ImageViewportPresentationSnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleRequestSnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleDisplaySnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleMetadataSnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleGeometrySnapshot)
Q_DECLARE_METATYPE(ImageViewportRoleSnapshot)
Q_DECLARE_METATYPE(ImageViewportDiagnosticsSnapshot)
Q_DECLARE_METATYPE(ImageViewportRevisionsSnapshot)
Q_DECLARE_METATYPE(ImageViewportStateSnapshot)
Q_DECLARE_METATYPE(ImageViewportCommandResult)
Q_DECLARE_METATYPE(ImageViewportCoordinateInput)
Q_DECLARE_METATYPE(ImageViewportCoordinateResult)
Q_DECLARE_METATYPE(PresentationTargetTransitionPolicy)
