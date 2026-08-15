// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiridocumentsession.h"
#include "facade/kiriimagedocument.h"
#include "facade/kiriimageviewportsurface.h"
#include "facade/kirimediainformation.h"
#include "facade/kirivideodocument.h"
#include "facade/kiriviewapplication.h"
#include "kiriviewstate.h"
#include "localization/localization.h"

#include "qml_component_test_support.h"

#include <ImageViewport/imageviewport.h>
#include <KConfigGroup>
#include <KLocalizedQmlContext>
#include <KSharedConfig>
#include <KZip>
#include <QAction>
#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QKeySequence>
#include <QObject>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickView>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqml.h>
#include <cmath>
#include <memory>

class TestImageShortcuts : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void arrowKeysPanAsFixedViewerShortcuts();
    void shiftedCommaAndPeriodPanToScanEdges();
    void leftAndRightArrowKeysUseNavigationFallback();
    void arrowKeysAreIgnoredWhileViewerShortcutsAreSuppressed();
    void shiftArrowsMoveOnePageInTwoPageModeLeftToRight();
    void shiftArrowsMoveOnePageInTwoPageModeRightToLeft();
    void shiftArrowsAreIgnoredWhileViewerShortcutsAreSuppressed();
    void zoomPresetAndFitShortcutsUseNewMapping();
    void configuredActionShortcutsTriggerActions();
    void rotationShortcutsAccumulateAndWrap();
    void invalidPersistedViewerShortcutDoesNotDispatch();
    void quitViewerLocalShortcutTriggersQuitAction();
    void windowCommandShortcutsWorkWithoutQmlShortcutInstallers();
    void videoViewerLocalShortcutTriggersFullscreenAction();
    void videoPlaybackShortcutTriggersPlaybackAction();
    void videoPlaybackShortcutShowsImageUnsupportedToastForReadyImages();
    void videoImageOnlyShortcutsShowUnsupportedToast();
    void videoModeIgnoresReportedImagePannabilityForPanShortcuts();
};

namespace {
struct ImageShortcutsFixture
{
    std::unique_ptr<QQuickView> view;
    std::unique_ptr<QTemporaryDir> temporaryDirectory;
    QObject* root = nullptr;
    KiriViewApplication* application = nullptr;
    QString errorString;

    bool isValid() const { return view != nullptr && root != nullptr && application != nullptr; }
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
    qmlRegisterType<KiriImageDocument>("org.hnjae.kiriview", 1, 0, "KiriImageDocument");
    qmlRegisterType<KiriImageViewportSurface>(
        "org.hnjae.kiriview", 1, 0, "KiriImageViewportSurface");
    qmlRegisterType<KiriDocumentSession>("org.hnjae.kiriview", 1, 0, "KiriDocumentSession");
    qmlRegisterUncreatableType<KiriMediaInformation>("org.hnjae.kiriview", 1, 0,
        "KiriMediaInformation", "KiriMediaInformation is owned by KiriDocumentSession");
    qmlRegisterType<KiriVideoDocument>("org.hnjae.kiriview", 1, 0, "KiriVideoDocument");
    qmlRegisterType<KiriViewApplication>("org.hnjae.kiriview", 1, 0, "KiriViewApplication");
    registered = true;
}

void resetConfig()
{
    KiriViewState* state = KiriViewState::self();
    state->config()->deleteGroup(QStringLiteral("Interface"));
    state->config()->sync();
    state->config()->reparseConfiguration();
    state->read();

    KSharedConfig::Ptr appConfig = KSharedConfig::openConfig();
    appConfig->deleteGroup(QStringLiteral("Shortcuts"));
    appConfig->deleteGroup(QStringLiteral("ViewerLocalShortcuts"));
    appConfig->sync();
    appConfig->reparseConfiguration();
}

bool writeTestPng(const QString& path)
{
    QImage image(QSize(800, 800), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    return image.save(path, "PNG");
}

std::unique_ptr<QTemporaryDir> createImageDirectory(QString* sourcePath, QString* errorString)
{
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        *errorString = QStringLiteral("temporary image directory was not created");
        return nullptr;
    }

    const QString imagePath = directory->filePath(QStringLiteral("image.png"));
    if (!writeTestPng(imagePath)) {
        *errorString = QStringLiteral("failed to write test image");
        return nullptr;
    }

    *sourcePath = imagePath;
    return directory;
}

std::unique_ptr<QTemporaryDir> createComicBookArchive(QString* sourcePath, QString* errorString)
{
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        *errorString = QStringLiteral("temporary archive directory was not created");
        return nullptr;
    }

    QImage image(QSize(2, 4), QImage::Format_RGBA8888);
    image.fill(Qt::blue);
    QByteArray imageData;
    QBuffer imageBuffer(&imageData);
    if (!imageBuffer.open(QIODevice::WriteOnly) || !image.save(&imageBuffer, "PNG")) {
        *errorString = QStringLiteral("failed to encode test archive image");
        return nullptr;
    }

    const QString archivePath = directory->filePath(QStringLiteral("book.cbz"));
    KZip archive(archivePath);
    if (!archive.open(QIODevice::WriteOnly)) {
        *errorString = QStringLiteral("failed to open test archive for writing");
        return nullptr;
    }

    for (int index = 1; index <= 4; ++index) {
        const QString entryName = QStringLiteral("%1.png").arg(index, 2, 10, QLatin1Char('0'));
        if (!archive.writeFile(entryName, imageData)) {
            *errorString = QStringLiteral("failed to write test archive entry %1").arg(entryName);
            archive.close();
            return nullptr;
        }
    }
    archive.close();

    *sourcePath = archivePath;
    return directory;
}

std::unique_ptr<QTemporaryDir> createVideoFile(QString* sourcePath, QString* errorString)
{
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        *errorString = QStringLiteral("temporary video directory was not created");
        return nullptr;
    }

    const QString videoPath = directory->filePath(QStringLiteral("clip.mp4"));
    QFile file(videoPath);
    if (!file.open(QIODevice::WriteOnly)) {
        *errorString = QStringLiteral("failed to create test video file");
        return nullptr;
    }
    file.close();

    *sourcePath = videoPath;
    return directory;
}

QString fixtureQml(const QString& sourceUrl = QString())
{
    return QStringLiteral(R"(
import QtQuick
import org.hnjae.kiriview

Item {
    id: root

    focus: true
    width: 320
    height: 240

    property bool helpDialogOpen: false
    property bool imageHorizontallyPannable: false
    property bool toolbarTextInputFocused: false
    property bool videoFileDeletionInProgress: false
    property bool videoMode: false
    property int panCount: 0
    property int unsupportedImageActionCount: 0
    property int unsupportedVideoActionCount: 0
    property int openApplicationMenuCount: 0
    property string lastBoundaryMessage: ""
    property alias activeNavigationKnown: documentSession.activeNavigationKnown
    readonly property KiriImageDocument sessionImageDocument: documentSession.imageDocument
    readonly property int currentPageNumber: sessionImageDocument.currentPageNumber
    readonly property int currentLastPageNumber: sessionImageDocument.currentLastPageNumber
    readonly property int documentKind: documentSession.documentKind
    readonly property int pageCount: sessionImageDocument.pageCount
    readonly property bool rightToLeftReadingAvailable: sessionImageDocument.rightToLeftReadingAvailable
    property bool rightToLeftReadingEnabled: sessionImageDocument.rightToLeftReadingEnabled
    readonly property bool secondaryPageVisible: sessionImageDocument.secondaryPageVisible
    property KiriImageDocument testImageDocument: sessionImageDocument
    readonly property bool twoPageModeAvailable: sessionImageDocument.twoPageModeAvailable
    property bool twoPageModeEnabled: sessionImageDocument.twoPageModeEnabled
    readonly property point viewportScrollPosition: Qt.point(sessionImageDocument.horizontalScrollPosition, sessionImageDocument.verticalScrollPosition)
    readonly property bool viewportPannable: sessionImageDocument.viewportPannable
    readonly property int zoomMode: sessionImageDocument.zoomMode
    readonly property real zoomPercent: sessionImageDocument.zoomPercent

    onRightToLeftReadingEnabledChanged: {
        if (sessionImageDocument.rightToLeftReadingEnabled !== rightToLeftReadingEnabled) {
            sessionImageDocument.requestToggleRightToLeftReading();
        }
    }

    onTwoPageModeEnabledChanged: {
        if (sessionImageDocument.twoPageModeEnabled !== twoPageModeEnabled) {
            sessionImageDocument.requestToggleTwoPageMode();
        }
    }

    onHelpDialogOpenChanged: publishActionUiState()
    onToolbarTextInputFocusedChanged: publishActionUiState()

    function openImageAtPage(pageNumber) {
        documentSession.openActiveNavigationAtNumber(pageNumber);
    }

    function documentReady() {
        return sessionImageDocument.status === KiriImageDocument.Ready;
    }

    function resetPanLog() {
        panCount = 0;
    }

    function publishActionUiState() {
        application.updateActionUiGateSnapshot(helpDialogOpen, toolbarTextInputFocused, false, false, false, true, true);
    }

    function refreshDerivedDocumentState() {
        publishActionUiState();
    }

    function zoomToActualSize() {
        return sessionImageDocument.requestManualZoomPercent(100);
    }

    Component.onCompleted: {
        publishActionUiState();
        forceActiveFocus();
    }

    KiriViewApplication {
        id: application

        objectName: "application"
    }

    KiriDocumentSession {
        id: documentSession

        sourceUrl: "%1"
    }

    KiriImageViewportSurface {
        anchors.fill: parent
        document: root.sessionImageDocument
    }

    Connections {
        target: application

        function onImageBoundaryReached(message) {
            root.lastBoundaryMessage = message;
        }

        function onOpenApplicationMenuRequested() {
            root.openApplicationMenuCount += 1;
        }

        function onUnsupportedVideoActionTriggered(actionId) {
            root.unsupportedVideoActionCount += 1;
        }

        function onUnsupportedImageActionTriggered(actionId) {
            root.unsupportedImageActionCount += 1;
        }
    }
}
)")
        .arg(sourceUrl);
}

ImageShortcutsFixture createFixture(const QString& sourceUrl = QString())
{
    ImageShortcutsFixture fixture;
    registerKiriViewQmlTypes();
    fixture.view = std::make_unique<QQuickView>();
    fixture.view->resize(320, 240);
    fixture.view->setResizeMode(QQuickView::SizeRootObjectToView);
    addEnvironmentImportPaths(*fixture.view->engine());
    KLocalization::setupLocalizedContext(fixture.view->engine());

    QQmlComponent component(fixture.view->engine());
    component.setData(
        fixtureQml(sourceUrl).toUtf8(), QUrl(QStringLiteral("memory:test_imageshortcuts.qml")));
    if (!waitForQmlComponentReady(component)) {
        fixture.errorString = QStringLiteral("QML component did not become ready");
        return fixture;
    }
    if (component.isError()) {
        fixture.errorString = component.errorString();
        return fixture;
    }

    QObject* root = component.create();
    if (root == nullptr) {
        fixture.errorString = component.errorString();
        return fixture;
    }

    fixture.view->setContent(
        QUrl(QStringLiteral("memory:test_imageshortcuts.qml")), &component, root);
    fixture.view->show();
    if (!QTest::qWaitForWindowExposed(fixture.view.get())) {
        fixture.errorString = QStringLiteral("test window was not exposed");
        return fixture;
    }

    fixture.root = root;
    fixture.application = root->findChild<KiriViewApplication*>(QStringLiteral("application"));
    KiriDocumentSession* const documentSession
        = root->findChild<KiriDocumentSession*>(QString(), Qt::FindChildrenRecursively);
    if (fixture.application == nullptr || documentSession == nullptr) {
        fixture.errorString = QStringLiteral("application runtime objects were not created");
        return fixture;
    }
    fixture.application->setDocumentSession(documentSession);
    fixture.application->setShortcutHost(root);
    return fixture;
}

ImageShortcutsFixture createReadyFixture()
{
    QString sourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> directory = createImageDirectory(&sourcePath, &errorString);
    if (directory == nullptr) {
        ImageShortcutsFixture fixture;
        fixture.errorString = errorString;
        return fixture;
    }

    ImageShortcutsFixture fixture = createFixture(QUrl::fromLocalFile(sourcePath).toString());
    fixture.temporaryDirectory = std::move(directory);
    return fixture;
}

ImageShortcutsFixture createComicBookFixture()
{
    QString sourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> directory = createComicBookArchive(&sourcePath, &errorString);
    if (directory == nullptr) {
        ImageShortcutsFixture fixture;
        fixture.errorString = errorString;
        return fixture;
    }

    ImageShortcutsFixture fixture = createFixture(QUrl::fromLocalFile(sourcePath).toString());
    fixture.temporaryDirectory = std::move(directory);
    return fixture;
}

ImageShortcutsFixture createVideoFixture()
{
    QString sourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> directory = createVideoFile(&sourcePath, &errorString);
    if (directory == nullptr) {
        ImageShortcutsFixture fixture;
        fixture.errorString = errorString;
        return fixture;
    }

    ImageShortcutsFixture fixture = createFixture(QUrl::fromLocalFile(sourcePath).toString());
    fixture.temporaryDirectory = std::move(directory);
    return fixture;
}

bool documentReady(QObject* root)
{
    QVariant result;
    const bool invoked = QMetaObject::invokeMethod(
        root, "documentReady", Qt::DirectConnection, Q_RETURN_ARG(QVariant, result));
    return invoked && result.toBool();
}

void pressKey(QQuickView* view, Qt::Key key)
{
    QTest::keyClick(view, key);
    QCoreApplication::processEvents();
}

void pressKey(QQuickView* view, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    QTest::keyClick(view, key, modifiers);
    QCoreApplication::processEvents();
}

bool openImageAtPage(QObject* root, int pageNumber)
{
    return QMetaObject::invokeMethod(
        root, "openImageAtPage", Qt::DirectConnection, Q_ARG(QVariant, QVariant(pageNumber)));
}

bool refreshDerivedDocumentState(QObject* root)
{
    return QMetaObject::invokeMethod(root, "refreshDerivedDocumentState", Qt::DirectConnection);
}

bool zoomToActualSize(QObject* root)
{
    QVariant result;
    return QMetaObject::invokeMethod(
               root, "zoomToActualSize", Qt::DirectConnection, Q_RETURN_ARG(QVariant, result))
        && result.toBool();
}

QPointF viewportScrollPosition(QObject* root)
{
    return root->property("viewportScrollPosition").toPointF();
}

int imageZoomMode(QObject* root) { return root->property("zoomMode").toInt(); }

double imageZoomPercent(QObject* root) { return root->property("zoomPercent").toDouble(); }

bool zoomPercentApproximately(QObject* root, double expected)
{
    return std::abs(imageZoomPercent(root) - expected) < 0.001;
}

void prepareTwoPageSpread(ImageShortcutsFixture& fixture)
{
    QVERIFY(fixture.isValid());
    QTRY_VERIFY(documentReady(fixture.root));
    QTRY_COMPARE(fixture.root->property("pageCount").toInt(), 4);
    QObject* document = fixture.root->property("testImageDocument").value<QObject*>();
    QVERIFY(document != nullptr);
    QTRY_VERIFY(document->property("twoPageModeAvailable").toBool());
    QTRY_VERIFY(document->property("rightToLeftReadingAvailable").toBool());
    QVERIFY(refreshDerivedDocumentState(fixture.root));
    QVERIFY(fixture.root->setProperty("twoPageModeEnabled", true));
    QVERIFY(refreshDerivedDocumentState(fixture.root));
    QVERIFY(openImageAtPage(fixture.root, 2));
    QTRY_COMPARE(fixture.root->property("currentPageNumber").toInt(), 2);
    QTRY_COMPARE(fixture.root->property("currentLastPageNumber").toInt(), 3);
    QTRY_VERIFY(fixture.root->property("secondaryPageVisible").toBool());
}
}

void TestImageShortcuts::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    kiriview::initializeLocalization();
    resetConfig();
}

void TestImageShortcuts::init() { resetConfig(); }

void TestImageShortcuts::cleanup() { resetConfig(); }

void TestImageShortcuts::arrowKeysPanAsFixedViewerShortcuts()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));
    QVERIFY(zoomToActualSize(fixture.root));
    QTRY_VERIFY(fixture.root->property("viewportPannable").toBool());
    const QPointF initialPosition = viewportScrollPosition(fixture.root);
    pressKey(fixture.view.get(), Qt::Key_Right);
    QTRY_VERIFY(viewportScrollPosition(fixture.root).x() > initialPosition.x());

    const QPointF rightPosition = viewportScrollPosition(fixture.root);
    pressKey(fixture.view.get(), Qt::Key_Left);
    QTRY_VERIFY(viewportScrollPosition(fixture.root).x() < rightPosition.x());

    const QPointF horizontalReturnPosition = viewportScrollPosition(fixture.root);
    pressKey(fixture.view.get(), Qt::Key_Down);
    QTRY_VERIFY(viewportScrollPosition(fixture.root).y() > horizontalReturnPosition.y());

    const QPointF downPosition = viewportScrollPosition(fixture.root);
    pressKey(fixture.view.get(), Qt::Key_Up);
    QTRY_VERIFY(viewportScrollPosition(fixture.root).y() < downPosition.y());
}

void TestImageShortcuts::shiftedCommaAndPeriodPanToScanEdges()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));
    QVERIFY(zoomToActualSize(fixture.root));
    QTRY_VERIFY(fixture.root->property("viewportPannable").toBool());

    pressKey(fixture.view.get(), Qt::Key_Period, Qt::ShiftModifier);
    const QPointF bottomRightPosition = viewportScrollPosition(fixture.root);
    QTRY_VERIFY(bottomRightPosition.x() > 0.0);
    QTRY_VERIFY(bottomRightPosition.y() > 0.0);

    pressKey(fixture.view.get(), Qt::Key_Comma, Qt::ShiftModifier);
    QTRY_COMPARE(viewportScrollPosition(fixture.root), QPointF(0.0, 0.0));
}

void TestImageShortcuts::leftAndRightArrowKeysUseNavigationFallback()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));
    QTRY_VERIFY(fixture.root->property("activeNavigationKnown").toBool());

    pressKey(fixture.view.get(), Qt::Key_Left);
    QCOMPARE(fixture.root->property("lastBoundaryMessage").toString(),
        QStringLiteral("First media item"));

    fixture.root->setProperty("lastBoundaryMessage", QString());
    pressKey(fixture.view.get(), Qt::Key_Right);
    QCOMPARE(fixture.root->property("lastBoundaryMessage").toString(),
        QStringLiteral("Last media item"));
}

void TestImageShortcuts::arrowKeysAreIgnoredWhileViewerShortcutsAreSuppressed()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));
    QVERIFY(zoomToActualSize(fixture.root));
    QTRY_VERIFY(fixture.root->property("viewportPannable").toBool());
    const QPointF initialPosition = viewportScrollPosition(fixture.root);

    fixture.root->setProperty("toolbarTextInputFocused", true);
    pressKey(fixture.view.get(), Qt::Key_Left);
    pressKey(fixture.view.get(), Qt::Key_Up);
    QCOMPARE(viewportScrollPosition(fixture.root), initialPosition);

    fixture.root->setProperty("toolbarTextInputFocused", false);
    fixture.root->setProperty("helpDialogOpen", true);
    pressKey(fixture.view.get(), Qt::Key_Right);
    pressKey(fixture.view.get(), Qt::Key_Down);
    QCOMPARE(viewportScrollPosition(fixture.root), initialPosition);
}

void TestImageShortcuts::shiftArrowsMoveOnePageInTwoPageModeLeftToRight()
{
    ImageShortcutsFixture fixture = createComicBookFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    prepareTwoPageSpread(fixture);

    pressKey(fixture.view.get(), Qt::Key_Right, Qt::ShiftModifier);
    QTRY_COMPARE(fixture.root->property("currentPageNumber").toInt(), 3);
    QTRY_COMPARE(fixture.root->property("currentLastPageNumber").toInt(), 4);
    QTRY_VERIFY(documentReady(fixture.root));

    pressKey(fixture.view.get(), Qt::Key_Left, Qt::ShiftModifier);
    QTRY_COMPARE(fixture.root->property("currentPageNumber").toInt(), 2);
    QTRY_COMPARE(fixture.root->property("currentLastPageNumber").toInt(), 3);
}

void TestImageShortcuts::shiftArrowsMoveOnePageInTwoPageModeRightToLeft()
{
    ImageShortcutsFixture fixture = createComicBookFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    prepareTwoPageSpread(fixture);
    QVERIFY(fixture.root->setProperty("rightToLeftReadingEnabled", true));
    QVERIFY(refreshDerivedDocumentState(fixture.root));

    pressKey(fixture.view.get(), Qt::Key_Left, Qt::ShiftModifier);
    QTRY_COMPARE(fixture.root->property("currentPageNumber").toInt(), 3);
    QTRY_COMPARE(fixture.root->property("currentLastPageNumber").toInt(), 4);
    QTRY_VERIFY(documentReady(fixture.root));

    pressKey(fixture.view.get(), Qt::Key_Right, Qt::ShiftModifier);
    QTRY_COMPARE(fixture.root->property("currentPageNumber").toInt(), 2);
    QTRY_COMPARE(fixture.root->property("currentLastPageNumber").toInt(), 3);
}

void TestImageShortcuts::shiftArrowsAreIgnoredWhileViewerShortcutsAreSuppressed()
{
    ImageShortcutsFixture fixture = createComicBookFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    prepareTwoPageSpread(fixture);

    fixture.root->setProperty("toolbarTextInputFocused", true);
    pressKey(fixture.view.get(), Qt::Key_Right, Qt::ShiftModifier);
    QCOMPARE(fixture.root->property("currentPageNumber").toInt(), 2);

    fixture.root->setProperty("toolbarTextInputFocused", false);
    fixture.root->setProperty("helpDialogOpen", true);
    pressKey(fixture.view.get(), Qt::Key_Right, Qt::ShiftModifier);
    QCOMPARE(fixture.root->property("currentPageNumber").toInt(), 2);
}

void TestImageShortcuts::zoomPresetAndFitShortcutsUseNewMapping()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));

    pressKey(fixture.view.get(), Qt::Key_QuoteLeft);
    QTRY_VERIFY(zoomPercentApproximately(fixture.root, 50.0));
    QCOMPARE(imageZoomMode(fixture.root), static_cast<int>(KiriImageDocument::ZoomMode::Manual));

    pressKey(fixture.view.get(), Qt::Key_1);
    QTRY_VERIFY(zoomPercentApproximately(fixture.root, 100.0));
    QCOMPARE(imageZoomMode(fixture.root), static_cast<int>(KiriImageDocument::ZoomMode::Manual));

    pressKey(fixture.view.get(), Qt::Key_2);
    QTRY_VERIFY(zoomPercentApproximately(fixture.root, 200.0));
    QCOMPARE(imageZoomMode(fixture.root), static_cast<int>(KiriImageDocument::ZoomMode::Manual));

    pressKey(fixture.view.get(), Qt::Key_8);
    QTRY_COMPARE(
        imageZoomMode(fixture.root), static_cast<int>(KiriImageDocument::ZoomMode::FitHeight));

    pressKey(fixture.view.get(), Qt::Key_9);
    QTRY_COMPARE(
        imageZoomMode(fixture.root), static_cast<int>(KiriImageDocument::ZoomMode::FitWidth));

    pressKey(fixture.view.get(), Qt::Key_0);
    QTRY_COMPARE(imageZoomMode(fixture.root), static_cast<int>(KiriImageDocument::ZoomMode::Fit));
}

void TestImageShortcuts::configuredActionShortcutsTriggerActions()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));

    QAction* rotateAction
        = fixture.application->actionForId(KiriViewApplication::ViewRotateClockwiseAction);
    QVERIFY(rotateAction != nullptr);
    QSignalSpy triggeredSpy(rotateAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_R, Qt::ControlModifier);
    QCOMPARE(triggeredSpy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_R);
    QTRY_COMPARE(triggeredSpy.count(), 1);

    fixture.root->setProperty("toolbarTextInputFocused", true);
    pressKey(fixture.view.get(), Qt::Key_R);
    QCOMPARE(triggeredSpy.count(), 1);

    pressKey(fixture.view.get(), Qt::Key_R, Qt::ControlModifier);
    QCOMPARE(triggeredSpy.count(), 1);
}

void TestImageShortcuts::rotationShortcutsAccumulateAndWrap()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));

    KiriImageViewportSurface* surface
        = fixture.root->findChild<KiriImageViewportSurface*>(QString());
    QVERIFY(surface != nullptr);
    ImageViewport* viewport = surface->viewport();
    QVERIFY(viewport != nullptr);
    QCOMPARE(viewport->state().presentation().rotationDegrees(), 0);

    for (const int expected : { 90, 180, 270, 0 }) {
        pressKey(fixture.view.get(), Qt::Key_R);
        QTRY_COMPARE(viewport->state().presentation().rotationDegrees(), expected);
    }

    for (const int expected : { 270, 180, 90, 0 }) {
        pressKey(fixture.view.get(), Qt::Key_R, Qt::ShiftModifier);
        QTRY_COMPARE(viewport->state().presentation().rotationDegrees(), expected);
    }
}

void TestImageShortcuts::invalidPersistedViewerShortcutDoesNotDispatch()
{
    KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("ViewerLocalShortcuts"));
    group.writeEntry(QStringLiteral("view_rotate_clockwise"),
        QStringList { QStringLiteral("R"), QStringLiteral("Ctrl+") });
    group.sync();
    KSharedConfig::openConfig()->reparseConfiguration();

    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));

    QAction* rotateAction
        = fixture.application->actionForId(KiriViewApplication::ViewRotateClockwiseAction);
    QVERIFY(rotateAction != nullptr);
    QSignalSpy triggeredSpy(rotateAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_R);
    QCOMPARE(triggeredSpy.count(), 0);
    QCOMPARE(fixture.application->viewerLocalShortcutsForId(
                 KiriViewApplication::ViewRotateClockwiseAction),
        QList<QKeySequence>());
}

void TestImageShortcuts::quitViewerLocalShortcutTriggersQuitAction()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));

    QAction* quitAction = fixture.application->actionForId(KiriViewApplication::FileQuitAction);
    QVERIFY(quitAction != nullptr);
    QSignalSpy triggeredSpy(quitAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_Q);
    QTRY_COMPARE(triggeredSpy.count(), 1);

    fixture.root->setProperty("toolbarTextInputFocused", true);
    pressKey(fixture.view.get(), Qt::Key_Q);
    QCOMPARE(triggeredSpy.count(), 1);

    QVERIFY(fixture.application->setViewerLocalShortcutsForId(KiriViewApplication::FileQuitAction,
        { QKeySequence::fromString(QStringLiteral("Alt+Q"), QKeySequence::PortableText) }));
    pressKey(fixture.view.get(), Qt::Key_Q, Qt::AltModifier);
    QTRY_COMPARE(triggeredSpy.count(), 2);

    pressKey(fixture.view.get(), Qt::Key_Q, Qt::ControlModifier);
    QTRY_COMPARE(triggeredSpy.count(), 3);

    fixture.root->setProperty("helpDialogOpen", true);
    pressKey(fixture.view.get(), Qt::Key_Q, Qt::AltModifier);
    QCOMPARE(triggeredSpy.count(), 3);
}

void TestImageShortcuts::windowCommandShortcutsWorkWithoutQmlShortcutInstallers()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));

    QAction* rotateAction
        = fixture.application->actionForId(KiriViewApplication::ViewRotateClockwiseAction);
    QAction* showMenubarAction
        = fixture.application->actionForId(KiriViewApplication::OptionsShowMenubarAction);
    QVERIFY(rotateAction != nullptr);
    QVERIFY(showMenubarAction != nullptr);
    QSignalSpy rotateSpy(rotateAction, &QAction::triggered);
    QSignalSpy showMenubarSpy(showMenubarAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_R, Qt::ControlModifier);
    QCOMPARE(rotateSpy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_R);
    QTRY_COMPARE(rotateSpy.count(), 1);

    fixture.root->setProperty("toolbarTextInputFocused", true);
    pressKey(fixture.view.get(), Qt::Key_R);
    QCOMPARE(rotateSpy.count(), 1);

    fixture.root->setProperty("toolbarTextInputFocused", false);
    pressKey(fixture.view.get(), Qt::Key_F10);
    QTRY_COMPARE(fixture.root->property("openApplicationMenuCount").toInt(), 1);

    pressKey(fixture.view.get(), Qt::Key_M, Qt::ControlModifier);
    QTRY_COMPARE(showMenubarSpy.count(), 1);
}

void TestImageShortcuts::videoViewerLocalShortcutTriggersFullscreenAction()
{
    ImageShortcutsFixture fixture = createVideoFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_COMPARE(fixture.root->property("documentKind").toInt(),
        static_cast<int>(KiriDocumentSession::DocumentKind::Video));

    QAction* fullscreenAction
        = fixture.application->actionForId(KiriViewApplication::WindowFullscreenAction);
    QVERIFY(fullscreenAction != nullptr);
    QSignalSpy triggeredSpy(fullscreenAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_F);

    QTRY_COMPARE(triggeredSpy.count(), 1);
    QCOMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 0);
}

void TestImageShortcuts::videoPlaybackShortcutTriggersPlaybackAction()
{
    ImageShortcutsFixture fixture = createVideoFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_COMPARE(fixture.root->property("documentKind").toInt(),
        static_cast<int>(KiriDocumentSession::DocumentKind::Video));

    QAction* playbackAction
        = fixture.application->actionForId(KiriViewApplication::ViewToggleVideoPlaybackAction);
    QVERIFY(playbackAction != nullptr);
    QSignalSpy triggeredSpy(playbackAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_P);

    QTRY_COMPARE(triggeredSpy.count(), 1);
    QCOMPARE(fixture.root->property("unsupportedImageActionCount").toInt(), 0);
    QCOMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 0);
}

void TestImageShortcuts::videoPlaybackShortcutShowsImageUnsupportedToastForReadyImages()
{
    ImageShortcutsFixture fixture = createReadyFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_VERIFY(documentReady(fixture.root));

    QAction* playbackAction
        = fixture.application->actionForId(KiriViewApplication::ViewToggleVideoPlaybackAction);
    QVERIFY(playbackAction != nullptr);
    QSignalSpy triggeredSpy(playbackAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_P);

    QTRY_COMPARE(fixture.root->property("unsupportedImageActionCount").toInt(), 1);
    QCOMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 0);
    QCOMPARE(triggeredSpy.count(), 0);

    fixture.root->setProperty("toolbarTextInputFocused", true);
    pressKey(fixture.view.get(), Qt::Key_P);
    QCOMPARE(fixture.root->property("unsupportedImageActionCount").toInt(), 1);
}

void TestImageShortcuts::videoImageOnlyShortcutsShowUnsupportedToast()
{
    ImageShortcutsFixture fixture = createVideoFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_COMPARE(fixture.root->property("documentKind").toInt(),
        static_cast<int>(KiriDocumentSession::DocumentKind::Video));

    QAction* rotateAction
        = fixture.application->actionForId(KiriViewApplication::ViewRotateClockwiseAction);
    QAction* zoomInAction = fixture.application->actionForId(KiriViewApplication::ViewZoomInAction);
    QAction* zoom50Action
        = fixture.application->actionForId(KiriViewApplication::ViewZoom50PercentAction);
    QAction* zoom100Action
        = fixture.application->actionForId(KiriViewApplication::ViewZoom100PercentAction);
    QAction* zoom200Action
        = fixture.application->actionForId(KiriViewApplication::ViewZoom200PercentAction);
    QAction* fitHeightAction
        = fixture.application->actionForId(KiriViewApplication::ViewFitHeightAction);
    QAction* fitWidthAction
        = fixture.application->actionForId(KiriViewApplication::ViewFitWidthAction);
    QAction* fitWindowAction = fixture.application->actionForId(KiriViewApplication::ViewFitAction);
    QVERIFY(rotateAction != nullptr);
    QVERIFY(zoomInAction != nullptr);
    QVERIFY(zoom50Action != nullptr);
    QVERIFY(zoom100Action != nullptr);
    QVERIFY(zoom200Action != nullptr);
    QVERIFY(fitHeightAction != nullptr);
    QVERIFY(fitWidthAction != nullptr);
    QVERIFY(fitWindowAction != nullptr);

    QSignalSpy rotateSpy(rotateAction, &QAction::triggered);
    QSignalSpy zoomInSpy(zoomInAction, &QAction::triggered);
    QSignalSpy zoom50Spy(zoom50Action, &QAction::triggered);
    QSignalSpy zoom100Spy(zoom100Action, &QAction::triggered);
    QSignalSpy zoom200Spy(zoom200Action, &QAction::triggered);
    QSignalSpy fitHeightSpy(fitHeightAction, &QAction::triggered);
    QSignalSpy fitWidthSpy(fitWidthAction, &QAction::triggered);
    QSignalSpy fitWindowSpy(fitWindowAction, &QAction::triggered);

    pressKey(fixture.view.get(), Qt::Key_R, Qt::ControlModifier);
    QCOMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 0);
    QCOMPARE(rotateSpy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_R);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 1);
    QCOMPARE(rotateSpy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_Equal);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 2);
    QCOMPARE(zoomInSpy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_QuoteLeft);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 3);
    QCOMPARE(zoom50Spy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_1);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 4);
    QCOMPARE(zoom100Spy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_2);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 5);
    QCOMPARE(zoom200Spy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_8);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 6);
    QCOMPARE(fitHeightSpy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_9);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 7);
    QCOMPARE(fitWidthSpy.count(), 0);

    pressKey(fixture.view.get(), Qt::Key_0);
    QTRY_COMPARE(fixture.root->property("unsupportedVideoActionCount").toInt(), 8);
    QCOMPARE(fitWindowSpy.count(), 0);
}

void TestImageShortcuts::videoModeIgnoresReportedImagePannabilityForPanShortcuts()
{
    ImageShortcutsFixture fixture = createVideoFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_COMPARE(fixture.root->property("documentKind").toInt(),
        static_cast<int>(KiriDocumentSession::DocumentKind::Video));
    fixture.root->setProperty("panCount", 0);

    pressKey(fixture.view.get(), Qt::Key_Up);
    pressKey(fixture.view.get(), Qt::Key_Down);

    QCOMPARE(fixture.root->property("panCount").toInt(), 0);
}

QTEST_MAIN(TestImageShortcuts)

#include "tst_imageshortcuts.moc"
