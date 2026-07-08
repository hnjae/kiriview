#include "viewportcontroller_p.h"

#include "viewportcommandoutcome_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollergeometryhelpers_p.h"
#include "viewportcontrollerprovidercontract_p.h"

#include <limits>
#include <optional>
#include <utility>

namespace {
ImageViewportInternal::TargetSpreadRoleTerminalState& controllerTargetSpreadTerminalForRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? request.targetSpreadTerminal.primary
                                                    : request.targetSpreadTerminal.secondary;
}

const ImageViewportInternal::TargetSpreadRoleTerminalState* controllerTargetSpreadTerminalForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? &request.targetSpreadTerminal.primary
                                                    : &request.targetSpreadTerminal.secondary;
}

bool controllerTargetSpreadTerminalMatchesActiveRequest(ViewportControllerPort viewport)
{
    const auto& request = viewportRequestState(viewport);
    return request.targetSpreadTerminal.sealed
        && request.targetSpreadTerminal.generation == request.sequenceGeneration
        && request.targetSpreadTerminal.requestId == request.activeRequest.identity.id;
}

bool controllerTargetSpreadRequiresRole(
    ViewportControllerPort viewport, ImageViewport::PageRole role)
{
    return static_cast<bool>(sequenceForRole(viewportRequestState(viewport), role));
}

const ImageViewportInternal::TargetSpreadRoleTerminalState* controllerCurrentTerminalForRole(
    ViewportControllerPort viewport, ImageViewport::PageRole role)
{
    if (!controllerTargetSpreadTerminalMatchesActiveRequest(viewport)
        || !controllerTargetSpreadRequiresRole(viewport, role)) {
        return nullptr;
    }

    const ImageViewportInternal::RequestState& request = viewportRequestState(viewport);
    const auto* terminal = controllerTargetSpreadTerminalForRole(request, role);
    return terminal->terminal ? terminal : nullptr;
}

const ImageViewportInternal::TargetSpreadRoleTerminalState* controllerProjectedTargetSpreadTerminal(
    ViewportControllerPort viewport)
{
    const auto* primary
        = controllerCurrentTerminalForRole(viewport, ImageViewport::PageRole::Primary);
    const auto* secondary
        = controllerCurrentTerminalForRole(viewport, ImageViewport::PageRole::Secondary);
    if (primary && secondary) {
        if (primary->status == secondary->status) {
            return primary;
        }
        if (primary->status == ImageViewport::RequestStatus::Error) {
            return primary;
        }
        if (secondary->status == ImageViewport::RequestStatus::Error) {
            return secondary;
        }
        return primary;
    }
    return primary ? primary : secondary;
}

bool controllerTargetSpreadTerminalHasGenerationScope(ViewportControllerPort viewport)
{
    if (!controllerTargetSpreadTerminalMatchesActiveRequest(viewport)) {
        return false;
    }

    const auto& terminal = viewportRequestState(viewport).targetSpreadTerminal;
    return (terminal.primary.terminal
               && terminal.primary.failureScope == ImageViewportInternal::FailureScope::Generation)
        || (terminal.secondary.terminal
            && terminal.secondary.failureScope == ImageViewportInternal::FailureScope::Generation);
}

void publishControllerTargetSpreadTerminalProjection(
    ViewportControllerPort& viewport, ImageViewportInternal::ViewportChangeSet& changes)
{
    const auto* terminal = controllerProjectedTargetSpreadTerminal(viewport);
    if (!terminal) {
        return;
    }

    const bool diagnosticsValueChanged
        = viewportRequestState(viewport).errorString != terminal->diagnostic;
    viewportRequestState(viewport).status = terminal->status;
    viewportRequestState(viewport).reason = terminal->reason;
    viewportRequestState(viewport).errorString = terminal->diagnostic;
    markRequestMutation(changes);
    markDiagnosticsMutation(changes, diagnosticsValueChanged);
}

void clearControllerDisplayRequestTerminalForAcceptedRequest(ViewportControllerPort& viewport)
{
    if (!controllerTargetSpreadTerminalMatchesActiveRequest(viewport)
        || controllerTargetSpreadTerminalHasGenerationScope(viewport)) {
        return;
    }

    viewportRequestState(viewport).targetSpreadTerminal.clear();
}

bool controllerTargetRequiresSecondaryPayload(ViewportControllerPort& viewport)
{
    return hasSecondarySequence(viewport)
        && viewportRequestState(viewport).secondaryActiveRequest.target.frame >= 0;
}

void stageControllerBuiltInSecondaryPayload(ViewportControllerPort& viewport)
{
    if (!controllerTargetRequiresSecondaryPayload(viewport)
        || viewportRequestState(viewport).secondarySequenceIsProvider) {
        return;
    }

    ImageViewportInternal::PreparedPayload secondaryPayload;
    secondaryPayload.commitPending = true;
    secondaryPayload.generation = viewportRequestState(viewport).sequenceGeneration;
    secondaryPayload.requestId = viewportRequestState(viewport).activeRequest.identity.id;
    secondaryPayload.payloadId = ++viewportDisplayState(viewport).nextPreparedPayloadId;
    viewportRequestState(viewport).secondaryActiveRequest.preparedPayloadId
        = secondaryPayload.payloadId;
    const FramePreparation::BuiltInFrameAdmissionResult secondaryAdmission
        = FramePreparation::admitBuiltInFrame(
            viewportRequestState(viewport).secondarySequenceSource,
            viewportRequestState(viewport).secondaryActiveRequest.target.frame, secondaryPayload);
    viewportDisplayState(viewport).secondaryPendingRenderPayload
        = secondaryAdmission.preparedPayload;
}

void publishControllerSecondaryDisplayedRequest(ViewportControllerPort& viewport)
{
    if (!hasSecondarySequence(viewport)) {
        viewportDisplayState(viewport).secondaryDisplayedRequest = {};
        viewportDisplayState(viewport).secondaryDisplayedImageSize = {};
        viewportDisplayState(viewport).secondaryDisplayedImage = {};
        return;
    }

    ImageViewportInternal::DisplayRequestSnapshot snapshot;
    snapshot.generation = viewportRequestState(viewport).sequenceGeneration;
    snapshot.request = viewportRequestState(viewport).secondaryActiveRequest;
    const int displayedPosition
        = viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame.position;
    snapshot.request.target.position = displayedPosition;
    snapshot.request.resolvedFrame.position = displayedPosition;
    viewportDisplayState(viewport).secondaryDisplayedRequest = snapshot;
    viewportDisplayState(viewport).secondaryDisplayedImageSize
        = viewportRequestState(viewport).secondarySequenceIsProvider
        ? viewport.secondaryProviderState().logicalSize
        : viewport.secondarySequenceLogicalSize();
    if (!viewportRequestState(viewport).secondarySequenceIsProvider
        && !viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull()) {
        viewportDisplayState(viewport).secondaryDisplayedImage
            = viewportDisplayState(viewport).secondaryPendingRenderPayload.image;
    }
}

void stageControllerBuiltInPrimarySpreadPayload(ViewportControllerPort& viewport)
{
    viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
        viewport.hasDisplayableSequence());
    viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
    viewportDisplayState(viewport).beginPreparedPayloadIdentity(
        viewportRequestState(viewport).sequenceGeneration,
        viewportRequestState(viewport).activeRequest);
    const int frame = viewportRequestState(viewport).activeRequest.target.frame;
    if (frame >= 0) {
        const FramePreparation::BuiltInFrameAdmissionResult admission
            = FramePreparation::admitBuiltInFrame(viewportRequestState(viewport).sequenceSource,
                frame, viewportDisplayState(viewport).pendingRenderPayload);
        viewportDisplayState(viewport).pendingRenderPayload = admission.preparedPayload;
    }
    stageControllerBuiltInSecondaryPayload(viewport);
}

}

QRectF ViewportControllerContext::contentRect() const { return {}; }

QRectF ViewportControllerContext::visibleImageRect() const { return {}; }

QRectF ViewportControllerContext::itemBounds() const { return {}; }

bool ViewportControllerContext::hasActiveRequest() const { return false; }

bool ViewportControllerContext::hasReadyDisplay() const { return false; }

bool ViewportControllerContext::hasDisplayableSequence() const { return false; }

bool ViewportControllerContext::hasTimedSequence() const { return false; }

bool ViewportControllerContext::hasProviderSequence() const { return false; }

bool ViewportControllerContext::hasGenerationTerminalProviderFailure() const { return false; }

bool ViewportControllerContext::providerHasCompleteKnownMetadata() const { return false; }

ImageSequenceProviderKnownFacts ViewportControllerContext::providerKnownFacts() const { return {}; }

QSizeF ViewportControllerContext::providerKnownLogicalSize() const { return {}; }

TimingIntervals ViewportControllerContext::providerKnownTimingIntervals() const { return {}; }

ImageSequenceProviderCapabilitySupport
ViewportControllerContext::providerTimedPlaybackCapability() const
{
    return ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerContext::providerFrameSeekCapability() const
{
    return ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerContext::providerPositionSeekCapability() const
{
    return ImageSequenceProviderCapabilitySupport::Unavailable;
}

bool ViewportControllerContext::providerTimedPlaybackCapabilityKnownFalse() const { return false; }

bool ViewportControllerContext::providerFrameSeekCapabilityKnownFalse() const { return false; }

bool ViewportControllerContext::providerFrameSeekCapabilityKnownTrue() const { return false; }

bool ViewportControllerContext::providerPositionSeekCapabilityKnownFalse() const { return false; }

bool ViewportControllerContext::providerKnownFactsTimedFrameCount() const { return false; }

int ViewportControllerContext::providerKnownFactsFrameCount() const { return 0; }

int ViewportControllerContext::providerFrameStartPosition(int) const { return -1; }

int ViewportControllerContext::providerFrameIndexForPosition(int) const { return -1; }

ImageSequenceAuthoredAnimationFacts
ViewportControllerContext::providerAuthoredAnimationFacts() const
{
    return {};
}

int ViewportControllerContext::frameCount() const { return -1; }

int ViewportControllerContext::totalDuration() const { return -1; }

int ViewportControllerContext::sequenceFrameCount() const { return -1; }

int ViewportControllerContext::sequenceTotalDuration() const { return -1; }

int ViewportControllerContext::sequenceFrameIndexForPosition(int) const { return -1; }

int ViewportControllerContext::sequenceFrameStartPosition(int) const { return -1; }

ImageSequenceAuthoredAnimationFacts
ViewportControllerContext::sequenceAuthoredAnimationFacts() const
{
    return {};
}

bool ViewportControllerContext::hasSecondaryTimedSequence() const { return false; }

int ViewportControllerContext::secondarySequenceFrameCount() const { return -1; }

int ViewportControllerContext::secondarySequenceTotalDuration() const { return -1; }

int ViewportControllerContext::secondaryTotalDuration() const { return -1; }

int ViewportControllerContext::secondarySequenceFrameIndexForPosition(int) const { return -1; }

int ViewportControllerContext::secondarySequenceFrameStartPosition(int) const { return -1; }

ImageSequenceAuthoredAnimationFacts
ViewportControllerContext::secondarySequenceAuthoredAnimationFacts() const
{
    return {};
}

ImageSequenceProviderKnownFacts ViewportControllerContext::secondaryProviderKnownFacts() const
{
    return {};
}

QSizeF ViewportControllerContext::secondaryProviderKnownLogicalSize() const { return {}; }

TimingIntervals ViewportControllerContext::secondaryProviderKnownTimingIntervals() const
{
    return {};
}

ImageSequenceProviderCapabilitySupport
ViewportControllerContext::secondaryProviderTimedPlaybackCapability() const
{
    return ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerContext::secondaryProviderFrameSeekCapability() const
{
    return ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport
ViewportControllerContext::secondaryProviderPositionSeekCapability() const
{
    return ImageSequenceProviderCapabilitySupport::Unavailable;
}

QSizeF ViewportControllerContext::sequenceLogicalSize() const { return {}; }

QSizeF ViewportControllerContext::secondarySequenceLogicalSize() const { return {}; }

double ViewportControllerContext::width() const { return 0.0; }

double ViewportControllerContext::height() const { return 0.0; }

ViewportControllerPort::ViewportControllerPort(
    const ViewportControllerContext& context, ViewportControllerState& state)
    : context(context)
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

QRectF ViewportControllerPort::contentRect() const { return context.contentRect(); }

QRectF ViewportControllerPort::visibleImageRect() const { return context.visibleImageRect(); }

QRectF ViewportControllerPort::itemBounds() const { return context.itemBounds(); }

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

bool ViewportControllerPort::hasGenerationTerminalProviderFailure() const
{
    return context.hasGenerationTerminalProviderFailure();
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

int ViewportControllerPort::frameCount() const
{
    return hasProviderSequence() && state.engine.providerState().metadataReady
        ? (state.engine.providerState().timedMetadata
                  ? state.engine.providerState().timingIntervals.frameCount()
                  : 1)
        : sequenceFrameCount();
}

int ViewportControllerPort::totalDuration() const
{
    return hasProviderSequence() && state.engine.providerState().metadataReady
        ? (state.engine.providerState().timedMetadata
                  ? state.engine.providerState().timingIntervals.totalDuration()
                  : -1)
        : sequenceTotalDuration();
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

int ViewportControllerPort::secondaryTotalDuration() const
{
    return state.engine.requestState().secondarySequenceIsProvider
            && state.engine.secondaryProviderState().metadataReady
        ? (state.engine.secondaryProviderState().timedMetadata
                  ? state.engine.secondaryProviderState().timingIntervals.totalDuration()
                  : -1)
        : secondarySequenceTotalDuration();
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

double ViewportControllerPort::width() const { return context.width(); }

double ViewportControllerPort::height() const { return context.height(); }

ViewportController::ViewportController(const ViewportControllerContext& context)
    : viewport(context, state)
{
}

bool ViewportController::targetSpreadTerminalSealedForActiveRequest()
{
    return controllerTargetSpreadTerminalMatchesActiveRequest(viewport);
}

bool ViewportController::hasGenerationTerminalProviderFailure()
{
    return viewport.hasGenerationTerminalProviderFailure()
        || controllerTargetSpreadTerminalHasGenerationScope(viewport);
}

void ViewportController::recordTargetSpreadTerminal(ImageViewport::PageRole role,
    ImageViewport::RequestStatus status, ImageViewport::RequestReason reason,
    ImageViewportInternal::FailureScope failureScope, const QString& diagnostic,
    ImageViewportInternal::ViewportChangeSet& changes)
{
    auto& request = viewportRequestState(viewport);
    auto& terminal = request.targetSpreadTerminal;
    if (terminal.generation != request.sequenceGeneration
        || terminal.requestId != request.activeRequest.identity.id) {
        terminal.clear();
        terminal.generation = request.sequenceGeneration;
        terminal.requestId = request.activeRequest.identity.id;
    }

    terminal.sealed = true;
    auto& roleTerminal = controllerTargetSpreadTerminalForRole(request, role);
    roleTerminal.terminal = true;
    roleTerminal.status = status;
    roleTerminal.reason = reason;
    roleTerminal.failureScope = failureScope;
    roleTerminal.diagnostic = diagnostic;
    publishControllerTargetSpreadTerminalProjection(viewport, changes);
}

void ViewportController::publishLoadingWaitState(
    ImageViewportInternal::TargetSpreadWaitState waitState)
{
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewportInternal::projectWaitReason(waitState);
    publishRetainedOrEmptyDisplayStatus(viewport);
}

void ViewportController::beginAcceptedDisplayRequest(
    ImageViewportInternal::DisplayRequestOrigin origin, DisplayRequestTarget target,
    bool rememberAsLatestNonPlayback)
{
    viewportRequestState(viewport).beginDisplayRequest(origin, target, rememberAsLatestNonPlayback);
}

void ViewportController::beginAcceptedDisplayRequest(
    ImageViewportInternal::DisplayRequestOrigin origin, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame, bool rememberAsLatestNonPlayback)
{
    viewportRequestState(viewport).beginDisplayRequest(
        origin, target, resolvedFrame, rememberAsLatestNonPlayback);
}

void ViewportController::discardPendingRenderCommit()
{
    viewportDisplayState(viewport).clearPendingRenderPayload();
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
}

void ViewportController::setSecondaryActiveRequest(DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame, bool rememberAsLatestNonPlayback)
{
    viewportRequestState(viewport).secondaryActiveRequest.identity
        = viewportRequestState(viewport).activeRequest.identity;
    viewportRequestState(viewport).secondaryActiveRequest.target = target;
    viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame = resolvedFrame;
    viewportRequestState(viewport).secondaryActiveRequest.providerFrameToken = {};
    viewportRequestState(viewport).secondaryActiveRequest.preparedPayloadId
        = viewportRequestState(viewport).activeRequest.preparedPayloadId;
    if (rememberAsLatestNonPlayback && target.frame >= 0) {
        viewportRequestState(viewport).secondaryLatestNonPlaybackRequest
            = viewportRequestState(viewport).secondaryActiveRequest;
    }
}

void ViewportController::initializeSecondaryActiveRequest(
    DisplayRequestTarget target, ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    setSecondaryActiveRequest(target, resolvedFrame, true);
}

void ViewportController::publishReadyDisplayState()
{
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Ready;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::Ready;
    viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Ready;
}

void ViewportController::stageBuiltInPrimarySpreadPayload()
{
    stageControllerBuiltInPrimarySpreadPayload(viewport);
}

void ViewportController::publishRenderWaitingState()
{
    ImageViewportInternal::TargetSpreadWaitState waitState;
    waitState.requiresSecondary = controllerTargetRequiresSecondaryPayload(viewport);
    waitState.primary.renderWaiting = true;
    if (waitState.requiresSecondary) {
        waitState.secondary.renderWaiting = true;
    }
    publishLoadingWaitState(waitState);
}

void ViewportController::publishUploadPendingState()
{
    ImageViewportInternal::TargetSpreadWaitState waitState;
    waitState.requiresSecondary = controllerTargetRequiresSecondaryPayload(viewport);
    waitState.primary.uploadPending = true;
    if (waitState.requiresSecondary) {
        waitState.secondary.uploadPending = true;
    }
    publishLoadingWaitState(waitState);
}

void ViewportController::publishPendingRenderState()
{
    if (viewport.itemBounds().isEmpty()) {
        publishRenderWaitingState();
    } else {
        publishUploadPendingState();
    }
}

void ViewportController::publishSequenceReadyState()
{
    viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
        viewport.hasDisplayableSequence());
    publishReadyDisplayState();
    viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
    viewportDisplayState(viewport).beginPreparedPayloadIdentity(
        viewportRequestState(viewport).sequenceGeneration,
        viewportRequestState(viewport).activeRequest);
    int displayedPosition = -1;
    const int currentFrame = viewportRequestState(viewport).activeRequest.resolvedFrame.frame;
    if (viewport.hasProviderSequence()) {
        displayedPosition = viewportRequestState(viewport).activeRequest.resolvedFrame.position >= 0
            ? viewportRequestState(viewport).activeRequest.resolvedFrame.position
            : viewport.providerFrameStartPosition(currentFrame);
    } else {
        displayedPosition = viewportRequestState(viewport).activeRequest.resolvedFrame.position >= 0
            ? viewportRequestState(viewport).activeRequest.resolvedFrame.position
            : viewport.hasTimedSequence() ? viewport.sequenceFrameStartPosition(currentFrame)
                                          : -1;
    }
    viewportDisplayState(viewport).displayedRequest
        = viewportDisplayState(viewport).activeRequestSnapshot(
            viewportRequestState(viewport).sequenceGeneration,
            viewportRequestState(viewport).activeRequest, displayedPosition);
    viewportDisplayState(viewport).displayedImageSize = viewport.hasProviderSequence()
        ? viewportProviderState(viewport).logicalSize
        : viewport.sequenceLogicalSize();
    if (!viewportDisplayState(viewport).pendingRenderPayload.image.isNull()) {
        viewportDisplayState(viewport).displayedImage
            = viewportDisplayState(viewport).pendingRenderPayload.image;
    }
    viewportDisplayState(viewport).pendingRenderPayload.image = {};
    publishControllerSecondaryDisplayedRequest(viewport);
}

void ViewportController::publishStagedBuiltInPrimarySpreadReadyState()
{
    publishReadyDisplayState();
    const int currentFrame = viewportRequestState(viewport).activeRequest.resolvedFrame.frame;
    const int displayedPosition
        = viewportRequestState(viewport).activeRequest.resolvedFrame.position >= 0
        ? viewportRequestState(viewport).activeRequest.resolvedFrame.position
        : viewport.hasTimedSequence() ? viewport.sequenceFrameStartPosition(currentFrame)
                                      : -1;
    viewportDisplayState(viewport).displayedRequest
        = viewportDisplayState(viewport).activeRequestSnapshot(
            viewportRequestState(viewport).sequenceGeneration,
            viewportRequestState(viewport).activeRequest, displayedPosition);
    viewportDisplayState(viewport).displayedImageSize = viewport.sequenceLogicalSize();
    viewportDisplayState(viewport).displayedImage
        = viewportDisplayState(viewport).pendingRenderPayload.image;
    publishControllerSecondaryDisplayedRequest(viewport);
}

void ViewportController::publishAcceptedTargetState()
{
    clearControllerDisplayRequestTerminalForAcceptedRequest(viewport);
    stageControllerBuiltInPrimarySpreadPayload(viewport);
    publishPendingRenderState();
}

void ViewportController::publishSequenceReadyState(
    const ImageViewportInternal::PreparedPayload& providerPayload)
{
    if (!viewport.hasProviderSequence() || providerPayload.image.isNull()) {
        publishSequenceReadyState();
        return;
    }

    viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
        viewport.hasDisplayableSequence());
    publishReadyDisplayState();
    viewportDisplayState(viewport).commitPreparedPayloadIdentity(
        viewportRequestState(viewport).activeRequest, providerPayload);
    viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
    const int currentFrame = viewportRequestState(viewport).activeRequest.resolvedFrame.frame;
    viewportDisplayState(viewport).displayedRequest
        = viewportDisplayState(viewport).activeRequestSnapshot(
            viewportRequestState(viewport).sequenceGeneration,
            viewportRequestState(viewport).activeRequest,
            viewportRequestState(viewport).activeRequest.resolvedFrame.position >= 0
                ? viewportRequestState(viewport).activeRequest.resolvedFrame.position
                : viewport.providerFrameStartPosition(currentFrame));
    viewportDisplayState(viewport).displayedImageSize = viewportProviderState(viewport).logicalSize;
    viewportDisplayState(viewport).displayedImage = providerPayload.image;
    viewportDisplayState(viewport).pendingRenderPayload.image = {};
    publishControllerSecondaryDisplayedRequest(viewport);
}

void ViewportController::publishAcceptedTargetState(
    const ImageViewportInternal::PreparedPayload& providerPayload)
{
    if (viewport.hasProviderSequence() && !providerPayload.image.isNull()) {
        viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
            viewport.hasDisplayableSequence());
        viewportDisplayState(viewport).commitPreparedPayloadIdentity(
            viewportRequestState(viewport).activeRequest, providerPayload);
        stageControllerBuiltInSecondaryPayload(viewport);
        if (viewport.itemBounds().isEmpty()) {
            publishRenderWaitingState();
        } else {
            publishUploadPendingState();
            viewportDisplayState(viewport).pendingRenderPayload.commitPending = false;
        }
        viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
        return;
    }
    publishAcceptedTargetState();
}

void ViewportController::publishProviderFrameLoadingState()
{
    publishProviderFrameLoadingState(ImageViewport::PageRole::Primary);
}

void ViewportController::publishProviderFrameLoadingState(ImageViewport::PageRole role)
{
    clearControllerDisplayRequestTerminalForAcceptedRequest(viewport);
    ImageViewportInternal::TargetSpreadWaitState waitState;
    if (role == ImageViewport::PageRole::Secondary) {
        waitState.requiresSecondary = true;
        waitState.secondary.providerWaiting = true;
    } else {
        waitState.primary.providerWaiting = true;
    }
    publishLoadingWaitState(waitState);
    discardPendingRenderCommit();
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

ViewportEngine::PageSetState ViewportController::pageSetState() const
{
    return state.engine.pageSetState();
}

bool ViewportController::hasProviderSession() const
{
    return state.engine.providerState().session != nullptr;
}

bool ViewportController::hasProviderSession(ImageViewport::PageRole role) const
{
    return role == ImageViewport::PageRole::Secondary
        ? state.engine.secondaryProviderState().session != nullptr
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

ImageViewportInternal::ViewportChangeSet ViewportController::setLooping(bool looping)
{
    if (state.engine.requestState().looping == looping) {
        return {};
    }

    state.engine.requestState().looping = looping;
    return {};
}

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
    ViewportSequenceAssignmentResult result;
    if (assignment.pageSet.isClear()) {
        ImageSequence* const primarySequence
            = assignment.source.sequence ? assignment.source.sequence : assignment.sequence;
        ImageSequence* const secondarySequence = assignment.secondarySourceHandle.sequence
            ? assignment.secondarySourceHandle.sequence
            : assignment.secondarySequence;
        if (primarySequence) {
            assignment.pageSet = ImageViewportPageSet(primarySequence, secondarySequence);
        }
    }

    const ViewportEngine::PageSetAssignmentResult engineAssignment
        = state.engine.assignPageSet({ assignment.pageSet, assignment.transitionPolicy });
    if (engineAssignment.command.outcome != ImageViewport::CommandOutcome::Accepted) {
        const ViewportCommandResult commandResult
            = ImageViewportInternal::CommandOutcome::fromEngineCommand(
                viewport, engineAssignment.command);
        result.outcome = commandResult.outcome;
        result.changes = commandResult.changes;
        return result;
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
    const std::optional<ControllerTransitionPolicy> transitionPolicy
        = normalizeControllerTransitionPolicy(assignment.transitionPolicy);
    Q_ASSERT(transitionPolicy.has_value());
    const bool retainDisplay = transitionPolicy->displayTransition
        == PageSetTransitionPolicy::DisplayTransition::RetainPrevious;

    if (engineAssignment.clear) {
        const ViewportCommandResult clearResult = applyAcceptedClearPageSet(engineAssignment);
        result.outcome = clearResult.outcome;
        result.changes = clearResult.changes;
        result.providerFrameTransport = clearResult.providerFrameTransport;
        result.secondaryProviderFrameTransport = clearResult.secondaryProviderFrameTransport;
        return result;
    }

    result.providerFrameTransport = closeProviderSession();
    result.secondaryProviderFrameTransport
        = closeProviderSession(ImageViewport::PageRole::Secondary);

    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;
    const ImageViewport::PlaybackPhase oldPlaybackPhase
        = viewportRequestState(viewport).playbackPhase;
    const QString oldErrorString = viewportRequestState(viewport).errorString;
    const QString oldWarningString = viewportRequestState(viewport).warningString;
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const QPointF previousContentPosition
        = controllerContentPosition(viewport, state.engine.presentationState());
    const double previousZoomPercent
        = effectiveZoomPercent(controllerGeometryState(viewport, state.engine.presentationState()));
    ImageViewportInternal::ViewportChangeSet transitionChanges;

    const ViewportSequenceRoleSource secondarySource
        = resolvedSecondarySource(viewport, assignment);
    const DisplayRequestTarget secondaryInitialTarget = initialTargetForRoleSource(secondarySource);
    const ImageViewportInternal::ResolvedFrameIdentity secondaryInitialResolvedFrame
        = resolvedFrameForRoleSource(secondarySource);

    viewportRequestState(viewport).sequenceSource = std::move(assignment.source);
    viewportRequestState(viewport).sequence
        = viewportRequestState(viewport).sequenceSource.sequence;
    viewportRequestState(viewport).secondarySequenceSource
        = std::move(assignment.secondarySourceHandle);
    viewportRequestState(viewport).secondarySequence
        = viewportRequestState(viewport).secondarySequenceSource.sequence;
    viewportRequestState(viewport).secondarySequenceIsProvider = secondarySource.provider;
    state.secondarySource = secondarySource;
    viewportRequestState(viewport).sequenceGeneration = engineAssignment.pageSetState.generation;
    viewportRequestState(viewport).clearDisplayRequests();
    viewportDisplayState(viewport).nextPreparedPayloadId = 0;
    viewportDisplayState(viewport).clearPendingRenderPayload();
    if (!retainDisplay) {
        viewportDisplayState(viewport).clearDisplayedDisplay();
        viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    }
    viewportRequestState(viewport).errorString.clear();
    viewportRequestState(viewport).warningString.clear();
    viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportProviderState(viewport).metadataReady = false;
    viewportProviderState(viewport).timedMetadata = false;
    viewportProviderState(viewport).timedPlaybackSupport = false;
    viewportProviderState(viewport).frameSeekSupport = false;
    viewportProviderState(viewport).positionSeekSupport = false;
    viewportProviderState(viewport).authoredAnimationFacts = viewport.hasProviderSequence()
        ? viewportRequestState(viewport).sequenceSource.facts.authoredAnimationFacts
        : ImageSequenceAuthoredAnimationFacts {};
    viewportProviderState(viewport).logicalSize = {};
    viewportProviderState(viewport).timingIntervals = {};
    discardPendingRenderCommit();
    viewportProviderState(viewport).activeMetadataToken = {};
    viewportProviderState(viewport).activeFrameToken = {};
    state.engine.secondaryProviderState().metadataReady = false;
    state.engine.secondaryProviderState().timedMetadata = false;
    state.engine.secondaryProviderState().timedPlaybackSupport = false;
    state.engine.secondaryProviderState().frameSeekSupport = false;
    state.engine.secondaryProviderState().positionSeekSupport = false;
    state.engine.secondaryProviderState().authoredAnimationFacts = secondarySource.provider
        ? secondarySource.authoredAnimationFacts
        : ImageSequenceAuthoredAnimationFacts {};
    state.engine.secondaryProviderState().logicalSize = {};
    state.engine.secondaryProviderState().timingIntervals = {};
    state.engine.secondaryProviderState().activeMetadataToken = {};
    state.engine.secondaryProviderState().activeFrameToken = {};

    if (viewport.hasProviderSequence()) {
        if (viewport.providerHasCompleteKnownMetadata()) {
            const ImageSequenceProviderKnownFacts knownFacts = viewport.providerKnownFacts();
            viewportProviderState(viewport).metadataReady = true;
            viewportProviderState(viewport).timedMetadata = knownFacts.isTimedFrameList();
            viewportProviderState(viewport).timedPlaybackSupport
                = ImageViewportInternal::providerResolvedCapability(
                    viewport.providerTimedPlaybackCapability(),
                    viewportProviderState(viewport).timedMetadata);
            viewportProviderState(viewport).frameSeekSupport
                = ImageViewportInternal::providerResolvedCapability(
                    viewport.providerFrameSeekCapability(), true);
            viewportProviderState(viewport).positionSeekSupport
                = ImageViewportInternal::providerResolvedCapability(
                    viewport.providerPositionSeekCapability(),
                    viewportProviderState(viewport).timedMetadata);
            viewportProviderState(viewport).logicalSize = viewport.providerKnownLogicalSize();
            viewportProviderState(viewport).timingIntervals
                = viewportProviderState(viewport).timedMetadata
                ? viewport.providerKnownTimingIntervals()
                : TimingIntervals();
            const DisplayRequestTarget initialTarget {
                0,
                viewportProviderState(viewport).timedMetadata ? 0 : -1,
                ImageViewportInternal::ProviderRequestTargetKind::Frame,
            };
            viewportRequestState(viewport).beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, initialTarget, true);
            viewportRequestState(viewport).playbackPosition = initialTarget.position;
            initializeSecondaryActiveRequest(secondaryInitialTarget, secondaryInitialResolvedFrame);
        } else {
            const DisplayRequestTarget initialTarget {
                -1,
                -1,
                ImageViewportInternal::ProviderRequestTargetKind::Unknown,
            };
            viewportRequestState(viewport).beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, initialTarget, true);
            viewportRequestState(viewport).playbackPosition = initialTarget.position;
            initializeSecondaryActiveRequest(secondaryInitialTarget, secondaryInitialResolvedFrame);
        }
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        publishRetainedOrEmptyDisplayStatus(viewport);
        result.openProviderSession = true;
    } else if (viewport.hasDisplayableSequence()) {
        const DisplayRequestTarget initialTarget {
            0,
            viewport.hasTimedSequence() ? 0 : -1,
            ImageViewportInternal::ProviderRequestTargetKind::Unknown,
        };
        viewportRequestState(viewport).beginDisplayRequest(
            ImageViewportInternal::DisplayRequestOrigin::Initial, initialTarget, true);
        viewportRequestState(viewport).playbackPosition = initialTarget.position;
        initializeSecondaryActiveRequest(secondaryInitialTarget, secondaryInitialResolvedFrame);
        if (secondarySource.provider) {
            stageBuiltInPrimarySpreadPayload();
        } else {
            publishAcceptedTargetState();
        }
    } else {
        viewportDisplayState(viewport).clearDisplayedDisplay();
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::NoRequest;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::NoRequest;
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
        viewportDisplayState(viewport).clearPendingRenderPayload();
        viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    }

    if (secondarySource.provider) {
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        viewportDisplayState(viewport).status = retainDisplay
            ? retainedOrEmptyDisplayStatus(viewport)
            : ImageViewport::DisplayStatus::Empty;
        result.openSecondaryProviderSession = true;
    }

    transitionChanges = applyPresentationTransition(
        *transitionPolicy, previousContentPosition, previousZoomPercent);

    armAuthoredAutoplayIfEligible();

    markRequestMutation(result.changes);
    const bool displayValueChanged = viewportDisplayState(viewport).status != oldDisplayStatus
        || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
    result.changes.displayRevision = displayValueChanged;
    result.changes.displayState = displayValueChanged;
    result.changes.geometryState
        = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
    result.changes.playbackPhase = viewportRequestState(viewport).playbackPhase != oldPlaybackPhase;
    result.changes.diagnostics = viewportRequestState(viewport).errorString != oldErrorString
        || viewportRequestState(viewport).warningString != oldWarningString;
    result.changes.scheduleUpdate = true;
    mergeChanges(result.changes, transitionChanges);
    return result;
}

ViewportCommandResult ViewportController::rejectInvalidCommand()
{
    return ImageViewportInternal::CommandOutcome::invalid(viewport);
}

ViewportCommandResult ViewportController::rejectUnsupportedCommand()
{
    return ImageViewportInternal::CommandOutcome::unsupported(viewport);
}

ViewportCommandResult ViewportController::rejectIgnoredNoRequestCommand()
{
    return ImageViewportInternal::CommandOutcome::ignoredNoRequest(viewport);
}

ViewportCommandResult ViewportController::clear()
{
    return applyAcceptedClearPageSet(
        state.engine.assignPageSet({ ImageViewportPageSet::clear(), {} }));
}

ViewportCommandResult ViewportController::applyAcceptedClearPageSet(
    const ViewportEngine::PageSetAssignmentResult& assignment)
{
    ViewportCommandResult result;
    result.outcome = assignment.command.outcome;
    const bool requestChanged = viewport.hasActiveRequest()
        || viewportRequestState(viewport).sequence
        || viewportRequestState(viewport).secondarySequence;
    const bool displayChanged
        = viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Empty
        || viewportDisplayState(viewport).displayedImageSize.isValid();
    const bool playbackChanged
        = viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Stopped;
    const bool diagnosticsValueChanged = !viewportRequestState(viewport).errorString.isEmpty()
        || !viewportRequestState(viewport).warningString.isEmpty();
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const bool hasProviderSession = viewportProviderState(viewport).session != nullptr;
    result.providerFrameTransport.sessionClose = handleProviderSessionClose();
    result.providerFrameTransport.closeSession = hasProviderSession;
    result.secondaryProviderFrameTransport
        = closeProviderSession(ImageViewport::PageRole::Secondary);
    viewportRequestState(viewport).sequenceSource = {};
    viewportRequestState(viewport).sequence = nullptr;
    viewportRequestState(viewport).secondarySequenceSource = {};
    viewportRequestState(viewport).secondarySequence = nullptr;
    viewportRequestState(viewport).secondarySequenceIsProvider = false;
    state.secondarySource = {};
    if (assignment.pageSetChanged) {
        viewportRequestState(viewport).sequenceGeneration = assignment.pageSetState.generation;
    }
    viewportRequestState(viewport).clearDisplayRequests();
    viewportDisplayState(viewport).clearDisplayedDisplay();
    viewportDisplayState(viewport).nextPreparedPayloadId = 0;
    viewportDisplayState(viewport).clearPendingRenderPayload();
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::NoRequest;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::NoRequest;
    viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
    viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportProviderState(viewport).metadataReady = false;
    viewportProviderState(viewport).timedMetadata = false;
    viewportProviderState(viewport).authoredAnimationFacts = {};
    viewportProviderState(viewport).logicalSize = {};
    viewportProviderState(viewport).timingIntervals = {};
    viewportProviderState(viewport).activeMetadataToken = {};
    viewportProviderState(viewport).activeFrameToken = {};
    state.engine.secondaryProviderState().metadataReady = false;
    state.engine.secondaryProviderState().timedMetadata = false;
    state.engine.secondaryProviderState().timedPlaybackSupport = false;
    state.engine.secondaryProviderState().frameSeekSupport = false;
    state.engine.secondaryProviderState().positionSeekSupport = false;
    state.engine.secondaryProviderState().authoredAnimationFacts = {};
    state.engine.secondaryProviderState().logicalSize = {};
    state.engine.secondaryProviderState().timingIntervals = {};
    state.engine.secondaryProviderState().activeMetadataToken = {};
    state.engine.secondaryProviderState().activeFrameToken = {};
    viewportRequestState(viewport).errorString.clear();
    viewportRequestState(viewport).warningString.clear();
    ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
    result.changes.requestRevision = requestChanged;
    result.changes.displayRevision = displayChanged;
    result.changes.requestState = requestChanged;
    result.changes.displayState = displayChanged;
    result.changes.geometryState
        = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
    result.changes.playbackPhase = playbackChanged;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
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
