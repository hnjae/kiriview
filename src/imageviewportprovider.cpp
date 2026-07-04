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
    return controller.requestState().sequenceSource.facts.hasCompleteProviderKnownMetadata;
}

ImageSequenceProviderKnownFacts ImageViewportPrivate::providerKnownFacts() const
{
    return controller.requestState().sequenceSource.facts.providerKnownFacts;
}

QSizeF ImageViewportPrivate::providerKnownLogicalSize() const
{
    return controller.requestState().sequenceSource.facts.providerKnownLogicalSize;
}

TimingIntervals ImageViewportPrivate::providerKnownTimingIntervals() const
{
    return controller.requestState().sequenceSource.facts.providerKnownTimingIntervals;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerTimedPlaybackCapability() const
{
    return controller.requestState().sequenceSource.facts.providerTimedPlaybackCapability;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerFrameSeekCapability() const
{
    return controller.requestState().sequenceSource.facts.providerFrameSeekCapability;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerPositionSeekCapability() const
{
    return controller.requestState().sequenceSource.facts.providerPositionSeekCapability;
}

ImageSequenceProviderKnownFacts ImageViewportPrivate::secondaryProviderKnownFacts() const
{
    return controller.requestState().secondarySequenceSource.facts.providerKnownFacts;
}

QSizeF ImageViewportPrivate::secondaryProviderKnownLogicalSize() const
{
    return controller.requestState().secondarySequenceSource.facts.providerKnownLogicalSize;
}

TimingIntervals ImageViewportPrivate::secondaryProviderKnownTimingIntervals() const
{
    return controller.requestState().secondarySequenceSource.facts.providerKnownTimingIntervals;
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderTimedPlaybackCapability() const
{
    return controller.requestState().secondarySequenceSource.facts.providerTimedPlaybackCapability;
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderFrameSeekCapability() const
{
    return controller.requestState().secondarySequenceSource.facts.providerFrameSeekCapability;
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderPositionSeekCapability() const
{
    return controller.requestState()
        .secondarySequenceSource.facts.providerPositionSeekCapability;
}

void ImageViewportPrivate::handleProviderEvent(const ViewportProviderEvent& event)
{
    if (!controller.acceptsProviderSessionResult(event.role, event.sessionSerial)) {
        std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame(event.frameHandle);
        return;
    }

    switch (event.kind) {
    case ViewportProviderEvent::Kind::MetadataReady:
        handleProviderMetadataReady(event.role, event.token, event.metadata);
        break;
    case ViewportProviderEvent::Kind::ImageFrameReady:
        handleProviderFrameReady(event.role, event.token, event.imageFrame);
        break;
    case ViewportProviderEvent::Kind::ImageFrameWithMetadataReady:
        handleProviderFrameReadyWithMetadata(
            event.role, event.token, event.imageFrame, event.frameMetadata);
        break;
    case ViewportProviderEvent::Kind::FrameHandleReady:
        handleProviderFrameReady(event.role, event.token, event.frameHandle);
        break;
    case ViewportProviderEvent::Kind::FrameHandleWithMetadataReady:
        handleProviderFrameReadyWithMetadata(
            event.role, event.token, event.frameHandle, event.frameMetadata);
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
        handleProviderUnsupported(event.role, event.token, event.unsupportedCause,
            event.unsupportedCauseExplicit, event.diagnostic);
        break;
    case ViewportProviderEvent::Kind::Cancellation:
        handleProviderCancellation(event.role, event.token, event.diagnostic);
        break;
    }
}

void ImageViewportPrivate::startProviderMetadataRequest()
{
    const ViewportProviderMetadataRequestStartResult result
        = controller.startProviderMetadataRequest();
    if (result.closeSession) {
        recordProviderTransportResult(providerBridge.closeSession(
            result.sessionClose.metadataToken, result.sessionClose.frameToken));
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
        recordProviderTransportResult(
            bridge.closeSession(effect.sessionClose.metadataToken, effect.sessionClose.frameToken));
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
        recordProviderTransportResult(bridge.cancelRequest(effect.cancelToken));
    }
    if (effect.deferredControllerEvent != ViewportProviderDeferredControllerEvent::None) {
        scheduleProviderDeferredControllerEvent(effect.deferredControllerEvent, role);
    }
    if (effect.closeSession) {
        recordProviderTransportResult(
            bridge.closeSession(effect.sessionClose.metadataToken, effect.sessionClose.frameToken));
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

void ImageViewportPrivate::recordProviderTransportResult(
    const ViewportProviderTransportResult& result)
{
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    if (result.diagnostic.valid) {
        lastProviderTransportDiagnostic = result.diagnostic;
    }
#else
    Q_UNUSED(result);
#endif
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
    effect.deferredControllerEvent = result.deferredControllerEvent;
    applyProviderFrameTransportEffect(effect);
}

void ImageViewportPrivate::scheduleProviderDeferredControllerEvent(
    ViewportProviderDeferredControllerEvent event, PageRole role)
{
    switch (event) {
    case ViewportProviderDeferredControllerEvent::None:
        return;
    case ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest:
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
        if (synchronousProviderQueueFlushScheduler) {
            flushQueuedProviderFrameRequest(role);
            return;
        }
#endif
        QMetaObject::invokeMethod(
            q, [this, role]() { flushQueuedProviderFrameRequest(role); }, Qt::QueuedConnection);
        return;
    }
}

void ImageViewportPrivate::flushQueuedProviderFrameRequest(PageRole role)
{
    const ViewportProviderFrameQueueFlushResult result
        = controller.flushQueuedProviderFrameRequestEvent(role);
    applyProviderFrameTransportEffect(result.providerFrameTransport, role);
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
    PageRole role, ImageSequenceProviderRequestToken token,
    const ImageSequenceProviderMetadata& metadata)
{
    const ViewportProviderMetadataReadyResult result
        = controller.handleProviderMetadataReadyEvent(role, { token, metadata });
    applyProviderFrameTransportEffect(result.providerFrameTransport, role);
    applyControllerChanges(result.changes);
    if (result.changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::handleProviderFrameReady(
    PageRole role, ImageSequenceProviderRequestToken token, ImageFrame* frame)
{
    handleProviderFrameReadyWithMetadata(
        role, token, frame, ImageSequenceProviderFrameMetadata::stillFrame());
}

void ImageViewportPrivate::handleProviderFrameReady(
    PageRole role, ImageSequenceProviderRequestToken token,
    ImageSequenceProviderFrameHandle* frame)
{
    handleProviderFrameReadyWithMetadata(
        role, token, frame, ImageSequenceProviderFrameMetadata::stillFrame());
}

void ImageViewportPrivate::handleProviderFrameReadyWithMetadata(
    PageRole role, ImageSequenceProviderRequestToken token,
    ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata)
{
    std::unique_ptr<ImageSequenceProviderFrameHandle> ownedFrame(frame);
    handleProviderFrameReadyWithMetadata(
        role, token, ownedFrame ? ownedFrame->frame() : nullptr, metadata);
}

void ImageViewportPrivate::handleProviderFrameReadyWithMetadata(
    PageRole role, ImageSequenceProviderRequestToken token, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const auto changes = controller.handleProviderFrameEvent(role, { token }, frame, metadata);
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
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic,
            false });
}

void ImageViewportPrivate::handleProviderUnsupported(ImageSequenceProviderRequestToken token,
    ImageSequenceProviderSession::UnsupportedCause cause, bool causeExplicit,
    const QString& diagnostic)
{
    handleProviderUnsupported(PageRole::Primary, token, cause, causeExplicit, diagnostic);
}

void ImageViewportPrivate::handleProviderUnsupported(PageRole role,
    ImageSequenceProviderRequestToken token, ImageSequenceProviderSession::UnsupportedCause cause,
    bool causeExplicit, const QString& diagnostic)
{
    applyProviderTerminalEvent(*this, role,
        { token, ViewportProviderTerminalEvent::Kind::Unsupported, cause, diagnostic,
            causeExplicit });
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
            ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic,
            false });
}

std::shared_ptr<ImageSequenceProviderSessionFactory> ImageViewportPrivate::providerSessionFactory(
    PageRole role) const
{
    const ImageSequenceSource& source = role == PageRole::Secondary
        ? controller.requestState().secondarySequenceSource
        : controller.requestState().sequenceSource;
    return source.providerSessionFactory;
}

ImageSequenceProviderThreadingContract ImageViewportPrivate::providerThreadingContract(
    PageRole role) const
{
    const ImageSequenceSource& source = role == PageRole::Secondary
        ? controller.requestState().secondarySequenceSource
        : controller.requestState().sequenceSource;
    return source.facts.providerThreadingContract;
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
    const ImageSequenceSource& source = controller.requestState().sequenceSource;
    if (!source.sequence) {
        return {};
    }
    if (controller.providerMetadataReady()) {
        return controller.providerAuthoredAnimationFacts();
    }
    return source.facts.authoredAnimationFacts;
}
