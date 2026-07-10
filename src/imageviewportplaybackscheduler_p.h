#pragma once

#include "playbackclock_p.h"
#include "viewportcontrollerplaybackcontract_p.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

class ImageViewportPrivate;

class ImageViewportPlaybackScheduler
{
public:
    explicit ImageViewportPlaybackScheduler(ImageViewportPrivate& viewport);

    void apply(ViewportPlaybackScheduleEffect effect);
    void stop();
    void flushElapsed();

private:
    int takeElapsed();
    void handleTimeout();

    ImageViewportPrivate& viewport;
    QTimer timer;
    QElapsedTimer timebase;
    ImageViewportInternal::PlaybackClock clock;
};
