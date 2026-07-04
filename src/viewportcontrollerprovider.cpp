#include "viewportcontrollerhelpers_p.h"

namespace {
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

void appendProviderFrameQueueResult(
    ViewportProviderFrameTransportEffect& effect, ViewportProviderFrameQueueResult queue)
{
    effect.cancelToken = queue.cancelToken;
    effect.deferredControllerEvent = queue.deferredControllerEvent;
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
}

FramePreparation::ProviderFrameState ViewportController::providerFramePreparationState() const
{
    return providerFramePreparationState(ImageViewport::PageRole::Primary);
}

FramePreparation::ProviderFrameState ViewportController::providerFramePreparationState(
    ImageViewport::PageRole role) const
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(viewportRequestState(viewport), role);
    ImageViewportInternal::PreparedPayload preparedPayload
        = viewportDisplayState(viewport).pendingRenderPayload;
    if (role == ImageViewport::PageRole::Primary && !preparedPayload.identity().isValid()) {
        preparedPayload.generation = viewportRequestState(viewport).sequenceGeneration;
        preparedPayload.requestId = request.identity.id;
        preparedPayload.payloadId = preparedPayload.requestId == 0
            ? 0
            : viewportDisplayState(viewport).nextPreparedPayloadId + 1;
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

ViewportProviderFrameEventAcceptance ViewportController::acceptProviderFrameEvent(
    ImageViewport::PageRole role, ViewportProviderFrameEvent event)
{
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!hasProviderSequenceForRole(viewport, role) || !provider.session
        || !activeProviderFrameTokenMatchesActiveRequest(state, viewport, role, event.token)) {
        return {};
    }

    if (role == ImageViewport::PageRole::Secondary) {
        ImageViewportInternal::PreparedPayload& preparedPayload
            = viewportDisplayState(viewport).pendingRenderPayload;
        if (!preparedPayload.identity().isValid()) {
            preparedPayload.commitPending = true;
            preparedPayload.generation = viewportRequestState(viewport).sequenceGeneration;
            preparedPayload.requestId = viewportRequestState(viewport).activeRequest.identity.id;
            preparedPayload.payloadId = ++viewportDisplayState(viewport).nextPreparedPayloadId;
            if (displayedPrimaryPayloadMatchesActiveTarget(viewport)) {
                preparedPayload.image = viewportDisplayState(viewport).displayedImage;
            }
            viewportRequestState(viewport).activeRequest.preparedPayloadId
                = preparedPayload.payloadId;
        }
        provider.activeFrameToken = {};
    }

    return { true, providerFramePreparationState(role) };
}

ViewportProviderFrameEventAcceptance ViewportController::acceptProviderFrameEvent(
    ViewportProviderFrameEvent event)
{
    return acceptProviderFrameEvent(ImageViewport::PageRole::Primary, event);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ImageViewport::PageRole role, ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const ViewportProviderFrameEventAcceptance frameEvent = acceptProviderFrameEvent(role, event);
    if (!frameEvent.accepted) {
        return {};
    }

    const FramePreparation::ProviderFrameAdmissionResult admission
        = FramePreparation::admitProviderFrame(frame, metadata, frameEvent.preparationState);
    return handleProviderFrameAdmission(role, admission);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    return handleProviderFrameEvent(ImageViewport::PageRole::Primary, event, frame, metadata);
}

ViewportProviderMetadataEventAcceptance ViewportController::acceptProviderMetadataEvent(
    ViewportProviderMetadataEvent event)
{
    return acceptProviderMetadataEvent(ImageViewport::PageRole::Primary, event);
}

ViewportProviderMetadataEventAcceptance ViewportController::acceptProviderMetadataEvent(
    ImageViewport::PageRole role, ViewportProviderMetadataEvent event)
{
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!hasProviderSequenceForRole(viewport, role) || !provider.session
        || !provider.activeMetadataToken.isValid()
        || event.token != provider.activeMetadataToken) {
        return {};
    }

    provider.activeMetadataToken = {};
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
    clearQueuedProviderFrameRequest(state, role);
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    recordTargetSpreadTerminal(role, ImageViewport::RequestStatus::Error,
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
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }

    ViewportProviderSessionOpenResult result;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (provider.metadataReady) {
        discardPendingRenderCommit();
        appendProviderFrameStartResult(result.providerFrameTransport,
            startProviderFrameRequest(role,
                { activeRequestForRole(viewportRequestState(viewport), role).target }));
        return result;
    }

    appendProviderMetadataStartResult(
        result.providerMetadataTransport, startProviderMetadataRequest(role));
    return result;
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
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }

    const auto generationTerminalResult = [this, role](
                                              ImageViewportInternal::ViewportChangeSet changes) {
        ViewportProviderMetadataAdmissionResult result;
        result.changes = changes;
        result.providerFrameTransport.closeSession
            = providerGenerationStateForRole(state, role).session != nullptr;
        result.providerFrameTransport.sessionClose = handleProviderSessionClose(role);
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
    const ImageSequenceProviderCapabilitySupport frameSeekCapability = secondary
        ? viewport.secondaryProviderFrameSeekCapability()
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
        return generationTerminalResult(handleProviderMetadataContradiction(
            role, { QStringLiteral("provider metadata contradicts construction-time facts") }));
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

ViewportProviderMetadataReadyResult ViewportController::handleProviderMetadataReadyEvent(
    ImageViewport::PageRole role, const ViewportProviderMetadataReadyEvent& event)
{
    const ViewportProviderMetadataEventAcceptance metadataEvent
        = acceptProviderMetadataEvent(role, { event.token });
    if (!metadataEvent.accepted) {
        return {};
    }

    ViewportProviderMetadataReadyResult result;
    const ViewportProviderMetadataAdmissionResult admission
        = handleProviderMetadataAdmission(role, event.metadata);
    if (!admission.accepted) {
        result.changes = admission.changes;
        result.providerFrameTransport = admission.providerFrameTransport;
        return result;
    }

    mergeChanges(result.changes, handleProviderAcceptedMetadataFacts(role, admission.facts));
    const ViewportProviderMetadataTargetPolicyResult targetPolicy
        = handleProviderMetadataTargetPolicy(role, admission.facts);
    mergeChanges(result.changes, targetPolicy.changes);
    result.providerFrameTransport = targetPolicy.providerFrameTransport;
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameAdmission(
    const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    return handleProviderFrameAdmission(ImageViewport::PageRole::Primary, admission);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameAdmission(
    ImageViewport::PageRole role, const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!admission.accepted()) {
        clearQueuedProviderFrameRequest(state, role);
        provider.activeFrameToken = {};
        recordTargetSpreadTerminal(role, admission.status,
            admission.reason, ImageViewportInternal::FailureScope::DisplayRequest,
            admission.diagnostic, changes);
        setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
        return changes;
    }

    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    provider.activeFrameToken = {};
    const RequestStatusSnapshot oldRequestStatus = requestStatusSnapshot(viewport);
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    if (role == ImageViewport::PageRole::Secondary) {
        pendingPayloadForRole(viewportDisplayState(viewport), ImageViewport::PageRole::Secondary)
            = admission.preparedPayload;
        const bool primaryPayloadReady
            = viewportDisplayState(viewport).pendingRenderPayload.commitPending
            && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull();
        ImageViewportInternal::TargetSpreadWaitState waitState;
        waitState.requiresSecondary = true;
        if (primaryPayloadReady && viewport.itemBounds().isEmpty()) {
            waitState.primary.renderWaiting = true;
            waitState.secondary.renderWaiting = true;
        } else if (primaryPayloadReady) {
            waitState.primary.uploadPending = true;
            waitState.secondary.uploadPending = true;
        } else {
            waitState.primary.providerWaiting = true;
            waitState.secondary.uploadPending = true;
        }
        publishLoadingWaitState(waitState);
        viewportDisplayState(viewport).status
            = viewportDisplayState(viewport).displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
    } else {
        publishAcceptedTargetState(admission.preparedPayload);
        if (hasSecondaryProviderSequence(viewport)
            && viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull()) {
            ImageViewportInternal::TargetSpreadWaitState waitState;
            waitState.requiresSecondary = true;
            if (viewport.itemBounds().isEmpty()) {
                waitState.primary.renderWaiting = true;
            } else {
                waitState.primary.uploadPending = true;
            }
            waitState.secondary.providerWaiting = true;
            publishLoadingWaitState(waitState);
            viewportDisplayState(viewport).status
                = viewportDisplayState(viewport).displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
        }
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready
            && !viewportDisplayState(viewport).pendingRenderPayload.commitPending) {
            setPlaybackPhase(changes, viewportRequestState(viewport).stopPlaybackWhenRequestReady
                    ? ImageViewport::PlaybackPhase::Stopped
                    : ImageViewport::PlaybackPhase::Playing);
            viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        }
    }
    changes.requestRevision = requestStatusChanged(viewport, oldRequestStatus);
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
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!hasProviderSequenceForRole(viewport, role) || !provider.session) {
        return {};
    }

    if (activeProviderFrameTokenMatchesActiveRequest(state, viewport, role, event.token)) {
        return { handleProviderFrameTerminalResult(role, frameTerminalResultFor(event)), {} };
    }

    if (provider.metadataReady || !provider.activeMetadataToken.isValid()
        || event.token != provider.activeMetadataToken) {
        return {};
    }

    ViewportProviderTerminalEventResult result;
    result.changes = handleProviderMetadataTerminalResult(role, metadataTerminalResultFor(event));
    result.providerFrameTransport.closeSession = true;
    result.providerFrameTransport.sessionClose = handleProviderSessionClose(role);
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
        false,
    };

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!hasProviderSequenceForRole(viewport, role)) {
        return {};
    }
    if (activeProviderFrameTokenMatchesActiveRequest(state, viewport, role, event.token)) {
        return { handleProviderFrameTerminalResult(role, frameTerminalResultFor(terminalEvent)),
            closeProviderSession(role) };
    }
    if (provider.metadataReady || !provider.activeMetadataToken.isValid()
        || event.token != provider.activeMetadataToken) {
        return {};
    }
    return { handleProviderMetadataTerminalResult(role, metadataTerminalResultFor(terminalEvent)),
        closeProviderSession(role) };
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameTerminalResult(
    ImageViewport::PageRole role, const ViewportProviderFrameTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    clearQueuedProviderFrameRequest(state, role);
    providerGenerationStateForRole(state, role).activeFrameToken = {};
    recordTargetSpreadTerminal(role, result.status, result.reason,
        ImageViewportInternal::FailureScope::DisplayRequest,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic), changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameTerminalResult(
    const ViewportProviderFrameTerminalResult& result)
{
    return handleProviderFrameTerminalResult(ImageViewport::PageRole::Primary, result);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTerminalResult(
    ImageViewport::PageRole role, const ViewportProviderMetadataTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    providerGenerationStateForRole(state, role).activeMetadataToken = {};
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    recordTargetSpreadTerminal(role, result.status, result.reason,
        ImageViewportInternal::FailureScope::Generation,
        FramePreparation::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic), changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTerminalResult(
    const ViewportProviderMetadataTerminalResult& result)
{
    return handleProviderMetadataTerminalResult(ImageViewport::PageRole::Primary, result);
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
    recordTargetSpreadTerminal(role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        ImageViewportInternal::FailureScope::Generation, contradiction.diagnostic, changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
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
    recordTargetSpreadTerminal(role, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::PayloadRejection,
        ImageViewportInternal::FailureScope::Generation, rejection.diagnostic, changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
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
    recordTargetSpreadTerminal(role, rejection.status, rejection.reason,
        ImageViewportInternal::FailureScope::DisplayRequest, {}, changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    changes.diagnostics = changes.diagnostics || diagnosticsValueChanged;
    return changes;
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    return handleProviderMetadataTargetPolicy(ImageViewport::PageRole::Primary, facts);
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(viewportRequestState(viewport), role);
    const int providerFrameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;

    if (role == ImageViewport::PageRole::Primary) {
        const bool selectedFromPlaybackStart
            = viewportRequestState(viewport).providerPlaybackStartPending
            && request.target.providerTargetKind
            == ImageViewportInternal::ProviderRequestTargetKind::Playback;
        const bool selectedFromPosition = request.target.providerTargetKind
            == ImageViewportInternal::ProviderRequestTargetKind::Position;
        ImageViewportInternal::ProviderRequestTargetKind requestTargetKind = selectedFromPlaybackStart
            ? ImageViewportInternal::ProviderRequestTargetKind::Playback
            : (selectedFromPosition ? ImageViewportInternal::ProviderRequestTargetKind::Position
                                    : ImageViewportInternal::ProviderRequestTargetKind::Frame);
        int selectedFrame = request.target.frame >= 0 ? request.target.frame : 0;
        if (selectedFromPlaybackStart
            && (!facts.timedMetadata || !provider.timedPlaybackSupport)) {
            return { handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, true }) };
        }
        if (selectedFromPosition) {
            if (!facts.timedMetadata || !provider.positionSeekSupport) {
                return { handleProviderMetadataTargetRejection(role,
                    { ImageViewport::RequestStatus::Unsupported,
                        ImageViewport::RequestReason::UnsupportedRequest, -1, false, false,
                        false }) };
            }
            selectedFrame = facts.timingIntervals.frameIndexForPosition(request.target.position);
        }
        if (selectedFrame < 0 || selectedFrame >= providerFrameCount) {
            return { handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, selectedFrame, true,
                    selectedFromPosition, false }) };
        }

        return handleProviderMetadataTargetSelection(
            { requestTargetKind, selectedFrame, selectedFromPosition, facts.timedMetadata });
    }

    ViewportProviderMetadataTargetPolicyResult result;
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
        if (!facts.timedMetadata || !provider.timedPlaybackSupport) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false });
            return result;
        }
        int selectedFrame = request.target.frame;
        if (selectedFrame < 0) {
            selectedFrame = 0;
        }
        if (selectedFrame >= providerFrameCount) {
            result.changes = handleProviderMetadataTargetRejection(role,
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
        if (request.target.frame < 0 || request.target.frame >= providerFrameCount) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest, request.target.frame, false,
                    false, false });
            return result;
        }
        target = {
            request.target.frame,
            facts.timedMetadata ? facts.timingIntervals.frameStartPosition(request.target.frame) : -1,
            ImageViewportInternal::ProviderRequestTargetKind::Frame,
        };
    } else if (request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Position) {
        if (!facts.timedMetadata || !provider.positionSeekSupport) {
            result.changes = handleProviderMetadataTargetRejection(role,
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false });
            return result;
        }
        const int selectedFrame = facts.timingIntervals.frameIndexForPosition(request.target.position);
        if (selectedFrame < 0) {
            result.changes = handleProviderMetadataTargetRejection(role,
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

    const ViewportProviderFrameDispatchResult dispatch
        = dispatchProviderFrameRequest(role, { target });
    result.providerFrameTransport = dispatch.transport;
    result.changes.requestRevision = true;
    result.changes.requestState = true;
    if (!dispatch.accepted) {
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
        && isUnknownMetadataInitialRequest(viewportRequestState(viewport).secondaryActiveRequest);
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
    beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::MetadataBoundSelection,
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
    publishProviderFrameLoadingState();

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
    return handleProviderAcceptedMetadataFacts(ImageViewport::PageRole::Primary, facts);
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderAcceptedMetadataFacts(
    ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts)
{
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    provider.metadataReady = true;
    provider.timedMetadata = facts.timedMetadata;
    provider.timedPlaybackSupport = facts.timedPlaybackSupport;
    provider.frameSeekSupport = facts.frameSeekSupport;
    provider.positionSeekSupport = facts.positionSeekSupport;
    provider.logicalSize = facts.logicalSize;
    provider.timingIntervals = facts.timingIntervals;
    provider.authoredAnimationFacts = facts.authoredAnimationFacts;

    ImageViewportInternal::ViewportChangeSet changes;
    if (role == ImageViewport::PageRole::Secondary) {
        changes.requestRevision = true;
        changes.requestState = true;
    }
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
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return {};
    }
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!hasProviderSequenceForRole(viewport, role) || !provider.session) {
        return {};
    }
    if (event.progress
        && (!std::isfinite(event.progressValue) || event.progressValue < 0.0
            || event.progressValue > 1.0)) {
        return {};
    }

    const bool activeMetadataToken = !provider.metadataReady
        && provider.activeMetadataToken.isValid()
        && event.token == provider.activeMetadataToken;
    const bool activeFrameToken
        = activeProviderFrameTokenMatchesActiveRequest(state, viewport, role, event.token);
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
    const bool sealed = targetSpreadTerminalSealedForActiveRequest();
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!hasProviderSequenceForRole(viewport, role) || !provider.session) {
        return {};
    }

    const bool activeMetadataToken = !provider.metadataReady
        && provider.activeMetadataToken.isValid()
        && event.token == provider.activeMetadataToken;
    const bool activeFrameToken
        = activeProviderFrameTokenMatchesActiveRequest(state, viewport, role, event.token);
    if (!activeMetadataToken && !activeFrameToken) {
        return {};
    }

    if (sealed) {
        return {};
    }

    if (activeMetadataToken || !provider.metadataReady || !provider.timedMetadata
        || !activeProviderFrameRequestIsPlayback(state, viewport, role)) {
        ViewportProviderEndOfSequenceResult result;
        result.changes = handleProviderEndOfSequenceProtocolViolation(role,
            { activeMetadataToken, activeFrameToken });
        result.providerFrameTransport.closeSession = provider.session != nullptr;
        result.providerFrameTransport.sessionClose = handleProviderSessionClose(role);
        return result;
    }

    return handleProviderPlaybackEndOfSequence(role);
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderEndOfSequenceProtocolViolation(
    ViewportProviderEndOfSequenceProtocolViolation violation)
{
    return handleProviderEndOfSequenceProtocolViolation(
        ImageViewport::PageRole::Primary, violation);
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderEndOfSequenceProtocolViolation(
    ImageViewport::PageRole role, ViewportProviderEndOfSequenceProtocolViolation violation)
{
    ImageViewportInternal::ViewportChangeSet changes;
    clearQueuedProviderFrameRequest(state, role);
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (violation.activeMetadataToken) {
        provider.activeMetadataToken = {};
    }
    if (violation.activeFrameToken) {
        provider.activeFrameToken = {};
    }
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    recordTargetSpreadTerminal(role,
        ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::PayloadRejection,
        violation.activeMetadataToken ? ImageViewportInternal::FailureScope::Generation
                                      : ImageViewportInternal::FailureScope::DisplayRequest,
        QStringLiteral("provider protocol violation"), changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);
    return changes;
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderPlaybackEndOfSequence()
{
    return handleProviderPlaybackEndOfSequence(ImageViewport::PageRole::Primary);
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderPlaybackEndOfSequence(
    ImageViewport::PageRole role)
{
    ViewportProviderEndOfSequenceResult result;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    provider.activeFrameToken = {};
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();

    int selectedFrame = 0;
    int selectedPosition = 0;
    if (viewportRequestState(viewport).looping) {
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackPosition = 0;
    } else {
        selectedFrame = provider.timingIntervals.frameCount() - 1;
        selectedPosition = provider.timingIntervals.frameStartPosition(selectedFrame);
        viewportRequestState(viewport).playbackPosition = provider.timingIntervals.totalDuration();
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = true;
    }

    const ImageViewportInternal::DisplayRequestTarget providerTarget {
        selectedFrame,
        selectedPosition,
        ImageViewportInternal::ProviderRequestTargetKind::Playback,
    };
    if (role == ImageViewport::PageRole::Secondary) {
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback, primaryRequest.target,
            primaryRequest.resolvedFrame, false);
        setSecondaryActiveRequest(providerTarget, { selectedFrame, selectedPosition }, false);
    } else {
        viewportRequestState(viewport).activeRequest.target = providerTarget;
        viewportRequestState(viewport).activeRequest.resolvedFrame
            = { selectedFrame, selectedPosition };
    }

    if (role == ImageViewport::PageRole::Primary && !viewportRequestState(viewport).looping
        && viewport.hasReadyDisplay()
        && viewportDisplayState(viewport).displayedRequest.generation
            == viewportRequestState(viewport).sequenceGeneration
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.frame
            == selectedFrame
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.position
            == selectedPosition) {
        publishReadyDisplayState();
        setPlaybackPhase(result.changes, ImageViewport::PlaybackPhase::Stopped);
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        result.changes.requestRevision = true;
        result.changes.displayRevision = true;
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    publishProviderFrameLoadingState();
    const ViewportProviderFrameDispatchResult dispatch
        = dispatchProviderFrameRequest(role, { providerTarget });
    result.providerFrameTransport = dispatch.transport;
    if (!dispatch.accepted) {
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = true;
        return result;
    }
    setPlaybackPhase(result.changes, ImageViewport::PlaybackPhase::Waiting);
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
    ViewportProviderFrameTransportEffect effect;
    effect.closeSession = providerGenerationStateForRole(state, role).session != nullptr;
    effect.sessionClose = handleProviderSessionClose(role);
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
    clearQueuedProviderFrameRequest(state, role);
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
        publishProviderTokenExhaustion(viewport, state, role);
        return result;
    }

    result.sendCommand = provider.session != nullptr;
    result.token = provider.activeMetadataToken;
    return result;
}

ViewportProviderFrameRequestStartResult ViewportController::startProviderFrameRequest(
    ImageViewport::PageRole role, ViewportProviderFrameRequestStart request)
{
    ViewportProviderFrameRequestStartResult result;
    clearQueuedProviderFrameRequest(state, role);
    ImageViewportInternal::TargetSpreadWaitState waitState;
    if (role == ImageViewport::PageRole::Secondary) {
        waitState.requiresSecondary = true;
        waitState.secondary.providerWaiting = true;
    } else {
        waitState.primary.providerWaiting = true;
    }
    publishLoadingWaitState(waitState);

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    const ViewportProviderRequestTokenAllocation allocation = allocateProviderRequestToken(role);
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    provider.activeFrameToken = allocation.token;
    if (!provider.activeFrameToken.isValid()) {
        publishProviderTokenExhaustion(viewport, state, role);
        return result;
    }

    if (role == ImageViewport::PageRole::Secondary) {
        const int resolvedPosition = provider.timedMetadata
            ? provider.timingIntervals.frameStartPosition(request.target.frame)
            : -1;
        setSecondaryActiveRequest(request.target,
            { request.target.frame, resolvedPosition },
            request.target.providerTargetKind
                != ImageViewportInternal::ProviderRequestTargetKind::Playback);
    }

    ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(viewportRequestState(viewport), role);
    activeRequest.providerFrameToken = provider.activeFrameToken;
    result.accepted = true;
    result.sendCommand = provider.session != nullptr;
    result.command.token = provider.activeFrameToken;
    result.command.frame = activeRequest.resolvedFrame.frame;
    result.command.position = activeRequest.target.position;
    result.command.targetKind = activeRequest.target.providerTargetKind;
    return result;
}

ViewportProviderFrameQueueResult ViewportController::queueProviderFrameRequest(
    ViewportProviderFrameQueueRequest request)
{
    return queueProviderFrameRequest(ImageViewport::PageRole::Primary, request);
}

ViewportProviderFrameQueueResult ViewportController::queueProviderFrameRequest(
    ImageViewport::PageRole role, ViewportProviderFrameQueueRequest request)
{
    ViewportProviderFrameQueueResult result;
    ImageViewportInternal::TargetSpreadWaitState waitState;
    if (role == ImageViewport::PageRole::Secondary) {
        waitState.requiresSecondary = true;
        waitState.secondary.requestQueued = true;
    } else {
        waitState.primary.requestQueued = true;
    }
    publishLoadingWaitState(waitState);
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    discardPendingRenderCommit();

    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(viewportRequestState(viewport), role);
    if (provider.session && provider.activeFrameToken.isValid()) {
        result.cancelToken = provider.activeFrameToken;
    }
    provider.activeFrameToken = {};
    activeRequest.providerFrameToken = {};

    provider.queuedFrameRequest = true;
    provider.queuedFrameGeneration = viewportRequestState(viewport).sequenceGeneration;
    provider.queuedFrameRequestId = activeRequest.identity.id;
    provider.queuedFrame = request.frame;
    provider.queuedPosition = activeRequest.target.position;
    provider.queuedResolvedFrame = activeRequest.resolvedFrame;
    provider.queuedFrameFromPlayback
        = request.targetKind == ImageViewportInternal::ProviderRequestTargetKind::Playback;
    provider.queuedFrameTargetKind = request.targetKind;
    result.deferredControllerEvent
        = ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest;
    return result;
}

ViewportProviderFrameQueueFlush ViewportController::flushQueuedProviderFrameRequest()
{
    return flushQueuedProviderFrameRequest(ImageViewport::PageRole::Primary);
}

ViewportProviderFrameQueueFlush ViewportController::flushQueuedProviderFrameRequest(
    ImageViewport::PageRole role)
{
    ViewportProviderFrameQueueFlush flush;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    if (!provider.queuedFrameRequest || !hasProviderSequenceForRole(viewport, role)
        || !provider.session) {
        clearQueuedProviderFrameRequest(provider);
        return flush;
    }

    const int queuedFrame = provider.queuedFrame;
    const int queuedPosition = provider.queuedPosition;
    const ImageViewportInternal::ResolvedFrameIdentity queuedResolvedFrame
        = provider.queuedResolvedFrame;
    const quint64 queuedRequestId = provider.queuedFrameRequestId;
    const ImageViewportInternal::ProviderRequestTargetKind queuedTargetKind
        = provider.queuedFrameTargetKind;
    const ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(viewportRequestState(viewport), role);
    const bool stillCurrent = provider.queuedFrameGeneration
            == viewportRequestState(viewport).sequenceGeneration
        && queuedRequestId == activeRequest.identity.id
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::RequestQueued
        && activeRequest.target.frame == queuedFrame
        && activeRequest.target.position == queuedPosition
        && activeRequest.resolvedFrame.frame == queuedResolvedFrame.frame
        && activeRequest.resolvedFrame.position == queuedResolvedFrame.position
        && activeRequest.target.providerTargetKind == queuedTargetKind;
    clearQueuedProviderFrameRequest(provider);
    if (!stillCurrent) {
        return flush;
    }

    flush.startRequest = true;
    flush.frame = queuedFrame;
    flush.targetKind = queuedTargetKind;
    return flush;
}

ViewportProviderFrameQueueFlushResult ViewportController::flushQueuedProviderFrameRequestEvent()
{
    return flushQueuedProviderFrameRequestEvent(ImageViewport::PageRole::Primary);
}

ViewportProviderFrameQueueFlushResult ViewportController::flushQueuedProviderFrameRequestEvent(
    ImageViewport::PageRole role)
{
    ViewportProviderFrameQueueFlushResult result;
    const ViewportProviderFrameQueueFlush flush = flushQueuedProviderFrameRequest(role);
    if (!flush.startRequest) {
        return result;
    }

    const ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(viewportRequestState(viewport), role);
    const DisplayRequestTarget target {
        flush.frame,
        activeRequest.target.position,
        flush.targetKind,
    };
    const ViewportProviderFrameRequestStartResult start
        = startProviderFrameRequest(role, { target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    result.changes.requestRevision = true;
    result.changes.requestState = true;
    if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::ProviderFailure) {
        result.changes.diagnostics = true;
    }
    return result;
}

ViewportProviderFrameRequestStartResult ViewportController::startProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    return startProviderFrameRequest(ImageViewport::PageRole::Primary, request);
}

ViewportProviderFrameDispatchResult ViewportController::dispatchProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    return dispatchProviderFrameRequest(ImageViewport::PageRole::Primary, request);
}

ViewportProviderFrameDispatchResult ViewportController::dispatchProviderFrameRequest(
    ImageViewport::PageRole role, ViewportProviderFrameRequestStart request)
{
    ViewportProviderFrameDispatchResult result;
    if (providerGenerationStateForRole(state, role).activeFrameToken.isValid()) {
        result.accepted = true;
        appendProviderFrameQueueResult(result.transport,
            queueProviderFrameRequest(
                role, { request.target.frame, request.target.providerTargetKind }));
        return result;
    }

    const ViewportProviderFrameRequestStartResult start = startProviderFrameRequest(role, request);
    result.accepted = start.accepted;
    appendProviderFrameStartResult(result.transport, start);
    return result;
}
