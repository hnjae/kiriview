#pragma once

#include "imageviewporthelpers_p.h"
#include "imageviewportstate_p.h"
#include "renderadapter_p.h"
#include "viewportcontroller_p.h"
#include "viewportproviderbridge_p.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtQuick/QSGNode>

class ImageViewportPrivate : public ViewportProviderBridgeClient
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
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    void acknowledgeRenderCommitForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
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
    QRectF contentRectForImageSize(QSizeF imageSize) const;
    QRectF visibleImageRectForImageSize(QSizeF imageSize) const;
    QRectF itemBounds() const;
    QRectF contentRectForItemBounds(const QRectF& bounds) const;
    QRectF visibleImageRectForItemBounds(const QRectF& bounds) const;
    QSizeF currentImageSize() const;

    QSGNode* updatePaintNode(QSGNode* oldNode);
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry,
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);

    bool openProviderSession();
    QObject* providerCallbackTarget() const override;
    quint64 installProviderSession(ImageSequenceProviderSession* session) override;
    ImageSequenceProviderSession* takeProviderSession() override;
    ImageSequenceProviderSession* currentProviderSession() const override;
    bool acceptsProviderSessionResult(quint64 sessionSerial) const override;
    bool providerHasCompleteKnownMetadata() const;
    ImageSequenceProviderKnownFacts providerKnownFacts() const;
    QSizeF providerKnownLogicalSize() const;
    TimingIntervals providerKnownTimingIntervals() const;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const;
    void startProviderMetadataRequest();
    void requestProviderMetadata(ImageSequenceProviderRequestToken token);
    void requestProviderFrame(ImageSequenceProviderRequestToken token, int frame);
    void requestProviderPosition(ImageSequenceProviderRequestToken token, int frame, int position);
    void requestProviderPlayback(ImageSequenceProviderRequestToken token, int frame, int position);
    void cancelProviderRequest(ImageSequenceProviderRequestToken token);
    void applyProviderMetadataTransportEffect(
        const ViewportProviderMetadataTransportEffect& effect);
    void applyProviderFrameTransportEffect(const ViewportProviderFrameTransportEffect& effect);
    void queueProviderFrameRequest(int frame, ProviderRequestTargetKind targetKind);
    void flushQueuedProviderFrameRequest();
    bool startProviderFrameRequest(int frame, ProviderRequestTargetKind targetKind);
    void handleProviderEvent(const ViewportProviderEvent& event) override;
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
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory() const override;
    int providerFrameStartPosition(int frame) const;
    int providerFrameIndexForPosition(int position) const;
    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
    ImageSequenceProviderThreadingContract providerThreadingContract() const override;
    void incrementDisplayRevision();
    void incrementRequestRevision();
    void syncPlaybackTimer();
    void stopPlaybackTimer();
    void handlePlaybackTimer();
    int takePlaybackTimerElapsed();
    void flushPlaybackTimerElapsed();
    int playbackTimerInterval() const;
    void advancePlayback(int elapsedMilliseconds);
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
    QSizeF sequenceLogicalSize() const;
    QImage sequenceFrameImage(int frame) const;

    double width() const;
    double height() const;
    QQuickWindow* window() const;
    void update();

    ImageViewport* q = nullptr;
    ViewportController controller;
    ViewportProviderBridge providerBridge;
    RenderAdapter renderAdapter;
    ImageViewportInternal::PresentationState presentation;
    QTimer playbackTimer;
    QElapsedTimer playbackElapsedTimer;
};
