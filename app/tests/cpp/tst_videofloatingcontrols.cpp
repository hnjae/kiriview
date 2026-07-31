// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiridocumentsession.h"
#include "facade/kirivideoplaybackcontrols.h"
#include "localization/localization.h"

#include "qml_component_test_support.h"

#include <KLocalizedQmlContext>
#include <QDir>
#include <QMetaObject>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>
#include <QtQml/qqml.h>
#include <algorithm>
#include <cmath>
#include <memory>

class TestVideoFloatingControls : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void adaptiveWidthRelationshipsRemainStable();
};

namespace {
constexpr qreal fuzzyPixel = 0.75;
constexpr int fixtureHeight = 400;

struct VideoFloatingControlsFixture
{
    std::unique_ptr<QQmlComponent> component;
    std::unique_ptr<QQuickView> view;
    KiriDocumentSession* documentSession = nullptr;
    QQuickItem* root = nullptr;
    QQuickItem* viewport = nullptr;
    QQuickItem* controls = nullptr;
    QString errorString;

    [[nodiscard]] bool isValid() const
    {
        return component != nullptr && view != nullptr && documentSession != nullptr
            && root != nullptr && viewport != nullptr && controls != nullptr;
    }
};

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
    qmlRegisterType<KiriDocumentSession>("org.hnjae.kiriview", 1, 0, "KiriDocumentSession");
    registered = true;
}

QString qmlSourceImport()
{
    const QString qmlPath = QDir(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR))
                                .absoluteFilePath(QStringLiteral("../../src/qml"));
    return QUrl::fromLocalFile(qmlPath).toString();
}

QString fixtureQml()
{
    return QStringLiteral(R"(
import QtQuick
import org.hnjae.kiriview
import org.kde.kirigami as Kirigami
import "%1" as KiriViewQml

Item {
    id: root

    objectName: "videoFloatingControlsFixture"
    width: 800
    height: 400
    readonly property real expectedLargeSpacing: Kirigami.Units.largeSpacing

    KiriDocumentSession {
        id: documentSession

        objectName: "documentSession"
    }

    KiriViewQml.VideoViewport {
        id: viewport

        anchors.fill: parent
        documentSession: documentSession
        objectName: "videoViewport"
    }
}
)")
        .arg(qmlSourceImport());
}

VideoFloatingControlsFixture createFixture()
{
    VideoFloatingControlsFixture fixture;
    registerKiriViewQmlTypes();

    fixture.view = std::make_unique<QQuickView>();
    fixture.view->resize(800, fixtureHeight);
    fixture.view->setResizeMode(QQuickView::SizeRootObjectToView);
    addEnvironmentImportPaths(*fixture.view->engine());
    fixture.view->engine()->addImportPath(QDir(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR))
            .absoluteFilePath(QStringLiteral("../../src/qml")));
    KLocalization::setupLocalizedContext(fixture.view->engine());

    fixture.component = std::make_unique<QQmlComponent>(fixture.view->engine());
    fixture.component->setData(
        fixtureQml().toUtf8(), QUrl(QStringLiteral("memory:video_floating_controls.qml")));
    if (!waitForQmlComponentReady(*fixture.component)) {
        fixture.errorString = QStringLiteral("QML component did not become ready");
        return fixture;
    }
    if (fixture.component->isError()) {
        fixture.errorString = fixture.component->errorString();
        return fixture;
    }

    QObject* createdRoot = fixture.component->create();
    if (createdRoot == nullptr) {
        fixture.errorString = fixture.component->errorString();
        return fixture;
    }

    fixture.root = qobject_cast<QQuickItem*>(createdRoot);
    if (fixture.root == nullptr) {
        fixture.errorString = QStringLiteral("fixture root is not a QQuickItem");
        delete createdRoot;
        return fixture;
    }

    fixture.view->setContent(QUrl(QStringLiteral("memory:video_floating_controls.qml")),
        fixture.component.get(), createdRoot);
    fixture.view->show();
    if (!QTest::qWaitForWindowExposed(fixture.view.get())) {
        fixture.errorString = QStringLiteral("test window was not exposed");
        return fixture;
    }

    fixture.documentSession
        = fixture.root->findChild<KiriDocumentSession*>(QStringLiteral("documentSession"));
    fixture.viewport = fixture.root->findChild<QQuickItem*>(QStringLiteral("videoViewport"));
    fixture.controls
        = fixture.root->findChild<QQuickItem*>(QStringLiteral("videoPlaybackControls"));
    if (!fixture.isValid()) {
        fixture.errorString = QStringLiteral("fixture did not create required objects");
    }
    return fixture;
}

qreal requiredRealProperty(const QObject& object, const char* propertyName)
{
    const int propertyIndex = object.metaObject()->indexOfProperty(propertyName);
    Q_ASSERT(propertyIndex >= 0);
    return object.property(propertyName).toReal();
}

void reportEnvironment(VideoFloatingControlsFixture& fixture, bool transientTouchInput = false)
{
    KiriVideoPlaybackControls* playbackControls
        = fixture.documentSession->videoDocument()->playbackControls();
    playbackControls->reportEnvironment(fixture.root->width(), fixture.root->height(), 0.25,
        requiredRealProperty(*fixture.controls, "floatingNaturalWidth"),
        requiredRealProperty(*fixture.controls, "floatingSideMargin"), false, transientTouchInput,
        200, 1500);
    drainQmlPostedEvents();
}

void verifyClose(qreal actual, qreal expected, const char* relationship)
{
    QVERIFY2(std::abs(actual - expected) <= fuzzyPixel,
        qPrintable(QStringLiteral("%1: actual %2, expected %3")
                .arg(QString::fromLatin1(relationship))
                .arg(actual)
                .arg(expected)));
}

void verifySideMargins(const VideoFloatingControlsFixture& fixture, qreal requiredSideMargin)
{
    const qreal leftMargin = fixture.controls->x();
    const qreal rightMargin
        = fixture.root->width() - fixture.controls->x() - fixture.controls->width();
    QVERIFY2(leftMargin + fuzzyPixel >= requiredSideMargin,
        qPrintable(QStringLiteral("left margin %1 is smaller than required margin %2")
                .arg(leftMargin)
                .arg(requiredSideMargin)));
    QVERIFY2(rightMargin + fuzzyPixel >= requiredSideMargin,
        qPrintable(QStringLiteral("right margin %1 is smaller than required margin %2")
                .arg(rightMargin)
                .arg(requiredSideMargin)));
}
}

void TestVideoFloatingControls::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    registerKiriViewQmlTypes();
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
}

void TestVideoFloatingControls::init()
{
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral(".*Created graphical object was not placed in the graphics scene.*")));
}

void TestVideoFloatingControls::adaptiveWidthRelationshipsRemainStable()
{
    VideoFloatingControlsFixture fixture = createFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    const QMetaObject& controlsMetaObject = *fixture.controls->metaObject();
    QVERIFY2(controlsMetaObject.indexOfProperty("floatingNaturalWidth") >= 0,
        "VideoFloatingControls must expose a mode-independent floatingNaturalWidth");
    QVERIFY2(controlsMetaObject.indexOfProperty("floatingSideMargin") >= 0,
        "VideoFloatingControls must expose its one-side floatingSideMargin");

    const qreal initialNaturalWidth
        = requiredRealProperty(*fixture.controls, "floatingNaturalWidth");
    const qreal sideMargin = requiredRealProperty(*fixture.controls, "floatingSideMargin");
    QVERIFY(initialNaturalWidth > 0.0);
    QVERIFY(sideMargin >= 0.0);
    verifyClose(sideMargin, requiredRealProperty(*fixture.root, "expectedLargeSpacing"),
        "floating side margin is not Kirigami large spacing");

    QQuickItem* controlsRow = qvariant_cast<QQuickItem*>(fixture.controls->property("contentItem"));
    QVERIFY(controlsRow != nullptr);
    const qreal completeNaturalWidth = requiredRealProperty(*fixture.controls, "leftPadding")
        + requiredRealProperty(*fixture.controls, "rightPadding") + controlsRow->implicitWidth();
    verifyClose(initialNaturalWidth, completeNaturalWidth,
        "floating natural width does not contain the complete control row");

    const qreal gridUnit = requiredRealProperty(*fixture.viewport, "controlGridUnit");
    QVERIFY(gridUnit > 0.0);
    const int productionViewportHeight
        = static_cast<int>(std::ceil(std::max<qreal>(fixtureHeight, gridUnit * 17.0 + 1.0)));
    const int compactViewportWidth = static_cast<int>(std::floor(gridUnit * 31.0));
    fixture.view->resize(compactViewportWidth, productionViewportHeight);
    drainQmlPostedEvents();
    QTRY_VERIFY(fixture.controls->property("fixedMode").toBool());

    const int productionEntryWidth = static_cast<int>(std::ceil(
        std::max(gridUnit * 33.0, initialNaturalWidth + sideMargin * 2.0 + gridUnit) + 1.0));
    fixture.view->resize(productionEntryWidth, productionViewportHeight);
    drainQmlPostedEvents();
    QTRY_VERIFY(!fixture.controls->property("fixedMode").toBool());
    verifyClose(requiredRealProperty(*fixture.controls, "floatingNaturalWidth"),
        initialNaturalWidth, "production environment reporting changed the natural width");

    const int nominalViewportWidth = static_cast<int>(std::ceil(
        std::max({ 400.0, initialNaturalWidth * 4.0 / 3.0 + 4.0, sideMargin * 8.0 + 4.0 })));
    fixture.view->resize(nominalViewportWidth, productionViewportHeight);
    drainQmlPostedEvents();
    reportEnvironment(fixture);
    QTRY_VERIFY(!fixture.controls->property("fixedMode").toBool());

    verifyClose(requiredRealProperty(*fixture.controls, "floatingNaturalWidth"),
        initialNaturalWidth, "natural width changed when entering floating mode");
    const qreal nominalWidth = fixture.root->width() * 0.75;
    QVERIFY(initialNaturalWidth <= nominalWidth);
    QVERIFY(nominalWidth <= fixture.root->width() - sideMargin * 2.0);
    verifyClose(fixture.controls->width(), nominalWidth, "nominal 75% width");
    verifySideMargins(fixture, sideMargin);

    const int naturalFloorViewportWidth
        = static_cast<int>(std::ceil(initialNaturalWidth + sideMargin * 2.0 + 1.0));
    QVERIFY2(naturalFloorViewportWidth * 0.75 < initialNaturalWidth,
        "the active style does not expose a natural-width-floor scenario");
    fixture.view->resize(naturalFloorViewportWidth, productionViewportHeight);
    drainQmlPostedEvents();
    reportEnvironment(fixture);
    QTRY_VERIFY(!fixture.controls->property("fixedMode").toBool());
    QVERIFY(fixture.controls->width() + fuzzyPixel >= initialNaturalWidth);
    verifyClose(fixture.controls->width(), initialNaturalWidth, "natural-width floor");
    verifySideMargins(fixture, sideMargin);

    reportEnvironment(fixture, true);
    QTRY_VERIFY(fixture.controls->property("fixedMode").toBool());
    verifyClose(requiredRealProperty(*fixture.controls, "floatingNaturalWidth"),
        initialNaturalWidth, "natural width changed in forced fixed mode");

    reportEnvironment(fixture);
    QTRY_VERIFY(!fixture.controls->property("fixedMode").toBool());
    const qreal settledNaturalWidth
        = requiredRealProperty(*fixture.controls, "floatingNaturalWidth");
    const qreal settledWidth = fixture.controls->width();
    const qreal settledX = fixture.controls->x();
    for (int iteration = 0; iteration < 8; ++iteration) {
        drainQmlPostedEvents();
    }
    verifyClose(requiredRealProperty(*fixture.controls, "floatingNaturalWidth"),
        settledNaturalWidth, "natural width oscillated");
    verifyClose(fixture.controls->width(), settledWidth, "assigned width oscillated");
    verifyClose(fixture.controls->x(), settledX, "horizontal position oscillated");
}

QTEST_MAIN(TestVideoFloatingControls)

#include "tst_videofloatingcontrols.moc"
