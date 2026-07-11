#include "viewportengine_p.h"

#include "imageviewporttoken_p.h"
#include "imageviewportproviderfacts_p.h"

#include <limits>

namespace {
bool isPositiveGeometrySize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

QSizeF imageLogicalSize(const QImage& image)
{
    return image.isNull() ? QSizeF() : image.deviceIndependentSize();
}

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
    return role == ImageViewport::PageRole::Secondary ? display.roles[1].pendingRenderPayload
                                                      : display.roles[0].pendingRenderPayload;
}

ImageViewportInternal::PreparedPayload& pendingPayloadForRole(
    ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? display.roles[1].pendingRenderPayload
                                                      : display.roles[0].pendingRenderPayload;
}

ImageViewportInternal::DisplayRequest& activeRequestForRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.roles[1].activeRequest
                                                      : request.roles[0].activeRequest;
}

const ImageViewportInternal::DisplayRequest& activeRequestForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.roles[1].activeRequest
                                                      : request.roles[0].activeRequest;
}

bool targetSpreadTerminalMatchesActiveRequest(const ImageViewportInternal::RequestState& request)
{
    return request.targetSpreadTerminal.sealed
        && request.targetSpreadTerminal.generation == request.sequenceGeneration
        && request.targetSpreadTerminal.requestId == request.roles[0].activeRequest.identity.id;
}

bool hasProviderSequenceForRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary
        ? request.roles[0].source.facts.provider
        : (request.roles[1].sequence && request.roles[1].provider);
}

ImageViewport::DisplayStatus retainedOrEmptyDisplayStatus(
    const ImageViewportInternal::DisplayState& display)
{
    const bool canRetain = (display.status == ImageViewport::DisplayStatus::Ready
                               || display.status == ImageViewport::DisplayStatus::Retained)
        && display.roles[0].displayedImageSize.isValid();
    return canRetain ? ImageViewport::DisplayStatus::Retained : ImageViewport::DisplayStatus::Empty;
}

void resetProviderGeneration(ImageViewportInternal::ProviderGenerationState& provider,
    ImageSequenceAuthoredAnimationFacts authored = {})
{
    provider.metadataReady = false;
    provider.timedMetadata = false;
    provider.timedPlaybackSupport = false;
    provider.frameSeekSupport = false;
    provider.positionSeekSupport = false;
    provider.authoredAnimationFacts = authored;
    provider.logicalSize = {};
    provider.timingIntervals = {};
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
}

ImageViewportInternal::DisplayRequestTarget initialBuiltInTarget(
    const ImageViewportInternal::ImageSequenceSource& source)
{
    if (!source.facts.present || source.facts.provider || source.facts.frameCount <= 0) {
        return {};
    }
    return { 0, source.facts.timed ? sourceFrameStartPosition(source, 0) : -1,
        ImageViewportInternal::ProviderRequestTargetKind::Unknown };
}

void initializeSecondaryRequest(ImageViewportInternal::RequestState& request,
    ImageViewportInternal::DisplayRequestTarget target)
{
    request.roles[1].activeRequest.identity = request.roles[0].activeRequest.identity;
    request.roles[1].activeRequest.target = target;
    request.roles[1].activeRequest.resolvedFrame
        = target.frame >= 0
        ? ImageViewportInternal::ResolvedFrameIdentity { target.frame, target.position }
        : ImageViewportInternal::ResolvedFrameIdentity {};
    request.roles[1].activeRequest.preparedPayloadId = request.roles[0].activeRequest.preparedPayloadId;
    if (target.frame >= 0) {
        request.roles[1].latestNonPlaybackRequest = request.roles[1].activeRequest;
    }
}

void stageInitialBuiltInSpread(ImageViewportInternal::RequestState& request,
    ImageViewportInternal::DisplayState& display)
{
    display.captureRenderFailureRetainedDisplay(request.roles[0].source.facts.present);
    display.roles[0].pendingRenderPayload.commitPending = true;
    display.beginPreparedPayloadIdentity(request.sequenceGeneration, request.roles[0].activeRequest);
    display.roles[0].pendingRenderPayload
        = FramePreparation::admitBuiltInFrame(request.roles[0].source,
              request.roles[0].activeRequest.target.frame, display.roles[0].pendingRenderPayload)
              .preparedPayload;
    if (request.roles[1].sequence && !request.roles[1].provider
        && request.roles[1].activeRequest.target.frame >= 0) {
        ImageViewportInternal::PreparedPayload payload;
        payload.commitPending = true;
        payload.generation = request.sequenceGeneration;
        payload.requestId = request.roles[0].activeRequest.identity.id;
        payload.payloadId = ++display.nextPreparedPayloadId;
        request.roles[1].activeRequest.preparedPayloadId = payload.payloadId;
        display.roles[1].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[1].source,
                  request.roles[1].activeRequest.target.frame, payload)
                  .preparedPayload;
    }
}

double projectedZoomPercent(const PresentationGeometry::State& state)
{
    QSizeF spread = PresentationGeometry::spreadSize(state);
    const int rotation = ((state.rotationDegrees % 360) + 360) % 360;
    if (rotation == 90 || rotation == 270) {
        spread.transpose();
    }
    const QRectF content = PresentationGeometry::contentRect(state);
    if (content.isEmpty() || spread.width() <= 0.0 || spread.height() <= 0.0) {
        return state.manualZoom * 100.0;
    }
    return content.width() / spread.width() * state.devicePixelRatio * 100.0;
}

bool activeProviderFrameTokenMatchesActiveRequest(
    const ImageViewportInternal::ProviderGenerationState& provider,
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role,
    ImageSequenceProviderRequestToken token)
{
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
    return display.hasReadyDisplay(request.roles[0].source.facts.present)
        && display.roles[0].displayedRequest.generation == request.sequenceGeneration
        && display.roles[0].displayedRequest.request.resolvedFrame.frame == activeRequest.resolvedFrame.frame
        && display.roles[0].displayedRequest.request.resolvedFrame.position
        == activeRequest.resolvedFrame.position;
}

const QImage& displayedImageForRole(
    const ImageViewportInternal::DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? display.roles[1].displayedImage
                                                      : display.roles[0].displayedImage;
}

ImageViewportInternal::PreparedPayload primaryRenderPayload(
    const ImageViewportInternal::DisplayState& display,
    const ImageViewportInternal::RequestState& request, const ViewportRenderSnapshotInput& input)
{
    ImageViewportInternal::PreparedPayload payload = input.preparedPayload;
    if (payload.image.isNull() && display.hasReadyDisplay(request.roles[0].source.facts.present)) {
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

ImageViewportInternal::ProviderGenerationState& ViewportEngine::providerState(
    ImageViewport::PageRole role)
{
    return m_roles[roleIndex(role)].provider;
}

const ImageViewportInternal::ProviderGenerationState& ViewportEngine::providerState(
    ImageViewport::PageRole role) const
{
    return m_roles[roleIndex(role)].provider;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
const ImageViewportInternal::PresentationState& ViewportEngine::presentationState() const
{
    return m_presentationState;
}
#endif

ImageViewportInternal::ViewportChangeSet ViewportEngine::publishChanges(
    ImageViewportInternal::ViewportChangeSet changes)
{
    if (changes.requestRevision) {
        m_requestState.requestRevision = allocateRevisionValue();
    }
    if (changes.displayRevision) {
        m_displayState.revision = allocateRevisionValue();
        if (m_displayState.status == ImageViewport::DisplayStatus::Ready) {
            m_displayState.displayedPresentation = m_presentationState;
            m_displayState.displayedPresentationRevision = m_displayState.revision;
        }
    }
    if (changes.presentationRevision) {
        m_presentationRevision = allocateRevisionValue();
    }
    if (changes.commandRevision) {
        m_requestState.commandRevision = changes.commandRevisionValue != 0
            ? changes.commandRevisionValue
            : allocateRevisionValue();
        m_commandRevision = ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(
            m_requestState.commandRevision);
    }
    if (changes.requestRevision || changes.displayRevision || changes.presentationRevision
        || changes.commandRevision || changes.requestState || changes.displayState
        || changes.geometryState || changes.playbackPhase || changes.diagnostics) {
        m_snapshotRevision = allocateRevisionValue();
    }
    return changes;
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

ViewportEngine::GeometryInput ViewportEngine::projectedGeometryInput(const QRectF& itemBounds,
    double devicePixelRatio, GeometryProjectionTarget target) const
{
    const bool sequencePresent = m_requestState.roles[0].source.facts.present;
    const bool displayReady = m_displayState.hasReadyDisplay(sequencePresent);
    QSizeF primarySize = displayReady ? m_displayState.roles[0].displayedImageSize : QSizeF {};
    QSizeF secondarySize = displayReady ? m_displayState.roles[1].displayedImageSize : QSizeF {};

    if (target == GeometryProjectionTarget::PendingRender) {
        if (m_requestState.roles[0].source.facts.provider
            && isPositiveGeometrySize(providerState(ImageViewport::PageRole::Primary).logicalSize)) {
            primarySize = providerState(ImageViewport::PageRole::Primary).logicalSize;
        } else {
            const QSizeF pending = imageLogicalSize(m_displayState.roles[0].pendingRenderPayload.image);
            if (isPositiveGeometrySize(pending)) {
                primarySize = pending;
            }
        }

        if (!m_requestState.roles[1].sequence
            || m_requestState.roles[1].activeRequest.target.frame < 0) {
            secondarySize = {};
        } else if (m_requestState.roles[1].provider
            && isPositiveGeometrySize(providerState(ImageViewport::PageRole::Secondary).logicalSize)) {
            secondarySize = providerState(ImageViewport::PageRole::Secondary).logicalSize;
        } else {
            const QSizeF pending
                = imageLogicalSize(m_displayState.roles[1].pendingRenderPayload.image);
            if (isPositiveGeometrySize(pending)) {
                secondarySize = pending;
            }
        }
    }

    return { isPositiveGeometrySize(primarySize), itemBounds, primarySize, secondarySize,
        devicePixelRatio > 0.0 ? devicePixelRatio : 1.0 };
}

ViewportEngine::GeometryInput ViewportEngine::acceptedGeometryInput(
    const QRectF& itemBounds, double devicePixelRatio) const
{
    QSizeF primarySize;
    if (m_requestState.roles[0].source.facts.provider) {
        primarySize = providerState(ImageViewport::PageRole::Primary).logicalSize;
    } else {
        primarySize = ImageViewportInternal::sourceLogicalSize(m_requestState.roles[0].source);
    }
    if (!isPositiveGeometrySize(primarySize)) {
        primarySize = {};
    }

    QSizeF secondarySize;
    if (m_requestState.roles[1].sequence) {
        if (m_requestState.roles[1].provider) {
            secondarySize = providerState(ImageViewport::PageRole::Secondary).logicalSize;
        } else if (m_requestState.roles[1].activeRequest.target.frame >= 0) {
            secondarySize
                = ImageViewportInternal::sourceLogicalSize(m_requestState.roles[1].source);
        }
    }
    if (!isPositiveGeometrySize(secondarySize)) {
        secondarySize = {};
    }

    return { isPositiveGeometrySize(primarySize), itemBounds, primarySize, secondarySize,
        devicePixelRatio > 0.0 ? devicePixelRatio : 1.0 };
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
        = providerState(role);
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(m_requestState, role);
    ImageViewportInternal::PreparedPayload preparedPayload = m_displayState.roles[0].pendingRenderPayload;
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
        request.demandRevision,
        m_presentationState.exactnessPreference,
    };
}

ViewportEngine::ProviderFrameEventAdmission ViewportEngine::admitProviderFrameEvent(
    ProviderEventAdmissionInput input)
{
    if (targetSpreadTerminalMatchesActiveRequest(m_requestState)) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = providerState(input.role);
    if (!hasProviderSequenceForRole(m_requestState, input.role) || !provider.sessionActive
        || !activeProviderFrameTokenMatchesActiveRequest(
            provider, m_requestState, input.role, input.token)) {
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
                preparedPayload.image = m_displayState.roles[0].displayedImage;
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
        = providerState(input.role);
    if (!hasProviderSequenceForRole(m_requestState, input.role) || !provider.sessionActive
        || !provider.activeMetadataToken.isValid() || input.token != provider.activeMetadataToken) {
        return {};
    }

    provider.activeMetadataToken = {};
    return { true };
}

quint64 ViewportEngine::activateProviderSession(ImageViewport::PageRole role)
{
    ImageViewportInternal::ProviderGenerationState& provider
        = providerState(role);
    provider.sessionActive = true;
    return ++provider.sessionSerial;
}

void ViewportEngine::retireProviderSession(ImageViewport::PageRole role)
{
    providerState(role).sessionActive = false;
}

bool ViewportEngine::acceptsProviderSessionEvent(
    ImageViewport::PageRole role, quint64 sessionSerial, quint64 generation) const
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerState(role);
    return generation != 0 && generation == m_requestState.sequenceGeneration
        && provider.sessionActive && provider.sessionSerial == sessionSerial;
}

ViewportEngine::ProviderSessionBinding ViewportEngine::providerSessionBinding(
    ImageViewport::PageRole role) const
{
    const auto& source = role == ImageViewport::PageRole::Secondary
        ? m_requestState.roles[1].source
        : m_requestState.roles[0].source;
    const auto& provider = providerState(role);
    const quint64 generation = role == ImageViewport::PageRole::Secondary
        ? m_presentationTargetState.secondaryRoleGeneration
        : m_presentationTargetState.primaryRoleGeneration;
    return { source.providerSessionFactory, source.facts.providerThreadingContract, generation,
        provider.sessionSerial, provider.sessionActive };
}

ViewportProviderRequestTokenAllocation ViewportEngine::allocateProviderRequestToken(
    ImageViewport::PageRole role)
{
    ViewportProviderRequestTokenAllocation allocation;
    ImageViewportInternal::ProviderGenerationState& provider
        = providerState(role);
    if (provider.nextRequestToken != std::numeric_limits<quint64>::max()) {
        allocation.token = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(
            ++provider.nextRequestToken);
        return allocation;
    }

    allocation.closeSession = provider.sessionActive;
    allocation.sessionClose.metadataToken = provider.activeMetadataToken;
    allocation.sessionClose.frameToken = provider.activeFrameToken;
    clearQueuedProviderFrameRequest(role);
    provider.sessionActive = false;
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    provider.nextRequestToken = 0;
    m_requestState.providerPlaybackStartPending = false;
    m_requestState.stopPlaybackWhenRequestReady = false;
    m_requestState.status = ImageViewport::RequestStatus::Error;
    m_requestState.reason = ImageViewport::RequestReason::ProviderFailure;
    m_requestState.errorString = QStringLiteral("provider request token exhausted");
    m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    m_displayState.clearRenderFailureRetainedDisplay();
    return allocation;
}

void ViewportEngine::clearQueuedProviderFrameRequest(ImageViewport::PageRole role)
{
    ImageViewportInternal::ProviderGenerationState& provider
        = providerState(role);
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
    return providerState(role).activeFrameToken.isValid();
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
        = providerState(input.role);
    ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(m_requestState, input.role);
    if (provider.sessionActive && provider.activeFrameToken.isValid()) {
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
        = providerState(role);
    if (!provider.queuedFrameRequest || !hasProviderSequenceForRole(m_requestState, role)
        || !provider.sessionActive) {
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

ImageSequenceProviderDisplayDemand ViewportEngine::providerDisplayDemand(
    ImageViewport::PageRole role, const GeometryInput& geometry)
{
    ImageViewportInternal::DisplayRequest& request = activeRequestForRole(m_requestState, role);
    request.demandRevision = ImageViewportInternal::RevisionTokenPrivateAccess::demandFromValue(
        allocateRevisionValue());

    const PresentationGeometry::State projected = geometryState(geometry);
    const ImageViewportInternal::ProviderGenerationState& provider
        = providerState(role);
    const ImageViewportInternal::ImageSequenceSource& source
        = role == ImageViewport::PageRole::Secondary ? m_requestState.roles[1].source
                                                     : m_requestState.roles[0].source;
    const ImageViewportInternal::PreparedPayload& currentPayload
        = role == ImageViewport::PageRole::Secondary ? m_displayState.roles[1].displayedPayload
                                                     : m_displayState.roles[0].displayedPayload;

    QSizeF logicalSize = provider.logicalSize;
    if (logicalSize.isEmpty()) {
        logicalSize = ImageViewportInternal::sourceLogicalSize(source);
    }
    QRectF visibleRect = PresentationGeometry::visiblePageRect(projected, role);
    const QRectF logicalBounds(QPointF(), logicalSize);
    if (!logicalBounds.isEmpty()) {
        visibleRect = visibleRect.intersected(logicalBounds);
    }
    QSizeF targetPixels = PresentationGeometry::pageItemRect(projected, role).size();
    if (targetPixels.isValid() && projected.devicePixelRatio > 0.0) {
        targetPixels *= projected.devicePixelRatio;
    }

    ImageSequenceProviderDisplayDemand demand;
    demand.setDemandRevision(request.demandRevision);
    demand.setRequestRevision(
        ImageViewportInternal::RevisionTokenPrivateAccess::publicRevisionFromValue(
            m_requestState.requestRevision));
    demand.setPresentationRevision(
        ImageViewportInternal::RevisionTokenPrivateAccess::publicRevisionFromValue(
            m_presentationRevision != 0 ? m_presentationRevision
                                        : m_presentationTargetState.generation));
    demand.setRole(role);
    demand.setResolvedFrame(request.resolvedFrame.frame);
    demand.setRequestedPosition(
        request.target.providerTargetKind == ImageViewportInternal::ProviderRequestTargetKind::Frame
            ? request.resolvedFrame.position
            : request.target.position);
    demand.setSourceLogicalSize(logicalSize);
    demand.setVisibleSourceRect(visibleRect);
    demand.setTargetDisplaySizePixels(targetPixels);
    demand.setEffectiveDevicePixelRatio(projected.devicePixelRatio);
    demand.setRotationDegrees(m_presentationState.rotationDegrees);
    demand.setMirrorHorizontally(m_presentationState.mirrorHorizontally);
    demand.setMirrorVertically(m_presentationState.mirrorVertically);
    demand.setQualityPreference(m_presentationState.qualityPreference);
    demand.setExactnessPreference(m_presentationState.exactnessPreference);
    demand.setMaximumPayloadBytes(ImageSequenceLimits::maximumPayloadBytesPerFrame());
    demand.setAllocationGeneration(
        ImageViewportInternal::RevisionTokenPrivateAccess::generationFromValue(
            m_presentationTargetState.generation));
    demand.setCurrentPayloadQuality(currentPayload.quality);
    demand.setCurrentPayloadExactness(currentPayload.exactness);
    demand.setCurrentPayloadRasterSize(currentPayload.payloadRasterSize);
    demand.setCurrentSourceToPayloadScale(currentPayload.sourceToPayloadScale);
    return demand;
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
    result.command = clear ? accepted() : acceptedPreservingCommandDiagnostics();
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
    if (!presentationTargetChanged) {
        result.schedule = playbackScheduleEffect();
        return result;
    }

    const auto oldGeometry = geometryState(input.geometry);
    const auto oldPlayback = m_requestState.playbackPhase;
    const QString oldError = m_requestState.errorString;
    const QString oldWarning = m_requestState.warningString;
    result.providerEffects[0] = closeProviderSession(ImageViewport::PageRole::Primary);
    result.providerEffects[1] = closeProviderSession(ImageViewport::PageRole::Secondary);

    ImageViewportInternal::ImageSequenceSource primary = input.primarySource;
    ImageViewportInternal::ImageSequenceSource secondary = input.secondarySource;
    if (!clear && !primary.sequence) {
        primary = ImageViewportInternal::factorySequenceSource(input.presentationTarget.primary());
    }
    if (!clear && !secondary.sequence) {
        secondary
            = ImageViewportInternal::factorySequenceSource(input.presentationTarget.secondary());
    }
    m_requestState.roles[0].source = std::move(primary);
    m_requestState.roles[0].sequence = m_requestState.roles[0].source.sequence;
    m_requestState.roles[1].source = std::move(secondary);
    m_requestState.roles[1].sequence = m_requestState.roles[1].source.sequence;
    m_requestState.roles[1].provider
        = m_requestState.roles[1].source.facts.provider;
    m_requestState.sequenceGeneration = m_presentationTargetState.generation;
    m_requestState.clearDisplayRequests();
    m_displayState.nextPreparedPayloadId = 0;
    m_displayState.clearPendingRenderPayload();
    if (result.releaseDisplayedState) {
        m_displayState.clearDisplayedDisplay();
        m_displayState.clearRenderFailureRetainedDisplay();
    }
    m_requestState.errorString.clear();
    m_requestState.warningString.clear();
    m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    m_requestState.stopPlaybackWhenRequestReady = false;
    m_requestState.providerPlaybackStartPending = false;
    resetProviderGeneration(m_roles[0].provider,
        m_requestState.roles[0].source.facts.provider
            ? m_requestState.roles[0].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {});
    resetProviderGeneration(m_roles[1].provider,
        m_requestState.roles[1].source.facts.provider
            ? m_requestState.roles[1].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {});

    if (clear) {
        m_displayState.clearDisplayedDisplay();
        m_displayState.clearRenderFailureRetainedDisplay();
        m_requestState.status = ImageViewport::RequestStatus::NoRequest;
        m_requestState.reason = ImageViewport::RequestReason::NoRequest;
        m_displayState.status = ImageViewport::DisplayStatus::Empty;
    } else {
        const auto secondaryTarget = initialBuiltInTarget(m_requestState.roles[1].source);
        if (m_requestState.roles[0].source.facts.provider) {
            auto& provider = m_roles[0].provider;
            const auto& facts = m_requestState.roles[0].source.facts;
            ImageViewportInternal::DisplayRequestTarget target { -1, -1,
                ImageViewportInternal::ProviderRequestTargetKind::Unknown };
            if (facts.hasCompleteProviderKnownMetadata) {
                provider.metadataReady = true;
                provider.timedMetadata = facts.providerKnownFacts.isTimedFrameList();
                provider.timedPlaybackSupport = ImageViewportInternal::providerResolvedCapability(
                    facts.providerTimedPlaybackCapability, provider.timedMetadata);
                provider.frameSeekSupport = ImageViewportInternal::providerResolvedCapability(
                    facts.providerFrameSeekCapability, true);
                provider.positionSeekSupport = ImageViewportInternal::providerResolvedCapability(
                    facts.providerPositionSeekCapability, provider.timedMetadata);
                provider.logicalSize = facts.providerKnownLogicalSize;
                provider.timingIntervals = facts.providerKnownTimingIntervals;
                target = { 0, provider.timedMetadata ? 0 : -1,
                    ImageViewportInternal::ProviderRequestTargetKind::Frame };
            }
            m_requestState.beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, target, true);
            m_requestState.playbackPosition = target.position;
            initializeSecondaryRequest(m_requestState, secondaryTarget);
            m_requestState.status = ImageViewport::RequestStatus::Loading;
            m_requestState.reason = ImageViewport::RequestReason::ProviderWaiting;
            m_displayState.status = retainedOrEmptyDisplayStatus(m_displayState);
            activateProviderSession(ImageViewport::PageRole::Primary);
            result.openPrimaryProviderSession = true;
        } else if (m_requestState.roles[0].source.facts.present) {
            const auto target = initialBuiltInTarget(m_requestState.roles[0].source);
            m_requestState.beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, target, true);
            m_requestState.playbackPosition = target.position;
            initializeSecondaryRequest(m_requestState, secondaryTarget);
            stageInitialBuiltInSpread(m_requestState, m_displayState);
            ImageViewportInternal::TargetSpreadWaitState wait;
            wait.requiresSecondary = m_requestState.roles[1].sequence != nullptr;
            if (input.geometry.itemBounds.isEmpty()) {
                wait.primary.renderWaiting = true;
                if (wait.requiresSecondary && !m_requestState.roles[1].provider) {
                    wait.secondary.renderWaiting = true;
                }
            } else {
                wait.primary.uploadPending = true;
                if (wait.requiresSecondary && !m_requestState.roles[1].provider) {
                    wait.secondary.uploadPending = true;
                }
            }
            if (m_requestState.roles[1].provider) {
                wait.secondary.providerWaiting = true;
            }
            m_requestState.status = ImageViewport::RequestStatus::Loading;
            m_requestState.reason = ImageViewportInternal::projectWaitReason(wait);
            m_displayState.status = retainedOrEmptyDisplayStatus(m_displayState);
        }
        if (m_requestState.roles[1].provider) {
            m_requestState.status = ImageViewport::RequestStatus::Loading;
            m_requestState.reason = ImageViewport::RequestReason::ProviderWaiting;
            m_displayState.status = retainedOrEmptyDisplayStatus(m_displayState);
            activateProviderSession(ImageViewport::PageRole::Secondary);
            result.openSecondaryProviderSession = true;
        }
    }

    GeometryInput acceptedGeometry = input.geometry;
    acceptedGeometry.primaryPresent = m_requestState.roles[0].source.facts.present;
    acceptedGeometry.primarySize
        = ImageViewportInternal::sourceLogicalSize(m_requestState.roles[0].source);
    acceptedGeometry.secondarySize
        = ImageViewportInternal::sourceLogicalSize(m_requestState.roles[1].source);
    const auto transition = clear ? ImageViewportInternal::ViewportChangeSet {}
                                  : applyPresentationTargetTransition({ input.transitionPolicy.zoomTransition(),
        input.transitionPolicy.contentPositionTransition(), input.transitionPolicy.rotationTransition(),
        input.transitionPolicy.mirrorTransition(),
        input.transitionPolicy.fitModeTransition()
                == PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit
            ? std::optional<ImageViewport::FitMode>(input.transitionPolicy.fitMode())
            : std::nullopt,
        input.transitionPolicy.spreadDirectionTransition()
                == PresentationTargetTransitionPolicy::SpreadDirectionTransition::SetExplicit
            ? std::optional<ImageViewport::SpreadDirection>(input.transitionPolicy.spreadDirection())
            : std::nullopt,
        input.transitionPolicy.pageGapTransition()
                == PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit
            ? std::optional<double>(input.transitionPolicy.pageGap())
            : std::nullopt,
        acceptedGeometry, PresentationGeometry::contentPosition(oldGeometry),
        projectedZoomPercent(oldGeometry),
        m_displayState.hasReadyDisplay(m_requestState.roles[0].source.facts.present) });
    armAuthoredAutoplayIfEligible();
    result.changes = transition;
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    const auto newGeometry = geometryState(acceptedGeometry);
    result.changes.geometryState = PresentationGeometry::contentRect(oldGeometry)
            != PresentationGeometry::contentRect(newGeometry)
        || PresentationGeometry::visibleImageRect(oldGeometry)
            != PresentationGeometry::visibleImageRect(newGeometry);
    result.changes.playbackPhase = oldPlayback != m_requestState.playbackPhase;
    result.changes.diagnostics = oldError != m_requestState.errorString
        || oldWarning != m_requestState.warningString;
    result.changes.scheduleUpdate = true;
    result.schedule = playbackScheduleEffect();
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
