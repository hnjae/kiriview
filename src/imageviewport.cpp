#include "imagesequenceownership_p.h"
#include "imageviewport_p.h"

#include <utility>

using namespace ImageViewportInternal;

namespace {
void mergeControllerChanges(ViewportChangeSet& target, ViewportChangeSet source)
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
}

ImageSequence* ImageViewportPrivate::sequence() const { return request.sequence; }

void ImageViewportPrivate::setSequence(ImageSequence* sequence)
{
    if (!request.sequence && !sequence) {
        return;
    }

    std::shared_ptr<ImageSequence> sequenceOwner = factorySequenceOwner(sequence);
    ViewportSequenceAssignmentResult result
        = controller.assignSequence({ sequence, std::move(sequenceOwner) });
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    if (result.openProviderSession && !openProviderSession()) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
}

ImageViewportPrivate::RequestStatus ImageViewportPrivate::requestStatus() const
{
    return request.status;
}

ImageViewportPrivate::RequestReason ImageViewportPrivate::requestReason() const
{
    return request.reason;
}

ImageViewportPrivate::CommandReason ImageViewportPrivate::commandReason() const
{
    return request.commandReason;
}

ImageViewportPrivate::DisplayStatus ImageViewportPrivate::displayStatus() const
{
    return display.status;
}

ImageViewportPrivate::PlaybackPhase ImageViewportPrivate::playbackPhase() const
{
    return request.playbackPhase;
}

int ImageViewportPrivate::displayedFrame() const
{
    if (hasReadyDisplay()) {
        return display.displayedRequest.request.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::requestedFrame() const
{
    if (hasDisplayableSequence()) {
        return request.activeRequest.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::displayedPosition() const
{
    if (hasReadyDisplay()) {
        return display.displayedRequest.request.target.position;
    }

    return -1;
}

int ImageViewportPrivate::requestedPosition() const
{
    if (hasProviderSequence()
        && (provider.timedMetadata || request.activeRequest.target.position >= 0)) {
        return request.activeRequest.target.position;
    }
    if (hasTimedSequence()) {
        return request.activeRequest.target.position;
    }

    return -1;
}

int ImageViewportPrivate::frameCount() const
{
    if (hasProviderSequence() && provider.metadataReady) {
        return provider.timedMetadata ? provider.timingIntervals.frameCount() : 1;
    }
    if (hasProviderSequence() && !provider.metadataReady
        && request.sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(request.sequence->m_providerFrameSeekCapability)) {
        return request.sequence->m_providerKnownFacts.frameCount();
    }
    if (hasDisplayableSequence()) {
        return request.sequence->frameCount();
    }

    return -1;
}

int ImageViewportPrivate::totalDuration() const
{
    if (hasProviderSequence() && provider.timedMetadata) {
        return provider.timingIntervals.totalDuration();
    }
    if (hasTimedSequence()) {
        return request.sequence->totalDuration();
    }

    return -1;
}

QVariantMap ImageViewportPrivate::frameSeekBounds() const
{
    if (hasProviderSequence() && provider.metadataReady) {
        if (!provider.frameSeekSupport) {
            return invalidRange();
        }
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"),
                provider.timedMetadata ? provider.timingIntervals.frameCount() - 1 : 0 },
        };
    }
    if (hasProviderSequence() && !provider.metadataReady
        && request.sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(request.sequence->m_providerFrameSeekCapability)) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), request.sequence->m_providerKnownFacts.frameCount() - 1 },
        };
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), request.sequence->frameCount() - 1 },
        };
    }

    return invalidRange();
}

QVariantMap ImageViewportPrivate::positionSeekBounds() const
{
    if (hasProviderSequence() && provider.timedMetadata && provider.positionSeekSupport) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), provider.timingIntervals.totalDuration() },
        };
    }
    if (hasTimedSequence()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), request.sequence->totalDuration() },
        };
    }

    return invalidRange();
}

ImageViewportPrivate::TriState ImageViewportPrivate::timedPlaybackSupport() const
{
    if (hasProviderSequence() && provider.metadataReady) {
        return provider.timedPlaybackSupport ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(request.sequence->m_providerTimedPlaybackCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::frameSeekSupport() const
{
    if (hasProviderSequence() && provider.metadataReady) {
        return provider.frameSeekSupport ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(request.sequence->m_providerFrameSeekCapability);
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return TriState::True;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::positionSeekSupport() const
{
    if (hasProviderSequence() && provider.metadataReady) {
        return provider.positionSeekSupport ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(request.sequence->m_providerPositionSeekCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

QSizeF ImageViewportPrivate::displayedImageSize() const
{
    if (hasReadyDisplay()) {
        return display.displayedImageSize;
    }

    return QSizeF(0.0, 0.0);
}

uint ImageViewportPrivate::displayRevision() const { return display.revision; }

uint ImageViewportPrivate::requestRevision() const { return request.requestRevision; }

uint ImageViewportPrivate::commandRevision() const { return request.commandRevision; }

QString ImageViewportPrivate::errorString() const { return request.errorString; }

QString ImageViewportPrivate::warningString() const { return request.warningString; }
