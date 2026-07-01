#pragma once

#include "imageviewporthelpers_p.h"
#include "imageviewportstate_p.h"
#include "playbackclock_p.h"
#include "renderadapter_p.h"
#include "viewportcontroller_p.h"
#include "viewportproviderbridge_p.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtQuick/QSGNode>

class ImageViewportPrivate : public ViewportProviderBridgeClient, public ViewportControllerContext
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
    int frameCount() const override;
    int totalDuration() const override;
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
    QRectF contentRect() const override;
    QRectF visibleImageRect() const override;
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
    QRectF itemBounds() const override;
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
    bool providerHasCompleteKnownMetadata() const override;
    ImageSequenceProviderKnownFacts providerKnownFacts() const override;
    QSizeF providerKnownLogicalSize() const override;
    TimingIntervals providerKnownTimingIntervals() const override;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const override;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const override;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const override;
    void startProviderMetadataRequest();
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
    int providerFrameStartPosition(int frame) const override;
    int providerFrameIndexForPosition(int position) const override;
    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
    ImageSequenceProviderThreadingContract providerThreadingContract() const override;
    void incrementDisplayRevision();
    void incrementRequestRevision();
    void syncPlaybackTimer();
    void stopPlaybackTimer();
    void handlePlaybackTimer();
    int takePlaybackTimerElapsed();
    void flushPlaybackTimerElapsed();
    void advancePlayback(int elapsedMilliseconds);
    bool hasActiveRequest() const override;
    bool hasReadyDisplay() const override;
    bool hasDisplayableSequence() const override;
    bool hasStillSequence() const;
    bool hasTimedSequence() const override;
    bool hasProviderSequence() const override;
    bool hasGenerationTerminalProviderFailure() const override;
    bool providerTimedPlaybackCapabilityKnownFalse() const override;
    bool providerFrameSeekCapabilityKnownFalse() const override;
    bool providerFrameSeekCapabilityKnownTrue() const override;
    bool providerPositionSeekCapabilityKnownFalse() const override;
    bool providerKnownFactsTimedFrameCount() const override;
    int providerKnownFactsFrameCount() const override;
    int sequenceFrameCount() const override;
    int sequenceFrameIndexForPosition(int position) const override;
    int sequenceFrameStartPosition(int frame) const override;
    QSizeF sequenceLogicalSize() const override;
    QImage sequenceFrameImage(int frame) const override;

    double width() const override;
    double height() const override;
    QQuickWindow* window() const;
    void update();

    ImageViewport* q = nullptr;
    ViewportController controller;
    ViewportProviderBridge providerBridge;
    RenderAdapter renderAdapter;
    ImageViewportInternal::PresentationState presentation;
    QTimer playbackTimer;
    QElapsedTimer playbackClockTimebase;
    ImageViewportInternal::PlaybackClock playbackClock;
};
