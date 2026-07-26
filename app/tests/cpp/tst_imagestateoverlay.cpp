// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiridocumentsession.h"
#include "facade/kiriimagedocument.h"
#include "localization/localization.h"

#include <KLocalizedQmlContext>
#include <QDir>
#include <QImage>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtQml/qqml.h>
#include <memory>

namespace {
void addEnvironmentImportPaths(QQmlEngine& engine)
{
    const QString paths = qEnvironmentVariable("NIXPKGS_QML_SEARCH_PATHS");
    for (const QString& path : paths.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        engine.addImportPath(path);
    }
}

void registerKiriViewQmlTypes()
{
    static bool registered = false;
    if (registered) {
        return;
    }

    kiriview::initializeLocalization();
    qmlRegisterType<KiriImageDocument>("org.hnjae.kiriview", 1, 0, "KiriImageDocument");
    registered = true;
}

QUrl imageStateOverlayQmlUrl()
{
    return QUrl::fromLocalFile(QDir(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR))
            .absoluteFilePath(QStringLiteral("../../src/qml/ImageStateOverlay.qml")));
}

struct ImageStateOverlayFixture
{
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<KiriDocumentSession> documentSession;
    std::unique_ptr<QObject> root;
    QString errorString;
};

ImageStateOverlayFixture createFixture()
{
    ImageStateOverlayFixture fixture;
    registerKiriViewQmlTypes();
    fixture.engine = std::make_unique<QQmlEngine>();
    addEnvironmentImportPaths(*fixture.engine);
    KLocalization::setupLocalizedContext(fixture.engine.get());
    fixture.documentSession = std::make_unique<KiriDocumentSession>();

    QQmlComponent component(fixture.engine.get(), imageStateOverlayQmlUrl());
    const QVariantMap properties {
        { QStringLiteral("imageDocument"),
            QVariant::fromValue(static_cast<QObject*>(fixture.documentSession->imageDocument())) },
        { QStringLiteral("imageReady"), false },
        { QStringLiteral("openAction"), QVariant::fromValue(static_cast<QObject*>(nullptr)) },
        { QStringLiteral("unsupportedOpenedCollectionVideo"), false },
    };
    fixture.root.reset(component.createWithInitialProperties(properties));
    if (fixture.root == nullptr) {
        fixture.errorString = component.errorString();
    }
    return fixture;
}

bool loadingFeedbackVisible(const QObject& overlay)
{
    return overlay.property("loadingFeedbackVisible").toBool();
}
}

class TestImageStateOverlay : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void loadingFeedbackIsDelayedAndCancelledByTerminalState();
    void targetLifecycleTokenRestartsLoadingFeedbackDelay();
};

void TestImageStateOverlay::initTestCase()
{
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
}

void TestImageStateOverlay::loadingFeedbackIsDelayedAndCancelledByTerminalState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("first.png"));
    const QString secondPath = directory.filePath(QStringLiteral("second.png"));
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(QColor(20, 40, 60, 255));
    QVERIFY(image.save(firstPath));
    QVERIFY(image.save(secondPath));

    ImageStateOverlayFixture fixture = createFixture();
    QVERIFY2(fixture.root != nullptr, qPrintable(fixture.errorString));
    QVERIFY(!loadingFeedbackVisible(*fixture.root));

    fixture.documentSession->setSourceUrl(QUrl::fromLocalFile(firstPath));
    QTRY_COMPARE(
        fixture.documentSession->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QVERIFY(!loadingFeedbackVisible(*fixture.root));

    fixture.documentSession->setSourceUrl(QUrl());
    QTRY_COMPARE(
        fixture.documentSession->imageDocument()->status(), KiriImageDocument::Status::Null);
    QTest::qWait(200);
    QVERIFY(!loadingFeedbackVisible(*fixture.root));

    fixture.documentSession->setSourceUrl(QUrl::fromLocalFile(secondPath));
    QTRY_COMPARE(
        fixture.documentSession->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QTRY_VERIFY_WITH_TIMEOUT(loadingFeedbackVisible(*fixture.root), 1000);

    fixture.documentSession->setSourceUrl(QUrl());
    QTRY_COMPARE(
        fixture.documentSession->imageDocument()->status(), KiriImageDocument::Status::Null);
    QVERIFY(!loadingFeedbackVisible(*fixture.root));
}

void TestImageStateOverlay::targetLifecycleTokenRestartsLoadingFeedbackDelay()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl firstUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("first.png")));
    const QUrl secondUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("second.png")));
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(QColor(20, 40, 60, 255));
    QVERIFY(image.save(firstUrl.toLocalFile()));
    QVERIFY(image.save(secondUrl.toLocalFile()));

    ImageStateOverlayFixture fixture = createFixture();
    QVERIFY2(fixture.root != nullptr, qPrintable(fixture.errorString));

    fixture.documentSession->setSourceUrl(firstUrl);
    QTRY_COMPARE(
        fixture.documentSession->imageDocument()->status(), KiriImageDocument::Status::Loading);
    const QString firstToken = fixture.documentSession->imageDocument()->loadingTargetToken();
    const QString firstTargetKey = fixture.root->property("loadingTargetKey").toString();
    QVERIFY(!firstToken.isEmpty());
    QVERIFY(!firstTargetKey.isEmpty());

    fixture.documentSession->setSourceUrl(secondUrl);

    QTRY_VERIFY(fixture.documentSession->imageDocument()->loadingTargetToken() != firstToken);
    QTRY_VERIFY(fixture.root->property("loadingTargetKey").toString() != firstTargetKey);
    QVERIFY(!loadingFeedbackVisible(*fixture.root));
}

QTEST_MAIN(TestImageStateOverlay)

#include "tst_imagestateoverlay.moc"
