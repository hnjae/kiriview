#include "imageviewport_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

namespace {

double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}

}

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization
        = controller.beginRenderSynchronization(effectiveDevicePixelRatio(*this));
    QVector<RenderAdapter::Input::ImageLayer> imageLayers;
    imageLayers.reserve(synchronization.renderSnapshot.imageLayers.size());
    for (const ViewportRenderLayer& layer : synchronization.renderSnapshot.imageLayers) {
        imageLayers.append({ layer.preparedPayload, layer.targetRect, layer.sourceRect,
            layer.mirrorHorizontally, layer.mirrorVertically });
    }
    const bool imagePresent = !imageLayers.isEmpty();

    const RenderAdapter::Output render = renderAdapter.createNode(oldNode,
        {
            synchronization.renderSnapshot.itemSize,
            synchronization.renderSnapshot.backgroundMode,
            synchronization.renderSnapshot.backgroundColor,
            synchronization.renderSnapshot.preparedPayload,
            synchronization.renderSnapshot.targetRect,
            synchronization.renderSnapshot.sourceRect,
            synchronization.renderSnapshot.smoothing,
            synchronization.renderSnapshot.mipmap,
            synchronization.renderSnapshot.mirrorHorizontally,
            synchronization.renderSnapshot.mirrorVertically,
            imageLayers,
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

    if (render.result == RenderAdapter::CommitResult::Committed) {
        const auto changes = controller.acknowledgeRenderCommit(
            { render.preparedPayload }, imagePresent, synchronization);
        applyControllerChanges(changes);
        if (changes.playbackPhase) {
            syncPlaybackTimer();
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

    const auto changes = controller.handleGeometryChanged(oldContentRect, oldVisibleImageRect);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}
