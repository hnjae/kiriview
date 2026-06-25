#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QRectF>
#include <QtCore/QVariantMap>
#include <QtGui/QColor>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>

class ImageSequence : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use ImageSequenceFactory to create sequence handles")

private:
    explicit ImageSequence(QObject *parent = nullptr);
    friend class ImageSequenceFactory;
};

class ImageFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ImageFrame objects are created by C++ helpers or provider adapters")

public:
    explicit ImageFrame(QObject *parent = nullptr);
};

class TimedImageFrame : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TimedImageFrame objects are created by C++ helpers or provider adapters")

public:
    explicit TimedImageFrame(QObject *parent = nullptr);
};

class ImageSequenceProviderAdapter : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use a concrete provider adapter supplied by C++ or module helpers")

public:
    explicit ImageSequenceProviderAdapter(QObject *parent = nullptr);
};

class ImageSequenceFactory : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit ImageSequenceFactory(QObject *parent = nullptr);

    Q_INVOKABLE ImageSequence *fromImage(ImageFrame *frame);
    Q_INVOKABLE ImageSequence *fromFrames(const QList<TimedImageFrame *> &frames);
    Q_INVOKABLE ImageSequence *fromProvider(ImageSequenceProviderAdapter *adapter);
};

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(ImageSequence *sequence READ sequence WRITE setSequence NOTIFY sequenceChanged)
    Q_PROPERTY(bool frameCountKnown READ frameCountKnown NOTIFY sequenceInfoChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY sequenceInfoChanged)
    Q_PROPERTY(bool durationKnown READ durationKnown NOTIFY sequenceInfoChanged)
    Q_PROPERTY(double duration READ duration NOTIFY sequenceInfoChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY sequenceInfoChanged)
    Q_PROPERTY(bool canSeekByFrame READ canSeekByFrame NOTIFY sequenceInfoChanged)
    Q_PROPERTY(bool canSeekByPosition READ canSeekByPosition NOTIFY sequenceInfoChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY sequenceInfoChanged)
    Q_PROPERTY(int requestedFrame READ requestedFrame WRITE setRequestedFrame NOTIFY requestedFrameChanged)
    Q_PROPERTY(int displayedFrame READ displayedFrame NOTIFY displayedStateChanged)
    Q_PROPERTY(double requestedPosition READ requestedPosition NOTIFY requestedPositionChanged)
    Q_PROPERTY(double displayedPosition READ displayedPosition NOTIFY displayedStateChanged)
    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(LoopMode loopMode READ loopMode WRITE setLoopMode NOTIFY loopModeChanged)
    Q_PROPERTY(int loopCount READ loopCount WRITE setLoopCount NOTIFY loopCountChanged)
    Q_PROPERTY(int completedLoops READ completedLoops NOTIFY completedLoopsChanged)
    Q_PROPERTY(RequestStatus requestStatus READ requestStatus NOTIFY requestStatusChanged)
    Q_PROPERTY(RequestStatusReason requestStatusReason READ requestStatusReason NOTIFY requestStatusChanged)
    Q_PROPERTY(DisplayStatus displayStatus READ displayStatus NOTIFY displayedStateChanged)
    Q_PROPERTY(bool hasDisplayableFrame READ hasDisplayableFrame NOTIFY displayedStateChanged)
    Q_PROPERTY(bool displayedBelongsToCurrentSequence READ displayedBelongsToCurrentSequence NOTIFY displayedStateChanged)
    Q_PROPERTY(int displayRevision READ displayRevision NOTIFY displayRevisionChanged)
    Q_PROPERTY(QString displayedSnapshotToken READ displayedSnapshotToken NOTIFY displayedStateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString warningString READ warningString NOTIFY diagnosticsChanged)
    Q_PROPERTY(RetentionPolicy retentionPolicy READ retentionPolicy WRITE setRetentionPolicy NOTIFY retentionPolicyChanged)
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY presentationChanged)
    Q_PROPERTY(HorizontalAlignment horizontalAlignment READ horizontalAlignment WRITE setHorizontalAlignment NOTIFY presentationChanged)
    Q_PROPERTY(VerticalAlignment verticalAlignment READ verticalAlignment WRITE setVerticalAlignment NOTIFY presentationChanged)
    Q_PROPERTY(QRectF contentRect READ contentRect NOTIFY geometryStateChanged)
    Q_PROPERTY(QRectF visibleImageRect READ visibleImageRect NOTIFY geometryStateChanged)
    Q_PROPERTY(double paintedWidth READ paintedWidth NOTIFY geometryStateChanged)
    Q_PROPERTY(double paintedHeight READ paintedHeight NOTIFY geometryStateChanged)
    Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY presentationChanged)
    Q_PROPERTY(QPointF pan READ pan WRITE setPan NOTIFY presentationChanged)
    Q_PROPERTY(bool smooth READ smooth WRITE setSmooth NOTIFY presentationChanged)
    Q_PROPERTY(bool mipmap READ mipmap WRITE setMipmap NOTIFY presentationChanged)
    Q_PROPERTY(bool mirror READ mirror WRITE setMirror NOTIFY presentationChanged)
    Q_PROPERTY(bool mirrorVertically READ mirrorVertically WRITE setMirrorVertically NOTIFY presentationChanged)
    Q_PROPERTY(OrientationPolicy orientationPolicy READ orientationPolicy WRITE setOrientationPolicy NOTIFY presentationChanged)
    Q_PROPERTY(BackgroundMode backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY presentationChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY presentationChanged)
    Q_PROPERTY(ColorPolicy colorPolicy READ colorPolicy WRITE setColorPolicy NOTIFY presentationChanged)

public:
    enum PlaybackState {
        Stopped,
        Playing,
        Paused,
        Ended,
    };
    Q_ENUM(PlaybackState)

    enum RequestStatus {
        NoRequest,
        RequestLoading,
        RequestReady,
        RequestUnsupported,
        RequestError,
    };
    Q_ENUM(RequestStatus)

    enum RequestStatusReason {
        None,
        ProviderWaiting,
        RequestQueued,
        RenderDeferred,
        UploadPending,
        UnsupportedRequest,
        UnsupportedPolicy,
        InvalidRequest,
        CpuPreparationFailed,
        TextureUploadFailed,
        ProviderFailure,
    };
    Q_ENUM(RequestStatusReason)

    enum DisplayStatus {
        Empty,
        Current,
        RetainedPrevious,
    };
    Q_ENUM(DisplayStatus)

    enum LoopMode {
        SourceLoop,
        FiniteLoop,
        InfiniteLoop,
    };
    Q_ENUM(LoopMode)

    enum class RequestOutcome {
        OutcomeAccepted,
        OutcomeUnsupported,
        OutcomeInvalid,
    };
    Q_ENUM(RequestOutcome)

    enum FillMode {
        Stretch,
        PreserveAspectFit,
        PreserveAspectCrop,
        Pad,
    };
    Q_ENUM(FillMode)

    enum HorizontalAlignment {
        AlignLeft,
        AlignHCenter,
        AlignRight,
    };
    Q_ENUM(HorizontalAlignment)

    enum VerticalAlignment {
        AlignTop,
        AlignVCenter,
        AlignBottom,
    };
    Q_ENUM(VerticalAlignment)

    enum OrientationPolicy {
        ApplyOrientationBestEffort,
        PreserveOrientationMetadata,
        RequireOrientationApplied,
    };
    Q_ENUM(OrientationPolicy)

    enum class ColorPolicy {
        AssumeSrgbColor,
        PreserveColorMetadata,
    };
    Q_ENUM(ColorPolicy)

    enum BackgroundMode {
        Transparent,
        SolidColor,
        Checkerboard,
    };
    Q_ENUM(BackgroundMode)

    enum RetentionPolicy {
        ClearOnReplacement,
        RetainUntilReady,
        RetainThroughFailure,
    };
    Q_ENUM(RetentionPolicy)

    explicit ImageViewport(QQuickItem *parent = nullptr);

    ImageSequence *sequence() const;
    void setSequence(ImageSequence *sequence);

    bool frameCountKnown() const;
    int frameCount() const;
    bool durationKnown() const;
    double duration() const;
    bool canSeek() const;
    bool canSeekByFrame() const;
    bool canSeekByPosition() const;
    bool streaming() const;

    int requestedFrame() const;
    void setRequestedFrame(int frame);
    int displayedFrame() const;
    double requestedPosition() const;
    double displayedPosition() const;
    PlaybackState playbackState() const;
    double speed() const;
    void setSpeed(double speed);
    LoopMode loopMode() const;
    void setLoopMode(LoopMode mode);
    int loopCount() const;
    void setLoopCount(int count);
    int completedLoops() const;

    RequestStatus requestStatus() const;
    RequestStatusReason requestStatusReason() const;
    DisplayStatus displayStatus() const;
    bool hasDisplayableFrame() const;
    bool displayedBelongsToCurrentSequence() const;
    int displayRevision() const;
    QString displayedSnapshotToken() const;
    QString errorString() const;
    QString warningString() const;
    RetentionPolicy retentionPolicy() const;
    void setRetentionPolicy(RetentionPolicy policy);

    FillMode fillMode() const;
    void setFillMode(FillMode mode);
    HorizontalAlignment horizontalAlignment() const;
    void setHorizontalAlignment(HorizontalAlignment alignment);
    VerticalAlignment verticalAlignment() const;
    void setVerticalAlignment(VerticalAlignment alignment);
    QRectF contentRect() const;
    QRectF visibleImageRect() const;
    double paintedWidth() const;
    double paintedHeight() const;

    double zoom() const;
    void setZoom(double zoom);
    QPointF pan() const;
    void setPan(const QPointF &pan);
    bool smooth() const;
    void setSmooth(bool smooth);
    bool mipmap() const;
    void setMipmap(bool mipmap);
    bool mirror() const;
    void setMirror(bool mirror);
    bool mirrorVertically() const;
    void setMirrorVertically(bool mirror);
    OrientationPolicy orientationPolicy() const;
    void setOrientationPolicy(OrientationPolicy policy);
    BackgroundMode backgroundMode() const;
    void setBackgroundMode(BackgroundMode mode);
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);
    ColorPolicy colorPolicy() const;
    void setColorPolicy(ColorPolicy policy);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE RequestOutcome seek(int frame);
    Q_INVOKABLE RequestOutcome seekToPosition(double milliseconds);
    Q_INVOKABLE void clear();

    Q_INVOKABLE QVariantMap mapItemToImage(const QPointF &point) const;
    Q_INVOKABLE QVariantMap mapImageToItem(const QPointF &point) const;
    Q_INVOKABLE bool containsVisibleImagePoint(const QPointF &point) const;

signals:
    void sequenceChanged();
    void sequenceInfoChanged();
    void requestedFrameChanged();
    void requestedPositionChanged();
    void playbackStateChanged();
    void speedChanged();
    void loopModeChanged();
    void loopCountChanged();
    void completedLoopsChanged();
    void requestStatusChanged();
    void displayedStateChanged();
    void displayRevisionChanged();
    void diagnosticsChanged();
    void retentionPolicyChanged();
    void presentationChanged();
    void geometryStateChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    void notifyPresentationChanged(bool affectsGeometry);
    void setUnsupportedRequest(RequestStatusReason reason);
    void setWarningString(const QString &warning);

    QPointer<ImageSequence> m_sequence;
    int m_requestedFrame = -1;
    PlaybackState m_playbackState = Stopped;
    double m_speed = 1.0;
    LoopMode m_loopMode = SourceLoop;
    int m_loopCount = 1;
    RequestStatus m_requestStatus = NoRequest;
    RequestStatusReason m_requestStatusReason = None;
    RetentionPolicy m_retentionPolicy = RetainThroughFailure;
    FillMode m_fillMode = PreserveAspectFit;
    HorizontalAlignment m_horizontalAlignment = AlignHCenter;
    VerticalAlignment m_verticalAlignment = AlignVCenter;
    double m_zoom = 1.0;
    QPointF m_pan;
    bool m_smooth = true;
    bool m_mipmap = false;
    bool m_mirror = false;
    bool m_mirrorVertically = false;
    OrientationPolicy m_orientationPolicy = ApplyOrientationBestEffort;
    BackgroundMode m_backgroundMode = Transparent;
    QColor m_backgroundColor = Qt::transparent;
    ColorPolicy m_colorPolicy = ColorPolicy::AssumeSrgbColor;
    QString m_warningString;
};
