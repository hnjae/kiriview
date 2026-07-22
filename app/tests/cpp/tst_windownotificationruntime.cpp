// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/windownotificationruntime.h"
#include "async/timerscheduler.h"

#include <QObject>
#include <QString>
#include <QTest>
#include <memory>
#include <utility>
#include <vector>

class TestWindowNotificationRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void requestsReplaceAndReplay();
    void scopeClearOnlyDismissesMatchingNotification();
    void dismissalIsIdempotent();
    void supersededTimeoutCannotDismissReplacement();
};

namespace {
struct ManualTimerState
{
    kiriview::TimerDuration interval {};
    bool active = false;
    kiriview::RuntimeTimerCallback callback;
};

class ManualTimer final : public kiriview::RuntimeTimerHandle
{
public:
    explicit ManualTimer(std::shared_ptr<ManualTimerState> state)
        : m_state(std::move(state))
    {
    }

    void start(kiriview::TimerDuration interval) override
    {
        m_state->interval = interval;
        m_state->active = true;
    }
    void stop() override { m_state->active = false; }

private:
    std::shared_ptr<ManualTimerState> m_state;
};

class ManualTimerScheduler
{
public:
    kiriview::TimerScheduler scheduler()
    {
        return {
            []() { return kiriview::TimerDuration(0); },
            [this](QObject*, kiriview::TimerDuration interval,
                kiriview::RuntimeTimerCallback callback) {
                auto state = std::make_shared<ManualTimerState>();
                state->interval = interval;
                state->callback = std::move(callback);
                m_timers.push_back(state);
                return std::make_unique<ManualTimer>(std::move(state));
            },
        };
    }

    const ManualTimerState& timer(std::size_t index) const { return *m_timers.at(index); }
    std::size_t count() const { return m_timers.size(); }

    void fireEvenIfStopped(std::size_t index)
    {
        const auto state = m_timers.at(index);
        state->active = false;
        state->callback();
    }

private:
    std::vector<std::shared_ptr<ManualTimerState>> m_timers;
};

struct NotificationFixture
{
    QObject owner;
    ManualTimerScheduler timers;
    int snapshotChangeCount = 0;
    kiriview::WindowNotificationRuntime runtime;

    NotificationFixture()
        : runtime(&owner, timers.scheduler(), [this]() { ++snapshotChangeCount; })
    {
    }
};
}

void TestWindowNotificationRuntime::requestsReplaceAndReplay()
{
    NotificationFixture fixture;
    const kiriview::WindowNotificationRequest request {
        kiriview::WindowNotificationScope::NavigationBoundary,
        QStringLiteral("First image"),
    };

    fixture.runtime.submit(request);
    QVERIFY(fixture.runtime.snapshot().active);
    QCOMPARE(fixture.runtime.snapshot().message, request.message);
    QCOMPARE(fixture.runtime.snapshot().scope, request.scope);
    QCOMPARE(fixture.runtime.snapshot().replayRevision, quint64(1));
    QCOMPARE(fixture.timers.timer(0).interval, kiriview::TimerDuration(7000));

    fixture.runtime.submit(request);
    QCOMPARE(fixture.runtime.snapshot().replayRevision, quint64(2));
    QCOMPARE(fixture.snapshotChangeCount, 2);

    fixture.runtime.submit({
        kiriview::WindowNotificationScope::OperationFailure,
        QStringLiteral("Could not delete file"),
    });
    QCOMPARE(fixture.runtime.snapshot().message, QStringLiteral("Could not delete file"));
    QCOMPARE(fixture.runtime.snapshot().scope, kiriview::WindowNotificationScope::OperationFailure);
}

void TestWindowNotificationRuntime::scopeClearOnlyDismissesMatchingNotification()
{
    NotificationFixture fixture;
    fixture.runtime.submit({
        kiriview::WindowNotificationScope::OperationFailure,
        QStringLiteral("Could not delete file"),
    });
    fixture.runtime.clear(kiriview::WindowNotificationScope::NavigationBoundary);
    QVERIFY(fixture.runtime.snapshot().active);

    fixture.runtime.submit({
        kiriview::WindowNotificationScope::NavigationBoundary,
        QStringLiteral("Last image"),
    });
    fixture.runtime.clear(kiriview::WindowNotificationScope::NavigationBoundary);
    QVERIFY(!fixture.runtime.snapshot().active);
    QVERIFY(fixture.runtime.snapshot().message.isEmpty());
}

void TestWindowNotificationRuntime::dismissalIsIdempotent()
{
    NotificationFixture fixture;
    fixture.runtime.dismiss();
    QCOMPARE(fixture.snapshotChangeCount, 0);

    fixture.runtime.submit({
        kiriview::WindowNotificationScope::UnsupportedAction,
        QStringLiteral("This action is not available for videos"),
    });
    fixture.runtime.dismiss();
    fixture.runtime.dismiss();
    QCOMPARE(fixture.snapshotChangeCount, 2);
    QVERIFY(!fixture.runtime.snapshot().active);
}

void TestWindowNotificationRuntime::supersededTimeoutCannotDismissReplacement()
{
    NotificationFixture fixture;
    fixture.runtime.submit({
        kiriview::WindowNotificationScope::NavigationBoundary,
        QStringLiteral("First image"),
    });
    const std::size_t supersededTimer = fixture.timers.count() - 1;

    fixture.runtime.submit({
        kiriview::WindowNotificationScope::OperationFailure,
        QStringLiteral("Could not delete file"),
    });
    fixture.timers.fireEvenIfStopped(supersededTimer);
    QVERIFY(fixture.runtime.snapshot().active);
    QCOMPARE(fixture.runtime.snapshot().message, QStringLiteral("Could not delete file"));

    fixture.timers.fireEvenIfStopped(fixture.timers.count() - 1);
    QVERIFY(!fixture.runtime.snapshot().active);
}

QTEST_GUILESS_MAIN(TestWindowNotificationRuntime)

#include "tst_windownotificationruntime.moc"
