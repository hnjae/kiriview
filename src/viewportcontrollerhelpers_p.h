#pragma once

#include "viewportcontroller_p.h"

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

namespace {
using ImageViewportInternal::DisplayRequestTarget;
using ImageViewportInternal::PlaybackAdvanceTarget;
using ImageViewportInternal::playbackAdvanceTarget;

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
        source.authoredAnimationFacts = assignment.secondarySourceHandle.facts.authoredAnimationFacts;
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

ViewportCommandResult commandResultWithSecondaryTransport(ViewportCommandResult result)
{
    result.secondaryProviderFrameTransport = result.providerFrameTransport;
    result.providerFrameTransport = {};
    return result;
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

bool isPositiveGeometrySize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

QSizeF imageLogicalSize(const QImage& image)
{
    return image.isNull() ? QSizeF() : image.deviceIndependentSize();
}

enum class GeometryProjectionTarget {
    CurrentDisplay,
    PendingRender,
};

QSizeF displayedPrimaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& display = viewportDisplayState(viewport);
    if (!display.hasReadyDisplay(viewport.hasDisplayableSequence())) {
        return {};
    }
    return display.displayedImageSize;
}

QSizeF displayedSecondaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& display = viewportDisplayState(viewport);
    if (!display.hasReadyDisplay(viewport.hasDisplayableSequence())) {
        return {};
    }
    return display.secondaryDisplayedImageSize;
}

QSizeF pendingPrimaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& display = viewportDisplayState(viewport);
    if (viewport.hasProviderSequence()
        && isPositiveGeometrySize(viewportProviderState(viewport).logicalSize)) {
        return viewportProviderState(viewport).logicalSize;
    }
    const QSizeF pendingSize = imageLogicalSize(display.pendingRenderPayload.image);
    if (isPositiveGeometrySize(pendingSize)) {
        return pendingSize;
    }
    return displayedPrimaryGeometrySize(viewport);
}

QSizeF pendingSecondaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& request = viewportRequestState(viewport);
    if (!request.secondarySequence || request.secondaryActiveRequest.target.frame < 0) {
        return {};
    }

    const auto& display = viewportDisplayState(viewport);
    if (request.secondarySequenceIsProvider) {
        if (isPositiveGeometrySize(viewport.secondaryProviderState().logicalSize)) {
            return viewport.secondaryProviderState().logicalSize;
        }
        const QSizeF pendingSize = imageLogicalSize(display.secondaryPendingRenderPayload.image);
        return isPositiveGeometrySize(pendingSize) ? pendingSize
                                                   : displayedSecondaryGeometrySize(viewport);
    }

    const QSizeF pendingSize = imageLogicalSize(display.secondaryPendingRenderPayload.image);
    return isPositiveGeometrySize(pendingSize) ? pendingSize
                                               : displayedSecondaryGeometrySize(viewport);
}

QSizeF acceptedPrimaryGeometrySize(ViewportControllerPort viewport)
{
    if (viewport.hasProviderSequence()) {
        return isPositiveGeometrySize(viewportProviderState(viewport).logicalSize)
            ? viewportProviderState(viewport).logicalSize
            : QSizeF {};
    }

    const QSizeF sequenceSize = viewport.sequenceLogicalSize();
    return isPositiveGeometrySize(sequenceSize) ? sequenceSize : QSizeF {};
}

QSizeF acceptedSecondaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& request = viewportRequestState(viewport);
    if (!request.secondarySequence) {
        return {};
    }
    if (request.secondarySequenceIsProvider) {
        return isPositiveGeometrySize(viewport.secondaryProviderState().logicalSize)
            ? viewport.secondaryProviderState().logicalSize
            : QSizeF {};
    }
    if (request.secondaryActiveRequest.target.frame < 0) {
        return {};
    }

    const QSizeF sequenceSize = viewport.secondarySequenceLogicalSize();
    return isPositiveGeometrySize(sequenceSize) ? sequenceSize : QSizeF {};
}

PresentationGeometry::State controllerGeometryState(ViewportControllerPort viewport,
    const ImageViewportInternal::PresentationState& presentation, double devicePixelRatio = 1.0,
    std::optional<QRectF> itemBounds = std::nullopt,
    GeometryProjectionTarget target = GeometryProjectionTarget::CurrentDisplay)
{
    const QRectF bounds = itemBounds ? *itemBounds : viewport.itemBounds();
    QSizeF primarySize = displayedPrimaryGeometrySize(viewport);
    QSizeF secondarySize = displayedSecondaryGeometrySize(viewport);
    if (target == GeometryProjectionTarget::PendingRender) {
        primarySize = pendingPrimaryGeometrySize(viewport);
        secondarySize = pendingSecondaryGeometrySize(viewport);
    }

    return {
        isPositiveGeometrySize(primarySize),
        bounds,
        primarySize,
        secondarySize,
        presentation.pageGap,
        presentation.spreadDirection,
        presentation.fitMode,
        presentation.rotationDegrees,
        presentation.mirrorHorizontally,
        presentation.mirrorVertically,
        presentation.manualZoom,
        devicePixelRatio > 0.0 ? devicePixelRatio : 1.0,
        presentation.contentPosition,
    };
}

PresentationGeometry::State acceptedGeometryState(ViewportControllerPort viewport,
    const ImageViewportInternal::PresentationState& presentation, double devicePixelRatio = 1.0,
    std::optional<QRectF> itemBounds = std::nullopt)
{
    const QRectF bounds = itemBounds ? *itemBounds : viewport.itemBounds();
    const QSizeF primarySize = acceptedPrimaryGeometrySize(viewport);
    const QSizeF secondarySize = acceptedSecondaryGeometrySize(viewport);

    return {
        isPositiveGeometrySize(primarySize),
        bounds,
        primarySize,
        secondarySize,
        presentation.pageGap,
        presentation.spreadDirection,
        presentation.fitMode,
        presentation.rotationDegrees,
        presentation.mirrorHorizontally,
        presentation.mirrorVertically,
        presentation.manualZoom,
        devicePixelRatio > 0.0 ? devicePixelRatio : 1.0,
        presentation.contentPosition,
    };
}

QPointF controllerContentPosition(
    ViewportControllerPort& viewport, const ImageViewportInternal::PresentationState& presentation)
{
    return PresentationGeometry::contentPosition(controllerGeometryState(viewport, presentation));
}

QPointF controllerMaximumContentPosition(
    ViewportControllerPort& viewport, const ImageViewportInternal::PresentationState& presentation)
{
    return PresentationGeometry::maximumContentPosition(
        controllerGeometryState(viewport, presentation));
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
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
}

bool shouldPreservePlaybackPositionOnPlay(
    ImageViewport::PlaybackPhase phase, bool stopWhenRequestReady)
{
    return !stopWhenRequestReady
        && (phase == ImageViewport::PlaybackPhase::Playing
            || phase == ImageViewport::PlaybackPhase::Paused
            || phase == ImageViewport::PlaybackPhase::Waiting);
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

bool hasProviderSequenceForRole(ViewportControllerPort& viewport, ImageViewport::PageRole role)
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
        && viewportRequestState(viewport).secondaryActiveRequest.target.frame >= 0;
}

bool displayedPrimaryPayloadMatchesActiveTarget(ViewportControllerPort& viewport)
{
    return viewport.hasReadyDisplay()
        && viewportDisplayState(viewport).displayedRequest.generation
        == viewportRequestState(viewport).sequenceGeneration
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.frame
        == viewportRequestState(viewport).activeRequest.resolvedFrame.frame
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.position
        == viewportRequestState(viewport).activeRequest.resolvedFrame.position;
}

bool displayedSecondaryPayloadMatchesActiveTarget(ViewportControllerPort& viewport)
{
    if (!hasSecondarySequence(viewport)) {
        return true;
    }
    return viewport.hasReadyDisplay()
        && viewportDisplayState(viewport).secondaryDisplayedRequest.generation
        == viewportRequestState(viewport).sequenceGeneration
        && viewportDisplayState(viewport).secondaryDisplayedRequest.request.resolvedFrame.frame
        == viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame.frame
        && viewportDisplayState(viewport).secondaryDisplayedRequest.request.resolvedFrame.position
        == viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame.position;
}

DisplayRequestTarget providerPlaybackStartTarget(ViewportControllerPort& viewport)
{
    int selectedFrame = viewportRequestState(viewport).activeRequest.target.frame;
    if (selectedFrame < 0
        || selectedFrame >= viewportProviderState(viewport).timingIntervals.frameCount()) {
        selectedFrame = 0;
    }
    return DisplayRequestTarget { selectedFrame, viewport.providerFrameStartPosition(selectedFrame),
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

DisplayRequestTarget pendingProviderPlaybackTarget()
{
    return DisplayRequestTarget { -1, -1,
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

ImageViewport::PlaybackPhase playbackPhaseForCurrentRequest(ViewportControllerPort& viewport)
{
    return viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

bool effectiveLoopingForPlayback(
    ViewportControllerPort& viewport, ImageSequenceAuthoredAnimationFacts facts)
{
    if (viewportRequestState(viewport).looping) {
        return true;
    }

    switch (facts.loopMode()) {
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Finite:
        return viewportRequestState(viewport).playbackLoopIterationsCompleted + 1
            < facts.loopCount();
    case ImageSequenceAuthoredAnimationFacts::LoopMode::PlayOnce:
        return false;
    }
    return false;
}

ImageViewport::PlaybackPhase playbackAdvancePhaseForRequest(
    ImageViewport::RequestStatus requestStatus, bool reachedEnd)
{
    if (reachedEnd && requestStatus != ImageViewport::RequestStatus::Loading) {
        return ImageViewport::PlaybackPhase::Stopped;
    }
    return requestStatus == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

}
