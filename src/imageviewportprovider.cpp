#include "imageviewport_p.h"

#include <QtCore/QMetaObject>

#include <memory>

using namespace ImageViewportInternal;

namespace {
void applyProviderTerminalEvent(
    ImageViewportPrivate& viewport, const ViewportProviderTerminalEvent& event)
{
    const ViewportProviderTerminalEventResult result
        = viewport.controller.handleProviderTerminalEvent(event);
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
    viewport.applyProviderFrameTransportEffect(result.providerFrameTransport);
}

void applyProviderAcceptedMetadataFacts(
    ImageViewportPrivate& viewport, const ViewportProviderAcceptedMetadataFacts& facts)
{
    viewport.applyControllerChanges(viewport.controller.handleProviderAcceptedMetadataFacts(facts));
}
}

bool ImageViewportPrivate::openProviderSession()
{
    if (!providerBridge.openSession()) {
        return false;
    }

    const ViewportProviderSessionOpenResult result = controller.handleProviderSessionOpened();
    applyProviderMetadataTransportEffect(result.providerMetadataTransport);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    return true;
}

QObject* ImageViewportPrivate::providerCallbackTarget() const { return q; }

quint64 ImageViewportPrivate::installProviderSession(ImageSequenceProviderSession* session)
{
    return controller.installProviderSession(session);
}

ImageSequenceProviderSession* ImageViewportPrivate::takeProviderSession()
{
    return controller.takeProviderSession();
}

ImageSequenceProviderSession* ImageViewportPrivate::currentProviderSession() const
{
    return controller.currentProviderSession();
}

bool ImageViewportPrivate::providerHasCompleteKnownMetadata() const
{
    return controller.requestState().sequence
        && controller.requestState().sequence->m_hasCompleteProviderKnownMetadata;
}

ImageSequenceProviderKnownFacts ImageViewportPrivate::providerKnownFacts() const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->m_providerKnownFacts
        : ImageSequenceProviderKnownFacts {};
}

QSizeF ImageViewportPrivate::providerKnownLogicalSize() const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->m_providerKnownLogicalSize
        : QSizeF {};
}

TimingIntervals ImageViewportPrivate::providerKnownTimingIntervals() const
{
    return controller.requestState().sequence
            && controller.requestState().sequence->m_providerKnownTimingIntervals
        ? *controller.requestState().sequence->m_providerKnownTimingIntervals
        : TimingIntervals();
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerTimedPlaybackCapability() const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->m_providerTimedPlaybackCapability
        : ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerFrameSeekCapability() const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->m_providerFrameSeekCapability
        : ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerPositionSeekCapability() const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->m_providerPositionSeekCapability
        : ImageSequenceProviderCapabilitySupport::Unavailable;
}

void ImageViewportPrivate::handleProviderEvent(const ViewportProviderEvent& event)
{
    if (!controller.acceptsProviderSessionResult(event.sessionSerial)) {
        std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame(event.frameHandle);
        return;
    }

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
        providerBridge.requestMetadata(result.token);
    }
}

void ImageViewportPrivate::applyProviderMetadataTransportEffect(
    const ViewportProviderMetadataTransportEffect& effect)
{
    if (effect.closeSession) {
        providerBridge.closeSession(
            effect.sessionClose.metadataToken, effect.sessionClose.frameToken);
    }
    if (effect.sendCommand) {
        providerBridge.requestMetadata(effect.token);
    }
}

void ImageViewportPrivate::applyProviderFrameTransportEffect(
    const ViewportProviderFrameTransportEffect& effect)
{
    if (effect.cancelToken.isValid()) {
        providerBridge.cancelRequest(effect.cancelToken);
    }
    if (effect.scheduleFlush) {
        QMetaObject::invokeMethod(
            q, [this]() { flushQueuedProviderFrameRequest(); }, Qt::QueuedConnection);
    }
    if (effect.closeSession) {
        providerBridge.closeSession(
            effect.sessionClose.metadataToken, effect.sessionClose.frameToken);
    }
    if (!effect.sendCommand) {
        return;
    }
    if (effect.command.targetKind == ProviderRequestTargetKind::Playback) {
        providerBridge.requestPlayback(
            effect.command.token, effect.command.frame, effect.command.position);
    } else if (effect.command.targetKind == ProviderRequestTargetKind::Position) {
        providerBridge.requestPosition(
            effect.command.token, effect.command.frame, effect.command.position);
    } else {
        providerBridge.requestFrame(effect.command.token, effect.command.frame);
    }
}

void ImageViewportPrivate::queueProviderFrameRequest(
    int frame, ProviderRequestTargetKind targetKind)
{
    const ViewportProviderFrameQueueResult result
        = controller.queueProviderFrameRequest({ frame, targetKind });
    ViewportProviderFrameTransportEffect effect;
    effect.cancelToken = result.cancelToken;
    effect.scheduleFlush = result.scheduleFlush;
    applyProviderFrameTransportEffect(effect);
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
    if (controller.requestState().status == RequestStatus::Error
        && controller.requestState().reason == RequestReason::ProviderFailure) {
        emit q->diagnosticsChanged();
    }
}

bool ImageViewportPrivate::startProviderFrameRequest(
    int frame, ProviderRequestTargetKind targetKind)
{
    const DisplayRequestTarget target { frame,
        controller.requestState().activeRequest.target.position, targetKind };
    const ViewportProviderFrameRequestStartResult result
        = controller.startProviderFrameRequest({ target });
    ViewportProviderFrameTransportEffect effect;
    effect.closeSession = result.closeSession;
    effect.sessionClose = result.sessionClose;
    effect.sendCommand = result.sendCommand;
    effect.command = result.command;
    applyProviderFrameTransportEffect(effect);
    return result.accepted;
}

void ImageViewportPrivate::handleProviderMetadataReady(
    ImageSequenceProviderRequestToken token, const ImageSequenceProviderMetadata& metadata)
{
    const ViewportProviderMetadataEventAcceptance metadataEvent
        = controller.acceptProviderMetadataEvent({ token });
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
        applyProviderFrameTransportEffect(metadataAdmission.providerFrameTransport);
        return;
    }
    const ViewportProviderAcceptedMetadataFacts metadataFacts = metadataAdmission.facts;

    applyProviderAcceptedMetadataFacts(*this, metadataFacts);
    const ViewportProviderMetadataTargetPolicyResult targetResult
        = controller.handleProviderMetadataTargetPolicy(metadataFacts);
    applyProviderFrameTransportEffect(targetResult.providerFrameTransport);
    applyControllerChanges(targetResult.changes);
    if (targetResult.changes.playbackPhase) {
        syncPlaybackTimer();
    }
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
    const auto changes = controller.handleProviderFrameEvent({ token }, frame, metadata);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::handleProviderWaiting(ImageSequenceProviderRequestToken token)
{
    applyControllerChanges(controller.handleProviderWaitingEvent({ token, false, 0.0 }));
}

void ImageViewportPrivate::handleProviderProgress(
    ImageSequenceProviderRequestToken token, double progress)
{
    applyControllerChanges(controller.handleProviderWaitingEvent({ token, true, progress }));
}

void ImageViewportPrivate::handleProviderEndOfSequence(ImageSequenceProviderRequestToken token)
{
    const ViewportProviderEndOfSequenceResult result
        = controller.handleProviderEndOfSequenceEvent({ token });
    if (!result.providerFrameTransport.closeSession) {
        applyProviderFrameTransportEffect(result.providerFrameTransport);
    }
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        syncPlaybackTimer();
    }
    if (result.providerFrameTransport.closeSession) {
        applyProviderFrameTransportEffect(result.providerFrameTransport);
    }
}

void ImageViewportPrivate::handleProviderFailure(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    applyProviderTerminalEvent(*this,
        { token, ViewportProviderTerminalEvent::Kind::Failure,
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic });
}

void ImageViewportPrivate::handleProviderUnsupported(ImageSequenceProviderRequestToken token,
    ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic)
{
    applyProviderTerminalEvent(
        *this, { token, ViewportProviderTerminalEvent::Kind::Unsupported, cause, diagnostic });
}

void ImageViewportPrivate::handleProviderCancellation(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    applyProviderTerminalEvent(*this,
        { token, ViewportProviderTerminalEvent::Kind::Cancellation,
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic });
}

std::shared_ptr<ImageSequenceProviderSessionFactory>
ImageViewportPrivate::providerSessionFactory() const
{
    return hasProviderSequence() ? controller.requestState().sequence->m_providerSessionFactory
                                 : nullptr;
}

ImageSequenceProviderThreadingContract ImageViewportPrivate::providerThreadingContract() const
{
    if (hasProviderSequence()) {
        return controller.requestState().sequence->m_providerThreadingContract;
    }
    return ImageSequenceProviderThreadingContract::AffinityBound;
}

int ImageViewportPrivate::providerFrameStartPosition(int frame) const
{
    return controller.providerFrameStartPosition(frame);
}

int ImageViewportPrivate::providerFrameIndexForPosition(int position) const
{
    return controller.providerFrameIndexForPosition(position);
}
