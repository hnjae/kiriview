#include "imageviewport_p.h"

ImageViewportPrivate::ImageViewportPrivate(ImageViewport* viewport)
    : q(viewport)
    , controller(*this)
    , providerBridge(*this)
    , display(controller.displayState())
    , request(controller.requestState())
    , provider(controller.providerState())
{
    playbackTimer.setSingleShot(true);
    QObject::connect(&playbackTimer, &QTimer::timeout, q, [this]() { handlePlaybackTimer(); });
}

ImageViewportPrivate::~ImageViewportPrivate()
{
    stopPlaybackTimer();
    applyProviderFrameTransportEffect(controller.closeProviderSession());
}

double ImageViewportPrivate::width() const { return q->width(); }

double ImageViewportPrivate::height() const { return q->height(); }

QQuickWindow* ImageViewportPrivate::window() const { return q->window(); }

void ImageViewportPrivate::update() { q->update(); }
