#pragma once

#include "imageviewporthelpers_p.h"
#include "imageviewportstate_p.h"
#include "renderadapter_p.h"
#include "viewportcontroller_p.h"
#include "viewportproviderbridge_p.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtQuick/QSGNode>

class ImageViewportPrivate
{
public:
    using BackgroundMode = ImageViewport::BackgroundMode;
    using CommandOutcome = ImageViewport::CommandOutcome;
    using CommandReason = ImageViewport::CommandReason;
    using DisplayStatus = ImageViewport::DisplayStatus;
    using FillMode = ImageViewport::FillMode;
    using HorizontalAlignment = ImageViewport::HorizontalAlignment;
    using PlaybackPhase = ImageViewport::PlaybackPhase;
    using RequestReason = ImageViewport::RequestReason;
    using RequestStatus = ImageViewport::RequestStatus;
    using TriState = ImageViewport::TriState;
    using VerticalAlignment = ImageViewport::VerticalAlignment;

    explicit ImageViewportPrivate(ImageViewport *viewport);
    ~ImageViewportPrivate();

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
    uint displayRevision() const;
    uint requestRevision() const;
    uint commandRevision() const;
    QString errorString() const;
    QString warningString() const;

    CommandOutcome clear();
    CommandOutcome play();
    CommandOutcome pause();
    CommandOutcome stop();
    CommandOutcome seek(int frame);
    CommandOutcome seekToPosition(int milliseconds);
    CommandOutcome resetView();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
#endif
    CommandOutcome clearCommandImpl();
    CommandOutcome playCommandImpl();
    CommandOutcome pauseCommandImpl();
    CommandOutcome stopCommandImpl();
    CommandOutcome seekCommandImpl(int frame);
    CommandOutcome seekToPositionCommandImpl(int milliseconds);
    CommandOutcome resetViewCommandImpl();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTestImpl(int elapsedMilliseconds);
#endif

    QRectF contentRect() const;
    QRectF visibleImageRect() const;
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
    QVariantMap itemToImage(double x, double y) const;
    QVariantMap imageToItem(double x, double y) const;
    bool containsVisibleImagePoint(double x, double y) const;
    static QVariantMap invalidRange();
    static QVariantMap invalidCoordinateResult();
    void notifyPresentationChanged(bool affectsGeometry);
    QRectF currentContentRect() const;
    QRectF itemBounds() const;
    QRectF contentRectForItemBounds(const QRectF &bounds) const;
    QRectF visibleImageRectForItemBounds(const QRectF &bounds) const;
    QSizeF currentImageSize() const;

    QSGNode *updatePaintNode(QSGNode *oldNode);
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry, const QRectF &oldContentRect, const QRectF &oldVisibleImageRect);
    void reportRenderFailure();
    void captureRenderFailureRetainedDisplay();
    void clearRenderFailureRetainedDisplay();

    void closeProviderSession();
    bool openProviderSession();
    ImageSequenceProviderRequestToken nextProviderRequestToken();
    void requestProviderMetadata(const ImageSequenceProviderRequestToken &token);
    void requestProviderFrame(const ImageSequenceProviderRequestToken &token, int frame);
    void requestProviderPlayback(const ImageSequenceProviderRequestToken &token, int frame, int position);
    void cancelProviderRequest(const ImageSequenceProviderRequestToken &token);
    void handleProviderMetadataReady(const ImageSequenceProviderRequestToken &token, const ImageSequenceProviderMetadata &metadata);
    void handleProviderFrameReady(const ImageSequenceProviderRequestToken &token, ImageFrame *frame);
    void handleProviderFrameReadyWithMetadata(const ImageSequenceProviderRequestToken &token, ImageFrame *frame, const ImageSequenceProviderFrameMetadata &metadata);
    void handleProviderWaiting(const ImageSequenceProviderRequestToken &token);
    void handleProviderProgress(const ImageSequenceProviderRequestToken &token, double progress);
    void handleProviderEndOfSequence(const ImageSequenceProviderRequestToken &token);
    void handleProviderFailure(const ImageSequenceProviderRequestToken &token, const QString &diagnostic);
    void handleProviderUnsupported(const ImageSequenceProviderRequestToken &token, const QString &diagnostic);
    void handleProviderCancellation(const ImageSequenceProviderRequestToken &token, const QString &diagnostic);
    bool validateProviderStillMetadata(const ImageSequenceProviderMetadata &metadata);
    bool validateProviderTimedMetadata(const ImageSequenceProviderMetadata &metadata);
    bool validateProviderFrame(ImageFrame *frame, const ImageSequenceProviderFrameMetadata &metadata) const;
    int providerFrameStartPosition(int frame) const;
    int providerFrameIndexForPosition(int position) const;
    static QString boundedDiagnostic(const QString &diagnostic, const QString &fallback);
    void publishAcceptedTargetState(const QImage &providerImage = {});
    void publishSequenceReadyState(const QImage &providerImage = {});
    void publishRenderWaitingState();

    void incrementDisplayRevision();
    void incrementRequestRevision();
    void setPlaybackPhase(PlaybackPhase phase);
    void syncPlaybackTimer();
    void stopPlaybackTimer();
    void handlePlaybackTimer();
    int takePlaybackTimerElapsed();
    void flushPlaybackTimerElapsed();
    int playbackTimerInterval() const;
    void advancePlayback(int elapsedMilliseconds);
    void setCommandDiagnostic(CommandReason reason);
    void clearCommandDiagnosticForAcceptedCommand();
    bool clearDiagnostics();
    CommandOutcome ignoredNoRequest();
    bool hasActiveRequest() const;
    bool hasReadyDisplay() const;
    bool hasDisplayableSequence() const;
    bool hasStillSequence() const;
    bool hasTimedSequence() const;
    bool hasProviderSequence() const;
    bool hasGenerationTerminalProviderFailure() const;

    double width() const;
    double height() const;
    QQuickWindow *window() const;
    void update();

    ImageViewport *q = nullptr;
    ViewportController controller;
    ViewportProviderBridge providerBridge;
    RenderAdapter renderAdapter;
    ImageViewportInternal::PresentationState presentation;
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::ProviderGenerationState provider;
    QTimer playbackTimer;
    QElapsedTimer playbackElapsedTimer;

    QPointer<ImageSequence> &m_sequence = request.sequence;
    std::shared_ptr<ImageSequence> &m_sequenceOwner = request.sequenceOwner;
    RequestStatus &m_requestStatus = request.status;
    RequestReason &m_requestReason = request.reason;
    CommandReason &m_commandReason = request.commandReason;
    DisplayStatus &m_displayStatus = display.status;
    PlaybackPhase &m_playbackPhase = request.playbackPhase;
    FillMode &m_fillMode = presentation.fillMode;
    HorizontalAlignment &m_horizontalAlignment = presentation.horizontalAlignment;
    VerticalAlignment &m_verticalAlignment = presentation.verticalAlignment;
    BackgroundMode &m_backgroundMode = presentation.backgroundMode;
    QColor &m_backgroundColor = presentation.backgroundColor;
    double &m_zoom = presentation.zoom;
    QPointF &m_pan = presentation.pan;
    bool &m_smoothing = presentation.smoothing;
    bool &m_mipmap = presentation.mipmap;
    bool &m_mirrorHorizontally = presentation.mirrorHorizontally;
    bool &m_mirrorVertically = presentation.mirrorVertically;
    bool &m_looping = request.looping;
    bool &m_stopPlaybackWhenRequestReady = request.stopPlaybackWhenRequestReady;
    bool &m_providerPlaybackStartPending = request.providerPlaybackStartPending;
    int &m_currentFrame = request.currentFrame;
    int &m_requestedPosition = request.requestedPosition;
    int &m_playbackPosition = request.playbackPosition;
    int &m_latestNonPlaybackFrame = request.latestNonPlaybackFrame;
    int &m_latestNonPlaybackPosition = request.latestNonPlaybackPosition;
    quint64 &m_sequenceGeneration = request.sequenceGeneration;
    int &m_displayedFrame = display.displayedFrame;
    int &m_displayedPosition = display.displayedPosition;
    quint64 &m_displayedGeneration = display.displayedGeneration;
    QSizeF &m_displayedImageSize = display.displayedImageSize;
    QImage &m_displayedImage = display.displayedImage;
    QImage &m_pendingDisplayImage = display.pendingDisplayImage;
    bool &m_renderCommitPending = display.renderCommitPending;
    bool &m_renderFailureRetainedDisplayValid = display.renderFailureRetainedDisplayValid;
    int &m_renderFailureRetainedFrame = display.renderFailureRetainedFrame;
    int &m_renderFailureRetainedPosition = display.renderFailureRetainedPosition;
    quint64 &m_renderFailureRetainedGeneration = display.renderFailureRetainedGeneration;
    QSizeF &m_renderFailureRetainedImageSize = display.renderFailureRetainedImageSize;
    QImage &m_renderFailureRetainedImage = display.renderFailureRetainedImage;
    uint &m_displayRevision = display.revision;
    uint &m_requestRevision = request.requestRevision;
    uint &m_commandRevision = request.commandRevision;
    QString &m_errorString = request.errorString;
    QString &m_warningString = request.warningString;
    QPointer<ImageSequenceProviderSession> &m_providerSession = provider.session;
    quint64 &m_nextProviderRequestToken = provider.nextRequestToken;
    ImageSequenceProviderRequestToken &m_activeProviderMetadataToken = provider.activeMetadataToken;
    ImageSequenceProviderRequestToken &m_activeProviderFrameToken = provider.activeFrameToken;
    bool &m_activeProviderFrameFromPlayback = provider.activeFrameFromPlayback;
    bool &m_providerMetadataReady = provider.metadataReady;
    bool &m_providerTimedMetadata = provider.timedMetadata;
    QSizeF &m_providerLogicalSize = provider.logicalSize;
    QVector<int> &m_providerFrameDurations = provider.frameDurations;
};
