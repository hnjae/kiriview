#include "imageviewport_p.h"
#include "imagesequenceownership_p.h"

#include <utility>


using namespace ImageViewportInternal;

ImageSequence *ImageViewportPrivate::sequence() const
{
    return m_sequence;
}

void ImageViewportPrivate::setSequence(ImageSequence *sequence)
{
    if (!m_sequence && !sequence) {
        return;
    }

    const DisplayStatus oldDisplayStatus = m_displayStatus;
    const PlaybackPhase oldPlaybackPhase = m_playbackPhase;
    const QString oldErrorString = m_errorString;
    const QString oldWarningString = m_warningString;
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    std::shared_ptr<ImageSequence> sequenceOwner = factorySequenceOwner(sequence);
    closeProviderSession();
    m_sequence = sequence;
    m_sequenceOwner = std::move(sequenceOwner);
    ++m_sequenceGeneration;
    m_errorString.clear();
    m_warningString.clear();
    m_playbackPhase = PlaybackPhase::Stopped;
    m_stopPlaybackWhenRequestReady = false;
    m_providerPlaybackStartPending = false;
    m_providerMetadataReady = false;
    m_providerTimedMetadata = false;
    m_providerLogicalSize = {};
    m_providerFrameDurations.clear();
    discardPendingRenderCommit();
    m_activeProviderMetadataToken = {};
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;

    if (hasProviderSequence()) {
        if (m_sequence->m_hasProviderKnownMetadata) {
            m_providerMetadataReady = m_sequence->m_hasCompleteProviderKnownMetadata;
            m_providerTimedMetadata = !m_sequence->m_providerKnownFrameDurations.isEmpty();
            m_providerLogicalSize = m_sequence->m_providerKnownLogicalSize;
            m_providerFrameDurations = m_sequence->m_providerKnownFrameDurations;
            m_currentFrame = m_providerMetadataReady ? 0 : -1;
            m_requestedPosition = m_providerMetadataReady ? (m_providerTimedMetadata ? 0 : -1) : -1;
            m_playbackPosition = m_requestedPosition;
            m_latestNonPlaybackFrame = m_currentFrame;
            m_latestNonPlaybackPosition = m_requestedPosition;
        } else {
            m_currentFrame = -1;
            m_requestedPosition = -1;
            m_playbackPosition = -1;
            m_latestNonPlaybackFrame = -1;
            m_latestNonPlaybackPosition = -1;
        }
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        if (!openProviderSession()) {
            m_requestStatus = RequestStatus::Error;
            m_requestReason = RequestReason::ProviderFailure;
            m_errorString = QStringLiteral("provider session creation failed");
        }
    } else if (hasDisplayableSequence()) {
        m_currentFrame = 0;
        m_requestedPosition = hasTimedSequence() ? 0 : -1;
        m_playbackPosition = hasTimedSequence() ? 0 : -1;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        if (width() > 0.0 && height() > 0.0) {
            publishSequenceReadyState();
        } else {
            publishRenderWaitingState();
        }
    } else {
        m_currentFrame = -1;
        m_requestedPosition = -1;
        m_playbackPosition = -1;
        m_latestNonPlaybackFrame = -1;
        m_latestNonPlaybackPosition = -1;
        m_displayedFrame = -1;
        m_displayedPosition = -1;
        m_displayedGeneration = 0;
        m_displayedImageSize = {};
        m_displayedImage = {};
        m_requestStatus = RequestStatus::NoRequest;
        m_requestReason = RequestReason::NoRequest;
        m_displayStatus = DisplayStatus::Empty;
        m_renderCommitPending = false;
        clearRenderFailureRetainedDisplay();
    }

    incrementRequestRevision();
    const bool displayValueChanged = m_displayStatus != oldDisplayStatus || m_displayStatus == DisplayStatus::Ready;
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
    if (m_playbackPhase != oldPlaybackPhase) {
        emit q->playbackPhaseChanged();
    }
    if (m_errorString != oldErrorString || m_warningString != oldWarningString) {
        emit q->diagnosticsChanged();
    }
    syncPlaybackTimer();
    update();
}

ImageViewportPrivate::RequestStatus ImageViewportPrivate::requestStatus() const
{
    return m_requestStatus;
}

ImageViewportPrivate::RequestReason ImageViewportPrivate::requestReason() const
{
    return m_requestReason;
}

ImageViewportPrivate::CommandReason ImageViewportPrivate::commandReason() const
{
    return m_commandReason;
}

ImageViewportPrivate::DisplayStatus ImageViewportPrivate::displayStatus() const
{
    return m_displayStatus;
}

ImageViewportPrivate::PlaybackPhase ImageViewportPrivate::playbackPhase() const
{
    return m_playbackPhase;
}

int ImageViewportPrivate::displayedFrame() const
{
    if (hasReadyDisplay()) {
        return m_displayedFrame;
    }

    return -1;
}

int ImageViewportPrivate::requestedFrame() const
{
    if (hasDisplayableSequence()) {
        return m_currentFrame;
    }

    return -1;
}

int ImageViewportPrivate::displayedPosition() const
{
    if (hasReadyDisplay()) {
        return m_displayedPosition;
    }

    return -1;
}

int ImageViewportPrivate::requestedPosition() const
{
    if (hasProviderSequence() && (m_providerTimedMetadata || m_requestedPosition >= 0)) {
        return m_requestedPosition;
    }
    if (hasTimedSequence()) {
        return m_requestedPosition;
    }

    return -1;
}

int ImageViewportPrivate::frameCount() const
{
    if (hasProviderSequence() && (m_providerMetadataReady || m_sequence->m_hasProviderKnownMetadata)) {
        return m_providerTimedMetadata ? m_providerFrameDurations.size() : 1;
    }
    if (hasDisplayableSequence()) {
        return m_sequence->frameCount();
    }

    return -1;
}

int ImageViewportPrivate::totalDuration() const
{
    if (hasProviderSequence() && m_providerTimedMetadata) {
        int total = 0;
        for (int duration : std::as_const(m_providerFrameDurations)) {
            total += duration;
        }
        return total;
    }
    if (hasTimedSequence()) {
        return m_sequence->totalDuration();
    }

    return -1;
}

QVariantMap ImageViewportPrivate::frameSeekBounds() const
{
    if (hasProviderSequence() && (m_providerMetadataReady || m_sequence->m_hasProviderKnownMetadata)) {
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), m_providerTimedMetadata ? m_providerFrameDurations.size() - 1 : 0},
        };
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), m_sequence->frameCount() - 1},
        };
    }

    return invalidRange();
}

QVariantMap ImageViewportPrivate::positionSeekBounds() const
{
    if (hasProviderSequence() && m_providerTimedMetadata) {
        int total = 0;
        for (int duration : std::as_const(m_providerFrameDurations)) {
            total += duration;
        }
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), total},
        };
    }
    if (hasTimedSequence()) {
        return {
            {QStringLiteral("minimum"), 0},
            {QStringLiteral("maximum"), m_sequence->totalDuration()},
        };
    }

    return invalidRange();
}

ImageViewportPrivate::TriState ImageViewportPrivate::timedPlaybackSupport() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return m_providerTimedMetadata ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(m_sequence->m_providerTimedPlaybackCapability);
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
    if (hasProviderSequence() && m_providerMetadataReady) {
        return TriState::True;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(m_sequence->m_providerFrameSeekCapability);
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return TriState::True;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::positionSeekSupport() const
{
    if (hasProviderSequence() && m_providerMetadataReady) {
        return m_providerTimedMetadata ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(m_sequence->m_providerPositionSeekCapability);
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
        return m_displayedImageSize;
    }

    return QSizeF(0.0, 0.0);
}

uint ImageViewportPrivate::displayRevision() const
{
    return m_displayRevision;
}

uint ImageViewportPrivate::requestRevision() const
{
    return m_requestRevision;
}

uint ImageViewportPrivate::commandRevision() const
{
    return m_commandRevision;
}

QString ImageViewportPrivate::errorString() const
{
    return m_errorString;
}

QString ImageViewportPrivate::warningString() const
{
    return m_warningString;
}
