#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportenginestate_p.h"
#include "viewportengineprojection_p.h"

#include "imageviewporttoken_p.h"
#include "imageviewportproviderfacts_p.h"

#include <limits>
#include <utility>

namespace {
bool isPositiveGeometrySize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
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

}

ViewportEngine::ViewportEngine()
    : m_state(std::make_unique<ViewportEngineCanonicalState>())
{
}

ViewportEngine::~ViewportEngine() = default;

ViewportEngine::PendingPublication::PendingPublication(
    ViewportEngine* owner, ImageViewportInternal::ViewportChangeSet changes)
    : m_owner(owner)
    , m_changes(std::move(changes))
{
}

ViewportEngine::PendingPublication::PendingPublication(PendingPublication&& other) noexcept
    : m_owner(std::exchange(other.m_owner, nullptr))
    , m_changes(std::move(other.m_changes))
{
}

ViewportEngine::PendingPublication& ViewportEngine::PendingPublication::operator=(
    PendingPublication&& other) noexcept
{
    if (this != &other) {
        m_owner = std::exchange(other.m_owner, nullptr);
        m_changes = std::move(other.m_changes);
    }
    return *this;
}

ViewportEngine::CommandDiagnostics ViewportEngine::commandDiagnostics() const
{
    return { m_state->commandState.reason, m_state->commandState.revision };
}

ViewportEngine::PresentationTargetState ViewportEngine::presentationTargetState() const
{
    return m_state->requestState.presentationTarget;
}

ViewportEngineProviderStateAccess ViewportEngine::providerAccess()
{
    return { m_state->requestState.request, m_state->playbackState.playback,
        m_state->displayState.display, m_state->providerState.roles };
}

ViewportEnginePlaybackStateAccess ViewportEngine::playbackAccess()
{
    return { m_state->requestState.request, m_state->playbackState.playback,
        m_state->displayState.display, m_state->providerState.roles };
}

ViewportEngineRenderStateAccess ViewportEngine::renderAccess()
{
    return { m_state->requestState.request, m_state->playbackState.playback,
        m_state->displayState.display, m_state->providerState.roles,
        m_state->renderCoordination.nextSynchronizationAttempt, m_state->renderCoordination.lastSynchronization };
}

ViewportEnginePresentationStateAccess ViewportEngine::presentationAccess()
{
    return { m_state->requestState.request, m_state->displayState.display,
        m_state->presentationState.presentation };
}

ViewportEnginePresentationLoopingStateAccess ViewportEngine::presentationLoopingAccess()
{
    return ViewportEnginePresentationLoopingStateAccess(m_state->playbackState.playback);
}

ViewportEngineSnapshotStateAccess ViewportEngine::snapshotAccess() const
{
    return { m_state->requestState.request, m_state->playbackState.playback,
        m_state->displayState.display, m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->requestState.presentationTarget, m_state->commandState.reason,
        m_state->commandState.revision, m_state->commandState.publishedRevision,
        m_state->revisions.presentationRevision,
        m_state->revisions.snapshotRevision };
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ImageViewportInternal::DisplayState& ViewportEngine::displayState() { return m_state->displayState.display; }

const ImageViewportInternal::DisplayState& ViewportEngine::displayState() const
{
    return m_state->displayState.display;
}

ImageViewportInternal::RequestState& ViewportEngine::requestState() { return m_state->requestState.request; }

const ImageViewportInternal::RequestState& ViewportEngine::requestState() const
{
    return m_state->requestState.request;
}

ImageViewportInternal::PlaybackState& ViewportEngine::playbackState()
{
    return m_state->playbackState.playback;
}

const ImageViewportInternal::PlaybackState& ViewportEngine::playbackState() const
{
    return m_state->playbackState.playback;
}
#endif

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
const ImageViewportInternal::PresentationState& ViewportEngine::presentationState() const
{
    return m_state->presentationState.presentation;
}
#endif

ViewportEngine::PendingPublication ViewportEngine::preparePublication(
    ImageViewportInternal::ViewportChangeSet changes)
{
    return PendingPublication(this, std::move(changes));
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::publish(
    PendingPublication publication)
{
    if (publication.m_owner != this) {
        qFatal("ViewportEngine pending publication owner mismatch");
    }
    publication.m_owner = nullptr;
    auto changes = std::move(publication.m_changes);
    if (changes.requestRevision) {
        m_state->requestState.request.requestRevision = allocateRevisionValue();
    }
    if (changes.displayRevision) {
        m_state->displayState.display.revision = allocateRevisionValue();
        if (m_state->displayState.display.status == ImageViewport::DisplayStatus::Ready) {
            m_state->displayState.display.displayedPresentation = m_state->presentationState.presentation;
            m_state->displayState.display.displayedPresentationRevision = m_state->displayState.display.revision;
        }
    }
    if (changes.presentationRevision) {
        m_state->revisions.presentationRevision = allocateRevisionValue();
    }
    if (changes.commandRevision) {
        m_state->commandState.publishedRevision = changes.commandRevisionValue != 0
            ? changes.commandRevisionValue
            : allocateRevisionValue();
        m_state->commandState.revision = ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(
            m_state->commandState.publishedRevision);
    }
    if (changes.requestRevision || changes.displayRevision || changes.presentationRevision
        || changes.commandRevision || changes.requestState || changes.displayState
        || changes.geometryState || changes.playbackPhase || changes.diagnostics) {
        m_state->revisions.snapshotRevision = allocateRevisionValue();
    }
    return changes;
}

PresentationGeometry::State ViewportEngine::geometryState(const GeometryInput& input) const
{
    return geometryState(input, m_state->presentationState.presentation);
}

PresentationGeometry::State ViewportEngine::geometryState(
    const GeometryInput& input, const ImageViewportInternal::PresentationState& presentation) const
{
    return projectViewportGeometryState(input, presentation);
}

ViewportEngine::GeometryInput ViewportEngine::projectedGeometryInput(const QRectF& itemBounds,
    double devicePixelRatio, GeometryProjectionTarget target) const
{
    if (target == GeometryProjectionTarget::CurrentDisplay) {
        return projectViewportCurrentGeometry({ itemBounds, devicePixelRatio },
            { m_state->requestState.request, m_state->displayState.display });
    }
    return projectViewportPendingGeometry({ itemBounds, devicePixelRatio },
        { m_state->requestState.request, m_state->displayState.display,
            m_state->providerState.roles });
}

ViewportEngine::GeometryInput ViewportEngine::acceptedGeometryInput(
    const QRectF& itemBounds, double devicePixelRatio) const
{
    return projectViewportAcceptedGeometry({ itemBounds, devicePixelRatio },
        { m_state->requestState.request, m_state->providerState.roles });
}

ViewportRenderSnapshot ViewportEngine::renderSnapshot(
    const ViewportRenderSnapshotInput& input) const
{
    return projectViewportRenderSnapshot(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->presentationState.presentation });
}

FramePreparation::ProviderFrameState ViewportEngine::providerFramePreparationState(
    ImageViewport::PageRole role) const
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(role)].provider;
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(m_state->requestState.request, role);
    ImageViewportInternal::PreparedPayload preparedPayload = m_state->displayState.display.roles[0].pendingRenderPayload;
    if (role == ImageViewport::PageRole::Primary && !preparedPayload.identity().isValid()) {
        preparedPayload.generation = m_state->requestState.request.sequenceGeneration;
        preparedPayload.requestId = request.identity.id;
        preparedPayload.payloadId
            = preparedPayload.requestId == 0 ? 0 : m_state->displayState.display.nextPreparedPayloadId + 1;
    }
    return {
        provider.metadataReady,
        provider.timedMetadata,
        provider.logicalSize,
        provider.timingIntervals,
        request.resolvedFrame,
        preparedPayload,
        request.demandRevision,
        m_state->presentationState.presentation.exactnessPreference,
    };
}

ViewportEngine::ProviderFrameEventAdmission ViewportEngine::admitProviderFrameEvent(
    ProviderEventAdmissionInput input)
{
    if (targetSpreadTerminalMatchesActiveRequest(m_state->requestState.request)) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(input.role)].provider;
    if (!hasProviderSequenceForRole(m_state->requestState.request, input.role) || !provider.sessionActive
        || !activeProviderFrameTokenMatchesActiveRequest(
            provider, m_state->requestState.request, input.role, input.token)) {
        return {};
    }

    if (input.role == ImageViewport::PageRole::Secondary) {
        ImageViewportInternal::PreparedPayload& preparedPayload
            = pendingPayloadForRole(m_state->displayState.display, ImageViewport::PageRole::Primary);
        ImageViewportInternal::DisplayRequest& primaryRequest
            = activeRequestForRole(m_state->requestState.request, ImageViewport::PageRole::Primary);
        if (!preparedPayload.identity().isValid()) {
            preparedPayload.commitPending = true;
            preparedPayload.generation = m_state->requestState.request.sequenceGeneration;
            preparedPayload.requestId = primaryRequest.identity.id;
            preparedPayload.payloadId = ++m_state->displayState.display.nextPreparedPayloadId;
            if (displayedPrimaryPayloadMatchesActiveTarget(m_state->displayState.display, m_state->requestState.request)) {
                preparedPayload.image = m_state->displayState.display.roles[0].displayedImage;
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
    if (targetSpreadTerminalMatchesActiveRequest(m_state->requestState.request)) {
        return {};
    }

    ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(input.role)].provider;
    if (!hasProviderSequenceForRole(m_state->requestState.request, input.role) || !provider.sessionActive
        || !provider.activeMetadataToken.isValid() || input.token != provider.activeMetadataToken) {
        return {};
    }

    provider.activeMetadataToken = {};
    return { true };
}

quint64 ViewportEngine::activateProviderSession(ImageViewport::PageRole role)
{
    ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(role)].provider;
    provider.sessionActive = true;
    return ++provider.sessionSerial;
}

void ViewportEngine::retireProviderSession(ImageViewport::PageRole role)
{
    m_state->providerState.roles[roleIndex(role)].provider.sessionActive = false;
}

bool ViewportEngine::acceptsProviderSessionEvent(
    ImageViewport::PageRole role, quint64 sessionSerial, quint64 generation) const
{
    const ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(role)].provider;
    return generation != 0 && generation == m_state->requestState.request.sequenceGeneration
        && provider.sessionActive && provider.sessionSerial == sessionSerial;
}

ViewportEngine::ProviderSessionBinding ViewportEngine::providerSessionBinding(
    ImageViewport::PageRole role) const
{
    const auto& source = role == ImageViewport::PageRole::Secondary
        ? m_state->requestState.request.roles[1].source
        : m_state->requestState.request.roles[0].source;
    const auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
    const quint64 generation = role == ImageViewport::PageRole::Secondary
        ? m_state->requestState.presentationTarget.secondaryRoleGeneration
        : m_state->requestState.presentationTarget.primaryRoleGeneration;
    return { source.providerSessionFactory, source.facts.providerThreadingContract, generation,
        provider.sessionSerial, provider.sessionActive };
}

ViewportProviderRequestTokenAllocation ViewportEngine::allocateProviderRequestToken(
    ImageViewport::PageRole role)
{
    ViewportProviderRequestTokenAllocation allocation;
    ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(role)].provider;
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
    m_state->playbackState.playback.providerStartPending = false;
    m_state->playbackState.playback.stopWhenRequestReady = false;
    m_state->requestState.request.status = ImageViewport::RequestStatus::Error;
    m_state->requestState.request.reason = ImageViewport::RequestReason::ProviderFailure;
    m_state->requestState.request.errorString = QStringLiteral("provider request token exhausted");
    m_state->playbackState.playback.phase = ImageViewport::PlaybackPhase::Stopped;
    m_state->displayState.display.clearRenderFailureRetainedDisplay();
    return allocation;
}

void ViewportEngine::clearQueuedProviderFrameRequest(ImageViewport::PageRole role)
{
    ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(role)].provider;
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
    return m_state->providerState.roles[roleIndex(role)].provider.activeFrameToken.isValid();
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
    m_state->requestState.request.status = ImageViewport::RequestStatus::Loading;
    m_state->requestState.request.reason = ImageViewportInternal::projectWaitReason(waitState);
    m_state->displayState.display.status = retainedOrEmptyDisplayStatus(m_state->displayState.display);
    m_state->displayState.display.clearPendingRenderPayload();
    m_state->displayState.display.clearRenderFailureRetainedDisplay();

    ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(input.role)].provider;
    ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(m_state->requestState.request, input.role);
    if (provider.sessionActive && provider.activeFrameToken.isValid()) {
        result.cancelToken = provider.activeFrameToken;
    }
    provider.activeFrameToken = {};
    activeRequest.providerFrameToken = {};

    provider.queuedFrameRequest = true;
    provider.queuedFrameGeneration = m_state->requestState.request.sequenceGeneration;
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
        = m_state->providerState.roles[roleIndex(role)].provider;
    if (!provider.queuedFrameRequest || !hasProviderSequenceForRole(m_state->requestState.request, role)
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
        = activeRequestForRole(m_state->requestState.request, role);
    const bool stillCurrent = queuedGeneration == m_state->requestState.request.sequenceGeneration
        && queuedRequestId == activeRequest.identity.id
        && m_state->requestState.request.status == ImageViewport::RequestStatus::Loading
        && m_state->requestState.request.reason == ImageViewport::RequestReason::RequestQueued
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
    ImageViewportInternal::DisplayRequest& request = activeRequestForRole(m_state->requestState.request, role);
    request.demandRevision = ImageViewportInternal::RevisionTokenPrivateAccess::demandFromValue(
        allocateRevisionValue());

    const PresentationGeometry::State projected = geometryState(geometry);
    const ImageViewportInternal::ProviderGenerationState& provider
        = m_state->providerState.roles[roleIndex(role)].provider;
    const ImageViewportInternal::ImageSequenceSource& source
        = role == ImageViewport::PageRole::Secondary ? m_state->requestState.request.roles[1].source
                                                     : m_state->requestState.request.roles[0].source;
    const ImageViewportInternal::PreparedPayload& currentPayload
        = role == ImageViewport::PageRole::Secondary ? m_state->displayState.display.roles[1].displayedPayload
                                                     : m_state->displayState.display.roles[0].displayedPayload;

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
            m_state->requestState.request.requestRevision));
    demand.setPresentationRevision(
        ImageViewportInternal::RevisionTokenPrivateAccess::publicRevisionFromValue(
            m_state->revisions.presentationRevision != 0 ? m_state->revisions.presentationRevision
                                        : m_state->requestState.presentationTarget.generation));
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
    demand.setRotationDegrees(m_state->presentationState.presentation.rotationDegrees);
    demand.setMirrorHorizontally(m_state->presentationState.presentation.mirrorHorizontally);
    demand.setMirrorVertically(m_state->presentationState.presentation.mirrorVertically);
    demand.setQualityPreference(m_state->presentationState.presentation.qualityPreference);
    demand.setExactnessPreference(m_state->presentationState.presentation.exactnessPreference);
    demand.setMaximumPayloadBytes(ImageSequenceLimits::maximumPayloadBytesPerFrame());
    demand.setAllocationGeneration(
        ImageViewportInternal::RevisionTokenPrivateAccess::generationFromValue(
            m_state->requestState.presentationTarget.generation));
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
        return { rejectInvalidCommand(), m_state->requestState.presentationTarget };
    }

    const bool clear = input.presentationTarget.isClear();
    const bool clearNoop
        = clear && m_state->requestState.presentationTarget.acceptedRoleSet == ImageViewportRoleSet();
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
        m_state->requestState.presentationTarget
            = presentationTargetStateFor(input.presentationTarget, generation);
    }

    result.presentationTargetState = m_state->requestState.presentationTarget;
    if (!presentationTargetChanged) {
        result.schedule = playbackScheduleEffect();
        return result;
    }

    const auto oldGeometry = geometryState(input.geometry);
    const auto oldPlayback = m_state->playbackState.playback.phase;
    const QString oldError = m_state->requestState.request.errorString;
    const QString oldWarning = m_state->requestState.request.warningString;
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
    m_state->requestState.request.roles[0].source = std::move(primary);
    m_state->requestState.request.roles[0].sequence = m_state->requestState.request.roles[0].source.sequence;
    m_state->requestState.request.roles[1].source = std::move(secondary);
    m_state->requestState.request.roles[1].sequence = m_state->requestState.request.roles[1].source.sequence;
    m_state->requestState.request.roles[1].provider
        = m_state->requestState.request.roles[1].source.facts.provider;
    m_state->requestState.request.sequenceGeneration = m_state->requestState.presentationTarget.generation;
    m_state->requestState.request.clearDisplayRequests();
    m_state->playbackState.playback.resetRequestIdentity();
    m_state->displayState.display.nextPreparedPayloadId = 0;
    m_state->displayState.display.clearPendingRenderPayload();
    if (result.releaseDisplayedState) {
        m_state->displayState.display.clearDisplayedDisplay();
        m_state->displayState.display.clearRenderFailureRetainedDisplay();
    }
    m_state->requestState.request.errorString.clear();
    m_state->requestState.request.warningString.clear();
    m_state->playbackState.playback.phase = ImageViewport::PlaybackPhase::Stopped;
    m_state->playbackState.playback.stopWhenRequestReady = false;
    m_state->playbackState.playback.providerStartPending = false;
    resetProviderGeneration(m_state->providerState.roles[0].provider,
        m_state->requestState.request.roles[0].source.facts.provider
            ? m_state->requestState.request.roles[0].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {});
    resetProviderGeneration(m_state->providerState.roles[1].provider,
        m_state->requestState.request.roles[1].source.facts.provider
            ? m_state->requestState.request.roles[1].source.facts.authoredAnimationFacts
            : ImageSequenceAuthoredAnimationFacts {});

    if (clear) {
        m_state->displayState.display.clearDisplayedDisplay();
        m_state->displayState.display.clearRenderFailureRetainedDisplay();
        m_state->requestState.request.status = ImageViewport::RequestStatus::NoRequest;
        m_state->requestState.request.reason = ImageViewport::RequestReason::NoRequest;
        m_state->displayState.display.status = ImageViewport::DisplayStatus::Empty;
    } else {
        const auto secondaryTarget = initialBuiltInTarget(m_state->requestState.request.roles[1].source);
        if (m_state->requestState.request.roles[0].source.facts.provider) {
            auto& provider = m_state->providerState.roles[0].provider;
            const auto& facts = m_state->requestState.request.roles[0].source.facts;
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
            m_state->requestState.request.beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, target, true);
            m_state->playbackState.playback.position = target.position;
            initializeSecondaryRequest(m_state->requestState.request, secondaryTarget);
            m_state->requestState.request.status = ImageViewport::RequestStatus::Loading;
            m_state->requestState.request.reason = ImageViewport::RequestReason::ProviderWaiting;
            m_state->displayState.display.status = retainedOrEmptyDisplayStatus(m_state->displayState.display);
            activateProviderSession(ImageViewport::PageRole::Primary);
            result.openPrimaryProviderSession = true;
        } else if (m_state->requestState.request.roles[0].source.facts.present) {
            const auto target = initialBuiltInTarget(m_state->requestState.request.roles[0].source);
            m_state->requestState.request.beginDisplayRequest(
                ImageViewportInternal::DisplayRequestOrigin::Initial, target, true);
            m_state->playbackState.playback.position = target.position;
            initializeSecondaryRequest(m_state->requestState.request, secondaryTarget);
            stageInitialBuiltInSpread(m_state->requestState.request, m_state->displayState.display);
            ImageViewportInternal::TargetSpreadWaitState wait;
            wait.requiresSecondary = m_state->requestState.request.roles[1].sequence != nullptr;
            if (input.geometry.itemBounds.isEmpty()) {
                wait.primary.renderWaiting = true;
                if (wait.requiresSecondary && !m_state->requestState.request.roles[1].provider) {
                    wait.secondary.renderWaiting = true;
                }
            } else {
                wait.primary.uploadPending = true;
                if (wait.requiresSecondary && !m_state->requestState.request.roles[1].provider) {
                    wait.secondary.uploadPending = true;
                }
            }
            if (m_state->requestState.request.roles[1].provider) {
                wait.secondary.providerWaiting = true;
            }
            m_state->requestState.request.status = ImageViewport::RequestStatus::Loading;
            m_state->requestState.request.reason = ImageViewportInternal::projectWaitReason(wait);
            m_state->displayState.display.status = retainedOrEmptyDisplayStatus(m_state->displayState.display);
        }
        if (m_state->requestState.request.roles[1].provider) {
            m_state->requestState.request.status = ImageViewport::RequestStatus::Loading;
            m_state->requestState.request.reason = ImageViewport::RequestReason::ProviderWaiting;
            m_state->displayState.display.status = retainedOrEmptyDisplayStatus(m_state->displayState.display);
            activateProviderSession(ImageViewport::PageRole::Secondary);
            result.openSecondaryProviderSession = true;
        }
    }

    GeometryInput acceptedGeometry = input.geometry;
    acceptedGeometry.primaryPresent = m_state->requestState.request.roles[0].source.facts.present;
    acceptedGeometry.primarySize
        = ImageViewportInternal::sourceLogicalSize(m_state->requestState.request.roles[0].source);
    acceptedGeometry.secondarySize
        = ImageViewportInternal::sourceLogicalSize(m_state->requestState.request.roles[1].source);
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
        m_state->displayState.display.hasReadyDisplay(m_state->requestState.request.roles[0].source.facts.present) });
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
    result.changes.playbackPhase = oldPlayback != m_state->playbackState.playback.phase;
    result.changes.diagnostics = oldError != m_state->requestState.request.errorString
        || oldWarning != m_state->requestState.request.warningString;
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
    m_state->commandState.reason = reason;
    m_state->commandState.revision = nextCommandRevision();
    return { outcome, reason, m_state->commandState.revision, true };
}

ViewportEngine::CommandResult ViewportEngine::accepted()
{
    const bool hadDiagnostic
        = m_state->commandState.reason != ImageViewport::CommandReason::NoCommand;
    m_state->commandState.reason = ImageViewport::CommandReason::NoCommand;
    if (hadDiagnostic) {
        m_state->commandState.revision = nextCommandRevision();
        return { ImageViewport::CommandOutcome::Accepted, m_state->commandState.reason,
            m_state->commandState.revision, true };
    }
    return { ImageViewport::CommandOutcome::Accepted, m_state->commandState.reason,
        m_state->commandState.revision, false };
}

ViewportEngine::CommandResult ViewportEngine::acceptedPreservingCommandDiagnostics() const
{
    return { ImageViewport::CommandOutcome::Accepted, m_state->commandState.reason,
        m_state->commandState.revision, false };
}

RevisionToken ViewportEngine::nextCommandRevision()
{
    return ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(allocateRevisionValue());
}

quint64 ViewportEngine::allocateRevisionValue()
{
    if (m_state->revisions.nextRevision == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport revision token allocator exhausted");
    }
    return ++m_state->revisions.nextRevision;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportEngine::setNextRevisionValueForTest(quint64 token)
{
    m_state->revisions.nextRevision = token == 0 ? 0 : token - 1;
    m_state->commandState.revision = {};
    m_state->commandState.publishedRevision = 0;
}
#endif

quint64 ViewportEngine::nextPresentationTargetGeneration()
{
    if (m_state->requestState.nextPresentationTargetGeneration == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport presentation-target generation allocator exhausted");
    }
    return ++m_state->requestState.nextPresentationTargetGeneration;
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
