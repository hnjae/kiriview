// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "applicationruntime.h"

#include "applicationdiagnostics.h"
#include "applicationstartupsource.h"
#include "facade/kiridocumentsession.h"
#include "facade/kiriviewapplication.h"
#include "facade/kiriwindowshell.h"
#include "generated/applicationidentity.h"
#include "localization/localization.h"
#include "session/thumbnailimagestore.h"
#include "system/powersaverprovider.h"

#include <KLocalizedString>
#include <QApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QtGlobal>
#include <array>
#include <utility>

namespace {
void setupApplicationIdentity()
{
    QGuiApplication::setDesktopFileName(QString::fromLatin1(kiriview::application_identity::id));
    QGuiApplication::setApplicationDisplayName(i18nc("@title:application", "KiriView"));
}

void setupDefaultQuickStyle()
{
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
}
}

namespace kiriview {
void initializeApplicationRuntime()
{
    initializeLocalization();
    setupApplicationIdentity();
    setupDefaultQuickStyle();
}

void configureApplicationRuntimeDiagnostics(const ApplicationStartupSource& startupSource)
{
    configureApplicationDiagnosticLogging(startupSource.verbose);
}

void registerApplicationImageProviders(QQmlEngine& engine)
{
    engine.addImageProvider(QStringLiteral("kiriview-thumbnails"),
        new ThumbnailImageProvider(sharedThumbnailImageStore()));
}

void composeApplicationRuntimeGraph(KiriViewApplication& application,
    KiriDocumentSession& documentSession, KiriWindowShell& windowShell)
{
    application.setDocumentSession(&documentSession);
    application.setWindowShell(&windowShell);
    windowShell.attachApplication(&application);
    windowShell.attachDocumentSession(&documentSession);
}

void attachApplicationRuntimeWindow(
    KiriViewApplication& application, KiriWindowShell& windowShell, QObject& window)
{
    application.setShortcutHost(&window);
    windowShell.attachWindow(&window);
}

void loadApplicationMainQml(
    QQmlApplicationEngine& engine, const ApplicationStartupSource& startupSource)
{
    setupLocalizedContext(engine);
    registerApplicationImageProviders(engine);

    auto* powerSaverRuntime = new PowerSaverRuntime(&engine);
    KiriDocumentSessionDependencies documentSessionDependencies;
    const PowerSaverProvider powerSaverProvider = powerSaverRuntime->provider();
    documentSessionDependencies.imageDocument.powerSaver = powerSaverProvider;
    documentSessionDependencies.sessionRuntime.directMediaPredecodeDependencies.powerSaver
        = powerSaverProvider;
    auto* documentSession
        = new KiriDocumentSession(std::move(documentSessionDependencies), &engine);
    documentSession->setObjectName(QStringLiteral("documentSession"));
    auto* windowShell = new KiriWindowShell(&engine);
    auto* application = new KiriViewApplication(&engine);
    composeApplicationRuntimeGraph(*application, *documentSession, *windowShell);

    const QUrl initialSourceUrl = initialSourceUrlFromStartupSource(startupSource);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("kiriApplication"), QVariant::fromValue(application));
    initialProperties.insert(
        QStringLiteral("documentSession"), QVariant::fromValue(documentSession));
    initialProperties.insert(QStringLiteral("windowShell"), QVariant::fromValue(windowShell));
    engine.setInitialProperties(initialProperties);

    const QUrl mainQmlUrl(QString::fromLatin1(kiriview::application_identity::mainQmlUrl));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &engine,
        [application, documentSession, windowShell, initialSourceUrl](
            QObject* rootObject, const QUrl&) {
            if (rootObject != nullptr) {
                attachApplicationRuntimeWindow(*application, *windowShell, *rootObject);
                if (!initialSourceUrl.isEmpty()) {
                    documentSession->setSourceUrl(initialSourceUrl);
                }
            }
        },
        Qt::SingleShotConnection);
    engine.load(mainQmlUrl);
}

int runApplication(const ApplicationStartupSource& startupSource)
{
    std::array<char, 9> applicationName { 'k', 'i', 'r', 'i', 'v', 'i', 'e', 'w', '\0' };
    char* arguments[] = { applicationName.data() };
    int argumentCount = 1;

    QApplication application(argumentCount, arguments);
    initializeApplicationRuntime();
    configureApplicationRuntimeDiagnostics(startupSource);

    QQmlApplicationEngine engine;
    loadApplicationMainQml(engine, startupSource);

    return application.exec();
}
}
