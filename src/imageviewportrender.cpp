#include "imageviewport_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

using namespace ImageViewportInternal;

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const bool hasPendingProviderCommit = hasProviderSequence()
        && m_requestStatus == RequestStatus::Loading
        && (m_requestReason == RequestReason::UploadPending
            || m_requestReason == RequestReason::RenderWaiting)
        && m_renderCommitPending && !m_pendingDisplayImage.isNull() && !itemBounds().isEmpty();
    const QImage image = hasPendingProviderCommit
        ? m_pendingDisplayImage
        : (hasReadyDisplay() ? m_displayedImage : QImage());

    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    const DisplayStatus oldDisplayStatus = m_displayStatus;
    if (hasPendingProviderCommit) {
        publishSequenceReadyState(m_pendingDisplayImage);
    }
    const quint64 renderRequestId = m_renderCommitPending ? m_pendingRenderRequestId : 0;
    const quint64 renderPreparedPayloadId
        = m_renderCommitPending ? m_pendingPreparedPayloadId : 0;

    const RenderAdapter::Output render = renderAdapter.createNode(oldNode,
        {
            QSizeF(width(), height()),
            m_backgroundMode,
            m_backgroundColor,
            image,
            currentContentRect().intersected(itemBounds()),
            visibleImageRect(),
            m_smoothing,
            m_mipmap,
            m_mirrorHorizontally,
            m_mirrorVertically,
            renderRequestId,
            renderPreparedPayloadId,
            window(),
        });
    const bool renderMatchesPending = m_renderCommitPending
        && render.requestId == m_pendingRenderRequestId
        && render.preparedPayloadId == m_pendingPreparedPayloadId
        && render.preparedPayloadId == request.activeRequest.preparedPayloadId;
    if (render.result == RenderAdapter::CommitResult::Failed) {
        if (m_displayStatus == DisplayStatus::Ready && renderMatchesPending) {
            reportRenderFailure();
        }
        return nullptr;
    }

    const bool resumePlaybackAfterCommit = !image.isNull() && renderMatchesPending
        && m_playbackPhase == PlaybackPhase::Waiting && m_requestStatus == RequestStatus::Ready;
    if (!image.isNull()) {
        if (renderMatchesPending) {
            commitDisplayedRequestIdentity();
            m_renderCommitPending = false;
            clearPendingRenderIdentity();
        }
        clearRenderFailureRetainedDisplay();
        if (resumePlaybackAfterCommit) {
            setPlaybackPhase(
                m_stopPlaybackWhenRequestReady ? PlaybackPhase::Stopped : PlaybackPhase::Playing);
            m_stopPlaybackWhenRequestReady = false;
        }
        if (hasPendingProviderCommit) {
            incrementRequestRevision();
            incrementDisplayRevision();
            emit q->requestStateChanged();
            if (m_displayStatus != oldDisplayStatus) {
                emit q->displayStateChanged();
            }
            if (rectsDifferExactly(contentRect(), oldContentRect)
                || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
                emit q->geometryStateChanged();
            }
        }
    }
    return render.node;
}

void ImageViewportPrivate::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry,
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    if (newGeometry.width() == oldGeometry.width()
        && newGeometry.height() == oldGeometry.height()) {
        return;
    }

    bool displayRevisionChanged = false;
    if (hasDisplayableSequence() && m_requestStatus == RequestStatus::Loading
        && (m_requestReason == RequestReason::UploadPending
            || m_requestReason == RequestReason::RenderWaiting)
        && newGeometry.width() > 0.0 && newGeometry.height() > 0.0) {
        if (hasProviderSequence() && !m_pendingDisplayImage.isNull()) {
            update();
            return;
        }
        publishSequenceReadyState();
        if (m_playbackPhase == PlaybackPhase::Waiting) {
            setPlaybackPhase(
                m_stopPlaybackWhenRequestReady ? PlaybackPhase::Stopped : PlaybackPhase::Playing);
            m_stopPlaybackWhenRequestReady = false;
        }
        incrementRequestRevision();
        incrementDisplayRevision();
        displayRevisionChanged = true;
        emit q->requestStateChanged();
        emit q->displayStateChanged();
    } else if (hasProviderSequence() && m_requestStatus == RequestStatus::Loading
        && m_requestReason == RequestReason::UploadPending
        && (newGeometry.width() <= 0.0 || newGeometry.height() <= 0.0)
        && !m_pendingDisplayImage.isNull()) {
        m_requestReason = RequestReason::RenderWaiting;
        incrementRequestRevision();
        emit q->requestStateChanged();
    } else if (hasReadyDisplay()) {
        incrementDisplayRevision();
        displayRevisionChanged = true;
    }

    if (!displayRevisionChanged) {
        incrementDisplayRevision();
    }

    if (rectsDifferExactly(contentRect(), oldContentRect)
        || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
        emit q->geometryStateChanged();
    }
    update();
}

void ImageViewportPrivate::reportRenderFailure()
{
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    const DisplayStatus oldDisplayStatus = m_displayStatus;

    m_requestStatus = RequestStatus::Error;
    m_requestReason = RequestReason::RenderFailure;
    m_renderCommitPending = false;
    clearPendingRenderIdentity();
    if (m_renderFailureRetainedDisplayValid) {
        m_displayStatus = DisplayStatus::Retained;
        m_displayedFrame = m_renderFailureRetainedFrame;
        m_displayedPosition = m_renderFailureRetainedPosition;
        m_displayedGeneration = m_renderFailureRetainedGeneration;
        m_displayedRequestId = m_renderFailureRetainedRequestId;
        m_displayedPreparedPayloadId = m_renderFailureRetainedPreparedPayloadId;
        m_displayedImageSize = m_renderFailureRetainedImageSize;
        m_displayedImage = m_renderFailureRetainedImage;
    } else {
        m_displayStatus = DisplayStatus::Empty;
        m_displayedFrame = -1;
        m_displayedPosition = -1;
        m_displayedGeneration = 0;
        m_displayedRequestId = 0;
        m_displayedPreparedPayloadId = 0;
        m_displayedImageSize = {};
        m_displayedImage = {};
    }
    m_pendingDisplayImage = {};
    clearRenderFailureRetainedDisplay();
    m_errorString = QStringLiteral("render commit failed");
    setPlaybackPhase(PlaybackPhase::Stopped);

    incrementRequestRevision();
    incrementDisplayRevision();
    emit q->requestStateChanged();
    if (m_displayStatus != oldDisplayStatus) {
        emit q->displayStateChanged();
    }
    if (rectsDifferExactly(contentRect(), oldContentRect)
        || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
        emit q->geometryStateChanged();
    }
    emit q->diagnosticsChanged();
}

void ImageViewportPrivate::captureRenderFailureRetainedDisplay()
{
    if (!hasReadyDisplay()) {
        clearRenderFailureRetainedDisplay();
        return;
    }

    m_renderFailureRetainedDisplayValid = true;
    m_renderFailureRetainedFrame = m_displayedFrame;
    m_renderFailureRetainedPosition = m_displayedPosition;
    m_renderFailureRetainedGeneration = m_displayedGeneration;
    m_renderFailureRetainedRequestId = m_displayedRequestId;
    m_renderFailureRetainedPreparedPayloadId = m_displayedPreparedPayloadId;
    m_renderFailureRetainedImageSize = m_displayedImageSize;
    m_renderFailureRetainedImage = m_displayedImage;
}

void ImageViewportPrivate::clearRenderFailureRetainedDisplay()
{
    m_renderFailureRetainedDisplayValid = false;
    m_renderFailureRetainedFrame = -1;
    m_renderFailureRetainedPosition = -1;
    m_renderFailureRetainedGeneration = 0;
    m_renderFailureRetainedRequestId = 0;
    m_renderFailureRetainedPreparedPayloadId = 0;
    m_renderFailureRetainedImageSize = {};
    m_renderFailureRetainedImage = {};
}

void ImageViewportPrivate::discardPendingRenderCommit()
{
    m_pendingDisplayImage = {};
    m_renderCommitPending = false;
    clearPendingRenderIdentity();
    clearRenderFailureRetainedDisplay();
}
