#include "viewportcontroller_p.h"

#include "playbacktimeline_p.h"
#include "presentationgeometry_p.h"

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

int ViewportControllerContext::sequenceFrameIndexForPosition(int) const { return -1; }

int ViewportControllerContext::sequenceFrameStartPosition(int) const { return -1; }

ImageSequenceAuthoredAnimationFacts
ViewportControllerContext::sequenceAuthoredAnimationFacts() const
{
    return {};
}

bool ViewportControllerContext::hasSecondaryTimedSequence() const { return false; }

int ViewportControllerContext::secondarySequenceFrameCount() const { return -1; }

int ViewportControllerContext::secondaryTotalDuration() const { return -1; }

int ViewportControllerContext::secondarySequenceFrameIndexForPosition(int) const { return -1; }

int ViewportControllerContext::secondarySequenceFrameStartPosition(int) const { return -1; }

ImageSequenceAuthoredAnimationFacts
ViewportControllerContext::secondarySequenceAuthoredAnimationFacts() const
{
    return {};
}

ImageSequenceProviderKnownFacts
ViewportControllerContext::secondaryProviderKnownFacts() const
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

struct ControllerTransitionPolicy
{
    PageSetTransitionPolicy::DisplayTransition displayTransition
        = PageSetTransitionPolicy::DisplayTransition::RetainPrevious;
    PageSetTransitionPolicy::ZoomTransition zoomTransition
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

    ControllerTransitionPolicy normalized { policy.displayTransition(), policy.zoomTransition(),
        policy.contentPositionTransition(), policy.rotationTransition(), policy.mirrorTransition(),
        policy.replacementIntent() };

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
        presentation.fillMode,
        presentation.horizontalAlignment,
        presentation.verticalAlignment,
        presentation.rotationDegrees,
        presentation.mirrorHorizontally,
        presentation.mirrorVertically,
        presentation.zoom,
        devicePixelRatio > 0.0 ? devicePixelRatio : 1.0,
        presentation.pan,
    };
}

QRectF renderTargetRect(
    const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::pageItemRect(geometry, role).intersected(geometry.itemBounds);
}

QRectF renderSourceRect(
    const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::visiblePageRect(geometry, role);
}

ImageViewportInternal::PreparedPayload primaryRenderPayload(
    ViewportControllerPort viewport, const ViewportRenderSynchronization& synchronization)
{
    ImageViewportInternal::PreparedPayload payload = synchronization.preparedPayload;
    if (payload.image.isNull() && viewportDisplayState(viewport).hasReadyDisplay(
                                      viewport.hasDisplayableSequence())) {
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
    if (synchronization.pendingSecondaryCommit
        && !display.secondaryPendingRenderPayload.image.isNull()) {
        return display.secondaryPendingRenderPayload;
    }
    if ((synchronization.pendingProviderCommit || synchronization.pendingSecondaryCommit)
        && request.secondarySequence && !request.secondarySequenceIsProvider) {
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
    layers.append({ role, payload, targetRect, sourceRect, presentation.mirrorHorizontally,
        presentation.mirrorVertically });
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

QRectF contentRectForPresentation(
    ViewportControllerPort& viewport, const ImageViewportInternal::PresentationState& presentation)
{
    return PresentationGeometry::contentRect(controllerGeometryState(viewport, presentation));
}

QPointF clampedPoint(QPointF point, QPointF minimum, QPointF maximum)
{
    return QPointF(std::clamp(point.x(), minimum.x(), maximum.x()),
        std::clamp(point.y(), minimum.y(), maximum.y()));
}

QPointF contentPositionForRect(const QRectF& contentRect, const QRectF& itemBounds)
{
    if (contentRect.isEmpty() || itemBounds.isEmpty()) {
        return {};
    }

    const QPointF maximum(std::max(0.0, contentRect.width() - itemBounds.width()),
        std::max(0.0, contentRect.height() - itemBounds.height()));
    return clampedPoint(QPointF(-contentRect.x(), -contentRect.y()), {}, maximum);
}

QPointF maximumContentPositionForRect(const QRectF& contentRect, const QRectF& itemBounds)
{
    if (contentRect.isEmpty() || itemBounds.isEmpty()) {
        return {};
    }

    return QPointF(std::max(0.0, contentRect.width() - itemBounds.width()),
        std::max(0.0, contentRect.height() - itemBounds.height()));
}

bool applyContentPosition(ViewportControllerPort& viewport,
    ImageViewportInternal::PresentationState& presentation, QPointF requestedPosition)
{
    const QRectF content = contentRectForPresentation(viewport, presentation);
    const QRectF bounds = viewport.itemBounds();
    if (content.isEmpty() || bounds.isEmpty()) {
        return false;
    }

    const QPointF currentPosition = contentPositionForRect(content, bounds);
    const QPointF maximum = maximumContentPositionForRect(content, bounds);
    const QPointF nextPosition = clampedPoint(requestedPosition, {}, maximum);
    if (nextPosition == currentPosition) {
        return false;
    }

    presentation.pan += currentPosition - nextPosition;
    return true;
}

QPointF controllerContentPosition(
    ViewportControllerPort& viewport, const ImageViewportInternal::PresentationState& presentation)
{
    return contentPositionForRect(
        contentRectForPresentation(viewport, presentation), viewport.itemBounds());
}

QPointF controllerMaximumContentPosition(
    ViewportControllerPort& viewport, const ImageViewportInternal::PresentationState& presentation)
{
    return maximumContentPositionForRect(
        contentRectForPresentation(viewport, presentation), viewport.itemBounds());
}

bool clampPresentationPanToBounds(
    ViewportControllerPort& viewport, ImageViewportInternal::PresentationState& presentation)
{
    const QPointF savedPan = presentation.pan;
    const QRectF currentContent = contentRectForPresentation(viewport, presentation);
    const QRectF bounds = viewport.itemBounds();
    if (currentContent.isEmpty() || bounds.isEmpty()) {
        return false;
    }

    presentation.pan = {};
    const QRectF baseContent = contentRectForPresentation(viewport, presentation);
    presentation.pan = savedPan;

    const QPointF maximum = maximumContentPositionForRect(currentContent, bounds);
    const QPointF currentPosition = contentPositionForRect(currentContent, bounds);
    const QPointF clampedPosition = clampedPoint(currentPosition, {}, maximum);
    QPointF targetTopLeft = currentContent.topLeft();
    targetTopLeft.setX(maximum.x() == 0.0 ? baseContent.x() : -clampedPosition.x());
    targetTopLeft.setY(maximum.y() == 0.0 ? baseContent.y() : -clampedPosition.y());

    const QPointF adjustment = targetTopLeft - currentContent.topLeft();
    if (adjustment.isNull()) {
        return false;
    }

    presentation.pan += adjustment;
    return true;
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
    const ControllerTransitionPolicy& policy)
{
    ImageViewportInternal::ViewportChangeSet changes;
    auto markChanged = [&]() { mergeChanges(changes, presentationChanges(viewport, true)); };

    if (policy.zoomTransition == PageSetTransitionPolicy::ZoomTransition::ResetToContain) {
        if (presentation.fitMode != ImageViewport::FitMode::Contain || presentation.zoom != 1.0) {
            presentation.fitMode = ImageViewport::FitMode::Contain;
            presentation.zoom = 1.0;
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
        if (applyContentPosition(viewport, presentation, {})) {
            markChanged();
        }
    } else if (policy.contentPositionTransition
        == PageSetTransitionPolicy::ContentPositionTransition::ScanEnd) {
        if (applyContentPosition(
                viewport, presentation, controllerMaximumContentPosition(viewport, presentation))) {
            markChanged();
        }
    } else if (changes.presentation
        && policy.contentPositionTransition
            == PageSetTransitionPolicy::ContentPositionTransition::Clamp) {
        clampPresentationPanToBounds(viewport, presentation);
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

bool activeProviderFrameTokenMatchesActiveRequest(
    const ViewportControllerPort viewport, ImageSequenceProviderRequestToken token)
{
    return viewportProviderState(viewport).activeFrameToken.isValid()
        && token == viewportProviderState(viewport).activeFrameToken
        && viewportRequestState(viewport).activeRequestMatchesProviderFrameToken(token);
}

bool activeSecondaryProviderFrameTokenMatchesActiveRequest(const ViewportControllerPort viewport,
    const ImageViewportInternal::ProviderGenerationState& secondaryProvider,
    ImageSequenceProviderRequestToken token)
{
    return secondaryProvider.activeFrameToken.isValid()
        && token == secondaryProvider.activeFrameToken
        && viewportRequestState(viewport).secondaryActiveRequest.identity.id
        == viewportRequestState(viewport).activeRequest.identity.id;
}

bool activeProviderFrameRequestIsPlayback(const ViewportControllerPort viewport)
{
    return activeProviderFrameTokenMatchesActiveRequest(
               viewport, viewportProviderState(viewport).activeFrameToken)
        && viewportRequestState(viewport).activeRequest.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Playback;
}

bool activeSecondaryProviderFrameRequestIsPlayback(const ViewportControllerPort viewport,
    const ImageViewportInternal::ProviderGenerationState& secondaryProvider)
{
    return activeSecondaryProviderFrameTokenMatchesActiveRequest(
               viewport, secondaryProvider, secondaryProvider.activeFrameToken)
        && viewportRequestState(viewport).secondaryActiveRequest.target.providerTargetKind
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
            ImageViewport::RequestReason::UnsupportedRequest,
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
    effect.scheduleFlush = queue.scheduleFlush;
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

void clearQueuedProviderFrameRequest(ViewportControllerPort& viewport)
{
    viewportProviderState(viewport).queuedFrameRequest = false;
    viewportProviderState(viewport).queuedFrameGeneration = 0;
    viewportProviderState(viewport).queuedFrameRequestId = 0;
    viewportProviderState(viewport).queuedFrame = -1;
    viewportProviderState(viewport).queuedPosition = -1;
    viewportProviderState(viewport).queuedResolvedFrame = {};
    viewportProviderState(viewport).queuedFrameFromPlayback = false;
    viewportProviderState(viewport).queuedFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
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

bool hasSecondaryProviderSequence(ViewportControllerPort& viewport)
{
    return viewportRequestState(viewport).secondarySequence
        && viewportRequestState(viewport).secondarySequenceIsProvider;
}

bool hasSecondarySequence(ViewportControllerPort& viewport)
{
    return viewportRequestState(viewport).secondarySequence
        && viewportRequestState(viewport).secondaryActiveRequest.target.frame >= 0;
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

bool targetSpreadTerminalMatchesActiveRequest(const ViewportControllerPort& viewport)
{
    const auto& request = viewportRequestState(viewport);
    return request.targetSpreadTerminal.sealed
        && request.targetSpreadTerminal.generation == request.sequenceGeneration
        && request.targetSpreadTerminal.requestId == request.activeRequest.identity.id;
}

bool targetSpreadTerminalSealedForActiveRequest(const ViewportControllerPort& viewport)
{
    return targetSpreadTerminalMatchesActiveRequest(viewport);
}

bool targetSpreadRequiresRole(const ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary
        ? static_cast<bool>(viewportRequestState(viewport).sequence)
        : static_cast<bool>(viewportRequestState(viewport).secondarySequence);
}

const ImageViewportInternal::TargetSpreadRoleTerminalState* currentTerminalForRole(
    const ViewportControllerPort& viewport, ImageViewport::PageRole role)
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
    const ViewportControllerPort& viewport)
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

bool targetSpreadTerminalHasGenerationScope(const ViewportControllerPort& viewport)
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

bool hasGenerationTerminalProviderFailure(const ViewportControllerPort& viewport)
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

void publishTargetSpreadTerminalProjection(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes)
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
               || request.identity.origin == ImageViewportInternal::DisplayRequestOrigin::StopRestore
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

bool hasPendingSecondarySpreadPayload(ViewportControllerPort& viewport)
{
    return hasSecondaryProviderSequence(viewport)
        && viewportDisplayState(viewport).pendingRenderPayload.commitPending
        && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull()
        && !viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull();
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
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::RenderWaiting;
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    viewportDisplayState(viewport).pendingRenderPayload.commitPending = false;
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
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::UploadPending;
            viewportDisplayState(viewport).status
                = viewportDisplayState(viewport).displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            viewportDisplayState(viewport).pendingRenderPayload.commitPending = false;
        }
        viewportDisplayState(viewport).pendingRenderPayload.commitPending = true;
        return;
    }
    if (viewport.itemBounds().isEmpty()) {
        publishRenderWaitingState(viewport);
    } else {
        publishSequenceReadyState(viewport, providerImage);
    }
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
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::UploadPending;
            viewportDisplayState(viewport).status
                = viewportDisplayState(viewport).displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
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
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
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

bool renderPayloadMatches(
    ImageViewportInternal::PreparedPayloadIdentity actual,
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

PresentationGeometry::State ViewportController::geometryState(double devicePixelRatio) const
{
    return controllerGeometryState(viewport, state.presentation, devicePixelRatio);
}

PresentationGeometry::State ViewportController::geometryStateForItemBounds(
    const QRectF& itemBounds, double devicePixelRatio) const
{
    return controllerGeometryState(
        viewport, state.presentation, devicePixelRatio, itemBounds);
}

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

    if (!assignment.sequence) {
        const ViewportCommandResult clearResult = clear();
        result.outcome = clearResult.outcome;
        result.changes = clearResult.changes;
        result.providerFrameTransport = clearResult.providerFrameTransport;
        result.secondaryProviderFrameTransport = clearResult.secondaryProviderFrameTransport;
        return result;
    }

    result.providerFrameTransport = closeProviderSession();
    result.secondaryProviderFrameTransport = closeSecondaryProviderSession();

    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;
    const ImageViewport::PlaybackPhase oldPlaybackPhase
        = viewportRequestState(viewport).playbackPhase;
    const QString oldErrorString = viewportRequestState(viewport).errorString;
    const QString oldWarningString = viewportRequestState(viewport).warningString;
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewportInternal::ViewportChangeSet transitionChanges
        = applyPresentationTransition(viewport, state.presentation, *transitionPolicy);

    viewportRequestState(viewport).sequence = assignment.sequence;
    viewportRequestState(viewport).sequenceOwner = std::move(assignment.sequenceOwner);
    viewportRequestState(viewport).secondarySequence = assignment.secondarySequence;
    viewportRequestState(viewport).secondarySequenceOwner
        = std::move(assignment.secondarySequenceOwner);
    viewportRequestState(viewport).secondarySequenceIsProvider = assignment.secondaryIsProvider;
    ++viewportRequestState(viewport).sequenceGeneration;
    viewportRequestState(viewport).clearDisplayRequests();
    viewportDisplayState(viewport).nextPreparedPayloadId = 0;
    viewportDisplayState(viewport).clearPendingRenderPayload();
    if (!assignment.retainPreviousDisplay) {
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
    state.secondaryProvider.authoredAnimationFacts = assignment.secondaryIsProvider
        ? viewport.secondarySequenceAuthoredAnimationFacts()
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
            initializeSecondaryActiveRequest(viewport, assignment.secondaryInitialTarget,
                assignment.secondaryInitialResolvedFrame);
        } else {
            const DisplayRequestTarget initialTarget {
                -1,
                -1,
                ImageViewportInternal::ProviderRequestTargetKind::Unknown,
            };
            viewportRequestState(viewport).beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, initialTarget, true);
            viewportRequestState(viewport).playbackPosition = initialTarget.position;
            initializeSecondaryActiveRequest(viewport, assignment.secondaryInitialTarget,
                assignment.secondaryInitialResolvedFrame);
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
            viewport, assignment.secondaryInitialTarget, assignment.secondaryInitialResolvedFrame);
        if (assignment.secondaryIsProvider) {
            stageBuiltInPrimarySpreadPayload(viewport);
        } else if (viewport.width() > 0.0 && viewport.height() > 0.0) {
            publishSequenceReadyState(viewport);
        } else {
            publishRenderWaitingState(viewport);
        }
    } else {
        viewportDisplayState(viewport).clearDisplayedDisplay();
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::NoRequest;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::NoRequest;
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
        viewportDisplayState(viewport).clearPendingRenderPayload();
        viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    }

    if (assignment.secondaryIsProvider) {
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        if (assignment.retainPreviousDisplay
            && viewportDisplayState(viewport).displayedImageSize.isValid()) {
            viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Retained;
        } else {
            viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
        }
        result.openSecondaryProviderSession = true;
    }

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

ImageViewportInternal::ViewportChangeSet ViewportController::setSmoothing(bool smoothing)
{
    if (state.presentation.smoothing == smoothing) {
        return {};
    }

    state.presentation.smoothing = smoothing;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMipmap(bool mipmap)
{
    if (state.presentation.mipmap == mipmap) {
        return {};
    }

    state.presentation.mipmap = mipmap;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMirrorHorizontally(bool enabled)
{
    if (state.presentation.mirrorHorizontally == enabled) {
        return {};
    }

    state.presentation.mirrorHorizontally = enabled;
    return presentationChanges(viewport, true);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setMirrorVertically(bool enabled)
{
    if (state.presentation.mirrorVertically == enabled) {
        return {};
    }

    state.presentation.mirrorVertically = enabled;
    return presentationChanges(viewport, true);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setBackgroundMode(
    ImageViewport::BackgroundMode mode)
{
    if (!ImageViewportInternal::isValidBackgroundMode(mode)
        || state.presentation.backgroundMode == mode) {
        return {};
    }

    state.presentation.backgroundMode = mode;
    return presentationChanges(viewport, false);
}

ImageViewportInternal::ViewportChangeSet ViewportController::setBackgroundColor(const QColor& color)
{
    if (state.presentation.backgroundColor == color) {
        return {};
    }

    state.presentation.backgroundColor = color;
    return presentationChanges(viewport, false);
}

ViewportCommandResult ViewportController::setSpreadDirection(
    ImageViewport::SpreadDirection direction)
{
    if (!ImageViewportInternal::isValidSpreadDirection(direction)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.spreadDirection == direction) {
        return acceptedPresentationCommand(viewport);
    }

    state.presentation.spreadDirection = direction;
    clampPresentationPanToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setPageGap(double gap)
{
    if (!std::isfinite(gap) || gap < 0.0) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.pageGap == gap) {
        return acceptedPresentationCommand(viewport);
    }

    state.presentation.pageGap = gap;
    clampPresentationPanToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setFitMode(ImageViewport::FitMode mode, QPointF anchor)
{
    if (!ImageViewportInternal::isValidFitMode(mode)
        || !ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }
    if (state.presentation.fitMode == mode) {
        return acceptedPresentationCommand(viewport);
    }

    state.presentation.fitMode = mode;
    clampPresentationPanToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setZoomPercent(double percent, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePositive(percent)
        || percent > ImageViewportDisplayLimits::maximumManualZoomPercent()
        || !ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    const double zoom = percent / 100.0;
    if (state.presentation.fitMode == ImageViewport::FitMode::Manual
        && state.presentation.zoom == zoom) {
        return acceptedPresentationCommand(viewport);
    }

    state.presentation.fitMode = ImageViewport::FitMode::Manual;
    state.presentation.zoom = zoom;
    clampPresentationPanToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panBy(QPointF delta)
{
    if (!ImageViewportInternal::isFinitePoint(delta)) {
        return invalidPresentationCommand(viewport);
    }
    if (delta.isNull()) {
        return acceptedPresentationCommand(viewport);
    }

    if (!applyContentPosition(viewport, state.presentation,
            controllerContentPosition(viewport, state.presentation) + delta)) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panToStart()
{
    if (!applyContentPosition(viewport, state.presentation, {})) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::panToEnd()
{
    if (!applyContentPosition(viewport, state.presentation,
            controllerMaximumContentPosition(viewport, state.presentation))) {
        return acceptedPresentationCommand(viewport);
    }
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::scanNext()
{
    const QPointF maximum = controllerMaximumContentPosition(viewport, state.presentation);
    if (maximum.y() > 0.0) {
        return panBy(QPointF(0.0, std::max(1.0, viewport.itemBounds().height() * 0.9)));
    }
    if (maximum.x() > 0.0) {
        return panBy(QPointF(std::max(1.0, viewport.itemBounds().width() * 0.9), 0.0));
    }
    return acceptedPresentationCommand(viewport);
}

ViewportCommandResult ViewportController::scanPrevious()
{
    const QPointF maximum = controllerMaximumContentPosition(viewport, state.presentation);
    if (maximum.y() > 0.0) {
        return panBy(QPointF(0.0, -std::max(1.0, viewport.itemBounds().height() * 0.9)));
    }
    if (maximum.x() > 0.0) {
        return panBy(QPointF(-std::max(1.0, viewport.itemBounds().width() * 0.9), 0.0));
    }
    return acceptedPresentationCommand(viewport);
}

ViewportCommandResult ViewportController::rotateClockwise(QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    state.presentation.rotationDegrees = (state.presentation.rotationDegrees + 90) % 360;
    clampPresentationPanToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::rotateCounterClockwise(QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    state.presentation.rotationDegrees = (state.presentation.rotationDegrees + 270) % 360;
    clampPresentationPanToBounds(viewport, state.presentation);
    return acceptedPresentationCommand(viewport, presentationChanges(viewport, true));
}

ViewportCommandResult ViewportController::setMirrorHorizontally(bool enabled, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    return acceptedPresentationCommand(viewport, setMirrorHorizontally(enabled));
}

ViewportCommandResult ViewportController::setMirrorVertically(bool enabled, QPointF anchor)
{
    if (!ImageViewportInternal::isFinitePoint(anchor)) {
        return invalidPresentationCommand(viewport);
    }

    return acceptedPresentationCommand(viewport, setMirrorVertically(enabled));
}

ViewportCommandResult ViewportController::resetView()
{
    const bool changed = state.presentation.fitMode != ImageViewport::FitMode::Contain
        || state.presentation.zoom != 1.0 || state.presentation.pan.x() != 0.0
        || state.presentation.pan.y() != 0.0;
    state.presentation.fitMode = ImageViewport::FitMode::Contain;
    state.presentation.zoom = 1.0;
    state.presentation.pan = {};

    return acceptedPresentationCommand(viewport,
        changed ? presentationChanges(viewport, true)
                : ImageViewportInternal::ViewportChangeSet {});
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
    const bool closeProviderSession = viewportProviderState(viewport).session != nullptr;
    result.providerFrameTransport.sessionClose = handleProviderSessionClose();
    result.providerFrameTransport.closeSession = closeProviderSession;
    result.secondaryProviderFrameTransport = closeSecondaryProviderSession();
    viewportRequestState(viewport).sequence = nullptr;
    viewportRequestState(viewport).sequenceOwner.reset();
    viewportRequestState(viewport).secondarySequence = nullptr;
    viewportRequestState(viewport).secondarySequenceOwner.reset();
    viewportRequestState(viewport).secondarySequenceIsProvider = false;
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

ViewportCommandResult ViewportController::play()
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata
        && viewportProviderState(viewport).timedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(viewportRequestState(viewport).playbackPhase,
                  viewportRequestState(viewport).stopPlaybackWhenRequestReady)
            && viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Primary
            && viewportRequestState(viewport).playbackPosition >= 0;
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Unsupported
            || viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error) {
            const DisplayRequestTarget target = providerPlaybackStartTarget(viewport);
            clearCommandDiagnosticForAcceptedCommand(viewport, result);
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            applyProviderPlaybackStartTarget(viewport, target);
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            publishProviderFrameLoadingState(viewport);
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest({ target });
            result.providerFrameTransport = dispatch.transport;
            if (!dispatch.accepted) {
                result.changes.requestRevision = true;
                result.changes.requestState = true;
                result.changes.diagnostics = true;
                return result;
            }
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
            result.changes.requestRevision = true;
            result.changes.requestState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return result;
        }

        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.providerFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        }
        setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerTimedPlaybackCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        applyPendingProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }

    if (viewport.hasTimedSequence()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(viewportRequestState(viewport).playbackPhase,
                  viewportRequestState(viewport).stopPlaybackWhenRequestReady)
            && viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Primary
            && viewportRequestState(viewport).playbackPosition >= 0;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Unsupported
            || viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus
                = viewportDisplayState(viewport).status;
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            publishAcceptedTargetState(viewport);
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
            result.changes.requestRevision = true;
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.requestState = true;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
                || ImageViewportInternal::rectsDifferExactly(
                    viewport.visibleImageRect(), oldVisibleImageRect);
            result.changes.diagnostics = diagnosticsValueChanged;
            result.changes.scheduleUpdate = true;
            return result;
        }
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        }
        setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::playSecondaryBuiltIn()
{
    if (!viewport.hasActiveRequest()
        || viewportRequestState(viewport).secondaryActiveRequest.target.frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Secondary;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    viewportRequestState(viewport).playbackPosition
        = viewportRequestState(viewport).secondaryActiveRequest.target.position >= 0
        ? viewportRequestState(viewport).secondaryActiveRequest.target.position
        : viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame.position;
    setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
    return result;
}

ViewportCommandResult ViewportController::playSecondaryProvider()
{
    const ImageViewportInternal::DisplayRequest& request
        = viewportRequestState(viewport).secondaryActiveRequest;
    const bool currentIdentity = request.identity.id != 0
        && request.identity.id == viewportRequestState(viewport).activeRequest.identity.id;
    if (!viewport.hasActiveRequest() || !currentIdentity) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (!state.secondaryProvider.metadataReady) {
        if (ImageViewportInternal::providerCapabilityKnownFalse(
                viewport.secondaryProviderTimedPlaybackCapability())) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Secondary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        applyPendingSecondaryProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }

    if (request.target.frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    if (!state.secondaryProvider.timedMetadata || !state.secondaryProvider.timedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Secondary;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    viewportRequestState(viewport).playbackPosition
        = request.target.position >= 0 ? request.target.position : request.resolvedFrame.position;
    setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
    return result;
}

ViewportCommandResult ViewportController::pause()
{
    return pause(ImageViewport::PageRole::Primary);
}

ViewportCommandResult ViewportController::pause(ImageViewport::PageRole role)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Stopped
        && viewportRequestState(viewport).playbackRole != role) {
        return result;
    }
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
        || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
        viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Paused;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportCommandResult ViewportController::stop() { return stop(ImageViewport::PageRole::Primary); }

ViewportCommandResult ViewportController::stop(ImageViewport::PageRole role)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Stopped
        && viewportRequestState(viewport).playbackRole != role) {
        return result;
    }

    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    if (role == ImageViewport::PageRole::Secondary) {
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Stopped) {
            return result;
        }

        if (activeSecondaryProviderFrameRequestIsPlayback(viewport, state.secondaryProvider)) {
            result.providerFrameTransport.cancelToken = state.secondaryProvider.activeFrameToken;
            state.secondaryProvider.activeFrameToken = {};
        }

        const ImageViewportInternal::DisplayRequest restoreRequest
            = viewportRequestState(viewport).secondaryLatestNonPlaybackRequest;
        if (restoreRequest.target.frame >= 0) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus
                = viewportDisplayState(viewport).status;
            const ImageViewportInternal::DisplayRequest primaryRequest
                = viewportRequestState(viewport).activeRequest;
            beginAcceptedDisplayRequest(viewport,
                ImageViewportInternal::DisplayRequestOrigin::StopRestore, primaryRequest.target,
                primaryRequest.resolvedFrame, false);
            setSecondaryActiveRequest(
                viewport, restoreRequest.target, restoreRequest.resolvedFrame, true);
            viewportRequestState(viewport).playbackPosition = restoreRequest.target.position;
            publishAcceptedTargetState(viewport);
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
            result.changes.requestRevision = true;
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.requestState = true;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
                || ImageViewportInternal::rectsDifferExactly(
                    viewport.visibleImageRect(), oldVisibleImageRect);
            result.changes.scheduleUpdate = true;
            return result;
        }

        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
        return result;
    }

    const StopRestorePlan stopRestorePlan = stopRestorePlanFor(viewport);
    if (applyStopRestorePlan(*this, viewport, stopRestorePlan, result)) {
        return result;
    }
    setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
    return result;
}

ViewportCommandResult ViewportController::seek(int frame)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasDisplayableSequence()) {
        if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady) {
            if (!viewportProviderState(viewport).frameSeekSupport) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            const int maximumFrame = viewportProviderState(viewport).timedMetadata
                ? viewportProviderState(viewport).timingIntervals.frameCount() - 1
                : 0;
            if (frame > maximumFrame) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget { frame, viewport.providerFrameStartPosition(frame),
                    ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderReady);
        }

        if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
            if (viewport.providerFrameSeekCapabilityKnownFalse()) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            if (viewport.providerKnownFactsTimedFrameCount()
                && viewport.providerFrameSeekCapabilityKnownTrue()) {
                const int maximumFrame = viewport.providerKnownFactsFrameCount() - 1;
                if (frame > maximumFrame) {
                    ViewportCommandResult result;
                    result.outcome = ImageViewport::CommandOutcome::Invalid;
                    setCommandDiagnostic(
                        viewport, result, ImageViewport::CommandReason::InvalidRequest);
                    return result;
                }
            }

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget {
                    frame, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderPendingMetadata);
        }

        if (frame >= viewport.sequenceFrameCount()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget { frame,
                viewport.hasTimedSequence() ? viewport.sequenceFrameStartPosition(frame) : -1 },
            ExplicitSeekMaterialization::BuiltIn);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekSecondaryBuiltIn(
    ImageViewportInternal::DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    const ImageViewportInternal::DisplayRequest primaryRequest
        = viewportRequestState(viewport).activeRequest;
    beginAcceptedDisplayRequest(viewport, ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        primaryRequest.target, primaryRequest.resolvedFrame, true);
    setSecondaryActiveRequest(viewport, target, resolvedFrame, true);
    publishAcceptedTargetState(viewport);
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
    }

    result.changes.requestRevision = true;
    const bool displayValueChanged = viewportDisplayState(viewport).status != oldDisplayStatus
        || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
    result.changes.displayRevision = displayValueChanged;
    result.changes.requestState = true;
    result.changes.displayState = displayValueChanged;
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportCommandResult ViewportController::seekSecondaryProvider(int frame)
{
    if (!viewport.hasActiveRequest() || !hasSecondaryProviderSequence(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    const auto acceptTarget = [this](ImageViewportInternal::DisplayRequestTarget target,
                                  ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
                                  bool dispatchNow) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(viewport,
            ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, primaryRequest.target,
            primaryRequest.resolvedFrame, true);
        setSecondaryActiveRequest(viewport, target, resolvedFrame, true);

        if (dispatchNow) {
            if (state.secondaryProvider.session
                && state.secondaryProvider.activeFrameToken.isValid()) {
                result.providerFrameTransport.cancelToken
                    = state.secondaryProvider.activeFrameToken;
            }
            state.secondaryProvider.activeFrameToken = {};
            publishProviderFrameLoadingState(viewport);
            const ViewportProviderFrameRequestStartResult start
                = startSecondaryProviderFrameRequest(target);
            appendProviderFrameStartResult(result.providerFrameTransport, start);
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!start.accepted) {
                result.changes.diagnostics = true;
            }
        } else {
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
            discardPendingRenderCommit(viewport);
        }

        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsValueChanged;
        return result;
    };

    if (state.secondaryProvider.metadataReady) {
        if (!state.secondaryProvider.frameSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int maximumFrame = state.secondaryProvider.timedMetadata
            ? state.secondaryProvider.timingIntervals.frameCount() - 1
            : 0;
        if (frame > maximumFrame) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        const int position = state.secondaryProvider.timedMetadata
            ? state.secondaryProvider.timingIntervals.frameStartPosition(frame)
            : -1;
        return acceptTarget(
            { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Frame },
            { frame, position }, true);
    }

    if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport frameSeekCapability
            = viewport.secondaryProviderFrameSeekCapability();
        if (ImageViewportInternal::providerCapabilityKnownFalse(frameSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts = viewport.secondaryProviderKnownFacts();
        if (ImageViewportInternal::providerCapabilityKnownTrue(frameSeekCapability)
            && knownFacts.frameCount() >= 0 && frame >= knownFacts.frameCount()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptTarget(
            { frame, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame },
            { -1, -1 }, false);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekToPosition(int milliseconds)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    if (milliseconds < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerPositionSeekCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                -1, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { -1, -1 }, ExplicitSeekMaterialization::ProviderPendingMetadata);
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata) {
        if (!viewportProviderState(viewport).positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = viewport.providerFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { frame, viewport.providerFrameStartPosition(frame) },
            ExplicitSeekMaterialization::ProviderReady);
    }

    if (viewport.hasTimedSequence()) {
        const int frame = viewport.sequenceFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport, DisplayRequestTarget { frame, milliseconds },
            { frame, viewport.sequenceFrameStartPosition(frame) },
            ExplicitSeekMaterialization::BuiltIn);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekSecondaryProviderToPosition(int milliseconds)
{
    if (!viewport.hasActiveRequest() || !hasSecondaryProviderSequence(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (milliseconds < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    const auto acceptTarget = [this](ImageViewportInternal::DisplayRequestTarget target,
                                  ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
                                  bool dispatchNow) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(viewport,
            ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, primaryRequest.target,
            primaryRequest.resolvedFrame, true);
        setSecondaryActiveRequest(viewport, target, resolvedFrame, true);

        if (dispatchNow) {
            if (state.secondaryProvider.session
                && state.secondaryProvider.activeFrameToken.isValid()) {
                result.providerFrameTransport.cancelToken
                    = state.secondaryProvider.activeFrameToken;
            }
            state.secondaryProvider.activeFrameToken = {};
            publishProviderFrameLoadingState(viewport);
            const ViewportProviderFrameRequestStartResult start
                = startSecondaryProviderFrameRequest(target);
            appendProviderFrameStartResult(result.providerFrameTransport, start);
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!start.accepted) {
                result.changes.diagnostics = true;
            }
        } else {
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
            discardPendingRenderCommit(viewport);
        }

        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsValueChanged;
        return result;
    };

    if (!state.secondaryProvider.metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport positionSeekCapability
            = viewport.secondaryProviderPositionSeekCapability();
        if (ImageViewportInternal::providerCapabilityKnownFalse(positionSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts = viewport.secondaryProviderKnownFacts();
        if (ImageViewportInternal::providerCapabilityKnownTrue(positionSeekCapability)) {
            if (knownFacts.isStill()) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            if (knownFacts.isTimedFrameList()
                && TimingIntervals::fromFrameDurations(knownFacts.frameDurations())
                        .frameIndexForPosition(milliseconds)
                    < 0) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }
        }

        return acceptTarget(
            { -1, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { -1, -1 }, false);
    }

    if (state.secondaryProvider.metadataReady && state.secondaryProvider.timedMetadata) {
        if (!state.secondaryProvider.positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = state.secondaryProvider.timingIntervals.frameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }
        const int frameStart = state.secondaryProvider.timingIntervals.frameStartPosition(frame);
        return acceptTarget(
            { frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { frame, frameStart }, true);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleGeometryChanged(
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (viewport.hasDisplayableSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
            || viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting)
        && !viewport.itemBounds().isEmpty()) {
        if ((viewport.hasProviderSequence()
                && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull())
            || hasPendingSecondarySpreadPayload(viewport)) {
            changes.scheduleUpdate = true;
            return changes;
        }
        publishSequenceReadyState(viewport);
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
            setPlaybackPhase(viewport, changes,
                viewportRequestState(viewport).stopPlaybackWhenRequestReady
                    ? ImageViewport::PlaybackPhase::Stopped
                    : ImageViewport::PlaybackPhase::Playing);
            viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        }
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.requestState = true;
        changes.displayState = true;
    } else if (viewport.hasProviderSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
        && viewport.itemBounds().isEmpty()
        && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull()) {
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::RenderWaiting;
        changes.requestRevision = true;
        changes.requestState = true;
        changes.displayRevision = true;
    } else {
        changes.displayRevision = true;
    }

    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.scheduleUpdate = true;
    return changes;
}

FramePreparation::ProviderFrameState ViewportController::providerFramePreparationState() const
{
    ImageViewportInternal::PreparedPayload preparedPayload
        = viewportDisplayState(viewport).pendingRenderPayload;
    if (!preparedPayload.identity().isValid()) {
        preparedPayload.generation = viewportRequestState(viewport).sequenceGeneration;
        preparedPayload.requestId = viewportRequestState(viewport).activeRequest.identity.id;
        preparedPayload.payloadId = preparedPayload.requestId == 0
            ? 0
            : viewportDisplayState(viewport).nextPreparedPayloadId + 1;
    }
    return {
        viewportProviderState(viewport).metadataReady,
        viewportProviderState(viewport).timedMetadata,
        viewportProviderState(viewport).logicalSize,
        viewportProviderState(viewport).timingIntervals,
        viewportRequestState(viewport).activeRequest.resolvedFrame,
        preparedPayload,
    };
}

ViewportProviderFrameEventAcceptance ViewportController::acceptProviderFrameEvent(
    ViewportProviderFrameEvent event) const
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }
    if (!viewport.hasProviderSequence() || !viewportProviderState(viewport).session
        || !activeProviderFrameTokenMatchesActiveRequest(viewport, event.token)) {
        return {};
    }

    return { true, providerFramePreparationState() };
}

ViewportProviderFrameEventAcceptance ViewportController::acceptSecondaryProviderFrameEvent(
    ViewportProviderFrameEvent event)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }
    if (!state.secondaryProvider.session || !state.secondaryProvider.activeFrameToken.isValid()
        || event.token != state.secondaryProvider.activeFrameToken) {
        return {};
    }

    ImageViewportInternal::PreparedPayload preparedPayload
        = viewportDisplayState(viewport).pendingRenderPayload;
    if (!preparedPayload.identity().isValid()) {
        preparedPayload.commitPending = true;
        preparedPayload.generation = viewportRequestState(viewport).sequenceGeneration;
        preparedPayload.requestId = viewportRequestState(viewport).activeRequest.identity.id;
        preparedPayload.payloadId = ++viewportDisplayState(viewport).nextPreparedPayloadId;
        if (displayedPrimaryPayloadMatchesActiveTarget(viewport)) {
            preparedPayload.image = viewportDisplayState(viewport).displayedImage;
        }
        viewportRequestState(viewport).activeRequest.preparedPayloadId = preparedPayload.payloadId;
        viewportDisplayState(viewport).pendingRenderPayload = preparedPayload;
    }

    FramePreparation::ProviderFrameState preparationState;
    preparationState.metadataReady = state.secondaryProvider.metadataReady;
    preparationState.timedMetadata = state.secondaryProvider.timedMetadata;
    preparationState.logicalSize = state.secondaryProvider.logicalSize;
    preparationState.timingIntervals = state.secondaryProvider.timingIntervals;
    preparationState.resolvedFrame
        = viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame;
    preparationState.preparedPayload = preparedPayload;
    state.secondaryProvider.activeFrameToken = {};
    return { true, preparationState };
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const ViewportProviderFrameEventAcceptance frameEvent = acceptProviderFrameEvent(event);
    if (!frameEvent.accepted) {
        return {};
    }

    return handleProviderFrameAdmission(
        FramePreparation::admitProviderFrame(frame, metadata, frameEvent.preparationState));
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleSecondaryProviderFrameEvent(
    ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const ViewportProviderFrameEventAcceptance frameEvent
        = acceptSecondaryProviderFrameEvent(event);
    if (!frameEvent.accepted) {
        return {};
    }

    return handleSecondaryProviderFrameAdmission(
        FramePreparation::admitProviderFrame(frame, metadata, frameEvent.preparationState));
}

ViewportProviderMetadataEventAcceptance ViewportController::acceptProviderMetadataEvent(
    ViewportProviderMetadataEvent event)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }
    if (!viewport.hasProviderSequence() || !viewportProviderState(viewport).session
        || !viewportProviderState(viewport).activeMetadataToken.isValid()
        || event.token != viewportProviderState(viewport).activeMetadataToken) {
        return {};
    }

    viewportProviderState(viewport).activeMetadataToken = {};
    return { true };
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderSessionOpenFailure(
    const QString& diagnostic)
{
    return handleProviderSessionOpenFailure(ImageViewport::PageRole::Primary, diagnostic);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderSessionOpenFailure(
    ImageViewport::PageRole role, const QString& diagnostic)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (role == ImageViewport::PageRole::Primary) {
        clearQueuedProviderFrameRequest(viewport);
    }
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    recordTargetSpreadTerminal(viewport, role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::ProviderFailure,
        ImageViewportInternal::FailureScope::Generation, diagnostic, changes);
    return changes;
}

ViewportProviderSessionOpenResult ViewportController::handleProviderSessionOpened()
{
    return handleProviderSessionOpened(ImageViewport::PageRole::Primary);
}

ViewportProviderSessionOpenResult ViewportController::handleProviderSessionOpened(
    ImageViewport::PageRole role)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }
    if (role == ImageViewport::PageRole::Secondary) {
        ViewportProviderSessionOpenResult result;
        appendProviderMetadataStartResult(
            result.providerMetadataTransport, handleSecondaryProviderSessionOpened());
        return result;
    }

    ViewportProviderSessionOpenResult result;
    if (viewportProviderState(viewport).metadataReady) {
        discardPendingRenderCommit(viewport);
        appendProviderFrameStartResult(result.providerFrameTransport,
            startProviderFrameRequest({ viewportRequestState(viewport).activeRequest.target }));
        return result;
    }

    appendProviderMetadataStartResult(
        result.providerMetadataTransport, startProviderMetadataRequest());
    return result;
}

ViewportProviderMetadataRequestStartResult
ViewportController::handleSecondaryProviderSessionOpened()
{
    return startSecondaryProviderMetadataRequest();
}

quint64 ViewportController::installProviderSession(ImageSequenceProviderSession* session)
{
    return installProviderSession(ImageViewport::PageRole::Primary, session);
}

quint64 ViewportController::installProviderSession(
    ImageViewport::PageRole role, ImageSequenceProviderSession* session)
{
    if (role == ImageViewport::PageRole::Secondary) {
        state.secondaryProvider.session = session;
        if (!state.secondaryProvider.session) {
            return 0;
        }

        ++state.secondaryProvider.sessionSerial;
        return state.secondaryProvider.sessionSerial;
    }

    viewportProviderState(viewport).session = session;
    if (!viewportProviderState(viewport).session) {
        return 0;
    }

    ++viewportProviderState(viewport).sessionSerial;
    return viewportProviderState(viewport).sessionSerial;
}

ImageSequenceProviderSession* ViewportController::takeProviderSession()
{
    return takeProviderSession(ImageViewport::PageRole::Primary);
}

ImageSequenceProviderSession* ViewportController::takeProviderSession(ImageViewport::PageRole role)
{
    if (role == ImageViewport::PageRole::Secondary) {
        ImageSequenceProviderSession* session = state.secondaryProvider.session;
        state.secondaryProvider.session.clear();
        return session;
    }

    ImageSequenceProviderSession* session = viewportProviderState(viewport).session;
    viewportProviderState(viewport).session.clear();
    return session;
}

ImageSequenceProviderSession* ViewportController::currentProviderSession() const
{
    return currentProviderSession(ImageViewport::PageRole::Primary);
}

ImageSequenceProviderSession* ViewportController::currentProviderSession(
    ImageViewport::PageRole role) const
{
    if (role == ImageViewport::PageRole::Secondary) {
        return state.secondaryProvider.session;
    }

    return viewportProviderState(viewport).session;
}

bool ViewportController::acceptsProviderSessionResult(quint64 sessionSerial) const
{
    return acceptsProviderSessionResult(ImageViewport::PageRole::Primary, sessionSerial);
}

bool ViewportController::acceptsProviderSessionResult(
    ImageViewport::PageRole role, quint64 sessionSerial) const
{
    if (role == ImageViewport::PageRole::Secondary) {
        return state.secondaryProvider.session
            && state.secondaryProvider.sessionSerial == sessionSerial;
    }

    return viewportProviderState(viewport).session
        && viewportProviderState(viewport).sessionSerial == sessionSerial;
}

ViewportProviderMetadataAdmissionResult ViewportController::handleProviderMetadataAdmission(
    const ImageSequenceProviderMetadata& metadata)
{
    return handleProviderMetadataAdmission(ImageViewport::PageRole::Primary, metadata);
}

ViewportProviderMetadataAdmissionResult ViewportController::handleProviderMetadataAdmission(
    ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }

    const auto generationTerminalResult
        = [this, role](ImageViewportInternal::ViewportChangeSet changes) {
              ViewportProviderMetadataAdmissionResult result;
              result.changes = changes;
              if (role == ImageViewport::PageRole::Secondary) {
                  result.providerFrameTransport.closeSession
                      = state.secondaryProvider.session != nullptr;
                  result.providerFrameTransport.sessionClose
                      = handleSecondaryProviderSessionClose();
                  return result;
              }

              result.providerFrameTransport.closeSession
                  = viewportProviderState(viewport).session != nullptr;
              result.providerFrameTransport.sessionClose = handleProviderSessionClose();
              return result;
          };

    const auto admission = FramePreparation::admitProviderMetadata(metadata);
    if (!admission.accepted()) {
        return generationTerminalResult(
            handleProviderMetadataAdmissionRejection(role, { admission.diagnostic }));
    }

    const bool secondary = role == ImageViewport::PageRole::Secondary;
    const ImageSequenceProviderCapabilitySupport timedPlaybackCapability = secondary
        ? viewport.secondaryProviderTimedPlaybackCapability()
        : viewport.providerTimedPlaybackCapability();
    const ImageSequenceProviderCapabilitySupport frameSeekCapability
        = secondary ? viewport.secondaryProviderFrameSeekCapability()
                    : viewport.providerFrameSeekCapability();
    const ImageSequenceProviderCapabilitySupport positionSeekCapability = secondary
        ? viewport.secondaryProviderPositionSeekCapability()
        : viewport.providerPositionSeekCapability();
    if (ImageViewportInternal::providerCapabilityContradictsMetadata(
            timedPlaybackCapability, metadata.timedPlaybackSupport())
        || ImageViewportInternal::providerCapabilityContradictsMetadata(
            frameSeekCapability, metadata.frameSeekSupport())
        || ImageViewportInternal::providerCapabilityContradictsMetadata(
            positionSeekCapability, metadata.positionSeekSupport())) {
        return generationTerminalResult(handleProviderMetadataContradiction(role,
            { QStringLiteral("provider metadata contradicts construction-time capabilities") }));
    }

    const ImageSequenceProviderKnownFacts knownFacts
        = secondary ? viewport.secondaryProviderKnownFacts() : viewport.providerKnownFacts();
    if (ImageViewportInternal::providerFactsContradictMetadata(knownFacts, metadata)) {
        return generationTerminalResult(handleProviderMetadataContradiction(role,
            { QStringLiteral("provider metadata contradicts construction-time facts") }));
    }

    const ImageSequenceAuthoredAnimationFacts fallbackAuthoredFacts = secondary
        ? viewport.secondarySequenceAuthoredAnimationFacts()
        : viewport.providerAuthoredAnimationFacts();
    ViewportProviderMetadataAdmissionResult result;
    result.accepted = true;
    result.facts = {
        admission.timedMetadata,
        metadata.timedPlaybackSupport(),
        metadata.frameSeekSupport(),
        metadata.positionSeekSupport(),
        admission.logicalSize,
        admission.timingIntervals,
        metadata.hasAuthoredAnimationFacts() ? metadata.authoredAnimationFacts()
                                             : fallbackAuthoredFacts,
    };
    return result;
}

ViewportProviderMetadataAdmissionResult
ViewportController::handleSecondaryProviderMetadataAdmission(
    const ImageSequenceProviderMetadata& metadata)
{
    return handleProviderMetadataAdmission(ImageViewport::PageRole::Secondary, metadata);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameAdmission(
    const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (!admission.accepted()) {
        clearQueuedProviderFrameRequest(viewport);
        viewportProviderState(viewport).activeFrameToken = {};
        recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Primary, admission.status,
            admission.reason, ImageViewportInternal::FailureScope::DisplayRequest,
            admission.diagnostic, changes);
        setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
        return changes;
    }

    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    viewportProviderState(viewport).activeFrameToken = {};
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    publishAcceptedTargetState(viewport, admission.preparedPayload);
    if (hasSecondaryProviderSequence(viewport)
        && viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull()) {
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        viewportDisplayState(viewport).status
            = viewportDisplayState(viewport).displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
    }
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready
        && !viewportDisplayState(viewport).pendingRenderPayload.commitPending) {
        setPlaybackPhase(viewport, changes,
            viewportRequestState(viewport).stopPlaybackWhenRequestReady
                ? ImageViewport::PlaybackPhase::Stopped
                : ImageViewport::PlaybackPhase::Playing);
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    }
    changes.requestRevision = true;
    changes.displayRevision = true;
    changes.requestState = true;
    changes.displayState = true;
    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.diagnostics = diagnosticsValueChanged;
    changes.scheduleUpdate = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleSecondaryProviderFrameAdmission(
    const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (!admission.accepted()) {
        state.secondaryProvider.activeFrameToken = {};
        recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Secondary, admission.status,
            admission.reason, ImageViewportInternal::FailureScope::DisplayRequest,
            admission.diagnostic, changes);
        setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
        return changes;
    }

    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    viewportDisplayState(viewport).secondaryPendingRenderPayload = admission.preparedPayload;
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    const bool primaryPayloadReady
        = viewportDisplayState(viewport).pendingRenderPayload.commitPending
        && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull();
    viewportRequestState(viewport).reason = !primaryPayloadReady
        ? ImageViewport::RequestReason::ProviderWaiting
        : viewport.itemBounds().isEmpty() ? ImageViewport::RequestReason::RenderWaiting
                                          : ImageViewport::RequestReason::UploadPending;
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;

    changes.requestRevision = true;
    changes.displayRevision = true;
    changes.requestState = true;
    changes.displayState = true;
    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.diagnostics = diagnosticsValueChanged;
    changes.scheduleUpdate = true;
    return changes;
}

ViewportProviderTerminalEventResult ViewportController::handleProviderTerminalEvent(
    const ViewportProviderTerminalEvent& event)
{
    return handleProviderTerminalEvent(ImageViewport::PageRole::Primary, event);
}

ViewportProviderTerminalEventResult ViewportController::handleProviderTerminalEvent(
    ImageViewport::PageRole role, const ViewportProviderTerminalEvent& event)
{
    if (role == ImageViewport::PageRole::Secondary) {
        if (!hasSecondaryProviderSequence(viewport) || !state.secondaryProvider.session) {
            return {};
        }

        if (activeSecondaryProviderFrameTokenMatchesActiveRequest(
                viewport, state.secondaryProvider, event.token)) {
            return { handleSecondaryProviderFrameTerminalResult(frameTerminalResultFor(event)),
                {} };
        }

        if (state.secondaryProvider.metadataReady
            || !state.secondaryProvider.activeMetadataToken.isValid()
            || event.token != state.secondaryProvider.activeMetadataToken) {
            return {};
        }

        ViewportProviderTerminalEventResult result;
        result.changes
            = handleSecondaryProviderMetadataTerminalResult(metadataTerminalResultFor(event));
        result.providerFrameTransport.closeSession = true;
        result.providerFrameTransport.sessionClose = handleSecondaryProviderSessionClose();
        return result;
    }

    if (!viewport.hasProviderSequence() || !viewportProviderState(viewport).session) {
        return {};
    }

    if (activeProviderFrameTokenMatchesActiveRequest(viewport, event.token)) {
        return { handleProviderFrameTerminalResult(frameTerminalResultFor(event)), {} };
    }

    if (viewportProviderState(viewport).metadataReady
        || !viewportProviderState(viewport).activeMetadataToken.isValid()
        || event.token != viewportProviderState(viewport).activeMetadataToken) {
        return {};
    }

    ViewportProviderTerminalEventResult result;
    result.changes = handleProviderMetadataTerminalResult(metadataTerminalResultFor(event));
    result.providerFrameTransport.closeSession = true;
    result.providerFrameTransport.sessionClose = handleProviderSessionClose();
    return result;
}

ViewportProviderTerminalEventResult ViewportController::handleProviderDispatchFailure(
    ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event)
{
    const ViewportProviderTerminalEvent terminalEvent {
        event.token,
        ViewportProviderTerminalEvent::Kind::Failure,
        ImageSequenceProviderSession::UnsupportedCause::PayloadRejection,
        event.diagnostic.isEmpty() ? QStringLiteral("provider command delivery failed")
                                   : event.diagnostic,
    };

    if (role == ImageViewport::PageRole::Secondary) {
        if (!hasSecondaryProviderSequence(viewport)) {
            return {};
        }
        if (activeSecondaryProviderFrameTokenMatchesActiveRequest(
                viewport, state.secondaryProvider, event.token)) {
            return { handleSecondaryProviderFrameTerminalResult(
                         frameTerminalResultFor(terminalEvent)),
                closeProviderSession(role) };
        }
        if (state.secondaryProvider.metadataReady
            || !state.secondaryProvider.activeMetadataToken.isValid()
            || event.token != state.secondaryProvider.activeMetadataToken) {
            return {};
        }
        return { handleSecondaryProviderMetadataTerminalResult(
                     metadataTerminalResultFor(terminalEvent)),
            closeProviderSession(role) };
    }

    if (!viewport.hasProviderSequence()) {
        return {};
    }
    if (activeProviderFrameTokenMatchesActiveRequest(viewport, event.token)) {
        return { handleProviderFrameTerminalResult(frameTerminalResultFor(terminalEvent)),
            closeProviderSession(role) };
    }
    if (viewportProviderState(viewport).metadataReady
        || !viewportProviderState(viewport).activeMetadataToken.isValid()
        || event.token != viewportProviderState(viewport).activeMetadataToken) {
        return {};
    }
    return { handleProviderMetadataTerminalResult(metadataTerminalResultFor(terminalEvent)),
        closeProviderSession(role) };
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameTerminalResult(
    const ViewportProviderFrameTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    clearQueuedProviderFrameRequest(viewport);
    viewportProviderState(viewport).activeFrameToken = {};
    recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Primary, result.status,
        result.reason, ImageViewportInternal::FailureScope::DisplayRequest,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic),
        changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleSecondaryProviderFrameTerminalResult(
    const ViewportProviderFrameTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    state.secondaryProvider.activeFrameToken = {};
    recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Secondary, result.status,
        result.reason, ImageViewportInternal::FailureScope::DisplayRequest,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic),
        changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTerminalResult(
    const ViewportProviderMetadataTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewportProviderState(viewport).activeMetadataToken = {};
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Primary, result.status,
        result.reason, ImageViewportInternal::FailureScope::Generation,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic),
        changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleSecondaryProviderMetadataTerminalResult(
    const ViewportProviderMetadataTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    state.secondaryProvider.activeMetadataToken = {};
    recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Secondary, result.status,
        result.reason, ImageViewportInternal::FailureScope::Generation,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic),
        changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataContradiction(
    const ViewportProviderMetadataContradiction& contradiction)
{
    return handleProviderMetadataContradiction(ImageViewport::PageRole::Primary, contradiction);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataContradiction(
    ImageViewport::PageRole role, const ViewportProviderMetadataContradiction& contradiction)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    recordTargetSpreadTerminal(viewport, role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        ImageViewportInternal::FailureScope::Generation, contradiction.diagnostic, changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderMetadataAdmissionRejection(
    const ViewportProviderMetadataAdmissionRejection& rejection)
{
    return handleProviderMetadataAdmissionRejection(ImageViewport::PageRole::Primary, rejection);
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderMetadataAdmissionRejection(
    ImageViewport::PageRole role, const ViewportProviderMetadataAdmissionRejection& rejection)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    recordTargetSpreadTerminal(viewport, role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        ImageViewportInternal::FailureScope::Generation, rejection.diagnostic, changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTargetRejection(
    ViewportProviderMetadataTargetRejection rejection)
{
    return handleProviderMetadataTargetRejection(ImageViewport::PageRole::Primary, rejection);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTargetRejection(
    ImageViewport::PageRole role, ViewportProviderMetadataTargetRejection rejection)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (rejection.updateActiveTarget) {
        viewportRequestState(viewport).activeRequest.target.frame = rejection.selectedFrame;
        viewportRequestState(viewport).activeRequest.resolvedFrame
            = { rejection.selectedFrame, -1 };
        if (!rejection.selectedFromPosition) {
            viewportRequestState(viewport).activeRequest.target.position = -1;
        }
        viewportRequestState(viewport).playbackPosition = -1;
    }
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    if (rejection.clearPlaybackStartPending) {
        viewportRequestState(viewport).providerPlaybackStartPending = false;
    }
    recordTargetSpreadTerminal(viewport, role, rejection.status, rejection.reason,
        ImageViewportInternal::FailureScope::DisplayRequest, {}, changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.diagnostics = changes.diagnostics || diagnosticsValueChanged;
    return changes;
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }

    const bool selectedFromPlaybackStart
        = viewportRequestState(viewport).providerPlaybackStartPending
        && viewportRequestState(viewport).activeRequest.target.providerTargetKind
            == ImageViewportInternal::ProviderRequestTargetKind::Playback;
    const bool selectedFromPosition
        = viewportRequestState(viewport).activeRequest.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Position;
    ImageViewportInternal::ProviderRequestTargetKind requestTargetKind = selectedFromPlaybackStart
        ? ImageViewportInternal::ProviderRequestTargetKind::Playback
        : (selectedFromPosition ? ImageViewportInternal::ProviderRequestTargetKind::Position
                                : ImageViewportInternal::ProviderRequestTargetKind::Frame);
    int selectedFrame = viewportRequestState(viewport).activeRequest.target.frame >= 0
        ? viewportRequestState(viewport).activeRequest.target.frame
        : 0;
    const int providerFrameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;
    if (selectedFromPlaybackStart
        && (!facts.timedMetadata || !viewportProviderState(viewport).timedPlaybackSupport)) {
        return { handleProviderMetadataTargetRejection(ImageViewport::PageRole::Primary,
            { ImageViewport::RequestStatus::Unsupported,
                ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, true }) };
    }
    if (selectedFromPosition) {
        if (!facts.timedMetadata || !viewportProviderState(viewport).positionSeekSupport) {
            return { handleProviderMetadataTargetRejection(ImageViewport::PageRole::Primary,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false }) };
        }
        selectedFrame = viewport.providerFrameIndexForPosition(
            viewportRequestState(viewport).activeRequest.target.position);
    }
    if (selectedFrame < 0 || selectedFrame >= providerFrameCount) {
        return { handleProviderMetadataTargetRejection(ImageViewport::PageRole::Primary,
            { ImageViewport::RequestStatus::Unsupported, ImageViewport::RequestReason::InvalidRequest,
                selectedFrame, true, selectedFromPosition, false }) };
    }

    return handleProviderMetadataTargetSelection(
        { requestTargetKind, selectedFrame, selectedFromPosition, facts.timedMetadata });
}

ViewportProviderMetadataTargetPolicyResult
ViewportController::handleSecondaryProviderMetadataTargetPolicy(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    ViewportProviderMetadataTargetPolicyResult result;
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return result;
    }

    const ImageViewportInternal::DisplayRequest& request
        = viewportRequestState(viewport).secondaryActiveRequest;
    const bool currentIdentity = request.identity.id != 0
        && request.identity.id == viewportRequestState(viewport).activeRequest.identity.id;
    if (!currentIdentity) {
        return result;
    }

    ImageViewportInternal::DisplayRequestTarget target;
    if (isUnknownMetadataInitialRequest(request)) {
        target = {
            0,
            facts.timedMetadata ? 0 : -1,
            ImageViewportInternal::ProviderRequestTargetKind::Frame,
        };
    } else if (request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Playback) {
        if (!facts.timedMetadata || !state.secondaryProvider.timedPlaybackSupport) {
            result.changes = handleProviderMetadataTargetRejection(ImageViewport::PageRole::Secondary,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false });
            return result;
        }
        int selectedFrame = request.target.frame;
        if (selectedFrame < 0) {
            selectedFrame = 0;
        }
        const int providerFrameCount = facts.timingIntervals.frameCount();
        if (selectedFrame >= providerFrameCount) {
            result.changes = handleProviderMetadataTargetRejection(ImageViewport::PageRole::Secondary,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, selectedFrame, false, false,
                    false });
            return result;
        }
        target = {
            selectedFrame,
            facts.timingIntervals.frameStartPosition(selectedFrame),
            ImageViewportInternal::ProviderRequestTargetKind::Playback,
        };
    } else if (request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Frame) {
        const int providerFrameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;
        if (request.target.frame < 0 || request.target.frame >= providerFrameCount) {
            result.changes = handleProviderMetadataTargetRejection(ImageViewport::PageRole::Secondary,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, request.target.frame, false,
                    false, false });
            return result;
        }
        target = {
            request.target.frame,
            facts.timedMetadata ? facts.timingIntervals.frameStartPosition(request.target.frame)
                                : -1,
            ImageViewportInternal::ProviderRequestTargetKind::Frame,
        };
    } else if (request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Position) {
        if (!facts.timedMetadata || !state.secondaryProvider.positionSeekSupport) {
            result.changes = handleProviderMetadataTargetRejection(ImageViewport::PageRole::Secondary,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false });
            return result;
        }
        const int selectedFrame
            = facts.timingIntervals.frameIndexForPosition(request.target.position);
        if (selectedFrame < 0) {
            result.changes = handleProviderMetadataTargetRejection(ImageViewport::PageRole::Secondary,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, -1, false, false, false });
            return result;
        }
        target = {
            selectedFrame,
            request.target.position,
            ImageViewportInternal::ProviderRequestTargetKind::Position,
        };
    } else {
        return result;
    }

    const ViewportProviderFrameRequestStartResult start
        = startSecondaryProviderFrameRequest(target);
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    result.changes.requestRevision = true;
    result.changes.requestState = true;
    if (!start.accepted) {
        result.changes.diagnostics = true;
    }
    return result;
}

ViewportProviderMetadataTargetPolicyResult
ViewportController::handleProviderMetadataTargetSelection(
    ViewportProviderMetadataTargetSelection selection)
{
    ViewportProviderMetadataTargetPolicyResult result;
    const bool carrySecondaryInitialRequest = hasSecondaryProviderSequence(viewport)
        && viewportRequestState(viewport).secondaryActiveRequest.identity.id
            == viewportRequestState(viewport).activeRequest.identity.id
        && isUnknownMetadataInitialRequest(viewportRequestState(viewport).activeRequest)
        && isUnknownMetadataInitialRequest(
            viewportRequestState(viewport).secondaryActiveRequest);
    const bool rememberAsLatestNonPlayback
        = selection.targetKind != ImageViewportInternal::ProviderRequestTargetKind::Playback;
    const int selectedPosition = selection.selectedFromPosition
        ? viewportRequestState(viewport).activeRequest.target.position
        : selection.timedMetadata ? viewport.providerFrameStartPosition(selection.selectedFrame)
                                  : -1;
    const ImageViewportInternal::ResolvedFrameIdentity resolvedFrame {
        selection.selectedFrame,
        selection.timedMetadata ? viewport.providerFrameStartPosition(selection.selectedFrame) : -1,
    };
    beginAcceptedDisplayRequest(viewport,
        ImageViewportInternal::DisplayRequestOrigin::MetadataBoundSelection,
        { selection.selectedFrame, selectedPosition, selection.targetKind }, resolvedFrame,
        rememberAsLatestNonPlayback);
    if (carrySecondaryInitialRequest) {
        viewportRequestState(viewport).secondaryActiveRequest.identity
            = viewportRequestState(viewport).activeRequest.identity;
        viewportRequestState(viewport).secondaryActiveRequest.preparedPayloadId
            = viewportRequestState(viewport).activeRequest.preparedPayloadId;
    }
    viewportRequestState(viewport).playbackPosition
        = viewportRequestState(viewport).activeRequest.target.position;
    publishProviderFrameLoadingState(viewport);

    viewportRequestState(viewport).providerPlaybackStartPending = false;
    const ViewportProviderFrameRequestStartResult start
        = startProviderFrameRequest({ viewportRequestState(viewport).activeRequest.target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (!start.accepted) {
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = true;
        return result;
    }

    result.changes.requestRevision = true;
    result.changes.requestState = true;
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderAcceptedMetadataFacts(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }

    viewportProviderState(viewport).metadataReady = true;
    viewportProviderState(viewport).timedMetadata = facts.timedMetadata;
    viewportProviderState(viewport).timedPlaybackSupport = facts.timedPlaybackSupport;
    viewportProviderState(viewport).frameSeekSupport = facts.frameSeekSupport;
    viewportProviderState(viewport).positionSeekSupport = facts.positionSeekSupport;
    viewportProviderState(viewport).logicalSize = facts.logicalSize;
    viewportProviderState(viewport).timingIntervals = facts.timingIntervals;
    viewportProviderState(viewport).authoredAnimationFacts = facts.authoredAnimationFacts;
    return {};
}

ViewportProviderMetadataEventAcceptance ViewportController::acceptSecondaryProviderMetadataEvent(
    ViewportProviderMetadataEvent event)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }
    if (!state.secondaryProvider.session || !state.secondaryProvider.activeMetadataToken.isValid()
        || event.token != state.secondaryProvider.activeMetadataToken) {
        return {};
    }

    state.secondaryProvider.activeMetadataToken = {};
    return { true };
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleSecondaryProviderAcceptedMetadataFacts(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }

    state.secondaryProvider.metadataReady = true;
    state.secondaryProvider.timedMetadata = facts.timedMetadata;
    state.secondaryProvider.timedPlaybackSupport = facts.timedPlaybackSupport;
    state.secondaryProvider.frameSeekSupport = facts.frameSeekSupport;
    state.secondaryProvider.positionSeekSupport = facts.positionSeekSupport;
    state.secondaryProvider.logicalSize = facts.logicalSize;
    state.secondaryProvider.timingIntervals = facts.timingIntervals;
    state.secondaryProvider.authoredAnimationFacts = facts.authoredAnimationFacts;

    ImageViewportInternal::ViewportChangeSet changes;
    changes.requestRevision = true;
    changes.requestState = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaitingEvent(
    ViewportProviderWaitingEvent event)
{
    return handleProviderWaitingEvent(ImageViewport::PageRole::Primary, event);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaitingEvent(
    ImageViewport::PageRole role, ViewportProviderWaitingEvent event)
{
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return {};
    }
    if (role == ImageViewport::PageRole::Secondary) {
        if (!hasSecondaryProviderSequence(viewport) || !state.secondaryProvider.session) {
            return {};
        }
        if (event.progress
            && (!std::isfinite(event.progressValue) || event.progressValue < 0.0
                || event.progressValue > 1.0)) {
            return {};
        }

        const bool activeMetadataToken = !state.secondaryProvider.metadataReady
            && state.secondaryProvider.activeMetadataToken.isValid()
            && event.token == state.secondaryProvider.activeMetadataToken;
        const bool activeFrameToken = activeSecondaryProviderFrameTokenMatchesActiveRequest(
            viewport, state.secondaryProvider, event.token);
        if (!activeMetadataToken && !activeFrameToken) {
            return {};
        }

        return handleProviderWaiting();
    }

    if (!viewport.hasProviderSequence() || !viewportProviderState(viewport).session) {
        return {};
    }
    if (event.progress
        && (!std::isfinite(event.progressValue) || event.progressValue < 0.0
            || event.progressValue > 1.0)) {
        return {};
    }

    const bool activeMetadataToken = !viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).activeMetadataToken.isValid()
        && event.token == viewportProviderState(viewport).activeMetadataToken;
    const bool activeFrameToken
        = activeProviderFrameTokenMatchesActiveRequest(viewport, event.token);
    if (!activeMetadataToken && !activeFrameToken) {
        return {};
    }

    return handleProviderWaiting();
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaiting()
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (viewportRequestState(viewport).status != ImageViewport::RequestStatus::Loading
        || viewportRequestState(viewport).reason == ImageViewport::RequestReason::ProviderWaiting) {
        return changes;
    }

    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
    changes.requestRevision = true;
    changes.requestState = true;
    return changes;
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderEndOfSequenceEvent(
    ViewportProviderEndOfSequenceEvent event)
{
    return handleProviderEndOfSequenceEvent(ImageViewport::PageRole::Primary, event);
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderEndOfSequenceEvent(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event)
{
    const bool sealed = targetSpreadTerminalSealedForActiveRequest(viewport);
    if (role == ImageViewport::PageRole::Secondary) {
        if (!hasSecondaryProviderSequence(viewport) || !state.secondaryProvider.session) {
            return {};
        }

        const bool activeMetadataToken = !state.secondaryProvider.metadataReady
            && state.secondaryProvider.activeMetadataToken.isValid()
            && event.token == state.secondaryProvider.activeMetadataToken;
        const bool activeFrameToken = activeSecondaryProviderFrameTokenMatchesActiveRequest(
            viewport, state.secondaryProvider, event.token);
        if (!activeMetadataToken && !activeFrameToken) {
            return {};
        }

        if (sealed) {
            return {};
        }

        if (activeMetadataToken || !state.secondaryProvider.metadataReady
            || !state.secondaryProvider.timedMetadata
            || !activeSecondaryProviderFrameRequestIsPlayback(viewport, state.secondaryProvider)) {
            ViewportProviderEndOfSequenceResult result;
            result.changes = handleSecondaryProviderEndOfSequenceProtocolViolation(
                { activeMetadataToken, activeFrameToken });
            result.providerFrameTransport.closeSession = state.secondaryProvider.session != nullptr;
            result.providerFrameTransport.sessionClose = handleSecondaryProviderSessionClose();
            return result;
        }

        return handleSecondaryProviderPlaybackEndOfSequence();
    }

    if (!viewport.hasProviderSequence() || !viewportProviderState(viewport).session) {
        return {};
    }

    const bool activeMetadataToken = !viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).activeMetadataToken.isValid()
        && event.token == viewportProviderState(viewport).activeMetadataToken;
    const bool activeFrameToken
        = activeProviderFrameTokenMatchesActiveRequest(viewport, event.token);
    if (!activeMetadataToken && !activeFrameToken) {
        return {};
    }

    if (sealed) {
        return {};
    }

    if (activeMetadataToken || !viewportProviderState(viewport).metadataReady
        || !viewportProviderState(viewport).timedMetadata
        || !activeProviderFrameRequestIsPlayback(viewport)) {
        ViewportProviderEndOfSequenceResult result;
        result.changes = handleProviderEndOfSequenceProtocolViolation(
            { activeMetadataToken, activeFrameToken });
        result.providerFrameTransport.closeSession
            = viewportProviderState(viewport).session != nullptr;
        result.providerFrameTransport.sessionClose = handleProviderSessionClose();
        return result;
    }

    return handleProviderPlaybackEndOfSequence();
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderEndOfSequenceProtocolViolation(
    ViewportProviderEndOfSequenceProtocolViolation violation)
{
    ImageViewportInternal::ViewportChangeSet changes;
    clearQueuedProviderFrameRequest(viewport);
    if (violation.activeMetadataToken) {
        viewportProviderState(viewport).activeMetadataToken = {};
    }
    if (violation.activeFrameToken) {
        viewportProviderState(viewport).activeFrameToken = {};
    }
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Primary,
        ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::PayloadRejection,
        violation.activeMetadataToken ? ImageViewportInternal::FailureScope::Generation
                                      : ImageViewportInternal::FailureScope::DisplayRequest,
        QStringLiteral("provider protocol violation"), changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleSecondaryProviderEndOfSequenceProtocolViolation(
    ViewportProviderEndOfSequenceProtocolViolation violation)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (violation.activeMetadataToken) {
        state.secondaryProvider.activeMetadataToken = {};
    }
    if (violation.activeFrameToken) {
        state.secondaryProvider.activeFrameToken = {};
    }
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    recordTargetSpreadTerminal(viewport, ImageViewport::PageRole::Secondary,
        ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::PayloadRejection,
        violation.activeMetadataToken ? ImageViewportInternal::FailureScope::Generation
                                      : ImageViewportInternal::FailureScope::DisplayRequest,
        QStringLiteral("provider protocol violation"), changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderPlaybackEndOfSequence()
{
    ViewportProviderEndOfSequenceResult result;
    viewportProviderState(viewport).activeFrameToken = {};
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();

    int selectedFrame = 0;
    int selectedPosition = 0;
    if (viewportRequestState(viewport).looping) {
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackPosition = 0;
    } else {
        selectedFrame = viewport.frameCount() - 1;
        selectedPosition = viewport.providerFrameStartPosition(selectedFrame);
        viewportRequestState(viewport).playbackPosition = viewport.totalDuration();
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = true;
    }

    viewportRequestState(viewport).activeRequest.target.frame = selectedFrame;
    viewportRequestState(viewport).activeRequest.target.position = selectedPosition;
    viewportRequestState(viewport).activeRequest.resolvedFrame
        = { selectedFrame, selectedPosition };
    viewportRequestState(viewport).activeRequest.target.providerTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Playback;

    if (!viewportRequestState(viewport).looping && viewport.hasReadyDisplay()
        && viewportDisplayState(viewport).displayedRequest.generation
            == viewportRequestState(viewport).sequenceGeneration
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.frame
            == selectedFrame
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.position
            == selectedPosition) {
        publishReadyDisplayState(viewport);
        setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Stopped);
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        result.changes.requestRevision = true;
        result.changes.displayRevision = true;
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    publishProviderFrameLoadingState(viewport);
    const ViewportProviderFrameRequestStartResult start
        = startProviderFrameRequest({ viewportRequestState(viewport).activeRequest.target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (!start.accepted) {
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = true;
        return result;
    }
    setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Waiting);
    result.changes.requestRevision = true;
    result.changes.displayRevision = true;
    result.changes.requestState = true;
    result.changes.displayState = true;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportProviderEndOfSequenceResult
ViewportController::handleSecondaryProviderPlaybackEndOfSequence()
{
    ViewportProviderEndOfSequenceResult result;
    state.secondaryProvider.activeFrameToken = {};
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();

    int selectedFrame = 0;
    int selectedPosition = 0;
    if (viewportRequestState(viewport).looping) {
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackPosition = 0;
    } else {
        selectedFrame = state.secondaryProvider.timingIntervals.frameCount() - 1;
        selectedPosition
            = state.secondaryProvider.timingIntervals.frameStartPosition(selectedFrame);
        viewportRequestState(viewport).playbackPosition
            = state.secondaryProvider.timingIntervals.totalDuration();
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = true;
    }

    const ImageViewportInternal::DisplayRequest primaryRequest
        = viewportRequestState(viewport).activeRequest;
    beginAcceptedDisplayRequest(viewport, ImageViewportInternal::DisplayRequestOrigin::Playback,
        primaryRequest.target, primaryRequest.resolvedFrame, false);

    const ImageViewportInternal::DisplayRequestTarget providerTarget {
        selectedFrame,
        selectedPosition,
        ImageViewportInternal::ProviderRequestTargetKind::Playback,
    };
    setSecondaryActiveRequest(viewport, providerTarget, { selectedFrame, selectedPosition }, false);

    publishProviderFrameLoadingState(viewport);
    const ViewportProviderFrameRequestStartResult start
        = startSecondaryProviderFrameRequest(providerTarget);
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (!start.accepted) {
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = true;
        return result;
    }
    setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Waiting);
    result.changes.requestRevision = true;
    result.changes.displayRevision = true;
    result.changes.requestState = true;
    result.changes.displayState = true;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession()
{
    ViewportProviderFrameTransportEffect effect;
    effect.closeSession = viewportProviderState(viewport).session != nullptr;
    effect.sessionClose = handleProviderSessionClose();
    return effect;
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession(
    ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? closeSecondaryProviderSession()
                                                      : closeProviderSession();
}

ViewportProviderFrameTransportEffect ViewportController::closeSecondaryProviderSession()
{
    ViewportProviderFrameTransportEffect effect;
    effect.closeSession = state.secondaryProvider.session != nullptr;
    effect.sessionClose = handleProviderSessionClose(ImageViewport::PageRole::Secondary);
    return effect;
}

ViewportProviderSessionClose ViewportController::handleProviderSessionClose()
{
    return handleProviderSessionClose(ImageViewport::PageRole::Primary);
}

ViewportProviderSessionClose ViewportController::handleProviderSessionClose(
    ImageViewport::PageRole role)
{
    ViewportProviderSessionClose sessionClose;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (role == ImageViewport::PageRole::Primary) {
        clearQueuedProviderFrameRequest(viewport);
    }
    if (!provider.session) {
        return sessionClose;
    }

    sessionClose.metadataToken = provider.activeMetadataToken;
    sessionClose.frameToken = provider.activeFrameToken;
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    provider.nextRequestToken = 0;
    return sessionClose;
}

ViewportProviderSessionClose ViewportController::handleSecondaryProviderSessionClose()
{
    return handleProviderSessionClose(ImageViewport::PageRole::Secondary);
}

ViewportProviderRequestTokenAllocation ViewportController::allocateProviderRequestToken()
{
    return allocateProviderRequestToken(ImageViewport::PageRole::Primary);
}

ViewportProviderRequestTokenAllocation ViewportController::allocateProviderRequestToken(
    ImageViewport::PageRole role)
{
    ViewportProviderRequestTokenAllocation allocation;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (provider.nextRequestToken == std::numeric_limits<quint64>::max()) {
        allocation.closeSession = provider.session != nullptr;
        allocation.sessionClose = handleProviderSessionClose(role);
        return allocation;
    }

    ++provider.nextRequestToken;
    allocation.token = ImageSequenceProviderRequestToken(provider.nextRequestToken);
    return allocation;
}

ViewportProviderRequestTokenAllocation ViewportController::allocateSecondaryProviderRequestToken()
{
    return allocateProviderRequestToken(ImageViewport::PageRole::Secondary);
}

ViewportProviderMetadataRequestStartResult ViewportController::startProviderMetadataRequest()
{
    return startProviderMetadataRequest(ImageViewport::PageRole::Primary);
}

ViewportProviderMetadataRequestStartResult ViewportController::startProviderMetadataRequest(
    ImageViewport::PageRole role)
{
    ViewportProviderMetadataRequestStartResult result;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    const ViewportProviderRequestTokenAllocation allocation = allocateProviderRequestToken(role);
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    provider.activeMetadataToken = allocation.token;
    if (!provider.activeMetadataToken.isValid()) {
        publishProviderTokenExhaustion(viewport);
        return result;
    }

    result.sendCommand = provider.session != nullptr;
    result.token = provider.activeMetadataToken;
    return result;
}

ViewportProviderMetadataRequestStartResult
ViewportController::startSecondaryProviderMetadataRequest()
{
    return startProviderMetadataRequest(ImageViewport::PageRole::Secondary);
}

ViewportProviderFrameRequestStartResult ViewportController::startSecondaryProviderFrameRequest(
    int frame)
{
    const int position = state.secondaryProvider.timedMetadata
        ? state.secondaryProvider.timingIntervals.frameStartPosition(frame)
        : -1;
    return startSecondaryProviderFrameRequest(
        { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Frame });
}

ViewportProviderFrameRequestStartResult ViewportController::startSecondaryProviderFrameRequest(
    ImageViewportInternal::DisplayRequestTarget target)
{
    ViewportProviderFrameRequestStartResult result;
    const ViewportProviderRequestTokenAllocation allocation
        = allocateSecondaryProviderRequestToken();
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    state.secondaryProvider.activeFrameToken = allocation.token;
    if (!state.secondaryProvider.activeFrameToken.isValid()) {
        publishProviderTokenExhaustion(viewport);
        return result;
    }

    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
    const int resolvedPosition = state.secondaryProvider.timedMetadata
        ? state.secondaryProvider.timingIntervals.frameStartPosition(target.frame)
        : -1;
    setSecondaryActiveRequest(viewport, target, { target.frame, resolvedPosition },
        target.providerTargetKind != ImageViewportInternal::ProviderRequestTargetKind::Playback);
    result.accepted = true;
    result.sendCommand = state.secondaryProvider.session != nullptr;
    result.command.token = state.secondaryProvider.activeFrameToken;
    result.command.frame = target.frame;
    result.command.position = target.position;
    result.command.targetKind = target.providerTargetKind;
    return result;
}

ViewportProviderFrameQueueResult ViewportController::queueProviderFrameRequest(
    ViewportProviderFrameQueueRequest request)
{
    ViewportProviderFrameQueueResult result;
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::RequestQueued;
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    discardPendingRenderCommit(viewport);

    if (viewportProviderState(viewport).session
        && viewportProviderState(viewport).activeFrameToken.isValid()) {
        result.cancelToken = viewportProviderState(viewport).activeFrameToken;
    }
    viewportProviderState(viewport).activeFrameToken = {};

    viewportProviderState(viewport).queuedFrameRequest = true;
    viewportProviderState(viewport).queuedFrameGeneration
        = viewportRequestState(viewport).sequenceGeneration;
    viewportProviderState(viewport).queuedFrameRequestId
        = viewportRequestState(viewport).activeRequest.identity.id;
    viewportProviderState(viewport).queuedFrame = request.frame;
    viewportProviderState(viewport).queuedPosition
        = viewportRequestState(viewport).activeRequest.target.position;
    viewportProviderState(viewport).queuedResolvedFrame
        = viewportRequestState(viewport).activeRequest.resolvedFrame;
    viewportProviderState(viewport).queuedFrameFromPlayback
        = request.targetKind == ImageViewportInternal::ProviderRequestTargetKind::Playback;
    viewportProviderState(viewport).queuedFrameTargetKind = request.targetKind;
    result.scheduleFlush = true;
    return result;
}

ViewportProviderFrameQueueFlush ViewportController::flushQueuedProviderFrameRequest()
{
    ViewportProviderFrameQueueFlush flush;
    if (!viewportProviderState(viewport).queuedFrameRequest || !viewport.hasProviderSequence()
        || !viewportProviderState(viewport).session) {
        clearQueuedProviderFrameRequest(viewport);
        return flush;
    }

    const int queuedFrame = viewportProviderState(viewport).queuedFrame;
    const int queuedPosition = viewportProviderState(viewport).queuedPosition;
    const ImageViewportInternal::ResolvedFrameIdentity queuedResolvedFrame
        = viewportProviderState(viewport).queuedResolvedFrame;
    const quint64 queuedRequestId = viewportProviderState(viewport).queuedFrameRequestId;
    const ImageViewportInternal::ProviderRequestTargetKind queuedTargetKind
        = viewportProviderState(viewport).queuedFrameTargetKind;
    const bool stillCurrent = viewportProviderState(viewport).queuedFrameGeneration
            == viewportRequestState(viewport).sequenceGeneration
        && queuedRequestId == viewportRequestState(viewport).activeRequest.identity.id
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::RequestQueued
        && viewportRequestState(viewport).activeRequest.target.frame == queuedFrame
        && viewportRequestState(viewport).activeRequest.target.position == queuedPosition
        && viewportRequestState(viewport).activeRequest.resolvedFrame.frame
            == queuedResolvedFrame.frame
        && viewportRequestState(viewport).activeRequest.resolvedFrame.position
            == queuedResolvedFrame.position
        && viewportRequestState(viewport).activeRequest.target.providerTargetKind
            == queuedTargetKind;
    clearQueuedProviderFrameRequest(viewport);
    if (!stillCurrent) {
        return flush;
    }

    flush.startRequest = true;
    flush.frame = queuedFrame;
    flush.targetKind = queuedTargetKind;
    return flush;
}

ViewportProviderFrameRequestStartResult ViewportController::startProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    ViewportProviderFrameRequestStartResult result;
    clearQueuedProviderFrameRequest(viewport);
    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
    const ViewportProviderRequestTokenAllocation allocation = allocateProviderRequestToken();
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    viewportProviderState(viewport).activeFrameToken = allocation.token;
    if (!viewportProviderState(viewport).activeFrameToken.isValid()) {
        publishProviderTokenExhaustion(viewport);
        return result;
    }

    viewportRequestState(viewport).activeRequest.providerFrameToken
        = viewportProviderState(viewport).activeFrameToken;
    result.accepted = true;
    result.sendCommand = viewportProviderState(viewport).session != nullptr;
    result.command.token = viewportProviderState(viewport).activeFrameToken;
    result.command.frame = viewportRequestState(viewport).activeRequest.resolvedFrame.frame;
    result.command.position = request.target.position;
    result.command.targetKind = request.target.providerTargetKind;
    return result;
}

ViewportProviderFrameDispatchResult ViewportController::dispatchProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    ViewportProviderFrameDispatchResult result;
    if (viewportProviderState(viewport).activeFrameToken.isValid()) {
        result.accepted = true;
        appendProviderFrameQueueResult(result.transport,
            queueProviderFrameRequest({ request.target.frame, request.target.providerTargetKind }));
        return result;
    }

    const ViewportProviderFrameRequestStartResult start = startProviderFrameRequest(request);
    result.accepted = start.accepted;
    appendProviderFrameStartResult(result.transport, start);
    return result;
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization(double devicePixelRatio)
{
    ViewportRenderSynchronization synchronization;
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        synchronization.oldContentRect = viewport.contentRect();
        synchronization.oldVisibleImageRect = viewport.visibleImageRect();
        synchronization.oldDisplayStatus = viewportDisplayState(viewport).status;
        synchronization.geometryState = controllerGeometryState(
            viewport, state.presentation, devicePixelRatio, std::nullopt,
            GeometryProjectionTarget::CurrentDisplay);
        synchronization.renderSnapshot
            = renderSnapshotForSynchronization(viewport, synchronization, state.presentation);
        return synchronization;
    }
    synchronization.pendingSecondaryCommit
        = viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
            || viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting)
        && hasPendingSecondarySpreadPayload(viewport) && !viewport.itemBounds().isEmpty();
    synchronization.pendingProviderCommit = viewport.hasProviderSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
            || viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting)
        && viewportDisplayState(viewport).pendingRenderPayload.commitPending
        && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull()
        && (!hasSecondaryProviderSequence(viewport)
            || !viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull())
        && !viewport.itemBounds().isEmpty();
    synchronization.oldContentRect = viewport.contentRect();
    synchronization.oldVisibleImageRect = viewport.visibleImageRect();
    synchronization.oldDisplayStatus = viewportDisplayState(viewport).status;
    if (synchronization.pendingProviderCommit || synchronization.pendingSecondaryCommit) {
        synchronization.preparedPayload = viewportDisplayState(viewport).pendingRenderPayload;
    } else if (viewportDisplayState(viewport).pendingRenderPayload.commitPending
        && viewport.hasReadyDisplay()) {
        synchronization.preparedPayload = viewportDisplayState(viewport).pendingRenderPayload;
        synchronization.preparedPayload.image = viewportDisplayState(viewport).displayedImage;
    }
    synchronization.geometryState = controllerGeometryState(viewport, state.presentation,
        devicePixelRatio, std::nullopt,
        synchronization.pendingProviderCommit || synchronization.pendingSecondaryCommit
            ? GeometryProjectionTarget::PendingRender
            : GeometryProjectionTarget::CurrentDisplay);
    synchronization.renderSnapshot
        = renderSnapshotForSynchronization(viewport, synchronization, state.presentation);
    return synchronization;
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderCommit(
    ViewportRenderAcknowledgement acknowledgement, bool renderedImagePresent,
    const ViewportRenderSynchronization& synchronization)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return changes;
    }
    if (!renderedImagePresent) {
        return changes;
    }

    const bool renderMatchesPending
        = renderCommitAcknowledgementMatchesPending(viewport, acknowledgement);
    if (!renderMatchesPending) {
        return changes;
    }
    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;
    if (synchronization.pendingProviderCommit) {
        publishSequenceReadyState(viewport, synchronization.preparedPayload);
    } else if (synchronization.pendingSecondaryCommit) {
        publishStagedBuiltInPrimarySpreadReadyState(viewport);
    }
    if (synchronization.pendingSecondaryCommit) {
        viewportDisplayState(viewport).secondaryDisplayedImage
            = viewportDisplayState(viewport).secondaryPendingRenderPayload.image;
        viewportDisplayState(viewport).secondaryDisplayedImageSize
            = state.secondaryProvider.logicalSize;
    }
    const bool resumePlaybackAfterCommit
        = viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready;
    viewportDisplayState(viewport).commitDisplayedRequestSnapshot(
        viewportRequestState(viewport).sequenceGeneration,
        viewportRequestState(viewport).activeRequest,
        viewportDisplayState(viewport).pendingRenderPayload.payloadId);
    viewportDisplayState(viewport).clearPendingRenderPayload();
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    if (resumePlaybackAfterCommit) {
        setPlaybackPhase(viewport, changes,
            viewportRequestState(viewport).stopPlaybackWhenRequestReady
                ? ImageViewport::PlaybackPhase::Stopped
                : ImageViewport::PlaybackPhase::Playing);
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    }
    if (synchronization.pendingProviderCommit || synchronization.pendingSecondaryCommit) {
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.requestState = true;
        changes.displayState = viewportDisplayState(viewport).status
            != (synchronization.pendingProviderCommit ? synchronization.oldDisplayStatus
                                                      : oldDisplayStatus);
        changes.geometryState = ImageViewportInternal::rectsDifferExactly(
                                    viewport.contentRect(), synchronization.oldContentRect)
            || ImageViewportInternal::rectsDifferExactly(
                viewport.visibleImageRect(), synchronization.oldVisibleImageRect);
    }
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderFailure(
    ViewportRenderAcknowledgement acknowledgement)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        return changes;
    }
    const bool renderMatchesPending
        = renderFailureAcknowledgementMatchesPending(viewport, acknowledgement);
    const bool pendingProviderCommit = viewport.hasProviderSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
            || viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting)
        && viewportDisplayState(viewport).pendingRenderPayload.commitPending
        && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull()
        && (!hasSecondaryProviderSequence(viewport)
            || !viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull());
    const bool pendingSecondaryCommit = hasPendingSecondarySpreadPayload(viewport);
    if (!renderMatchesPending
        || (viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready
            && !pendingProviderCommit && !pendingSecondaryCommit)) {
        return changes;
    }

    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;

    viewportDisplayState(viewport).clearPendingRenderPayload();
    if (viewportDisplayState(viewport).renderFailureRetainedDisplayValid) {
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Retained;
        viewportDisplayState(viewport).displayedRequest
            = viewportDisplayState(viewport).renderFailureRetainedRequest;
        viewportDisplayState(viewport).displayedImageSize
            = viewportDisplayState(viewport).renderFailureRetainedImageSize;
        viewportDisplayState(viewport).displayedImage
            = viewportDisplayState(viewport).renderFailureRetainedImage;
    } else {
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
        viewportDisplayState(viewport).clearDisplayedDisplay();
    }
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    recordTargetSpreadTerminal(viewport, acknowledgement.failedRole,
        ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::RenderFailure,
        ImageViewportInternal::FailureScope::DisplayRequest,
        QStringLiteral("render commit failed"), changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);

    changes.displayRevision = true;
    changes.displayState = viewportDisplayState(viewport).status != oldDisplayStatus;
    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    return changes;
}

int ViewportController::playbackTimerInterval() const
{
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Playing
        || viewportRequestState(viewport).status != ImageViewport::RequestStatus::Ready) {
        return -1;
    }

    if (viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Secondary) {
        if (state.secondaryProvider.metadataReady && state.secondaryProvider.timedMetadata) {
            const int currentFrame
                = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
            if (currentFrame < 0
                || currentFrame >= state.secondaryProvider.timingIntervals.frameCount()) {
                return -1;
            }
            const int frameStart
                = state.secondaryProvider.timingIntervals.frameStartPosition(currentFrame);
            const int nextFrameStart
                = currentFrame + 1 < state.secondaryProvider.timingIntervals.frameCount()
                ? state.secondaryProvider.timingIntervals.frameStartPosition(currentFrame + 1)
                : state.secondaryProvider.timingIntervals.totalDuration();
            const int frameDuration = nextFrameStart - frameStart;
            if (frameStart < 0 || frameDuration <= 0) {
                return -1;
            }

            const int playbackPosition = viewportRequestState(viewport).playbackPosition >= 0
                ? viewportRequestState(viewport).playbackPosition
                : frameStart;
            const int remaining = frameStart + frameDuration - playbackPosition;
            return std::max(1, remaining);
        }

        if (!viewport.hasSecondaryTimedSequence()) {
            return -1;
        }
        const int currentFrame = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
        if (currentFrame < 0 || currentFrame >= viewport.secondarySequenceFrameCount()) {
            return -1;
        }
        const int frameStart = viewport.secondarySequenceFrameStartPosition(currentFrame);
        const int nextFrameStart = currentFrame + 1 < viewport.secondarySequenceFrameCount()
            ? viewport.secondarySequenceFrameStartPosition(currentFrame + 1)
            : viewport.secondaryTotalDuration();
        const int frameDuration = nextFrameStart - frameStart;
        if (frameStart < 0 || frameDuration <= 0) {
            return -1;
        }

        const int playbackPosition = viewportRequestState(viewport).playbackPosition >= 0
            ? viewportRequestState(viewport).playbackPosition
            : frameStart;
        const int remaining = frameStart + frameDuration - playbackPosition;
        return std::max(1, remaining);
    }

    int frameStart = -1;
    int frameDuration = -1;
    const int currentFrame = viewportRequestState(viewport).activeRequest.target.frame;
    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata) {
        if (currentFrame < 0 || currentFrame >= viewport.frameCount()) {
            return -1;
        }
        frameStart = viewport.providerFrameStartPosition(currentFrame);
        frameDuration = viewportProviderState(viewport).timingIntervals.frameDuration(currentFrame);
    } else if (viewport.hasTimedSequence()) {
        if (currentFrame < 0 || currentFrame >= viewport.sequenceFrameCount()) {
            return -1;
        }
        frameStart = viewport.sequenceFrameStartPosition(currentFrame);
        const int nextFrameStart = currentFrame + 1 < viewport.sequenceFrameCount()
            ? viewport.sequenceFrameStartPosition(currentFrame + 1)
            : viewport.totalDuration();
        frameDuration = nextFrameStart - frameStart;
    } else {
        return -1;
    }

    if (frameStart < 0 || frameDuration <= 0) {
        return -1;
    }

    const int playbackPosition = viewportRequestState(viewport).playbackPosition >= 0
        ? viewportRequestState(viewport).playbackPosition
        : frameStart;
    const int remaining = frameStart + frameDuration - playbackPosition;
    return std::max(1, remaining);
}

ViewportPlaybackAdvanceResult ViewportController::advancePlayback(int elapsedMilliseconds)
{
    ViewportPlaybackAdvanceResult result;
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Playing
        || elapsedMilliseconds <= 0) {
        return result;
    }

    if (viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Secondary) {
        if (state.secondaryProvider.metadataReady && state.secondaryProvider.timedMetadata) {
            const int totalDuration = state.secondaryProvider.timingIntervals.totalDuration();
            const int previousFrame
                = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
            const int currentFrame
                = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
            const PlaybackAdvanceTarget target = playbackAdvanceTarget(
                elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
                effectiveLoopingForPlayback(
                    viewport, state.secondaryProvider.authoredAnimationFacts),
                totalDuration, state.secondaryProvider.timingIntervals.frameCount(),
                [this](int frame) {
                    return state.secondaryProvider.timingIntervals.frameStartPosition(frame);
                },
                [this](int position) {
                    return state.secondaryProvider.timingIntervals.frameIndexForPosition(position);
                });
            if (!target.valid) {
                return result;
            }

            viewportRequestState(viewport).playbackPosition = target.playbackPosition;
            if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame
                && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready) {
                return result;
            }

            const ImageViewportInternal::DisplayRequest primaryRequest
                = viewportRequestState(viewport).activeRequest;
            beginAcceptedDisplayRequest(viewport,
                ImageViewportInternal::DisplayRequestOrigin::Playback, primaryRequest.target,
                primaryRequest.resolvedFrame, false);
            const DisplayRequestTarget providerTarget {
                target.displayTarget.frame,
                target.displayTarget.position,
                ImageViewportInternal::ProviderRequestTargetKind::Playback,
            };
            setSecondaryActiveRequest(viewport, providerTarget,
                { providerTarget.frame,
                    state.secondaryProvider.timingIntervals.frameStartPosition(
                        providerTarget.frame) });
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
            viewportDisplayState(viewport).status
                = viewportDisplayState(viewport).displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            const ViewportProviderFrameRequestStartResult start
                = startSecondaryProviderFrameRequest(providerTarget);
            appendProviderFrameStartResult(result.secondaryProviderFrameTransport, start);
            if (start.accepted) {
                updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
            }

            applyPlaybackAdvancePhase(viewport, result.changes, target);
            result.changes.requestRevision = true;
            if (target.displayTarget.frame != previousFrame
                || viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready) {
                result.changes.displayRevision = true;
            }
            result.changes.requestState = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!start.accepted) {
                result.changes.diagnostics = true;
            }
            return result;
        }

        if (!viewport.hasSecondaryTimedSequence()) {
            return result;
        }

        const int totalDuration = viewport.secondaryTotalDuration();
        const int previousFrame
            = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
        const int currentFrame = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
        const PlaybackAdvanceTarget target = playbackAdvanceTarget(
            elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
            effectiveLoopingForPlayback(
                viewport, viewport.secondarySequenceAuthoredAnimationFacts()),
            totalDuration, viewport.secondarySequenceFrameCount(),
            [this](int frame) { return viewport.secondarySequenceFrameStartPosition(frame); },
            [this](int position) {
                return viewport.secondarySequenceFrameIndexForPosition(position);
            });
        if (!target.valid) {
            return result;
        }

        viewportRequestState(viewport).playbackPosition = target.playbackPosition;
        if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
            return result;
        }

        const QRectF oldContentRect = viewport.contentRect();
        const QRectF oldVisibleImageRect = viewport.visibleImageRect();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(viewport, ImageViewportInternal::DisplayRequestOrigin::Playback,
            primaryRequest.target, primaryRequest.resolvedFrame, false);
        setSecondaryActiveRequest(viewport, target.displayTarget,
            { target.displayTarget.frame, target.displayTarget.position });
        publishAcceptedTargetState(viewport);
        updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
        applyPlaybackAdvancePhase(viewport, result.changes, target);
        result.changes.requestRevision = true;
        if (target.displayTarget.frame != previousFrame
            || viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready) {
            result.changes.displayRevision = true;
        }
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.geometryState
            = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
            || ImageViewportInternal::rectsDifferExactly(
                viewport.visibleImageRect(), oldVisibleImageRect);
        result.changes.scheduleUpdate = true;
        return result;
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata) {
        const int duration = viewport.totalDuration();
        const int previousFrame = viewportRequestState(viewport).activeRequest.target.frame;
        const int currentFrame = viewportRequestState(viewport).activeRequest.target.frame;
        const PlaybackAdvanceTarget target = playbackAdvanceTarget(
            elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
            effectiveLoopingForPlayback(viewport, viewport.providerAuthoredAnimationFacts()),
            duration, viewport.frameCount(),
            [this](int frame) { return viewport.providerFrameStartPosition(frame); },
            [this](int position) { return viewport.providerFrameIndexForPosition(position); });
        if (!target.valid) {
            return result;
        }

        viewportRequestState(viewport).playbackPosition = target.playbackPosition;
        if (target.displayTarget.frame == previousFrame
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready) {
            if (viewportRequestState(viewport).stopPlaybackWhenRequestReady) {
                setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Stopped);
                viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
            } else {
                applyPlaybackAdvancePhase(viewport, result.changes, target);
            }
            return result;
        }

        applyPlaybackTarget(viewport, target.displayTarget);
        viewportRequestState(viewport).activeRequest.target.providerTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Playback;
        publishProviderFrameLoadingState(viewport);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ViewportProviderFrameDispatchResult dispatch
            = dispatchProviderFrameRequest({ viewportRequestState(viewport).activeRequest.target });
        result.providerFrameTransport = dispatch.transport;
        appendPlaybackRequestChange(viewport, result.changes, previousFrame);
        if (!dispatch.accepted) {
            result.changes.diagnostics = true;
            result.changes.scheduleUpdate = true;
            return result;
        }

        applyPlaybackAdvancePhase(viewport, result.changes, target);
        updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    if (!viewport.hasTimedSequence()) {
        return result;
    }

    const int totalDuration = viewport.totalDuration();
    const int previousFrame = viewportRequestState(viewport).activeRequest.target.frame;
    const int currentFrame = viewportRequestState(viewport).activeRequest.target.frame;
    const PlaybackAdvanceTarget target = playbackAdvanceTarget(
        elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
        effectiveLoopingForPlayback(viewport, viewport.sequenceAuthoredAnimationFacts()),
        totalDuration, viewport.sequenceFrameCount(),
        [this](int frame) { return viewport.sequenceFrameStartPosition(frame); },
        [this](int position) { return viewport.sequenceFrameIndexForPosition(position); });
    if (!target.valid) {
        return result;
    }

    viewportRequestState(viewport).playbackPosition = target.playbackPosition;
    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        return result;
    }

    applyPlaybackTarget(viewport, target.displayTarget);
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    publishAcceptedTargetState(viewport);
    updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
    applyPlaybackAdvancePhase(viewport, result.changes, target);
    appendPlaybackRequestChange(viewport, result.changes, previousFrame);
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.scheduleUpdate = true;
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
