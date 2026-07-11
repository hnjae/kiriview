#include "imageviewportplaybackscheduler_p.h"

#include "imageviewport_p.h"
#include "viewportplaybackcontract_p.h"

#include <QtCore/QObject>

ImageViewportPlaybackScheduler::ImageViewportPlaybackScheduler(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
    timebase.start();
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, viewport.q, [this]() { handleTimeout(); });
}

void ImageViewportPlaybackScheduler::apply(ViewportPlaybackScheduleEffect effect)
{
    using Action = ViewportPlaybackScheduleEffect::Action;
    if (effect.action == Action::NoChange) {
        return;
    }
    if (effect.action == Action::Stop || effect.delayMilliseconds <= 0) {
        stop();
        return;
    }

    clock.restart(timebase.elapsed());
    timer.start(effect.delayMilliseconds);
}

void ImageViewportPlaybackScheduler::stop()
{
    timer.stop();
    clock.invalidate();
}

void ImageViewportPlaybackScheduler::flushElapsed()
{
    if (!clock.isValid()) {
        return;
    }

    viewport.advancePlayback(takeElapsed());
}

int ImageViewportPlaybackScheduler::takeElapsed()
{
    const int elapsedMilliseconds = clock.takeElapsed(timebase.elapsed());
    timer.stop();
    return elapsedMilliseconds;
}

void ImageViewportPlaybackScheduler::handleTimeout() { viewport.advancePlayback(takeElapsed()); }
