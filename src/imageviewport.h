#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>

#include <functional>
#include <memory>
#include <optional>

class ImageSequenceProviderSessionFactory;
class ImageSequenceProviderMetadata;
class ImageViewportPrivate;
class TimingIntervals;

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

private:
    enum class TimingModel {
        None,
        Still,
        TimedList,
        Provider,
    };

    explicit ImageSequence(QObject* parent = nullptr);
    explicit ImageSequence(QSizeF logicalSize, QImage stillImage, QObject* parent = nullptr);
    explicit ImageSequence(QSizeF logicalSize, const QVector<int>& frameDurations,
        QVector<QImage> frameImages, QObject* parent = nullptr);
    explicit ImageSequence(
        std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
        ImageSequenceProviderKnownFacts providerKnownFacts,
        ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
        ImageSequenceProviderCapabilitySupport frameSeekCapability,
        ImageSequenceProviderCapabilitySupport positionSeekCapability,
        ImageSequenceProviderThreadingContract providerThreadingContract,
        QObject* parent = nullptr);

    bool isValid() const;
    bool isStill() const;
    bool isTimedList() const;
    bool isProvider() const;
    QSizeF logicalSize() const;
    int frameCount() const;
    int totalDuration() const;
    int frameStartPosition(int frame) const;
    int frameIndexForPosition(int position) const;
    QImage frameImage(int frame) const;

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
public:
    QImage frameImageForTest(int frame) const;

private:
#endif

    TimingModel m_timingModel = TimingModel::None;
    QSizeF m_logicalSize;
    QImage m_stillImage;
    std::shared_ptr<const TimingIntervals> m_timingIntervals;
    QVector<QImage> m_frameImages;
    std::shared_ptr<ImageSequenceProviderSessionFactory> m_providerSessionFactory;
    ImageSequenceProviderKnownFacts m_providerKnownFacts;
    bool m_hasProviderKnownMetadata = false;
    bool m_hasCompleteProviderKnownMetadata = false;
    QSizeF m_providerKnownLogicalSize;
    int m_providerKnownFrameCount = -1;
    std::shared_ptr<const TimingIntervals> m_providerKnownTimingIntervals;
    ImageSequenceProviderCapabilitySupport m_providerTimedPlaybackCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport m_providerFrameSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport m_providerPositionSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderThreadingContract m_providerThreadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;

    friend class ImageSequenceFactory;
    friend class ImageViewport;
    friend class ImageViewportPrivate;
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
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    ImageFrame(const QImage& image, qsizetype payloadByteSizeForTest, QObject* parent = nullptr);
#endif

    bool isValid() const;
    QSizeF logicalSize() const;
    qint64 payloadByteSize() const;
    bool hasAlphaChannel() const;
    OrientationPolicy orientationPolicy() const;

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    QImage imageForTest() const;
#endif

private:
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
    friend class ImageViewportPrivate;
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
    QVector<QImage> m_frameImages;
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
    enum class TimingModel {
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
    void setTimedPlaybackSupport(bool supported);
    void setFrameSeekSupport(bool supported);
    void setPositionSeekSupport(bool supported);
    bool timedPlaybackSupport() const;
    bool frameSeekSupport() const;
    bool positionSeekSupport() const;

private:
    TimingModel m_timingModel = TimingModel::Invalid;
    QSizeF m_logicalSize;
    QVector<int> m_frameDurations;
    std::optional<bool> m_timedPlaybackSupport;
    std::optional<bool> m_frameSeekSupport;
    std::optional<bool> m_positionSeekSupport;
};

class ImageSequenceProviderFrameMetadata
{
public:
    enum class TimingModel {
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
    TimingModel m_timingModel = TimingModel::Invalid;
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
    Q_PROPERTY(uint value READ value CONSTANT)

public:
    RevisionToken() = default;
    explicit RevisionToken(uint value)
        : m_value(value)
    {
    }

    bool isValid() const { return m_value != 0; }
    uint value() const { return m_value; }

private:
    uint m_value = 0;
};

class PageSetTransitionPolicy
{
    Q_GADGET
    QML_VALUE_TYPE(pageSetTransitionPolicy)
    Q_PROPERTY(DisplayTransition displayTransition READ displayTransition CONSTANT)
    Q_PROPERTY(ZoomTransition zoomTransition READ zoomTransition CONSTANT)
    Q_PROPERTY(
        ContentPositionTransition contentPositionTransition READ contentPositionTransition CONSTANT)
    Q_PROPERTY(RotationTransition rotationTransition READ rotationTransition CONSTANT)
    Q_PROPERTY(MirrorTransition mirrorTransition READ mirrorTransition CONSTANT)
    Q_PROPERTY(ReplacementIntent replacementIntent READ replacementIntent CONSTANT)

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

    enum class ReplacementIntent {
        NewTarget,
        SameTargetRefinement,
    };
    Q_ENUM(ReplacementIntent)

    PageSetTransitionPolicy() = default;

    DisplayTransition displayTransition() const { return m_displayTransition; }
    ZoomTransition zoomTransition() const { return m_zoomTransition; }
    ContentPositionTransition contentPositionTransition() const
    {
        return m_contentPositionTransition;
    }
    RotationTransition rotationTransition() const { return m_rotationTransition; }
    MirrorTransition mirrorTransition() const { return m_mirrorTransition; }
    ReplacementIntent replacementIntent() const { return m_replacementIntent; }

private:
    DisplayTransition m_displayTransition = DisplayTransition::RetainPrevious;
    ZoomTransition m_zoomTransition = ZoomTransition::Preserve;
    ContentPositionTransition m_contentPositionTransition = ContentPositionTransition::Clamp;
    RotationTransition m_rotationTransition = RotationTransition::Preserve;
    MirrorTransition m_mirrorTransition = MirrorTransition::Preserve;
    ReplacementIntent m_replacementIntent = ReplacementIntent::NewTarget;
};

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_ELEMENT
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
    Q_PROPERTY(QVariantMap frameSeekBounds READ frameSeekBounds NOTIFY requestStateChanged)
    Q_PROPERTY(QVariantMap positionSeekBounds READ positionSeekBounds NOTIFY requestStateChanged)
    Q_PROPERTY(int primaryFrameCount READ primaryFrameCount NOTIFY requestStateChanged)
    Q_PROPERTY(int secondaryFrameCount READ secondaryFrameCount NOTIFY requestStateChanged)
    Q_PROPERTY(int primaryTotalDuration READ primaryTotalDuration NOTIFY requestStateChanged)
    Q_PROPERTY(int secondaryTotalDuration READ secondaryTotalDuration NOTIFY requestStateChanged)
    Q_PROPERTY(
        QVariantMap primaryFrameSeekBounds READ primaryFrameSeekBounds NOTIFY requestStateChanged)
    Q_PROPERTY(QVariantMap secondaryFrameSeekBounds READ secondaryFrameSeekBounds NOTIFY
            requestStateChanged)
    Q_PROPERTY(QVariantMap primaryPositionSeekBounds READ primaryPositionSeekBounds NOTIFY
            requestStateChanged)
    Q_PROPERTY(QVariantMap secondaryPositionSeekBounds READ secondaryPositionSeekBounds NOTIFY
            requestStateChanged)
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
    Q_PROPERTY(QSizeF contentSize READ contentSize NOTIFY geometryStateChanged)
    Q_PROPERTY(QPointF contentPosition READ contentPosition NOTIFY geometryStateChanged)
    Q_PROPERTY(
        QPointF maximumContentPosition READ maximumContentPosition NOTIFY geometryStateChanged)
    Q_PROPERTY(bool horizontalPannable READ horizontalPannable NOTIFY geometryStateChanged)
    Q_PROPERTY(bool verticalPannable READ verticalPannable NOTIFY geometryStateChanged)
    Q_PROPERTY(uint displayRevision READ displayRevision NOTIFY displayRevisionChanged)
    Q_PROPERTY(uint requestRevision READ requestRevision NOTIFY requestRevisionChanged)
    Q_PROPERTY(uint commandRevision READ commandRevision NOTIFY commandRevisionChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString warningString READ warningString NOTIFY diagnosticsChanged)
    Q_PROPERTY(FitMode fitMode READ fitMode WRITE setFitModeProperty NOTIFY presentationChanged)
    Q_PROPERTY(
        double zoomPercent READ zoomPercent WRITE setZoomPercentProperty NOTIFY presentationChanged)
    Q_PROPERTY(int rotationDegrees READ rotationDegrees NOTIFY presentationChanged)
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY presentationChanged)
    Q_PROPERTY(HorizontalAlignment horizontalAlignment READ horizontalAlignment WRITE
            setHorizontalAlignment NOTIFY presentationChanged)
    Q_PROPERTY(VerticalAlignment verticalAlignment READ verticalAlignment WRITE setVerticalAlignment
            NOTIFY presentationChanged)
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
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY presentationChanged)
    Q_PROPERTY(QPointF pan READ pan WRITE setPan NOTIFY presentationChanged)
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

    enum class CommandOutcome {
        Accepted,
        Invalid,
        Unsupported,
        IgnoredNoRequest,
    };
    Q_ENUM(CommandOutcome)

    enum class FillMode {
        Contain,
        Cover,
        Stretch,
        Center,
    };
    Q_ENUM(FillMode)

    enum class HorizontalAlignment {
        AlignLeft,
        AlignHCenter,
        AlignRight,
    };
    Q_ENUM(HorizontalAlignment)

    enum class VerticalAlignment {
        AlignTop,
        AlignVCenter,
        AlignBottom,
    };
    Q_ENUM(VerticalAlignment)

    enum class BackgroundMode {
        Transparent,
        SolidColor,
        Checkerboard,
    };
    Q_ENUM(BackgroundMode)

    explicit ImageViewport(QQuickItem* parent = nullptr);
    ~ImageViewport() override;

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
    QVariantMap frameSeekBounds() const;
    QVariantMap positionSeekBounds() const;
    int primaryFrameCount() const;
    int secondaryFrameCount() const;
    int primaryTotalDuration() const;
    int secondaryTotalDuration() const;
    QVariantMap primaryFrameSeekBounds() const;
    QVariantMap secondaryFrameSeekBounds() const;
    QVariantMap primaryPositionSeekBounds() const;
    QVariantMap secondaryPositionSeekBounds() const;
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
    QSizeF contentSize() const;
    QPointF contentPosition() const;
    QPointF maximumContentPosition() const;
    bool horizontalPannable() const;
    bool verticalPannable() const;
    uint displayRevision() const;
    uint requestRevision() const;
    uint commandRevision() const;
    QString errorString() const;
    QString warningString() const;

    FitMode fitMode() const;
    void setFitModeProperty(FitMode mode);
    double zoomPercent() const;
    void setZoomPercentProperty(double percent);
    int rotationDegrees() const;
    FillMode fillMode() const;
    void setFillMode(FillMode mode);
    HorizontalAlignment horizontalAlignment() const;
    void setHorizontalAlignment(HorizontalAlignment alignment);
    VerticalAlignment verticalAlignment() const;
    void setVerticalAlignment(VerticalAlignment alignment);
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
    double zoom() const;
    void setZoom(double zoom);
    QPointF pan() const;
    void setPan(QPointF pan);
    bool looping() const;
    void setLooping(bool looping);

    Q_INVOKABLE ImageViewport::CommandOutcome clear();
    Q_INVOKABLE ImageViewport::CommandOutcome play();
    Q_INVOKABLE ImageViewport::CommandOutcome play(PageRole role);
    Q_INVOKABLE ImageViewport::CommandOutcome pause();
    Q_INVOKABLE ImageViewport::CommandOutcome pause(PageRole role);
    Q_INVOKABLE ImageViewport::CommandOutcome stop();
    Q_INVOKABLE ImageViewport::CommandOutcome stop(PageRole role);
    Q_INVOKABLE ImageViewport::CommandOutcome seek(int frame);
    Q_INVOKABLE ImageViewport::CommandOutcome seek(PageRole role, int frame);
    Q_INVOKABLE ImageViewport::CommandOutcome seekToPosition(int milliseconds);
    Q_INVOKABLE ImageViewport::CommandOutcome seekToPosition(PageRole role, int milliseconds);
    Q_INVOKABLE ImageViewport::CommandOutcome setPageSet(
        const QVariant& primary, const QVariant& secondary, const QVariant& policy = {});
    Q_INVOKABLE ImageViewport::CommandOutcome setSpreadDirection(SpreadDirection direction);
    Q_INVOKABLE ImageViewport::CommandOutcome setPageGap(double gap);
    Q_INVOKABLE ImageViewport::CommandOutcome setFitMode(FitMode mode, QPointF anchor);
    Q_INVOKABLE ImageViewport::CommandOutcome setZoomPercent(double percent, QPointF anchor);
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
    Q_INVOKABLE QVariantMap itemToSpread(double x, double y) const;
    Q_INVOKABLE QVariantMap spreadToItem(double x, double y) const;
    Q_INVOKABLE QVariantMap itemToPage(PageRole role, double x, double y) const;
    Q_INVOKABLE QVariantMap pageToItem(PageRole role, double x, double y) const;
    Q_INVOKABLE bool containsVisibleSpreadPoint(double x, double y) const;
    Q_INVOKABLE bool containsVisiblePagePoint(PageRole role, double x, double y) const;
    Q_INVOKABLE QVariantMap itemToImage(double x, double y) const;
    Q_INVOKABLE QVariantMap imageToItem(double x, double y) const;
    Q_INVOKABLE bool containsVisibleImagePoint(double x, double y) const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
    void setNextProviderRequestTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    void acknowledgeRenderCommitForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
#endif

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

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    friend class ImageViewportPrivate;

    std::unique_ptr<ImageViewportPrivate> d;
};

Q_DECLARE_METATYPE(ImageSequenceProviderRequestToken)
Q_DECLARE_METATYPE(ImageSequenceProviderMetadata)
Q_DECLARE_METATYPE(ImageSequenceProviderFrameMetadata)
Q_DECLARE_METATYPE(ImageViewportRange)
Q_DECLARE_METATYPE(CoordinateResult)
Q_DECLARE_METATYPE(RevisionToken)
Q_DECLARE_METATYPE(PageSetTransitionPolicy)
