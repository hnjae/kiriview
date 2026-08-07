// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRINDOWSHELL_H
#define KIRIVIEW_KIRINDOWSHELL_H

#include "application/windowchromeruntime.h"
#include "application/windownotificationruntime.h"

#include <QObject>
#include <QPointer>
#include <QWindow>
#include <QtQml/qqmlregistration.h>
#include <vector>

class KiriWindowShell : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("KiriWindowShell is created by the application runtime")

    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY chromeSnapshotChanged)
    Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY windowTitleChanged)
    Q_PROPERTY(bool pointerHidden READ pointerHidden NOTIFY chromeSnapshotChanged)
    Q_PROPERTY(bool toolbarRevealed READ toolbarRevealed NOTIFY chromeSnapshotChanged)
    Q_PROPERTY(int chromeRevision READ chromeRevision NOTIFY chromeSnapshotChanged)
    Q_PROPERTY(bool notificationActive READ notificationActive NOTIFY notificationSnapshotChanged)
    Q_PROPERTY(
        QString notificationMessage READ notificationMessage NOTIFY notificationSnapshotChanged)
    Q_PROPERTY(int notificationReplayRevision READ notificationReplayRevision NOTIFY
            notificationSnapshotChanged)

public:
    explicit KiriWindowShell(QObject* parent = nullptr);
    explicit KiriWindowShell(kiriview::TimerScheduler timerScheduler, QObject* parent = nullptr);

    bool fullscreen() const;
    QString windowTitle() const;
    bool pointerHidden() const;
    bool toolbarRevealed() const;
    int chromeRevision() const;
    bool notificationActive() const;
    QString notificationMessage() const;
    int notificationReplayRevision() const;

    void attachWindow(QObject* window);
    void attachApplication(QObject* application);
    void attachDocumentSession(QObject* session);
    Q_INVOKABLE void requestToggleFullscreen();
    Q_INVOKABLE void requestLeaveFullscreen();
    Q_INVOKABLE void reportPointerMoved(bool inTopRevealArea);
    Q_INVOKABLE void reportTopRevealEntered();
    Q_INVOKABLE void reportToolbarInteractionActive(bool active);
    Q_INVOKABLE void reportHelpDialogOpen(bool open);
    Q_INVOKABLE void dismissNotification();

Q_SIGNALS:
    void windowTitleChanged();
    void chromeSnapshotChanged();
    void notificationSnapshotChanged();

private:
    static kiriview::WindowVisibility runtimeVisibility(QWindow::Visibility visibility);
    static QWindow::Visibility facadeVisibility(kiriview::WindowVisibility visibility);
    void refreshWindowTitle();
    void submitNotification(kiriview::WindowNotificationScope scope, const QString& message);
    void clearNavigationBoundaryNotification();

    QPointer<QWindow> m_window;
    QPointer<class KiriViewApplication> m_application;
    QPointer<class KiriDocumentSession> m_documentSession;
    QMetaObject::Connection m_visibilityConnection;
    std::vector<QMetaObject::Connection> m_applicationConnections;
    std::vector<QMetaObject::Connection> m_documentSessionConnections;
    QString m_windowTitle;
    kiriview::WindowChromeRuntime m_chromeRuntime;
    kiriview::WindowNotificationRuntime m_notificationRuntime;
};

#endif
