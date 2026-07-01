#include "imageviewport_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

using namespace ImageViewportInternal;

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    auto preparedPayload = synchronization.preparedPayload;
    if (!preparedPayload.commitPending && hasReadyDisplay()) {
        preparedPayload.image = controller.displayState().displayedImage;
    }
    const bool imagePresent = !preparedPayload.image.isNull();
    QRectF targetRect = currentContentRect().intersected(itemBounds());
    QRectF sourceRect = visibleImageRect();
    if (synchronization.pendingProviderCommit) {
        targetRect
            = contentRectForImageSize(controller.providerLogicalSize()).intersected(itemBounds());
        sourceRect = visibleImageRectForImageSize(controller.providerLogicalSize());
    }

    const RenderAdapter::Output render = renderAdapter.createNode(oldNode,
        {
            QSizeF(width(), height()),
            presentation.backgroundMode,
            presentation.backgroundColor,
            preparedPayload,
            targetRect,
            sourceRect,
            presentation.smoothing,
            presentation.mipmap,
            presentation.mirrorHorizontally,
            presentation.mirrorVertically,
            window(),
        });
    if (render.result == RenderAdapter::CommitResult::Failed) {
        const auto changes = controller.acknowledgeRenderFailure({ render.preparedPayload });
        applyControllerChanges(changes);
        if (changes.playbackPhase) {
            syncPlaybackTimer();
        }
        return nullptr;
    }

    const auto changes = controller.acknowledgeRenderCommit(
        { render.preparedPayload }, imagePresent, synchronization);
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
