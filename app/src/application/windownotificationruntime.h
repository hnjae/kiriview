// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_WINDOWNOTIFICATIONRUNTIME_H
#define KIRIVIEW_WINDOWNOTIFICATIONRUNTIME_H

#include "async/timerscheduler.h"

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>

namespace kiriview {
enum class WindowNotificationScope {
    NavigationBoundary,
    UnsupportedAction,
    UnsupportedMedia,
    OperationFailure,
};

struct WindowNotificationRequest
{
    WindowNotificationScope scope = WindowNotificationScope::OperationFailure;
    QString message;
};

struct WindowNotificationSnapshot
{
    quint64 revision = 0;
    quint64 replayRevision = 0;
    bool active = false;
    QString message;
    WindowNotificationScope scope = WindowNotificationScope::OperationFailure;

    bool operator==(const WindowNotificationSnapshot& other) const
    {
        return revision == other.revision && replayRevision == other.replayRevision
            && active == other.active && message == other.message && scope == other.scope;
    }
};

class WindowNotificationRuntime final
{
public:
    using SnapshotChanged = std::function<void()>;

    explicit WindowNotificationRuntime(
        QObject* owner, TimerScheduler timerScheduler = {}, SnapshotChanged snapshotChanged = {});
    ~WindowNotificationRuntime();

    [[nodiscard]] const WindowNotificationSnapshot& snapshot() const;
    void submit(WindowNotificationRequest request);
    void clear(WindowNotificationScope scope);
    void dismiss();

private:
    void scheduleDismissal();
    void invalidateTimer();
    void commit(WindowNotificationSnapshot snapshot);

    QObject* m_owner = nullptr;
    TimerScheduler m_timerScheduler;
    SnapshotChanged m_snapshotChanged;
    WindowNotificationSnapshot m_snapshot;
    quint64 m_timerGeneration = 0;
    std::unique_ptr<RuntimeTimerHandle> m_timer;
};
}

#endif
