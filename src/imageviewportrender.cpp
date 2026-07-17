#include "imageviewportrenderhost_p.h"

#include "imageviewport_p.h"
#include "renderadapter_scenegraph_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

namespace {

double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}

}

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
            { std::move(rolePayloads), render.failedRole, render.failureCause, attempt.attempt },
            { render.smoothingUnavailable, render.mipmapUnavailable }, imagePresent } };
}

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const auto attemptValue = renderAttemptForHost();
    if (!attemptValue) {
        return oldNode;
    }
    const ViewportRenderAttempt& attempt = *attemptValue;
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
    const ViewportRenderAttempt attempt
        = engine.beginRenderSynchronization({ { itemBounds(), effectiveDevicePixelRatio(*this) } });
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
        const auto reduced = engine.handleRenderHostFact({ std::move(fact) });
        ViewportEngineTransition transition;
        transition.changes = reduced.changes;
        transition.playbackSchedule = reduced.playbackSchedule;
        transition.observations = reduced.observations;
        appendProviderTransport(
            transition.providerAfterPublication, reduced.providerEffects[0], PageRole::Primary);
        appendProviderTransport(
            transition.providerAfterPublication, reduced.providerEffects[1], PageRole::Secondary);
        applyEngineTransition(std::move(transition));
    };
    if (QThread::currentThread() == q->thread()) {
        apply();
        return;
    }
    QMetaObject::invokeMethod(q, std::move(apply), Qt::QueuedConnection);
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
    appendProviderTransport(
        transition.providerAfterPublication, reduced.providerEffects[0], PageRole::Primary);
    appendProviderTransport(
        transition.providerAfterPublication, reduced.providerEffects[1], PageRole::Secondary);
    applyEngineTransition(std::move(transition));
}
