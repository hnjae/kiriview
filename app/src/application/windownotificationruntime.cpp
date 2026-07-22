// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "windownotificationruntime.h"

#include <utility>

namespace {
constexpr kiriview::TimerDuration notificationTimeout { 7000 };
}

namespace kiriview {
WindowNotificationRuntime::WindowNotificationRuntime(
    QObject* owner, TimerScheduler timerScheduler, SnapshotChanged snapshotChanged)
    : m_owner(owner)
    , m_timerScheduler(timerSchedulerWithDefaults(std::move(timerScheduler)))
    , m_snapshotChanged(std::move(snapshotChanged))
{
}

WindowNotificationRuntime::~WindowNotificationRuntime() { invalidateTimer(); }

const WindowNotificationSnapshot& WindowNotificationRuntime::snapshot() const { return m_snapshot; }

void WindowNotificationRuntime::submit(WindowNotificationRequest request)
{
    if (request.message.isEmpty()) {
        return;
    }

    WindowNotificationSnapshot next = m_snapshot;
    next.active = true;
    next.message = std::move(request.message);
    next.scope = request.scope;
    next.replayRevision = m_snapshot.replayRevision + 1;
    commit(std::move(next));
    scheduleDismissal();
}

void WindowNotificationRuntime::clear(WindowNotificationScope scope)
{
    if (m_snapshot.active && m_snapshot.scope == scope) {
        dismiss();
    }
}

void WindowNotificationRuntime::dismiss()
{
    if (!m_snapshot.active) {
        return;
    }

    invalidateTimer();
    WindowNotificationSnapshot next = m_snapshot;
    next.active = false;
    next.message.clear();
    commit(std::move(next));
}

void WindowNotificationRuntime::scheduleDismissal()
{
    invalidateTimer();
    const quint64 generation = m_timerGeneration;
    m_timer = m_timerScheduler.singleShotTimer(m_owner, notificationTimeout, [this, generation]() {
        if (generation == m_timerGeneration) {
            dismiss();
        }
    });
    m_timer->start(notificationTimeout);
}

void WindowNotificationRuntime::invalidateTimer()
{
    ++m_timerGeneration;
    if (m_timer) {
        m_timer->stop();
        m_timer.reset();
    }
}

void WindowNotificationRuntime::commit(WindowNotificationSnapshot snapshot)
{
    snapshot.revision = m_snapshot.revision;
    if (snapshot == m_snapshot) {
        return;
    }

    snapshot.revision = m_snapshot.revision + 1;
    m_snapshot = std::move(snapshot);
    if (m_snapshotChanged) {
        m_snapshotChanged();
    }
}
}
