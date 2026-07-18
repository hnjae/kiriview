// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/windowchromeruntime.h"
#include "async/timerscheduler.h"

#include <QObject>
#include <QTest>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class TestWindowChromeRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fullscreenRestoresPriorVisibility();
    void pointerAndToolbarLifecycleUsesAcceptedFacts();
    void supersededTimeoutsCannotMutateCurrentState();
};

namespace {
struct ManualTimerState
{
    int intervalMsec = 0;
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

    void start() override { m_state->active = true; }
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
            []() { return qint64(0); },
            [this](QObject*, int intervalMsec, kiriview::RuntimeTimerCallback callback) {
                auto state = std::make_shared<ManualTimerState>();
                state->intervalMsec = intervalMsec;
                state->callback = std::move(callback);
                m_timers.push_back(state);
                return std::make_unique<ManualTimer>(std::move(state));
            },
        };
    }

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

struct ChromeFixture
{
    QObject owner;
    ManualTimerScheduler timers;
    std::vector<kiriview::WindowVisibility> appliedVisibilities;
    int snapshotChangeCount = 0;
    kiriview::WindowChromeRuntime runtime;

    ChromeFixture()
        : runtime(&owner, timers.scheduler(),
              {
                  [this](kiriview::WindowVisibility visibility) {
                      appliedVisibilities.push_back(visibility);
                  },
                  [this]() { ++snapshotChangeCount; },
              })
    {
    }
};
}

void TestWindowChromeRuntime::fullscreenRestoresPriorVisibility()
{
    for (const kiriview::WindowVisibility visibility : { kiriview::WindowVisibility::Windowed,
             kiriview::WindowVisibility::Maximized, kiriview::WindowVisibility::Minimized }) {
        ChromeFixture fixture;
        fixture.runtime.observeVisibility(visibility);

        fixture.runtime.requestToggleFullscreen();
        QCOMPARE(fixture.appliedVisibilities.back(), kiriview::WindowVisibility::Fullscreen);
        fixture.runtime.observeVisibility(kiriview::WindowVisibility::Fullscreen);
        QVERIFY(fixture.runtime.snapshot().fullscreen);
        QVERIFY(fixture.runtime.snapshot().pointerHidden);
        QVERIFY(fixture.runtime.snapshot().toolbarRevealed);

        fixture.runtime.requestToggleFullscreen();
        QCOMPARE(fixture.appliedVisibilities.back(), visibility);
        fixture.runtime.observeVisibility(visibility);
        QVERIFY(!fixture.runtime.snapshot().fullscreen);
        QVERIFY(!fixture.runtime.snapshot().pointerHidden);
        QVERIFY(!fixture.runtime.snapshot().toolbarRevealed);
    }
}

void TestWindowChromeRuntime::pointerAndToolbarLifecycleUsesAcceptedFacts()
{
    ChromeFixture fixture;
    fixture.runtime.observeVisibility(kiriview::WindowVisibility::Windowed);
    fixture.runtime.observeVisibility(kiriview::WindowVisibility::Fullscreen);

    fixture.runtime.reportPointerMoved(false);
    QVERIFY(!fixture.runtime.snapshot().pointerHidden);

    fixture.runtime.reportPointerMoved(true);
    QVERIFY(fixture.runtime.snapshot().toolbarRevealed);

    fixture.runtime.reportToolbarInteractionActive(true);
    QVERIFY(fixture.runtime.snapshot().toolbarRevealed);
    fixture.runtime.reportToolbarInteractionActive(false);

    const std::size_t lastTimer = fixture.timers.count() - 1;
    fixture.timers.fireEvenIfStopped(lastTimer);
    QVERIFY(!fixture.runtime.snapshot().toolbarRevealed);
}

void TestWindowChromeRuntime::supersededTimeoutsCannotMutateCurrentState()
{
    ChromeFixture fixture;
    fixture.runtime.observeVisibility(kiriview::WindowVisibility::Fullscreen);
    fixture.runtime.reportPointerMoved(false);
    const std::size_t supersededPointerTimer = fixture.timers.count() - 1;

    fixture.runtime.reportPointerMoved(false);
    QVERIFY(!fixture.runtime.snapshot().pointerHidden);
    fixture.timers.fireEvenIfStopped(supersededPointerTimer);
    QVERIFY(!fixture.runtime.snapshot().pointerHidden);

    fixture.runtime.observeVisibility(kiriview::WindowVisibility::Windowed);
    fixture.timers.fireEvenIfStopped(fixture.timers.count() - 1);
    QVERIFY(!fixture.runtime.snapshot().pointerHidden);
    QVERIFY(!fixture.runtime.snapshot().toolbarRevealed);
}

QTEST_GUILESS_MAIN(TestWindowChromeRuntime)

#include "test_windowchromeruntime.moc"
