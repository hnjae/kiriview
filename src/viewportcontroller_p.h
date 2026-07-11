#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollerassignmentcontract_p.h"
#include "viewportengine_p.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>

#include <functional>

struct ControllerTransitionPolicy;
struct ViewportCommandResult;
struct ViewportPlaybackAdvanceResult;
struct ViewportPlaybackScheduleEffect;
struct ViewportPresentationCommandInput;
struct ViewportProviderAcceptedMetadataFacts;
struct ViewportProviderDispatchFailureEvent;
struct ViewportProviderEndOfSequenceEvent;
struct ViewportProviderEndOfSequenceProtocolViolation;
struct ViewportProviderEndOfSequenceResult;
struct ViewportProviderEvent;
struct ViewportProviderEventResult;
struct ViewportProviderFrameDispatchResult;
struct ViewportProviderFrameEvent;
struct ViewportProviderFrameEventAcceptance;
struct ViewportProviderFrameQueueFlush;
struct ViewportProviderFrameQueueFlushResult;
struct ViewportProviderFrameQueueRequest;
struct ViewportProviderFrameQueueResult;
struct ViewportProviderFrameRequestStart;
struct ViewportProviderFrameRequestStartResult;
struct ViewportProviderFrameTerminalResult;
struct ViewportProviderFrameTransportEffect;
struct ViewportProviderMetadataAdmissionRejection;
struct ViewportProviderMetadataAdmissionResult;
struct ViewportProviderMetadataContradiction;
struct ViewportProviderMetadataEvent;
struct ViewportProviderMetadataEventAcceptance;
struct ViewportProviderMetadataReadyEvent;
struct ViewportProviderMetadataReadyResult;
struct ViewportProviderMetadataRequestStartResult;
struct ViewportProviderMetadataTargetPolicyResult;
struct ViewportProviderMetadataTargetRejection;
struct ViewportProviderMetadataTargetSelection;
struct ViewportProviderMetadataTerminalResult;
struct ViewportProviderRequestTokenAllocation;
struct ViewportProviderSchedulerFailureResult;
struct ViewportProviderSessionClose;
struct ViewportProviderSessionOpenResult;
struct ViewportProviderTerminalEvent;
struct ViewportProviderTerminalEventResult;
struct ViewportProviderWaitingEvent;
struct ViewportRenderAcknowledgement;
struct ViewportRenderSynchronization;
struct ViewportSequenceAssignment;
struct ViewportSequenceAssignmentResult;

class ViewportController
{
public:
    explicit ViewportController(std::function<QRectF()> captureItemBounds);

    const ImageViewportInternal::PresentationState& presentationState() const;
    const ImageViewportInternal::DisplayState& displayState() const;
    const ImageViewportInternal::RequestState& requestState() const;
    bool hasProviderSession() const;
    bool providerMetadataReady() const;
    bool secondaryProviderMetadataReady() const;
    ImageViewportStateSnapshot stateSnapshot(double devicePixelRatio = 1.0) const;
    ImageViewportInternal::ViewportChangeSet publishChanges(
        ImageViewportInternal::ViewportChangeSet changes);
    PresentationGeometry::State geometryState(double devicePixelRatio = 1.0) const;
    PresentationGeometry::State geometryStateForItemBounds(
        const QRectF& itemBounds, double devicePixelRatio = 1.0) const;
    double minimumManualZoomPercent() const;
    double maximumManualZoomPercent(double devicePixelRatio = 1.0) const;
    double manualZoomStepFactor() const;
    double clampedManualZoomPercent(double percent, double devicePixelRatio = 1.0) const;
    double steppedManualZoomPercent(int stepCount, double devicePixelRatio = 1.0) const;

    ViewportSequenceAssignmentResult assignSequence(ViewportSequenceAssignment assignment);
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewport::PageRole role);
    ViewportCommandResult clear();
    ViewportCommandResult play(ImageViewport::PageRole role);
    ViewportCommandResult pause(ImageViewport::PageRole role);
    ViewportCommandResult stop(ImageViewport::PageRole role);
    ViewportCommandResult seek(ImageViewport::PageRole role, int frame);
    ViewportCommandResult seekToPosition(ImageViewport::PageRole role, int milliseconds);
    ViewportCommandResult applyPlaybackCommand(ViewportPlaybackCommand command);
    ViewportCommandResult setPresentation(const ViewportPresentationCommandInput& input);
    ViewportCommandResult setSpreadDirection(ImageViewport::SpreadDirection direction);
    ViewportCommandResult setPageGap(double gap);
    ViewportCommandResult setFitMode(ImageViewport::FitMode mode, QPointF anchor);
    ViewportCommandResult setZoomPercent(
        double percent, QPointF anchor, double devicePixelRatio = 1.0);
    ViewportCommandResult zoomByStep(int stepCount, QPointF anchor, double devicePixelRatio = 1.0);
    ViewportCommandResult panBy(QPointF delta);
    ViewportCommandResult panToStart();
    ViewportCommandResult panToEnd();
    ViewportCommandResult scanNext();
    ViewportCommandResult scanPrevious();
    ViewportCommandResult rotateClockwise(QPointF anchor);
    ViewportCommandResult rotateCounterClockwise(QPointF anchor);
    ViewportCommandResult setMirrorHorizontally(bool enabled, QPointF anchor);
    ViewportCommandResult setMirrorVertically(bool enabled, QPointF anchor);
    ViewportCommandResult resetView();
    ViewportProviderEventResult handleProviderEvent(const ViewportProviderEvent& event);
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        double devicePixelRatio = 1.0);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameEvent(ImageViewport::PageRole role,
        ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameEvent(
        ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata);
    ViewportProviderMetadataEventAcceptance acceptProviderMetadataEvent(
        ViewportProviderMetadataEvent event);
    ViewportProviderMetadataEventAcceptance acceptProviderMetadataEvent(
        ImageViewport::PageRole role, ViewportProviderMetadataEvent event);
    ViewportProviderMetadataReadyResult handleProviderMetadataReadyEvent(
        ImageViewport::PageRole role, const ViewportProviderMetadataReadyEvent& event);
    ImageViewportInternal::ViewportChangeSet handleProviderSessionOpenFailure(
        const QString& diagnostic);
    ImageViewportInternal::ViewportChangeSet handleProviderSessionOpenFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    ViewportProviderSessionOpenResult handleProviderSessionOpened();
    ViewportProviderSessionOpenResult handleProviderSessionOpened(ImageViewport::PageRole role);
    quint64 activateProviderSession();
    quint64 activateProviderSession(ImageViewport::PageRole role);
    void retireProviderSession();
    void retireProviderSession(ImageViewport::PageRole role);
    quint64 currentProviderGeneration() const;
    quint64 currentProviderGeneration(ImageViewport::PageRole role) const;
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory(
        ImageViewport::PageRole role) const;
    ImageSequenceProviderThreadingContract providerThreadingContract(
        ImageViewport::PageRole role) const;
    bool acceptsProviderSessionResult(quint64 sessionSerial) const;
    bool acceptsProviderSessionResult(ImageViewport::PageRole role, quint64 sessionSerial) const;
    bool acceptsProviderSessionResult(
        ImageViewport::PageRole role, quint64 sessionSerial, quint64 generation) const;
    ViewportProviderMetadataAdmissionResult handleProviderMetadataAdmission(
        const ImageSequenceProviderMetadata& metadata);
    ViewportProviderMetadataAdmissionResult handleProviderMetadataAdmission(
        ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata);
    ViewportProviderTerminalEventResult handleProviderTerminalEvent(
        const ViewportProviderTerminalEvent& event);
    ViewportProviderTerminalEventResult handleProviderTerminalEvent(
        ImageViewport::PageRole role, const ViewportProviderTerminalEvent& event);
    ViewportProviderTerminalEventResult handleProviderDispatchFailure(
        ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event);
    ViewportProviderSchedulerFailureResult handleProviderQueueFlushSchedulingFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    ImageViewportInternal::ViewportChangeSet handleProviderAcceptedMetadataFacts(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ImageViewportInternal::ViewportChangeSet handleProviderAcceptedMetadataFacts(
        ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetPolicy(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetPolicy(
        ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderEndOfSequenceResult handleProviderEndOfSequenceEvent(
        ViewportProviderEndOfSequenceEvent event);
    ViewportProviderEndOfSequenceResult handleProviderEndOfSequenceEvent(
        ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event);
    ViewportProviderFrameTransportEffect closeProviderSession();
    ViewportProviderFrameQueueFlushResult flushQueuedProviderFrameRequestEvent();
    ViewportProviderFrameQueueFlushResult flushQueuedProviderFrameRequestEvent(
        ImageViewport::PageRole role);
    ViewportProviderFrameRequestStartResult startProviderFrameRequest(
        ViewportProviderFrameRequestStart request);
    ViewportProviderFrameRequestStartResult startProviderFrameRequest(
        ImageViewport::PageRole role, ViewportProviderFrameRequestStart request);
    ViewportProviderFrameDispatchResult dispatchProviderFrameRequest(
        ViewportProviderFrameRequestStart request);
    ViewportProviderFrameDispatchResult dispatchProviderFrameRequest(
        ImageViewport::PageRole role, ViewportProviderFrameRequestStart request);
    ViewportEngine::GeometryChangeResult handleGeometryChanged(
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);
    ViewportRenderSynchronization beginRenderSynchronization(double devicePixelRatio = 1.0);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderCommit(
        const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
        const ViewportRenderSynchronization& synchronization);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderFailure(
        const ViewportRenderAcknowledgement& acknowledgement);
    ViewportPlaybackScheduleEffect playbackScheduleEffect() const;
    ViewportPlaybackAdvanceResult advancePlayback(int elapsedMilliseconds);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void setNextProviderRequestTokenForTest(quint64 token);
    void setNextProviderRequestTokenForTest(ImageViewport::PageRole role, quint64 token);
    void setNextRevisionTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    quint64 secondaryPendingRenderPayloadIdForTest() const;
    ImageViewportInternal::RenderFailureDiagnostic
    lastAcceptedRenderFailureDiagnosticForTest() const;
#endif

private:
    QRectF itemBounds() const;
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ViewportProviderWaitingEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ImageViewport::PageRole role, ViewportProviderWaitingEvent event);
    ViewportProviderSessionClose handleProviderSessionClose();
    ViewportProviderSessionClose handleProviderSessionClose(ImageViewport::PageRole role);
    ViewportProviderRequestTokenAllocation allocateProviderRequestToken();
    ViewportProviderRequestTokenAllocation allocateProviderRequestToken(
        ImageViewport::PageRole role);
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest();
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest(
        ImageViewport::PageRole role);
    ViewportProviderFrameQueueResult queueProviderFrameRequest(
        ViewportProviderFrameQueueRequest request);
    ViewportProviderFrameQueueResult queueProviderFrameRequest(
        ImageViewport::PageRole role, ViewportProviderFrameQueueRequest request);
    ViewportProviderFrameQueueFlush flushQueuedProviderFrameRequest();
    ViewportProviderFrameQueueFlush flushQueuedProviderFrameRequest(ImageViewport::PageRole role);

    std::function<QRectF()> captureItemBounds;
    ViewportEngine engine;
};
