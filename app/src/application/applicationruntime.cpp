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
#include <QPointer>
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

ApplicationMainQmlLoadResult loadApplicationQmlRoot(QQmlApplicationEngine& engine,
    const QUrl& mainQmlUrl, const ApplicationMainQmlRootCallback& rootCallback,
    const ApplicationMainQmlLoader& loader)
{
    QPointer<QObject> rootObject;
    bool creationFailed = false;
    const QMetaObject::Connection createdConnection = QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &engine,
        [&rootObject, &creationFailed, mainQmlUrl](QObject* createdObject, const QUrl& objectUrl) {
            if (objectUrl != mainQmlUrl) {
                return;
            }
            if (createdObject == nullptr) {
                creationFailed = true;
                return;
            }
            rootObject = createdObject;
        },
        Qt::DirectConnection);
    const QMetaObject::Connection failedConnection = QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &engine,
        [&creationFailed, mainQmlUrl](const QUrl& objectUrl) {
            if (objectUrl == mainQmlUrl) {
                creationFailed = true;
            }
        },
        Qt::DirectConnection);

    if (loader) {
        loader(engine, mainQmlUrl);
    } else {
        engine.load(mainQmlUrl);
    }

    QObject::disconnect(createdConnection);
    QObject::disconnect(failedConnection);
    if (creationFailed || rootObject.isNull()) {
        return ApplicationMainQmlLoadResult::Failed;
    }

    if (rootCallback) {
        rootCallback(*rootObject);
    }
    return ApplicationMainQmlLoadResult::Created;
}

ApplicationMainQmlLoadResult loadApplicationMainQml(
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
    return loadApplicationQmlRoot(engine, mainQmlUrl,
        [application, documentSession, windowShell, initialSourceUrl](QObject& rootObject) {
            attachApplicationRuntimeWindow(*application, *windowShell, rootObject);
            if (!initialSourceUrl.isEmpty()) {
                documentSession->setSourceUrl(initialSourceUrl);
            }
        });
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
    if (loadApplicationMainQml(engine, startupSource) != ApplicationMainQmlLoadResult::Created) {
        writeApplicationStartupDiagnostic(QStringLiteral("failed to create the main window"));
        return 1;
    }

    return application.exec();
}
}
