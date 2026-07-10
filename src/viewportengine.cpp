#include "viewportengine_p.h"

#include "imageviewporttoken_p.h"

#include <limits>

namespace {
bool fitModeValid(ImageViewport::FitMode mode)
{
    switch (mode) {
    case ImageViewport::FitMode::Contain:
    case ImageViewport::FitMode::FitWidth:
    case ImageViewport::FitMode::FitHeight:
    case ImageViewport::FitMode::Manual:
        return true;
    }
    return false;
}

QRectF renderTargetRect(const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::pageItemRect(geometry, role).intersected(geometry.itemBounds);
}

QRectF renderSourceRect(const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::visiblePageRect(geometry, role);
}

const ImageViewportInternal::PreparedPayload& pendingPayloadForRole(
    const ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? display.secondaryPendingRenderPayload
                                                      : display.pendingRenderPayload;
}

ImageViewportInternal::PreparedPayload& pendingPayloadForRole(
    ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? display.secondaryPendingRenderPayload
                                                      : display.pendingRenderPayload;
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

ImageViewportInternal::ProviderGenerationState& providerGenerationStateForRole(
    ViewportEngine& engine, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? engine.secondaryProviderState()
                                                      : engine.providerState();
}

const ImageViewportInternal::ProviderGenerationState& providerGenerationStateForRole(
    const ViewportEngine& engine, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? engine.secondaryProviderState()
                                                      : engine.providerState();
}

bool targetSpreadTerminalMatchesActiveRequest(const ImageViewportInternal::RequestState& request)
{
    return request.targetSpreadTerminal.sealed
        && request.targetSpreadTerminal.generation == request.sequenceGeneration
        && request.targetSpreadTerminal.requestId == request.activeRequest.identity.id;
}

bool hasProviderSequenceForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary
        ? request.sequenceSource.facts.provider
        : (request.secondarySequence && request.secondarySequenceIsProvider);
}

ImageViewport::DisplayStatus retainedOrEmptyDisplayStatus(
    const ImageViewportInternal::DisplayState& display)
{
    const bool canRetain = (display.status == ImageViewport::DisplayStatus::Ready
                               || display.status == ImageViewport::DisplayStatus::Retained)
        && display.displayedImageSize.isValid();
    return canRetain ? ImageViewport::DisplayStatus::Retained : ImageViewport::DisplayStatus::Empty;
}

bool activeProviderFrameTokenMatchesActiveRequest(const ViewportEngine& engine,
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role,
    ImageSequenceProviderRequestToken token)
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(engine, role);
    if (!provider.activeFrameToken.isValid() || token != provider.activeFrameToken) {
        return false;
    }

    const ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(request, role);
    return token.isValid() && token == activeRequest.providerFrameToken;
}

bool displayedPrimaryPayloadMatchesActiveTarget(const ImageViewportInternal::DisplayState& display,
    const ImageViewportInternal::RequestState& request)
{
    const ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(request, ImageViewport::PageRole::Primary);
    return display.hasReadyDisplay(request.sequenceSource.facts.present)
        && display.displayedRequest.generation == request.sequenceGeneration
        && display.displayedRequest.request.resolvedFrame.frame == activeRequest.resolvedFrame.frame
        && display.displayedRequest.request.resolvedFrame.position
        == activeRequest.resolvedFrame.position;
}

const QImage& displayedImageForRole(
    const ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? display.secondaryDisplayedImage
                                                      : display.displayedImage;
}

ImageViewportInternal::PreparedPayload primaryRenderPayload(
    const ImageViewportInternal::DisplayState& display,
    const ImageViewportInternal::RequestState& request, const ViewportRenderSnapshotInput& input)
{
    ImageViewportInternal::PreparedPayload payload = input.preparedPayload;
    if (payload.image.isNull() && display.hasReadyDisplay(request.sequenceSource.facts.present)) {
        payload.image = displayedImageForRole(display, ImageViewport::PageRole::Primary);
    }
    return payload;
}

ImageViewportInternal::PreparedPayload secondaryRenderPayload(
    const ImageViewportInternal::DisplayState& display, const ViewportRenderSnapshotInput& input,
    const ImageViewportInternal::PreparedPayload& primaryPayload)
{
    ImageViewportInternal::PreparedPayload payload = primaryPayload;
    const auto& secondaryPending
        = pendingPayloadForRole(display, ImageViewport::PageRole::Secondary);
    if (input.pendingTargetCommit && !secondaryPending.image.isNull()) {
        return secondaryPending;
    }
    payload.image = displayedImageForRole(display, ImageViewport::PageRole::Secondary);
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
}

ImageViewportStateSnapshot ViewportEngine::snapshot() const { return {}; }

ViewportEngine::CommandDiagnostics ViewportEngine::commandDiagnostics() const
{
    return { m_requestState.commandReason, m_commandRevision };
}

ViewportEngine::PresentationTargetState ViewportEngine::presentationTargetState() const
{
    return m_presentationTargetState;
}

ImageViewportInternal::DisplayState& ViewportEngine::displayState() { return m_displayState; }

const ImageViewportInternal::DisplayState& ViewportEngine::displayState() const
{
    return m_displayState;
}

ImageViewportInternal::RequestState& ViewportEngine::requestState() { return m_requestState; }

const ImageViewportInternal::RequestState& ViewportEngine::requestState() const
{
    return m_requestState;
}

ImageViewportInternal::ProviderGenerationState& ViewportEngine::providerState()
{
    return m_providerState;
}

const ImageViewportInternal::ProviderGenerationState& ViewportEngine::providerState() const
{
    return m_providerState;
}

ImageViewportInternal::ProviderGenerationState& ViewportEngine::secondaryProviderState()
{
    return m_secondaryProviderState;
}

const ImageViewportInternal::ProviderGenerationState& ViewportEngine::secondaryProviderState() const
{
    return m_secondaryProviderState;
}

const ImageViewportInternal::PresentationState& ViewportEngine::presentationState() const
{
    return m_presentationState;
}

PresentationGeometry::State ViewportEngine::geometryState(const GeometryInput& input) const
{
    return geometryState(input, m_presentationState);
}

PresentationGeometry::State ViewportEngine::geometryState(
    const GeometryInput& input, const ImageViewportInternal::PresentationState& presentation) const
{
    return {
        input.primaryPresent,
        input.itemBounds,
        input.primarySize,
        input.secondarySize,
        presentation.pageGap,
        presentation.spreadDirection,
        presentation.fitMode,
        presentation.rotationDegrees,
        presentation.mirrorHorizontally,
        presentation.mirrorVertically,
        presentation.manualZoom,
        input.devicePixelRatio > 0.0 ? input.devicePixelRatio : 1.0,
        presentation.contentPosition,
    };
}

ViewportRenderSnapshot ViewportEngine::renderSnapshot(
    const ViewportRenderSnapshotInput& input) const
{
    ViewportRenderSnapshot snapshot;
    snapshot.itemSize = input.itemSize;
    snapshot.backgroundMode = m_presentationState.backgroundMode;
    snapshot.backgroundColor = m_presentationState.backgroundColor;
    snapshot.smoothing = m_presentationState.smoothing;
    snapshot.mipmap = m_presentationState.mipmap;
    snapshot.rotationDegrees = m_presentationState.rotationDegrees;
    snapshot.mirrorHorizontally = m_presentationState.mirrorHorizontally;
    snapshot.mirrorVertically = m_presentationState.mirrorVertically;

    const ImageViewportInternal::PreparedPayload primaryPayload
        = primaryRenderPayload(m_displayState, m_requestState, input);
    snapshot.preparedPayload = primaryPayload;
    snapshot.targetRect = renderTargetRect(input.geometryState, ImageViewport::PageRole::Primary);
    snapshot.sourceRect = renderSourceRect(input.geometryState, ImageViewport::PageRole::Primary);
    appendRenderLayer(snapshot.imageLayers, ImageViewport::PageRole::Primary, primaryPayload,
        snapshot.targetRect, snapshot.sourceRect, m_presentationState, false);

    const ImageViewportInternal::PreparedPayload secondaryPayload
        = secondaryRenderPayload(m_displayState, input, primaryPayload);
    appendRenderLayer(snapshot.imageLayers, ImageViewport::PageRole::Secondary, secondaryPayload,
        renderTargetRect(input.geometryState, ImageViewport::PageRole::Secondary),
        renderSourceRect(input.geometryState, ImageViewport::PageRole::Secondary),
        m_presentationState, true);
    return snapshot;
}

FramePreparation::ProviderFrameState ViewportEngine::providerFramePreparationState(
    ImageViewport::PageRole role) const
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(*this, role);
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(m_requestState, role);
    ImageViewportInternal::PreparedPayload preparedPayload = m_displayState.pendingRenderPayload;
    if (role == ImageViewport::PageRole::Primary && !preparedPayload.identity().isValid()) {
        preparedPayload.generation = m_requestState.sequenceGeneration;
        preparedPayload.requestId = request.identity.id;
        preparedPayload.payloadId
            = preparedPayload.requestId == 0 ? 0 : m_displayState.nextPreparedPayloadId + 1;
    }
    return {
        provider.metadataReady,
        provider.timedMetadata,
        provider.logicalSize,
        provider.timingIntervals,
        request.resolvedFrame,
        preparedPayload,
    };
}

ViewportEngine::ProviderFrameEventAdmission ViewportEngine::admitProviderFrameEvent(
    ProviderEventAdmissionInput input)
{
    if (targetSpreadTerminalMatchesActiveRequest(m_requestState)) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(*this, input.role);
    if (!hasProviderSequenceForRole(m_requestState, input.role) || !provider.session
        || !activeProviderFrameTokenMatchesActiveRequest(
            *this, m_requestState, input.role, input.token)) {
        return {};
    }

    if (input.role == ImageViewport::PageRole::Secondary) {
        ImageViewportInternal::PreparedPayload& preparedPayload
            = pendingPayloadForRole(m_displayState, ImageViewport::PageRole::Primary);
        ImageViewportInternal::DisplayRequest& primaryRequest
            = activeRequestForRole(m_requestState, ImageViewport::PageRole::Primary);
        if (!preparedPayload.identity().isValid()) {
            preparedPayload.commitPending = true;
            preparedPayload.generation = m_requestState.sequenceGeneration;
            preparedPayload.requestId = primaryRequest.identity.id;
            preparedPayload.payloadId = ++m_displayState.nextPreparedPayloadId;
            if (displayedPrimaryPayloadMatchesActiveTarget(m_displayState, m_requestState)) {
                preparedPayload.image = m_displayState.displayedImage;
            }
            primaryRequest.preparedPayloadId = preparedPayload.payloadId;
        }
        provider.activeFrameToken = {};
    }

    return { true, providerFramePreparationState(input.role) };
}

ViewportEngine::ProviderMetadataEventAdmission ViewportEngine::admitProviderMetadataEvent(
    ProviderEventAdmissionInput input)
{
    if (targetSpreadTerminalMatchesActiveRequest(m_requestState)) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(*this, input.role);
    if (!hasProviderSequenceForRole(m_requestState, input.role) || !provider.session
        || !provider.activeMetadataToken.isValid() || input.token != provider.activeMetadataToken) {
        return {};
    }

    provider.activeMetadataToken = {};
    return { true };
}

void ViewportEngine::clearQueuedProviderFrameRequest(ImageViewport::PageRole role)
{
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(*this, role);
    provider.queuedFrameRequest = false;
    provider.queuedFrameGeneration = 0;
    provider.queuedFrameRequestId = 0;
    provider.queuedFrame = -1;
    provider.queuedPosition = -1;
    provider.queuedResolvedFrame = {};
    provider.queuedFrameFromPlayback = false;
    provider.queuedFrameTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
}

bool ViewportEngine::hasActiveProviderFrameToken(ImageViewport::PageRole role) const
{
    return providerGenerationStateForRole(*this, role).activeFrameToken.isValid();
}

ViewportEngine::ProviderFrameQueueResult ViewportEngine::queueProviderFrameRequest(
    ProviderFrameQueueInput input)
{
    ProviderFrameQueueResult result;

    ImageViewportInternal::TargetSpreadWaitState waitState;
    if (input.role == ImageViewport::PageRole::Secondary) {
        waitState.requiresSecondary = true;
        waitState.secondary.requestQueued = true;
    } else {
        waitState.primary.requestQueued = true;
    }
    m_requestState.status = ImageViewport::RequestStatus::Loading;
    m_requestState.reason = ImageViewportInternal::projectWaitReason(waitState);
    m_displayState.status = retainedOrEmptyDisplayStatus(m_displayState);
    m_displayState.clearPendingRenderPayload();
    m_displayState.clearRenderFailureRetainedDisplay();

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(*this, input.role);
    ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(m_requestState, input.role);
    if (provider.session && provider.activeFrameToken.isValid()) {
        result.cancelToken = provider.activeFrameToken;
    }
    provider.activeFrameToken = {};
    activeRequest.providerFrameToken = {};

    provider.queuedFrameRequest = true;
    provider.queuedFrameGeneration = m_requestState.sequenceGeneration;
    provider.queuedFrameRequestId = activeRequest.identity.id;
    provider.queuedFrame = input.frame;
    provider.queuedPosition = activeRequest.target.position;
    provider.queuedResolvedFrame = activeRequest.resolvedFrame;
    provider.queuedFrameFromPlayback
        = input.targetKind == ImageViewportInternal::ProviderRequestTargetKind::Playback;
    provider.queuedFrameTargetKind = input.targetKind;
    result.deferredFlush = true;
    return result;
}

ViewportEngine::ProviderFrameQueueFlushResult ViewportEngine::flushQueuedProviderFrameRequest(
    ImageViewport::PageRole role)
{
    ProviderFrameQueueFlushResult result;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(*this, role);
    if (!provider.queuedFrameRequest || !hasProviderSequenceForRole(m_requestState, role)
        || !provider.session) {
        clearQueuedProviderFrameRequest(role);
        return result;
    }

    const quint64 queuedGeneration = provider.queuedFrameGeneration;
    const int queuedFrame = provider.queuedFrame;
    const int queuedPosition = provider.queuedPosition;
    const ImageViewportInternal::ResolvedFrameIdentity queuedResolvedFrame
        = provider.queuedResolvedFrame;
    const quint64 queuedRequestId = provider.queuedFrameRequestId;
    const ImageViewportInternal::ProviderRequestTargetKind queuedTargetKind
        = provider.queuedFrameTargetKind;
    const ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(m_requestState, role);
    const bool stillCurrent = queuedGeneration == m_requestState.sequenceGeneration
        && queuedRequestId == activeRequest.identity.id
        && m_requestState.status == ImageViewport::RequestStatus::Loading
        && m_requestState.reason == ImageViewport::RequestReason::RequestQueued
        && activeRequest.target.frame == queuedFrame
        && activeRequest.target.position == queuedPosition
        && activeRequest.resolvedFrame.frame == queuedResolvedFrame.frame
        && activeRequest.resolvedFrame.position == queuedResolvedFrame.position
        && activeRequest.target.providerTargetKind == queuedTargetKind;
    clearQueuedProviderFrameRequest(role);
    if (!stillCurrent) {
        return result;
    }

    result.startRequest = true;
    result.frame = queuedFrame;
    result.targetKind = queuedTargetKind;
    return result;
}

ViewportEngine::PresentationTargetAssignmentResult ViewportEngine::assignPresentationTarget(
    const PresentationTargetAssignmentInput& input)
{
    if (!input.presentationTarget.isValid() || !input.transitionPolicy.isValid()) {
        return { rejectInvalidCommand(), m_presentationTargetState };
    }

    const bool clear = input.presentationTarget.isClear();
    const bool clearNoop
        = clear && m_presentationTargetState.acceptedRoleSet == ImageViewportRoleSet();
    const bool presentationTargetChanged = !clearNoop;
    PresentationTargetAssignmentResult result;
    result.command = acceptedPreservingCommandDiagnostics();
    result.presentationTargetChanged = presentationTargetChanged;
    result.clear = clear;
    result.retainPreviousDisplay = input.transitionPolicy.displayTransition()
        == PresentationTargetTransitionPolicy::DisplayTransition::RetainPrevious;
    result.releaseDisplayedState = clear || !result.retainPreviousDisplay;
    result.resetDisplayRequests = presentationTargetChanged;
    result.stopPlayback = presentationTargetChanged;
    result.closeProviderSessions = presentationTargetChanged;

    if (presentationTargetChanged) {
        const quint64 generation = nextPresentationTargetGeneration();
        m_presentationTargetState
            = presentationTargetStateFor(input.presentationTarget, generation);
    }

    result.presentationTargetState = m_presentationTargetState;
    return result;
}

ViewportEngine::CommandResult ViewportEngine::rejectInvalidCommand()
{
    return rejected(
        ImageViewport::CommandOutcome::Invalid, ImageViewport::CommandReason::InvalidRequest);
}

ViewportEngine::CommandResult ViewportEngine::rejectMalformedEnumCommand()
{
    return rejectInvalidCommand();
}

ViewportEngine::CommandResult ViewportEngine::clearFromEmpty() { return accepted(); }

ViewportEngine::CommandResult ViewportEngine::validatePresentationNoop(ImageViewport::FitMode mode)
{
    if (!fitModeValid(mode)) {
        return rejectMalformedEnumCommand();
    }
    return accepted();
}

ViewportEngine::CommandResult ViewportEngine::rejected(
    ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason)
{
    m_requestState.commandReason = reason;
    m_commandRevision = nextCommandRevision();
    return { outcome, reason, m_commandRevision, true };
}

ViewportEngine::CommandResult ViewportEngine::accepted()
{
    const bool hadDiagnostic
        = m_requestState.commandReason != ImageViewport::CommandReason::NoCommand;
    m_requestState.commandReason = ImageViewport::CommandReason::NoCommand;
    if (hadDiagnostic) {
        m_commandRevision = nextCommandRevision();
        return { ImageViewport::CommandOutcome::Accepted, m_requestState.commandReason,
            m_commandRevision, true };
    }
    return { ImageViewport::CommandOutcome::Accepted, m_requestState.commandReason,
        m_commandRevision, false };
}

ViewportEngine::CommandResult ViewportEngine::acceptedPreservingCommandDiagnostics() const
{
    return { ImageViewport::CommandOutcome::Accepted, m_requestState.commandReason,
        m_commandRevision, false };
}

RevisionToken ViewportEngine::nextCommandRevision()
{
    return ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(allocateRevisionValue());
}

quint64 ViewportEngine::allocateRevisionValue()
{
    if (m_nextRevision == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport revision token allocator exhausted");
    }
    return ++m_nextRevision;
}

void ViewportEngine::setNextRevisionValueForTest(quint64 token)
{
    m_nextRevision = token == 0 ? 0 : token - 1;
    m_commandRevision = {};
}

quint64 ViewportEngine::nextPresentationTargetGeneration()
{
    if (m_nextPresentationTargetGeneration == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport presentation-target generation allocator exhausted");
    }
    return ++m_nextPresentationTargetGeneration;
}

ViewportEngine::PresentationTargetState ViewportEngine::presentationTargetStateFor(
    const ImageViewportPresentationTarget& presentationTarget, quint64 generation) const
{
    PresentationTargetState state;
    if (presentationTarget.isClear()) {
        state.presentationTarget = ImageViewportPresentationTarget::clear();
        state.generation = generation;
        return state;
    }

    state.presentationTarget = presentationTarget;
    state.acceptedRoleSet = ImageViewportRoleSet(true, presentationTarget.secondary() != nullptr);
    state.targetRoleSet = state.acceptedRoleSet;
    state.generation = generation;
    state.primaryRoleGeneration = generation;
    state.secondaryRoleGeneration = presentationTarget.secondary() ? generation : 0;
    state.activeRole = ImageViewport::PageRole::Primary;
    state.activeRoleValid = true;
    return state;
}
