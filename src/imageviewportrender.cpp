#include "imageviewport_p.h"
#include "presentationgeometry_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGNode>

#include <algorithm>

using namespace ImageViewportInternal;

namespace {

bool isPositiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}

QSizeF imageLogicalSize(const QImage& image)
{
    return image.isNull() ? QSizeF() : image.deviceIndependentSize();
}

PresentationGeometry::State renderGeometryState(
    const ImageViewportPrivate& viewport, QSizeF primarySize, QSizeF secondarySize)
{
    const ImageViewportInternal::PresentationState& presentation
        = viewport.controller.presentationState();
    return {
        isPositiveSize(primarySize),
        viewport.itemBounds(),
        primarySize,
        secondarySize,
        presentation.pageGap,
        presentation.spreadDirection,
        presentation.fitMode,
        presentation.fillMode,
        presentation.horizontalAlignment,
        presentation.verticalAlignment,
        presentation.rotationDegrees,
        presentation.mirrorHorizontally,
        presentation.mirrorVertically,
        presentation.zoom,
        effectiveDevicePixelRatio(viewport),
        presentation.pan,
    };
}

}

QSGNode* ImageViewportPrivate::updatePaintNode(QSGNode* oldNode)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    const ImageViewportInternal::PresentationState& presentation = controller.presentationState();
    auto preparedPayload = synchronization.preparedPayload;
    if (!preparedPayload.commitPending && hasReadyDisplay()) {
        preparedPayload.image = controller.displayState().displayedImage;
    }
    const bool imagePresent = !preparedPayload.image.isNull();
    const bool pendingSpreadGeometry
        = synchronization.pendingProviderCommit || synchronization.pendingSecondaryCommit;
    QSizeF primaryRenderSize;
    QSizeF secondaryRenderSize;
    if (pendingSpreadGeometry) {
        primaryRenderSize = synchronization.pendingProviderCommit
            ? controller.providerLogicalSize()
            : imageLogicalSize(preparedPayload.image);
        if (ImageSequence* sequence = secondarySequence(); sequence && sequence->isValid()) {
            if (sequence->isProvider()) {
                secondaryRenderSize = controller.secondaryProviderLogicalSize();
                if (!isPositiveSize(secondaryRenderSize)) {
                    secondaryRenderSize = imageLogicalSize(
                        controller.displayState().secondaryPendingRenderPayload.image);
                }
            } else {
                secondaryRenderSize = sequence->logicalSize();
            }
        }
    }
    const PresentationGeometry::State pendingGeometry
        = renderGeometryState(*this, primaryRenderSize, secondaryRenderSize);
    QRectF targetRect = primaryItemRect().intersected(itemBounds());
    QRectF sourceRect = visibleImageRect();
    if (pendingSpreadGeometry) {
        targetRect = PresentationGeometry::pageItemRect(pendingGeometry, PageRole::Primary)
                         .intersected(itemBounds());
        sourceRect = PresentationGeometry::visiblePageRect(pendingGeometry, PageRole::Primary);
    }

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
            const QRectF secondaryTargetRect = pendingSpreadGeometry
                ? PresentationGeometry::pageItemRect(pendingGeometry, PageRole::Secondary)
                      .intersected(itemBounds())
                : secondaryItemRect().intersected(itemBounds());
            const QRectF secondarySourceRect = pendingSpreadGeometry
                ? PresentationGeometry::visiblePageRect(pendingGeometry, PageRole::Secondary)
                : visibleSecondaryPageRect();
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
