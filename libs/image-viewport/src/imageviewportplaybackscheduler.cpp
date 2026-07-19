// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportplaybackscheduler_p.h"

#include "viewportplaybackcontract_p.h"

#include <QtCore/QObject>

ImageViewportPlaybackScheduler::ImageViewportPlaybackScheduler(
    QObject& dispatchContext, ImageViewportPageRole role, ElapsedSink elapsedSink)
    : elapsedSink(std::move(elapsedSink))
    , role(role)
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

    const qint64 now = timebase.elapsed();
    const bool preserveElapsed = clock.isValid() && generation == effect.generation;
    generation = effect.generation;
    scheduleIdentity = effect.scheduleIdentity;
    if (preserveElapsed) {
        timer.start(std::max(1, effect.delayMilliseconds - clock.elapsed(now)));
    } else {
        clock.restart(now);
        timer.start(effect.delayMilliseconds);
    }
}

void ImageViewportPlaybackScheduler::stop()
{
    timer.stop();
    clock.invalidate();
    generation = 0;
    scheduleIdentity = 0;
}

void ImageViewportPlaybackScheduler::flushElapsed()
{
    if (!clock.isValid()) {
        return;
    }

    if (elapsedSink) {
        const auto fact
            = ViewportPlaybackTimeoutFact { role, generation, scheduleIdentity, takeElapsed() };
        elapsedSink(fact);
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
        const auto fact
            = ViewportPlaybackTimeoutFact { role, generation, scheduleIdentity, takeElapsed() };
        elapsedSink(fact);
    }
}
