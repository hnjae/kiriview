// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    QSGNode* renderedNode = oldNode;
    ViewportRenderHostFact fact;
    bool hasFact = false;
    bool renderUnavailable = false;
    {
        const auto attemptValue = renderAttemptForHost();
        if (!attemptValue) {
            return oldNode;
        }
        const ViewportRenderAttempt& attempt = *attemptValue;
        if (!window() && attempt.snapshot.requiredRoleSet.primary()) {
            renderUnavailable = true;
        } else {
            ImageViewportRenderHostResult render
                = renderHost.synchronize(oldNode, window(), attempt);
            renderedNode = render.node;
            fact = std::move(render.fact);
            hasFact = true;
        }
    }
    if (renderUnavailable) {
        return oldNode;
    }
    if (hasFact) {
        applyRenderHostFact(std::move(fact));
    }
    return renderedNode;
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
    const quint64 attempt = fact.acknowledgement.attempt;
    auto apply = [this, attempt, fact = std::move(fact)]() mutable {
        discardPendingRenderMailbox(attempt);
        applyEngineTransition(engine.handleRenderHostFact({ std::move(fact) }));
    };
    const QThread* renderThread = q->thread();
    if (renderThread != nullptr && renderThread->isCurrentThread()) {
        apply();
        return;
    }
    QMetaObject::invokeMethod(q, std::move(apply), Qt::QueuedConnection);
}

void ImageViewportPrivate::discardPendingRenderMailbox(quint64 attempt)
{
    const QMutexLocker lock(&renderMailboxMutex);
    if (attempt != 0 && (!renderMailboxValid || renderMailbox.attempt != attempt)) {
        return;
    }
    renderMailbox = {};
    renderMailboxValid = false;
}
