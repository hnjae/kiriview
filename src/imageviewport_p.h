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
    using DisplayRequestSnapshot = ImageViewportInternal::DisplayRequestSnapshot;
    using DisplayRequestOrigin = ImageViewportInternal::DisplayRequestOrigin;
    using ProviderRequestTargetKind = ImageViewportInternal::ProviderRequestTargetKind;

    explicit ImageViewportPrivate(ImageViewport* viewport);
    ~ImageViewportPrivate();

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
    void setNextProviderRequestTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
#endif
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTestImpl(int elapsedMilliseconds);
    void setNextProviderRequestTokenForTestImpl(quint64 token);
    bool hasPendingRenderCommitForTestImpl() const;
    quint64 activeRequestIdForTestImpl() const;
    quint64 displayedRequestIdForTestImpl() const;
    quint64 pendingRenderPayloadIdForTestImpl() const;
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
    void setBackgroundColor(const QColor& color);
    double zoom() const;
    void setZoom(double zoom);
    QPointF pan() const;
    void setPan(QPointF pan);
    bool looping() const;
    void setLooping(bool looping);
    QVariantMap itemToImage(double x, double y) const;
    QVariantMap imageToItem(double x, double y) const;
    bool containsVisibleImagePoint(double x, double y) const;
    static QVariantMap invalidRange();
    static QVariantMap invalidCoordinateResult();
    void applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes);
    void notifyPresentationChanged(bool affectsGeometry);
    QRectF currentContentRect() const;
    QRectF itemBounds() const;
    QRectF contentRectForItemBounds(const QRectF& bounds) const;
    QRectF visibleImageRectForItemBounds(const QRectF& bounds) const;
    QSizeF currentImageSize() const;

    QSGNode* updatePaintNode(QSGNode* oldNode);
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry,
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);
    void captureRenderFailureRetainedDisplay();
    void clearRenderFailureRetainedDisplay();
    void discardPendingRenderCommit();

    void closeProviderSession();
    bool openProviderSession();
    ImageSequenceProviderRequestToken nextProviderRequestToken();
    void publishProviderFrameLoadingState();
    void requestProviderMetadata(ImageSequenceProviderRequestToken token);
    void requestProviderFrame(ImageSequenceProviderRequestToken token, int frame);
    void requestProviderPosition(ImageSequenceProviderRequestToken token, int frame, int position);
    void requestProviderPlayback(ImageSequenceProviderRequestToken token, int frame, int position);
    void cancelProviderRequest(ImageSequenceProviderRequestToken token);
    void clearQueuedProviderFrameRequest();
    void queueProviderFrameRequest(int frame, ProviderRequestTargetKind targetKind);
    void flushQueuedProviderFrameRequest();
    bool startProviderFrameRequest(int frame, ProviderRequestTargetKind targetKind);
    bool dispatchProviderFrameRequest(int frame, ProviderRequestTargetKind targetKind);
    void publishProviderTokenExhaustion();
    void handleProviderMetadataReady(
        ImageSequenceProviderRequestToken token, const ImageSequenceProviderMetadata& metadata);
    void handleProviderFrameReady(ImageSequenceProviderRequestToken token, ImageFrame* frame);
    void handleProviderFrameReadyWithMetadata(ImageSequenceProviderRequestToken token,
        ImageFrame* frame, ImageSequenceProviderFrameMetadata metadata);
    void handleProviderFrameReady(
        ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame);
    void handleProviderFrameReadyWithMetadata(ImageSequenceProviderRequestToken token,
        ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata);
    void handleProviderWaiting(ImageSequenceProviderRequestToken token);
    void handleProviderProgress(ImageSequenceProviderRequestToken token, double progress);
    void handleProviderEndOfSequence(ImageSequenceProviderRequestToken token);
    void handleProviderFailure(ImageSequenceProviderRequestToken token, const QString& diagnostic);
    void handleProviderUnsupported(ImageSequenceProviderRequestToken token,
        ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic);
    void handleProviderCancellation(
        ImageSequenceProviderRequestToken token, const QString& diagnostic);
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory() const;
    int providerFrameStartPosition(int frame) const;
    int providerFrameIndexForPosition(int position) const;
    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
    ImageSequenceProviderThreadingContract providerThreadingContract() const;
    void publishAcceptedTargetState(const QImage& providerImage = {});
    void publishReadyDisplayState();
    void publishSequenceReadyState(const QImage& providerImage = {});
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
    void clearRequestIdentity();
    void beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin origin,
        bool rememberAsLatestNonPlayback);
    void beginInitialDisplayRequest(bool rememberAsLatestNonPlayback);
    DisplayRequestSnapshot activeDisplayRequestSnapshot(int displayedPosition) const;
    void commitDisplayedRequestSnapshot();
    void clearDisplayedDisplay();
    void beginPreparedPayloadIdentity();
    void clearPendingRenderIdentity();
    CommandOutcome ignoredNoRequest();
    bool hasActiveRequest() const;
    bool hasReadyDisplay() const;
    bool hasDisplayableSequence() const;
    bool hasStillSequence() const;
    bool hasTimedSequence() const;
    bool hasProviderSequence() const;
    bool hasGenerationTerminalProviderFailure() const;
    bool providerTimedPlaybackCapabilityKnownFalse() const;
    bool providerFrameSeekCapabilityKnownFalse() const;
    bool providerFrameSeekCapabilityKnownTrue() const;
    bool providerPositionSeekCapabilityKnownFalse() const;
    bool providerKnownFactsTimedFrameCount() const;
    int providerKnownFactsFrameCount() const;
    int sequenceFrameCount() const;
    int sequenceFrameIndexForPosition(int position) const;
    int sequenceFrameStartPosition(int frame) const;

    double width() const;
    double height() const;
    QQuickWindow* window() const;
    void update();

    ImageViewport* q = nullptr;
    ViewportController controller;
    ViewportProviderBridge providerBridge;
    RenderAdapter renderAdapter;
    ImageViewportInternal::PresentationState presentation;
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::ProviderGenerationState provider;
    QTimer playbackTimer;
    QElapsedTimer playbackElapsedTimer;

    QPointer<ImageSequence>& m_sequence = request.sequence;
    std::shared_ptr<ImageSequence>& m_sequenceOwner = request.sequenceOwner;
    RequestStatus& m_requestStatus = request.status;
    RequestReason& m_requestReason = request.reason;
    CommandReason& m_commandReason = request.commandReason;
    DisplayStatus& m_displayStatus = display.status;
    PlaybackPhase& m_playbackPhase = request.playbackPhase;
    FillMode& m_fillMode = presentation.fillMode;
    HorizontalAlignment& m_horizontalAlignment = presentation.horizontalAlignment;
    VerticalAlignment& m_verticalAlignment = presentation.verticalAlignment;
    BackgroundMode& m_backgroundMode = presentation.backgroundMode;
    QColor& m_backgroundColor = presentation.backgroundColor;
    double& m_zoom = presentation.zoom;
    QPointF& m_pan = presentation.pan;
    bool& m_smoothing = presentation.smoothing;
    bool& m_mipmap = presentation.mipmap;
    bool& m_mirrorHorizontally = presentation.mirrorHorizontally;
    bool& m_mirrorVertically = presentation.mirrorVertically;
    bool& m_looping = request.looping;
    bool& m_stopPlaybackWhenRequestReady = request.stopPlaybackWhenRequestReady;
    bool& m_providerPlaybackStartPending = request.providerPlaybackStartPending;
    QSizeF& m_displayedImageSize = display.displayedImageSize;
    QImage& m_displayedImage = display.displayedImage;
    QImage& m_pendingDisplayImage = display.pendingDisplayImage;
    bool& m_renderCommitPending = display.renderCommitPending;
    quint64& m_nextPreparedPayloadId = display.nextPreparedPayloadId;
    quint64& m_pendingRenderRequestId = display.pendingRenderRequestId;
    quint64& m_pendingPreparedPayloadId = display.pendingPreparedPayloadId;
    bool& m_renderFailureRetainedDisplayValid = display.renderFailureRetainedDisplayValid;
    QSizeF& m_renderFailureRetainedImageSize = display.renderFailureRetainedImageSize;
    QImage& m_renderFailureRetainedImage = display.renderFailureRetainedImage;
    uint& m_displayRevision = display.revision;
    uint& m_requestRevision = request.requestRevision;
    uint& m_commandRevision = request.commandRevision;
    QString& m_errorString = request.errorString;
    QString& m_warningString = request.warningString;
    QPointer<ImageSequenceProviderSession>& m_providerSession = provider.session;
    quint64& m_providerSessionSerial = provider.sessionSerial;
    quint64& m_nextProviderRequestToken = provider.nextRequestToken;
    ImageSequenceProviderRequestToken& m_activeProviderMetadataToken = provider.activeMetadataToken;
    ImageSequenceProviderRequestToken& m_activeProviderFrameToken = provider.activeFrameToken;
    quint64& m_activeProviderFrameRequestId = provider.activeFrameRequestId;
    bool& m_activeProviderFrameFromPlayback = provider.activeFrameFromPlayback;
    ProviderRequestTargetKind& m_activeProviderFrameTargetKind = provider.activeFrameTargetKind;
    bool& m_queuedProviderFrameRequest = provider.queuedFrameRequest;
    quint64& m_queuedProviderFrameGeneration = provider.queuedFrameGeneration;
    quint64& m_queuedProviderFrameRequestId = provider.queuedFrameRequestId;
    int& m_queuedProviderFrame = provider.queuedFrame;
    int& m_queuedProviderPosition = provider.queuedPosition;
    bool& m_queuedProviderFrameFromPlayback = provider.queuedFrameFromPlayback;
    ProviderRequestTargetKind& m_queuedProviderFrameTargetKind = provider.queuedFrameTargetKind;
    bool& m_providerMetadataReady = provider.metadataReady;
    bool& m_providerTimedMetadata = provider.timedMetadata;
    bool& m_providerTimedPlaybackSupport = provider.timedPlaybackSupport;
    bool& m_providerFrameSeekSupport = provider.frameSeekSupport;
    bool& m_providerPositionSeekSupport = provider.positionSeekSupport;
    QSizeF& m_providerLogicalSize = provider.logicalSize;
    TimingIntervals& m_providerTimingIntervals = provider.timingIntervals;
};
