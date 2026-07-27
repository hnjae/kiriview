// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <QtQuick/QQuickWindow>

ImageViewportPrivate::ImageViewportPrivate(ImageViewport* viewport)
    : q(viewport)
    , providerHost(
          *viewport,
          [this](ViewportProviderHostEvent event) { enqueueProviderHostEvent(std::move(event)); },
          [this](ImageViewportInternal::ProviderTransportDiagnostic diagnostic) {
              internalObservability.recordProviderCleanupFailure(diagnostic);
          },
          [this](ViewportProviderTransportCommand command) {
              enqueueDeferredProviderTransport(std::move(command));
          })
{
    for (const auto role : { PageRole::Primary, PageRole::Secondary }) {
        playbackSchedulers[role == PageRole::Secondary ? 1U : 0U]
            = std::make_unique<ImageViewportPlaybackScheduler>(*viewport, role,
                [this](ViewportPlaybackTimeoutFact fact) { advancePlayback(fact); });
    }
    lastStateSnapshot = state();
}

ImageViewportPrivate::~ImageViewportPrivate()
{
    for (auto& scheduler : playbackSchedulers)
        scheduler->stop();
    providerHost.releaseAllProviderLeases();
    providerHost.applyTransportEffects(engine.shutdown());
    providerHost.shutdown();
}

double ImageViewportPrivate::width() const { return q->width(); }

double ImageViewportPrivate::height() const { return q->height(); }

QQuickWindow* ImageViewportPrivate::window() const { return q->window(); }

ViewportEngineViewportState ImageViewportPrivate::viewportState() const
{
    const QQuickWindow* currentWindow = window();
    return { itemBounds(), currentWindow ? currentWindow->effectiveDevicePixelRatio() : 1.0,
        currentWindow != nullptr };
}

void ImageViewportPrivate::update() { q->update(); }
