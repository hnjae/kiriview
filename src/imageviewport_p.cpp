#include "imageviewport_p.h"

ImageViewportPrivate::ImageViewportPrivate(ImageViewport* viewport)
    : q(viewport)
    , controller(*this)
    , playbackScheduler(*this)
    , providerHost(*this)
    , renderHost(*this)
{
}

ImageViewportPrivate::~ImageViewportPrivate()
{
    playbackScheduler.stop();
    providerHost.closeActiveSessions();
}

double ImageViewportPrivate::width() const { return q->width(); }

double ImageViewportPrivate::height() const { return q->height(); }

QQuickWindow* ImageViewportPrivate::window() const { return q->window(); }

void ImageViewportPrivate::update() { q->update(); }
