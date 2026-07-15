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

ImageViewportCommandResult ImageViewportPrivate::commandResult(
    CommandOutcome outcome, const ImageViewportStateSnapshot& snapshot) const
{
    return ImageViewportCommandResult(outcome, snapshot.diagnostics().commandReason(),
        snapshot.revisions().command(), snapshot.revisions().snapshot());
}
