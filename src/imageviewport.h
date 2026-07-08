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

class ImageSequenceProviderSessionFactory;
class ImageSequenceProviderMetadata;
class ImageViewportPrivate;
class ImageViewportCommandResult;
class ImageViewportCoordinateInput;
class ImageViewportCoordinateResult;
class ImageViewportDiagnosticsSnapshot;
class ImageViewportDisplaySnapshot;
class ImageViewportPresentationSnapshot;
class ImageViewportRequestSnapshot;
class ImageViewportRevisionsSnapshot;
class ImageViewportRoleDisplaySnapshot;
class ImageViewportRoleGeometrySnapshot;
class ImageViewportRoleMetadataSnapshot;
class ImageViewportRoleRequestSnapshot;
class ImageViewportRoleSnapshot;
class ImageViewportStateSnapshot;
class PageGeometry;
class PageSetTransitionPolicy;
class TimingIntervals;

namespace ImageViewportInternal {
class ImageFramePrivateAccess;
class ImageSequenceData;
class ImageSequencePrivateAccess;
}

enum class ImageSequenceProviderThreadingContract {
    AffinityBound,
    ThreadSafe,
};

enum class ImageSequenceProviderCapabilitySupport {
    Unavailable,
    DeclaredFalse,
    DeclaredTrue,
    KnownFalse,
    KnownTrue,
};

class ImageSequenceAuthoredAnimationFacts
{
    Q_GADGET
    QML_VALUE_TYPE(imageSequenceAuthoredAnimationFacts)
    Q_PROPERTY(bool autoplay READ autoplay CONSTANT)
    Q_PROPERTY(bool progressiveAnimationReadiness READ progressiveAnimationReadiness CONSTANT)
    Q_PROPERTY(LoopMode loopMode READ loopMode CONSTANT)
    Q_PROPERTY(int loopCount READ loopCount CONSTANT)

public:
    enum class LoopMode {
        PlayOnce,
        Finite,
        Infinite,
    };
    Q_ENUM(LoopMode)

    ImageSequenceAuthoredAnimationFacts() = default;
    static ImageSequenceAuthoredAnimationFacts finiteLoop(int loopCount);
    static ImageSequenceAuthoredAnimationFacts infiniteLoop();

    bool autoplay() const;
    void setAutoplay(bool autoplay);
    bool progressiveAnimationReadiness() const;
    void setProgressiveAnimationReadiness(bool progressiveAnimationReadiness);
    LoopMode loopMode() const;
    int loopCount() const;
    bool setFiniteLoopCount(int loopCount);

private:
    bool m_autoplay = false;
    bool m_progressiveAnimationReadiness = false;
    LoopMode m_loopMode = LoopMode::PlayOnce;
    int m_loopCount = 1;
};

class ImageSequenceProviderKnownFacts
{
public:
    enum class Kind {
        Unknown,
        LogicalSize,
        Still,
        TimedFrameCount,
        TimedFrameList,
    };

    ImageSequenceProviderKnownFacts() = default;
    static ImageSequenceProviderKnownFacts logicalSize(QSizeF logicalSize);
    static ImageSequenceProviderKnownFacts still(QSizeF logicalSize);
    static ImageSequenceProviderKnownFacts timedFrameCount(QSizeF logicalSize, int frameCount);
    static ImageSequenceProviderKnownFacts fixedDurationFrames(
        QSizeF logicalSize, int frameCount, int frameDuration);
    static ImageSequenceProviderKnownFacts timedFrameList(
        QSizeF logicalSize, QVector<int> frameDurations);

    bool isSpecified() const;
    bool isValid() const;
    bool isComplete() const;
    bool isLogicalSizeOnly() const;
    bool isStill() const;
    bool isTimedFrameCount() const;
    bool isTimedFrameList() const;
    QSizeF logicalSize() const;
    int frameCount() const;
    QVector<int> frameDurations() const;

private:
    Kind m_kind = Kind::Unknown;
    QSizeF m_logicalSize;
    int m_frameCount = -1;
    QVector<int> m_frameDurations;
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
    Q_PROPERTY(QSizeF logicalSize READ logicalSize CONSTANT)
    Q_PROPERTY(qint64 payloadByteSize READ payloadByteSize CONSTANT)
    Q_PROPERTY(bool hasAlphaChannel READ hasAlphaChannel CONSTANT)
    Q_PROPERTY(OrientationPolicy orientationPolicy READ orientationPolicy CONSTANT)

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

    bool isValid() const;
    QSizeF logicalSize() const;
    qint64 payloadByteSize() const;
    bool hasAlphaChannel() const;
    OrientationPolicy orientationPolicy() const;

private:
    ImageFrame(const QImage& image, qsizetype payloadByteSizeOverride, QObject* parent = nullptr);
    const QImage& imagePayload() const;

    QImage m_image;
    QSizeF m_logicalSize;
    qint64 m_payloadByteSize = 0;
    bool m_hasAlphaChannel = false;
    OrientationPolicy m_orientationPolicy = OrientationPolicy::Identity;

    friend class ImageSequenceFactory;
    friend class TimedImageFrameList;
    friend class FramePreparation;
    friend class ImageViewport;
    friend class ImageViewportInternal::ImageFramePrivateAccess;
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
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString warningString READ warningString NOTIFY diagnosticsChanged)

public:
    explicit TimedImageFrameList(QObject* parent = nullptr);

    int count() const;
    QString errorString() const;
    QString warningString() const;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts() const;
    void setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    bool appendFrame(const QImage& image, int durationMilliseconds);
    Q_INVOKABLE bool appendFrame(ImageFrame* frame, int durationMilliseconds);
    Q_INVOKABLE void clear();

signals:
    void countChanged();
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
    ImageSequenceAuthoredAnimationFacts m_authoredAnimationFacts;
    QString m_errorString;
    QString m_warningString;

    friend class ImageSequenceFactory;
};

class ImageSequenceProviderAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use a concrete provider adapter supplied by C++ or module helpers")

public:
    using CapabilitySupport = ImageSequenceProviderCapabilitySupport;

    explicit ImageSequenceProviderAdapter(QObject* parent = nullptr);
    virtual std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const = 0;
    virtual ImageSequenceProviderMetadata knownMetadata() const;
    virtual ImageSequenceProviderKnownFacts knownFacts() const;
    virtual CapabilitySupport timedPlaybackCapability() const;
    virtual CapabilitySupport frameSeekCapability() const;
    virtual CapabilitySupport positionSeekCapability() const;
    virtual ImageSequenceAuthoredAnimationFacts authoredAnimationFacts() const;
    virtual ImageSequenceProviderThreadingContract threadingContract() const;
};

class ImageSequenceProviderRequestToken
{
public:
    ImageSequenceProviderRequestToken() = default;
    explicit ImageSequenceProviderRequestToken(quint64 id);

    quint64 id() const;
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
    quint64 m_id = 0;
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

    bool isSpecified() const;
    bool isValid() const;
    bool isStill() const;
    bool isTimedFrameList() const;
    QSizeF logicalSize() const;
    QVector<int> frameDurations() const;
    bool hasAuthoredAnimationFacts() const;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts() const;
    void setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    void setTimedPlaybackSupport(bool supported);
    void setFrameSeekSupport(bool supported);
    void setPositionSeekSupport(bool supported);
    bool timedPlaybackSupport() const;
    bool frameSeekSupport() const;
    bool positionSeekSupport() const;

private:
    Kind m_kind = Kind::Invalid;
    QSizeF m_logicalSize;
    QVector<int> m_frameDurations;
    bool m_hasAuthoredAnimationFacts = false;
    ImageSequenceAuthoredAnimationFacts m_authoredAnimationFacts;
    std::optional<bool> m_timedPlaybackSupport;
    std::optional<bool> m_frameSeekSupport;
    std::optional<bool> m_positionSeekSupport;
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

    virtual void requestMetadata(ImageSequenceProviderRequestToken token) = 0;
    virtual void requestFrame(ImageSequenceProviderRequestToken token, int frame);
    virtual void requestPosition(
        ImageSequenceProviderRequestToken token, int resolvedFrame, int requestedPosition);
    virtual void requestPlayback(ImageSequenceProviderRequestToken token, int frame, int position);
    virtual void cancelRequest(ImageSequenceProviderRequestToken token);
    virtual void close();

signals:
    void metadataReady(const ImageSequenceProviderRequestToken& token,
        const ImageSequenceProviderMetadata& metadata);
    // Compatibility borrowed-frame results. The provider retains ownership until delivery returns.
    void imageFrameReady(const ImageSequenceProviderRequestToken& token, ImageFrame* frame);
    void imageFrameWithMetadataReady(const ImageSequenceProviderRequestToken& token,
        ImageFrame* frame, const ImageSequenceProviderFrameMetadata& metadata);
    // Transfer results. The viewport releases the handle exactly once after accepting or dropping
    // it.
    void frameHandleReady(
        const ImageSequenceProviderRequestToken& token, ImageSequenceProviderFrameHandle* frame);
    void frameHandleWithMetadataReady(const ImageSequenceProviderRequestToken& token,
        ImageSequenceProviderFrameHandle* frame,
        const ImageSequenceProviderFrameMetadata& metadata);
    void providerWaiting(const ImageSequenceProviderRequestToken& token);
    void providerProgress(const ImageSequenceProviderRequestToken& token, double progress);
    void endOfSequence(const ImageSequenceProviderRequestToken& token);
    void providerFailed(const ImageSequenceProviderRequestToken& token, const QString& diagnostic);
    void providerUnsupportedWithCause(const ImageSequenceProviderRequestToken& token,
        ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic);
    void providerUnsupported(
        const ImageSequenceProviderRequestToken& token, const QString& diagnostic);
    void providerCancelled(
        const ImageSequenceProviderRequestToken& token, const QString& diagnostic);
};

class ImageSequenceProviderSessionFactory
{
public:
    virtual ~ImageSequenceProviderSessionFactory() = default;
    ImageSequenceProviderSessionFactory(const ImageSequenceProviderSessionFactory&) = delete;
    ImageSequenceProviderSessionFactory& operator=(const ImageSequenceProviderSessionFactory&)
        = delete;
    virtual ImageSequenceProviderSession* createSession(QObject* parent) = 0;

protected:
    ImageSequenceProviderSessionFactory() = default;
};

class ImageSequenceFactoryResult : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageSequenceFactoryResult objects are returned by ImageSequenceFactory")
    Q_PROPERTY(ImageSequence* sequence READ sequence CONSTANT)
    Q_PROPERTY(FactoryOutcome outcome READ outcome CONSTANT)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)
    Q_PROPERTY(QString warningString READ warningString CONSTANT)

public:
    enum class FactoryOutcome {
        Created,
        Invalid,
        Unsupported,
        Error,
    };
    Q_ENUM(FactoryOutcome)

    explicit ImageSequenceFactoryResult(ImageSequence* sequence, FactoryOutcome outcome,
        QString errorString = {}, QString warningString = {}, QObject* parent = nullptr);

    ImageSequence* sequence() const;
    FactoryOutcome outcome() const;
    QString errorString() const;
    QString warningString() const;

private:
    friend class ImageSequenceFactory;

    explicit ImageSequenceFactoryResult(std::shared_ptr<ImageSequence> sequence,
        FactoryOutcome outcome, QString errorString = {}, QString warningString = {},
        QObject* parent = nullptr);

    QPointer<ImageSequence> m_sequence;
    std::shared_ptr<ImageSequence> m_sequenceOwner;
    FactoryOutcome m_outcome;
    QString m_errorString;
    QString m_warningString;
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
    Q_PROPERTY(int maximumLogicalWidth READ getMaximumLogicalWidth CONSTANT)
    Q_PROPERTY(int maximumLogicalHeight READ getMaximumLogicalHeight CONSTANT)
    Q_PROPERTY(qint64 maximumPixelsPerFrame READ getMaximumPixelsPerFrame CONSTANT)
    Q_PROPERTY(qint64 maximumPayloadBytesPerFrame READ getMaximumPayloadBytesPerFrame CONSTANT)
    Q_PROPERTY(int maximumTimedListFrameCount READ getMaximumTimedListFrameCount CONSTANT)
    Q_PROPERTY(int maximumFrameDuration READ getMaximumFrameDuration CONSTANT)
    Q_PROPERTY(int maximumTotalSequenceDuration READ getMaximumTotalSequenceDuration CONSTANT)
    Q_PROPERTY(int maximumDiagnosticStringLength READ getMaximumDiagnosticStringLength CONSTANT)

public:
    explicit ImageSequenceLimits(QObject* parent = nullptr);

    int getMaximumLogicalWidth() const;
    int getMaximumLogicalHeight() const;
    qint64 getMaximumPixelsPerFrame() const;
    qint64 getMaximumPayloadBytesPerFrame() const;
    int getMaximumTimedListFrameCount() const;
    int getMaximumFrameDuration() const;
    int getMaximumTotalSequenceDuration() const;
    int getMaximumDiagnosticStringLength() const;

    static int maximumLogicalWidth();
    static int maximumLogicalHeight();
    static qint64 maximumPixelsPerFrame();
    static qint64 maximumPayloadBytesPerFrame();
    static int maximumTimedListFrameCount();
    static int maximumFrameDuration();
    static int maximumTotalSequenceDuration();
    static int maximumDiagnosticStringLength();
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

class CoordinateResult
{
    Q_GADGET
    QML_VALUE_TYPE(coordinateResult)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(double x READ x CONSTANT)
    Q_PROPERTY(double y READ y CONSTANT)

public:
    CoordinateResult() = default;
    CoordinateResult(bool valid, double x, double y)
        : m_valid(valid)
        , m_x(x)
        , m_y(y)
    {
    }

    bool isValid() const { return m_valid; }
    double x() const { return m_x; }
    double y() const { return m_y; }

    friend bool operator==(CoordinateResult lhs, CoordinateResult rhs)
    {
        return lhs.m_valid == rhs.m_valid && lhs.m_x == rhs.m_x && lhs.m_y == rhs.m_y;
    }
    friend bool operator!=(CoordinateResult lhs, CoordinateResult rhs) { return !(lhs == rhs); }

private:
    bool m_valid = false;
    double m_x = 0.0;
    double m_y = 0.0;
};

class RevisionToken
{
    Q_GADGET
    QML_VALUE_TYPE(revisionToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(quint64 value READ value CONSTANT)

public:
    RevisionToken() = default;
    explicit RevisionToken(quint64 value)
        : m_value(value)
    {
    }

    bool isValid() const { return m_value != 0; }
    quint64 value() const { return m_value; }

    friend bool operator==(RevisionToken lhs, RevisionToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }
    friend bool operator!=(RevisionToken lhs, RevisionToken rhs) { return !(lhs == rhs); }

private:
    quint64 m_value = 0;
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
};

class ImageViewportPageSetGenerationToken
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportPageSetGenerationToken)
    Q_PROPERTY(bool valid READ isValid CONSTANT)

public:
    ImageViewportPageSetGenerationToken() = default;

    bool isValid() const { return m_value != 0; }

    friend bool operator==(
        ImageViewportPageSetGenerationToken lhs, ImageViewportPageSetGenerationToken rhs)
    {
        return lhs.m_value == rhs.m_value;
    }
    friend bool operator!=(
        ImageViewportPageSetGenerationToken lhs, ImageViewportPageSetGenerationToken rhs)
    {
        return !(lhs == rhs);
    }

private:
    explicit ImageViewportPageSetGenerationToken(quint64 value)
        : m_value(value)
    {
    }

    quint64 m_value = 0;

    friend ImageViewportPrivate;
    friend class ImageViewportRequestSnapshot;
    friend class ImageViewportDisplaySnapshot;
    friend class ImageViewportRoleRequestSnapshot;
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

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_ELEMENT
    Q_PROPERTY(ImageViewportStateSnapshot state READ state NOTIFY stateChanged)
    Q_PROPERTY(ImageSequence* sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
    Q_PROPERTY(ImageSequence* primarySequence READ primarySequence NOTIFY sequenceChanged)
    Q_PROPERTY(ImageSequence* secondarySequence READ secondarySequence NOTIFY sequenceChanged)
    Q_PROPERTY(SpreadDirection spreadDirection READ spreadDirection WRITE setSpreadDirectionProperty
            NOTIFY presentationChanged)
    Q_PROPERTY(double pageGap READ pageGap WRITE setPageGapProperty NOTIFY presentationChanged)
    Q_PROPERTY(RequestStatus requestStatus READ requestStatus NOTIFY requestStateChanged)
    Q_PROPERTY(RequestReason requestReason READ requestReason NOTIFY requestStateChanged)
    Q_PROPERTY(CommandReason commandReason READ commandReason NOTIFY commandStateChanged)
    Q_PROPERTY(DisplayStatus displayStatus READ displayStatus NOTIFY displayStateChanged)
    Q_PROPERTY(PlaybackPhase playbackPhase READ playbackPhase NOTIFY playbackPhaseChanged)
    Q_PROPERTY(int displayedFrame READ displayedFrame NOTIFY displayStateChanged)
    Q_PROPERTY(int requestedFrame READ requestedFrame NOTIFY requestStateChanged)
    Q_PROPERTY(int primaryDisplayedFrame READ primaryDisplayedFrame NOTIFY displayStateChanged)
    Q_PROPERTY(int primaryRequestedFrame READ primaryRequestedFrame NOTIFY requestStateChanged)
    Q_PROPERTY(int secondaryDisplayedFrame READ secondaryDisplayedFrame NOTIFY displayStateChanged)
    Q_PROPERTY(int secondaryRequestedFrame READ secondaryRequestedFrame NOTIFY requestStateChanged)
    Q_PROPERTY(int displayedPosition READ displayedPosition NOTIFY displayStateChanged)
    Q_PROPERTY(int requestedPosition READ requestedPosition NOTIFY requestStateChanged)
    Q_PROPERTY(
        int primaryDisplayedPosition READ primaryDisplayedPosition NOTIFY displayStateChanged)
    Q_PROPERTY(
        int primaryRequestedPosition READ primaryRequestedPosition NOTIFY requestStateChanged)
    Q_PROPERTY(
        int secondaryDisplayedPosition READ secondaryDisplayedPosition NOTIFY displayStateChanged)
    Q_PROPERTY(
        int secondaryRequestedPosition READ secondaryRequestedPosition NOTIFY requestStateChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY requestStateChanged)
    Q_PROPERTY(int totalDuration READ totalDuration NOTIFY requestStateChanged)
    Q_PROPERTY(ImageViewportRange frameSeekBounds READ frameSeekBounds NOTIFY requestStateChanged)
    Q_PROPERTY(
        ImageViewportRange positionSeekBounds READ positionSeekBounds NOTIFY requestStateChanged)
    Q_PROPERTY(int primaryFrameCount READ primaryFrameCount NOTIFY requestStateChanged)
    Q_PROPERTY(int secondaryFrameCount READ secondaryFrameCount NOTIFY requestStateChanged)
    Q_PROPERTY(int primaryTotalDuration READ primaryTotalDuration NOTIFY requestStateChanged)
    Q_PROPERTY(int secondaryTotalDuration READ secondaryTotalDuration NOTIFY requestStateChanged)
    Q_PROPERTY(ImageViewportRange primaryFrameSeekBounds READ primaryFrameSeekBounds NOTIFY
            requestStateChanged)
    Q_PROPERTY(ImageViewportRange secondaryFrameSeekBounds READ secondaryFrameSeekBounds NOTIFY
            requestStateChanged)
    Q_PROPERTY(ImageViewportRange primaryPositionSeekBounds READ primaryPositionSeekBounds NOTIFY
            requestStateChanged)
    Q_PROPERTY(ImageViewportRange secondaryPositionSeekBounds READ secondaryPositionSeekBounds
            NOTIFY requestStateChanged)
    Q_PROPERTY(TriState timedPlaybackSupport READ timedPlaybackSupport NOTIFY requestStateChanged)
    Q_PROPERTY(TriState frameSeekSupport READ frameSeekSupport NOTIFY requestStateChanged)
    Q_PROPERTY(TriState positionSeekSupport READ positionSeekSupport NOTIFY requestStateChanged)
    Q_PROPERTY(TriState primaryTimedPlaybackSupport READ primaryTimedPlaybackSupport NOTIFY
            requestStateChanged)
    Q_PROPERTY(TriState secondaryTimedPlaybackSupport READ secondaryTimedPlaybackSupport NOTIFY
            requestStateChanged)
    Q_PROPERTY(
        TriState primaryFrameSeekSupport READ primaryFrameSeekSupport NOTIFY requestStateChanged)
    Q_PROPERTY(TriState secondaryFrameSeekSupport READ secondaryFrameSeekSupport NOTIFY
            requestStateChanged)
    Q_PROPERTY(TriState primaryPositionSeekSupport READ primaryPositionSeekSupport NOTIFY
            requestStateChanged)
    Q_PROPERTY(TriState secondaryPositionSeekSupport READ secondaryPositionSeekSupport NOTIFY
            requestStateChanged)
    Q_PROPERTY(QSizeF displayedImageSize READ displayedImageSize NOTIFY displayStateChanged)
    Q_PROPERTY(QSizeF displayedSpreadSize READ displayedSpreadSize NOTIFY displayStateChanged)
    Q_PROPERTY(
        QSizeF primaryDisplayedImageSize READ primaryDisplayedImageSize NOTIFY displayStateChanged)
    Q_PROPERTY(QSizeF secondaryDisplayedImageSize READ secondaryDisplayedImageSize NOTIFY
            displayStateChanged)
    Q_PROPERTY(QRectF contentRect READ contentRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF visibleImageRect READ visibleImageRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF visibleSpreadRect READ visibleSpreadRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF primaryPageRect READ primaryPageRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF secondaryPageRect READ secondaryPageRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF primaryItemRect READ primaryItemRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF secondaryItemRect READ secondaryItemRect NOTIFY geometryStateChanged)
    Q_PROPERTY(
        QRectF visiblePrimaryPageRect READ visiblePrimaryPageRect NOTIFY geometryStateChanged)
    Q_PROPERTY(
        QRectF visibleSecondaryPageRect READ visibleSecondaryPageRect NOTIFY geometryStateChanged)
    Q_PROPERTY(
        PageGeometry primaryPageGeometry READ primaryPageGeometry NOTIFY geometryStateChanged)
    Q_PROPERTY(
        PageGeometry secondaryPageGeometry READ secondaryPageGeometry NOTIFY geometryStateChanged)
    Q_PROPERTY(QSizeF contentSize READ contentSize NOTIFY geometryStateChanged)
    Q_PROPERTY(QPointF contentPosition READ contentPosition NOTIFY geometryStateChanged)
    Q_PROPERTY(
        QPointF maximumContentPosition READ maximumContentPosition NOTIFY geometryStateChanged)
    Q_PROPERTY(bool horizontalPannable READ horizontalPannable NOTIFY geometryStateChanged)
    Q_PROPERTY(bool verticalPannable READ verticalPannable NOTIFY geometryStateChanged)
    Q_PROPERTY(RevisionToken displayRevision READ displayRevision NOTIFY displayRevisionChanged)
    Q_PROPERTY(RevisionToken requestRevision READ requestRevision NOTIFY requestRevisionChanged)
    Q_PROPERTY(RevisionToken commandRevision READ commandRevision NOTIFY commandRevisionChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString warningString READ warningString NOTIFY diagnosticsChanged)
    Q_PROPERTY(FitMode fitMode READ fitMode WRITE setFitModeProperty NOTIFY presentationChanged)
    Q_PROPERTY(
        double zoomPercent READ zoomPercent WRITE setZoomPercentProperty NOTIFY presentationChanged)
    Q_PROPERTY(double minimumManualZoomPercent READ minimumManualZoomPercent CONSTANT)
    Q_PROPERTY(
        double maximumManualZoomPercent READ maximumManualZoomPercent NOTIFY geometryStateChanged)
    Q_PROPERTY(double manualZoomStepFactor READ manualZoomStepFactor CONSTANT)
    Q_PROPERTY(int rotationDegrees READ rotationDegrees NOTIFY presentationChanged)
    Q_PROPERTY(bool smoothing READ smoothing WRITE setSmoothing NOTIFY presentationChanged)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap NOTIFY presentationChanged)
    Q_PROPERTY(bool mirrorHorizontally READ mirrorHorizontally WRITE setMirrorHorizontally NOTIFY
            presentationChanged)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically WRITE setMirrorVertically NOTIFY
            presentationChanged)
    Q_PROPERTY(BackgroundMode backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY
            presentationChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY
            presentationChanged)
    Q_PROPERTY(bool looping READ looping WRITE setLooping NOTIFY loopingChanged)

public:
    enum class PageRole {
        Primary,
        Secondary,
    };
    Q_ENUM(PageRole)

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
        Placeholder,
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

    enum class TriState {
        Unavailable,
        False,
        True,
    };
    Q_ENUM(TriState)

    enum class CapabilitySupport {
        Unavailable,
        False,
        True,
    };
    Q_ENUM(CapabilitySupport)

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

    enum class QualityPreference {
        Default,
        FastFirstDisplay,
        BalancedDetail,
        ExactDetail,
    };
    Q_ENUM(QualityPreference)

    enum class ExactnessPreference {
        Default,
        AllowInexact,
        PreferExact,
        RequireExact,
    };
    Q_ENUM(ExactnessPreference)

    enum class PayloadQuality {
        Unknown,
        Preview,
        FirstDisplay,
        BoundedDetail,
        Exact,
    };
    Q_ENUM(PayloadQuality)

    enum class PayloadExactness {
        Unknown,
        NotExact,
        ExactForSource,
    };
    Q_ENUM(PayloadExactness)

    enum class CoordinateSpace {
        Item,
        Spread,
        Page,
    };
    Q_ENUM(CoordinateSpace)

    explicit ImageViewport(QQuickItem* parent = nullptr);
    ~ImageViewport() override;

    ImageViewportStateSnapshot state() const;
    ImageSequence* sequence() const;
    void setSequence(ImageSequence* sequence);
    ImageSequence* primarySequence() const;
    ImageSequence* secondarySequence() const;
    SpreadDirection spreadDirection() const;
    void setSpreadDirectionProperty(SpreadDirection direction);
    double pageGap() const;
    void setPageGapProperty(double gap);

    RequestStatus requestStatus() const;
    RequestReason requestReason() const;
    CommandReason commandReason() const;
    DisplayStatus displayStatus() const;
    PlaybackPhase playbackPhase() const;
    int displayedFrame() const;
    int requestedFrame() const;
    int primaryDisplayedFrame() const;
    int primaryRequestedFrame() const;
    int secondaryDisplayedFrame() const;
    int secondaryRequestedFrame() const;
    int displayedPosition() const;
    int requestedPosition() const;
    int primaryDisplayedPosition() const;
    int primaryRequestedPosition() const;
    int secondaryDisplayedPosition() const;
    int secondaryRequestedPosition() const;
    int frameCount() const;
    int totalDuration() const;
    ImageViewportRange frameSeekBounds() const;
    ImageViewportRange positionSeekBounds() const;
    int primaryFrameCount() const;
    int secondaryFrameCount() const;
    int primaryTotalDuration() const;
    int secondaryTotalDuration() const;
    ImageViewportRange primaryFrameSeekBounds() const;
    ImageViewportRange secondaryFrameSeekBounds() const;
    ImageViewportRange primaryPositionSeekBounds() const;
    ImageViewportRange secondaryPositionSeekBounds() const;
    TriState timedPlaybackSupport() const;
    TriState frameSeekSupport() const;
    TriState positionSeekSupport() const;
    TriState primaryTimedPlaybackSupport() const;
    TriState secondaryTimedPlaybackSupport() const;
    TriState primaryFrameSeekSupport() const;
    TriState secondaryFrameSeekSupport() const;
    TriState primaryPositionSeekSupport() const;
    TriState secondaryPositionSeekSupport() const;
    QSizeF displayedImageSize() const;
    QSizeF displayedSpreadSize() const;
    QSizeF primaryDisplayedImageSize() const;
    QSizeF secondaryDisplayedImageSize() const;
    QRectF contentRect() const;
    QRectF visibleImageRect() const;
    QRectF visibleSpreadRect() const;
    QRectF primaryPageRect() const;
    QRectF secondaryPageRect() const;
    QRectF primaryItemRect() const;
    QRectF secondaryItemRect() const;
    QRectF visiblePrimaryPageRect() const;
    QRectF visibleSecondaryPageRect() const;
    PageGeometry primaryPageGeometry() const;
    PageGeometry secondaryPageGeometry() const;
    QSizeF contentSize() const;
    QPointF contentPosition() const;
    QPointF maximumContentPosition() const;
    bool horizontalPannable() const;
    bool verticalPannable() const;
    RevisionToken displayRevision() const;
    RevisionToken requestRevision() const;
    RevisionToken commandRevision() const;
    QString errorString() const;
    QString warningString() const;

    FitMode fitMode() const;
    void setFitModeProperty(FitMode mode);
    double zoomPercent() const;
    void setZoomPercentProperty(double percent);
    double minimumManualZoomPercent() const;
    double maximumManualZoomPercent() const;
    double manualZoomStepFactor() const;
    int rotationDegrees() const;
    bool smoothing() const;
    void setSmoothing(bool smoothing);
    bool mipmap() const;
    void setMipmap(bool mipmap);
    bool mirrorHorizontally() const;
    void setMirrorHorizontally(bool mirror);
    bool mirrorVertically() const;
    void setMirrorVertically(bool mirror);
    BackgroundMode backgroundMode() const;
    void setBackgroundMode(BackgroundMode mode);
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& color);
    bool looping() const;
    void setLooping(bool looping);

    Q_INVOKABLE ImageViewport::CommandOutcome clear();
    Q_INVOKABLE ImageViewport::CommandOutcome play();
    Q_INVOKABLE ImageViewport::CommandOutcome play(ImageViewport::PageRole role);
    Q_INVOKABLE ImageViewport::CommandOutcome pause();
    Q_INVOKABLE ImageViewport::CommandOutcome pause(ImageViewport::PageRole role);
    Q_INVOKABLE ImageViewport::CommandOutcome stop();
    Q_INVOKABLE ImageViewport::CommandOutcome stop(ImageViewport::PageRole role);
    Q_INVOKABLE ImageViewport::CommandOutcome seek(int frame);
    Q_INVOKABLE ImageViewport::CommandOutcome seek(ImageViewport::PageRole role, int frame);
    Q_INVOKABLE ImageViewport::CommandOutcome seekToPosition(int milliseconds);
    Q_INVOKABLE ImageViewport::CommandOutcome seekToPosition(
        ImageViewport::PageRole role, int milliseconds);
    Q_INVOKABLE ImageViewport::CommandOutcome setPageSet(
        const QVariant& primary, const QVariant& secondary);
    Q_INVOKABLE ImageViewport::CommandOutcome setPageSet(
        const QVariant& primary, const QVariant& secondary, PageSetTransitionPolicy policy);
    ImageViewport::CommandOutcome setPageSet(ImageSequence* primary, ImageSequence* secondary);
    ImageViewport::CommandOutcome setPageSet(
        ImageSequence* primary, ImageSequence* secondary, PageSetTransitionPolicy policy);
    Q_INVOKABLE ImageViewport::CommandOutcome setSpreadDirection(
        ImageViewport::SpreadDirection direction);
    Q_INVOKABLE ImageViewport::CommandOutcome setPageGap(double gap);
    Q_INVOKABLE ImageViewport::CommandOutcome setFitMode(
        ImageViewport::FitMode mode, QPointF anchor);
    Q_INVOKABLE ImageViewport::CommandOutcome setZoomPercent(double percent, QPointF anchor);
    Q_INVOKABLE ImageViewport::CommandOutcome zoomByStep(int stepCount, QPointF anchor);
    Q_INVOKABLE double clampedManualZoomPercent(double percent) const;
    Q_INVOKABLE double steppedManualZoomPercent(int stepCount) const;
    Q_INVOKABLE ImageViewport::CommandOutcome panBy(QPointF delta);
    Q_INVOKABLE ImageViewport::CommandOutcome panToStart();
    Q_INVOKABLE ImageViewport::CommandOutcome panToEnd();
    Q_INVOKABLE ImageViewport::CommandOutcome scanNext();
    Q_INVOKABLE ImageViewport::CommandOutcome scanPrevious();
    Q_INVOKABLE ImageViewport::CommandOutcome rotateClockwise(QPointF anchor);
    Q_INVOKABLE ImageViewport::CommandOutcome rotateCounterClockwise(QPointF anchor);
    Q_INVOKABLE ImageViewport::CommandOutcome setMirrorHorizontally(bool enabled, QPointF anchor);
    Q_INVOKABLE ImageViewport::CommandOutcome setMirrorVertically(bool enabled, QPointF anchor);
    Q_INVOKABLE ImageViewport::CommandOutcome resetView();
    Q_INVOKABLE CoordinateResult itemToSpread(double x, double y) const;
    Q_INVOKABLE CoordinateResult spreadToItem(double x, double y) const;
    Q_INVOKABLE CoordinateResult nearestVisibleSpreadPoint(double x, double y) const;
    Q_INVOKABLE CoordinateResult itemToPage(ImageViewport::PageRole role, double x, double y) const;
    Q_INVOKABLE CoordinateResult pageToItem(ImageViewport::PageRole role, double x, double y) const;
    Q_INVOKABLE CoordinateResult nearestVisiblePagePoint(
        ImageViewport::PageRole role, double x, double y) const;
    Q_INVOKABLE PageGeometry pageGeometry(ImageViewport::PageRole role) const;
    Q_INVOKABLE bool containsVisibleSpreadPoint(double x, double y) const;
    Q_INVOKABLE bool containsVisiblePagePoint(
        ImageViewport::PageRole role, double x, double y) const;
    Q_INVOKABLE CoordinateResult itemToImage(double x, double y) const;
    Q_INVOKABLE CoordinateResult imageToItem(double x, double y) const;
    Q_INVOKABLE CoordinateResult nearestVisibleImagePoint(double x, double y) const;
    Q_INVOKABLE bool containsVisibleImagePoint(double x, double y) const;

signals:
    void sequenceChanged();
    void requestStateChanged();
    void commandStateChanged();
    void displayStateChanged();
    void playbackPhaseChanged();
    void displayRevisionChanged();
    void requestRevisionChanged();
    void commandRevisionChanged();
    void diagnosticsChanged();
    void presentationChanged();
    void geometryStateChanged();
    void loopingChanged();
    void stateChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    friend ImageViewportPrivate;

    std::unique_ptr<ImageViewportPrivate> d;
};

class PageGeometry
{
    Q_GADGET
    QML_VALUE_TYPE(pageGeometry)
    Q_PROPERTY(ImageViewport::PageRole role READ role CONSTANT)
    Q_PROPERTY(QRectF pageRect READ pageRect CONSTANT)
    Q_PROPERTY(QRectF itemRect READ itemRect CONSTANT)
    Q_PROPERTY(QRectF visiblePageRect READ visiblePageRect CONSTANT)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)

public:
    PageGeometry() = default;
    PageGeometry(ImageViewport::PageRole role, QRectF pageRect, QRectF itemRect,
        QRectF visiblePageRect, bool available)
        : m_role(role)
        , m_pageRect(pageRect)
        , m_itemRect(itemRect)
        , m_visiblePageRect(visiblePageRect)
        , m_available(available)
    {
    }

    ImageViewport::PageRole role() const { return m_role; }
    QRectF pageRect() const { return m_pageRect; }
    QRectF itemRect() const { return m_itemRect; }
    QRectF visiblePageRect() const { return m_visiblePageRect; }
    bool isAvailable() const { return m_available; }

    friend bool operator==(const PageGeometry& lhs, const PageGeometry& rhs)
    {
        return lhs.m_role == rhs.m_role && lhs.m_pageRect == rhs.m_pageRect
            && lhs.m_itemRect == rhs.m_itemRect && lhs.m_visiblePageRect == rhs.m_visiblePageRect
            && lhs.m_available == rhs.m_available;
    }
    friend bool operator!=(const PageGeometry& lhs, const PageGeometry& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewport::PageRole m_role = ImageViewport::PageRole::Primary;
    QRectF m_pageRect;
    QRectF m_itemRect;
    QRectF m_visiblePageRect;
    bool m_available = false;
};

class PageSetTransitionPolicy
{
    Q_GADGET
    QML_VALUE_TYPE(pageSetTransitionPolicy)
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

    PageSetTransitionPolicy() = default;

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

    friend bool operator==(PageSetTransitionPolicy lhs, PageSetTransitionPolicy rhs)
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
    friend bool operator!=(PageSetTransitionPolicy lhs, PageSetTransitionPolicy rhs)
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
    Q_PROPERTY(ImageViewportPageSetGenerationToken acceptedPageSetGeneration READ
            acceptedPageSetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet acceptedRoleSet READ acceptedRoleSet CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet targetRoleSet READ targetRoleSet CONSTANT)
    Q_PROPERTY(QVariant activeRole READ activeRole CONSTANT)
    Q_PROPERTY(QVariant playbackRole READ playbackRole CONSTANT)

public:
    ImageViewportRequestSnapshot() = default;
    ImageViewportRequestSnapshot(ImageViewport::RequestStatus status,
        ImageViewport::RequestReason reason, ImageViewport::PlaybackPhase playbackPhase,
        ImageViewportPageSetGenerationToken acceptedPageSetGeneration,
        ImageViewportRoleSet acceptedRoleSet, ImageViewportRoleSet targetRoleSet,
        QVariant activeRole, QVariant playbackRole)
        : m_status(status)
        , m_reason(reason)
        , m_playbackPhase(playbackPhase)
        , m_acceptedPageSetGeneration(acceptedPageSetGeneration)
        , m_acceptedRoleSet(acceptedRoleSet)
        , m_targetRoleSet(targetRoleSet)
        , m_activeRole(std::move(activeRole))
        , m_playbackRole(std::move(playbackRole))
    {
    }

    ImageViewport::RequestStatus status() const { return m_status; }
    ImageViewport::RequestReason reason() const { return m_reason; }
    ImageViewport::PlaybackPhase playbackPhase() const { return m_playbackPhase; }
    ImageViewportPageSetGenerationToken acceptedPageSetGeneration() const
    {
        return m_acceptedPageSetGeneration;
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
            && lhs.m_acceptedPageSetGeneration == rhs.m_acceptedPageSetGeneration
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
    ImageViewportPageSetGenerationToken m_acceptedPageSetGeneration;
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
    Q_PROPERTY(ImageViewportPageSetGenerationToken displayedPageSetGeneration READ
            displayedPageSetGeneration CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet displayedRoleSet READ displayedRoleSet CONSTANT)
    Q_PROPERTY(ImageViewportRoleSet targetRoleSet READ targetRoleSet CONSTANT)
    Q_PROPERTY(bool belongsToAcceptedPageSet READ belongsToAcceptedPageSet CONSTANT)
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
        ImageViewportPageSetGenerationToken displayedPageSetGeneration,
        ImageViewportRoleSet displayedRoleSet, ImageViewportRoleSet targetRoleSet,
        bool belongsToAcceptedPageSet, bool retained,
        ImageViewportRevisionToken displayedPresentationRevision,
        ImageViewportRevisionToken targetPresentationRevision, QSizeF spreadSize,
        QRectF contentRect, QSizeF contentSize, QPointF contentPosition,
        QPointF maximumContentPosition, QRectF visibleSpreadRect, bool horizontalPannable,
        bool verticalPannable)
        : m_status(status)
        , m_phase(phase)
        , m_displayedPageSetGeneration(displayedPageSetGeneration)
        , m_displayedRoleSet(displayedRoleSet)
        , m_targetRoleSet(targetRoleSet)
        , m_belongsToAcceptedPageSet(belongsToAcceptedPageSet)
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
    ImageViewportPageSetGenerationToken displayedPageSetGeneration() const
    {
        return m_displayedPageSetGeneration;
    }
    ImageViewportRoleSet displayedRoleSet() const { return m_displayedRoleSet; }
    ImageViewportRoleSet targetRoleSet() const { return m_targetRoleSet; }
    bool belongsToAcceptedPageSet() const { return m_belongsToAcceptedPageSet; }
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
            && lhs.m_displayedPageSetGeneration == rhs.m_displayedPageSetGeneration
            && lhs.m_displayedRoleSet == rhs.m_displayedRoleSet
            && lhs.m_targetRoleSet == rhs.m_targetRoleSet
            && lhs.m_belongsToAcceptedPageSet == rhs.m_belongsToAcceptedPageSet
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
    ImageViewportPageSetGenerationToken m_displayedPageSetGeneration;
    ImageViewportRoleSet m_displayedRoleSet;
    ImageViewportRoleSet m_targetRoleSet;
    bool m_belongsToAcceptedPageSet = false;
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
    Q_PROPERTY(ImageViewport::QualityPreference qualityPreference READ qualityPreference CONSTANT)
    Q_PROPERTY(
        ImageViewport::ExactnessPreference exactnessPreference READ exactnessPreference CONSTANT)

public:
    ImageViewportPresentationSnapshot() = default;
    ImageViewportPresentationSnapshot(ImageViewport::FitMode fitMode, double zoomPercent,
        double minimumManualZoomPercent, double maximumManualZoomPercent,
        double manualZoomStepFactor, int rotationDegrees, bool mirrorHorizontally,
        bool mirrorVertically, ImageViewport::SpreadDirection spreadDirection, double pageGap,
        ImageViewport::BackgroundMode backgroundMode, QColor backgroundColor, bool smoothing,
        bool mipmap, bool looping, ImageViewport::QualityPreference qualityPreference,
        ImageViewport::ExactnessPreference exactnessPreference)
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
    ImageViewport::QualityPreference qualityPreference() const { return m_qualityPreference; }
    ImageViewport::ExactnessPreference exactnessPreference() const { return m_exactnessPreference; }

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
    ImageViewport::QualityPreference m_qualityPreference
        = ImageViewport::QualityPreference::Default;
    ImageViewport::ExactnessPreference m_exactnessPreference
        = ImageViewport::ExactnessPreference::Default;
};

class ImageViewportRoleRequestSnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleRequestSnapshot)
    Q_PROPERTY(bool belongsToAcceptedPageSet READ belongsToAcceptedPageSet CONSTANT)
    Q_PROPERTY(
        ImageViewportPageSetGenerationToken pageSetGeneration READ pageSetGeneration CONSTANT)
    Q_PROPERTY(ImageViewport::PageRole role READ role CONSTANT)
    Q_PROPERTY(int frame READ frame CONSTANT)
    Q_PROPERTY(int position READ position CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(ImageViewportDemandRevisionToken demandRevision READ demandRevision CONSTANT)

public:
    ImageViewportRoleRequestSnapshot() = default;
    ImageViewportRoleRequestSnapshot(bool belongsToAcceptedPageSet,
        ImageViewportPageSetGenerationToken pageSetGeneration, ImageViewport::PageRole role,
        int frame, int position, QSizeF sourceLogicalSize,
        ImageViewportDemandRevisionToken demandRevision)
        : m_belongsToAcceptedPageSet(belongsToAcceptedPageSet)
        , m_pageSetGeneration(pageSetGeneration)
        , m_role(role)
        , m_frame(frame)
        , m_position(position)
        , m_sourceLogicalSize(sourceLogicalSize)
        , m_demandRevision(demandRevision)
    {
    }

    bool belongsToAcceptedPageSet() const { return m_belongsToAcceptedPageSet; }
    ImageViewportPageSetGenerationToken pageSetGeneration() const { return m_pageSetGeneration; }
    ImageViewport::PageRole role() const { return m_role; }
    int frame() const { return m_frame; }
    int position() const { return m_position; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }

    friend bool operator==(
        const ImageViewportRoleRequestSnapshot& lhs, const ImageViewportRoleRequestSnapshot& rhs)
    {
        return lhs.m_belongsToAcceptedPageSet == rhs.m_belongsToAcceptedPageSet
            && lhs.m_pageSetGeneration == rhs.m_pageSetGeneration && lhs.m_role == rhs.m_role
            && lhs.m_frame == rhs.m_frame && lhs.m_position == rhs.m_position
            && lhs.m_sourceLogicalSize == rhs.m_sourceLogicalSize
            && lhs.m_demandRevision == rhs.m_demandRevision;
    }
    friend bool operator!=(
        const ImageViewportRoleRequestSnapshot& lhs, const ImageViewportRoleRequestSnapshot& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_belongsToAcceptedPageSet = false;
    ImageViewportPageSetGenerationToken m_pageSetGeneration;
    ImageViewport::PageRole m_role = ImageViewport::PageRole::Primary;
    int m_frame = -1;
    int m_position = -1;
    QSizeF m_sourceLogicalSize;
    ImageViewportDemandRevisionToken m_demandRevision;
};

class ImageViewportRoleDisplaySnapshot
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportRoleDisplaySnapshot)
    Q_PROPERTY(bool belongsToAcceptedPageSet READ belongsToAcceptedPageSet CONSTANT)
    Q_PROPERTY(bool retained READ retained CONSTANT)
    Q_PROPERTY(int frame READ frame CONSTANT)
    Q_PROPERTY(int position READ position CONSTANT)
    Q_PROPERTY(QSizeF sourceLogicalSize READ sourceLogicalSize CONSTANT)
    Q_PROPERTY(QSizeF payloadRasterSize READ payloadRasterSize CONSTANT)
    Q_PROPERTY(QSizeF sourceToPayloadScale READ sourceToPayloadScale CONSTANT)
    Q_PROPERTY(ImageViewport::PayloadQuality quality READ quality CONSTANT)
    Q_PROPERTY(ImageViewport::PayloadExactness exactness READ exactness CONSTANT)
    Q_PROPERTY(bool currentForDemand READ currentForDemand CONSTANT)
    Q_PROPERTY(ImageViewportDemandRevisionToken demandRevision READ demandRevision CONSTANT)

public:
    ImageViewportRoleDisplaySnapshot() = default;
    ImageViewportRoleDisplaySnapshot(bool belongsToAcceptedPageSet, bool retained, int frame,
        int position, QSizeF sourceLogicalSize, QSizeF payloadRasterSize,
        QSizeF sourceToPayloadScale, ImageViewport::PayloadQuality quality,
        ImageViewport::PayloadExactness exactness, bool currentForDemand,
        ImageViewportDemandRevisionToken demandRevision)
        : m_belongsToAcceptedPageSet(belongsToAcceptedPageSet)
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

    bool belongsToAcceptedPageSet() const { return m_belongsToAcceptedPageSet; }
    bool retained() const { return m_retained; }
    int frame() const { return m_frame; }
    int position() const { return m_position; }
    QSizeF sourceLogicalSize() const { return m_sourceLogicalSize; }
    QSizeF payloadRasterSize() const { return m_payloadRasterSize; }
    QSizeF sourceToPayloadScale() const { return m_sourceToPayloadScale; }
    ImageViewport::PayloadQuality quality() const { return m_quality; }
    ImageViewport::PayloadExactness exactness() const { return m_exactness; }
    bool currentForDemand() const { return m_currentForDemand; }
    ImageViewportDemandRevisionToken demandRevision() const { return m_demandRevision; }

    friend bool operator==(
        const ImageViewportRoleDisplaySnapshot& lhs, const ImageViewportRoleDisplaySnapshot& rhs)
    {
        return lhs.m_belongsToAcceptedPageSet == rhs.m_belongsToAcceptedPageSet
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
    bool m_belongsToAcceptedPageSet = false;
    bool m_retained = false;
    int m_frame = -1;
    int m_position = -1;
    QSizeF m_sourceLogicalSize;
    QSizeF m_payloadRasterSize;
    QSizeF m_sourceToPayloadScale;
    ImageViewport::PayloadQuality m_quality = ImageViewport::PayloadQuality::Unknown;
    ImageViewport::PayloadExactness m_exactness = ImageViewport::PayloadExactness::Unknown;
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
    Q_PROPERTY(ImageViewport::CapabilitySupport frameSeekSupport READ frameSeekSupport CONSTANT)
    Q_PROPERTY(
        ImageViewport::CapabilitySupport positionSeekSupport READ positionSeekSupport CONSTANT)
    Q_PROPERTY(
        ImageViewport::CapabilitySupport timedPlaybackSupport READ timedPlaybackSupport CONSTANT)
    Q_PROPERTY(bool autoplay READ autoplay CONSTANT)
    Q_PROPERTY(bool progressiveAnimationReadiness READ progressiveAnimationReadiness CONSTANT)
    Q_PROPERTY(ImageSequenceAuthoredAnimationFacts::LoopMode loopMode READ loopMode CONSTANT)
    Q_PROPERTY(int loopCount READ loopCount CONSTANT)

public:
    ImageViewportRoleMetadataSnapshot() = default;
    ImageViewportRoleMetadataSnapshot(bool available, QSizeF sourceLogicalSize, int frameCount,
        int totalDuration, ImageViewportRange frameSeekBounds,
        ImageViewportRange positionSeekBounds, ImageViewport::CapabilitySupport frameSeekSupport,
        ImageViewport::CapabilitySupport positionSeekSupport,
        ImageViewport::CapabilitySupport timedPlaybackSupport, bool autoplay,
        bool progressiveAnimationReadiness, ImageSequenceAuthoredAnimationFacts::LoopMode loopMode,
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
    ImageViewport::CapabilitySupport frameSeekSupport() const { return m_frameSeekSupport; }
    ImageViewport::CapabilitySupport positionSeekSupport() const { return m_positionSeekSupport; }
    ImageViewport::CapabilitySupport timedPlaybackSupport() const { return m_timedPlaybackSupport; }
    bool autoplay() const { return m_autoplay; }
    bool progressiveAnimationReadiness() const { return m_progressiveAnimationReadiness; }
    ImageSequenceAuthoredAnimationFacts::LoopMode loopMode() const { return m_loopMode; }
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
    ImageViewport::CapabilitySupport m_frameSeekSupport
        = ImageViewport::CapabilitySupport::Unavailable;
    ImageViewport::CapabilitySupport m_positionSeekSupport
        = ImageViewport::CapabilitySupport::Unavailable;
    ImageViewport::CapabilitySupport m_timedPlaybackSupport
        = ImageViewport::CapabilitySupport::Unavailable;
    bool m_autoplay = false;
    bool m_progressiveAnimationReadiness = false;
    ImageSequenceAuthoredAnimationFacts::LoopMode m_loopMode
        = ImageSequenceAuthoredAnimationFacts::LoopMode::PlayOnce;
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
    Q_PROPERTY(QVariant pageRole READ pageRole WRITE setPageRole)
    Q_PROPERTY(QPointF point READ point WRITE setPoint)

public:
    ImageViewportCoordinateInput() = default;

    ImageViewport::CoordinateSpace sourceSpace() const { return m_sourceSpace; }
    void setSourceSpace(ImageViewport::CoordinateSpace sourceSpace) { m_sourceSpace = sourceSpace; }
    ImageViewport::CoordinateSpace targetSpace() const { return m_targetSpace; }
    void setTargetSpace(ImageViewport::CoordinateSpace targetSpace) { m_targetSpace = targetSpace; }
    QVariant pageRole() const { return m_pageRole; }
    void setPageRole(QVariant pageRole) { m_pageRole = std::move(pageRole); }
    QPointF point() const { return m_point; }
    void setPoint(QPointF point) { m_point = point; }

    friend bool operator==(
        const ImageViewportCoordinateInput& lhs, const ImageViewportCoordinateInput& rhs)
    {
        return lhs.m_sourceSpace == rhs.m_sourceSpace && lhs.m_targetSpace == rhs.m_targetSpace
            && lhs.m_pageRole == rhs.m_pageRole && lhs.m_point == rhs.m_point;
    }
    friend bool operator!=(
        const ImageViewportCoordinateInput& lhs, const ImageViewportCoordinateInput& rhs)
    {
        return !(lhs == rhs);
    }

private:
    ImageViewport::CoordinateSpace m_sourceSpace = ImageViewport::CoordinateSpace::Item;
    ImageViewport::CoordinateSpace m_targetSpace = ImageViewport::CoordinateSpace::Spread;
    QVariant m_pageRole;
    QPointF m_point;
};

class ImageViewportCoordinateResult
{
    Q_GADGET
    QML_VALUE_TYPE(imageViewportCoordinateResult)
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(QPointF point READ point CONSTANT)
    Q_PROPERTY(ImageViewport::CoordinateSpace sourceSpace READ sourceSpace CONSTANT)
    Q_PROPERTY(ImageViewport::CoordinateSpace targetSpace READ targetSpace CONSTANT)
    Q_PROPERTY(QVariant pageRole READ pageRole CONSTANT)

public:
    ImageViewportCoordinateResult() = default;
    ImageViewportCoordinateResult(bool valid, QPointF point,
        ImageViewport::CoordinateSpace sourceSpace, ImageViewport::CoordinateSpace targetSpace,
        QVariant pageRole = {})
        : m_valid(valid)
        , m_point(point)
        , m_sourceSpace(sourceSpace)
        , m_targetSpace(targetSpace)
        , m_pageRole(std::move(pageRole))
    {
    }

    bool isValid() const { return m_valid; }
    QPointF point() const { return m_point; }
    ImageViewport::CoordinateSpace sourceSpace() const { return m_sourceSpace; }
    ImageViewport::CoordinateSpace targetSpace() const { return m_targetSpace; }
    QVariant pageRole() const { return m_pageRole; }

    friend bool operator==(
        const ImageViewportCoordinateResult& lhs, const ImageViewportCoordinateResult& rhs)
    {
        return lhs.m_valid == rhs.m_valid && lhs.m_point == rhs.m_point
            && lhs.m_sourceSpace == rhs.m_sourceSpace && lhs.m_targetSpace == rhs.m_targetSpace
            && lhs.m_pageRole == rhs.m_pageRole;
    }
    friend bool operator!=(
        const ImageViewportCoordinateResult& lhs, const ImageViewportCoordinateResult& rhs)
    {
        return !(lhs == rhs);
    }

private:
    bool m_valid = false;
    QPointF m_point;
    ImageViewport::CoordinateSpace m_sourceSpace = ImageViewport::CoordinateSpace::Item;
    ImageViewport::CoordinateSpace m_targetSpace = ImageViewport::CoordinateSpace::Spread;
    QVariant m_pageRole;
};

inline QDebug operator<<(QDebug debug, ImageViewportRange range)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "ImageViewportRange(" << range.minimum() << ", " << range.maximum() << ")";
    return debug;
}

inline QDebug operator<<(QDebug debug, CoordinateResult result)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "CoordinateResult(valid=" << result.isValid() << ", x=" << result.x()
                    << ", y=" << result.y() << ")";
    return debug;
}

inline QDebug operator<<(QDebug debug, RevisionToken token)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "RevisionToken(valid=" << token.isValid() << ", value=" << token.value()
                    << ")";
    return debug;
}

inline QDebug operator<<(QDebug debug, const PageGeometry& geometry)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "PageGeometry(role=" << static_cast<int>(geometry.role())
                    << ", available=" << geometry.isAvailable()
                    << ", pageRect=" << geometry.pageRect() << ", itemRect=" << geometry.itemRect()
                    << ", visiblePageRect=" << geometry.visiblePageRect() << ")";
    return debug;
}

Q_DECLARE_METATYPE(ImageSequenceProviderRequestToken)
Q_DECLARE_METATYPE(ImageSequenceAuthoredAnimationFacts)
Q_DECLARE_METATYPE(ImageSequenceProviderMetadata)
Q_DECLARE_METATYPE(ImageSequenceProviderFrameMetadata)
Q_DECLARE_METATYPE(ImageViewportRange)
Q_DECLARE_METATYPE(CoordinateResult)
Q_DECLARE_METATYPE(RevisionToken)
Q_DECLARE_METATYPE(ImageViewportRevisionToken)
Q_DECLARE_METATYPE(ImageViewportPageSetGenerationToken)
Q_DECLARE_METATYPE(ImageViewportDemandRevisionToken)
Q_DECLARE_METATYPE(ImageViewportRoleSet)
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
Q_DECLARE_METATYPE(PageGeometry)
Q_DECLARE_METATYPE(PageSetTransitionPolicy)
