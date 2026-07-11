#include "viewportcontroller_p.h"

#include "viewportcommandoutcome_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollergeometryhelpers_p.h"
#include "viewportcontrollerprovidercontract_p.h"

#include <limits>
#include <optional>
#include <utility>

ViewportControllerPort::ViewportControllerPort(
    std::function<QRectF()> captureItemBounds, ViewportControllerState& state)
    : captureItemBounds(std::move(captureItemBounds))
    , state(state)
{
}

ImageViewportInternal::DisplayState& ViewportControllerPort::displayState()
{
    return state.engine.displayState();
}

const ImageViewportInternal::DisplayState& ViewportControllerPort::displayState() const
{
    return state.engine.displayState();
}

ImageViewportInternal::RequestState& ViewportControllerPort::requestState()
{
    return state.engine.requestState();
}

const ImageViewportInternal::RequestState& ViewportControllerPort::requestState() const
{
    return state.engine.requestState();
}

ImageViewportInternal::ProviderGenerationState& ViewportControllerPort::providerState()
{
    return state.engine.providerState();
}

const ImageViewportInternal::ProviderGenerationState& ViewportControllerPort::providerState() const
{
    return state.engine.providerState();
}

ImageViewportInternal::ProviderGenerationState& ViewportControllerPort::secondaryProviderState()
{
    return state.engine.secondaryProviderState();
}

const ImageViewportInternal::ProviderGenerationState&
ViewportControllerPort::secondaryProviderState() const
{
    return state.engine.secondaryProviderState();
}

ViewportEngine& ViewportControllerPort::engine() { return state.engine; }

const ViewportEngine& ViewportControllerPort::engine() const { return state.engine; }

QRectF ViewportControllerPort::contentRect() const
{
    return PresentationGeometry::contentRect(state.engine.geometryState(
        controllerGeometryInput(*this, 1.0, std::nullopt,
            GeometryProjectionTarget::CurrentDisplay)));
}

QRectF ViewportControllerPort::visibleImageRect() const
{
    return PresentationGeometry::visibleImageRect(state.engine.geometryState(
        controllerGeometryInput(*this, 1.0, std::nullopt,
            GeometryProjectionTarget::CurrentDisplay)));
}

QRectF ViewportControllerPort::itemBounds() const
{
    return captureItemBounds ? captureItemBounds() : QRectF {};
}

bool ViewportControllerPort::hasActiveRequest() const
{
    return state.engine.requestState().status != ImageViewport::RequestStatus::NoRequest;
}

bool ViewportControllerPort::hasReadyDisplay() const
{
    return state.engine.displayState().hasReadyDisplay(hasDisplayableSequence());
}

bool ViewportControllerPort::hasDisplayableSequence() const
{
    return state.engine.requestState().sequenceSource.facts.present;
}

bool ViewportControllerPort::hasTimedSequence() const
{
    return state.engine.requestState().sequenceSource.facts.timed;
}

bool ViewportControllerPort::hasProviderSequence() const
{
    return state.engine.requestState().sequenceSource.facts.provider;
}

bool ViewportControllerPort::providerHasCompleteKnownMetadata() const
{
    return state.engine.requestState().sequenceSource.facts.hasCompleteProviderKnownMetadata;
}

ImageSequenceProviderKnownFacts ViewportControllerPort::providerKnownFacts() const
{
    return state.engine.requestState().sequenceSource.facts.providerKnownFacts;
}

QSizeF ViewportControllerPort::providerKnownLogicalSize() const
{
    return state.engine.requestState().sequenceSource.facts.providerKnownLogicalSize;
}

TimingIntervals ViewportControllerPort::providerKnownTimingIntervals() const
{
    return state.engine.requestState().sequenceSource.facts.providerKnownTimingIntervals;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::providerTimedPlaybackCapability() const
{
    return state.engine.requestState().sequenceSource.facts.providerTimedPlaybackCapability;
}

ImageSequenceProviderCapabilitySupport ViewportControllerPort::providerFrameSeekCapability() const
{
    return state.engine.requestState().sequenceSource.facts.providerFrameSeekCapability;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::providerPositionSeekCapability() const
{
    return state.engine.requestState().sequenceSource.facts.providerPositionSeekCapability;
}

bool ViewportControllerPort::providerTimedPlaybackCapabilityKnownFalse() const
{
    return ImageViewportInternal::providerCapabilityKnownFalse(providerTimedPlaybackCapability());
}

bool ViewportControllerPort::providerFrameSeekCapabilityKnownFalse() const
{
    return ImageViewportInternal::providerCapabilityKnownFalse(providerFrameSeekCapability());
}

bool ViewportControllerPort::providerFrameSeekCapabilityKnownTrue() const
{
    return ImageViewportInternal::providerCapabilityKnownTrue(providerFrameSeekCapability());
}

bool ViewportControllerPort::providerPositionSeekCapabilityKnownFalse() const
{
    return ImageViewportInternal::providerCapabilityKnownFalse(providerPositionSeekCapability());
}

bool ViewportControllerPort::providerKnownFactsTimedFrameCount() const
{
    return providerKnownFacts().isTimedFrameCount();
}

int ViewportControllerPort::providerKnownFactsFrameCount() const
{
    return providerKnownFactsTimedFrameCount() ? providerKnownFacts().frameCount() : 0;
}

int ViewportControllerPort::providerFrameStartPosition(int frame) const
{
    return state.engine.providerState().timedMetadata
        ? state.engine.providerState().timingIntervals.frameStartPosition(frame)
        : providerKnownTimingIntervals().frameStartPosition(frame);
}

int ViewportControllerPort::providerFrameIndexForPosition(int position) const
{
    return state.engine.providerState().timedMetadata
        ? state.engine.providerState().timingIntervals.frameIndexForPosition(position)
        : providerKnownTimingIntervals().frameIndexForPosition(position);
}

ImageSequenceAuthoredAnimationFacts ViewportControllerPort::providerAuthoredAnimationFacts() const
{
    return state.engine.providerState().authoredAnimationFacts;
}

int ViewportControllerPort::sequenceFrameCount() const
{
    return state.engine.requestState().sequenceSource.facts.frameCount;
}

int ViewportControllerPort::sequenceTotalDuration() const
{
    return state.engine.requestState().sequenceSource.facts.totalDuration;
}

int ViewportControllerPort::sequenceFrameIndexForPosition(int position) const
{
    return sourceFrameIndexForPosition(state.engine.requestState().sequenceSource, position);
}

int ViewportControllerPort::sequenceFrameStartPosition(int frame) const
{
    return sourceFrameStartPosition(state.engine.requestState().sequenceSource, frame);
}

ImageSequenceAuthoredAnimationFacts ViewportControllerPort::sequenceAuthoredAnimationFacts() const
{
    return state.engine.requestState().sequenceSource.facts.authoredAnimationFacts;
}

bool ViewportControllerPort::hasSecondaryTimedSequence() const
{
    return state.engine.requestState().secondarySequenceSource.facts.timed;
}

int ViewportControllerPort::secondarySequenceFrameCount() const
{
    return state.engine.requestState().secondarySequenceSource.facts.frameCount;
}

int ViewportControllerPort::secondarySequenceTotalDuration() const
{
    return state.engine.requestState().secondarySequenceSource.facts.totalDuration;
}

int ViewportControllerPort::secondarySequenceFrameIndexForPosition(int position) const
{
    return sourceFrameIndexForPosition(
        state.engine.requestState().secondarySequenceSource, position);
}

int ViewportControllerPort::secondarySequenceFrameStartPosition(int frame) const
{
    return sourceFrameStartPosition(state.engine.requestState().secondarySequenceSource, frame);
}

ImageSequenceAuthoredAnimationFacts
ViewportControllerPort::secondarySequenceAuthoredAnimationFacts() const
{
    return state.engine.requestState().secondarySequenceSource.facts.authoredAnimationFacts;
}

ImageSequenceProviderKnownFacts ViewportControllerPort::secondaryProviderKnownFacts() const
{
    return state.engine.requestState().secondarySequenceSource.facts.providerKnownFacts;
}

QSizeF ViewportControllerPort::secondaryProviderKnownLogicalSize() const
{
    return state.engine.requestState().secondarySequenceSource.facts.providerKnownLogicalSize;
}

TimingIntervals ViewportControllerPort::secondaryProviderKnownTimingIntervals() const
{
    return state.engine.requestState().secondarySequenceSource.facts.providerKnownTimingIntervals;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::secondaryProviderTimedPlaybackCapability() const
{
    return state.engine.requestState()
        .secondarySequenceSource.facts.providerTimedPlaybackCapability;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::secondaryProviderFrameSeekCapability() const
{
    return state.engine.requestState().secondarySequenceSource.facts.providerFrameSeekCapability;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::secondaryProviderPositionSeekCapability() const
{
    return state.engine.requestState().secondarySequenceSource.facts.providerPositionSeekCapability;
}

QSizeF ViewportControllerPort::sequenceLogicalSize() const
{
    return sourceLogicalSize(state.engine.requestState().sequenceSource);
}

QSizeF ViewportControllerPort::secondarySequenceLogicalSize() const
{
    return sourceLogicalSize(state.engine.requestState().secondarySequenceSource);
}

double ViewportControllerPort::width() const { return itemBounds().width(); }

double ViewportControllerPort::height() const { return itemBounds().height(); }

ViewportController::ViewportController(std::function<QRectF()> captureItemBounds)
    : viewport(std::move(captureItemBounds), state)
{
}

ImageViewportStateSnapshot ViewportController::stateSnapshot(double devicePixelRatio) const
{
    return state.engine.snapshot({ acceptedGeometryInput(viewport, devicePixelRatio),
        controllerGeometryInput(
            viewport, devicePixelRatio, std::nullopt, GeometryProjectionTarget::CurrentDisplay) });
}

ImageViewportInternal::ViewportChangeSet ViewportController::publishChanges(
    ImageViewportInternal::ViewportChangeSet changes)
{
    return state.engine.publishChanges(changes);
}


const ImageViewportInternal::PresentationState& ViewportController::presentationState() const
{
    return state.engine.presentationState();
}

const ImageViewportInternal::DisplayState& ViewportController::displayState() const
{
    return state.engine.displayState();
}

const ImageViewportInternal::RequestState& ViewportController::requestState() const
{
    return state.engine.requestState();
}

ViewportEngine::PresentationTargetState ViewportController::presentationTargetState() const
{
    return state.engine.presentationTargetState();
}

bool ViewportController::hasProviderSession() const
{
    return state.engine.providerState().sessionActive;
}

bool ViewportController::hasProviderSession(ImageViewport::PageRole role) const
{
    return role == ImageViewport::PageRole::Secondary
        ? state.engine.secondaryProviderState().sessionActive
        : hasProviderSession();
}

bool ViewportController::providerMetadataReady() const
{
    return state.engine.providerState().metadataReady;
}

bool ViewportController::secondaryProviderMetadataReady() const
{
    return state.engine.secondaryProviderState().metadataReady;
}

bool ViewportController::providerTimedMetadata() const
{
    return state.engine.providerState().timedMetadata;
}

ImageSequenceAuthoredAnimationFacts ViewportController::providerAuthoredAnimationFacts() const
{
    return state.engine.providerState().authoredAnimationFacts;
}

bool ViewportController::secondaryProviderTimedMetadata() const
{
    return state.engine.secondaryProviderState().timedMetadata;
}

bool ViewportController::providerTimedPlaybackSupported() const
{
    return state.engine.providerState().timedPlaybackSupport;
}

bool ViewportController::secondaryProviderTimedPlaybackSupported() const
{
    return state.engine.secondaryProviderState().timedPlaybackSupport;
}

bool ViewportController::providerFrameSeekSupported() const
{
    return state.engine.providerState().frameSeekSupport;
}

bool ViewportController::secondaryProviderFrameSeekSupported() const
{
    return state.engine.secondaryProviderState().frameSeekSupport;
}

bool ViewportController::providerPositionSeekSupported() const
{
    return state.engine.providerState().positionSeekSupport;
}

bool ViewportController::secondaryProviderPositionSeekSupported() const
{
    return state.engine.secondaryProviderState().positionSeekSupport;
}

QSizeF ViewportController::providerLogicalSize() const
{
    return state.engine.providerState().logicalSize;
}

QSizeF ViewportController::secondaryProviderLogicalSize() const
{
    return state.engine.secondaryProviderState().logicalSize;
}

int ViewportController::providerFrameCount() const
{
    return state.engine.providerState().timedMetadata
        ? state.engine.providerState().timingIntervals.frameCount()
        : 1;
}

int ViewportController::secondaryProviderFrameCount() const
{
    return state.engine.secondaryProviderState().timedMetadata
        ? state.engine.secondaryProviderState().timingIntervals.frameCount()
        : 1;
}

int ViewportController::providerTotalDuration() const
{
    return state.engine.providerState().timedMetadata
        ? state.engine.providerState().timingIntervals.totalDuration()
        : -1;
}

int ViewportController::secondaryProviderTotalDuration() const
{
    return state.engine.secondaryProviderState().timedMetadata
        ? state.engine.secondaryProviderState().timingIntervals.totalDuration()
        : -1;
}

int ViewportController::providerFrameDuration(int frame) const
{
    return state.engine.providerState().timedMetadata
        ? state.engine.providerState().timingIntervals.frameDuration(frame)
        : -1;
}

int ViewportController::providerFrameStartPosition(int frame) const
{
    return state.engine.providerState().timedMetadata
        ? state.engine.providerState().timingIntervals.frameStartPosition(frame)
        : -1;
}

int ViewportController::providerFrameIndexForPosition(int position) const
{
    return state.engine.providerState().timedMetadata
        ? state.engine.providerState().timingIntervals.frameIndexForPosition(position)
        : -1;
}

bool ViewportController::looping() const { return state.engine.requestState().looping; }

quint64 ViewportController::allocateRevisionToken() { return state.engine.allocateRevisionValue(); }

void ViewportController::incrementDisplayRevision()
{
    state.engine.displayState().revision = allocateRevisionToken();
}

void ViewportController::incrementRequestRevision()
{
    state.engine.requestState().requestRevision = allocateRevisionToken();
}

void ViewportController::incrementCommandRevision()
{
    state.engine.requestState().commandRevision = allocateRevisionToken();
}

void ViewportController::setCommandRevision(quint64 revision)
{
    state.engine.requestState().commandRevision = revision;
}

ViewportSequenceAssignmentResult ViewportController::assignSequence(
    ViewportSequenceAssignment assignment)
{
    if (assignment.presentationTarget.isClear()) {
        ImageSequence* const primary = assignment.source.sequence
            ? assignment.source.sequence
            : assignment.sequence;
        ImageSequence* const secondary = assignment.secondarySourceHandle.sequence
            ? assignment.secondarySourceHandle.sequence
            : assignment.secondarySequence;
        if (primary) {
            assignment.presentationTarget = ImageViewportPresentationTarget(primary, secondary);
        }
    }
    if (assignment.source.sequence && !assignment.source.facts.present) {
        assignment.source = ImageViewportInternal::makeImageSequenceSource(
            assignment.source.sequence, std::move(assignment.source.owner));
    }
    if (assignment.secondarySourceHandle.sequence
        && !assignment.secondarySourceHandle.facts.present) {
        assignment.secondarySourceHandle = ImageViewportInternal::makeImageSequenceSource(
            assignment.secondarySourceHandle.sequence,
            std::move(assignment.secondarySourceHandle.owner));
    }
    if (!assignment.source.sequence && assignment.sequence) {
        assignment.source = ImageViewportInternal::makeImageSequenceSource(assignment.sequence);
    }
    if (!assignment.secondarySourceHandle.sequence && assignment.secondarySequence) {
        assignment.secondarySourceHandle
            = ImageViewportInternal::makeImageSequenceSource(assignment.secondarySequence);
    }

    const auto engineResult = state.engine.assignPresentationTarget(
        { assignment.presentationTarget, assignment.transitionPolicy,
            std::move(assignment.source), std::move(assignment.secondarySourceHandle),
            controllerGeometryInput(viewport) });
    const ViewportCommandResult command
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(engineResult.command);
    ViewportSequenceAssignmentResult result;
    result.outcome = command.outcome;
    result.changes = command.changes;
    mergeChanges(result.changes, engineResult.changes);
    result.providerFrameTransport = engineResult.providerEffects[0];
    result.secondaryProviderFrameTransport = engineResult.providerEffects[1];
    result.openProviderSession = engineResult.openPrimaryProviderSession;
    result.openSecondaryProviderSession = engineResult.openSecondaryProviderSession;
    return result;
}

ViewportCommandResult ViewportController::clear()
{
    ViewportSequenceAssignmentResult assignment = assignSequence({});
    ViewportCommandResult result;
    result.outcome = assignment.outcome;
    result.changes = assignment.changes;
    result.providerFrameTransport = assignment.providerFrameTransport;
    result.secondaryProviderFrameTransport = assignment.secondaryProviderFrameTransport;
    result.playbackSchedule = state.engine.playbackScheduleEffect();
    return result;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::setNextProviderRequestTokenForTest(quint64 token)
{
    state.engine.providerState().nextRequestToken = token;
}

void ViewportController::setNextProviderRequestTokenForTest(
    ImageViewport::PageRole role, quint64 token)
{
    if (role == ImageViewport::PageRole::Secondary) {
        state.engine.secondaryProviderState().nextRequestToken = token;
        return;
    }

    setNextProviderRequestTokenForTest(token);
}

void ViewportController::setNextRevisionTokenForTest(quint64 token)
{
    state.engine.setNextRevisionValueForTest(token);
    state.engine.displayState().revision = 0;
    state.engine.requestState().requestRevision = 0;
    state.engine.requestState().commandRevision = 0;
}

bool ViewportController::hasPendingRenderCommitForTest() const
{
    return state.engine.displayState().pendingRenderPayload.commitPending;
}

quint64 ViewportController::activeRequestIdForTest() const
{
    return state.engine.requestState().activeRequest.identity.id;
}

quint64 ViewportController::displayedRequestIdForTest() const
{
    return state.engine.displayState().displayedRequest.request.identity.id;
}

quint64 ViewportController::pendingRenderGenerationForTest() const
{
    return state.engine.displayState().pendingRenderPayload.generation;
}

quint64 ViewportController::pendingRenderPayloadIdForTest() const
{
    return state.engine.displayState().pendingRenderPayload.payloadId;
}

quint64 ViewportController::secondaryPendingRenderPayloadIdForTest() const
{
    return state.engine.displayState().secondaryPendingRenderPayload.payloadId;
}

ImageViewportInternal::RenderFailureDiagnostic
ViewportController::lastAcceptedRenderFailureDiagnosticForTest() const
{
    return state.engine.requestState().lastAcceptedRenderFailure;
}
#endif
