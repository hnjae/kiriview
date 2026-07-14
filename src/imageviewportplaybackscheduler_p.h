#pragma once

#include "playbackclock_p.h"
#include "viewportplaybackcontract_p.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <functional>

class QObject;

class ImageViewportPlaybackScheduler
{
public:
    using ElapsedSink = std::function<void(int)>;

    ImageViewportPlaybackScheduler(QObject& dispatchContext, ElapsedSink elapsedSink);

    void apply(ViewportPlaybackScheduleEffect effect);
    void stop();
    void flushElapsed();

private:
    int takeElapsed();
    void handleTimeout();

    ElapsedSink elapsedSink;
    QTimer timer;
    QElapsedTimer timebase;
    ImageViewportInternal::PlaybackClock clock;
};
