// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "windowchromeruntime.h"

#include <utility>

namespace {
constexpr kiriview::TimerDuration fullscreenChromeTimeout { 1000 };

bool restorable(kiriview::WindowVisibility visibility)
{
    return visibility == kiriview::WindowVisibility::Windowed
        || visibility == kiriview::WindowVisibility::Maximized
        || visibility == kiriview::WindowVisibility::Minimized;
}
}

namespace kiriview {
WindowChromeRuntime::WindowChromeRuntime(
    QObject* owner, TimerScheduler timerScheduler, WindowChromeRuntimePorts ports)
    : m_owner(owner)
    , m_timerScheduler(timerSchedulerWithDefaults(std::move(timerScheduler)))
    , m_ports(std::move(ports))
{
}

WindowChromeRuntime::~WindowChromeRuntime()
{
    invalidatePointerTimer();
    invalidateToolbarTimer();
}

const WindowChromeSnapshot& WindowChromeRuntime::snapshot() const { return m_snapshot; }

void WindowChromeRuntime::observeVisibility(WindowVisibility visibility)
{
    m_observedVisibility = visibility;
    if (visibility == WindowVisibility::Fullscreen) {
        if (!m_snapshot.fullscreen) {
            enterFullscreen();
        }
        return;
    }

    if (restorable(visibility)) {
        m_restoreVisibility = visibility;
    }
    if (m_snapshot.fullscreen) {
        leaveFullscreen();
    }
}

void WindowChromeRuntime::requestToggleFullscreen()
{
    if (!m_ports.applyVisibility) {
        return;
    }

    m_ports.applyVisibility(
        m_snapshot.fullscreen ? m_restoreVisibility : WindowVisibility::Fullscreen);
}

void WindowChromeRuntime::requestLeaveFullscreen()
{
    if (m_snapshot.fullscreen && m_ports.applyVisibility) {
        m_ports.applyVisibility(m_restoreVisibility);
    }
}

void WindowChromeRuntime::requestToolbarReveal() { revealToolbar(); }

void WindowChromeRuntime::reportPointerMoved(bool inTopRevealArea)
{
    if (!m_snapshot.fullscreen) {
        return;
    }

    WindowChromeSnapshot next = m_snapshot;
    next.pointerHidden = false;
    commit(next);
    schedulePointerHide();

    if (inTopRevealArea) {
        revealToolbar();
    } else {
        scheduleToolbarHide();
    }
}

void WindowChromeRuntime::reportTopRevealEntered()
{
    if (m_snapshot.fullscreen && !m_helpDialogOpen) {
        revealToolbar();
    }
}

void WindowChromeRuntime::reportToolbarInteractionActive(bool active)
{
    if (m_toolbarInteractionActive == active) {
        return;
    }

    m_toolbarInteractionActive = active;
    if (!m_snapshot.fullscreen) {
        return;
    }
    if (active) {
        revealToolbar();
        invalidateToolbarTimer();
    } else {
        scheduleToolbarHide();
    }
}

void WindowChromeRuntime::reportHelpDialogOpen(bool open) { m_helpDialogOpen = open; }

void WindowChromeRuntime::enterFullscreen()
{
    if (restorable(m_observedVisibility)) {
        m_restoreVisibility = m_observedVisibility;
    }

    invalidatePointerTimer();
    WindowChromeSnapshot next = m_snapshot;
    next.fullscreen = true;
    next.pointerHidden = true;
    next.toolbarRevealed = true;
    commit(next);
    scheduleToolbarHide();
}

void WindowChromeRuntime::leaveFullscreen()
{
    invalidatePointerTimer();
    invalidateToolbarTimer();
    m_toolbarInteractionActive = false;

    WindowChromeSnapshot next = m_snapshot;
    next.fullscreen = false;
    next.pointerHidden = false;
    next.toolbarRevealed = false;
    commit(next);
}

void WindowChromeRuntime::revealToolbar()
{
    if (!m_snapshot.fullscreen || m_helpDialogOpen) {
        return;
    }

    WindowChromeSnapshot next = m_snapshot;
    next.toolbarRevealed = true;
    commit(next);
    scheduleToolbarHide();
}

void WindowChromeRuntime::schedulePointerHide()
{
    invalidatePointerTimer();
    const quint64 generation = m_pointerTimerGeneration;
    m_pointerTimer
        = m_timerScheduler.singleShotTimer(m_owner, fullscreenChromeTimeout, [this, generation]() {
              if (generation != m_pointerTimerGeneration || !m_snapshot.fullscreen) {
                  return;
              }

              WindowChromeSnapshot next = m_snapshot;
              next.pointerHidden = true;
              if (!m_toolbarInteractionActive) {
                  next.toolbarRevealed = false;
              }
              commit(next);
          });
    m_pointerTimer->start(fullscreenChromeTimeout);
}

void WindowChromeRuntime::scheduleToolbarHide()
{
    invalidateToolbarTimer();
    if (!m_snapshot.fullscreen || !m_snapshot.toolbarRevealed || m_toolbarInteractionActive) {
        return;
    }

    const quint64 generation = m_toolbarTimerGeneration;
    m_toolbarTimer
        = m_timerScheduler.singleShotTimer(m_owner, fullscreenChromeTimeout, [this, generation]() {
              if (generation != m_toolbarTimerGeneration || !m_snapshot.fullscreen
                  || m_toolbarInteractionActive) {
                  return;
              }

              WindowChromeSnapshot next = m_snapshot;
              next.toolbarRevealed = false;
              commit(next);
          });
    m_toolbarTimer->start(fullscreenChromeTimeout);
}

void WindowChromeRuntime::invalidatePointerTimer()
{
    ++m_pointerTimerGeneration;
    if (m_pointerTimer) {
        m_pointerTimer->stop();
        m_pointerTimer.reset();
    }
}

void WindowChromeRuntime::invalidateToolbarTimer()
{
    ++m_toolbarTimerGeneration;
    if (m_toolbarTimer) {
        m_toolbarTimer->stop();
        m_toolbarTimer.reset();
    }
}

void WindowChromeRuntime::commit(WindowChromeSnapshot snapshot)
{
    snapshot.revision = m_snapshot.revision;
    if (snapshot == m_snapshot) {
        return;
    }

    snapshot.revision = m_snapshot.revision + 1;
    m_snapshot = snapshot;
    if (m_ports.snapshotChanged) {
        m_ports.snapshotChanged();
    }
}
}
