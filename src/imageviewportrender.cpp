#include "imageviewport_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

#include <algorithm>

using namespace ImageViewportInternal;

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    auto preparedPayload = synchronization.preparedPayload;
    if (!preparedPayload.commitPending && hasReadyDisplay()) {
        preparedPayload.image = controller.displayState().displayedImage;
    }
    const bool imagePresent = !preparedPayload.image.isNull();
    QRectF targetRect = primaryItemRect().intersected(itemBounds());
    QRectF sourceRect = visibleImageRect();
    if (synchronization.pendingProviderCommit) {
        targetRect
            = contentRectForImageSize(controller.providerLogicalSize()).intersected(itemBounds());
        sourceRect = visibleImageRectForImageSize(controller.providerLogicalSize());
    }

    QVector<RenderAdapter::Input::ImageLayer> imageLayers;
    if (imagePresent) {
        imageLayers.append({ preparedPayload, targetRect, sourceRect, presentation.mirrorHorizontally,
            presentation.mirrorVertically });
    }
    if (!synchronization.pendingProviderCommit && hasReadyDisplay()) {
        if (ImageSequence* sequence = secondarySequence();
            sequence && sequence->isValid() && !sequence->isProvider()) {
            const int frame = sequence->frameCount() > 0
                ? std::clamp(displayedFrame(), 0, sequence->frameCount() - 1)
                : -1;
            QImage secondaryImage = frame >= 0 ? sequence->frameImage(frame) : QImage();
            const QRectF secondaryTargetRect = secondaryItemRect().intersected(itemBounds());
            const QRectF secondarySourceRect = visibleSecondaryPageRect();
            if (!secondaryImage.isNull() && !secondaryTargetRect.isEmpty()
                && !secondarySourceRect.isEmpty()) {
                auto secondaryPayload = preparedPayload;
                secondaryPayload.image = secondaryImage;
                imageLayers.append({ secondaryPayload, secondaryTargetRect, secondarySourceRect,
                    presentation.mirrorHorizontally, presentation.mirrorVertically });
            }
        }
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
