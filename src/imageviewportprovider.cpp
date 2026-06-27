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

void ImageViewportPrivate::queueProviderFrameRequest(
    int frame, ProviderRequestTargetKind targetKind)
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
    m_activeProviderFrameRequestId = 0;
    m_activeProviderFrameFromPlayback = false;
    m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;

    m_queuedProviderFrameRequest = true;
    m_queuedProviderFrameGeneration = m_sequenceGeneration;
    m_queuedProviderFrameRequestId = m_activeRequestId;
    m_queuedProviderFrame = frame;
    m_queuedProviderPosition = m_requestedPosition;
    m_queuedProviderFrameFromPlayback = targetKind == ProviderRequestTargetKind::Playback;
    m_queuedProviderFrameTargetKind = targetKind;
    QMetaObject::invokeMethod(
        q, [this]() { flushQueuedProviderFrameRequest(); }, Qt::QueuedConnection);
}

void ImageViewportPrivate::flushQueuedProviderFrameRequest()
{
    if (!m_queuedProviderFrameRequest || !hasProviderSequence() || !m_providerSession) {
        clearQueuedProviderFrameRequest();
        return;
    }

    const int queuedFrame = m_queuedProviderFrame;
    const int queuedPosition = m_queuedProviderPosition;
    const quint64 queuedRequestId = m_queuedProviderFrameRequestId;
    const ProviderRequestTargetKind queuedTargetKind = m_queuedProviderFrameTargetKind;
    const bool stillCurrent = m_queuedProviderFrameGeneration == m_sequenceGeneration
        && queuedRequestId == m_activeRequestId
        && m_requestStatus == RequestStatus::Loading
        && m_requestReason == RequestReason::RequestQueued && m_currentFrame == queuedFrame
        && m_requestedPosition == queuedPosition && m_currentProviderTargetKind == queuedTargetKind;
    clearQueuedProviderFrameRequest();
    if (!stillCurrent) {
        return;
    }

    startProviderFrameRequest(queuedFrame, queuedTargetKind);
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
    clearQueuedProviderFrameRequest();
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_activeProviderFrameToken = nextProviderRequestToken();
    m_activeProviderFrameRequestId = m_activeRequestId;
    if (!m_activeProviderFrameToken.isValid()) {
        publishProviderTokenExhaustion();
        return false;
    }

    m_activeProviderFrameTargetKind = targetKind;
    m_activeProviderFrameFromPlayback = targetKind == ProviderRequestTargetKind::Playback;
    if (m_providerSession) {
        if (targetKind == ProviderRequestTargetKind::Playback) {
            requestProviderPlayback(m_activeProviderFrameToken, frame, m_requestedPosition);
        } else if (targetKind == ProviderRequestTargetKind::Position) {
            requestProviderPosition(m_activeProviderFrameToken, frame, m_requestedPosition);
        } else {
            requestProviderFrame(m_activeProviderFrameToken, frame);
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
    if (!hasProviderSequence() || !m_providerSession || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    m_activeProviderMetadataToken = {};

    const auto metadataAdmission = FramePreparation::admitProviderMetadata(metadata);
    if (!metadataAdmission.accepted()) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = metadataAdmission.diagnostic;
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        closeProviderSession();
        return;
    }

    if (providerCapabilityContradictsMetadata(
            m_sequence->m_providerTimedPlaybackCapability, metadata.timedPlaybackSupport())
        || providerCapabilityContradictsMetadata(
            m_sequence->m_providerFrameSeekCapability, metadata.frameSeekSupport())
        || providerCapabilityContradictsMetadata(
            m_sequence->m_providerPositionSeekCapability, metadata.positionSeekSupport())) {
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

    if (providerFactsContradictMetadata(m_sequence->m_providerKnownFacts, metadata)) {
        m_requestStatus = RequestStatus::Error;
        m_requestReason = RequestReason::PayloadRejection;
        m_errorString = QStringLiteral("provider metadata contradicts construction-time facts");
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        closeProviderSession();
        return;
    }

    m_providerMetadataReady = true;
    m_providerTimedMetadata = metadataAdmission.timedMetadata;
    m_providerTimedPlaybackSupport = metadata.timedPlaybackSupport();
    m_providerFrameSeekSupport = metadata.frameSeekSupport();
    m_providerPositionSeekSupport = metadata.positionSeekSupport();
    m_providerLogicalSize = metadataAdmission.logicalSize;
    m_providerTimingIntervals = metadataAdmission.timingIntervals;
    const bool selectedFromPlaybackStart = m_providerPlaybackStartPending
        && m_currentProviderTargetKind == ProviderRequestTargetKind::Playback;
    const bool selectedFromPosition
        = m_currentProviderTargetKind == ProviderRequestTargetKind::Position;
    ProviderRequestTargetKind requestTargetKind = selectedFromPlaybackStart
        ? ProviderRequestTargetKind::Playback
        : (selectedFromPosition ? ProviderRequestTargetKind::Position
                                : ProviderRequestTargetKind::Frame);
    int selectedFrame = m_currentFrame >= 0 ? m_currentFrame : 0;
    const int providerFrameCount
        = metadataAdmission.timedMetadata ? m_providerTimingIntervals.frameCount() : 1;
    if (selectedFromPlaybackStart
        && (!metadataAdmission.timedMetadata || !m_providerTimedPlaybackSupport)) {
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
        if (!metadataAdmission.timedMetadata || !m_providerPositionSeekSupport) {
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

    beginDisplayRequest(
        DisplayRequestOrigin::MetadataBoundSelection,
        requestTargetKind != ProviderRequestTargetKind::Playback);
    m_currentFrame = selectedFrame;
    if (!selectedFromPosition) {
        m_requestedPosition
            = metadataAdmission.timedMetadata ? providerFrameStartPosition(selectedFrame) : -1;
    }
    m_playbackPosition = m_requestedPosition;
    m_currentProviderTargetKind = requestTargetKind;
    if (requestTargetKind != ProviderRequestTargetKind::Playback) {
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        m_latestNonPlaybackProviderTargetKind = requestTargetKind;
    }
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::ProviderWaiting;
    m_displayStatus
        = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    discardPendingRenderCommit();

    m_providerPlaybackStartPending = false;
    if (!startProviderFrameRequest(selectedFrame, requestTargetKind)) {
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
        || token != m_activeProviderFrameToken
        || m_activeProviderFrameRequestId != m_activeRequestId) {
        return;
    }

    const auto admission = FramePreparation::admitProviderFrame(frame, metadata,
        {
            m_providerMetadataReady,
            m_providerTimedMetadata,
            m_providerLogicalSize,
            m_providerTimingIntervals,
            m_currentFrame,
        });
    if (!admission.accepted()) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameRequestId = 0;
        m_activeProviderFrameFromPlayback = false;
        m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
        m_requestStatus = admission.status;
        m_requestReason = admission.reason;
        m_errorString = admission.diagnostic;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        emit q->diagnosticsChanged();
        return;
    }

    const bool diagnosticsValueChanged = clearDiagnostics();
    m_activeProviderFrameToken = {};
    m_activeProviderFrameRequestId = 0;
    m_activeProviderFrameFromPlayback = false;
    m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
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
        = m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken
        && m_activeProviderFrameRequestId == m_activeRequestId;
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
        = m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken
        && m_activeProviderFrameRequestId == m_activeRequestId;
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
            m_activeProviderFrameRequestId = 0;
            m_activeProviderFrameFromPlayback = false;
            m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
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
    m_activeProviderFrameRequestId = 0;
    m_activeProviderFrameFromPlayback = false;
    m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
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
    m_currentProviderTargetKind = ProviderRequestTargetKind::Playback;

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
    if (!startProviderFrameRequest(selectedFrame, ProviderRequestTargetKind::Playback)) {
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

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken
        && m_activeProviderFrameRequestId == m_activeRequestId) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameRequestId = 0;
        m_activeProviderFrameFromPlayback = false;
        m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
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

void ImageViewportPrivate::handleProviderUnsupported(ImageSequenceProviderRequestToken token,
    ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken
        && m_activeProviderFrameRequestId == m_activeRequestId) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameRequestId = 0;
        m_activeProviderFrameFromPlayback = false;
        m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
        m_requestStatus = RequestStatus::Unsupported;
        m_requestReason
            = cause == ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest
            ? RequestReason::UnsupportedRequest
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

    if (m_activeProviderFrameToken.isValid() && token == m_activeProviderFrameToken
        && m_activeProviderFrameRequestId == m_activeRequestId) {
        clearQueuedProviderFrameRequest();
        m_activeProviderFrameToken = {};
        m_activeProviderFrameRequestId = 0;
        m_activeProviderFrameFromPlayback = false;
        m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
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
