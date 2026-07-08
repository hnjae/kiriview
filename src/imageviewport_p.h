#pragma once

#include "imageviewport.h"
#include "imageviewportdiagnostics_p.h"
#include "imageviewportplaybackscheduler_p.h"
#include "imageviewportproviderhost_p.h"
#include "imageviewportrenderhost_p.h"
#include "imageviewportstate_p.h"
#include "viewportcontroller_p.h"

class ImageViewportPrivate : public ViewportControllerContext
{
public:
    using BackgroundMode = ImageViewport::BackgroundMode;
    using CommandOutcome = ImageViewport::CommandOutcome;
    using CommandReason = ImageViewport::CommandReason;
    using DisplayStatus = ImageViewport::DisplayStatus;
    using FitMode = ImageViewport::FitMode;
    using PageRole = ImageViewport::PageRole;
    using PlaybackPhase = ImageViewport::PlaybackPhase;
    using RequestReason = ImageViewport::RequestReason;
    using RequestStatus = ImageViewport::RequestStatus;
    using SpreadDirection = ImageViewport::SpreadDirection;
    using TriState = ImageViewport::TriState;
    using DisplayRequestSnapshot = ImageViewportInternal::DisplayRequestSnapshot;
    using DisplayRequestOrigin = ImageViewportInternal::DisplayRequestOrigin;
    using ProviderRequestTargetKind = ImageViewportInternal::ProviderRequestTargetKind;

    explicit ImageViewportPrivate(ImageViewport* viewport);
    ~ImageViewportPrivate();
    static ImageViewportPrivate* get(ImageViewport& viewport) { return viewport.d.get(); }
    static const ImageViewportPrivate* get(const ImageViewport& viewport)
    {
        return viewport.d.get();
    }

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
    int frameCount() const override;
    int totalDuration() const override;
    bool hasSecondaryTimedSequence() const override;
    int secondarySequenceFrameCount() const override;
    int sequenceTotalDuration() const override;
    int secondarySequenceTotalDuration() const override;
    int secondarySequenceFrameIndexForPosition(int position) const override;
    int secondarySequenceFrameStartPosition(int frame) const override;
    ImageViewportRange frameSeekBounds() const;
    ImageViewportRange positionSeekBounds() const;
    int primaryFrameCount() const;
    int secondaryFrameCount() const;
    int primaryTotalDuration() const;
    int secondaryTotalDuration() const override;
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
    RevisionToken displayRevision() const;
    RevisionToken requestRevision() const;
    RevisionToken commandRevision() const;
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
    CommandOutcome setPageSet(const QVariant& pageSet);
    CommandOutcome setPageSet(const QVariant& primary, const QVariant& secondary);
    CommandOutcome setPageSet(
        const QVariant& primary, const QVariant& secondary, PageSetTransitionPolicy policy);
    CommandOutcome setPageSet(ImageViewportPageSet pageSet);
    CommandOutcome setPageSet(ImageViewportPageSet pageSet, PageSetTransitionPolicy policy);
    CommandOutcome setPageSet(ImageSequence* primary, ImageSequence* secondary);
    CommandOutcome setPageSet(
        ImageSequence* primary, ImageSequence* secondary, PageSetTransitionPolicy policy);
    CommandOutcome setSpreadDirection(SpreadDirection direction);
    CommandOutcome setPageGap(double gap);
    CommandOutcome setFitMode(FitMode mode, QPointF anchor);
    CommandOutcome setZoomPercent(double percent, QPointF anchor);
    CommandOutcome zoomByStep(int stepCount, QPointF anchor);
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
    CommandOutcome setPresentation(ImageViewportPresentationCommand command);
    ImageViewportCoordinateResult mapPoint(ImageViewportCoordinateInput input) const;
    bool containsPoint(ImageViewportCoordinateInput input) const;
    ImageViewportCoordinateResult nearestVisiblePoint(ImageViewportCoordinateInput input) const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
    void setNextProviderRequestTokenForTest(quint64 token);
    void setNextProviderRequestTokenForTest(PageRole role, quint64 token);
    void setNextRevisionTokenForTest(quint64 token);
    void failNextProviderCommandDeliveryForTest(PageRole role);
    void failNextProviderQueueFlushSchedulingForTest(PageRole role);
    void useSynchronousProviderExecutorForTest();
    void useSynchronousProviderEventDeliveryForTest();
    void useSynchronousProviderQueueFlushSchedulerForTest();
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    quint64 secondaryPendingRenderPayloadIdForTest() const;
    ImageViewportInternal::RenderFailureDiagnostic
    lastAcceptedRenderFailureDiagnosticForTest() const;
    ImageViewportInternal::ProviderTransportDiagnostic
    lastProviderTransportDiagnosticForTest() const;
    ImageViewportInternal::ProviderSchedulerDiagnostic
    lastProviderSchedulerDiagnosticForTest() const;
    void acknowledgeRenderCommitForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderCommitForTest(quint64 generation, quint64 requestId,
        quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId);
    void acknowledgeRenderFailureForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(
        PageRole failedRole, quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(PageRole failedRole, quint64 generation, quint64 requestId,
        quint64 preparedPayloadId, RenderFailureCause cause);
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
    PageGeometry primaryPageGeometry() const;
    PageGeometry secondaryPageGeometry() const;
    PageGeometry pageGeometry(PageRole role) const;
    QSizeF contentSize() const;
    QPointF contentPosition() const;
    QPointF maximumContentPosition() const;
    bool horizontalPannable() const;
    bool verticalPannable() const;
    FitMode fitMode() const;
    void setFitModeProperty(FitMode mode);
    double zoomPercent() const;
    void setZoomPercentProperty(double percent);
    double minimumManualZoomPercent() const;
    double maximumManualZoomPercent() const;
    double manualZoomStepFactor() const;
    double clampedManualZoomPercent(double percent) const;
    double steppedManualZoomPercent(int stepCount) const;
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
    CoordinateResult itemToSpread(double x, double y) const;
    CoordinateResult spreadToItem(double x, double y) const;
    CoordinateResult nearestVisibleSpreadPoint(double x, double y) const;
    CoordinateResult itemToPage(PageRole role, double x, double y) const;
    CoordinateResult pageToItem(PageRole role, double x, double y) const;
    CoordinateResult nearestVisiblePagePoint(PageRole role, double x, double y) const;
    bool containsVisibleSpreadPoint(double x, double y) const;
    bool containsVisiblePagePoint(PageRole role, double x, double y) const;
    CoordinateResult itemToImage(double x, double y) const;
    CoordinateResult imageToItem(double x, double y) const;
    CoordinateResult nearestVisibleImagePoint(double x, double y) const;
    bool containsVisibleImagePoint(double x, double y) const;
    static ImageViewportRange invalidRange();
    static CoordinateResult invalidCoordinateResult();
    void applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes);
    void refreshStateSnapshot();
    QRectF currentContentRect() const;
    QRectF itemBounds() const override;
    QRectF contentRectForItemBounds(const QRectF& bounds) const;
    QRectF visibleImageRectForItemBounds(const QRectF& bounds) const;

    bool providerHasCompleteKnownMetadata() const override;
    ImageSequenceProviderKnownFacts providerKnownFacts() const override;
    QSizeF providerKnownLogicalSize() const override;
    TimingIntervals providerKnownTimingIntervals() const override;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const override;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const override;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const override;
    int providerFrameStartPosition(int frame) const override;
    int providerFrameIndexForPosition(int position) const override;
    ImageSequenceAuthoredAnimationFacts providerAuthoredAnimationFacts() const override;
    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
    void incrementDisplayRevision();
    void incrementRequestRevision();
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
    ImageSequenceAuthoredAnimationFacts sequenceAuthoredAnimationFacts() const override;
    ImageSequenceAuthoredAnimationFacts secondarySequenceAuthoredAnimationFacts() const override;
    ImageSequenceProviderKnownFacts secondaryProviderKnownFacts() const override;
    QSizeF secondaryProviderKnownLogicalSize() const override;
    TimingIntervals secondaryProviderKnownTimingIntervals() const override;
    ImageSequenceProviderCapabilitySupport
    secondaryProviderTimedPlaybackCapability() const override;
    ImageSequenceProviderCapabilitySupport secondaryProviderFrameSeekCapability() const override;
    ImageSequenceProviderCapabilitySupport secondaryProviderPositionSeekCapability() const override;
    QSizeF sequenceLogicalSize() const override;
    QSizeF secondarySequenceLogicalSize() const override;

    double width() const override;
    double height() const override;
    QQuickWindow* window() const;
    void update();

    ImageViewport* q = nullptr;
    ViewportController controller;
    ImageViewportPlaybackScheduler playbackScheduler;
    ImageViewportProviderHost providerHost;
    ImageViewportRenderHost renderHost;
    ImageViewportInternal::InternalDiagnostics internalDiagnostics;
    ImageViewportStateSnapshot lastStateSnapshot;
};
