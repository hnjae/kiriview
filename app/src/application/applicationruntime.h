// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APPLICATIONRUNTIME_H
#define KIRIVIEW_APPLICATIONRUNTIME_H

#include <functional>
#include <memory>

class QQmlApplicationEngine;
class QQmlEngine;
class QObject;
class QUrl;
class KiriDocumentSession;
class KiriViewApplication;
class KiriWindowShell;

namespace kiriview {
struct ApplicationStartupSource;

enum class ApplicationMainQmlLoadResult {
    Created,
    Failed,
};

using ApplicationMainQmlRootCallback = std::function<void(QObject&)>;
using ApplicationMainQmlLoader = std::function<void(QQmlApplicationEngine&, const QUrl&)>;

void initializeApplicationRuntime();
void configureApplicationRuntimeDiagnostics(const ApplicationStartupSource& startupSource);
void registerApplicationImageProviders(QQmlEngine& engine);
void composeApplicationRuntimeGraph(KiriViewApplication& application,
    KiriDocumentSession& documentSession, KiriWindowShell& windowShell);
void attachApplicationRuntimeWindow(
    KiriViewApplication& application, KiriWindowShell& windowShell, QObject& window);
[[nodiscard]] ApplicationMainQmlLoadResult loadApplicationQmlRoot(QQmlApplicationEngine& engine,
    const QUrl& mainQmlUrl, const ApplicationMainQmlRootCallback& rootCallback,
    const ApplicationMainQmlLoader& loader = {});
[[nodiscard]] ApplicationMainQmlLoadResult loadApplicationMainQml(
    QQmlApplicationEngine& engine, const ApplicationStartupSource& startupSource);
[[nodiscard]] bool shutdownApplicationQmlRuntime(std::unique_ptr<QQmlApplicationEngine> engine,
    const std::function<bool()>& providerCleanupCompletion = {});
int runApplication(const ApplicationStartupSource& startupSource);
}

#endif
