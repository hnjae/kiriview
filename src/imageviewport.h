#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>

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
    };

    explicit ImageSequence(QObject *parent = nullptr);
    explicit ImageSequence(const QSizeF &logicalSize, QObject *parent = nullptr);
    explicit ImageSequence(const QSizeF &logicalSize, QVector<int> frameDurations, QObject *parent = nullptr);

    bool isValid() const;
    bool isStill() const;
    bool isTimedList() const;
    QSizeF logicalSize() const;
    int frameCount() const;
    int totalDuration() const;

    TimingModel m_timingModel = TimingModel::None;
    QSizeF m_logicalSize;
    QVector<int> m_frameDurations;

    friend class ImageSequenceFactory;
    friend class ImageViewport;
};

class ImageFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageFrame objects are created by C++ helpers or provider adapters")

public:
    explicit ImageFrame(QObject *parent = nullptr);
    explicit ImageFrame(const QImage &image, QObject *parent = nullptr);

    bool isValid() const;
    QSizeF logicalSize() const;
    qsizetype payloadByteSize() const;

private:
    QSizeF m_logicalSize;
    qsizetype m_payloadByteSize = 0;
};

class TimedImageFrameList : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString warningString READ warningString NOTIFY diagnosticsChanged)

public:
    explicit TimedImageFrameList(QObject *parent = nullptr);

    int count() const;
    QString errorString() const;
    QString warningString() const;
    Q_INVOKABLE bool appendFrame(ImageFrame *frame, int durationMilliseconds);
    Q_INVOKABLE void clear();

signals:
    void countChanged();
    void diagnosticsChanged();

private:
    bool isValid() const;
    QSizeF logicalSize() const;
    QVector<int> frameDurations() const;
    qsizetype payloadByteSize() const;
    int totalDuration() const;
    void setErrorString(const QString &errorString);

    QSizeF m_logicalSize;
    QVector<int> m_frameDurations;
    qsizetype m_payloadByteSize = 0;
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
    explicit ImageSequenceProviderAdapter(QObject *parent = nullptr);
};

class ImageSequenceFactoryResult : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageSequenceFactoryResult objects are returned by ImageSequenceFactory")
    Q_PROPERTY(ImageSequence *sequence READ sequence CONSTANT)
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

    explicit ImageSequenceFactoryResult(ImageSequence *sequence,
        FactoryOutcome outcome,
        QString errorString = {},
        QString warningString = {},
        QObject *parent = nullptr);

    ImageSequence *sequence() const;
    FactoryOutcome outcome() const;
    QString errorString() const;
    QString warningString() const;

private:
    QPointer<ImageSequence> m_sequence;
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
    explicit ImageSequenceFactory(QObject *parent = nullptr);

    Q_INVOKABLE ImageSequenceFactoryResult *fromFrame(ImageFrame *frame);
    Q_INVOKABLE ImageSequenceFactoryResult *fromTimedFrameList(TimedImageFrameList *list);
    Q_INVOKABLE ImageSequenceFactoryResult *fromProvider(ImageSequenceProviderAdapter *adapter);
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
    explicit ImageSequenceLimits(QObject *parent = nullptr);

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
    Q_PROPERTY(ImageSequence *sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
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
    Q_PROPERTY(HorizontalAlignment horizontalAlignment READ horizontalAlignment WRITE setHorizontalAlignment NOTIFY presentationChanged)
    Q_PROPERTY(VerticalAlignment verticalAlignment READ verticalAlignment WRITE setVerticalAlignment NOTIFY presentationChanged)
    Q_PROPERTY(bool smoothing READ smoothing WRITE setSmoothing NOTIFY presentationChanged)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap NOTIFY presentationChanged)
    Q_PROPERTY(bool mirrorHorizontally READ mirrorHorizontally WRITE setMirrorHorizontally NOTIFY presentationChanged)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically WRITE setMirrorVertically NOTIFY presentationChanged)
    Q_PROPERTY(BackgroundMode backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY presentationChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY presentationChanged)
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

    enum class RequestOutcome {
        Accepted,
        Invalid,
        Unsupported,
        IgnoredNoRequest,
    };
    Q_ENUM(RequestOutcome)

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

    explicit ImageViewport(QQuickItem *parent = nullptr);

    ImageSequence *sequence() const;
    void setSequence(ImageSequence *sequence);

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
    void setBackgroundColor(const QColor &color);
    double zoom() const;
    void setZoom(double zoom);
    QPointF pan() const;
    void setPan(const QPointF &pan);
    bool looping() const;
    void setLooping(bool looping);

    Q_INVOKABLE RequestOutcome clear();
    Q_INVOKABLE RequestOutcome play();
    Q_INVOKABLE RequestOutcome pause();
    Q_INVOKABLE RequestOutcome stop();
    Q_INVOKABLE RequestOutcome seek(int frame);
    Q_INVOKABLE RequestOutcome seekToPosition(int milliseconds);
    Q_INVOKABLE RequestOutcome resetView();
    Q_INVOKABLE QVariantMap itemToImage(double x, double y) const;
    Q_INVOKABLE QVariantMap imageToItem(double x, double y) const;
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

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    static QVariantMap invalidRange();
    static QVariantMap invalidCoordinateResult();

    void notifyPresentationChanged(bool affectsGeometry);
    void incrementDisplayRevision();
    void incrementRequestRevision();
    void setCommandDiagnostic(CommandReason reason);
    void clearCommandDiagnosticForAcceptedCommand();
    RequestOutcome ignoredNoRequest();
    bool hasActiveRequest() const;
    bool hasReadyDisplay() const;
    bool hasDisplayableSequence() const;
    bool hasStillSequence() const;
    bool hasTimedSequence() const;
    QRectF currentContentRect() const;
    QRectF itemBounds() const;
    QSizeF currentImageSize() const;
    void publishSequenceReadyState();
    void publishRenderWaitingState();

    QPointer<ImageSequence> m_sequence;
    RequestStatus m_requestStatus = RequestStatus::NoRequest;
    RequestReason m_requestReason = RequestReason::NoRequest;
    CommandReason m_commandReason = CommandReason::NoCommand;
    DisplayStatus m_displayStatus = DisplayStatus::Empty;
    PlaybackPhase m_playbackPhase = PlaybackPhase::Stopped;
    FillMode m_fillMode = FillMode::Contain;
    HorizontalAlignment m_horizontalAlignment = HorizontalAlignment::AlignHCenter;
    VerticalAlignment m_verticalAlignment = VerticalAlignment::AlignVCenter;
    BackgroundMode m_backgroundMode = BackgroundMode::Transparent;
    QColor m_backgroundColor = Qt::transparent;
    double m_zoom = 1.0;
    QPointF m_pan;
    bool m_smoothing = true;
    bool m_mipmap = false;
    bool m_mirrorHorizontally = false;
    bool m_mirrorVertically = false;
    bool m_looping = false;
    uint m_displayRevision = 0;
    uint m_requestRevision = 0;
    uint m_commandRevision = 0;
    QString m_errorString;
    QString m_warningString;
};
