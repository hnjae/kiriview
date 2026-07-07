#include "imageviewportrenderhost_p.h"

#include "imageviewport_p.h"
#include "renderadapter_scenegraph_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

namespace {

double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}

}

ImageViewportRenderHost::ImageViewportRenderHost(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
}

QSGNode* ImageViewportRenderHost::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization
        = viewport.controller.beginRenderSynchronization(effectiveDevicePixelRatio(viewport));
    QVector<RenderAdapter::Input::ImageLayer> imageLayers;
    imageLayers.reserve(synchronization.renderSnapshot.imageLayers.size());
    for (const ViewportRenderLayer& layer : synchronization.renderSnapshot.imageLayers) {
        imageLayers.append({ layer.role, layer.preparedPayload, layer.targetRect, layer.sourceRect,
            layer.rotationDegrees, layer.mirrorHorizontally, layer.mirrorVertically });
    }
    const bool imagePresent = !imageLayers.isEmpty();

    const RenderAdapter::Input planInput {
        synchronization.renderSnapshot.itemSize,
        synchronization.renderSnapshot.backgroundMode,
        synchronization.renderSnapshot.backgroundColor,
        synchronization.renderSnapshot.preparedPayload,
        synchronization.renderSnapshot.targetRect,
        synchronization.renderSnapshot.sourceRect,
        synchronization.renderSnapshot.rotationDegrees,
        synchronization.renderSnapshot.smoothing,
        synchronization.renderSnapshot.mipmap,
        synchronization.renderSnapshot.mirrorHorizontally,
        synchronization.renderSnapshot.mirrorVertically,
        imageLayers,
    };
    const RenderAdapterSceneGraph::Output render = RenderAdapterSceneGraph::createNode(
        renderAdapter, oldNode, { planInput, viewport.window() });
    if (render.result == RenderAdapter::CommitResult::Failed) {
        QSGNode* fallbackNode = render.node;
        QVector<ViewportRenderRolePayload> rolePayloads;
        rolePayloads.reserve(render.rolePayloads.size());
        for (const RenderAdapter::RolePayload& payload : render.rolePayloads) {
            rolePayloads.append({ payload.role, payload.preparedPayload });
        }
        const auto changes = viewport.controller.acknowledgeRenderFailure(
            { render.preparedPayload, rolePayloads, render.failedRole, render.failureCause });
        viewport.applyControllerChanges(changes);
        if (changes.playbackPhase) {
            viewport.syncPlaybackTimer();
        }
        if (fallbackNode && viewport.displayStatus() != ImageViewport::DisplayStatus::Empty) {
            return fallbackNode;
        }
        delete fallbackNode;
        return nullptr;
    }

    if (render.result == RenderAdapter::CommitResult::Committed) {
        QVector<ViewportRenderRolePayload> rolePayloads;
        rolePayloads.reserve(render.rolePayloads.size());
        for (const RenderAdapter::RolePayload& payload : render.rolePayloads) {
            rolePayloads.append({ payload.role, payload.preparedPayload });
        }
        const auto changes = viewport.controller.acknowledgeRenderCommit(
            { render.preparedPayload, rolePayloads }, imagePresent, synchronization);
        viewport.applyControllerChanges(changes);
        if (changes.playbackPhase) {
            viewport.syncPlaybackTimer();
        }
    }
    return render.node;
}

void ImageViewportRenderHost::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry,
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    if (newGeometry.width() == oldGeometry.width()
        && newGeometry.height() == oldGeometry.height()) {
        return;
    }

    const auto changes
        = viewport.controller.handleGeometryChanged(oldContentRect, oldVisibleImageRect);
    viewport.applyControllerChanges(changes);
    if (changes.playbackPhase) {
        viewport.syncPlaybackTimer();
    }
}
