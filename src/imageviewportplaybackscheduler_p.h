#pragma once

#include "playbackclock_p.h"
#include "viewportplaybackcontract_p.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <functional>
#include <optional>

class QObject;

class ImageViewportPlaybackScheduler
{
public:
    using ElapsedSink = std::function<void(int)>;

    ImageViewportPlaybackScheduler(QObject& dispatchContext, ElapsedSink elapsedSink);

    void apply(ViewportPlaybackScheduleEffect effect);
    void stop();
    void flushElapsed();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void setPendingElapsedForTest(int elapsedMilliseconds);
#endif

private:
    int takeElapsed();
    void handleTimeout();

    ElapsedSink elapsedSink;
    QTimer timer;
    QElapsedTimer timebase;
    ImageViewportInternal::PlaybackClock clock;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    std::optional<int> pendingElapsedForTest;
#endif
};
