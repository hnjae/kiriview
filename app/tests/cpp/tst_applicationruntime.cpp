// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationdiagnostics.h"
#include "application/applicationruntime.h"
#include "application/applicationstartupsource.h"

#include <ImageViewport/imagesequenceprovider.h>
#include <ImageViewport/imageviewport.h>
#include <QByteArray>
#include <QLoggingCategory>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QUrl>

#include <memory>

namespace {
kiriview::ApplicationStartupSource startupSource(
    kiriview::ApplicationStartupSourceKind kind, const QString& text = {}, bool verbose = false)
{
    return kiriview::ApplicationStartupSource { kind, text, verbose };
}

bool categoryDebugEnabled(const QString& name)
{
    const QByteArray utf8Name = name.toUtf8();
    QLoggingCategory category(utf8Name.constData(), QtWarningMsg);
    return category.isDebugEnabled();
}

QStringList diagnosticCategoryNames()
{
    return {
        QStringLiteral("org.hnjae.kiriview.decode"),
        QStringLiteral("org.hnjae.kiriview.navigation"),
        QStringLiteral("org.hnjae.kiriview.predecode"),
        QStringLiteral("org.hnjae.kiriview.thumbnail"),
        QStringLiteral("org.hnjae.kiriview.animation"),
        QStringLiteral("org.hnjae.kiriview.video"),
    };
}
}

class TestApplicationRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cleanup();
    void startupSourceUrlIsEmptyWithoutSource();
    void startupSourceUrlUsesLocalFilePath();
    void startupSourceUrlUsesUrlText();
    void startupSourceUrlRejectsEmptyUrlText();
    void startupSourceCarriesVerboseMode();
    void runtimeDiagnosticsStayDisabledWithoutVerboseStartup();
    void runtimeDiagnosticsEnableVerboseStartupCategories();
    void registersThumbnailImageProviderOnly();
    void nonNullMainQmlRootAttachesAndSucceeds();
    void nullMainQmlRootIsTerminalAndDoesNotAttach();
    void qmlRuntimeShutdownDestroysViewportBeforeProviderCleanup();
    void startupDiagnosticRecordNeutralizesControlAndFormatContent();
    void startupDiagnosticRecordHasFixedByteBound();
};

void TestApplicationRuntime::cleanup() { QLoggingCategory::setFilterRules(QString()); }

void TestApplicationRuntime::startupSourceUrlIsEmptyWithoutSource()
{
    const QUrl url = kiriview::initialSourceUrlFromStartupSource(
        startupSource(kiriview::ApplicationStartupSourceKind::None));

    QVERIFY(url.isEmpty());
}

void TestApplicationRuntime::startupSourceUrlUsesLocalFilePath()
{
    const QString path = QStringLiteral("/tmp/kiriview/image.png");

    const QUrl url = kiriview::initialSourceUrlFromStartupSource(
        startupSource(kiriview::ApplicationStartupSourceKind::LocalFilePath, path));

    QVERIFY(url.isValid());
    QVERIFY(url.isLocalFile());
    QCOMPARE(url.toLocalFile(), path);
}

void TestApplicationRuntime::startupSourceUrlUsesUrlText()
{
    const QUrl url = kiriview::initialSourceUrlFromStartupSource(
        startupSource(kiriview::ApplicationStartupSourceKind::UrlText,
            QStringLiteral("https://example.invalid/image.png")));

    QVERIFY(url.isValid());
    QCOMPARE(url, QUrl(QStringLiteral("https://example.invalid/image.png")));
}

void TestApplicationRuntime::startupSourceUrlRejectsEmptyUrlText()
{
    const QUrl url = kiriview::initialSourceUrlFromStartupSource(
        startupSource(kiriview::ApplicationStartupSourceKind::UrlText));

    QVERIFY(url.isEmpty());
}

void TestApplicationRuntime::startupSourceCarriesVerboseMode()
{
    const kiriview::ApplicationStartupSource source
        = startupSource(kiriview::ApplicationStartupSourceKind::None, {}, true);

    QVERIFY(source.verbose);
}

void TestApplicationRuntime::runtimeDiagnosticsStayDisabledWithoutVerboseStartup()
{
    QLoggingCategory::setFilterRules(
        QStringLiteral("org.hnjae.kiriview.*.debug=false\norg.hnjae.imageviewport.*.debug=false"));

    kiriview::configureApplicationRuntimeDiagnostics(
        startupSource(kiriview::ApplicationStartupSourceKind::None));

    for (const QString& categoryName : diagnosticCategoryNames()) {
        QVERIFY2(!categoryDebugEnabled(categoryName), qPrintable(categoryName));
    }
    QVERIFY(!imageViewportProviderLog().isDebugEnabled());
}

void TestApplicationRuntime::runtimeDiagnosticsEnableVerboseStartupCategories()
{
    QLoggingCategory::setFilterRules(
        QStringLiteral("org.hnjae.kiriview.*.debug=false\norg.hnjae.imageviewport.*.debug=false"));

    kiriview::configureApplicationRuntimeDiagnostics(
        startupSource(kiriview::ApplicationStartupSourceKind::None, {}, true));

    for (const QString& categoryName : diagnosticCategoryNames()) {
        QVERIFY2(categoryDebugEnabled(categoryName), qPrintable(categoryName));
    }
    QVERIFY(imageViewportProviderLog().isDebugEnabled());
}

void TestApplicationRuntime::registersThumbnailImageProviderOnly()
{
    QQmlEngine engine;

    kiriview::registerApplicationImageProviders(engine);

    QVERIFY(engine.imageProvider(QStringLiteral("kiriview-thumbnails")) != nullptr);
    QVERIFY(engine.imageProvider(QStringLiteral("kiriview-images")) == nullptr);
}

void TestApplicationRuntime::nonNullMainQmlRootAttachesAndSucceeds()
{
    QQmlApplicationEngine engine;
    QObject* attachedRoot = nullptr;
    int attachedCount = 0;
    const QUrl mainQmlUrl(QStringLiteral("qrc:/test/main.qml"));

    const kiriview::ApplicationMainQmlLoadResult result = kiriview::loadApplicationQmlRoot(
        engine, mainQmlUrl,
        [&attachedRoot, &attachedCount](QObject& root) {
            attachedRoot = &root;
            ++attachedCount;
        },
        [](QQmlApplicationEngine& target, const QUrl& url) {
            auto* root = new QObject(&target);
            Q_EMIT target.objectCreated(root, url);
        });

    QCOMPARE(result, kiriview::ApplicationMainQmlLoadResult::Created);
    QCOMPARE(attachedCount, 1);
    QVERIFY(attachedRoot != nullptr);
    QCOMPARE(attachedRoot->parent(), &engine);
}

void TestApplicationRuntime::nullMainQmlRootIsTerminalAndDoesNotAttach()
{
    QQmlApplicationEngine engine;
    int attachedCount = 0;
    const QUrl mainQmlUrl(QStringLiteral("qrc:/test/missing-main.qml"));

    const kiriview::ApplicationMainQmlLoadResult result = kiriview::loadApplicationQmlRoot(
        engine, mainQmlUrl, [&attachedCount](QObject&) { ++attachedCount; },
        [](QQmlApplicationEngine& target, const QUrl& url) {
            Q_EMIT target.objectCreated(nullptr, url);
            Q_EMIT target.objectCreationFailed(url);
        });

    QCOMPARE(result, kiriview::ApplicationMainQmlLoadResult::Failed);
    QCOMPARE(attachedCount, 0);
}

void TestApplicationRuntime::qmlRuntimeShutdownDestroysViewportBeforeProviderCleanup()
{
    auto engine = std::make_unique<QQmlApplicationEngine>();
    QPointer<ImageViewport> viewport(new ImageViewport);
    viewport->setParent(engine.get());
    bool viewportDestroyedBeforeCleanup = false;

    QVERIFY(kiriview::shutdownApplicationQmlRuntime(std::move(engine), [&]() {
        viewportDestroyedBeforeCleanup = viewport.isNull();
        return true;
    }));

    QVERIFY(viewportDestroyedBeforeCleanup);
    QVERIFY(viewport.isNull());
}

void TestApplicationRuntime::startupDiagnosticRecordNeutralizesControlAndFormatContent()
{
    const QString hostile
        = QStringLiteral("missing\\name\nnext\rline\t\x1b[31m\u202ehidden\u2028record");

    const QByteArray record = kiriview::applicationStartupDiagnosticRecord(hostile);

    QVERIFY(record.endsWith('\n'));
    QCOMPARE(record.count('\n'), 1);
    QVERIFY(!record.contains('\r'));
    QVERIFY(!record.contains('\t'));
    QVERIFY(!record.contains('\x1b'));
    QVERIFY(record.contains("\\\\"));
    QVERIFY(record.contains("\\n"));
    QVERIFY(record.contains("\\r"));
    QVERIFY(record.contains("\\t"));
    QVERIFY(!record.contains(QStringLiteral("\u202e").toUtf8()));
    QVERIFY(!record.contains(QStringLiteral("\u2028").toUtf8()));
    QVERIFY(record.contains("\\u202E"));
    QVERIFY(record.contains("\\u2028"));
}

void TestApplicationRuntime::startupDiagnosticRecordHasFixedByteBound()
{
    const QString hostile = QStringLiteral("very-long-missing-path\n").repeated(16'384);

    const QByteArray record = kiriview::applicationStartupDiagnosticRecord(hostile);

    QVERIFY(record.size() <= kiriview::maximumApplicationStartupDiagnosticBytes);
    QCOMPARE(record.count('\n'), 1);
    QVERIFY(record.contains("[truncated]"));
}

QTEST_GUILESS_MAIN(TestApplicationRuntime)

#include "tst_applicationruntime.moc"
