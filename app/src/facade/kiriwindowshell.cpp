// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiriwindowshell.h"

#include "facade/activenavigationboundaryfacts.h"
#include "facade/kiridocumentsession.h"
#include "facade/kiriimagedocument.h"
#include "facade/kiriviewapplication.h"

#include <KLocalizedString>
#include <QGuiApplication>
#include <QWindow>
#include <utility>

namespace {
void disconnectConnections(std::vector<QMetaObject::Connection>* connections)
{
    for (const QMetaObject::Connection& connection : *connections) {
        QObject::disconnect(connection);
    }
    connections->clear();
}
}

KiriWindowShell::KiriWindowShell(QObject* parent)
    : KiriWindowShell(kiriview::TimerScheduler {}, parent)
{
}

KiriWindowShell::KiriWindowShell(kiriview::TimerScheduler timerScheduler, QObject* parent)
    : QObject(parent)
    , m_chromeRuntime(this, timerScheduler,
          {
              [this](kiriview::WindowVisibility visibility) {
                  if (m_window != nullptr) {
                      m_window->setVisibility(facadeVisibility(visibility));
                  }
              },
              [this]() { Q_EMIT chromeSnapshotChanged(); },
          })
    , m_notificationRuntime(this, std::move(timerScheduler), [this]() {
        if (!m_notificationRuntime.snapshot().active) {
            resetNavigationBoundaryCorrelation();
        }
        Q_EMIT notificationSnapshotChanged();
    })
{
    refreshWindowTitle();
}

bool KiriWindowShell::fullscreen() const { return m_chromeRuntime.snapshot().fullscreen; }

QString KiriWindowShell::windowTitle() const { return m_windowTitle; }

bool KiriWindowShell::pointerHidden() const { return m_chromeRuntime.snapshot().pointerHidden; }

bool KiriWindowShell::toolbarRevealed() const { return m_chromeRuntime.snapshot().toolbarRevealed; }

int KiriWindowShell::chromeRevision() const
{
    return static_cast<int>(m_chromeRuntime.snapshot().revision);
}

bool KiriWindowShell::notificationActive() const { return m_notificationRuntime.snapshot().active; }

QString KiriWindowShell::notificationMessage() const
{
    return m_notificationRuntime.snapshot().message;
}

int KiriWindowShell::notificationReplayRevision() const
{
    return static_cast<int>(m_notificationRuntime.snapshot().replayRevision);
}

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

void KiriWindowShell::attachApplication(QObject* application)
{
    auto* kiriApplication = qobject_cast<KiriViewApplication*>(application);
    if (m_application == kiriApplication) {
        return;
    }

    disconnectConnections(&m_applicationConnections);
    m_application = kiriApplication;
    if (m_application == nullptr) {
        return;
    }

    m_applicationConnections.push_back(QObject::connect(kiriApplication,
        &KiriViewApplication::imageBoundaryReached, this,
        [this](const QString& message, const kiriview::NavigationBoundaryCorrelation& correlation,
            KiriDocumentSession* originSession) {
            submitActiveNavigationBoundaryNotification(message, correlation, originSession);
        }));
    m_applicationConnections.push_back(
        QObject::connect(kiriApplication, &KiriViewApplication::unsupportedVideoActionTriggered,
            this, [this](KiriViewApplication::ActionId) {
                submitNotification(kiriview::WindowNotificationScope::UnsupportedAction,
                    i18nc("@info:status", "This action is not available for videos"));
            }));
    m_applicationConnections.push_back(
        QObject::connect(kiriApplication, &KiriViewApplication::unsupportedImageActionTriggered,
            this, [this](KiriViewApplication::ActionId) {
                submitNotification(kiriview::WindowNotificationScope::UnsupportedAction,
                    i18nc("@info:status", "This action is not available for images"));
            }));
}

void KiriWindowShell::attachDocumentSession(QObject* session)
{
    auto* documentSession = qobject_cast<KiriDocumentSession*>(session);
    if (m_documentSession == documentSession) {
        return;
    }

    clearNavigationBoundaryNotification();
    disconnectConnections(&m_documentSessionConnections);
    m_documentSession = documentSession;
    if (m_documentSession == nullptr) {
        refreshWindowTitle();
        return;
    }

    m_documentSessionConnections.push_back(
        QObject::connect(documentSession, &QObject::destroyed, this, [this]() {
            m_documentSession.clear();
            clearNavigationBoundaryNotification();
            refreshWindowTitle();
        }));
    m_documentSessionConnections.push_back(QObject::connect(documentSession,
        &KiriDocumentSession::windowTitleSubjectChanged, this, [this]() { refreshWindowTitle(); }));
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    m_documentSessionConnections.push_back(
        QObject::connect(documentSession, &KiriDocumentSession::sourceUrlChanged, this,
            [this]() { clearNavigationBoundaryNotification(); }));
    m_documentSessionConnections.push_back(
        QObject::connect(documentSession, &KiriDocumentSession::activeNavigationChanged, this,
            [this]() { reconcileNavigationBoundaryNotification(); }));
    m_documentSessionConnections.push_back(QObject::connect(documentSession,
        &KiriDocumentSession::fileDeletionFailed, this, [this](const QString& message) {
            submitNotification(kiriview::WindowNotificationScope::OperationFailure, message);
        }));
    m_documentSessionConnections.push_back(QObject::connect(documentSession,
        &KiriDocumentSession::openWithFailed, this, [this](const QString& message) {
            submitNotification(kiriview::WindowNotificationScope::OperationFailure,
                message.isEmpty()
                    ? i18nc("@info:status", "Could not open media with another application")
                    : message);
        }));
    if (imageDocument == nullptr) {
        refreshWindowTitle();
        return;
    }

    m_documentSessionConnections.push_back(
        QObject::connect(imageDocument, &KiriImageDocument::containerNavigationBoundaryReached,
            this, [this](const QString& message) {
                submitNotification(kiriview::WindowNotificationScope::NavigationBoundary, message);
            }));
    m_documentSessionConnections.push_back(
        QObject::connect(imageDocument, &KiriImageDocument::unsupportedOpenedCollectionVideoEntered,
            this, [this](const QString& message) {
                submitNotification(kiriview::WindowNotificationScope::UnsupportedMedia, message);
            }));
    refreshWindowTitle();
}

void KiriWindowShell::requestToggleFullscreen() { m_chromeRuntime.requestToggleFullscreen(); }

void KiriWindowShell::requestLeaveFullscreen() { m_chromeRuntime.requestLeaveFullscreen(); }

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

void KiriWindowShell::dismissNotification() { m_notificationRuntime.dismiss(); }

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

void KiriWindowShell::submitNotification(
    kiriview::WindowNotificationScope scope, const QString& message)
{
    resetNavigationBoundaryCorrelation();
    m_notificationRuntime.submit({ scope, message });
}

void KiriWindowShell::submitActiveNavigationBoundaryNotification(const QString& message,
    const kiriview::NavigationBoundaryCorrelation& correlation,
    const KiriDocumentSession* originSession)
{
    if (message.isEmpty() || originSession == nullptr || originSession != m_documentSession) {
        return;
    }

    if (!kiriview::navigationBoundaryCorrelationIsCurrent(
            correlation, kiriview::activeNavigationBoundaryFacts(m_documentSession))) {
        return;
    }
    m_navigationBoundaryCorrelation = correlation;

    m_notificationRuntime.submit(
        { kiriview::WindowNotificationScope::NavigationBoundary, message });
}

void KiriWindowShell::clearNavigationBoundaryNotification()
{
    m_notificationRuntime.clear(kiriview::WindowNotificationScope::NavigationBoundary);
    resetNavigationBoundaryCorrelation();
}

void KiriWindowShell::reconcileNavigationBoundaryNotification()
{
    if (!m_navigationBoundaryCorrelation.has_value() || !m_notificationRuntime.snapshot().active
        || m_notificationRuntime.snapshot().scope
            != kiriview::WindowNotificationScope::NavigationBoundary) {
        return;
    }

    if (!kiriview::navigationBoundaryCorrelationIsCurrent(*m_navigationBoundaryCorrelation,
            kiriview::activeNavigationBoundaryFacts(m_documentSession))) {
        clearNavigationBoundaryNotification();
    }
}

void KiriWindowShell::resetNavigationBoundaryCorrelation()
{
    m_navigationBoundaryCorrelation.reset();
}

void KiriWindowShell::refreshWindowTitle()
{
    QString applicationName = QGuiApplication::applicationDisplayName();
    if (applicationName.isEmpty()) {
        applicationName = i18nc("@title:application", "KiriView");
    }
    const QString subject
        = m_documentSession == nullptr ? QString() : m_documentSession->windowTitleSubject();
    const QString title = subject.isEmpty()
        ? applicationName
        : i18nc("@title:window", "%1 — %2", subject, applicationName);
    if (m_windowTitle == title) {
        return;
    }
    m_windowTitle = title;
    Q_EMIT windowTitleChanged();
}
