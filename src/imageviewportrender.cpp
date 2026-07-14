#include "imageviewportrenderhost_p.h"

#include "imageviewport_p.h"
#include "renderadapter_scenegraph_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

namespace {

double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}

}

ImageViewportRenderHostResult ImageViewportRenderHost::synchronize(QSGNode* oldNode,
    QQuickWindow* window, const ViewportRenderSynchronization& synchronization)
{
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
    const RenderAdapterSceneGraph::Output render
        = RenderAdapterSceneGraph::createNode(renderAdapter, oldNode, { planInput, window });
    QVector<ViewportRenderRolePayload> rolePayloads;
    rolePayloads.reserve(render.rolePayloads.size());
    for (const RenderAdapter::RolePayload& payload : render.rolePayloads) {
        rolePayloads.append({ payload.role, payload.preparedPayload });
    }
    return { render.node, render.result,
        { render.preparedPayload, std::move(rolePayloads), render.failedRole,
            render.failureCause, synchronization.attempt },
        imagePresent };
}

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization
        = engine.beginRenderSynchronization(
            { { itemBounds(), effectiveDevicePixelRatio(*this) } });
    ImageViewportRenderHostResult render
        = renderHost.synchronize(oldNode, window(), synchronization);
    if (render.result == RenderAdapter::CommitResult::Failed) {
        QSGNode* fallbackNode = render.node;
        const auto reduced
            = engine.acknowledgeRenderFailure({ render.acknowledgement });
        ViewportEngineTransition transition;
        transition.changes = reduced.changes;
        transition.playbackSchedule = reduced.playbackSchedule;
        applyEngineTransition(std::move(transition));
        if (fallbackNode && displayStatus() != ImageViewportDisplayStatus::Empty) {
            return fallbackNode;
        }
        delete fallbackNode;
        return nullptr;
    }
    if (render.result == RenderAdapter::CommitResult::Committed) {
        const auto reduced = engine.acknowledgeRenderCommit({ render.acknowledgement,
            render.imagePresent, synchronization.attempt, synchronization.pendingTargetCommit,
            synchronization.pendingSecondaryProviderCommit, synchronization.preparedPayload,
            synchronization.oldDisplayStatus, synchronization.oldContentRect,
            synchronization.oldVisibleImageRect, synchronization.geometryState });
        ViewportEngineTransition transition;
        transition.changes = reduced.changes;
        transition.playbackSchedule = reduced.playbackSchedule;
        applyEngineTransition(std::move(transition));
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
    const auto reduced = engine.handleGeometryChanged(
        { { itemBounds(), 1.0 }, oldContentRect, oldVisibleImageRect });
    ViewportEngineTransition transition;
    transition.changes = reduced.changes;
    appendProviderTransport(transition.providerAfterPublication, reduced.providerEffects[0],
        PageRole::Primary);
    appendProviderTransport(transition.providerAfterPublication, reduced.providerEffects[1],
        PageRole::Secondary);
    applyEngineTransition(std::move(transition));
}
