#include "imagesequenceownership_p.h"
#include "imageviewport_p.h"

#include <utility>

using namespace ImageViewportInternal;

ImageSequence* ImageViewportPrivate::sequence() const { return request.sequence; }

void ImageViewportPrivate::setSequence(ImageSequence* sequence)
{
    if (!request.sequence && !sequence) {
        return;
    }

    const DisplayStatus oldDisplayStatus = display.status;
    const PlaybackPhase oldPlaybackPhase = request.playbackPhase;
    const QString oldErrorString = request.errorString;
    const QString oldWarningString = request.warningString;
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    std::shared_ptr<ImageSequence> sequenceOwner = factorySequenceOwner(sequence);
    applyProviderFrameTransportEffect(controller.closeProviderSession());
    request.sequence = sequence;
    request.sequenceOwner = std::move(sequenceOwner);
    ++request.sequenceGeneration;
    request.clearDisplayRequestIdentity();
    display.nextPreparedPayloadId = 0;
    display.clearPendingRenderPayload();
    request.errorString.clear();
    request.warningString.clear();
    request.playbackPhase = PlaybackPhase::Stopped;
    request.stopPlaybackWhenRequestReady = false;
    request.providerPlaybackStartPending = false;
    provider.metadataReady = false;
    provider.timedMetadata = false;
    provider.timedPlaybackSupport = false;
    provider.frameSeekSupport = false;
    provider.positionSeekSupport = false;
    provider.logicalSize = {};
    provider.timingIntervals = {};
    discardPendingRenderCommit();
    provider.activeMetadataToken = {};
    provider.activeFrameToken = {};
    provider.activeFrameRequestId = 0;
    provider.activeFrameFromPlayback = false;
    provider.activeFrameTargetKind = ProviderRequestTargetKind::Unknown;

    if (hasProviderSequence()) {
        if (request.sequence->m_hasCompleteProviderKnownMetadata) {
            provider.metadataReady = true;
            provider.timedMetadata = request.sequence->m_providerKnownFacts.isTimedFrameList();
            provider.timedPlaybackSupport = providerResolvedCapability(
                request.sequence->m_providerTimedPlaybackCapability, provider.timedMetadata);
            provider.frameSeekSupport
                = providerResolvedCapability(request.sequence->m_providerFrameSeekCapability, true);
            provider.positionSeekSupport = providerResolvedCapability(
                request.sequence->m_providerPositionSeekCapability, provider.timedMetadata);
            provider.logicalSize = request.sequence->m_providerKnownLogicalSize;
            provider.timingIntervals
                = provider.timedMetadata && request.sequence->m_providerKnownTimingIntervals
                ? *request.sequence->m_providerKnownTimingIntervals
                : TimingIntervals();
            const DisplayRequestTarget initialTarget {
                0,
                provider.timedMetadata ? 0 : -1,
                ProviderRequestTargetKind::Frame,
            };
            request.beginDisplayRequest(DisplayRequestOrigin::Initial, initialTarget, true);
            request.playbackPosition = initialTarget.position;
        } else {
            const DisplayRequestTarget initialTarget {
                -1,
                -1,
                ProviderRequestTargetKind::Unknown,
            };
            request.beginDisplayRequest(DisplayRequestOrigin::Initial, initialTarget, true);
            request.playbackPosition = initialTarget.position;
        }
        request.status = RequestStatus::Loading;
        request.reason = RequestReason::ProviderWaiting;
        display.status
            = display.displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        if (!openProviderSession()) {
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed"));
        }
    } else if (hasDisplayableSequence()) {
        const DisplayRequestTarget initialTarget {
            0,
            hasTimedSequence() ? 0 : -1,
            ProviderRequestTargetKind::Unknown,
        };
        request.beginDisplayRequest(DisplayRequestOrigin::Initial, initialTarget, true);
        request.playbackPosition = initialTarget.position;
        if (width() > 0.0 && height() > 0.0) {
            publishSequenceReadyState();
        } else {
            publishRenderWaitingState();
        }
    } else {
        request.activeRequest.target.frame = -1;
        request.activeRequest.target.position = -1;
        request.playbackPosition = -1;
        request.latestNonPlaybackRequest.target.frame = -1;
        request.latestNonPlaybackRequest.target.position = -1;
        request.activeRequest.target.providerTargetKind = ProviderRequestTargetKind::Unknown;
        request.latestNonPlaybackRequest.target.providerTargetKind
            = ProviderRequestTargetKind::Unknown;
        display.clearDisplayedDisplay();
        request.status = RequestStatus::NoRequest;
        request.reason = RequestReason::NoRequest;
        display.status = DisplayStatus::Empty;
        display.clearPendingRenderPayload();
        display.clearRenderFailureRetainedDisplay();
    }

    incrementRequestRevision();
    const bool displayValueChanged
        = display.status != oldDisplayStatus || display.status == DisplayStatus::Ready;
    if (displayValueChanged) {
        incrementDisplayRevision();
    }
    emit q->sequenceChanged();
    emit q->requestStateChanged();
    if (displayValueChanged) {
        emit q->displayStateChanged();
    }
    if (rectsDifferExactly(contentRect(), oldContentRect)
        || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
        emit q->geometryStateChanged();
    }
    if (request.playbackPhase != oldPlaybackPhase) {
        emit q->playbackPhaseChanged();
    }
    if (request.errorString != oldErrorString || request.warningString != oldWarningString) {
        emit q->diagnosticsChanged();
    }
    syncPlaybackTimer();
    update();
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
