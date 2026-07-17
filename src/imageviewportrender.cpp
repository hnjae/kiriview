#include "imageviewportrenderhost_p.h"

#include "imageviewport_p.h"
#include "renderadapter_scenegraph_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

ImageViewportRenderHostResult ImageViewportRenderHost::synchronize(
    QSGNode* oldNode, QQuickWindow* window, const ViewportRenderAttempt& attempt)
{
    QVector<RenderAdapter::Input::ImageLayer> imageLayers;
    imageLayers.reserve(attempt.snapshot.imageLayers.size());
    for (const ViewportRenderLayer& layer : attempt.snapshot.imageLayers) {
        imageLayers.append({ layer.role, layer.preparedPayload, layer.targetRect, layer.sourceRect,
            layer.rotationDegrees, layer.mirrorHorizontally, layer.mirrorVertically });
    }
    const bool imagePresent = !imageLayers.isEmpty();

    const RenderAdapter::Input planInput {
        attempt.snapshot.itemSize,
        attempt.snapshot.backgroundMode,
        attempt.snapshot.backgroundColor,
        attempt.snapshot.checkerboardLightColor,
        attempt.snapshot.checkerboardDarkColor,
        attempt.snapshot.checkerboardCellSize,
        attempt.snapshot.smoothing,
        attempt.snapshot.mipmap,
        attempt.snapshot.requiredRoleSet,
        imageLayers,
    };
    const RenderAdapterSceneGraph::Output render
        = RenderAdapterSceneGraph::createNode(renderAdapter, oldNode, { planInput, window });
    QVector<ViewportRenderRolePayload> rolePayloads;
    rolePayloads.reserve(render.rolePayloads.size());
    for (const RenderAdapter::RolePayload& payload : render.rolePayloads) {
        rolePayloads.append({ payload.role, payload.preparedPayload });
    }
    const auto outcome = render.result == RenderAdapter::CommitResult::Committed
        ? ViewportRenderHostFact::Outcome::Committed
        : render.result == RenderAdapter::CommitResult::Failed
        ? ViewportRenderHostFact::Outcome::Failed
        : ViewportRenderHostFact::Outcome::Empty;
    return { render.node,
        { outcome,
            { attempt.snapshot.targetSpread, attempt.snapshot.presentation, std::move(rolePayloads),
                render.failedRole, render.failureCause, attempt.attempt },
            { render.smoothingUnavailable, render.mipmapUnavailable }, imagePresent } };
}

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const auto attemptValue = renderAttemptForHost();
    if (!attemptValue) {
        return oldNode;
    }
    const ViewportRenderAttempt& attempt = *attemptValue;
    if (!window() && attempt.snapshot.requiredRoleSet.primary()) {
        return oldNode;
    }
    ImageViewportRenderHostResult render = renderHost.synchronize(oldNode, window(), attempt);
    if (render.fact.outcome == ViewportRenderHostFact::Outcome::Failed) {
        QSGNode* fallbackNode = render.node;
        applyRenderHostFact(std::move(render.fact));
        if (fallbackNode) {
            return fallbackNode;
        }
        return nullptr;
    }
    applyRenderHostFact(std::move(render.fact));
    return render.node;
}

void ImageViewportPrivate::prepareRenderSynchronization()
{
    const ViewportRenderAttempt attempt = engine.beginRenderSynchronization();
    const QMutexLocker lock(&renderMailboxMutex);
    renderMailbox = attempt;
    renderMailboxValid = true;
}

std::optional<ViewportRenderAttempt> ImageViewportPrivate::renderAttemptForHost() const
{
    const QMutexLocker lock(&renderMailboxMutex);
    if (!renderMailboxValid) {
        return std::nullopt;
    }
    return renderMailbox;
}

void ImageViewportPrivate::applyRenderHostFact(ViewportRenderHostFact fact)
{
    auto apply = [this, fact = std::move(fact)]() mutable {
        applyEngineTransition(engine.handleRenderHostFact({ std::move(fact) }));
    };
    if (QThread::currentThread() == q->thread()) {
        apply();
        return;
    }
    QMetaObject::invokeMethod(q, std::move(apply), Qt::QueuedConnection);
}
