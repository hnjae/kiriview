#include "imageviewport_p.h"

#include <QtQuick/QQuickWindow>

namespace {
double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}
}

ImageViewportStateSnapshot ImageViewportPrivate::state() const
{
    return engine.snapshot({ itemBounds(), effectiveDevicePixelRatio(*this) });
}

ImageViewportCommandResult ImageViewportPrivate::commandResult(CommandOutcome outcome) const
{
    const ImageViewportStateSnapshot snapshot = state();
    return ImageViewportCommandResult(outcome, snapshot.diagnostics().commandReason(),
        snapshot.revisions().command(), snapshot.revisions().snapshot());
}

void ImageViewportPrivate::refreshStateSnapshot()
{
    const ImageViewportStateSnapshot currentSnapshot = state();
    if (currentSnapshot == lastStateSnapshot) {
        return;
    }
    lastStateSnapshot = currentSnapshot;
    emit q->stateChanged();
}
