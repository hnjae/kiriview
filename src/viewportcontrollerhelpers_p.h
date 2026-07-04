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

enum class ExplicitSeekMaterialization {
    ProviderReady,
    ProviderPendingMetadata,
    BuiltIn,
};

void setCommandDiagnostic(ViewportControllerPort& viewport, ViewportCommandResult& result,
    ImageViewport::CommandReason reason)
{
    viewportRequestState(viewport).setCommandDiagnostic(reason);
    result.changes.commandRevision = true;
}

void clearCommandDiagnosticForAcceptedCommand(
    ViewportControllerPort& viewport, ViewportCommandResult& result)
{
    result.changes.commandRevision
        = viewportRequestState(viewport).clearCommandDiagnosticForAcceptedCommand()
        || result.changes.commandRevision;
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

void publishLoadingWaitState(
    ViewportControllerPort& viewport, ImageViewportInternal::TargetSpreadWaitState waitState)
{
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewportInternal::projectWaitReason(waitState);
}

struct ControllerTransitionPolicy
{
    PageSetTransitionPolicy::DisplayTransition displayTransition
        = PageSetTransitionPolicy::DisplayTransition::RetainPrevious;
    PageSetTransitionPolicy::ZoomTransition magnificationPolicy
        = PageSetTransitionPolicy::ZoomTransition::Preserve;
    PageSetTransitionPolicy::ContentPositionTransition contentPositionTransition
        = PageSetTransitionPolicy::ContentPositionTransition::Clamp;
    PageSetTransitionPolicy::RotationTransition rotationTransition
        = PageSetTransitionPolicy::RotationTransition::Preserve;
    PageSetTransitionPolicy::MirrorTransition mirrorTransition
        = PageSetTransitionPolicy::MirrorTransition::Preserve;
    PageSetTransitionPolicy::ReplacementIntent replacementIntent
        = PageSetTransitionPolicy::ReplacementIntent::NewTarget;
    std::optional<ImageViewport::FitMode> explicitFitMode;
    std::optional<ImageViewport::SpreadDirection> explicitSpreadDirection;
    std::optional<double> explicitPageGap;
};

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

    const QSizeF sequenceSize = viewport.secondarySequenceLogicalSize();
    return isPositiveGeometrySize(sequenceSize) ? sequenceSize
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

QRectF renderTargetRect(const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::pageItemRect(geometry, role).intersected(geometry.itemBounds);
}

QRectF renderSourceRect(const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::visiblePageRect(geometry, role);
}

ImageViewportInternal::PreparedPayload primaryRenderPayload(
    ViewportControllerPort viewport, const ViewportRenderSynchronization& synchronization)
{
    ImageViewportInternal::PreparedPayload payload = synchronization.preparedPayload;
    if (payload.image.isNull()
        && viewportDisplayState(viewport).hasReadyDisplay(viewport.hasDisplayableSequence())) {
        payload.image = viewportDisplayState(viewport).displayedImage;
    }
    return payload;
}

ImageViewportInternal::PreparedPayload secondaryRenderPayload(ViewportControllerPort viewport,
    const ViewportRenderSynchronization& synchronization,
    const ImageViewportInternal::PreparedPayload& primaryPayload)
{
    const auto& display = viewportDisplayState(viewport);
    const auto& request = viewportRequestState(viewport);
    ImageViewportInternal::PreparedPayload payload = primaryPayload;
    if (synchronization.pendingSecondaryProviderCommit
        && !display.secondaryPendingRenderPayload.image.isNull()) {
        return display.secondaryPendingRenderPayload;
    }
    if (synchronization.pendingTargetCommit && request.secondarySequence
        && !request.secondarySequenceIsProvider) {
        payload.image
            = viewport.secondarySequenceFrameImage(request.secondaryActiveRequest.target.frame);
        return payload;
    }
    payload.image = display.secondaryDisplayedImage;
    return payload;
}

void appendRenderLayer(QVector<ViewportRenderLayer>& layers, ImageViewport::PageRole role,
    const ImageViewportInternal::PreparedPayload& payload, const QRectF& targetRect,
    const QRectF& sourceRect, const ImageViewportInternal::PresentationState& presentation,
    bool requirePresentableRects)
{
    if (payload.image.isNull()) {
        return;
    }
    if (requirePresentableRects && (targetRect.isEmpty() || sourceRect.isEmpty())) {
        return;
    }
    layers.append({ role, payload, targetRect, sourceRect, presentation.rotationDegrees,
        presentation.mirrorHorizontally, presentation.mirrorVertically });
}

ViewportRenderSnapshot renderSnapshotForSynchronization(ViewportControllerPort viewport,
    const ViewportRenderSynchronization& synchronization,
    const ImageViewportInternal::PresentationState& presentation)
{
    ViewportRenderSnapshot snapshot;
    snapshot.itemSize = QSizeF(viewport.width(), viewport.height());
    snapshot.backgroundMode = presentation.backgroundMode;
    snapshot.backgroundColor = presentation.backgroundColor;
    snapshot.smoothing = presentation.smoothing;
    snapshot.mipmap = presentation.mipmap;
    snapshot.rotationDegrees = presentation.rotationDegrees;
    snapshot.mirrorHorizontally = presentation.mirrorHorizontally;
    snapshot.mirrorVertically = presentation.mirrorVertically;

    const ImageViewportInternal::PreparedPayload primaryPayload
        = primaryRenderPayload(viewport, synchronization);
    snapshot.preparedPayload = primaryPayload;
    snapshot.targetRect
        = renderTargetRect(synchronization.geometryState, ImageViewport::PageRole::Primary);
    snapshot.sourceRect
        = renderSourceRect(synchronization.geometryState, ImageViewport::PageRole::Primary);
    appendRenderLayer(snapshot.imageLayers, ImageViewport::PageRole::Primary, primaryPayload,
        snapshot.targetRect, snapshot.sourceRect, presentation, false);

    const ImageViewportInternal::PreparedPayload secondaryPayload
        = secondaryRenderPayload(viewport, synchronization, primaryPayload);
    appendRenderLayer(snapshot.imageLayers, ImageViewport::PageRole::Secondary, secondaryPayload,
        renderTargetRect(synchronization.geometryState, ImageViewport::PageRole::Secondary),
        renderSourceRect(synchronization.geometryState, ImageViewport::PageRole::Secondary),
        presentation, true);
    return snapshot;
}

QPointF clampedPoint(QPointF point, QPointF minimum, QPointF maximum)
{
    return QPointF(std::clamp(point.x(), minimum.x(), maximum.x()),
        std::clamp(point.y(), minimum.y(), maximum.y()));
}

bool applyContentPosition(ViewportControllerPort& viewport,
    ImageViewportInternal::PresentationState& presentation, QPointF requestedPosition)
{
    const PresentationGeometry::State geometry = controllerGeometryState(viewport, presentation);
    if (PresentationGeometry::contentRect(geometry).isEmpty() || geometry.itemBounds.isEmpty()) {
        return false;
    }

    const QPointF currentPosition = PresentationGeometry::contentPosition(geometry);
    const QPointF maximum = PresentationGeometry::maximumContentPosition(geometry);
    const QPointF nextPosition = clampedPoint(requestedPosition, {}, maximum);
    if (nextPosition == currentPosition && presentation.contentPosition == nextPosition) {
        return false;
    }

    presentation.contentPosition = nextPosition;
    return true;
}

bool applyContentPositionForGeometry(ImageViewportInternal::PresentationState& presentation,
    const PresentationGeometry::State& geometry, QPointF requestedPosition)
{
    if (PresentationGeometry::contentRect(geometry).isEmpty() || geometry.itemBounds.isEmpty()) {
        return false;
    }

    const QPointF currentPosition = PresentationGeometry::contentPosition(geometry);
    const QPointF maximum = PresentationGeometry::maximumContentPosition(geometry);
    const QPointF nextPosition = clampedPoint(requestedPosition, {}, maximum);
    if (nextPosition == currentPosition && presentation.contentPosition == nextPosition) {
        return false;
    }

    presentation.contentPosition = nextPosition;
    return true;
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

bool clampPresentationContentPositionToBounds(
    ViewportControllerPort& viewport, ImageViewportInternal::PresentationState& presentation)
{
    const PresentationGeometry::State currentGeometry
        = controllerGeometryState(viewport, presentation);
    if (PresentationGeometry::contentRect(currentGeometry).isEmpty()
        || currentGeometry.itemBounds.isEmpty()) {
        return false;
    }

    const QPointF clampedPosition = PresentationGeometry::contentPosition(currentGeometry);
    if (presentation.contentPosition == clampedPosition) {
        return false;
    }

    presentation.contentPosition = clampedPosition;
    return true;
}

void preserveAnchoredContentPosition(ViewportControllerPort& viewport,
    ImageViewportInternal::PresentationState& presentation,
    const PresentationGeometry::State& previousGeometry, QPointF anchor)
{
    const CoordinateResult anchoredSpreadPoint
        = PresentationGeometry::itemToSpread(previousGeometry, anchor.x(), anchor.y());
    if (!anchoredSpreadPoint.isValid()) {
        clampPresentationContentPositionToBounds(viewport, presentation);
        return;
    }

    const PresentationGeometry::State nextGeometry
        = controllerGeometryState(viewport, presentation);
    presentation.contentPosition = PresentationGeometry::contentPositionForAnchoredSpreadPoint(
        nextGeometry, QPointF(anchoredSpreadPoint.x(), anchoredSpreadPoint.y()), anchor);
}

ImageViewportInternal::ViewportChangeSet presentationChanges(
    ViewportControllerPort& viewport, bool affectsGeometry)
{
    ImageViewportInternal::ViewportChangeSet changes;
    changes.presentation = true;
    changes.displayRevision = true;
    changes.geometryState
        = affectsGeometry && viewport.hasReadyDisplay() && !viewport.itemBounds().isEmpty();
    changes.scheduleUpdate = true;
    return changes;
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

ViewportCommandResult acceptedPresentationCommand(
    ViewportControllerPort& viewport, ImageViewportInternal::ViewportChangeSet changes = {})
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    result.changes = changes;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    return result;
}

ViewportCommandResult invalidPresentationCommand(ViewportControllerPort& viewport)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Invalid;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
    return result;
}

ImageViewportInternal::ViewportChangeSet applyPresentationTransition(
    ViewportControllerPort& viewport, ImageViewportInternal::PresentationState& presentation,
    const ControllerTransitionPolicy& policy, QPointF previousContentPosition)
{
    ImageViewportInternal::ViewportChangeSet changes;
    auto markChanged = [&]() { mergeChanges(changes, presentationChanges(viewport, true)); };

    if (policy.magnificationPolicy == PageSetTransitionPolicy::ZoomTransition::ResetToContain) {
        if (presentation.fitMode != ImageViewport::FitMode::Contain
            || presentation.manualZoom != 1.0) {
            presentation.fitMode = ImageViewport::FitMode::Contain;
            presentation.manualZoom = 1.0;
            markChanged();
        }
    }
    if (policy.explicitFitMode && presentation.fitMode != *policy.explicitFitMode) {
        presentation.fitMode = *policy.explicitFitMode;
        markChanged();
    }
    if (policy.rotationTransition == PageSetTransitionPolicy::RotationTransition::Reset
        && presentation.rotationDegrees != 0) {
        presentation.rotationDegrees = 0;
        markChanged();
    }
    if (policy.mirrorTransition == PageSetTransitionPolicy::MirrorTransition::Reset
        && (presentation.mirrorHorizontally || presentation.mirrorVertically)) {
        presentation.mirrorHorizontally = false;
        presentation.mirrorVertically = false;
        markChanged();
    }
    if (policy.explicitSpreadDirection
        && presentation.spreadDirection != *policy.explicitSpreadDirection) {
        presentation.spreadDirection = *policy.explicitSpreadDirection;
        markChanged();
    }
    if (policy.explicitPageGap && presentation.pageGap != *policy.explicitPageGap) {
        presentation.pageGap = *policy.explicitPageGap;
        markChanged();
    }

    if (policy.contentPositionTransition
        == PageSetTransitionPolicy::ContentPositionTransition::ScanStart) {
        if (applyContentPositionForGeometry(
                presentation, acceptedGeometryState(viewport, presentation), {})) {
            markChanged();
        }
    } else if (policy.contentPositionTransition
        == PageSetTransitionPolicy::ContentPositionTransition::ScanEnd) {
        const PresentationGeometry::State geometry = acceptedGeometryState(viewport, presentation);
        if (applyContentPositionForGeometry(
                presentation, geometry, PresentationGeometry::maximumContentPosition(geometry))) {
            markChanged();
        }
    } else if (policy.contentPositionTransition
        == PageSetTransitionPolicy::ContentPositionTransition::Clamp) {
        if (applyContentPositionForGeometry(presentation,
                acceptedGeometryState(viewport, presentation), previousContentPosition)) {
            markChanged();
        }
    }

    return changes;
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

ViewportProviderFrameTerminalResult frameTerminalResultFor(
    const ViewportProviderTerminalEvent& event)
{
    switch (event.kind) {
    case ViewportProviderTerminalEvent::Kind::Unsupported:
        return {
            ImageViewport::RequestStatus::Unsupported,
            event.unsupportedCause
                    == ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest
                ? ImageViewport::RequestReason::UnsupportedRequest
                : ImageViewport::RequestReason::PayloadRejection,
            event.diagnostic,
            QStringLiteral("provider unsupported"),
        };
    case ViewportProviderTerminalEvent::Kind::Cancellation:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider cancelled request"),
        };
    case ViewportProviderTerminalEvent::Kind::Failure:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider failure"),
        };
    }

    return {};
}

ViewportProviderMetadataTerminalResult metadataTerminalResultFor(
    const ViewportProviderTerminalEvent& event)
{
    switch (event.kind) {
    case ViewportProviderTerminalEvent::Kind::Unsupported:
        return {
            ImageViewport::RequestStatus::Unsupported,
            event.unsupportedCauseExplicit
                    && event.unsupportedCause
                        == ImageSequenceProviderSession::UnsupportedCause::PayloadRejection
                ? ImageViewport::RequestReason::PayloadRejection
                : ImageViewport::RequestReason::UnsupportedRequest,
            event.diagnostic,
            QStringLiteral("provider unsupported"),
        };
    case ViewportProviderTerminalEvent::Kind::Cancellation:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider cancelled request"),
        };
    case ViewportProviderTerminalEvent::Kind::Failure:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider failure"),
        };
    }

    return {};
}

void setPlaybackPhase(ViewportControllerPort& viewport, ViewportCommandResult& result,
    ImageViewport::PlaybackPhase phase)
{
    if (viewportRequestState(viewport).playbackPhase == phase) {
        return;
    }

    viewportRequestState(viewport).playbackPhase = phase;
    result.changes.playbackPhase = true;
}

void appendProviderFrameQueueResult(
    ViewportProviderFrameTransportEffect& effect, ViewportProviderFrameQueueResult queue)
{
    effect.cancelToken = queue.cancelToken;
    effect.deferredControllerEvent = queue.deferredControllerEvent;
}

void appendProviderFrameStartResult(ViewportProviderFrameTransportEffect& effect,
    const ViewportProviderFrameRequestStartResult& start)
{
    effect.closeSession = start.closeSession;
    effect.sessionClose = start.sessionClose;
    effect.sendCommand = start.sendCommand;
    effect.command = start.command;
}

void appendProviderMetadataStartResult(ViewportProviderMetadataTransportEffect& effect,
    const ViewportProviderMetadataRequestStartResult& start)
{
    effect.closeSession = start.closeSession;
    effect.sessionClose = start.sessionClose;
    effect.sendCommand = start.sendCommand;
    effect.token = start.token;
}

void clearQueuedProviderFrameRequest(ImageViewportInternal::ProviderGenerationState& provider)
{
    provider.queuedFrameRequest = false;
    provider.queuedFrameGeneration = 0;
    provider.queuedFrameRequestId = 0;
    provider.queuedFrame = -1;
    provider.queuedPosition = -1;
    provider.queuedResolvedFrame = {};
    provider.queuedFrameFromPlayback = false;
    provider.queuedFrameTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
}

void clearQueuedProviderFrameRequest(
    ViewportControllerState& state, ImageViewport::PageRole role)
{
    clearQueuedProviderFrameRequest(providerGenerationStateForRole(state, role));
}

void clearQueuedProviderFrameRequest(ViewportControllerPort& viewport)
{
    clearQueuedProviderFrameRequest(viewportProviderState(viewport));
}

void publishProviderTokenExhaustion(ViewportControllerPort& viewport)
{
    clearQueuedProviderFrameRequest(viewport);
    viewportProviderState(viewport).activeMetadataToken = {};
    viewportProviderState(viewport).activeFrameToken = {};
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Error;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderFailure;
    viewportRequestState(viewport).errorString = QStringLiteral("provider request token exhausted");
    viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Stopped;
}

DisplayRequestTarget providerLatestNonPlaybackTarget(ViewportControllerPort& viewport)
{
    DisplayRequestTarget target;
    target.frame = viewportRequestState(viewport).latestNonPlaybackRequest.target.frame;
    target.position = viewportRequestState(viewport).latestNonPlaybackRequest.target.position;
    target.providerTargetKind
        = viewportRequestState(viewport).latestNonPlaybackRequest.target.providerTargetKind;
    return target;
}

DisplayRequestTarget providerStopRestoreTarget(ViewportControllerPort& viewport)
{
    DisplayRequestTarget target = providerLatestNonPlaybackTarget(viewport);
    if (target.frame < 0 && target.position >= 0) {
        target.frame = viewport.providerFrameIndexForPosition(target.position);
    }
    if (target.frame < 0 && target.position < 0
        && viewportRequestState(viewport).activeRequest.target.frame >= 0) {
        target.frame = viewportRequestState(viewport).activeRequest.target.frame;
        target.position = viewport.providerFrameStartPosition(target.frame);
    }
    if (target.position < 0 && target.frame >= 0) {
        target.position = viewport.providerFrameStartPosition(target.frame);
    }
    if (target.providerTargetKind == ImageViewportInternal::ProviderRequestTargetKind::Unknown
        && target.frame >= 0) {
        target.providerTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Frame;
    }
    return target;
}

ImageViewportInternal::ResolvedFrameIdentity providerStopRestoreResolvedFrame(
    ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    if (viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame.isValid()
        && viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame.frame
            == target.frame) {
        return viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame;
    }
    if (target.frame < 0) {
        return {};
    }
    return { target.frame, viewport.providerFrameStartPosition(target.frame) };
}

void beginStopRestoreDisplayRequest(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    viewportRequestState(viewport).beginDisplayRequest(
        ImageViewportInternal::DisplayRequestOrigin::StopRestore, target, resolvedFrame, true);
    viewportRequestState(viewport).playbackPosition = target.position;
}

void applyStopRestoreTarget(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    beginStopRestoreDisplayRequest(viewport, target, resolvedFrame);
}

void applyProviderStopRestoreTarget(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    beginStopRestoreDisplayRequest(viewport, target, resolvedFrame);
}

void beginAcceptedDisplayRequest(ViewportControllerPort& viewport,
    ImageViewportInternal::DisplayRequestOrigin origin, DisplayRequestTarget target,
    bool rememberAsLatestNonPlayback)
{
    viewportRequestState(viewport).beginDisplayRequest(origin, target, rememberAsLatestNonPlayback);
}

void beginAcceptedDisplayRequest(ViewportControllerPort& viewport,
    ImageViewportInternal::DisplayRequestOrigin origin, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame, bool rememberAsLatestNonPlayback)
{
    viewportRequestState(viewport).beginDisplayRequest(
        origin, target, resolvedFrame, rememberAsLatestNonPlayback);
}

bool stopRestoreTargetIsReadyDisplay(ViewportControllerPort& viewport)
{
    return viewport.hasReadyDisplay()
        && viewportDisplayState(viewport).displayedRequest.generation
        == viewportRequestState(viewport).sequenceGeneration
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.frame
        == viewportRequestState(viewport).activeRequest.resolvedFrame.frame
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.position
        == viewportRequestState(viewport).activeRequest.resolvedFrame.position;
}

void publishProviderStopRestoreLoading(ViewportControllerPort& viewport);

enum class StopRestoreWaitingState {
    ProviderLoading,
    RenderWaiting,
};

struct StopRestorePublication
{
    bool readyDisplay = false;
    ImageViewport::DisplayStatus oldDisplayStatus = ImageViewport::DisplayStatus::Empty;
};

enum class StopRestorePlanKind {
    None,
    ProviderPendingMetadata,
    ProviderQueuedPlayback,
    ProviderActivePlayback,
    BuiltInRenderWait,
};

struct StopRestorePlan
{
    StopRestorePlanKind kind = StopRestorePlanKind::None;
    DisplayRequestTarget target;
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame;
};

StopRestorePlan stopRestorePlanFor(ViewportControllerPort& viewport)
{
    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewportRequestState(viewport).activeRequest.target.frame < 0
        && viewportRequestState(viewport).activeRequest.target.position < 0) {
        return { StopRestorePlanKind::ProviderPendingMetadata,
            providerLatestNonPlaybackTarget(viewport),
            viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame };
    }
    if (viewport.hasProviderSequence() && viewportProviderState(viewport).timedMetadata
        && viewportProviderState(viewport).queuedFrameRequest
        && viewportProviderState(viewport).queuedFrameFromPlayback) {
        const DisplayRequestTarget target = providerStopRestoreTarget(viewport);
        return { StopRestorePlanKind::ProviderQueuedPlayback, target,
            providerStopRestoreResolvedFrame(viewport, target) };
    }
    if (viewport.hasProviderSequence() && viewportProviderState(viewport).timedMetadata
        && activeProviderFrameRequestIsPlayback(viewport)) {
        const DisplayRequestTarget target = providerStopRestoreTarget(viewport);
        return { StopRestorePlanKind::ProviderActivePlayback, target,
            providerStopRestoreResolvedFrame(viewport, target) };
    }
    if (viewport.hasTimedSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting
        && (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewportRequestState(viewport).latestNonPlaybackRequest.target.frame >= 0
        && viewportRequestState(viewport).activeRequest.target.frame
            != viewportRequestState(viewport).latestNonPlaybackRequest.target.frame) {
        return { StopRestorePlanKind::BuiltInRenderWait,
            DisplayRequestTarget {
                viewportRequestState(viewport).latestNonPlaybackRequest.target.frame,
                viewportRequestState(viewport).latestNonPlaybackRequest.target.position },
            viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame };
    }
    return {};
}

void discardPendingRenderCommit(ViewportControllerPort& viewport)
{
    viewportDisplayState(viewport).clearPendingRenderPayload();
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
}

void publishProviderTokenExhaustion(
    ViewportControllerPort& viewport, ViewportControllerState& state, ImageViewport::PageRole role)
{
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    clearQueuedProviderFrameRequest(provider);
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Error;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderFailure;
    viewportRequestState(viewport).errorString = QStringLiteral("provider request token exhausted");
    viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
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

ImageViewportInternal::TargetSpreadRoleTerminalState& targetSpreadTerminalForRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? request.targetSpreadTerminal.primary
                                                    : request.targetSpreadTerminal.secondary;
}

const ImageViewportInternal::TargetSpreadRoleTerminalState* targetSpreadTerminalForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? &request.targetSpreadTerminal.primary
                                                    : &request.targetSpreadTerminal.secondary;
}

bool targetSpreadTerminalMatchesActiveRequest(ViewportControllerPort viewport)
{
    const auto& request = viewportRequestState(viewport);
    return request.targetSpreadTerminal.sealed
        && request.targetSpreadTerminal.generation == request.sequenceGeneration
        && request.targetSpreadTerminal.requestId == request.activeRequest.identity.id;
}

bool targetSpreadTerminalSealedForActiveRequest(ViewportControllerPort viewport)
{
    return targetSpreadTerminalMatchesActiveRequest(viewport);
}

bool targetSpreadRequiresRole(ViewportControllerPort viewport, ImageViewport::PageRole role)
{
    return static_cast<bool>(sequenceForRole(viewportRequestState(viewport), role));
}

const ImageViewportInternal::TargetSpreadRoleTerminalState* currentTerminalForRole(
    ViewportControllerPort viewport, ImageViewport::PageRole role)
{
    if (!targetSpreadTerminalMatchesActiveRequest(viewport)
        || !targetSpreadRequiresRole(viewport, role)) {
        return nullptr;
    }

    const ImageViewportInternal::RequestState& request = viewportRequestState(viewport);
    const auto* terminal = targetSpreadTerminalForRole(request, role);
    return terminal->terminal ? terminal : nullptr;
}

const ImageViewportInternal::TargetSpreadRoleTerminalState* projectedTargetSpreadTerminal(
    ViewportControllerPort viewport)
{
    const auto* primary = currentTerminalForRole(viewport, ImageViewport::PageRole::Primary);
    const auto* secondary = currentTerminalForRole(viewport, ImageViewport::PageRole::Secondary);
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

bool targetSpreadTerminalHasGenerationScope(ViewportControllerPort viewport)
{
    if (!targetSpreadTerminalMatchesActiveRequest(viewport)) {
        return false;
    }

    const auto& terminal = viewportRequestState(viewport).targetSpreadTerminal;
    return (terminal.primary.terminal
               && terminal.primary.failureScope == ImageViewportInternal::FailureScope::Generation)
        || (terminal.secondary.terminal
            && terminal.secondary.failureScope == ImageViewportInternal::FailureScope::Generation);
}

bool hasGenerationTerminalProviderFailure(ViewportControllerPort viewport)
{
    return viewport.hasGenerationTerminalProviderFailure()
        || targetSpreadTerminalHasGenerationScope(viewport);
}

void clearDisplayRequestTerminalForAcceptedRequest(ViewportControllerPort& viewport)
{
    if (!targetSpreadTerminalMatchesActiveRequest(viewport)
        || targetSpreadTerminalHasGenerationScope(viewport)) {
        return;
    }

    viewportRequestState(viewport).targetSpreadTerminal.clear();
}

void publishTargetSpreadTerminalProjection(
    ViewportControllerPort& viewport, ImageViewportInternal::ViewportChangeSet& changes)
{
    const auto* terminal = projectedTargetSpreadTerminal(viewport);
    if (!terminal) {
        return;
    }

    const bool diagnosticsValueChanged
        = viewportRequestState(viewport).errorString != terminal->diagnostic;
    viewportRequestState(viewport).status = terminal->status;
    viewportRequestState(viewport).reason = terminal->reason;
    viewportRequestState(viewport).errorString = terminal->diagnostic;
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = changes.diagnostics || diagnosticsValueChanged;
}

void recordTargetSpreadTerminal(ViewportControllerPort& viewport, ImageViewport::PageRole role,
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
    auto& roleTerminal = targetSpreadTerminalForRole(request, role);
    roleTerminal.terminal = true;
    roleTerminal.status = status;
    roleTerminal.reason = reason;
    roleTerminal.failureScope = failureScope;
    roleTerminal.diagnostic = diagnostic;
    publishTargetSpreadTerminalProjection(viewport, changes);
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

void setSecondaryActiveRequest(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
    bool rememberAsLatestNonPlayback = false)
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

void initializeSecondaryActiveRequest(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    setSecondaryActiveRequest(viewport, target, resolvedFrame, true);
}

bool targetRequiresSecondaryPayload(ViewportControllerPort& viewport)
{
    return hasSecondarySequence(viewport)
        && viewportRequestState(viewport).secondaryActiveRequest.target.frame >= 0;
}

bool secondaryPayloadReadyForPendingTarget(ViewportControllerPort& viewport)
{
    if (!targetRequiresSecondaryPayload(viewport)) {
        return true;
    }
    if (hasSecondaryProviderSequence(viewport)) {
        return !viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull();
    }
    return viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame.isValid();
}

bool hasPendingTargetSpreadPayload(ViewportControllerPort& viewport)
{
    return viewportDisplayState(viewport).pendingRenderPayload.commitPending
        && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull()
        && secondaryPayloadReadyForPendingTarget(viewport);
}

bool requestIsWaitingForRenderCommit(ViewportControllerPort& viewport)
{
    return viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
            || viewportRequestState(viewport).reason
                == ImageViewport::RequestReason::RenderWaiting);
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

void publishSecondaryDisplayedRequest(ViewportControllerPort& viewport)
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
    if (!viewportRequestState(viewport).secondarySequenceIsProvider) {
        viewportDisplayState(viewport).secondaryDisplayedImage
            = viewport.secondarySequenceFrameImage(snapshot.request.target.frame);
    }
}

void stageBuiltInPrimarySpreadPayload(ViewportControllerPort& viewport)
{
    viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
        viewport.hasDisplayableSequence());
    viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
    viewportDisplayState(viewport).beginPreparedPayloadIdentity(
        viewportRequestState(viewport).sequenceGeneration,
        viewportRequestState(viewport).activeRequest);
    const int frame = viewportRequestState(viewport).activeRequest.target.frame;
    viewportDisplayState(viewport).pendingRenderPayload.image
        = frame >= 0 ? viewport.sequenceFrameImage(frame) : QImage();
}

void publishReadyDisplayState(ViewportControllerPort& viewport)
{
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Ready;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::Ready;
    viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Ready;
}

void publishRenderWaitingState(ViewportControllerPort& viewport)
{
    ImageViewportInternal::TargetSpreadWaitState waitState;
    waitState.requiresSecondary = targetRequiresSecondaryPayload(viewport);
    waitState.primary.renderWaiting = true;
    if (waitState.requiresSecondary) {
        waitState.secondary.renderWaiting = true;
    }
    publishLoadingWaitState(viewport, waitState);
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
}

void publishUploadPendingState(ViewportControllerPort& viewport)
{
    ImageViewportInternal::TargetSpreadWaitState waitState;
    waitState.requiresSecondary = targetRequiresSecondaryPayload(viewport);
    waitState.primary.uploadPending = true;
    if (waitState.requiresSecondary) {
        waitState.secondary.uploadPending = true;
    }
    publishLoadingWaitState(viewport, waitState);
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
}

void publishPendingRenderState(ViewportControllerPort& viewport)
{
    if (viewport.itemBounds().isEmpty()) {
        publishRenderWaitingState(viewport);
    } else {
        publishUploadPendingState(viewport);
    }
}

void publishSequenceReadyState(ViewportControllerPort& viewport, const QImage& providerImage = {})
{
    viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
        viewport.hasDisplayableSequence());
    publishReadyDisplayState(viewport);
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
    if (viewport.hasProviderSequence()) {
        if (!providerImage.isNull()) {
            viewportDisplayState(viewport).displayedImage = providerImage;
        } else if (!viewportDisplayState(viewport).pendingRenderPayload.image.isNull()) {
            viewportDisplayState(viewport).displayedImage
                = viewportDisplayState(viewport).pendingRenderPayload.image;
        }
        viewportDisplayState(viewport).pendingRenderPayload.image = {};
    } else {
        viewportDisplayState(viewport).displayedImage = viewport.sequenceFrameImage(
            viewportDisplayState(viewport).displayedRequest.request.target.frame);
    }
    publishSecondaryDisplayedRequest(viewport);
}

void publishStagedBuiltInPrimarySpreadReadyState(ViewportControllerPort& viewport)
{
    publishReadyDisplayState(viewport);
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
    publishSecondaryDisplayedRequest(viewport);
}

void publishAcceptedTargetState(ViewportControllerPort& viewport, const QImage& providerImage = {})
{
    if (viewport.hasProviderSequence() && !providerImage.isNull()) {
        viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
            viewport.hasDisplayableSequence());
        viewportDisplayState(viewport).pendingRenderPayload.image = providerImage;
        viewportDisplayState(viewport).beginPreparedPayloadIdentity(
            viewportRequestState(viewport).sequenceGeneration,
            viewportRequestState(viewport).activeRequest);
        if (viewport.itemBounds().isEmpty()) {
            publishRenderWaitingState(viewport);
        } else {
            publishUploadPendingState(viewport);
            viewportDisplayState(viewport).pendingRenderPayload.commitPending = false;
        }
        viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
        return;
    }
    clearDisplayRequestTerminalForAcceptedRequest(viewport);
    stageBuiltInPrimarySpreadPayload(viewport);
    publishPendingRenderState(viewport);
}

void publishSequenceReadyState(
    ViewportControllerPort& viewport, const ImageViewportInternal::PreparedPayload& providerPayload)
{
    if (!viewport.hasProviderSequence() || providerPayload.image.isNull()) {
        publishSequenceReadyState(viewport, providerPayload.image);
        return;
    }

    viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
        viewport.hasDisplayableSequence());
    publishReadyDisplayState(viewport);
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
    publishSecondaryDisplayedRequest(viewport);
}

void publishAcceptedTargetState(
    ViewportControllerPort& viewport, const ImageViewportInternal::PreparedPayload& providerPayload)
{
    if (viewport.hasProviderSequence() && !providerPayload.image.isNull()) {
        viewportDisplayState(viewport).captureRenderFailureRetainedDisplay(
            viewport.hasDisplayableSequence());
        viewportDisplayState(viewport).commitPreparedPayloadIdentity(
            viewportRequestState(viewport).activeRequest, providerPayload);
        if (viewport.itemBounds().isEmpty()) {
            publishRenderWaitingState(viewport);
        } else {
            publishUploadPendingState(viewport);
            viewportDisplayState(viewport).pendingRenderPayload.commitPending = false;
        }
        viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
        return;
    }
    publishAcceptedTargetState(viewport, providerPayload.image);
}

void publishProviderFrameLoadingState(ViewportControllerPort& viewport)
{
    clearDisplayRequestTerminalForAcceptedRequest(viewport);
    ImageViewportInternal::TargetSpreadWaitState waitState;
    waitState.primary.providerWaiting = true;
    publishLoadingWaitState(viewport, waitState);
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    discardPendingRenderCommit(viewport);
}

StopRestorePublication publishStopRestoreTarget(ViewportControllerPort& viewport,
    DisplayRequestTarget target, ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
    StopRestoreWaitingState waitingState)
{
    StopRestorePublication publication;
    publication.oldDisplayStatus = viewportDisplayState(viewport).status;
    if (waitingState == StopRestoreWaitingState::ProviderLoading) {
        applyProviderStopRestoreTarget(viewport, target, resolvedFrame);
    } else {
        applyStopRestoreTarget(viewport, target, resolvedFrame);
    }

    publication.readyDisplay = stopRestoreTargetIsReadyDisplay(viewport);
    if (publication.readyDisplay) {
        publishReadyDisplayState(viewport);
    } else if (waitingState == StopRestoreWaitingState::ProviderLoading) {
        publishProviderStopRestoreLoading(viewport);
    } else {
        publishRenderWaitingState(viewport);
    }
    return publication;
}

void completeStopRestoreRequest(ViewportControllerPort& viewport, ViewportCommandResult& result)
{
    setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
    result.changes.requestRevision = true;
    result.changes.requestState = true;
}

ImageViewportInternal::PreparedPayloadIdentity acknowledgementPayloadForRole(
    const ViewportRenderAcknowledgement& acknowledgement, ImageViewport::PageRole role)
{
    for (const ViewportRenderRolePayload& rolePayload : acknowledgement.rolePayloads) {
        if (rolePayload.role == role) {
            return rolePayload.preparedPayload;
        }
    }
    return role == ImageViewport::PageRole::Primary
        ? acknowledgement.preparedPayload
        : ImageViewportInternal::PreparedPayloadIdentity {};
}

ImageViewportInternal::PreparedPayloadIdentity expectedRenderPayloadForRole(
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    if (role == ImageViewport::PageRole::Primary) {
        return viewportDisplayState(viewport).pendingRenderPayload.identity();
    }
    if (!hasSecondarySequence(viewport)) {
        return {};
    }
    const ImageViewportInternal::PreparedPayloadIdentity secondaryIdentity
        = viewportDisplayState(viewport).secondaryPendingRenderPayload.identity();
    return secondaryIdentity.isValid()
        ? secondaryIdentity
        : viewportDisplayState(viewport).pendingRenderPayload.identity();
}

bool renderPayloadMatches(ImageViewportInternal::PreparedPayloadIdentity actual,
    ImageViewportInternal::PreparedPayloadIdentity expected)
{
    return actual.isValid() && expected.isValid() && actual.generation == expected.generation
        && actual.requestId == expected.requestId && actual.payloadId == expected.payloadId;
}

bool primaryRenderAcknowledgementMatchesPending(
    ViewportControllerPort& viewport, const ViewportRenderAcknowledgement& acknowledgement)
{
    const ImageViewportInternal::PreparedPayloadIdentity primaryPayload
        = acknowledgementPayloadForRole(acknowledgement, ImageViewport::PageRole::Primary);
    return viewportDisplayState(viewport).pendingRenderPayloadMatches(primaryPayload)
        && viewportRequestState(viewport).activeRequestOwnsPreparedPayload(primaryPayload);
}

bool renderCommitAcknowledgementMatchesPending(
    ViewportControllerPort& viewport, const ViewportRenderAcknowledgement& acknowledgement)
{
    if (!primaryRenderAcknowledgementMatchesPending(viewport, acknowledgement)) {
        return false;
    }
    if (!hasSecondarySequence(viewport)) {
        return true;
    }
    return renderPayloadMatches(
        acknowledgementPayloadForRole(acknowledgement, ImageViewport::PageRole::Secondary),
        expectedRenderPayloadForRole(viewport, ImageViewport::PageRole::Secondary));
}

bool renderFailureAcknowledgementMatchesPending(
    ViewportControllerPort& viewport, const ViewportRenderAcknowledgement& acknowledgement)
{
    if (acknowledgement.failedRole == ImageViewport::PageRole::Primary) {
        return primaryRenderAcknowledgementMatchesPending(viewport, acknowledgement);
    }
    return renderPayloadMatches(
        acknowledgementPayloadForRole(acknowledgement, ImageViewport::PageRole::Secondary),
        expectedRenderPayloadForRole(viewport, ImageViewport::PageRole::Secondary));
}

void setPlaybackPhase(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, ImageViewport::PlaybackPhase phase)
{
    if (viewportRequestState(viewport).playbackPhase == phase) {
        return;
    }

    viewportRequestState(viewport).playbackPhase = phase;
    changes.playbackPhase = true;
}

void publishProviderStopRestoreLoading(ViewportControllerPort& viewport)
{
    publishProviderFrameLoadingState(viewport);
}

bool appendProviderStopRestoreFrameStart(
    ViewportController& controller, ViewportControllerPort& viewport, ViewportCommandResult& result)
{
    if (!viewportProviderState(viewport).session
        || viewportRequestState(viewport).activeRequest.target.frame < 0) {
        return true;
    }

    DisplayRequestTarget target = viewportRequestState(viewport).activeRequest.target;
    target.providerTargetKind
        = viewportRequestState(viewport).latestNonPlaybackRequest.target.providerTargetKind;
    const ViewportProviderFrameRequestStartResult start
        = controller.startProviderFrameRequest({ target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (start.accepted) {
        return true;
    }

    result.changes.requestRevision = true;
    result.changes.requestState = true;
    result.changes.diagnostics = true;
    return false;
}

bool applyStopRestorePlan(ViewportController& controller, ViewportControllerPort& viewport,
    StopRestorePlan plan, ViewportCommandResult& result)
{
    switch (plan.kind) {
    case StopRestorePlanKind::ProviderPendingMetadata:
        applyProviderStopRestoreTarget(viewport, plan.target, plan.resolvedFrame);
        viewportRequestState(viewport).providerPlaybackStartPending = false;
        completeStopRestoreRequest(viewport, result);
        return true;
    case StopRestorePlanKind::ProviderQueuedPlayback: {
        clearQueuedProviderFrameRequest(viewport);

        const StopRestorePublication publication = publishStopRestoreTarget(
            viewport, plan.target, plan.resolvedFrame, StopRestoreWaitingState::ProviderLoading);
        if (!publication.readyDisplay) {
            if (!appendProviderStopRestoreFrameStart(controller, viewport, result)) {
                return true;
            }
        }
        completeStopRestoreRequest(viewport, result);
        return true;
    }
    case StopRestorePlanKind::ProviderActivePlayback: {
        if (viewportProviderState(viewport).session) {
            result.providerFrameTransport.cancelToken
                = viewportProviderState(viewport).activeFrameToken;
        }
        viewportProviderState(viewport).activeFrameToken = {};

        const StopRestorePublication publication = publishStopRestoreTarget(
            viewport, plan.target, plan.resolvedFrame, StopRestoreWaitingState::ProviderLoading);
        if (publication.readyDisplay) {
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            completeStopRestoreRequest(viewport, result);
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return true;
        }
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        if (!appendProviderStopRestoreFrameStart(controller, viewport, result)) {
            return true;
        }
        completeStopRestoreRequest(viewport, result);
        result.changes.diagnostics = diagnosticsValueChanged;
        return true;
    }
    case StopRestorePlanKind::BuiltInRenderWait: {
        const StopRestorePublication publication = publishStopRestoreTarget(
            viewport, plan.target, plan.resolvedFrame, StopRestoreWaitingState::RenderWaiting);
        completeStopRestoreRequest(viewport, result);
        result.changes.displayRevision
            = viewportDisplayState(viewport).status != publication.oldDisplayStatus;
        result.changes.displayState = result.changes.displayRevision;
        result.changes.scheduleUpdate = true;
        return true;
    }
    case StopRestorePlanKind::None:
        return false;
    }
    return false;
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

void applyProviderPlaybackStartTarget(ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).activeRequest.target.frame = target.frame;
    viewportRequestState(viewport).activeRequest.target.position = target.position;
    viewportRequestState(viewport).activeRequest.resolvedFrame
        = { target.frame, viewport.providerFrameStartPosition(target.frame) };
    viewportRequestState(viewport).playbackPosition = target.position;
    viewportRequestState(viewport).activeRequest.target.providerTargetKind
        = target.providerTargetKind;
}

DisplayRequestTarget pendingProviderPlaybackTarget()
{
    return DisplayRequestTarget { -1, -1,
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

void applyPendingProviderPlaybackTarget(
    ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    viewportRequestState(viewport).providerPlaybackStartPending = true;
    viewportRequestState(viewport).activeRequest.target.frame = target.frame;
    viewportRequestState(viewport).activeRequest.target.position = target.position;
    viewportRequestState(viewport).activeRequest.resolvedFrame = { -1, -1 };
    viewportRequestState(viewport).playbackPosition = target.position;
    viewportRequestState(viewport).activeRequest.target.providerTargetKind
        = target.providerTargetKind;
}

void applyPendingSecondaryProviderPlaybackTarget(
    ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    viewportRequestState(viewport).secondaryActiveRequest.target = target;
    viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame = { -1, -1 };
    viewportRequestState(viewport).secondaryActiveRequest.providerFrameToken = {};
    viewportRequestState(viewport).playbackPosition = target.position;
}

template <typename FrameStartFor>
int playbackStartPosition(ViewportControllerPort& viewport, FrameStartFor frameStartFor)
{
    const auto& target = viewportRequestState(viewport).activeRequest.target;
    return target.position >= 0 ? target.position : frameStartFor(target.frame);
}

template <typename FrameStartFor>
void seedPlaybackPosition(ViewportControllerPort& viewport, FrameStartFor frameStartFor)
{
    viewportRequestState(viewport).playbackPosition
        = playbackStartPosition(viewport, frameStartFor);
}

ImageViewport::PlaybackPhase playbackPhaseForCurrentRequest(ViewportControllerPort& viewport)
{
    return viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

void armAuthoredAutoplayIfEligible(ViewportControllerPort& viewport)
{
    if (viewport.hasProviderSequence()) {
        const ImageSequenceAuthoredAnimationFacts facts = viewport.providerAuthoredAnimationFacts();
        if (!facts.autoplay() || viewport.providerTimedPlaybackCapabilityKnownFalse()) {
            return;
        }

        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        if (!viewportProviderState(viewport).metadataReady) {
            applyPendingProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
            viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Waiting;
            return;
        }
        if (viewportProviderState(viewport).timedMetadata
            && viewportProviderState(viewport).timedPlaybackSupport) {
            seedPlaybackPosition(viewport,
                [&viewport](int frame) { return viewport.providerFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackPhase = playbackPhaseForCurrentRequest(viewport);
        }
        return;
    }

    const ImageSequenceAuthoredAnimationFacts facts = viewport.sequenceAuthoredAnimationFacts();
    if (!viewport.hasTimedSequence() || !facts.autoplay()) {
        return;
    }

    viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    seedPlaybackPosition(
        viewport, [&viewport](int frame) { return viewport.sequenceFrameStartPosition(frame); });
    viewportRequestState(viewport).playbackPhase = playbackPhaseForCurrentRequest(viewport);
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

void updateLoopProgressForAcceptedPlaybackTarget(
    ViewportControllerPort& viewport, const PlaybackAdvanceTarget& target)
{
    if (target.looped && !viewportRequestState(viewport).looping) {
        ++viewportRequestState(viewport).playbackLoopIterationsCompleted;
    }
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

void applyPlaybackTarget(ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    beginAcceptedDisplayRequest(
        viewport, ImageViewportInternal::DisplayRequestOrigin::Playback, target, false);
}

void applyPlaybackAdvancePhase(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, const PlaybackAdvanceTarget& target)
{
    if (target.reachedEnd) {
        viewportRequestState(viewport).stopPlaybackWhenRequestReady
            = viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading;
    }
    setPlaybackPhase(viewport, changes,
        playbackAdvancePhaseForRequest(viewportRequestState(viewport).status, target.reachedEnd));
}

void appendPlaybackRequestChange(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, int previousFrame)
{
    changes.requestRevision = true;
    if (viewportRequestState(viewport).activeRequest.target.frame != previousFrame
        || viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready) {
        changes.displayRevision = true;
    }
    changes.requestState = true;
    changes.displayState = true;
}

ViewportCommandResult acceptExplicitSeek(ViewportController& controller,
    ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
    ExplicitSeekMaterialization materialization)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    beginAcceptedDisplayRequest(viewport, ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        target, resolvedFrame, true);
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).playbackPosition = target.position;

    switch (materialization) {
    case ExplicitSeekMaterialization::ProviderReady: {
        publishProviderFrameLoadingState(viewport);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ViewportProviderFrameDispatchResult dispatch
            = controller.dispatchProviderFrameRequest({ target });
        result.providerFrameTransport = dispatch.transport;
        if (!dispatch.accepted) {
            result.changes.requestRevision = true;
            result.changes.displayRevision = true;
            result.changes.requestState = true;
            result.changes.displayState = true;
            result.changes.diagnostics = true;
            result.changes.scheduleUpdate = true;
            return result;
        }
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing) {
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
        }
        result.changes.requestRevision = true;
        result.changes.displayRevision = true;
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }
    case ExplicitSeekMaterialization::ProviderPendingMetadata: {
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        discardPendingRenderCommit(viewport);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        return result;
    }
    case ExplicitSeekMaterialization::BuiltIn: {
        const QRectF oldContentRect = viewport.contentRect();
        const QRectF oldVisibleImageRect = viewport.visibleImageRect();
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        publishAcceptedTargetState(viewport);
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
        }
        result.changes.requestRevision = true;
        result.changes.displayRevision = true;
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.geometryState
            = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
            || ImageViewportInternal::rectsDifferExactly(
                viewport.visibleImageRect(), oldVisibleImageRect);
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }
    }

    return result;
}

ViewportCommandResult acceptExplicitSeek(ViewportController& controller,
    ViewportControllerPort& viewport, DisplayRequestTarget target,
    ExplicitSeekMaterialization materialization)
{
    return acceptExplicitSeek(
        controller, viewport, target, { target.frame, target.position }, materialization);
}
}
