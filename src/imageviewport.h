#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
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
    explicit ImageSequence(QSizeF logicalSize, QVector<int> frameDurations,
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
    QVector<int> m_frameDurations;
    std::shared_ptr<const TimingIntervals> m_timingIntervals;
    QVector<QImage> m_frameImages;
    std::shared_ptr<ImageSequenceProviderSessionFactory> m_providerSessionFactory;
    ImageSequenceProviderKnownFacts m_providerKnownFacts;
    bool m_hasProviderKnownMetadata = false;
    bool m_hasCompleteProviderKnownMetadata = false;
    QSizeF m_providerKnownLogicalSize;
    int m_providerKnownFrameCount = -1;
    QVector<int> m_providerKnownFrameDurations;
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
    // Transfer results. The viewport releases the handle exactly once after accepting or dropping it.
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

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    QML_ELEMENT
    Q_PROPERTY(ImageSequence* sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
    Q_PROPERTY(RequestStatus requestStatus READ requestStatus NOTIFY requestStateChanged)
    Q_PROPERTY(RequestReason requestReason READ requestReason NOTIFY requestStateChanged)
    Q_PROPERTY(CommandReason commandReason READ commandReason NOTIFY commandStateChanged)
    Q_PROPERTY(DisplayStatus displayStatus READ displayStatus NOTIFY displayStateChanged)
    Q_PROPERTY(PlaybackPhase playbackPhase READ playbackPhase NOTIFY playbackPhaseChanged)
    Q_PROPERTY(int displayedFrame READ displayedFrame NOTIFY displayStateChanged)
    Q_PROPERTY(int requestedFrame READ requestedFrame NOTIFY requestStateChanged)
    Q_PROPERTY(int displayedPosition READ displayedPosition NOTIFY displayStateChanged)
    Q_PROPERTY(int requestedPosition READ requestedPosition NOTIFY requestStateChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY requestStateChanged)
    Q_PROPERTY(int totalDuration READ totalDuration NOTIFY requestStateChanged)
    Q_PROPERTY(QVariantMap frameSeekBounds READ frameSeekBounds NOTIFY requestStateChanged)
    Q_PROPERTY(QVariantMap positionSeekBounds READ positionSeekBounds NOTIFY requestStateChanged)
    Q_PROPERTY(TriState timedPlaybackSupport READ timedPlaybackSupport NOTIFY requestStateChanged)
    Q_PROPERTY(TriState frameSeekSupport READ frameSeekSupport NOTIFY requestStateChanged)
    Q_PROPERTY(TriState positionSeekSupport READ positionSeekSupport NOTIFY requestStateChanged)
    Q_PROPERTY(QSizeF displayedImageSize READ displayedImageSize NOTIFY displayStateChanged)
    Q_PROPERTY(QRectF contentRect READ contentRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF visibleImageRect READ visibleImageRect NOTIFY geometryStateChanged)
    Q_PROPERTY(uint displayRevision READ displayRevision NOTIFY displayRevisionChanged)
    Q_PROPERTY(uint requestRevision READ requestRevision NOTIFY requestRevisionChanged)
    Q_PROPERTY(uint commandRevision READ commandRevision NOTIFY commandRevisionChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString warningString READ warningString NOTIFY diagnosticsChanged)
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

    RequestStatus requestStatus() const;
    RequestReason requestReason() const;
    CommandReason commandReason() const;
    DisplayStatus displayStatus() const;
    PlaybackPhase playbackPhase() const;
    int displayedFrame() const;
    int requestedFrame() const;
    int displayedPosition() const;
    int requestedPosition() const;
    int frameCount() const;
    int totalDuration() const;
    QVariantMap frameSeekBounds() const;
    QVariantMap positionSeekBounds() const;
    TriState timedPlaybackSupport() const;
    TriState frameSeekSupport() const;
    TriState positionSeekSupport() const;
    QSizeF displayedImageSize() const;
    QRectF contentRect() const;
    QRectF visibleImageRect() const;
    uint displayRevision() const;
    uint requestRevision() const;
    uint commandRevision() const;
    QString errorString() const;
    QString warningString() const;

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
    Q_INVOKABLE ImageViewport::CommandOutcome pause();
    Q_INVOKABLE ImageViewport::CommandOutcome stop();
    Q_INVOKABLE ImageViewport::CommandOutcome seek(int frame);
    Q_INVOKABLE ImageViewport::CommandOutcome seekToPosition(int milliseconds);
    Q_INVOKABLE ImageViewport::CommandOutcome resetView();
    Q_INVOKABLE QVariantMap itemToImage(double x, double y) const;
    Q_INVOKABLE QVariantMap imageToItem(double x, double y) const;
    Q_INVOKABLE bool containsVisibleImagePoint(double x, double y) const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
    void setNextProviderRequestTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
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
