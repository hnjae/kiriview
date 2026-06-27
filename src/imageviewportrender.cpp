#include "imageviewport_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

using namespace ImageViewportInternal;

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    auto preparedPayload = synchronization.preparedPayload;
    if (!preparedPayload.commitPending && hasReadyDisplay()) {
        preparedPayload.image = m_displayedImage;
    }
    const bool imagePresent = !preparedPayload.image.isNull();
    QRectF targetRect = currentContentRect().intersected(itemBounds());
    QRectF sourceRect = visibleImageRect();
    if (synchronization.pendingProviderCommit) {
        targetRect = contentRectForImageSize(m_providerLogicalSize).intersected(itemBounds());
        sourceRect = visibleImageRectForImageSize(m_providerLogicalSize);
    }

    const RenderAdapter::Output render = renderAdapter.createNode(oldNode,
        {
            QSizeF(width(), height()),
            m_backgroundMode,
            m_backgroundColor,
            preparedPayload,
            targetRect,
            sourceRect,
            m_smoothing,
            m_mipmap,
            m_mirrorHorizontally,
            m_mirrorVertically,
            window(),
        });
    if (render.result == RenderAdapter::CommitResult::Failed) {
        const auto changes = controller.acknowledgeRenderFailure(
            { render.generation, render.requestId, render.preparedPayloadId });
        applyControllerChanges(changes);
        if (changes.playbackPhase) {
            syncPlaybackTimer();
        }
        return nullptr;
    }

    const auto changes = controller.acknowledgeRenderCommit(
        { render.generation, render.requestId, render.preparedPayloadId }, imagePresent,
        synchronization);
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

    const auto changes = controller.handleGeometryChanged(oldContentRect, oldVisibleImageRect);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
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
    clearPendingRenderIdentity();
    clearRenderFailureRetainedDisplay();
}
