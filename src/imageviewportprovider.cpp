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

void applyProviderFrameTerminalResult(
    ImageViewportPrivate& viewport, const ViewportProviderFrameTerminalResult& result)
{
    const auto changes = viewport.controller.handleProviderFrameTerminalResult(result);
    viewport.applyControllerChanges(changes);
    if (changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
}

void applyProviderMetadataTerminalResult(
    ImageViewportPrivate& viewport, const ViewportProviderMetadataTerminalResult& result)
{
    const auto changes = viewport.controller.handleProviderMetadataTerminalResult(result);
    viewport.applyControllerChanges(changes);
    if (changes.playbackPhase) {
        viewport.syncPlaybackTimer();
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

void applyProviderMetadataAdmissionRejection(
    ImageViewportPrivate& viewport, const QString& diagnostic)
{
    const auto changes
        = viewport.controller.handleProviderMetadataAdmissionRejection({ diagnostic });
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
}

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
    m_queuedProviderFrameGeneration = request.sequenceGeneration;
    m_queuedProviderFrameRequestId = request.activeRequest.identity.id;
    m_queuedProviderFrame = frame;
    m_queuedProviderPosition = request.activeRequest.target.position;
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
    const bool stillCurrent = m_queuedProviderFrameGeneration == request.sequenceGeneration
        && queuedRequestId == request.activeRequest.identity.id
        && m_requestStatus == RequestStatus::Loading
        && m_requestReason == RequestReason::RequestQueued
        && request.activeRequest.target.frame == queuedFrame
        && request.activeRequest.target.position == queuedPosition
        && request.activeRequest.target.providerTargetKind == queuedTargetKind;
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
    m_activeProviderFrameRequestId = request.activeRequest.identity.id;
    if (!m_activeProviderFrameToken.isValid()) {
        publishProviderTokenExhaustion();
        return false;
    }

    request.activeRequest.providerFrameToken = m_activeProviderFrameToken;
    m_activeProviderFrameTargetKind = targetKind;
    m_activeProviderFrameFromPlayback = targetKind == ProviderRequestTargetKind::Playback;
    if (m_providerSession) {
        if (targetKind == ProviderRequestTargetKind::Playback) {
            requestProviderPlayback(
                m_activeProviderFrameToken, frame, request.activeRequest.target.position);
        } else if (targetKind == ProviderRequestTargetKind::Position) {
            requestProviderPosition(
                m_activeProviderFrameToken, frame, request.activeRequest.target.position);
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
        applyProviderMetadataAdmissionRejection(*this, metadataAdmission.diagnostic);
        closeProviderSession();
        return;
    }

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

    m_providerMetadataReady = true;
    m_providerTimedMetadata = metadataAdmission.timedMetadata;
    m_providerTimedPlaybackSupport = metadata.timedPlaybackSupport();
    m_providerFrameSeekSupport = metadata.frameSeekSupport();
    m_providerPositionSeekSupport = metadata.positionSeekSupport();
    m_providerLogicalSize = metadataAdmission.logicalSize;
    m_providerTimingIntervals = metadataAdmission.timingIntervals;
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
        = metadataAdmission.timedMetadata ? m_providerTimingIntervals.frameCount() : 1;
    if (selectedFromPlaybackStart
        && (!metadataAdmission.timedMetadata || !m_providerTimedPlaybackSupport)) {
        applyProviderMetadataTargetRejection(
            *this,
            {RequestStatus::Unsupported, RequestReason::UnsupportedRequest, -1, false, false,
                true});
        return;
    }
    if (selectedFromPosition) {
        if (!metadataAdmission.timedMetadata || !m_providerPositionSeekSupport) {
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

    beginDisplayRequest(
        DisplayRequestOrigin::MetadataBoundSelection,
        requestTargetKind != ProviderRequestTargetKind::Playback);
    request.activeRequest.target.frame = selectedFrame;
    if (!selectedFromPosition) {
        request.activeRequest.target.position
            = metadataAdmission.timedMetadata ? providerFrameStartPosition(selectedFrame) : -1;
    }
    request.playbackPosition = request.activeRequest.target.position;
    request.activeRequest.target.providerTargetKind = requestTargetKind;
    if (requestTargetKind != ProviderRequestTargetKind::Playback) {
        request.latestNonPlaybackRequest.target.frame = request.activeRequest.target.frame;
        request.latestNonPlaybackRequest.target.position = request.activeRequest.target.position;
        request.latestNonPlaybackRequest.target.providerTargetKind = requestTargetKind;
    }
    publishProviderFrameLoadingState();

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
    if (!hasProviderSequence() || !m_providerSession
        || !activeProviderFrameTokenMatchesActiveRequest(*this, token)) {
        return;
    }

    const auto admission = FramePreparation::admitProviderFrame(
        frame, metadata, controller.providerFramePreparationState());
    const auto changes = controller.handleProviderFrameAdmission(admission);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::handleProviderWaiting(ImageSequenceProviderRequestToken token)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    const bool activeMetadataToken = !m_providerMetadataReady
        && m_activeProviderMetadataToken.isValid() && token == m_activeProviderMetadataToken;
    const bool activeFrameToken = activeProviderFrameTokenMatchesActiveRequest(*this, token);
    if (!activeMetadataToken && !activeFrameToken) {
        return;
    }

    applyControllerChanges(controller.handleProviderWaiting());
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
    const bool activeFrameToken = activeProviderFrameTokenMatchesActiveRequest(*this, token);
    if (!activeMetadataToken && !activeFrameToken) {
        return;
    }

    if (activeMetadataToken || !m_providerMetadataReady || !m_providerTimedMetadata
        || !m_activeProviderFrameFromPlayback) {
        const auto changes = controller.handleProviderEndOfSequenceProtocolViolation(
            {activeMetadataToken, activeFrameToken});
        applyControllerChanges(changes);
        if (changes.playbackPhase) {
            syncPlaybackTimer();
        }
        closeProviderSession();
        return;
    }

    const auto changes = controller.handleProviderPlaybackEndOfSequence();
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::handleProviderFailure(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (activeProviderFrameTokenMatchesActiveRequest(*this, token)) {
        applyProviderFrameTerminalResult(*this,
            {
                RequestStatus::Error,
                RequestReason::ProviderFailure,
                diagnostic,
                QStringLiteral("provider failure"),
            });
        return;
    }

    if (m_providerMetadataReady || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    applyProviderMetadataTerminalResult(*this,
        {
            RequestStatus::Error,
            RequestReason::ProviderFailure,
            diagnostic,
            QStringLiteral("provider failure"),
        });
    closeProviderSession();
}

void ImageViewportPrivate::handleProviderUnsupported(ImageSequenceProviderRequestToken token,
    ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (activeProviderFrameTokenMatchesActiveRequest(*this, token)) {
        applyProviderFrameTerminalResult(*this,
            {
                RequestStatus::Unsupported,
                cause == ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest
                    ? RequestReason::UnsupportedRequest
                    : RequestReason::PayloadRejection,
                diagnostic,
                QStringLiteral("provider unsupported"),
            });
        return;
    }

    if (m_providerMetadataReady || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    applyProviderMetadataTerminalResult(*this,
        {
            RequestStatus::Unsupported,
            RequestReason::UnsupportedRequest,
            diagnostic,
            QStringLiteral("provider unsupported"),
        });
    closeProviderSession();
}

void ImageViewportPrivate::handleProviderCancellation(
    ImageSequenceProviderRequestToken token, const QString& diagnostic)
{
    if (!hasProviderSequence() || !m_providerSession) {
        return;
    }

    if (activeProviderFrameTokenMatchesActiveRequest(*this, token)) {
        applyProviderFrameTerminalResult(*this,
            {
                RequestStatus::Error,
                RequestReason::ProviderFailure,
                diagnostic,
                QStringLiteral("provider cancelled request"),
            });
        return;
    }

    if (m_providerMetadataReady || !m_activeProviderMetadataToken.isValid()
        || token != m_activeProviderMetadataToken) {
        return;
    }

    applyProviderMetadataTerminalResult(*this,
        {
            RequestStatus::Error,
            RequestReason::ProviderFailure,
            diagnostic,
            QStringLiteral("provider cancelled request"),
        });
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
