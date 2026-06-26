#include "framepreparation_p.h"
#include "imageviewport_p.h"

#include <QtCore/QMetaObject>

#include <memory>

using namespace ImageViewportInternal;

void ImageViewportPrivate::closeProviderSession()
{
    clearQueuedProviderFrameRequest();
    providerBridge.closeSession();
}

bool ImageViewportPrivate::openProviderSession() { return providerBridge.openSession(); }

ImageSequenceProviderRequestToken ImageViewportPrivate::nextProviderRequestToken()
{
    return providerBridge.nextRequestToken();
}

void ImageViewportPrivate::requestProviderMetadata(ImageSequenceProviderRequestToken token)
{
    providerBridge.requestMetadata(token);
}

void ImageViewportPrivate::requestProviderFrame(ImageSequenceProviderRequestToken token, int frame)
{
    providerBridge.requestFrame(token, frame);
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
    m_queuedProviderFrame = -1;
    m_queuedProviderPosition = -1;
    m_queuedProviderFrameFromPlayback = false;
}

void ImageViewportPrivate::queueProviderFrameRequest(int frame, bool fromPlayback)
{
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::RequestQueued;
    m_displayStatus
        = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    discardPendingRenderCommit();

    if (m_providerSession && m_activeProviderFrameToken.isValid()) {
        cancelProviderRequest(m_activeProviderFrameToken);
    }
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;

    m_queuedProviderFrameRequest = true;
    m_queuedProviderFrameGeneration = m_sequenceGeneration;
    m_queuedProviderFrame = frame;
    m_queuedProviderPosition = m_requestedPosition;
    m_queuedProviderFrameFromPlayback = fromPlayback;
    QMetaObject::invokeMethod(q, [this]() { flushQueuedProviderFrameRequest(); },
        Qt::QueuedConnection);
}

void ImageViewportPrivate::flushQueuedProviderFrameRequest()
{
    if (!m_queuedProviderFrameRequest || !hasProviderSequence() || !m_providerSession) {
        clearQueuedProviderFrameRequest();
        return;
    }

    const int queuedFrame = m_queuedProviderFrame;
    const int queuedPosition = m_queuedProviderPosition;
    const bool queuedFromPlayback = m_queuedProviderFrameFromPlayback;
    const bool stillCurrent = m_queuedProviderFrameGeneration == m_sequenceGeneration
        && m_requestStatus == RequestStatus::Loading
        && m_requestReason == RequestReason::RequestQueued && m_currentFrame == queuedFrame
        && m_requestedPosition == queuedPosition;
    clearQueuedProviderFrameRequest();
    if (!stillCurrent) {
        return;
    }

    startProviderFrameRequest(queuedFrame, queuedFromPlayback);
    incrementRequestRevision();
    emit q->requestStateChanged();
    if (m_requestStatus == RequestStatus::Error
        && m_requestReason == RequestReason::ProviderFailure) {
        emit q->diagnosticsChanged();
    }
}

bool ImageViewportPrivate::startProviderFrameRequest(int frame, bool fromPlayback)
{
    clearQueuedProviderFrameRequest();
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_activeProviderFrameToken = nextProviderRequestToken();
    if (!m_activeProviderFrameToken.isValid()) {
        publishProviderTokenExhaustion();
        return false;
    }

    m_activeProviderFrameFromPlayback = fromPlayback;
    if (m_providerSession) {
        if (fromPlayback) {
            requestProviderPlayback(m_activeProviderFrameToken, frame, m_requestedPosition);
        } else {
            requestProviderFrame(m_activeProviderFrameToken, frame);
        }
    }
    return true;
}

void ImageViewportPrivate::publishProviderTokenExhaustion()
{
    clearQueuedProviderFrameRequest();
    m_activeProviderMetadataToken = {};
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;
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
    if (!hasProviderSequence() || !m_providerSession || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_activeProviderMetadataToken = {};

    const QString metadataViolation = providerMetadataLimitViolation(metadata);
    if (!metadataViolation.isEmpty()) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = metadataViolation;
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        closeProviderSession();
        return;
    }

    const bool isStillMetadata = validateProviderStillMetadata(metadata);
    const bool isTimedMetadata = validateProviderTimedMetadata(metadata);
    if (!isStillMetadata && !isTimedMetadata) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider metadata is invalid");
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        closeProviderSession();
        return;
    }

    if (providerCapabilityContradictsMetadata(
            m_sequence->m_providerTimedPlaybackCapability, isTimedMetadata)
        || providerCapabilityContradictsMetadata(m_sequence->m_providerFrameSeekCapability, true)
        || providerCapabilityContradictsMetadata(
            m_sequence->m_providerPositionSeekCapability, isTimedMetadata)) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString
            = QStringLiteral("provider metadata contradicts construction-time capabilities");
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        closeProviderSession();
        return;
    }

    if (m_sequence->m_hasProviderKnownMetadata) {
        const bool knownTimedMetadata = !m_sequence->m_providerKnownFrameDurations.isEmpty();
        const bool contradictsKnownMetadata = knownTimedMetadata != isTimedMetadata
            || metadata.logicalSize() != m_sequence->m_providerKnownLogicalSize
            || (knownTimedMetadata
                && metadata.frameDurations() != m_sequence->m_providerKnownFrameDurations);
        if (contradictsKnownMetadata) {
            m_requestStatus = RequestStatus::Error;
            m_requestReason = RequestReason::PayloadRejection;
            m_errorString
                = QStringLiteral("provider metadata contradicts construction-time metadata");
            m_providerPlaybackStartPending = false;
            setPlaybackPhase(PlaybackPhase::Stopped);
            incrementRequestRevision();
            emit q->requestStateChanged();
            emit q->diagnosticsChanged();
            closeProviderSession();
            return;
        }
    }

    m_providerMetadataReady = true;
    m_providerTimedMetadata = isTimedMetadata;
    m_providerLogicalSize = metadata.logicalSize();
    m_providerFrameDurations = isTimedMetadata ? metadata.frameDurations() : QVector<int>();
    const bool selectedFromPlaybackStart
        = m_providerPlaybackStartPending && m_currentFrame < 0 && m_requestedPosition < 0;
    int selectedFrame = m_currentFrame >= 0 ? m_currentFrame : 0;
    const int providerFrameCount = isTimedMetadata ? m_providerFrameDurations.size() : 1;
    const bool selectedFromPosition = m_currentFrame < 0 && m_requestedPosition >= 0;
    if (selectedFromPlaybackStart && !isTimedMetadata) {
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = RequestReason::UnsupportedRequest;
        const bool diagnosticsValueChanged = clearDiagnostics();
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        return;
    }
    if (selectedFromPosition) {
        if (!isTimedMetadata) {
            m_requestStatus = RequestStatus::Unsupported;
            m_requestReason = RequestReason::UnsupportedRequest;
            const bool diagnosticsValueChanged = clearDiagnostics();
            setPlaybackPhase(PlaybackPhase::Stopped);
            incrementRequestRevision();
            emit q->requestStateChanged();
            if (diagnosticsValueChanged) {
                emit q->diagnosticsChanged();
            }
            return;
        }
        selectedFrame = providerFrameIndexForPosition(m_requestedPosition);
    }
    if (selectedFrame < 0 || selectedFrame >= providerFrameCount) {
        m_currentFrame = selectedFrame;
        if (!selectedFromPosition) {
            m_requestedPosition = -1;
        }
        m_playbackPosition = -1;
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = RequestReason::InvalidRequest;
        const bool diagnosticsValueChanged = clearDiagnostics();
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        return;
    }

    m_currentFrame = selectedFrame;
    if (!selectedFromPosition) {
        m_requestedPosition = isTimedMetadata ? providerFrameStartPosition(selectedFrame) : -1;
    }
    m_playbackPosition = m_requestedPosition;
    if (m_playbackPhase != PlaybackPhase::Waiting) {
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
    }
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_displayStatus
        = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    discardPendingRenderCommit();

    m_providerPlaybackStartPending = false;
    if (!startProviderFrameRequest(selectedFrame, selectedFromPlaybackStart)) {
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }
    incrementRequestRevision();
    emit q->requestStateChanged();
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
    if (!hasProviderSequence() || !m_providerSession || !m_activeProviderFrameToken.isValid()
        || token != m_activeProviderFrameToken) {
        return;
    }

    if (m_providerMetadataReady && FramePreparation::exceedsPayloadLimit(frame)) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString
            = QStringLiteral("provider frame payload exceeds maximumPayloadBytesPerFrame");
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }

    if (!validateProviderFrame(frame, metadata)) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider frame payload is invalid");
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }

    const bool diagnosticsValueChanged = clearDiagnostics();
    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    publishAcceptedTargetState(frame->imagePayload());
    if (m_playbackPhase == PlaybackPhase::Waiting && m_requestStatus == RequestStatus::Ready
        && !m_renderCommitPending) {
        setPlaybackPhase(
            m_stopPlaybackWhenRequestReady ? PlaybackPhase::Stopped : PlaybackPhase::Playing);
        m_stopPlaybackWhenRequestReady = false;
    }
    incrementRequestRevision();
    incrementDisplayRevision();
    emit q->requestStateChanged();
    emit q->displayStateChanged();
    if (rectsDifferExactly(contentRect(), oldContentRect)
        || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
        emit q->geometryStateChanged();
    }
    if (diagnosticsValueChanged) {
        emit q->diagnosticsChanged();
    }
    update();
}

void ImageViewportPrivate::handleProviderWaiting(ImageSequenceProviderRequestToken token)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    const bool activeMetadataToken = !m_providerMetadataReady
        && m_activeProviderMetadataToken.isValid() && token == m_activeProviderMetadataToken;
    const bool activeFrameToken
        = m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken;
    if ((!activeMetadataToken && !activeFrameToken) || m_requestStatus != RequestStatus::Loading) {
        return;
    }

    if (m_requestReason == RequestReason::ProviderWaiting) {
        return;
    }

    m_requestReason = RequestReason::ProviderWaiting;
    incrementRequestRevision();
    emit q->requestStateChanged();
}

void ImageViewportPrivate::handleProviderProgress(
    ImageSequenceProviderRequestToken token, double progress)
{
    if (!std::isfinite(progress) || progress < 0.0 || progress > 1.0) {
        return;
    }

    handleProviderWaiting(token);
}

void ImageViewportPrivate::handleProviderEndOfSequence(ImageSequenceProviderRequestToken token)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    const bool activeMetadataToken = !m_providerMetadataReady
        && m_activeProviderMetadataToken.isValid() && token == m_activeProviderMetadataToken;
    const bool activeFrameToken
        = m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken;
    if (!activeMetadataToken && !activeFrameToken) {
        return;
    }

    if (activeMetadataToken || !m_providerMetadataReady || !m_providerTimedMetadata
        || !m_activeProviderFrameFromPlayback) {
        clearQueuedProviderFrameRequest();
        if (activeMetadataToken) {
            m_activeProviderMetadataToken = {};
        }
        if (activeFrameToken) {
            m_activeProviderFrameToken = {};
            m_activeProviderFrameFromPlayback = false;
        }
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider protocol violation");
        m_providerPlaybackStartPending = false;
        m_stopPlaybackWhenRequestReady = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        closeProviderSession();
        return;
    }

    m_activeProviderFrameToken = {};
    m_activeProviderFrameFromPlayback = false;
    const bool diagnosticsValueChanged = clearDiagnostics();

    int selectedFrame = 0;
    int selectedPosition = 0;
    if (m_looping) {
        m_stopPlaybackWhenRequestReady = false;
        m_playbackPosition = 0;
    } else {
        selectedFrame = frameCount() - 1;
        selectedPosition = providerFrameStartPosition(selectedFrame);
        m_playbackPosition = totalDuration();
        m_stopPlaybackWhenRequestReady = true;
    }

    m_currentFrame = selectedFrame;
    m_requestedPosition = selectedPosition;

    if (!m_looping && hasReadyDisplay() && m_displayedGeneration == m_sequenceGeneration
        && m_displayedFrame == selectedFrame && m_displayedPosition == selectedPosition) {
        m_requestStatus = RequestStatus::Ready;
        m_requestReason = RequestReason::Ready;
        m_displayStatus = DisplayStatus::Ready;
        setPlaybackPhase(PlaybackPhase::Stopped);
        m_stopPlaybackWhenRequestReady = false;
        incrementRequestRevision();
        incrementDisplayRevision();
        emit q->requestStateChanged();
        emit q->displayStateChanged();
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        update();
        return;
    }

    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_displayStatus
        = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    discardPendingRenderCommit();
    if (!startProviderFrameRequest(selectedFrame, true)) {
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }
    setPlaybackPhase(PlaybackPhase::Waiting);
    incrementRequestRevision();
    incrementDisplayRevision();
    emit q->requestStateChanged();
    emit q->displayStateChanged();
    if (diagnosticsValueChanged) {
        emit q->diagnosticsChanged();
    }
    update();
}

void ImageViewportPrivate::handleProviderFailure(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::ProviderFailure;
        m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider failure"));
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }

    if (m_providerMetadataReady || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_requestStatus = RequestStatus::Error;
    m_requestReason = RequestReason::ProviderFailure;
    m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider failure"));
    m_activeProviderMetadataToken = {};
    m_providerPlaybackStartPending = false;
    setPlaybackPhase(PlaybackPhase::Stopped);
    incrementRequestRevision();
    emit q->requestStateChanged();
    emit q->diagnosticsChanged();
    closeProviderSession();
}

void ImageViewportPrivate::handleProviderUnsupported(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken) {
        clearQueuedProviderFrameRequest();
        const bool unsupportedPlaybackRequest = m_activeProviderFrameFromPlayback;
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason = unsupportedPlaybackRequest ? RequestReason::UnsupportedRequest
                                                     : RequestReason::PayloadRejection;
        m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider unsupported"));
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }

    if (m_providerMetadataReady || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_requestStatus = RequestStatus::Unsupported;
    m_requestReason = RequestReason::UnsupportedRequest;
    m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider unsupported"));
    m_activeProviderMetadataToken = {};
    m_providerPlaybackStartPending = false;
    setPlaybackPhase(PlaybackPhase::Stopped);
    incrementRequestRevision();
    emit q->requestStateChanged();
    emit q->diagnosticsChanged();
    closeProviderSession();
}

void ImageViewportPrivate::handleProviderCancellation(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameFromPlayback = false;
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::ProviderFailure;
        m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider cancelled request"));
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }

    if (m_providerMetadataReady || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_requestStatus = RequestStatus::Error;
    m_requestReason = RequestReason::ProviderFailure;
    m_errorString = boundedDiagnostic(diagnostic, QStringLiteral("provider cancelled request"));
    m_activeProviderMetadataToken = {};
    m_providerPlaybackStartPending = false;
    setPlaybackPhase(PlaybackPhase::Stopped);
    incrementRequestRevision();
    emit q->requestStateChanged();
    emit q->diagnosticsChanged();
    closeProviderSession();
}

bool ImageViewportPrivate::validateProviderStillMetadata(
    const ImageSequenceProviderMetadata& metadata)
{
    return FramePreparation::validateProviderStillMetadata(metadata);
}

bool ImageViewportPrivate::validateProviderTimedMetadata(
    const ImageSequenceProviderMetadata& metadata)
{
    return FramePreparation::validateProviderTimedMetadata(metadata);
}

bool ImageViewportPrivate::validateProviderFrame(
    ImageFrame* frame, ImageSequenceProviderFrameMetadata metadata) const
{
    return FramePreparation::validateProviderFrame(frame, metadata,
        {
            m_providerMetadataReady,
            m_providerTimedMetadata,
            m_providerLogicalSize,
            m_providerFrameDurations,
            m_currentFrame,
        });
}

std::shared_ptr<ImageSequenceProviderSessionFactory>
ImageViewportPrivate::providerSessionFactory() const
{
    return hasProviderSequence() ? m_sequence->m_providerSessionFactory : nullptr;
}

int ImageViewportPrivate::providerFrameStartPosition(int frame) const
{
    if (!m_providerTimedMetadata) {
        return -1;
    }
    return FramePreparation::providerFrameStartPosition(m_providerFrameDurations, frame);
}

int ImageViewportPrivate::providerFrameIndexForPosition(int position) const
{
    if (!m_providerTimedMetadata) {
        return -1;
    }
    return FramePreparation::providerFrameIndexForPosition(m_providerFrameDurations, position);
}
