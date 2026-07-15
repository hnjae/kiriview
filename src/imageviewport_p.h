#pragma once

#include "coordinateresult_p.h"
#include "imageviewportdiagnostics_p.h"
#include "imageviewportplaybackscheduler_p.h"
#include "imageviewportproviderhost_p.h"
#include "imageviewportrenderhost_p.h"
#include "imageviewportstate_p.h"
#include "viewportengine_p.h"
#include "viewportitemtransaction_p.h"
#include <ImageViewport/ImageViewport>

#include <QtCore/QMutex>

#include <optional>

class ImageViewportPrivate
{
public:
    using BackgroundMode = ImageViewportBackgroundMode;
    using CommandOutcome = ImageViewportCommandOutcome;
    using CommandReason = ImageViewportCommandReason;
    using DisplayStatus = ImageViewportDisplayStatus;
    using FitMode = ImageViewportFitMode;
    using PageRole = ImageViewportPageRole;
    using PlaybackPhase = ImageViewportPlaybackPhase;
    using RequestReason = ImageViewportRequestReason;
    using RequestStatus = ImageViewportRequestStatus;
    using SpreadDirection = ImageViewportSpreadDirection;
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
    quint64 currentRenderAttemptForTest() const;
    void reportRenderQualityFallbackForTest(
        quint64 renderAttempt, bool smoothingUnavailable, bool mipmapUnavailable);
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
    void applyEngineTransition(ViewportEngineTransition transition);
    void enqueueProviderHostEvent(ViewportProviderHostEvent event);
    void drainProviderHostEvents();
    void devicePixelRatioChanged();
    void refreshStateSnapshot();
    QRectF currentContentRect() const;
    QRectF itemBounds() const;
    QRectF contentRectForItemBounds(const QRectF& bounds) const;
    QRectF visibleImageRectForItemBounds(const QRectF& bounds) const;

    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
    CommandOutcome executePlaybackCommand(ViewportPlaybackCommand command);
    void advancePlayback(int elapsedMilliseconds);
    bool hasActiveRequest() const;
    bool hasReadyDisplay() const;
    bool hasDisplayableSequence() const;

    double width() const;
    double height() const;
    QQuickWindow* window() const;
    void update();
    QSGNode* updatePaintNode(QSGNode* oldNode);
    void prepareRenderSynchronization();
    std::optional<ViewportRenderSynchronization> renderSynchronizationForHost() const;
    void applyRenderHostFact(RenderAdapter::CommitResult result,
        ViewportRenderAcknowledgement acknowledgement,
        ViewportRenderQualityFallbackFact qualityFallback, bool imagePresent,
        ViewportRenderSynchronization synchronization);
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry,
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);

    ImageViewport* q = nullptr;
    ViewportEngine engine;
    ImageViewportPlaybackScheduler playbackScheduler;
    ImageViewportProviderHost providerHost;
    ImageViewportRenderHost renderHost;
    ImageViewportInternal::InternalDiagnostics internalDiagnostics;
    ImageViewportStateSnapshot lastStateSnapshot;
    int transitionApplicationDepth = 0;
    ViewportPlaybackScheduleEffect pendingPlaybackSchedule;
    QVector<ViewportProviderHostEvent> pendingProviderHostEvents;
    bool drainingProviderHostEvents = false;
    mutable QMutex renderMailboxMutex;
    ViewportRenderSynchronization renderMailbox;
    bool renderMailboxValid = false;
};
