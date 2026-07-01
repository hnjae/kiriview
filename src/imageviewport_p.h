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
    using FitMode = ImageViewport::FitMode;
    using HorizontalAlignment = ImageViewport::HorizontalAlignment;
    using PageRole = ImageViewport::PageRole;
    using PlaybackPhase = ImageViewport::PlaybackPhase;
    using RequestReason = ImageViewport::RequestReason;
    using RequestStatus = ImageViewport::RequestStatus;
    using SpreadDirection = ImageViewport::SpreadDirection;
    using TriState = ImageViewport::TriState;
    using VerticalAlignment = ImageViewport::VerticalAlignment;
    using DisplayRequestSnapshot = ImageViewportInternal::DisplayRequestSnapshot;
    using DisplayRequestOrigin = ImageViewportInternal::DisplayRequestOrigin;
    using ProviderRequestTargetKind = ImageViewportInternal::ProviderRequestTargetKind;

    explicit ImageViewportPrivate(ImageViewport* viewport);
    ~ImageViewportPrivate();

    ImageSequence* sequence() const;
    void setSequence(ImageSequence* sequence);
    ImageSequence* primarySequence() const;
    ImageSequence* secondarySequence() const;
    int frameCountForSequence(ImageSequence* sequence) const;
    int totalDurationForSequence(ImageSequence* sequence) const;
    QVariantMap frameSeekBoundsForSequence(ImageSequence* sequence) const;
    QVariantMap positionSeekBoundsForSequence(ImageSequence* sequence) const;
    TriState timedPlaybackSupportForSequence(ImageSequence* sequence) const;
    TriState frameSeekSupportForSequence(ImageSequence* sequence) const;
    TriState positionSeekSupportForSequence(ImageSequence* sequence) const;
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
    int frameCount() const override;
    int totalDuration() const override;
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
    uint displayRevision() const;
    uint requestRevision() const;
    uint commandRevision() const;
    QString errorString() const;
    QString warningString() const;

    CommandOutcome clear();
    CommandOutcome play();
    CommandOutcome play(PageRole role);
    CommandOutcome pause();
    CommandOutcome pause(PageRole role);
    CommandOutcome stop();
    CommandOutcome stop(PageRole role);
    CommandOutcome seek(int frame);
    CommandOutcome seek(PageRole role, int frame);
    CommandOutcome seekToPosition(int milliseconds);
    CommandOutcome seekToPosition(PageRole role, int milliseconds);
    CommandOutcome setPageSet(
        const QVariant& primary, const QVariant& secondary, const QVariant& policy);
    CommandOutcome setSpreadDirection(SpreadDirection direction);
    CommandOutcome setPageGap(double gap);
    CommandOutcome setFitMode(FitMode mode, QPointF anchor);
    CommandOutcome setZoomPercent(double percent, QPointF anchor);
    CommandOutcome panBy(QPointF delta);
    CommandOutcome panToStart();
    CommandOutcome panToEnd();
    CommandOutcome scanNext();
    CommandOutcome scanPrevious();
    CommandOutcome rotateClockwise(QPointF anchor);
    CommandOutcome rotateCounterClockwise(QPointF anchor);
    CommandOutcome setMirrorHorizontally(bool enabled, QPointF anchor);
    CommandOutcome setMirrorVertically(bool enabled, QPointF anchor);
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
    QVariantMap itemToSpread(double x, double y) const;
    QVariantMap spreadToItem(double x, double y) const;
    QVariantMap itemToPage(PageRole role, double x, double y) const;
    QVariantMap pageToItem(PageRole role, double x, double y) const;
    bool containsVisibleSpreadPoint(double x, double y) const;
    bool containsVisiblePagePoint(PageRole role, double x, double y) const;
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

    bool openProviderSession(PageRole role = PageRole::Primary);
    QObject* providerCallbackTarget() const override;
    quint64 installProviderSession(PageRole role, ImageSequenceProviderSession* session) override;
    ImageSequenceProviderSession* takeProviderSession(PageRole role) override;
    ImageSequenceProviderSession* currentProviderSession(PageRole role) const override;
    bool providerHasCompleteKnownMetadata() const override;
    ImageSequenceProviderKnownFacts providerKnownFacts() const override;
    QSizeF providerKnownLogicalSize() const override;
    TimingIntervals providerKnownTimingIntervals() const override;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const override;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const override;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const override;
    void startProviderMetadataRequest();
    void applyProviderMetadataTransportEffect(const ViewportProviderMetadataTransportEffect& effect,
        PageRole role = PageRole::Primary);
    void applyProviderFrameTransportEffect(const ViewportProviderFrameTransportEffect& effect,
        PageRole role = PageRole::Primary);
    void queueProviderFrameRequest(int frame, ProviderRequestTargetKind targetKind);
    void flushQueuedProviderFrameRequest();
    bool startProviderFrameRequest(int frame, ProviderRequestTargetKind targetKind);
    void handleProviderEvent(const ViewportProviderEvent& event) override;
    void handleProviderMetadataReady(
        ImageSequenceProviderRequestToken token, const ImageSequenceProviderMetadata& metadata);
    void handleSecondaryProviderMetadataReady(
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
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory(
        PageRole role) const override;
    int providerFrameStartPosition(int frame) const override;
    int providerFrameIndexForPosition(int position) const override;
    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
    ImageSequenceProviderThreadingContract providerThreadingContract(PageRole role) const override;
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
    ViewportProviderBridge secondaryProviderBridge;
    RenderAdapter renderAdapter;
    ImageViewportInternal::PresentationState presentation;
    QTimer playbackTimer;
    QElapsedTimer playbackClockTimebase;
    ImageViewportInternal::PlaybackClock playbackClock;
};
