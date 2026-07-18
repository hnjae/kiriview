#include "imageviewportplaybackscheduler_p.h"

#include "viewportplaybackcontract_p.h"

#include <QtCore/QObject>

ImageViewportPlaybackScheduler::ImageViewportPlaybackScheduler(
    QObject& dispatchContext, ElapsedSink elapsedSink)
    : elapsedSink(std::move(elapsedSink))
{
    timebase.start();
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &dispatchContext, [this]() { handleTimeout(); });
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

    if (elapsedSink) {
        elapsedSink(takeElapsed());
    }
}

int ImageViewportPlaybackScheduler::takeElapsed()
{
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    if (pendingElapsedForTest) {
        const int elapsedMilliseconds = *pendingElapsedForTest;
        pendingElapsedForTest.reset();
        timer.stop();
        clock.invalidate();
        return elapsedMilliseconds;
    }
#endif
    const int elapsedMilliseconds = clock.takeElapsed(timebase.elapsed());
    timer.stop();
    return elapsedMilliseconds;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPlaybackScheduler::setPendingElapsedForTest(int elapsedMilliseconds)
{
    pendingElapsedForTest = std::max(0, elapsedMilliseconds);
}
#endif

void ImageViewportPlaybackScheduler::handleTimeout()
{
    if (elapsedSink) {
        elapsedSink(takeElapsed());
    }
}
