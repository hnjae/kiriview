#include "imagesequence_p.h"
#include "imageviewport_p.h"

#include <QtCore/QMetaObject>

#include <memory>

using namespace ImageViewportInternal;

namespace {
void applyProviderTerminalEvent(ImageViewportPrivate& viewport, ImageViewport::PageRole role,
    const ViewportProviderTerminalEvent& event)
{
    const ViewportProviderTerminalEventResult result
        = viewport.controller.handleProviderTerminalEvent(role, event);
    viewport.applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
    viewport.applyProviderFrameTransportEffect(result.providerFrameTransport, role);
}
}

bool ImageViewportPrivate::openProviderSession(PageRole role)
{
    ViewportProviderBridge& bridge
        = role == PageRole::Secondary ? secondaryProviderBridge : providerBridge;
    if (!bridge.openSession()) {
        return false;
    }

    const ViewportProviderSessionOpenResult result = controller.handleProviderSessionOpened(role);
    applyProviderMetadataTransportEffect(result.providerMetadataTransport, role);
    applyProviderFrameTransportEffect(result.providerFrameTransport, role);
    return true;
}

QObject* ImageViewportPrivate::providerCallbackTarget() const { return q; }

quint64 ImageViewportPrivate::installProviderSession(
    PageRole role, ImageSequenceProviderSession* session)
{
    return controller.installProviderSession(role, session);
}

ImageSequenceProviderSession* ImageViewportPrivate::takeProviderSession(PageRole role)
{
    return controller.takeProviderSession(role);
}

ImageSequenceProviderSession* ImageViewportPrivate::currentProviderSession(PageRole role) const
{
    return controller.currentProviderSession(role);
}

bool ImageViewportPrivate::providerHasCompleteKnownMetadata() const
{
    return ImageSequencePrivateAccess::hasCompleteProviderKnownMetadata(
        controller.requestState().sequence);
}

ImageSequenceProviderKnownFacts ImageViewportPrivate::providerKnownFacts() const
{
    return ImageSequencePrivateAccess::providerKnownFacts(controller.requestState().sequence);
}

QSizeF ImageViewportPrivate::providerKnownLogicalSize() const
{
    return ImageSequencePrivateAccess::providerKnownLogicalSize(controller.requestState().sequence);
}

TimingIntervals ImageViewportPrivate::providerKnownTimingIntervals() const
{
    return ImageSequencePrivateAccess::providerKnownTimingIntervals(
        controller.requestState().sequence);
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerTimedPlaybackCapability() const
{
    return ImageSequencePrivateAccess::providerTimedPlaybackCapability(
        controller.requestState().sequence);
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerFrameSeekCapability() const
{
    return ImageSequencePrivateAccess::providerFrameSeekCapability(
        controller.requestState().sequence);
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerPositionSeekCapability() const
{
    return ImageSequencePrivateAccess::providerPositionSeekCapability(
        controller.requestState().sequence);
}

ImageSequenceProviderKnownFacts ImageViewportPrivate::secondaryProviderKnownFacts() const
{
    ImageSequence* sequence = secondarySequence();
    return ImageSequencePrivateAccess::providerKnownFacts(sequence);
}

QSizeF ImageViewportPrivate::secondaryProviderKnownLogicalSize() const
{
    ImageSequence* sequence = secondarySequence();
    return ImageSequencePrivateAccess::providerKnownLogicalSize(sequence);
}

TimingIntervals ImageViewportPrivate::secondaryProviderKnownTimingIntervals() const
{
    ImageSequence* sequence = secondarySequence();
    return ImageSequencePrivateAccess::providerKnownTimingIntervals(sequence);
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderTimedPlaybackCapability() const
{
    ImageSequence* sequence = secondarySequence();
    return ImageSequencePrivateAccess::providerTimedPlaybackCapability(sequence);
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderFrameSeekCapability() const
{
    ImageSequence* sequence = secondarySequence();
    return ImageSequencePrivateAccess::providerFrameSeekCapability(sequence);
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderPositionSeekCapability() const
{
    ImageSequence* sequence = secondarySequence();
    return ImageSequencePrivateAccess::providerPositionSeekCapability(sequence);
}

void ImageViewportPrivate::handleProviderEvent(const ViewportProviderEvent& event)
{
    if (!controller.acceptsProviderSessionResult(event.role, event.sessionSerial)) {
        std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame(event.frameHandle);
        return;
    }

    if (event.role == PageRole::Secondary) {
        switch (event.kind) {
        case ViewportProviderEvent::Kind::MetadataReady:
            handleSecondaryProviderMetadataReady(event.token, event.metadata);
            break;
        case ViewportProviderEvent::Kind::ImageFrameReady:
            handleSecondaryProviderFrameReady(event.token, event.imageFrame);
            break;
        case ViewportProviderEvent::Kind::ImageFrameWithMetadataReady:
            handleSecondaryProviderFrameReadyWithMetadata(
                event.token, event.imageFrame, event.frameMetadata);
            break;
        case ViewportProviderEvent::Kind::FrameHandleReady:
            handleSecondaryProviderFrameReady(event.token, event.frameHandle);
            break;
        case ViewportProviderEvent::Kind::FrameHandleWithMetadataReady:
            handleSecondaryProviderFrameReadyWithMetadata(
                event.token, event.frameHandle, event.frameMetadata);
            break;
        case ViewportProviderEvent::Kind::Waiting:
            handleProviderWaiting(event.role, event.token);
            break;
        case ViewportProviderEvent::Kind::Progress:
            handleProviderProgress(event.role, event.token, event.progress);
            break;
        case ViewportProviderEvent::Kind::EndOfSequence:
            handleProviderEndOfSequence(event.role, event.token);
            break;
        case ViewportProviderEvent::Kind::Failure:
            handleProviderFailure(event.role, event.token, event.diagnostic);
            break;
        case ViewportProviderEvent::Kind::Unsupported:
            handleProviderUnsupported(
                event.role, event.token, event.unsupportedCause, event.diagnostic);
            break;
        case ViewportProviderEvent::Kind::Cancellation:
            handleProviderCancellation(event.role, event.token, event.diagnostic);
            break;
        }
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
        if (!providerBridge.requestMetadata(result.token)) {
            handleProviderDispatchFailure(PageRole::Primary, result.token,
                QStringLiteral("provider command delivery failed"));
        }
    }
}

void ImageViewportPrivate::applyProviderMetadataTransportEffect(
    const ViewportProviderMetadataTransportEffect& effect, PageRole role)
{
    ViewportProviderBridge& bridge
        = role == PageRole::Secondary ? secondaryProviderBridge : providerBridge;
    if (effect.closeSession) {
        bridge.closeSession(effect.sessionClose.metadataToken, effect.sessionClose.frameToken);
    }
    if (effect.sendCommand) {
        if (!bridge.requestMetadata(effect.token)) {
            handleProviderDispatchFailure(
                role, effect.token, QStringLiteral("provider command delivery failed"));
        }
    }
}

void ImageViewportPrivate::applyProviderFrameTransportEffect(
    const ViewportProviderFrameTransportEffect& effect, PageRole role)
{
    ViewportProviderBridge& bridge
        = role == PageRole::Secondary ? secondaryProviderBridge : providerBridge;
    if (effect.cancelToken.isValid()) {
        bridge.cancelRequest(effect.cancelToken);
    }
    if (effect.scheduleFlush) {
        QMetaObject::invokeMethod(
            q, [this]() { flushQueuedProviderFrameRequest(); }, Qt::QueuedConnection);
    }
    if (effect.closeSession) {
        bridge.closeSession(effect.sessionClose.metadataToken, effect.sessionClose.frameToken);
    }
    if (!effect.sendCommand) {
        return;
    }
    bool delivered = false;
    if (effect.command.targetKind == ProviderRequestTargetKind::Playback) {
        delivered = bridge.requestPlayback(
            effect.command.token, effect.command.frame, effect.command.position);
    } else if (effect.command.targetKind == ProviderRequestTargetKind::Position) {
        delivered = bridge.requestPosition(
            effect.command.token, effect.command.frame, effect.command.position);
    } else {
        delivered = bridge.requestFrame(effect.command.token, effect.command.frame);
    }
    if (!delivered) {
        handleProviderDispatchFailure(
            role, effect.command.token, QStringLiteral("provider command delivery failed"));
    }
}

void ImageViewportPrivate::handleProviderDispatchFailure(
    PageRole role, ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    const ViewportProviderTerminalEventResult result
        = controller.handleProviderDispatchFailure(role, { token, diagnostic });
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        syncPlaybackTimer();
    }
    applyProviderFrameTransportEffect(result.providerFrameTransport, role);
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
    const ViewportProviderFrameQueueFlushResult result
        = controller.flushQueuedProviderFrameRequestEvent();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        syncPlaybackTimer();
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
    const ViewportProviderMetadataReadyResult result
        = controller.handleProviderMetadataReadyEvent(PageRole::Primary, { token, metadata });
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::handleSecondaryProviderMetadataReady(
    ImageSequenceProviderRequestToken token, const ImageSequenceProviderMetadata& metadata)
{
    const ViewportProviderMetadataReadyResult result
        = controller.handleProviderMetadataReadyEvent(PageRole::Secondary, { token, metadata });
    applyProviderFrameTransportEffect(result.providerFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
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

void ImageViewportPrivate::handleSecondaryProviderFrameReady(
    ImageSequenceProviderRequestToken token, ImageFrame* frame)
{
    handleSecondaryProviderFrameReadyWithMetadata(
        token, frame, ImageSequenceProviderFrameMetadata::stillFrame());
}

void ImageViewportPrivate::handleSecondaryProviderFrameReady(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame)
{
    handleSecondaryProviderFrameReadyWithMetadata(
        token, frame, ImageSequenceProviderFrameMetadata::stillFrame());
}

void ImageViewportPrivate::handleSecondaryProviderFrameReadyWithMetadata(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    std::unique_ptr<ImageSequenceProviderFrameHandle> ownedFrame(frame);
    handleSecondaryProviderFrameReadyWithMetadata(
        token, ownedFrame ? ownedFrame->frame() : nullptr, metadata);
}

void ImageViewportPrivate::handleSecondaryProviderFrameReadyWithMetadata(
    ImageSequenceProviderRequestToken token, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const auto changes
        = controller.handleProviderFrameEvent(PageRole::Secondary, { token }, frame, metadata);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::handleProviderWaiting(ImageSequenceProviderRequestToken token)
{
    handleProviderWaiting(PageRole::Primary, token);
}

void ImageViewportPrivate::handleProviderWaiting(
    PageRole role, ImageSequenceProviderRequestToken token)
{
    applyControllerChanges(controller.handleProviderWaitingEvent(role, { token, false, 0.0 }));
}

void ImageViewportPrivate::handleProviderProgress(
    ImageSequenceProviderRequestToken token, double progress)
{
    handleProviderProgress(PageRole::Primary, token, progress);
}

void ImageViewportPrivate::handleProviderProgress(
    PageRole role, ImageSequenceProviderRequestToken token, double progress)
{
    applyControllerChanges(controller.handleProviderWaitingEvent(role, { token, true, progress }));
}

void ImageViewportPrivate::handleProviderEndOfSequence(ImageSequenceProviderRequestToken token)
{
    handleProviderEndOfSequence(PageRole::Primary, token);
}

void ImageViewportPrivate::handleProviderEndOfSequence(
    PageRole role, ImageSequenceProviderRequestToken token)
{
    const ViewportProviderEndOfSequenceResult result
        = controller.handleProviderEndOfSequenceEvent(role, { token });
    if (!result.providerFrameTransport.closeSession) {
        applyProviderFrameTransportEffect(result.providerFrameTransport, role);
    }
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        syncPlaybackTimer();
    }
    if (result.providerFrameTransport.closeSession) {
        applyProviderFrameTransportEffect(result.providerFrameTransport, role);
    }
}

void ImageViewportPrivate::handleProviderFailure(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    handleProviderFailure(PageRole::Primary, token, diagnostic);
}

void ImageViewportPrivate::handleProviderFailure(
    PageRole role, ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    applyProviderTerminalEvent(*this, role,
        { token, ViewportProviderTerminalEvent::Kind::Failure,
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic });
}

void ImageViewportPrivate::handleProviderUnsupported(ImageSequenceProviderRequestToken token,
    ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic)
{
    handleProviderUnsupported(PageRole::Primary, token, cause, diagnostic);
}

void ImageViewportPrivate::handleProviderUnsupported(PageRole role,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderSession::UnsupportedCause cause,
    const QString& diagnostic)
{
    applyProviderTerminalEvent(*this, role,
        { token, ViewportProviderTerminalEvent::Kind::Unsupported, cause, diagnostic });
}

void ImageViewportPrivate::handleProviderCancellation(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    handleProviderCancellation(PageRole::Primary, token, diagnostic);
}

void ImageViewportPrivate::handleProviderCancellation(
    PageRole role, ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    applyProviderTerminalEvent(*this, role,
        { token, ViewportProviderTerminalEvent::Kind::Cancellation,
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic });
}

std::shared_ptr<ImageSequenceProviderSessionFactory> ImageViewportPrivate::providerSessionFactory(
    PageRole role) const
{
    ImageSequence* sequence = role == PageRole::Secondary ? secondarySequence() : this->sequence();
    return ImageSequencePrivateAccess::providerSessionFactory(sequence);
}

ImageSequenceProviderThreadingContract ImageViewportPrivate::providerThreadingContract(
    PageRole role) const
{
    ImageSequence* sequence = role == PageRole::Secondary ? secondarySequence() : this->sequence();
    return ImageSequencePrivateAccess::providerThreadingContract(sequence);
}

int ImageViewportPrivate::providerFrameStartPosition(int frame) const
{
    return controller.providerFrameStartPosition(frame);
}

int ImageViewportPrivate::providerFrameIndexForPosition(int position) const
{
    return controller.providerFrameIndexForPosition(position);
}

ImageSequenceAuthoredAnimationFacts ImageViewportPrivate::providerAuthoredAnimationFacts() const
{
    ImageSequence* sequence = this->sequence();
    if (!sequence) {
        return {};
    }
    if (controller.providerMetadataReady()) {
        return controller.providerAuthoredAnimationFacts();
    }
    return ImageSequencePrivateAccess::authoredAnimationFacts(sequence);
}
