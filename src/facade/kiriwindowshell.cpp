// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiriwindowshell.h"

#include <QWindow>
#include <utility>

KiriWindowShell::KiriWindowShell(QObject* parent)
    : KiriWindowShell(kiriview::TimerScheduler {}, parent)
{
}

KiriWindowShell::KiriWindowShell(kiriview::TimerScheduler timerScheduler, QObject* parent)
    : QObject(parent)
    , m_chromeRuntime(this, std::move(timerScheduler),
          {
              [this](kiriview::WindowVisibility visibility) {
                  if (m_window != nullptr) {
                      m_window->setVisibility(facadeVisibility(visibility));
                  }
              },
              [this]() { Q_EMIT chromeSnapshotChanged(); },
          })
{
}

bool KiriWindowShell::fullscreen() const { return m_chromeRuntime.snapshot().fullscreen; }

bool KiriWindowShell::pointerHidden() const { return m_chromeRuntime.snapshot().pointerHidden; }

bool KiriWindowShell::toolbarRevealed() const { return m_chromeRuntime.snapshot().toolbarRevealed; }

quint64 KiriWindowShell::chromeRevision() const { return m_chromeRuntime.snapshot().revision; }

void KiriWindowShell::attachWindow(QObject* window)
{
    QWindow* nextWindow = qobject_cast<QWindow*>(window);
    if (m_window == nextWindow) {
        return;
    }

    QObject::disconnect(m_visibilityConnection);
    m_window = nextWindow;
    if (m_window == nullptr) {
        return;
    }

    m_visibilityConnection = QObject::connect(
        m_window, &QWindow::visibilityChanged, this, [this](QWindow::Visibility visibility) {
            m_chromeRuntime.observeVisibility(runtimeVisibility(visibility));
        });
    m_chromeRuntime.observeVisibility(runtimeVisibility(m_window->visibility()));
}

void KiriWindowShell::requestToggleFullscreen() { m_chromeRuntime.requestToggleFullscreen(); }

void KiriWindowShell::reportPointerMoved(bool inTopRevealArea)
{
    m_chromeRuntime.reportPointerMoved(inTopRevealArea);
}

void KiriWindowShell::reportTopRevealEntered() { m_chromeRuntime.reportTopRevealEntered(); }

void KiriWindowShell::reportToolbarInteractionActive(bool active)
{
    m_chromeRuntime.reportToolbarInteractionActive(active);
}

void KiriWindowShell::reportHelpDialogOpen(bool open)
{
    m_chromeRuntime.reportHelpDialogOpen(open);
}

kiriview::WindowVisibility KiriWindowShell::runtimeVisibility(QWindow::Visibility visibility)
{
    switch (visibility) {
    case QWindow::Maximized:
        return kiriview::WindowVisibility::Maximized;
    case QWindow::Minimized:
        return kiriview::WindowVisibility::Minimized;
    case QWindow::FullScreen:
        return kiriview::WindowVisibility::Fullscreen;
    case QWindow::Hidden:
    case QWindow::AutomaticVisibility:
    case QWindow::Windowed:
        return kiriview::WindowVisibility::Windowed;
    }
    return kiriview::WindowVisibility::Windowed;
}

QWindow::Visibility KiriWindowShell::facadeVisibility(kiriview::WindowVisibility visibility)
{
    switch (visibility) {
    case kiriview::WindowVisibility::Maximized:
        return QWindow::Maximized;
    case kiriview::WindowVisibility::Minimized:
        return QWindow::Minimized;
    case kiriview::WindowVisibility::Fullscreen:
        return QWindow::FullScreen;
    case kiriview::WindowVisibility::Windowed:
        return QWindow::Windowed;
    }
    return QWindow::Windowed;
}
