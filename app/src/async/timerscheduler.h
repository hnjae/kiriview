// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_TIMERSCHEDULER_H
#define KIRIVIEW_TIMERSCHEDULER_H

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

namespace kiriview {
using TimerDuration = std::chrono::milliseconds;

class RuntimeTimerHandle
{
public:
    RuntimeTimerHandle() = default;
    virtual ~RuntimeTimerHandle() = default;

    virtual void start(TimerDuration interval) = 0;
    virtual void stop() = 0;
    Q_DISABLE_COPY_MOVE(RuntimeTimerHandle)
};

using RuntimeTimerCallback = std::move_only_function<void()>;
using MonotonicClock = std::function<TimerDuration()>;
using SingleShotTimerFactory = std::function<std::unique_ptr<RuntimeTimerHandle>(
    QObject*, TimerDuration, RuntimeTimerCallback)>;

struct TimerScheduler
{
    MonotonicClock currentMonotonicTime;
    SingleShotTimerFactory singleShotTimer;
};

namespace Detail {
    class QtSingleShotTimer final : public RuntimeTimerHandle
    {
    public:
        QtSingleShotTimer(QObject* owner, TimerDuration interval, RuntimeTimerCallback callback)
            : m_callback(std::move(callback))
        {
            m_timer.setSingleShot(true);
            m_timer.setInterval(qtInterval(interval));

            QObject* context = owner == nullptr ? &m_timer : owner;
            QObject::connect(&m_timer, &QTimer::timeout, context, [this]() {
                if (m_callback) {
                    m_callback();
                }
            });
        }

        void start(TimerDuration interval) override { m_timer.start(qtInterval(interval)); }
        void stop() override { m_timer.stop(); }

    private:
        static int qtInterval(TimerDuration interval)
        {
            return static_cast<int>(std::clamp<TimerDuration::rep>(
                interval.count(), 0, std::numeric_limits<int>::max()));
        }

        QTimer m_timer;
        RuntimeTimerCallback m_callback;
    };
}

inline TimerScheduler defaultTimerScheduler()
{
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();

    return TimerScheduler {
        [clock]() { return TimerDuration(clock->isValid() ? clock->elapsed() : 0); },
        [](QObject* owner, TimerDuration interval,
            RuntimeTimerCallback callback) -> std::unique_ptr<RuntimeTimerHandle> {
            return std::make_unique<Detail::QtSingleShotTimer>(
                owner, interval, std::move(callback));
        },
    };
}

inline TimerScheduler timerSchedulerWithDefaults(TimerScheduler scheduler)
{
    TimerScheduler defaults = defaultTimerScheduler();
    if (!scheduler.currentMonotonicTime) {
        scheduler.currentMonotonicTime = std::move(defaults.currentMonotonicTime);
    }
    if (!scheduler.singleShotTimer) {
        scheduler.singleShotTimer = std::move(defaults.singleShotTimer);
    }

    return scheduler;
}
}

#endif
