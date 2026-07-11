#pragma once

#include "coordinateresult_p.h"
#include "imageviewport.h"
#include "imageviewportdiagnostics_p.h"
#include "imageviewportplaybackscheduler_p.h"
#include "imageviewportproviderhost_p.h"
#include "imageviewportrenderhost_p.h"
#include "imageviewportstate_p.h"
#include "viewportcontroller_p.h"

class ImageViewportPrivate
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
    ImageViewportCommandResult commandResult(CommandOutcome outcome) const;
    SpreadDirection spreadDirection() const;
    double pageGap() const;

    CommandReason commandReason() const;
    DisplayStatus displayStatus() const;
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
    QSizeF displayedSpreadSize() const;
    QString errorString() const;
    QString warningString() const;

    CommandOutcome clear();
    CommandOutcome play(PageRole role);
    CommandOutcome pause(PageRole role);
    CommandOutcome stop(PageRole role);
    CommandOutcome seek(PageRole role, int frame);
    CommandOutcome seekToPosition(PageRole role, int milliseconds);
    CommandOutcome setPresentationTarget(ImageViewportPresentationTarget presentationTarget,
        PresentationTargetTransitionPolicy policy);
    CommandOutcome resetView();
    CommandOutcome setPresentation(ImageViewportPresentationCommand command);
    ImageViewportCoordinateResult mapPoint(const ImageViewportCoordinateInput& input) const;
    bool containsPoint(const ImageViewportCoordinateInput& input) const;
    ImageViewportCoordinateResult nearestVisiblePoint(
        const ImageViewportCoordinateInput& input) const;
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
    FitMode fitMode() const;
    double zoomPercent() const;
    double minimumManualZoomPercent() const;
    double maximumManualZoomPercent() const;
    double manualZoomStepFactor() const;
    int rotationDegrees() const;
    bool smoothing() const;
    bool mipmap() const;
    bool mirrorHorizontally() const;
    bool mirrorVertically() const;
    BackgroundMode backgroundMode() const;
    QColor backgroundColor() const;
    bool looping() const;
    CoordinateResult itemToSpread(double x, double y) const;
    CoordinateResult spreadToItem(double x, double y) const;
    CoordinateResult nearestVisibleSpreadPoint(double x, double y) const;
    CoordinateResult itemToPage(PageRole role, double x, double y) const;
    CoordinateResult pageToItem(PageRole role, double x, double y) const;
    CoordinateResult nearestVisiblePagePoint(PageRole role, double x, double y) const;
    bool containsVisibleSpreadPoint(double x, double y) const;
    bool containsVisiblePagePoint(PageRole role, double x, double y) const;
    static CoordinateResult invalidCoordinateResult();
    void applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes);
    void devicePixelRatioChanged();
    void refreshStateSnapshot();
    QRectF currentContentRect() const;
    QRectF itemBounds() const;
    QRectF contentRectForItemBounds(const QRectF& bounds) const;
    QRectF visibleImageRectForItemBounds(const QRectF& bounds) const;

    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
    void advancePlayback(int elapsedMilliseconds);
    bool hasActiveRequest() const;
    bool hasReadyDisplay() const;
    bool hasDisplayableSequence() const;

    double width() const;
    double height() const;
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
