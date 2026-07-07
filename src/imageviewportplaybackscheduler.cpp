#include "imageviewportplaybackscheduler_p.h"

#include "imageviewport_p.h"

#include <QtCore/QObject>

ImageViewportPlaybackScheduler::ImageViewportPlaybackScheduler(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
    timebase.start();
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, viewport.q, [this]() { handleTimeout(); });
}

void ImageViewportPlaybackScheduler::sync()
{
    const int interval = viewport.controller.playbackTimerInterval();
    if (interval <= 0) {
        stop();
        return;
    }

    clock.restart(timebase.elapsed());
    timer.start(interval);
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

void ImageViewportPlaybackScheduler::handleTimeout()
{
    viewport.advancePlayback(takeElapsed());
    sync();
}
