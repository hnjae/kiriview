#include "imageviewport_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <QtQuick/QQuickWindow>

ImageViewportPrivate::ImageViewportPrivate(ImageViewport* viewport)
    : q(viewport)
    , playbackScheduler(
          *viewport, [this](int elapsedMilliseconds) { advancePlayback(elapsedMilliseconds); })
    , providerHost(
          *viewport,
          [this](ViewportProviderHostEvent event) { enqueueProviderHostEvent(std::move(event)); },
          [this](ImageViewportInternal::ProviderTransportDiagnostic diagnostic) {
              internalObservability.recordProviderCleanupFailure(diagnostic);
          })
{
    lastStateSnapshot = state();
}

ImageViewportPrivate::~ImageViewportPrivate()
{
    playbackScheduler.stop();
    providerHost.releaseAllFrameLeases();
    providerHost.applyTransportEffects(engine.shutdown());
    providerHost.shutdown();
}

double ImageViewportPrivate::width() const { return q->width(); }

double ImageViewportPrivate::height() const { return q->height(); }

QQuickWindow* ImageViewportPrivate::window() const { return q->window(); }

ViewportEngineViewportInput ImageViewportPrivate::viewportInput() const
{
    const QQuickWindow* currentWindow = window();
    return { itemBounds(), currentWindow ? currentWindow->effectiveDevicePixelRatio() : 1.0,
        currentWindow != nullptr };
}

void ImageViewportPrivate::update() { q->update(); }
