#include "viewportcontroller_p.h"

#include "viewportcontrollerhelpers_p.h"

#include "imageviewportproviderfacts_p.h"
#include "imageviewportvalidation_p.h"
#include "playbacktimeline_p.h"
#include "presentationgeometry_p.h"
#include "viewportgeometryhelpers_p.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

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

QImage ViewportControllerContext::sequenceFrameImage(int) const { return {}; }

QImage ViewportControllerContext::secondarySequenceFrameImage(int) const { return {}; }

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
    return state.display;
}

const ImageViewportInternal::DisplayState& ViewportControllerPort::displayState() const
{
    return state.display;
}

ImageViewportInternal::RequestState& ViewportControllerPort::requestState()
{
    return state.request;
}

const ImageViewportInternal::RequestState& ViewportControllerPort::requestState() const
{
    return state.request;
}

ImageViewportInternal::ProviderGenerationState& ViewportControllerPort::providerState()
{
    return state.provider;
}

const ImageViewportInternal::ProviderGenerationState& ViewportControllerPort::providerState() const
{
    return state.provider;
}

ImageViewportInternal::ProviderGenerationState& ViewportControllerPort::secondaryProviderState()
{
    return state.secondaryProvider;
}

const ImageViewportInternal::ProviderGenerationState&
ViewportControllerPort::secondaryProviderState() const
{
    return state.secondaryProvider;
}

QRectF ViewportControllerPort::contentRect() const { return context.contentRect(); }

QRectF ViewportControllerPort::visibleImageRect() const { return context.visibleImageRect(); }

QRectF ViewportControllerPort::itemBounds() const { return context.itemBounds(); }

bool ViewportControllerPort::hasActiveRequest() const { return context.hasActiveRequest(); }

bool ViewportControllerPort::hasReadyDisplay() const { return context.hasReadyDisplay(); }

bool ViewportControllerPort::hasDisplayableSequence() const
{
    return context.hasDisplayableSequence();
}

bool ViewportControllerPort::hasTimedSequence() const { return context.hasTimedSequence(); }

bool ViewportControllerPort::hasProviderSequence() const { return context.hasProviderSequence(); }

bool ViewportControllerPort::hasGenerationTerminalProviderFailure() const
{
    return context.hasGenerationTerminalProviderFailure();
}

bool ViewportControllerPort::providerHasCompleteKnownMetadata() const
{
    return context.providerHasCompleteKnownMetadata();
}

ImageSequenceProviderKnownFacts ViewportControllerPort::providerKnownFacts() const
{
    return context.providerKnownFacts();
}

QSizeF ViewportControllerPort::providerKnownLogicalSize() const
{
    return context.providerKnownLogicalSize();
}

TimingIntervals ViewportControllerPort::providerKnownTimingIntervals() const
{
    return context.providerKnownTimingIntervals();
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::providerTimedPlaybackCapability() const
{
    return context.providerTimedPlaybackCapability();
}

ImageSequenceProviderCapabilitySupport ViewportControllerPort::providerFrameSeekCapability() const
{
    return context.providerFrameSeekCapability();
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::providerPositionSeekCapability() const
{
    return context.providerPositionSeekCapability();
}

bool ViewportControllerPort::providerTimedPlaybackCapabilityKnownFalse() const
{
    return context.providerTimedPlaybackCapabilityKnownFalse();
}

bool ViewportControllerPort::providerFrameSeekCapabilityKnownFalse() const
{
    return context.providerFrameSeekCapabilityKnownFalse();
}

bool ViewportControllerPort::providerFrameSeekCapabilityKnownTrue() const
{
    return context.providerFrameSeekCapabilityKnownTrue();
}

bool ViewportControllerPort::providerPositionSeekCapabilityKnownFalse() const
{
    return context.providerPositionSeekCapabilityKnownFalse();
}

bool ViewportControllerPort::providerKnownFactsTimedFrameCount() const
{
    return context.providerKnownFactsTimedFrameCount();
}

int ViewportControllerPort::providerKnownFactsFrameCount() const
{
    return context.providerKnownFactsFrameCount();
}

int ViewportControllerPort::providerFrameStartPosition(int frame) const
{
    return context.providerFrameStartPosition(frame);
}

int ViewportControllerPort::providerFrameIndexForPosition(int position) const
{
    return context.providerFrameIndexForPosition(position);
}

ImageSequenceAuthoredAnimationFacts ViewportControllerPort::providerAuthoredAnimationFacts() const
{
    return context.providerAuthoredAnimationFacts();
}

int ViewportControllerPort::frameCount() const { return context.frameCount(); }

int ViewportControllerPort::totalDuration() const { return context.totalDuration(); }

int ViewportControllerPort::sequenceFrameCount() const { return context.sequenceFrameCount(); }

int ViewportControllerPort::sequenceTotalDuration() const
{
    return context.sequenceTotalDuration();
}

int ViewportControllerPort::sequenceFrameIndexForPosition(int position) const
{
    return context.sequenceFrameIndexForPosition(position);
}

int ViewportControllerPort::sequenceFrameStartPosition(int frame) const
{
    return context.sequenceFrameStartPosition(frame);
}

ImageSequenceAuthoredAnimationFacts ViewportControllerPort::sequenceAuthoredAnimationFacts() const
{
    return context.sequenceAuthoredAnimationFacts();
}

bool ViewportControllerPort::hasSecondaryTimedSequence() const
{
    return context.hasSecondaryTimedSequence();
}

int ViewportControllerPort::secondarySequenceFrameCount() const
{
    return context.secondarySequenceFrameCount();
}

int ViewportControllerPort::secondarySequenceTotalDuration() const
{
    return context.secondarySequenceTotalDuration();
}

int ViewportControllerPort::secondaryTotalDuration() const
{
    return context.secondaryTotalDuration();
}

int ViewportControllerPort::secondarySequenceFrameIndexForPosition(int position) const
{
    return context.secondarySequenceFrameIndexForPosition(position);
}

int ViewportControllerPort::secondarySequenceFrameStartPosition(int frame) const
{
    return context.secondarySequenceFrameStartPosition(frame);
}

ImageSequenceAuthoredAnimationFacts
ViewportControllerPort::secondarySequenceAuthoredAnimationFacts() const
{
    return context.secondarySequenceAuthoredAnimationFacts();
}

ImageSequenceProviderKnownFacts ViewportControllerPort::secondaryProviderKnownFacts() const
{
    return context.secondaryProviderKnownFacts();
}

QSizeF ViewportControllerPort::secondaryProviderKnownLogicalSize() const
{
    return context.secondaryProviderKnownLogicalSize();
}

TimingIntervals ViewportControllerPort::secondaryProviderKnownTimingIntervals() const
{
    return context.secondaryProviderKnownTimingIntervals();
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::secondaryProviderTimedPlaybackCapability() const
{
    return context.secondaryProviderTimedPlaybackCapability();
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::secondaryProviderFrameSeekCapability() const
{
    return context.secondaryProviderFrameSeekCapability();
}

ImageSequenceProviderCapabilitySupport
ViewportControllerPort::secondaryProviderPositionSeekCapability() const
{
    return context.secondaryProviderPositionSeekCapability();
}

QSizeF ViewportControllerPort::sequenceLogicalSize() const { return context.sequenceLogicalSize(); }

QSizeF ViewportControllerPort::secondarySequenceLogicalSize() const
{
    return context.secondarySequenceLogicalSize();
}

QImage ViewportControllerPort::sequenceFrameImage(int frame) const
{
    return context.sequenceFrameImage(frame);
}

QImage ViewportControllerPort::secondarySequenceFrameImage(int frame) const
{
    return context.secondarySequenceFrameImage(frame);
}

double ViewportControllerPort::width() const { return context.width(); }

double ViewportControllerPort::height() const { return context.height(); }

ViewportController::ViewportController(const ViewportControllerContext& context)
    : viewport(context, state)
{
}

const ImageViewportInternal::PresentationState& ViewportController::presentationState() const
{
    return state.presentation;
}

const ImageViewportInternal::DisplayState& ViewportController::displayState() const
{
    return state.display;
}

const ImageViewportInternal::RequestState& ViewportController::requestState() const
{
    return state.request;
}

bool ViewportController::hasProviderSession() const { return state.provider.session != nullptr; }

bool ViewportController::hasProviderSession(ImageViewport::PageRole role) const
{
    return role == ImageViewport::PageRole::Secondary ? state.secondaryProvider.session != nullptr
                                                      : hasProviderSession();
}

bool ViewportController::providerMetadataReady() const { return state.provider.metadataReady; }

bool ViewportController::secondaryProviderMetadataReady() const
{
    return state.secondaryProvider.metadataReady;
}

bool ViewportController::providerTimedMetadata() const { return state.provider.timedMetadata; }

ImageSequenceAuthoredAnimationFacts ViewportController::providerAuthoredAnimationFacts() const
{
    return state.provider.authoredAnimationFacts;
}

bool ViewportController::secondaryProviderTimedMetadata() const
{
    return state.secondaryProvider.timedMetadata;
}

bool ViewportController::providerTimedPlaybackSupported() const
{
    return state.provider.timedPlaybackSupport;
}

bool ViewportController::secondaryProviderTimedPlaybackSupported() const
{
    return state.secondaryProvider.timedPlaybackSupport;
}

bool ViewportController::providerFrameSeekSupported() const
{
    return state.provider.frameSeekSupport;
}

bool ViewportController::secondaryProviderFrameSeekSupported() const
{
    return state.secondaryProvider.frameSeekSupport;
}

bool ViewportController::providerPositionSeekSupported() const
{
    return state.provider.positionSeekSupport;
}

bool ViewportController::secondaryProviderPositionSeekSupported() const
{
    return state.secondaryProvider.positionSeekSupport;
}

QSizeF ViewportController::providerLogicalSize() const { return state.provider.logicalSize; }

QSizeF ViewportController::secondaryProviderLogicalSize() const
{
    return state.secondaryProvider.logicalSize;
}

int ViewportController::providerFrameCount() const
{
    return state.provider.timedMetadata ? state.provider.timingIntervals.frameCount() : 1;
}

int ViewportController::secondaryProviderFrameCount() const
{
    return state.secondaryProvider.timedMetadata
        ? state.secondaryProvider.timingIntervals.frameCount()
        : 1;
}

int ViewportController::providerTotalDuration() const
{
    return state.provider.timedMetadata ? state.provider.timingIntervals.totalDuration() : -1;
}

int ViewportController::secondaryProviderTotalDuration() const
{
    return state.secondaryProvider.timedMetadata
        ? state.secondaryProvider.timingIntervals.totalDuration()
        : -1;
}

int ViewportController::providerFrameDuration(int frame) const
{
    return state.provider.timedMetadata ? state.provider.timingIntervals.frameDuration(frame) : -1;
}

int ViewportController::providerFrameStartPosition(int frame) const
{
    return state.provider.timedMetadata ? state.provider.timingIntervals.frameStartPosition(frame)
                                        : -1;
}

int ViewportController::providerFrameIndexForPosition(int position) const
{
    return state.provider.timedMetadata
        ? state.provider.timingIntervals.frameIndexForPosition(position)
        : -1;
}

bool ViewportController::looping() const { return state.request.looping; }

ImageViewportInternal::ViewportChangeSet ViewportController::setLooping(bool looping)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (state.request.looping == looping) {
        return changes;
    }

    state.request.looping = looping;
    changes.looping = true;
    return changes;
}

void ViewportController::incrementDisplayRevision() { ++state.display.revision; }

void ViewportController::incrementRequestRevision() { ++state.request.requestRevision; }

void ViewportController::incrementCommandRevision() { ++state.request.commandRevision; }

ViewportSequenceAssignmentResult ViewportController::assignSequence(
    ViewportSequenceAssignment assignment)
{
    ViewportSequenceAssignmentResult result;
    const std::optional<ControllerTransitionPolicy> transitionPolicy
        = normalizeControllerTransitionPolicy(assignment.transitionPolicy);
    if (!transitionPolicy) {
        ViewportCommandResult commandResult;
        commandResult.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, commandResult, ImageViewport::CommandReason::InvalidRequest);
        result.outcome = commandResult.outcome;
        result.changes = commandResult.changes;
        return result;
    }
    const bool retainDisplay = transitionPolicy->displayTransition
        == PageSetTransitionPolicy::DisplayTransition::RetainPrevious;

    if (!assignment.sequence) {
        const ViewportCommandResult clearResult = clear();
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
    const QPointF previousContentPosition = controllerContentPosition(viewport, state.presentation);
    ImageViewportInternal::ViewportChangeSet transitionChanges;

    const ViewportSequenceRoleSource secondarySource
        = resolvedSecondarySource(viewport, assignment);
    const DisplayRequestTarget secondaryInitialTarget = initialTargetForRoleSource(secondarySource);
    const ImageViewportInternal::ResolvedFrameIdentity secondaryInitialResolvedFrame
        = resolvedFrameForRoleSource(secondarySource);

    viewportRequestState(viewport).sequence = assignment.sequence;
    viewportRequestState(viewport).sequenceOwner = std::move(assignment.sequenceOwner);
    viewportRequestState(viewport).secondarySequence = assignment.secondarySequence;
    viewportRequestState(viewport).secondarySequenceOwner
        = std::move(assignment.secondarySequenceOwner);
    viewportRequestState(viewport).secondarySequenceIsProvider = secondarySource.provider;
    state.secondarySource = secondarySource;
    ++viewportRequestState(viewport).sequenceGeneration;
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
        ? viewport.providerAuthoredAnimationFacts()
        : ImageSequenceAuthoredAnimationFacts {};
    viewportProviderState(viewport).logicalSize = {};
    viewportProviderState(viewport).timingIntervals = {};
    discardPendingRenderCommit(viewport);
    viewportProviderState(viewport).activeMetadataToken = {};
    viewportProviderState(viewport).activeFrameToken = {};
    state.secondaryProvider.metadataReady = false;
    state.secondaryProvider.timedMetadata = false;
    state.secondaryProvider.timedPlaybackSupport = false;
    state.secondaryProvider.frameSeekSupport = false;
    state.secondaryProvider.positionSeekSupport = false;
    state.secondaryProvider.authoredAnimationFacts = secondarySource.provider
        ? secondarySource.authoredAnimationFacts
        : ImageSequenceAuthoredAnimationFacts {};
    state.secondaryProvider.logicalSize = {};
    state.secondaryProvider.timingIntervals = {};
    state.secondaryProvider.activeMetadataToken = {};
    state.secondaryProvider.activeFrameToken = {};

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
            initializeSecondaryActiveRequest(
                viewport, secondaryInitialTarget, secondaryInitialResolvedFrame);
        } else {
            const DisplayRequestTarget initialTarget {
                -1,
                -1,
                ImageViewportInternal::ProviderRequestTargetKind::Unknown,
            };
            viewportRequestState(viewport).beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, initialTarget, true);
            viewportRequestState(viewport).playbackPosition = initialTarget.position;
            initializeSecondaryActiveRequest(
                viewport, secondaryInitialTarget, secondaryInitialResolvedFrame);
        }
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        viewportDisplayState(viewport).status
            = viewportDisplayState(viewport).displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
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
        initializeSecondaryActiveRequest(
            viewport, secondaryInitialTarget, secondaryInitialResolvedFrame);
        if (secondarySource.provider) {
            stageBuiltInPrimarySpreadPayload(viewport);
        } else {
            publishAcceptedTargetState(viewport);
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
        if (retainDisplay && viewportDisplayState(viewport).displayedImageSize.isValid()) {
            viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Retained;
        } else {
            viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
        }
        result.openSecondaryProviderSession = true;
    }

    transitionChanges = applyPresentationTransition(
        viewport, state.presentation, *transitionPolicy, previousContentPosition);

    armAuthoredAutoplayIfEligible(viewport);

    result.changes.requestRevision = true;
    const bool displayValueChanged = viewportDisplayState(viewport).status != oldDisplayStatus
        || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
    result.changes.displayRevision = displayValueChanged;
    result.changes.sequence = true;
    result.changes.requestState = true;
    result.changes.displayState = displayValueChanged;
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.playbackPhase = viewportRequestState(viewport).playbackPhase != oldPlaybackPhase;
    result.changes.diagnostics = viewportRequestState(viewport).errorString != oldErrorString
        || viewportRequestState(viewport).warningString != oldWarningString;
    result.changes.scheduleUpdate = true;
    mergeChanges(result.changes, transitionChanges);
    return result;
}

ViewportCommandResult ViewportController::rejectInvalidCommand()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Invalid;
    state.request.setCommandDiagnostic(ImageViewport::CommandReason::InvalidRequest);
    result.changes.commandRevision = true;
    return result;
}

ViewportCommandResult ViewportController::rejectUnsupportedCommand()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    state.request.setCommandDiagnostic(ImageViewport::CommandReason::UnsupportedRequest);
    result.changes.commandRevision = true;
    return result;
}

ViewportCommandResult ViewportController::rejectIgnoredNoRequestCommand()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
    state.request.setCommandDiagnostic(ImageViewport::CommandReason::IgnoredNoRequest);
    result.changes.commandRevision = true;
    return result;
}

ViewportCommandResult ViewportController::clear()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const bool sequenceValueChanged = viewportRequestState(viewport).sequence != nullptr
        || viewportRequestState(viewport).secondarySequence != nullptr;
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
    viewportRequestState(viewport).sequence = nullptr;
    viewportRequestState(viewport).sequenceOwner.reset();
    viewportRequestState(viewport).secondarySequence = nullptr;
    viewportRequestState(viewport).secondarySequenceOwner.reset();
    viewportRequestState(viewport).secondarySequenceIsProvider = false;
    state.secondarySource = {};
    ++viewportRequestState(viewport).sequenceGeneration;
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
    state.secondaryProvider.metadataReady = false;
    state.secondaryProvider.timedMetadata = false;
    state.secondaryProvider.timedPlaybackSupport = false;
    state.secondaryProvider.frameSeekSupport = false;
    state.secondaryProvider.positionSeekSupport = false;
    state.secondaryProvider.authoredAnimationFacts = {};
    state.secondaryProvider.logicalSize = {};
    state.secondaryProvider.timingIntervals = {};
    state.secondaryProvider.activeMetadataToken = {};
    state.secondaryProvider.activeFrameToken = {};
    viewportRequestState(viewport).errorString.clear();
    viewportRequestState(viewport).warningString.clear();
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    result.changes.requestRevision = requestChanged;
    result.changes.displayRevision = displayChanged;
    result.changes.sequence = sequenceValueChanged;
    result.changes.requestState = requestChanged;
    result.changes.displayState = displayChanged;
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.playbackPhase = playbackChanged;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportCommandResult ViewportController::acceptNoopCommand()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    return result;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::setNextProviderRequestTokenForTest(quint64 token)
{
    state.provider.nextRequestToken = token;
}

void ViewportController::setNextProviderRequestTokenForTest(
    ImageViewport::PageRole role, quint64 token)
{
    if (role == ImageViewport::PageRole::Secondary) {
        state.secondaryProvider.nextRequestToken = token;
        return;
    }

    setNextProviderRequestTokenForTest(token);
}

bool ViewportController::hasPendingRenderCommitForTest() const
{
    return state.display.pendingRenderPayload.commitPending;
}

quint64 ViewportController::activeRequestIdForTest() const
{
    return state.request.activeRequest.identity.id;
}

quint64 ViewportController::displayedRequestIdForTest() const
{
    return state.display.displayedRequest.request.identity.id;
}

quint64 ViewportController::pendingRenderGenerationForTest() const
{
    return state.display.pendingRenderPayload.generation;
}

quint64 ViewportController::pendingRenderPayloadIdForTest() const
{
    return state.display.pendingRenderPayload.payloadId;
}

quint64 ViewportController::secondaryPendingRenderPayloadIdForTest() const
{
    return state.display.secondaryPendingRenderPayload.payloadId;
}
#endif
