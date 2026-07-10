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

struct ControllerTransitionPolicy;
struct ViewportCommandResult;
struct ViewportMetadataProjection;
struct ViewportPlaybackAdvanceResult;
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

struct ViewportControllerState
{
    ViewportSequenceRoleSource secondarySource;
    ViewportEngine engine;
};

class ViewportControllerContext
{
public:
    ViewportControllerContext() = default;
    ViewportControllerContext(const ViewportControllerContext&) = delete;
    ViewportControllerContext& operator=(const ViewportControllerContext&) = delete;
    ViewportControllerContext(ViewportControllerContext&&) = delete;
    ViewportControllerContext& operator=(ViewportControllerContext&&) = delete;
    virtual ~ViewportControllerContext() = default;

    // Viewport geometry inputs.
    virtual QRectF contentRect() const;
    virtual QRectF visibleImageRect() const;
    virtual QRectF itemBounds() const;
    virtual double width() const;
    virtual double height() const;

    // Presentation-target assignment and active request facts.
    virtual bool hasActiveRequest() const;
    virtual bool hasReadyDisplay() const;
    virtual bool hasDisplayableSequence() const;
    virtual bool hasTimedSequence() const;
    virtual bool hasProviderSequence() const;
    virtual bool hasGenerationTerminalProviderFailure() const;

    // Primary provider construction/runtime facts.
    virtual bool providerHasCompleteKnownMetadata() const;
    virtual ImageSequenceProviderKnownFacts providerKnownFacts() const;
    virtual QSizeF providerKnownLogicalSize() const;
    virtual TimingIntervals providerKnownTimingIntervals() const;
    virtual ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const;
    virtual ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const;
    virtual ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const;
    virtual bool providerTimedPlaybackCapabilityKnownFalse() const;
    virtual bool providerFrameSeekCapabilityKnownFalse() const;
    virtual bool providerFrameSeekCapabilityKnownTrue() const;
    virtual bool providerPositionSeekCapabilityKnownFalse() const;
    virtual bool providerKnownFactsTimedFrameCount() const;
    virtual int providerKnownFactsFrameCount() const;
    virtual int providerFrameStartPosition(int frame) const;
    virtual int providerFrameIndexForPosition(int position) const;
    virtual ImageSequenceAuthoredAnimationFacts providerAuthoredAnimationFacts() const;

    // Primary built-in sequence facts and payload data.
    virtual int sequenceFrameCount() const;
    virtual int sequenceTotalDuration() const;
    virtual int sequenceFrameIndexForPosition(int position) const;
    virtual int sequenceFrameStartPosition(int frame) const;
    virtual ImageSequenceAuthoredAnimationFacts sequenceAuthoredAnimationFacts() const;
    virtual QSizeF sequenceLogicalSize() const;

    // Secondary built-in sequence facts and payload data.
    virtual bool hasSecondaryTimedSequence() const;
    virtual int secondarySequenceFrameCount() const;
    virtual int secondarySequenceTotalDuration() const;
    virtual int secondarySequenceFrameIndexForPosition(int position) const;
    virtual int secondarySequenceFrameStartPosition(int frame) const;
    virtual ImageSequenceAuthoredAnimationFacts secondarySequenceAuthoredAnimationFacts() const;
    virtual QSizeF secondarySequenceLogicalSize() const;

    // Secondary provider construction/runtime facts.
    virtual ImageSequenceProviderKnownFacts secondaryProviderKnownFacts() const;
    virtual QSizeF secondaryProviderKnownLogicalSize() const;
    virtual TimingIntervals secondaryProviderKnownTimingIntervals() const;
    virtual ImageSequenceProviderCapabilitySupport secondaryProviderTimedPlaybackCapability() const;
    virtual ImageSequenceProviderCapabilitySupport secondaryProviderFrameSeekCapability() const;
    virtual ImageSequenceProviderCapabilitySupport secondaryProviderPositionSeekCapability() const;
};

class ViewportControllerPort
{
public:
    ViewportControllerPort(
        const ViewportControllerContext& context, ViewportControllerState& state);

    ImageViewportInternal::DisplayState& displayState();
    const ImageViewportInternal::DisplayState& displayState() const;
    ImageViewportInternal::RequestState& requestState();
    const ImageViewportInternal::RequestState& requestState() const;
    ImageViewportInternal::ProviderGenerationState& providerState();
    const ImageViewportInternal::ProviderGenerationState& providerState() const;
    ImageViewportInternal::ProviderGenerationState& secondaryProviderState();
    const ImageViewportInternal::ProviderGenerationState& secondaryProviderState() const;
    ViewportEngine& engine();
    const ViewportEngine& engine() const;

    QRectF contentRect() const;
    QRectF visibleImageRect() const;
    QRectF itemBounds() const;
    bool hasActiveRequest() const;
    bool hasReadyDisplay() const;
    bool hasDisplayableSequence() const;
    bool hasTimedSequence() const;
    bool hasProviderSequence() const;
    bool hasGenerationTerminalProviderFailure() const;
    bool providerHasCompleteKnownMetadata() const;
    ImageSequenceProviderKnownFacts providerKnownFacts() const;
    QSizeF providerKnownLogicalSize() const;
    TimingIntervals providerKnownTimingIntervals() const;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const;
    bool providerTimedPlaybackCapabilityKnownFalse() const;
    bool providerFrameSeekCapabilityKnownFalse() const;
    bool providerFrameSeekCapabilityKnownTrue() const;
    bool providerPositionSeekCapabilityKnownFalse() const;
    bool providerKnownFactsTimedFrameCount() const;
    int providerKnownFactsFrameCount() const;
    int providerFrameStartPosition(int frame) const;
    int providerFrameIndexForPosition(int position) const;
    ImageSequenceAuthoredAnimationFacts providerAuthoredAnimationFacts() const;
    int sequenceFrameCount() const;
    int sequenceTotalDuration() const;
    int sequenceFrameIndexForPosition(int position) const;
    int sequenceFrameStartPosition(int frame) const;
    ImageSequenceAuthoredAnimationFacts sequenceAuthoredAnimationFacts() const;
    bool hasSecondaryTimedSequence() const;
    int secondarySequenceFrameCount() const;
    int secondarySequenceTotalDuration() const;
    int secondarySequenceFrameIndexForPosition(int position) const;
    int secondarySequenceFrameStartPosition(int frame) const;
    ImageSequenceAuthoredAnimationFacts secondarySequenceAuthoredAnimationFacts() const;
    ImageSequenceProviderKnownFacts secondaryProviderKnownFacts() const;
    QSizeF secondaryProviderKnownLogicalSize() const;
    TimingIntervals secondaryProviderKnownTimingIntervals() const;
    ImageSequenceProviderCapabilitySupport secondaryProviderTimedPlaybackCapability() const;
    ImageSequenceProviderCapabilitySupport secondaryProviderFrameSeekCapability() const;
    ImageSequenceProviderCapabilitySupport secondaryProviderPositionSeekCapability() const;
    QSizeF sequenceLogicalSize() const;
    QSizeF secondarySequenceLogicalSize() const;
    double width() const;
    double height() const;

private:
    const ViewportControllerContext& context;
    ViewportControllerState& state;
};

class ViewportController
{
public:
    explicit ViewportController(const ViewportControllerContext& context);

    const ImageViewportInternal::PresentationState& presentationState() const;
    const ImageViewportInternal::DisplayState& displayState() const;
    const ImageViewportInternal::RequestState& requestState() const;
    ImageViewportStateSnapshot stateSnapshot(double devicePixelRatio = 1.0) const;
    ImageViewportInternal::ViewportChangeSet publishChanges(
        ImageViewportInternal::ViewportChangeSet changes);
    ViewportEngine::PresentationTargetState presentationTargetState() const;
    bool hasProviderSession() const;
    bool hasProviderSession(ImageViewport::PageRole role) const;
    bool providerMetadataReady() const;
    bool secondaryProviderMetadataReady() const;
    bool secondaryProviderTimedMetadata() const;
    bool secondaryProviderTimedPlaybackSupported() const;
    bool secondaryProviderFrameSeekSupported() const;
    bool secondaryProviderPositionSeekSupported() const;
    QSizeF secondaryProviderLogicalSize() const;
    int secondaryProviderFrameCount() const;
    int secondaryProviderTotalDuration() const;
    ImageSequenceAuthoredAnimationFacts providerAuthoredAnimationFacts() const;
    bool providerTimedMetadata() const;
    bool providerTimedPlaybackSupported() const;
    bool providerFrameSeekSupported() const;
    bool providerPositionSeekSupported() const;
    QSizeF providerLogicalSize() const;
    int providerFrameCount() const;
    int providerTotalDuration() const;
    int providerFrameDuration(int frame) const;
    int providerFrameStartPosition(int frame) const;
    int providerFrameIndexForPosition(int position) const;
    ViewportMetadataProjection metadataProjection(ImageViewport::PageRole role) const;
    bool looping() const;
    PresentationGeometry::State geometryState(double devicePixelRatio = 1.0) const;
    PresentationGeometry::State geometryStateForItemBounds(
        const QRectF& itemBounds, double devicePixelRatio = 1.0) const;
    double minimumManualZoomPercent() const;
    double maximumManualZoomPercent(double devicePixelRatio = 1.0) const;
    double manualZoomStepFactor() const;
    double clampedManualZoomPercent(double percent, double devicePixelRatio = 1.0) const;
    double steppedManualZoomPercent(int stepCount, double devicePixelRatio = 1.0) const;
    void incrementDisplayRevision();
    void incrementRequestRevision();
    void incrementCommandRevision();
    void setCommandRevision(quint64 revision);
    void beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin origin,
        ImageViewportInternal::DisplayRequestTarget target, bool rememberAsLatestNonPlayback);
    void beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin origin,
        ImageViewportInternal::DisplayRequestTarget target,
        ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
        bool rememberAsLatestNonPlayback);
    void discardPendingRenderCommit();
    void setSecondaryActiveRequest(ImageViewportInternal::DisplayRequestTarget target,
        ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
        bool rememberAsLatestNonPlayback = false);
    void publishReadyDisplayState();
    void publishRenderWaitingState();
    void publishAcceptedTargetState();
    void publishAcceptedTargetState(const ImageViewportInternal::PreparedPayload& providerPayload);
    void publishProviderFrameLoadingState();
    void publishProviderFrameLoadingState(ImageViewport::PageRole role);
    void setPlaybackPhase(ViewportCommandResult& result, ImageViewport::PlaybackPhase phase);
    void setPlaybackPhase(
        ImageViewportInternal::ViewportChangeSet& changes, ImageViewport::PlaybackPhase phase);

    ViewportSequenceAssignmentResult assignSequence(ViewportSequenceAssignment assignment);
    ViewportCommandResult rejectInvalidCommand();
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewport::PageRole role);
    ViewportCommandResult clear();
    ViewportCommandResult play(ImageViewport::PageRole role);
    ViewportCommandResult pause(ImageViewport::PageRole role);
    ViewportCommandResult stop(ImageViewport::PageRole role);
    ViewportCommandResult seek(ImageViewport::PageRole role, int frame);
    ViewportCommandResult seekToPosition(ImageViewport::PageRole role, int milliseconds);
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
    quint64 installProviderSession(ImageSequenceProviderSession* session);
    quint64 installProviderSession(
        ImageViewport::PageRole role, ImageSequenceProviderSession* session);
    ImageSequenceProviderSession* takeProviderSession();
    ImageSequenceProviderSession* takeProviderSession(ImageViewport::PageRole role);
    ImageSequenceProviderSession* currentProviderSession() const;
    ImageSequenceProviderSession* currentProviderSession(ImageViewport::PageRole role) const;
    quint64 currentProviderGeneration() const;
    quint64 currentProviderGeneration(ImageViewport::PageRole role) const;
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
    ImageViewportInternal::ViewportChangeSet handleGeometryChanged(
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);
    ViewportRenderSynchronization beginRenderSynchronization(double devicePixelRatio = 1.0);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderCommit(
        const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
        const ViewportRenderSynchronization& synchronization);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderFailure(
        const ViewportRenderAcknowledgement& acknowledgement);
    int playbackTimerInterval() const;
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
    quint64 allocateRevisionToken();
    ImageViewportInternal::ViewportChangeSet applyPresentationTransition(
        const ControllerTransitionPolicy& policy, QPointF previousContentPosition,
        double previousZoomPercent);
    ViewportCommandResult applyAcceptedClearPresentationTarget(
        const ViewportEngine::PresentationTargetAssignmentResult& assignment);
    void publishLoadingWaitState(ImageViewportInternal::TargetSpreadWaitState waitState);
    void initializeSecondaryActiveRequest(ImageViewportInternal::DisplayRequestTarget target,
        ImageViewportInternal::ResolvedFrameIdentity resolvedFrame);
    void stageBuiltInPrimarySpreadPayload();
    void publishUploadPendingState();
    void publishPendingRenderState();
    void publishSequenceReadyState();
    void publishSequenceReadyState(const ImageViewportInternal::PreparedPayload& providerPayload);
    void publishStagedBuiltInPrimarySpreadReadyState();
    ViewportCommandResult rejectUnsupportedCommand();
    ViewportCommandResult rejectIgnoredNoRequestCommand();
    ViewportCommandResult playPrimary();
    ViewportCommandResult seekPrimary(int frame);
    ViewportCommandResult seekPrimaryToPosition(int milliseconds);
    ViewportCommandResult seekSecondaryBuiltIn(ImageViewportInternal::DisplayRequestTarget target,
        ImageViewportInternal::ResolvedFrameIdentity resolvedFrame);
    ViewportCommandResult seekSecondaryProvider(int frame);
    ViewportCommandResult seekSecondaryProviderToPosition(int milliseconds);
    bool targetSpreadTerminalSealedForActiveRequest();
    bool hasGenerationTerminalProviderFailure();
    void recordTargetSpreadTerminal(ImageViewport::PageRole role,
        ImageViewport::RequestStatus status, ImageViewport::RequestReason reason,
        ImageViewportInternal::FailureScope failureScope, const QString& diagnostic,
        ImageViewportInternal::ViewportChangeSet& changes);
    void armAuthoredAutoplayIfEligible();
    ViewportProviderFrameEventAcceptance acceptProviderFrameEvent(
        ImageViewport::PageRole role, ViewportProviderFrameEvent event);
    ViewportProviderFrameEventAcceptance acceptProviderFrameEvent(ViewportProviderFrameEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameAdmission(
        ImageViewport::PageRole role,
        const FramePreparation::ProviderFrameAdmissionResult& admission);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameAdmission(
        const FramePreparation::ProviderFrameAdmissionResult& admission);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameTerminalResult(
        ImageViewport::PageRole role, const ViewportProviderFrameTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameTerminalResult(
        const ViewportProviderFrameTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTerminalResult(
        ImageViewport::PageRole role, const ViewportProviderMetadataTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTerminalResult(
        const ViewportProviderMetadataTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataContradiction(
        const ViewportProviderMetadataContradiction& contradiction);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataContradiction(
        ImageViewport::PageRole role, const ViewportProviderMetadataContradiction& contradiction);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataAdmissionRejection(
        const ViewportProviderMetadataAdmissionRejection& rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataAdmissionRejection(
        ImageViewport::PageRole role, const ViewportProviderMetadataAdmissionRejection& rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTargetRejection(
        ViewportProviderMetadataTargetRejection rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTargetRejection(
        ImageViewport::PageRole role, ViewportProviderMetadataTargetRejection rejection);
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetSelection(
        ViewportProviderMetadataTargetSelection selection);
    ImageViewportInternal::ViewportChangeSet handleProviderEndOfSequenceProtocolViolation(
        ImageViewport::PageRole role, ViewportProviderEndOfSequenceProtocolViolation violation);
    ImageViewportInternal::ViewportChangeSet handleProviderEndOfSequenceProtocolViolation(
        ViewportProviderEndOfSequenceProtocolViolation violation);
    ViewportProviderEndOfSequenceResult handleProviderPlaybackEndOfSequence(
        ImageViewport::PageRole role);
    ViewportProviderEndOfSequenceResult handleProviderPlaybackEndOfSequence();
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ViewportProviderWaitingEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ImageViewport::PageRole role, ViewportProviderWaitingEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderWaiting();
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

    ViewportControllerState state;
    ViewportControllerPort viewport;
};
