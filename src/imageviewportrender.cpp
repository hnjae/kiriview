#include "imageviewport_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

using namespace ImageViewportInternal;

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    const QImage image = synchronization.pendingProviderCommit
        ? m_displayedImage
        : (hasReadyDisplay() ? m_displayedImage : QImage());
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
    if (render.result == RenderAdapter::CommitResult::Failed) {
        const auto changes = controller.acknowledgeRenderFailure(
            { render.requestId, render.preparedPayloadId });
        applyControllerChanges(changes);
        if (changes.playbackPhase) {
            syncPlaybackTimer();
        }
        return nullptr;
    }

    const auto changes = controller.acknowledgeRenderCommit(
        { render.requestId, render.preparedPayloadId }, !image.isNull(), synchronization);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
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

void ImageViewportPrivate::captureRenderFailureRetainedDisplay()
{
    if (!hasReadyDisplay()) {
        clearRenderFailureRetainedDisplay();
        return;
    }

    m_renderFailureRetainedDisplayValid = true;
    request.renderFailureRetainedRequest = request.displayedRequest;
    m_renderFailureRetainedImageSize = m_displayedImageSize;
    m_renderFailureRetainedImage = m_displayedImage;
}

void ImageViewportPrivate::clearRenderFailureRetainedDisplay()
{
    m_renderFailureRetainedDisplayValid = false;
    request.renderFailureRetainedRequest = {};
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
