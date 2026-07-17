#pragma once

#include <ImageViewport/imagesequence.h>
#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <functional>
#include <memory>
#include <optional>

namespace ImageViewportInternal {
class ProviderRequestTokenPrivateAccess;
}

class ImageSequenceProviderDescriptor;
class ImageSequenceProviderEvent;
class ImageSequenceProviderRequest;

enum class ImageSequenceProviderThreadingContract {
    AffinityBound,
    ThreadSafe,
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

class ImageSequenceProviderSession : public QObject
{
    Q_OBJECT

public:
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
    static ImageSequenceProviderFrameEnvelope stillFrame();
    static ImageSequenceProviderFrameEnvelope timedFrame(
        int frame, int frameStartPosition, int frameDuration);

    bool isValid() const;
    bool isStillFrame() const;
    bool isTimedFrame() const;
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
    int m_frame = -1;
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
    Q_PROPERTY(ImageViewportAllocationGenerationToken allocationGeneration READ allocationGeneration
            WRITE setAllocationGeneration)
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
    ImageViewportAllocationGenerationToken allocationGeneration() const
    {
        return m_allocationGeneration;
    }
    void setAllocationGeneration(ImageViewportAllocationGenerationToken generation)
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
    ImageViewportAllocationGenerationToken m_allocationGeneration;
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

Q_DECLARE_METATYPE(ImageSequenceProviderRequestToken)
Q_DECLARE_METATYPE(ImageSequenceProviderMetadata)
Q_DECLARE_METATYPE(ImageSequenceProviderRequestKind)
Q_DECLARE_METATYPE(ImageSequenceProviderEventKind)
Q_DECLARE_METATYPE(ImageSequenceProviderUnsupportedCause)
Q_DECLARE_METATYPE(ImageSequenceProviderFrameEnvelope)
Q_DECLARE_METATYPE(ImageSequenceProviderDisplayDemand)
Q_DECLARE_METATYPE(ImageSequenceProviderRequest)
Q_DECLARE_METATYPE(ImageSequenceProviderEvent)
Q_DECLARE_METATYPE(ImageSequenceProviderDescriptor)
