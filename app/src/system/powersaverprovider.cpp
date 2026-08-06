// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "powersaverprovider.h"

#include "powerprofilemonitor.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <cstdint>
#include <utility>
#include <vector>

namespace kiriview {
namespace {
    struct PowerSaverSubscriber
    {
        PowerSaverChangedCallback callback;
        bool active = true;
    };

    struct SharedPowerSaverState
    {
        [[nodiscard]] bool powerSaverEnabled() const
        {
            const QMutexLocker lock(&mutex);
            return enabled;
        }

        quint64 subscribe(PowerSaverChangedCallback callback)
        {
            const QMutexLocker lock(&mutex);
            const quint64 id = nextSubscriberId++;
            subscribers.insert(id,
                std::make_shared<PowerSaverSubscriber>(
                    PowerSaverSubscriber { std::move(callback), true }));
            return id;
        }

        void unsubscribe(quint64 id)
        {
            const QMutexLocker lock(&mutex);
            const auto iterator = subscribers.find(id);
            if (iterator == subscribers.end()) {
                return;
            }
            iterator.value()->active = false;
            subscribers.erase(iterator);
        }

        void update(bool value)
        {
            std::vector<std::shared_ptr<PowerSaverSubscriber>> snapshot;
            {
                const QMutexLocker lock(&mutex);
                enabled = value;
                snapshot.reserve(static_cast<std::size_t>(subscribers.size()));
                for (const std::shared_ptr<PowerSaverSubscriber>& subscriber : subscribers) {
                    snapshot.push_back(subscriber);
                }
            }

            for (const std::shared_ptr<PowerSaverSubscriber>& subscriber : snapshot) {
                PowerSaverChangedCallback callback;
                {
                    const QMutexLocker lock(&mutex);
                    if (subscriber->active) {
                        callback = subscriber->callback;
                    }
                }
                if (callback) {
                    callback(value);
                }
            }
        }

        mutable QMutex mutex;
        bool enabled = false;
        quint64 nextSubscriberId = 1;
        QHash<quint64, std::shared_ptr<PowerSaverSubscriber>> subscribers;
    };

    class SharedPowerSaverMonitor final : public PowerSaverStateMonitor
    {
    public:
        SharedPowerSaverMonitor(
            std::shared_ptr<SharedPowerSaverState> state, PowerSaverChangedCallback callback)
            : m_state(std::move(state))
            , m_subscriberId(m_state->subscribe(std::move(callback)))
        {
        }

        ~SharedPowerSaverMonitor() override { m_state->unsubscribe(m_subscriberId); }

        [[nodiscard]] bool powerSaverEnabled() const override
        {
            return m_state->powerSaverEnabled();
        }

    private:
        std::shared_ptr<SharedPowerSaverState> m_state;
        quint64 m_subscriberId = 0;
    };
}

class PowerSaverRuntime::Private
{
public:
    explicit Private(PowerProfileMonitorRuntime runtime)
        : state(std::make_shared<SharedPowerSaverState>())
        , monitor(std::make_unique<PowerProfileMonitor>(
              [state = state](bool enabled) { state->update(enabled); }, std::move(runtime)))
    {
    }

    std::shared_ptr<SharedPowerSaverState> state;
    std::unique_ptr<PowerProfileMonitor> monitor;
};

PowerSaverRuntime::PowerSaverRuntime(QObject* parent)
    : PowerSaverRuntime(PowerProfileMonitorRuntime {}, parent)
{
}

PowerSaverRuntime::PowerSaverRuntime(PowerProfileMonitorRuntime runtime, QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Private>(powerProfileMonitorRuntimeWithDefaults(std::move(runtime))))
{
}

PowerSaverRuntime::~PowerSaverRuntime() = default;

PowerSaverProvider PowerSaverRuntime::provider() const
{
    const std::weak_ptr<SharedPowerSaverState> state = d->state;
    return PowerSaverProvider {
        [state](PowerSaverChangedCallback callback) -> std::unique_ptr<PowerSaverStateMonitor> {
            const std::shared_ptr<SharedPowerSaverState> sharedState = state.lock();
            if (sharedState == nullptr) {
                return {};
            }
            return std::make_unique<SharedPowerSaverMonitor>(sharedState, std::move(callback));
        },
    };
}
}
