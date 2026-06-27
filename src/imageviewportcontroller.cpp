#include "framepreparation_p.h"
#include "imageviewport_p.h"

#include <algorithm>
#include <limits>

using namespace ImageViewportInternal;

namespace {
struct PlaybackAdvanceTarget {
    DisplayRequestTarget displayTarget;
    int playbackPosition = -1;
    bool reachedEnd = false;
    bool looped = false;
    bool valid = false;
};

template <typename FrameStartFor, typename FrameIndexFor>
PlaybackAdvanceTarget playbackAdvanceTarget(int elapsedMilliseconds, int currentFrame,
    int currentPlaybackPosition, bool looping, int totalDuration, int frameCount,
    FrameStartFor frameStartFor, FrameIndexFor frameIndexFor)
{
    PlaybackAdvanceTarget target;
    int nextPlaybackPosition
        = currentPlaybackPosition < 0 ? frameStartFor(currentFrame) : currentPlaybackPosition;
    nextPlaybackPosition += elapsedMilliseconds;

    if (nextPlaybackPosition >= totalDuration) {
        if (looping) {
            const int wrappedPosition = totalDuration > 0 ? nextPlaybackPosition % totalDuration : 0;
            const int wrappedFrame = frameIndexFor(wrappedPosition);
            if (wrappedFrame < 0) {
                return target;
            }
            target.displayTarget.frame = wrappedFrame;
            target.playbackPosition = wrappedPosition;
            target.displayTarget.position = frameStartFor(wrappedFrame);
            target.looped = true;
            target.valid = true;
            return target;
        }

        const int finalFrame = frameCount - 1;
        target.displayTarget.frame = finalFrame;
        target.displayTarget.position = frameStartFor(finalFrame);
        target.playbackPosition = totalDuration;
        target.reachedEnd = true;
        target.valid = true;
        return target;
    }

    const int nextFrame = frameIndexFor(nextPlaybackPosition);
    if (nextFrame < 0) {
        return target;
    }
    target.displayTarget.frame = nextFrame;
    target.displayTarget.position = frameStartFor(nextFrame);
    target.playbackPosition = nextPlaybackPosition;
    target.valid = true;
    return target;
}

void applyPlaybackTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    viewport.beginDisplayRequest(DisplayRequestOrigin::Playback, false);
    viewport.request.activeRequest.target.frame = target.frame;
    viewport.request.activeRequest.target.position = target.position;
}

void publishPlaybackRequestChange(ImageViewportPrivate& viewport, int previousFrame)
{
    viewport.incrementRequestRevision();
    if (viewport.request.activeRequest.target.frame != previousFrame
        || viewport.m_displayStatus != ImageViewport::DisplayStatus::Ready) {
        viewport.incrementDisplayRevision();
    }
    emit viewport.q->requestStateChanged();
    emit viewport.q->displayStateChanged();
}

ImageViewport::PlaybackPhase playbackAdvancePhaseForRequest(
    ImageViewport::RequestStatus requestStatus, bool reachedEnd)
{
    if (reachedEnd && requestStatus != ImageViewport::RequestStatus::Loading) {
        return ImageViewport::PlaybackPhase::Stopped;
    }
    return requestStatus == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

void applyPlaybackAdvancePhase(ImageViewportPrivate& viewport, const PlaybackAdvanceTarget& target)
{
    if (target.reachedEnd) {
        viewport.m_stopPlaybackWhenRequestReady
            = viewport.m_requestStatus == ImageViewport::RequestStatus::Loading;
    }
    viewport.setPlaybackPhase(
        playbackAdvancePhaseForRequest(viewport.m_requestStatus, target.reachedEnd));
}
}

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    if (m_playbackPhase != PlaybackPhase::Playing || elapsedMilliseconds <= 0) {
        return;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        const int duration = totalDuration();
        const int previousFrame = request.activeRequest.target.frame;
        const int currentFrame = request.activeRequest.target.frame;
        const PlaybackAdvanceTarget target = playbackAdvanceTarget(
            elapsedMilliseconds, currentFrame, request.playbackPosition, m_looping, duration,
            frameCount(), [this](int frame) { return providerFrameStartPosition(frame); },
            [this](int position) { return providerFrameIndexForPosition(position); });
        if (!target.valid) {
            return;
        }
        request.playbackPosition = target.playbackPosition;
        if (target.displayTarget.frame == previousFrame && m_requestStatus == RequestStatus::Ready) {
            if (m_stopPlaybackWhenRequestReady) {
                setPlaybackPhase(PlaybackPhase::Stopped);
                m_stopPlaybackWhenRequestReady = false;
            } else {
                applyPlaybackAdvancePhase(*this, target);
            }
            return;
        }

        applyPlaybackTarget(*this, target.displayTarget);
        request.activeRequest.target.providerTargetKind = ProviderRequestTargetKind::Playback;
        publishProviderFrameLoadingState();
        const bool diagnosticsValueChanged = clearDiagnostics();
        if (!dispatchProviderFrameRequest(
                target.displayTarget.frame, ProviderRequestTargetKind::Playback)) {
            publishPlaybackRequestChange(*this, previousFrame);
            emit q->diagnosticsChanged();
            update();
            return;
        }
        applyPlaybackAdvancePhase(*this, target);
        publishPlaybackRequestChange(*this, previousFrame);
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        update();
        return;
    }

    if (!hasTimedSequence()) {
        return;
    }

    const int totalDuration = m_sequence->totalDuration();
    const int previousFrame = request.activeRequest.target.frame;
    const int currentFrame = request.activeRequest.target.frame;
    const PlaybackAdvanceTarget target = playbackAdvanceTarget(
        elapsedMilliseconds, currentFrame, request.playbackPosition, m_looping, totalDuration,
        m_sequence->frameCount(), [this](int frame) { return m_sequence->frameStartPosition(frame); },
        [this](int position) { return m_sequence->frameIndexForPosition(position); });
    if (!target.valid) {
        return;
    }

    request.playbackPosition = target.playbackPosition;
    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        return;
    }

    applyPlaybackTarget(*this, target.displayTarget);
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    publishAcceptedTargetState();
    applyPlaybackAdvancePhase(*this, target);
    publishPlaybackRequestChange(*this, previousFrame);
    if (rectsDifferExactly(contentRect(), oldContentRect)
        || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
        emit q->geometryStateChanged();
    }
    update();
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTestImpl(int elapsedMilliseconds)
{
    advancePlayback(elapsedMilliseconds);
    syncPlaybackTimer();
}

void ImageViewportPrivate::setNextProviderRequestTokenForTestImpl(quint64 token)
{
    m_nextProviderRequestToken = token;
}

bool ImageViewportPrivate::hasPendingRenderCommitForTestImpl() const
{
    return m_pendingRenderPayload.commitPending;
}

quint64 ImageViewportPrivate::activeRequestIdForTestImpl() const
{
    return request.activeRequest.identity.id;
}

quint64 ImageViewportPrivate::displayedRequestIdForTestImpl() const
{
    return request.displayedRequest.request.identity.id;
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTestImpl() const
{
    return m_pendingRenderPayload.payloadId;
}
#endif

void ImageViewportPrivate::incrementDisplayRevision()
{
    ++m_displayRevision;
    emit q->displayRevisionChanged();
}

void ImageViewportPrivate::incrementRequestRevision()
{
    ++m_requestRevision;
    emit q->requestRevisionChanged();
}

void ImageViewportPrivate::setPlaybackPhase(PlaybackPhase phase)
{
    if (m_playbackPhase == phase) {
        return;
    }

    m_playbackPhase = phase;
    emit q->playbackPhaseChanged();
    syncPlaybackTimer();
}

void ImageViewportPrivate::syncPlaybackTimer()
{
    const int interval = playbackTimerInterval();
    if (interval <= 0) {
        stopPlaybackTimer();
        return;
    }

    playbackElapsedTimer.restart();
    playbackTimer.start(interval);
}

void ImageViewportPrivate::stopPlaybackTimer()
{
    playbackTimer.stop();
    playbackElapsedTimer.invalidate();
}

void ImageViewportPrivate::handlePlaybackTimer()
{
    advancePlayback(takePlaybackTimerElapsed());
    syncPlaybackTimer();
}

int ImageViewportPrivate::takePlaybackTimerElapsed()
{
    const qint64 elapsedMilliseconds
        = playbackElapsedTimer.isValid() ? playbackElapsedTimer.elapsed() : 0;
    playbackTimer.stop();
    playbackElapsedTimer.invalidate();
    return static_cast<int>(std::min<qint64>(elapsedMilliseconds, std::numeric_limits<int>::max()));
}

void ImageViewportPrivate::flushPlaybackTimerElapsed()
{
    if (!playbackElapsedTimer.isValid()) {
        return;
    }

    advancePlayback(takePlaybackTimerElapsed());
}

int ImageViewportPrivate::playbackTimerInterval() const
{
    if (m_playbackPhase != PlaybackPhase::Playing || m_requestStatus != RequestStatus::Ready) {
        return -1;
    }

    int frameStart = -1;
    int frameDuration = -1;
    const int currentFrame = request.activeRequest.target.frame;
    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        if (currentFrame < 0 || currentFrame >= m_providerTimingIntervals.frameCount()) {
            return -1;
        }
        frameStart = providerFrameStartPosition(currentFrame);
        frameDuration = m_providerTimingIntervals.frameDuration(currentFrame);
    } else if (hasTimedSequence()) {
        if (currentFrame < 0 || currentFrame >= m_sequence->frameCount()) {
            return -1;
        }
        frameStart = m_sequence->frameStartPosition(currentFrame);
        const int nextFrameStart = currentFrame + 1 < m_sequence->frameCount()
            ? m_sequence->frameStartPosition(currentFrame + 1)
            : m_sequence->totalDuration();
        frameDuration = nextFrameStart - frameStart;
    } else {
        return -1;
    }

    if (frameStart < 0 || frameDuration <= 0) {
        return -1;
    }

    const int playbackPosition = request.playbackPosition >= 0 ? request.playbackPosition : frameStart;
    const int remaining = frameStart + frameDuration - playbackPosition;
    return std::max(1, remaining);
}

void ImageViewportPrivate::setCommandDiagnostic(CommandReason reason)
{
    m_commandReason = reason;
    ++m_commandRevision;
    emit q->commandRevisionChanged();
    emit q->commandStateChanged();
}

void ImageViewportPrivate::clearCommandDiagnosticForAcceptedCommand()
{
    if (m_commandReason == CommandReason::NoCommand) {
        return;
    }

    setCommandDiagnostic(CommandReason::NoCommand);
}

bool ImageViewportPrivate::clearDiagnostics()
{
    if (m_errorString.isEmpty() && m_warningString.isEmpty()) {
        return false;
    }

    m_errorString.clear();
    m_warningString.clear();
    return true;
}

void ImageViewportPrivate::clearRequestIdentity()
{
    request.nextRequestId = 0;
    request.activeRequest.identity = {};
    request.latestNonPlaybackRequest.identity = {};
}

void ImageViewportPrivate::beginDisplayRequest(
    DisplayRequestOrigin origin, bool rememberAsLatestNonPlayback)
{
    request.activeRequest.identity.id = ++request.nextRequestId;
    request.activeRequest.identity.origin = origin;
    request.activeRequest.providerFrameToken = {};
    request.activeRequest.preparedPayloadId = 0;
    if (rememberAsLatestNonPlayback) {
        request.latestNonPlaybackRequest.identity = request.activeRequest.identity;
    }
}

void ImageViewportPrivate::beginInitialDisplayRequest(bool rememberAsLatestNonPlayback)
{
    beginDisplayRequest(DisplayRequestOrigin::Initial, rememberAsLatestNonPlayback);
}

ImageViewportPrivate::DisplayRequestSnapshot ImageViewportPrivate::activeDisplayRequestSnapshot(
    int displayedPosition) const
{
    DisplayRequestSnapshot snapshot = request.displayedRequest;
    snapshot.generation = request.sequenceGeneration;
    snapshot.request.target = request.activeRequest.target;
    snapshot.request.target.frame = request.activeRequest.target.frame;
    snapshot.request.target.position = displayedPosition;
    return snapshot;
}

void ImageViewportPrivate::commitDisplayedRequestSnapshot()
{
    const auto displayedTarget = request.displayedRequest.request.target;
    request.displayedRequest.generation = request.sequenceGeneration;
    request.displayedRequest.request = request.activeRequest;
    request.displayedRequest.request.target = displayedTarget;
    request.displayedRequest.request.preparedPayloadId = m_pendingRenderPayload.payloadId;
}

void ImageViewportPrivate::clearDisplayedDisplay()
{
    request.displayedRequest = {};
    m_displayedImageSize = {};
    m_displayedImage = {};
}

void ImageViewportPrivate::beginPreparedPayloadIdentity()
{
    m_pendingRenderPayload.generation = request.sequenceGeneration;
    m_pendingRenderPayload.requestId = request.activeRequest.identity.id;
    m_pendingRenderPayload.payloadId
        = request.activeRequest.identity.id == 0 ? 0 : ++m_nextPreparedPayloadId;
    request.activeRequest.preparedPayloadId = m_pendingRenderPayload.payloadId;
}

void ImageViewportPrivate::clearPendingRenderIdentity()
{
    m_pendingRenderPayload = {};
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::ignoredNoRequest()
{
    setCommandDiagnostic(CommandReason::IgnoredNoRequest);
    return CommandOutcome::IgnoredNoRequest;
}

bool ImageViewportPrivate::hasActiveRequest() const
{
    return m_requestStatus != RequestStatus::NoRequest;
}

bool ImageViewportPrivate::hasReadyDisplay() const
{
    return hasDisplayableSequence()
        && (m_displayStatus == DisplayStatus::Ready || m_displayStatus == DisplayStatus::Retained)
        && m_displayedImageSize.isValid() && m_displayedImageSize.width() > 0.0
        && m_displayedImageSize.height() > 0.0;
}

bool ImageViewportPrivate::hasDisplayableSequence() const
{
    return m_sequence && m_sequence->isValid();
}

bool ImageViewportPrivate::hasStillSequence() const { return m_sequence && m_sequence->isStill(); }

bool ImageViewportPrivate::hasTimedSequence() const
{
    return m_sequence && m_sequence->isTimedList();
}

bool ImageViewportPrivate::hasProviderSequence() const
{
    return m_sequence && m_sequence->isProvider();
}

bool ImageViewportPrivate::hasGenerationTerminalProviderFailure() const
{
    return hasProviderSequence() && !m_providerSession
        && (m_requestStatus == RequestStatus::Unsupported
            || m_requestStatus == RequestStatus::Error);
}

bool ImageViewportPrivate::providerTimedPlaybackCapabilityKnownFalse() const
{
    return m_sequence
        && providerCapabilityKnownFalse(m_sequence->m_providerTimedPlaybackCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownFalse() const
{
    return m_sequence && providerCapabilityKnownFalse(m_sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownTrue() const
{
    return m_sequence && providerCapabilityKnownTrue(m_sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerPositionSeekCapabilityKnownFalse() const
{
    return m_sequence && providerCapabilityKnownFalse(m_sequence->m_providerPositionSeekCapability);
}

bool ImageViewportPrivate::providerKnownFactsTimedFrameCount() const
{
    return m_sequence && m_sequence->m_providerKnownFacts.isTimedFrameCount();
}

int ImageViewportPrivate::providerKnownFactsFrameCount() const
{
    return providerKnownFactsTimedFrameCount() ? m_sequence->m_providerKnownFacts.frameCount() : 0;
}

int ImageViewportPrivate::sequenceFrameCount() const
{
    return m_sequence ? m_sequence->frameCount() : 0;
}

int ImageViewportPrivate::sequenceFrameIndexForPosition(int position) const
{
    return m_sequence ? m_sequence->frameIndexForPosition(position) : -1;
}

int ImageViewportPrivate::sequenceFrameStartPosition(int frame) const
{
    return m_sequence ? m_sequence->frameStartPosition(frame) : -1;
}

QString ImageViewportPrivate::boundedDiagnostic(const QString& diagnostic, const QString& fallback)
{
    return FramePreparation::boundedDiagnostic(diagnostic, fallback);
}
void ImageViewportPrivate::publishAcceptedTargetState(const QImage& providerImage)
{
    if (hasProviderSequence() && !providerImage.isNull()) {
        captureRenderFailureRetainedDisplay();
        m_pendingRenderPayload.image = providerImage;
        beginPreparedPayloadIdentity();
        if (itemBounds().isEmpty()) {
            publishRenderWaitingState();
        } else {
            m_requestStatus = RequestStatus::Loading;
            m_requestReason = RequestReason::UploadPending;
            m_displayStatus
                = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
            m_pendingRenderPayload.commitPending = false;
        }
        m_pendingRenderPayload.commitPending = true;
        return;
    }
    if (itemBounds().isEmpty()) {
        publishRenderWaitingState();
    } else {
        publishSequenceReadyState(providerImage);
    }
}

void ImageViewportPrivate::publishReadyDisplayState()
{
    m_requestStatus = RequestStatus::Ready;
    m_requestReason = RequestReason::Ready;
    m_displayStatus = DisplayStatus::Ready;
}

void ImageViewportPrivate::publishSequenceReadyState(const QImage& providerImage)
{
    captureRenderFailureRetainedDisplay();
    publishReadyDisplayState();
    m_pendingRenderPayload.commitPending = true;
    beginPreparedPayloadIdentity();
    int displayedPosition = -1;
    const int currentFrame = request.activeRequest.target.frame;
    if (hasProviderSequence()) {
        displayedPosition = providerFrameStartPosition(currentFrame);
    } else {
        displayedPosition = hasTimedSequence() ? m_sequence->frameStartPosition(currentFrame) : -1;
    }
    request.displayedRequest = activeDisplayRequestSnapshot(displayedPosition);
    m_displayedImageSize
        = hasProviderSequence() ? m_providerLogicalSize : m_sequence->logicalSize();
    if (hasProviderSequence()) {
        if (!providerImage.isNull()) {
            m_displayedImage = providerImage;
        } else if (!m_pendingRenderPayload.image.isNull()) {
            m_displayedImage = m_pendingRenderPayload.image;
        }
        m_pendingRenderPayload.image = {};
    } else {
        m_displayedImage = m_sequence
            ? m_sequence->frameImage(request.displayedRequest.request.target.frame)
            : QImage();
    }
}

void ImageViewportPrivate::publishRenderWaitingState()
{
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::RenderWaiting;
    m_displayStatus
        = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    m_pendingRenderPayload.commitPending = false;
}
