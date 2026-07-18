// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRINDOWSHELL_H
#define KIRIVIEW_KIRINDOWSHELL_H

#include "application/windowchromeruntime.h"

#include <QObject>
#include <QPointer>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class KiriWindowShell : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("KiriWindowShell is created by the application runtime")

    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY chromeSnapshotChanged)
    Q_PROPERTY(bool pointerHidden READ pointerHidden NOTIFY chromeSnapshotChanged)
    Q_PROPERTY(bool toolbarRevealed READ toolbarRevealed NOTIFY chromeSnapshotChanged)
    Q_PROPERTY(quint64 chromeRevision READ chromeRevision NOTIFY chromeSnapshotChanged)

public:
    explicit KiriWindowShell(QObject* parent = nullptr);
    explicit KiriWindowShell(kiriview::TimerScheduler timerScheduler, QObject* parent = nullptr);

    bool fullscreen() const;
    bool pointerHidden() const;
    bool toolbarRevealed() const;
    quint64 chromeRevision() const;

    Q_INVOKABLE void attachWindow(QObject* window);
    Q_INVOKABLE void requestToggleFullscreen();
    Q_INVOKABLE void reportPointerMoved(bool inTopRevealArea);
    Q_INVOKABLE void reportTopRevealEntered();
    Q_INVOKABLE void reportToolbarInteractionActive(bool active);
    Q_INVOKABLE void reportHelpDialogOpen(bool open);

Q_SIGNALS:
    void chromeSnapshotChanged();

private:
    static kiriview::WindowVisibility runtimeVisibility(QWindow::Visibility visibility);
    static QWindow::Visibility facadeVisibility(kiriview::WindowVisibility visibility);

    QPointer<QWindow> m_window;
    QMetaObject::Connection m_visibilityConnection;
    kiriview::WindowChromeRuntime m_chromeRuntime;
};

#endif
