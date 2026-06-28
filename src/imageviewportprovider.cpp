#include "framepreparation_p.h"
#include "imageviewport_p.h"

#include <QtCore/QMetaObject>

#include <memory>

using namespace ImageViewportInternal;

namespace {
bool activeProviderFrameTokenMatchesActiveRequest(
    const ImageViewportPrivate& viewport, ImageSequenceProviderRequestToken token)
{
    return viewport.m_activeProviderFrameToken.isValid()
        && token == viewport.m_activeProviderFrameToken
        && token == viewport.request.activeRequest.providerFrameToken
        && viewport.m_activeProviderFrameRequestId == viewport.request.activeRequest.identity.id;
}

void applyProviderTerminalEvent(
    ImageViewportPrivate& viewport, const ViewportProviderTerminalEvent& event)
{
    const ViewportProviderTerminalEventResult result
        = viewport.controller.handleProviderTerminalEvent(event);
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
    if (result.closeSession) {
        viewport.closeProviderSession();
    }
}

void applyProviderMetadataContradiction(ImageViewportPrivate& viewport, const QString& diagnostic)
{
    const auto changes = viewport.controller.handleProviderMetadataContradiction({ diagnostic });
    viewport.applyControllerChanges(changes);
    if (changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
}

void applyProviderMetadataTargetRejection(
    ImageViewportPrivate& viewport, ViewportProviderMetadataTargetRejection rejection)
{
    const auto changes = viewport.controller.handleProviderMetadataTargetRejection(rejection);
    viewport.applyControllerChanges(changes);
    if (changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
}

void applyProviderMetadataTargetSelection(
    ImageViewportPrivate& viewport, ViewportProviderMetadataTargetSelection selection)
{
    const auto changes = viewport.controller.handleProviderMetadataTargetSelection(selection);
    viewport.applyControllerChanges(changes);
    if (changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
}

void applyProviderAcceptedMetadataFacts(
    ImageViewportPrivate& viewport, const ViewportProviderAcceptedMetadataFacts& facts)
{
    viewport.applyControllerChanges(viewport.controller.handleProviderAcceptedMetadataFacts(facts));
}
}

void ImageViewportPrivate::closeProviderSession()
{
    const ViewportProviderSessionClose sessionClose = controller.handleProviderSessionClose();
    providerBridge.closeSession(sessionClose.metadataToken, sessionClose.frameToken);
}

bool ImageViewportPrivate::openProviderSession()
{
    if (!providerBridge.openSession()) {
        return false;
    }

    if (m_providerMetadataReady) {
        discardPendingRenderCommit();
        startProviderFrameRequest(
            request.activeRequest.target.frame, request.activeRequest.target.providerTargetKind);
    } else {
        startProviderMetadataRequest();
    }
    return true;
}

QObject* ImageViewportPrivate::providerCallbackTarget() const
{
    return q;
}

quint64 ImageViewportPrivate::installProviderSession(ImageSequenceProviderSession* session)
{
    m_providerSession = session;
    if (!m_providerSession) {
        return 0;
    }

    ++m_providerSessionSerial;
    return m_providerSessionSerial;
}

ImageSequenceProviderSession* ImageViewportPrivate::takeProviderSession()
{
    ImageSequenceProviderSession* session = m_providerSession;
    m_providerSession.clear();
    return session;
}

ImageSequenceProviderSession* ImageViewportPrivate::currentProviderSession() const
{
    return m_providerSession;
}

bool ImageViewportPrivate::acceptsProviderSessionResult(quint64 sessionSerial) const
{
    return m_providerSession && m_providerSessionSerial == sessionSerial;
}

void ImageViewportPrivate::handleProviderEvent(const ViewportProviderEvent& event)
{
    switch (event.kind) {
    case ViewportProviderEvent::Kind::MetadataReady:
        handleProviderMetadataReady(event.token, event.metadata);
        break;
    case ViewportProviderEvent::Kind::ImageFrameReady:
        handleProviderFrameReady(event.token, event.imageFrame);
        break;
    case ViewportProviderEvent::Kind::ImageFrameWithMetadataReady:
        handleProviderFrameReadyWithMetadata(event.token, event.imageFrame, event.frameMetadata);
        break;
    case ViewportProviderEvent::Kind::FrameHandleReady:
        handleProviderFrameReady(event.token, event.frameHandle);
        break;
    case ViewportProviderEvent::Kind::FrameHandleWithMetadataReady:
        handleProviderFrameReadyWithMetadata(event.token, event.frameHandle, event.frameMetadata);
        break;
    case ViewportProviderEvent::Kind::Waiting:
        handleProviderWaiting(event.token);
        break;
    case ViewportProviderEvent::Kind::Progress:
        handleProviderProgress(event.token, event.progress);
        break;
    case ViewportProviderEvent::Kind::EndOfSequence:
        handleProviderEndOfSequence(event.token);
        break;
    case ViewportProviderEvent::Kind::Failure:
        handleProviderFailure(event.token, event.diagnostic);
        break;
    case ViewportProviderEvent::Kind::Unsupported:
        handleProviderUnsupported(event.token, event.unsupportedCause, event.diagnostic);
        break;
    case ViewportProviderEvent::Kind::Cancellation:
        handleProviderCancellation(event.token, event.diagnostic);
        break;
    }
}

void ImageViewportPrivate::startProviderMetadataRequest()
{
    const ViewportProviderMetadataRequestStartResult result
        = controller.startProviderMetadataRequest();
    if (result.closeSession) {
        providerBridge.closeSession(
            result.sessionClose.metadataToken, result.sessionClose.frameToken);
    }
    if (result.sendCommand) {
        requestProviderMetadata(result.token);
    }
}

void ImageViewportPrivate::requestProviderMetadata(ImageSequenceProviderRequestToken token)
{
    providerBridge.requestMetadata(token);
}

void ImageViewportPrivate::requestProviderFrame(ImageSequenceProviderRequestToken token, int frame)
{
    providerBridge.requestFrame(token, frame);
}

void ImageViewportPrivate::requestProviderPosition(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    providerBridge.requestPosition(token, frame, position);
}

void ImageViewportPrivate::requestProviderPlayback(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    providerBridge.requestPlayback(token, frame, position);
}

void ImageViewportPrivate::cancelProviderRequest(ImageSequenceProviderRequestToken token)
{
    providerBridge.cancelRequest(token);
}

void ImageViewportPrivate::clearQueuedProviderFrameRequest()
{
    m_queuedProviderFrameRequest = false;
    m_queuedProviderFrameGeneration = 0;
    m_queuedProviderFrameRequestId = 0;
    m_queuedProviderFrame = -1;
    m_queuedProviderPosition = -1;
    m_queuedProviderFrameFromPlayback = false;
    m_queuedProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
}

void ImageViewportPrivate::publishProviderFrameLoadingState()
{
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    discardPendingRenderCommit();
}

void ImageViewportPrivate::queueProviderFrameRequest(
    int frame, ProviderRequestTargetKind targetKind)
{
    const ViewportProviderFrameQueueResult result = controller.queueProviderFrameRequest(
        { frame, targetKind });
    if (result.cancelToken.isValid()) {
        cancelProviderRequest(result.cancelToken);
    }
    if (result.scheduleFlush) {
        QMetaObject::invokeMethod(
            q, [this]() { flushQueuedProviderFrameRequest(); }, Qt::QueuedConnection);
    }
}

void ImageViewportPrivate::flushQueuedProviderFrameRequest()
{
    const ViewportProviderFrameQueueFlush flush = controller.flushQueuedProviderFrameRequest();
    if (!flush.startRequest) {
        return;
    }

    startProviderFrameRequest(flush.frame, flush.targetKind);
    incrementRequestRevision();
    emit q->requestStateChanged();
    if (m_requestStatus == RequestStatus::Error
        && m_requestReason == RequestReason::ProviderFailure) {
        emit q->diagnosticsChanged();
    }
}

bool ImageViewportPrivate::startProviderFrameRequest(
    int frame, ProviderRequestTargetKind targetKind)
{
    const ViewportProviderFrameRequestStartResult result
        = controller.startProviderFrameRequest({ frame, targetKind });
    if (result.closeSession) {
        providerBridge.closeSession(
            result.sessionClose.metadataToken, result.sessionClose.frameToken);
    }
    if (!result.accepted) {
        return false;
    }
    if (result.sendCommand) {
        if (result.command.targetKind == ProviderRequestTargetKind::Playback) {
            requestProviderPlayback(
                result.command.token, result.command.frame, result.command.position);
        } else if (result.command.targetKind == ProviderRequestTargetKind::Position) {
            requestProviderPosition(
                result.command.token, result.command.frame, result.command.position);
        } else {
            requestProviderFrame(result.command.token, result.command.frame);
        }
    }
    return true;
}

bool ImageViewportPrivate::dispatchProviderFrameRequest(
    int frame, ProviderRequestTargetKind targetKind)
{
    if (m_activeProviderFrameToken.isValid()) {
        queueProviderFrameRequest(frame, targetKind);
        return true;
    }

    return startProviderFrameRequest(frame, targetKind);
}

void ImageViewportPrivate::publishProviderTokenExhaustion()
{
    clearQueuedProviderFrameRequest();
    m_activeProviderMetadataToken = {};
    m_activeProviderFrameToken = {};
    m_activeProviderFrameRequestId = 0;
    m_activeProviderFrameFromPlayback = false;
    m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
    m_providerPlaybackStartPending = false;
    m_stopPlaybackWhenRequestReady = false;
    m_requestStatus = RequestStatus::Error;
    m_requestReason = RequestReason::ProviderFailure;
    m_errorString = QStringLiteral("provider request token exhausted");
    setPlaybackPhase(PlaybackPhase::Stopped);
}

void ImageViewportPrivate::handleProviderMetadataReady(
    ImageSequenceProviderRequestToken token, const ImageSequenceProviderMetadata& metadata)
{
    const ViewportProviderMetadataEventAcceptance metadataEvent
        = controller.acceptProviderMetadataEvent({token});
    if (!metadataEvent.accepted) {
        return;
    }

    const ViewportProviderMetadataAdmissionResult metadataAdmission
        = controller.handleProviderMetadataAdmission(metadata);
    if (!metadataAdmission.accepted) {
        applyControllerChanges(metadataAdmission.changes);
        if (metadataAdmission.changes.playbackPhase) {
            syncPlaybackTimer();
        }
        closeProviderSession();
        return;
    }
    const ViewportProviderAcceptedMetadataFacts metadataFacts = metadataAdmission.facts;

    if (providerCapabilityContradictsMetadata(
            m_sequence->m_providerTimedPlaybackCapability, metadata.timedPlaybackSupport())
        || providerCapabilityContradictsMetadata(
            m_sequence->m_providerFrameSeekCapability, metadata.frameSeekSupport())
        || providerCapabilityContradictsMetadata(
            m_sequence->m_providerPositionSeekCapability, metadata.positionSeekSupport())) {
        applyProviderMetadataContradiction(
            *this,
            QStringLiteral("provider metadata contradicts construction-time capabilities"));
        closeProviderSession();
        return;
    }

    if (providerFactsContradictMetadata(m_sequence->m_providerKnownFacts, metadata)) {
        applyProviderMetadataContradiction(
            *this, QStringLiteral("provider metadata contradicts construction-time facts"));
        closeProviderSession();
        return;
    }

    applyProviderAcceptedMetadataFacts(
        *this, metadataFacts);
    const bool selectedFromPlaybackStart = m_providerPlaybackStartPending
        && request.activeRequest.target.providerTargetKind == ProviderRequestTargetKind::Playback;
    const bool selectedFromPosition
        = request.activeRequest.target.providerTargetKind == ProviderRequestTargetKind::Position;
    ProviderRequestTargetKind requestTargetKind = selectedFromPlaybackStart
        ? ProviderRequestTargetKind::Playback
        : (selectedFromPosition ? ProviderRequestTargetKind::Position
                                : ProviderRequestTargetKind::Frame);
    int selectedFrame
        = request.activeRequest.target.frame >= 0 ? request.activeRequest.target.frame : 0;
    const int providerFrameCount
        = metadataFacts.timedMetadata ? m_providerTimingIntervals.frameCount() : 1;
    if (selectedFromPlaybackStart
        && (!metadataFacts.timedMetadata || !m_providerTimedPlaybackSupport)) {
        applyProviderMetadataTargetRejection(
            *this,
            {RequestStatus::Unsupported, RequestReason::UnsupportedRequest, -1, false, false,
                true});
        return;
    }
    if (selectedFromPosition) {
        if (!metadataFacts.timedMetadata || !m_providerPositionSeekSupport) {
            applyProviderMetadataTargetRejection(
                *this,
                {RequestStatus::Unsupported, RequestReason::UnsupportedRequest, -1, false,
                    false, false});
            return;
        }
        selectedFrame = providerFrameIndexForPosition(request.activeRequest.target.position);
    }
    if (selectedFrame < 0 || selectedFrame >= providerFrameCount) {
        applyProviderMetadataTargetRejection(
            *this,
            {RequestStatus::Unsupported, RequestReason::InvalidRequest, selectedFrame, true,
                selectedFromPosition, false});
        return;
    }

    applyProviderMetadataTargetSelection(
        *this,
        {requestTargetKind, selectedFrame, selectedFromPosition, metadataFacts.timedMetadata});
}

void ImageViewportPrivate::handleProviderFrameReady(
    ImageSequenceProviderRequestToken token, ImageFrame* frame)
{
    handleProviderFrameReadyWithMetadata(
        token, frame, ImageSequenceProviderFrameMetadata::stillFrame());
}

void ImageViewportPrivate::handleProviderFrameReady(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame)
{
    handleProviderFrameReadyWithMetadata(
        token, frame, ImageSequenceProviderFrameMetadata::stillFrame());
}

void ImageViewportPrivate::handleProviderFrameReadyWithMetadata(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    std::unique_ptr<ImageSequenceProviderFrameHandle> ownedFrame(frame);
    handleProviderFrameReadyWithMetadata(
        token, ownedFrame ? ownedFrame->frame() : nullptr, metadata);
}

void ImageViewportPrivate::handleProviderFrameReadyWithMetadata(
    ImageSequenceProviderRequestToken token, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const ViewportProviderFrameEventAcceptance frameEvent
        = controller.acceptProviderFrameEvent({token});
    if (!frameEvent.accepted) {
        return;
    }

    const auto admission = FramePreparation::admitProviderFrame(
        frame, metadata, frameEvent.preparationState);
    const auto changes = controller.handleProviderFrameAdmission(admission);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::handleProviderWaiting(ImageSequenceProviderRequestToken token)
{
    applyControllerChanges(controller.handleProviderWaitingEvent({token, false, 0.0}));
}

void ImageViewportPrivate::handleProviderProgress(
    ImageSequenceProviderRequestToken token, double progress)
{
    applyControllerChanges(controller.handleProviderWaitingEvent({token, true, progress}));
}

void ImageViewportPrivate::handleProviderEndOfSequence(ImageSequenceProviderRequestToken token)
{
    const ViewportProviderEndOfSequenceResult result
        = controller.handleProviderEndOfSequenceEvent({token});
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        syncPlaybackTimer();
    }
    if (result.closeSession) {
        closeProviderSession();
    }
}

void ImageViewportPrivate::handleProviderFailure(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    applyProviderTerminalEvent(
        *this, {token, ViewportProviderTerminalEvent::Kind::Failure,
                   ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic});
}

void ImageViewportPrivate::handleProviderUnsupported(ImageSequenceProviderRequestToken token,
    ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic)
{
    applyProviderTerminalEvent(
        *this, {token, ViewportProviderTerminalEvent::Kind::Unsupported, cause, diagnostic});
}

void ImageViewportPrivate::handleProviderCancellation(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    applyProviderTerminalEvent(
        *this, {token, ViewportProviderTerminalEvent::Kind::Cancellation,
                   ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic});
}

std::shared_ptr<ImageSequenceProviderSessionFactory>
ImageViewportPrivate::providerSessionFactory() const
{
    return hasProviderSequence() ? m_sequence->m_providerSessionFactory : nullptr;
}

ImageSequenceProviderThreadingContract ImageViewportPrivate::providerThreadingContract() const
{
    if (hasProviderSequence()) {
        return m_sequence->m_providerThreadingContract;
    }
    return ImageSequenceProviderThreadingContract::AffinityBound;
}

int ImageViewportPrivate::providerFrameStartPosition(int frame) const
{
    if (!m_providerTimedMetadata) {
        return -1;
    }
    return m_providerTimingIntervals.frameStartPosition(frame);
}

int ImageViewportPrivate::providerFrameIndexForPosition(int position) const
{
    if (!m_providerTimedMetadata) {
        return -1;
    }
    return m_providerTimingIntervals.frameIndexForPosition(position);
}
