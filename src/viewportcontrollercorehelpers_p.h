#pragma once

#include "viewportcontroller_p.h"

#include "imageviewportproviderfacts_p.h"
#include "viewportcontrollermetadatacontract_p.h"
#include "viewportgeometryhelpers_p.h"

#include <optional>

namespace {
using ImageViewportInternal::DisplayRequestTarget;

struct ViewportProviderRoleState
{
    ImageViewportInternal::ProviderGenerationState& provider;
    ImageViewportInternal::DisplayRequest& activeRequest;
    ImageViewportInternal::DisplayRequest& latestNonPlaybackRequest;
};

struct ConstViewportProviderRoleState
{
    const ImageViewportInternal::ProviderGenerationState& provider;
    const ImageViewportInternal::DisplayRequest& activeRequest;
    const ImageViewportInternal::DisplayRequest& latestNonPlaybackRequest;
};

struct ViewportDisplayRoleState
{
    ImageViewportInternal::DisplayRequestSnapshot& displayedRequest;
    QSizeF& displayedImageSize;
    QImage& displayedImage;
    ImageViewportInternal::PreparedPayload& pendingPayload;
};

struct ConstViewportDisplayRoleState
{
    const ImageViewportInternal::DisplayRequestSnapshot& displayedRequest;
    const QSizeF& displayedImageSize;
    const QImage& displayedImage;
    const ImageViewportInternal::PreparedPayload& pendingPayload;
};

ImageViewportInternal::DisplayState& viewportDisplayState(ViewportControllerPort viewport)
{
    return viewport.displayState();
}

ImageViewportInternal::RequestState& viewportRequestState(ViewportControllerPort viewport)
{
    return viewport.requestState();
}

ImageViewportInternal::ProviderGenerationState& viewportProviderState(
    ViewportControllerPort viewport)
{
    return viewport.providerState();
}

ImageViewportInternal::ProviderGenerationState& providerGenerationStateForRole(
    ViewportControllerState& state, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? state.secondaryProvider : state.provider;
}

const ImageViewportInternal::ProviderGenerationState& providerGenerationStateForRole(
    const ViewportControllerState& state, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? state.secondaryProvider : state.provider;
}

QPointer<ImageSequence>& sequenceForRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondarySequence
                                                      : request.sequence;
}

const QPointer<ImageSequence>& sequenceForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondarySequence
                                                      : request.sequence;
}

ImageViewportInternal::ImageSequenceSource& sequenceSourceForRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondarySequenceSource
                                                      : request.sequenceSource;
}

const ImageViewportInternal::ImageSequenceSource& sequenceSourceForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondarySequenceSource
                                                      : request.sequenceSource;
}

ImageViewportInternal::DisplayRequest& activeRequestForRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondaryActiveRequest
                                                      : request.activeRequest;
}

const ImageViewportInternal::DisplayRequest& activeRequestForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondaryActiveRequest
                                                      : request.activeRequest;
}

ImageViewportInternal::DisplayRequest& latestNonPlaybackRequestForRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondaryLatestNonPlaybackRequest
                                                      : request.latestNonPlaybackRequest;
}

const ImageViewportInternal::DisplayRequest& latestNonPlaybackRequestForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.secondaryLatestNonPlaybackRequest
                                                      : request.latestNonPlaybackRequest;
}

ViewportProviderRoleState providerRoleStateFor(
    ViewportControllerState& state, ImageViewport::PageRole role)
{
    return { providerGenerationStateForRole(state, role), activeRequestForRole(state.request, role),
        latestNonPlaybackRequestForRole(state.request, role) };
}

ConstViewportProviderRoleState providerRoleStateFor(
    const ViewportControllerState& state, ImageViewport::PageRole role)
{
    return { providerGenerationStateForRole(state, role), activeRequestForRole(state.request, role),
        latestNonPlaybackRequestForRole(state.request, role) };
}

ViewportDisplayRoleState displayRoleStateFor(
    ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary
        ? ViewportDisplayRoleState { display.secondaryDisplayedRequest,
              display.secondaryDisplayedImageSize, display.secondaryDisplayedImage,
              display.secondaryPendingRenderPayload }
        : ViewportDisplayRoleState { display.displayedRequest, display.displayedImageSize,
              display.displayedImage, display.pendingRenderPayload };
}

ConstViewportDisplayRoleState displayRoleStateFor(
    const ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary
        ? ConstViewportDisplayRoleState { display.secondaryDisplayedRequest,
              display.secondaryDisplayedImageSize, display.secondaryDisplayedImage,
              display.secondaryPendingRenderPayload }
        : ConstViewportDisplayRoleState { display.displayedRequest, display.displayedImageSize,
              display.displayedImage, display.pendingRenderPayload };
}

ImageViewportInternal::PreparedPayload& pendingPayloadForRole(
    ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? display.secondaryPendingRenderPayload
                                                      : display.pendingRenderPayload;
}

const ImageViewportInternal::PreparedPayload& pendingPayloadForRole(
    const ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? display.secondaryPendingRenderPayload
                                                      : display.pendingRenderPayload;
}

ImageViewport::TriState triStateFromSupport(bool supported)
{
    return supported ? ImageViewport::TriState::True : ImageViewport::TriState::False;
}

void projectFrameSeekBounds(ViewportMetadataProjection& projection)
{
    if (projection.frameSeekSupport == ImageViewport::TriState::True && projection.frameCount > 0) {
        projection.frameSeekBounds = ImageViewportRange(0, projection.frameCount - 1);
    }
}

void projectPositionSeekBounds(ViewportMetadataProjection& projection)
{
    if (projection.positionSeekSupport == ImageViewport::TriState::True
        && projection.totalDuration >= 0) {
        projection.positionSeekBounds = ImageViewportRange(0, projection.totalDuration);
    }
}

ViewportMetadataProjection projectTimedMetadata(int frameCount, int totalDuration,
    bool timedPlaybackSupport, bool frameSeekSupport, bool positionSeekSupport)
{
    ViewportMetadataProjection projection;
    projection.frameCount = frameCount;
    projection.totalDuration = totalDuration;
    projection.timedPlaybackSupport = triStateFromSupport(timedPlaybackSupport);
    projection.frameSeekSupport = triStateFromSupport(frameSeekSupport);
    projection.positionSeekSupport = triStateFromSupport(positionSeekSupport);
    projectFrameSeekBounds(projection);
    projectPositionSeekBounds(projection);
    return projection;
}

ViewportMetadataProjection projectStillMetadata(bool frameSeekSupport)
{
    ViewportMetadataProjection projection;
    projection.frameCount = 1;
    projection.timedPlaybackSupport = ImageViewport::TriState::False;
    projection.frameSeekSupport = triStateFromSupport(frameSeekSupport);
    projection.positionSeekSupport = ImageViewport::TriState::False;
    projectFrameSeekBounds(projection);
    return projection;
}

ViewportMetadataProjection projectProviderRuntimeMetadata(
    const ImageViewportInternal::ProviderGenerationState& provider)
{
    if (!provider.metadataReady) {
        return {};
    }
    if (!provider.timedMetadata) {
        return projectStillMetadata(provider.frameSeekSupport);
    }
    return projectTimedMetadata(provider.timingIntervals.frameCount(),
        provider.timingIntervals.totalDuration(), provider.timedPlaybackSupport,
        provider.frameSeekSupport, provider.positionSeekSupport);
}

ViewportMetadataProjection projectProviderConstructionMetadata(
    const ImageSequenceProviderKnownFacts& facts,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability)
{
    ViewportMetadataProjection projection;
    projection.timedPlaybackSupport
        = ImageViewportInternal::capabilitySupportToTriState(timedPlaybackCapability);
    projection.frameSeekSupport
        = ImageViewportInternal::capabilitySupportToTriState(frameSeekCapability);
    projection.positionSeekSupport
        = ImageViewportInternal::capabilitySupportToTriState(positionSeekCapability);

    if (!facts.isSpecified() || facts.isLogicalSizeOnly()) {
        return projection;
    }

    if (facts.isStill()) {
        return projectStillMetadata(
            ImageViewportInternal::providerResolvedCapability(frameSeekCapability, true));
    }

    if (facts.isTimedFrameList()) {
        const TimingIntervals timingIntervals
            = TimingIntervals::fromFrameDurations(facts.frameDurations());
        return projectTimedMetadata(timingIntervals.frameCount(), timingIntervals.totalDuration(),
            ImageViewportInternal::providerResolvedCapability(timedPlaybackCapability, true),
            ImageViewportInternal::providerResolvedCapability(frameSeekCapability, true),
            ImageViewportInternal::providerResolvedCapability(positionSeekCapability, true));
    }

    if (facts.isTimedFrameCount()
        && ImageViewportInternal::providerCapabilityKnownTrue(frameSeekCapability)) {
        projection.frameCount = facts.frameCount();
        projectFrameSeekBounds(projection);
    }

    return projection;
}

ViewportMetadataProjection projectBuiltInMetadata(
    bool present, bool timed, int frameCount, int totalDuration)
{
    if (!present) {
        return {};
    }
    if (!timed) {
        ViewportMetadataProjection projection;
        projection.frameCount = frameCount;
        projection.timedPlaybackSupport = ImageViewport::TriState::False;
        projection.frameSeekSupport = ImageViewport::TriState::True;
        projection.positionSeekSupport = ImageViewport::TriState::False;
        projectFrameSeekBounds(projection);
        return projection;
    }
    return projectTimedMetadata(frameCount, totalDuration, true, true, true);
}

ViewportSequenceRoleSource resolvedSecondarySource(
    ViewportControllerPort& viewport, const ViewportSequenceAssignment& assignment)
{
    Q_UNUSED(viewport);
    ViewportSequenceRoleSource source = assignment.secondarySource;
    if (!assignment.secondarySourceHandle.sequence) {
        return {};
    }
    if (!source.present) {
        source.present = assignment.secondarySourceHandle.facts.present;
        source.provider = assignment.secondarySourceHandle.facts.provider;
        source.timed = assignment.secondarySourceHandle.facts.timed;
        source.authoredAnimationFacts
            = assignment.secondarySourceHandle.facts.authoredAnimationFacts;
    }
    if (source.present && !source.provider && source.frameCount < 0) {
        source.frameCount = assignment.secondarySourceHandle.facts.frameCount;
        source.firstFramePosition
            = source.timed ? sourceFrameStartPosition(assignment.secondarySourceHandle, 0) : -1;
        source.timingIntervals = assignment.secondarySourceHandle.facts.timingIntervals;
    }
    return source;
}

ImageViewportInternal::DisplayRequestTarget initialTargetForRoleSource(
    const ViewportSequenceRoleSource& source)
{
    if (!source.present || source.provider || source.frameCount <= 0) {
        return {};
    }
    return { 0, source.timed ? source.firstFramePosition : -1,
        ImageViewportInternal::ProviderRequestTargetKind::Unknown };
}

ImageViewportInternal::ResolvedFrameIdentity resolvedFrameForRoleSource(
    const ViewportSequenceRoleSource& source)
{
    const ImageViewportInternal::DisplayRequestTarget target = initialTargetForRoleSource(source);
    return target.frame >= 0
        ? ImageViewportInternal::ResolvedFrameIdentity { target.frame, target.position }
        : ImageViewportInternal::ResolvedFrameIdentity {};
}

int frameStartPositionForRoleSource(
    ViewportControllerPort& viewport, const ViewportSequenceRoleSource& source, int frame)
{
    Q_UNUSED(viewport);
    if (!source.timed) {
        return -1;
    }
    if (source.timingIntervals.isValid()) {
        return source.timingIntervals.frameStartPosition(frame);
    }
    return -1;
}

int frameIndexForRoleSource(
    ViewportControllerPort& viewport, const ViewportSequenceRoleSource& source, int position)
{
    Q_UNUSED(viewport);
    if (!source.timed) {
        return -1;
    }
    if (source.timingIntervals.isValid()) {
        return source.timingIntervals.frameIndexForPosition(position);
    }
    return -1;
}

struct RequestStatusSnapshot
{
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
};

RequestStatusSnapshot requestStatusSnapshot(ViewportControllerPort& viewport)
{
    return { viewportRequestState(viewport).status, viewportRequestState(viewport).reason };
}

bool requestStatusChanged(ViewportControllerPort& viewport, RequestStatusSnapshot snapshot)
{
    return viewportRequestState(viewport).status != snapshot.status
        || viewportRequestState(viewport).reason != snapshot.reason;
}

void markRequestMutation(ImageViewportInternal::ViewportChangeSet& changes)
{
    changes.requestRevision = true;
    changes.requestState = true;
}

void markDisplayMutation(
    ImageViewportInternal::ViewportChangeSet& changes, bool displayStateChanged = true)
{
    changes.displayRevision = true;
    changes.displayState = changes.displayState || displayStateChanged;
}

void markRequestAndDisplayMutation(ImageViewportInternal::ViewportChangeSet& changes)
{
    markRequestMutation(changes);
    markDisplayMutation(changes);
}

void markDiagnosticsMutation(
    ImageViewportInternal::ViewportChangeSet& changes, bool diagnosticsChanged = true)
{
    changes.diagnostics = changes.diagnostics || diagnosticsChanged;
}

void markProviderDispatchFailure(ImageViewportInternal::ViewportChangeSet& changes)
{
    markRequestMutation(changes);
    markDiagnosticsMutation(changes);
}

void markScheduleUpdate(ImageViewportInternal::ViewportChangeSet& changes)
{
    changes.scheduleUpdate = true;
}

bool viewportGeometryChanged(ViewportControllerPort& viewport, const QRectF& oldContentRect,
    const QRectF& oldVisibleImageRect)
{
    return ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
}

ImageViewport::DisplayStatus retainedOrEmptyDisplayStatus(ViewportControllerPort& viewport)
{
    const auto primaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
    return primaryDisplay.displayedImageSize.isValid() ? ImageViewport::DisplayStatus::Retained
                                                       : ImageViewport::DisplayStatus::Empty;
}

void publishRetainedOrEmptyDisplayStatus(ViewportControllerPort& viewport)
{
    viewportDisplayState(viewport).status = retainedOrEmptyDisplayStatus(viewport);
}

std::optional<ControllerTransitionPolicy> normalizeControllerTransitionPolicy(
    const PageSetTransitionPolicy& policy)
{
    if (!policy.isValid()) {
        return std::nullopt;
    }

    const PageSetTransitionPolicy* const policyAccess = &policy;
    ControllerTransitionPolicy normalized { policy.displayTransition(),
        policyAccess->zoomTransition(), policy.contentPositionTransition(),
        policy.rotationTransition(), policy.mirrorTransition(), policy.replacementIntent() };

    if (policy.fitModeTransition() == PageSetTransitionPolicy::FitModeTransition::SetExplicit) {
        normalized.explicitFitMode = policy.fitMode();
    }
    if (policy.spreadDirectionTransition()
        == PageSetTransitionPolicy::SpreadDirectionTransition::SetExplicit) {
        normalized.explicitSpreadDirection = policy.spreadDirection();
    }
    if (policy.pageGapTransition() == PageSetTransitionPolicy::PageGapTransition::SetExplicit) {
        normalized.explicitPageGap = policy.pageGap();
    }

    return normalized;
}

void mergeChanges(ImageViewportInternal::ViewportChangeSet& target,
    ImageViewportInternal::ViewportChangeSet source)
{
    target.requestState = target.requestState || source.requestState;
    target.displayState = target.displayState || source.displayState;
    target.geometryState = target.geometryState || source.geometryState;
    target.playbackPhase = target.playbackPhase || source.playbackPhase;
    target.diagnostics = target.diagnostics || source.diagnostics;
    target.presentation = target.presentation || source.presentation;
    target.sequence = target.sequence || source.sequence;
    target.looping = target.looping || source.looping;
    target.displayRevision = target.displayRevision || source.displayRevision;
    target.requestRevision = target.requestRevision || source.requestRevision;
    target.commandRevision = target.commandRevision || source.commandRevision;
    if (source.commandRevisionValue != 0) {
        target.commandRevisionValue = source.commandRevisionValue;
    }
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
    if (source.renderFailureDiagnostic.valid) {
        target.renderFailureDiagnostic = source.renderFailureDiagnostic;
    }
}

bool activeProviderFrameTokenMatchesActiveRequest(const ViewportControllerState& state,
    const ViewportControllerPort viewport, ImageViewport::PageRole role,
    ImageSequenceProviderRequestToken token)
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!provider.activeFrameToken.isValid() || token != provider.activeFrameToken) {
        return false;
    }

    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(viewportRequestState(viewport), role);
    return token.isValid() && token == request.providerFrameToken;
}

bool activeProviderFrameTokenMatchesActiveRequest(
    const ViewportControllerPort viewport, ImageSequenceProviderRequestToken token)
{
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Primary);
    return viewportProviderState(viewport).activeFrameToken.isValid()
        && token == viewportProviderState(viewport).activeFrameToken && token.isValid()
        && token == request.providerFrameToken;
}

bool activeProviderFrameRequestIsPlayback(const ViewportControllerState& state,
    const ViewportControllerPort viewport, ImageViewport::PageRole role)
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    return activeProviderFrameTokenMatchesActiveRequest(
               state, viewport, role, provider.activeFrameToken)
        && activeRequestForRole(viewportRequestState(viewport), role).target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Playback;
}

bool activeProviderFrameRequestIsPlayback(const ViewportControllerPort viewport)
{
    return activeProviderFrameTokenMatchesActiveRequest(
               viewport, viewportProviderState(viewport).activeFrameToken)
        && activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Primary)
               .target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Playback;
}

void appendProviderFrameStartResult(ViewportProviderFrameTransportEffect& effect,
    const ViewportProviderFrameRequestStartResult& start)
{
    effect.closeSession = start.closeSession;
    effect.sessionClose = start.sessionClose;
    effect.sendCommand = start.sendCommand;
    effect.command = start.command;
}

bool hasSecondaryProviderSequence(ViewportControllerPort& viewport)
{
    return sequenceForRole(viewportRequestState(viewport), ImageViewport::PageRole::Secondary)
        && viewportRequestState(viewport).secondarySequenceIsProvider;
}

bool hasProviderSequenceForRole(ViewportControllerPort viewport, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary
        ? viewport.hasProviderSequence()
        : (sequenceForRole(viewportRequestState(viewport), role)
              && viewportRequestState(viewport).secondarySequenceIsProvider);
}

bool hasSecondarySequence(ViewportControllerPort& viewport)
{
    return sequenceForRole(viewportRequestState(viewport), ImageViewport::PageRole::Secondary)
        && activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Secondary)
               .target.frame
        >= 0;
}

bool hasSecondaryRole(ViewportControllerPort& viewport)
{
    return sequenceForRole(viewportRequestState(viewport), ImageViewport::PageRole::Secondary);
}

bool isUnknownMetadataInitialRequest(const ImageViewportInternal::DisplayRequest& request)
{
    return (request.identity.origin == ImageViewportInternal::DisplayRequestOrigin::Initial
               || request.identity.origin
                   == ImageViewportInternal::DisplayRequestOrigin::StopRestore
               || request.identity.origin
                   == ImageViewportInternal::DisplayRequestOrigin::MetadataBoundSelection)
        && request.target.frame < 0 && request.target.position < 0
        && request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Unknown;
}

bool targetRequiresSecondaryPayload(ViewportControllerPort& viewport)
{
    return hasSecondarySequence(viewport)
        && activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Secondary)
               .target.frame
        >= 0;
}

bool displayedPrimaryPayloadMatchesActiveTarget(ViewportControllerPort& viewport)
{
    const auto primaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
    const ImageViewportInternal::DisplayRequest& primaryRequest
        = activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Primary);
    return viewport.hasReadyDisplay()
        && primaryDisplay.displayedRequest.generation
        == viewportRequestState(viewport).sequenceGeneration
        && primaryDisplay.displayedRequest.request.resolvedFrame.frame
        == primaryRequest.resolvedFrame.frame
        && primaryDisplay.displayedRequest.request.resolvedFrame.position
        == primaryRequest.resolvedFrame.position;
}

bool displayedSecondaryPayloadMatchesActiveTarget(ViewportControllerPort& viewport)
{
    if (!hasSecondarySequence(viewport)) {
        return true;
    }
    const auto secondaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Secondary);
    const ImageViewportInternal::DisplayRequest& secondaryRequest
        = activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Secondary);
    return viewport.hasReadyDisplay()
        && secondaryDisplay.displayedRequest.generation
        == viewportRequestState(viewport).sequenceGeneration
        && secondaryDisplay.displayedRequest.request.resolvedFrame.frame
        == secondaryRequest.resolvedFrame.frame
        && secondaryDisplay.displayedRequest.request.resolvedFrame.position
        == secondaryRequest.resolvedFrame.position;
}

}
