#include "imageviewport_p.h"
#include "presentationgeometry_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

#include <algorithm>

using namespace ImageViewportInternal;

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
    const ImageViewportInternal::PresentationState& presentation = controller.presentationState();
    auto preparedPayload = synchronization.preparedPayload;
    if (!preparedPayload.commitPending && hasReadyDisplay()) {
        preparedPayload.image = controller.displayState().displayedImage;
    }
    const bool imagePresent = !preparedPayload.image.isNull();
    const bool pendingSpreadGeometry
        = synchronization.pendingProviderCommit || synchronization.pendingSecondaryCommit;
    const PresentationGeometry::State geometry = synchronization.geometryState;
    QRectF targetRect
        = PresentationGeometry::pageItemRect(geometry, PageRole::Primary).intersected(itemBounds());
    QRectF sourceRect = PresentationGeometry::visiblePageRect(geometry, PageRole::Primary);

    QVector<RenderAdapter::Input::ImageLayer> imageLayers;
    if (imagePresent) {
        imageLayers.append({ preparedPayload, targetRect, sourceRect,
            presentation.mirrorHorizontally, presentation.mirrorVertically });
    }
    const bool hasDisplayOrPendingSpread = hasReadyDisplay()
        || synchronization.pendingProviderCommit || synchronization.pendingSecondaryCommit;
    if (hasDisplayOrPendingSpread) {
        if (ImageSequence* sequence = secondarySequence(); sequence && sequence->isValid()) {
            auto secondaryPayload = preparedPayload;
            QImage secondaryImage;
            if (sequence->isProvider()) {
                if (!controller.displayState().secondaryPendingRenderPayload.image.isNull()) {
                    secondaryPayload = controller.displayState().secondaryPendingRenderPayload;
                    secondaryImage = secondaryPayload.image;
                } else {
                    secondaryImage = controller.displayState().secondaryDisplayedImage;
                    secondaryPayload.image = secondaryImage;
                }
            } else {
                const int roleFrame = synchronization.pendingProviderCommit
                        || synchronization.pendingSecondaryCommit
                    ? secondaryRequestedFrame()
                    : secondaryDisplayedFrame();
                const int frame = sequence->frameCount() > 0
                    ? std::clamp(roleFrame, 0, sequence->frameCount() - 1)
                    : -1;
                secondaryImage = frame >= 0 ? sequence->frameImage(frame) : QImage();
                secondaryPayload.image = secondaryImage;
            }
            const QRectF secondaryTargetRect
                = PresentationGeometry::pageItemRect(geometry, PageRole::Secondary)
                      .intersected(itemBounds());
            const QRectF secondarySourceRect
                = PresentationGeometry::visiblePageRect(geometry, PageRole::Secondary);
            if (!secondaryImage.isNull() && !secondaryTargetRect.isEmpty()
                && !secondarySourceRect.isEmpty()) {
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
