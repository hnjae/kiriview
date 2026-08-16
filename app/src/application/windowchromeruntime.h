// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_WINDOWCHROMERUNTIME_H
#define KIRIVIEW_WINDOWCHROMERUNTIME_H

#include "async/timerscheduler.h"

#include <QObject>
#include <QtGlobal>
#include <functional>
#include <memory>

namespace kiriview {
enum class WindowVisibility {
    Windowed,
    Maximized,
    Minimized,
    Fullscreen,
};

struct WindowChromeSnapshot
{
    quint64 revision = 0;
    bool fullscreen = false;
    bool pointerHidden = false;
    bool toolbarRevealed = false;

    bool operator==(const WindowChromeSnapshot& other) const
    {
        return revision == other.revision && fullscreen == other.fullscreen
            && pointerHidden == other.pointerHidden && toolbarRevealed == other.toolbarRevealed;
    }
};

struct WindowChromeRuntimePorts
{
    std::function<void(WindowVisibility)> applyVisibility;
    std::function<void()> snapshotChanged;
};

class WindowChromeRuntime final
{
public:
    explicit WindowChromeRuntime(
        QObject* owner, TimerScheduler timerScheduler = {}, WindowChromeRuntimePorts ports = {});
    ~WindowChromeRuntime();
    Q_DISABLE_COPY_MOVE(WindowChromeRuntime)

    [[nodiscard]] const WindowChromeSnapshot& snapshot() const;
    void observeVisibility(WindowVisibility visibility);
    void requestToggleFullscreen();
    void requestLeaveFullscreen();
    void requestToolbarReveal();
    void reportPointerMoved(bool inTopRevealArea);
    void reportTopRevealEntered();
    void reportToolbarInteractionActive(bool active);
    void reportHelpDialogOpen(bool open);

private:
    void enterFullscreen();
    void leaveFullscreen();
    void revealToolbar();
    void schedulePointerHide();
    void scheduleToolbarHide();
    void invalidatePointerTimer();
    void invalidateToolbarTimer();
    void commit(WindowChromeSnapshot snapshot);

    QObject* m_owner = nullptr;
    TimerScheduler m_timerScheduler;
    WindowChromeRuntimePorts m_ports;
    WindowChromeSnapshot m_snapshot;
    WindowVisibility m_observedVisibility = WindowVisibility::Windowed;
    WindowVisibility m_restoreVisibility = WindowVisibility::Windowed;
    bool m_toolbarInteractionActive = false;
    bool m_helpDialogOpen = false;
    quint64 m_pointerTimerGeneration = 0;
    quint64 m_toolbarTimerGeneration = 0;
    std::unique_ptr<RuntimeTimerHandle> m_pointerTimer;
    std::unique_ptr<RuntimeTimerHandle> m_toolbarTimer;
};
}

#endif
