// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/applicationruntime.h"
#include "facade/kiridocumentsession.h"
#include "facade/kiridocumentsessioncomposition.h"
#include "facade/kiriimagedocument.h"
#include "facade/kiriimageviewportsurface.h"
#include "facade/kirimediainformation.h"
#include "facade/kirivideodocument.h"
#include "facade/kiriviewapplication.h"
#include "facade/kiriwindowshell.h"
#include "facade/menuaccesskeyrouter.h"
#include "image_async_test_support.h"
#include "kiriviewstate.h"
#include "localization/localization.h"

#include <ImageViewport/imageviewport.h>
#include <KLocalizedQmlContext>
#include <KZip>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QNativeGestureEvent>
#include <QObject>
#include <QPointingDevice>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStringList>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QWheelEvent>
#include <QtQml/qqml.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

class TestMainWindowToolBar : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void startupCreatesOneVisibleToolbarWithDisabledMediaControls();
    void shellOwnsFinalWindowTitleProjection();
    void toolbarPageReadoutUsesSystemFixedWidthFont();
    void startupInitialDirectImageRendersMainViewport();
    void startupInitialComicArchiveRendersAndNavigatesMainViewport();
    void outsideScopePresentationCommitResetsFitSelection();
    void comicPageReplacementKeepsRightToolbarPresentationStable();
    void spreadPageReplacementKeepsRightToolbarPresentationStable();
    void dropOpensFirstUrlOnly();
    void fileDialogUsesSingleSelectionMode();
    void directImageShowsMediaPositionAfterSiblingListing();
    void goToPageShortcutFocusesPageNumberInput();
    void zoomShortcutFocusesZoomInput();
    void fullscreenZoomShortcutRevealsToolbarAndFocusesInput();
    void directoryImageDocumentShowsPagePosition();
    void pageNumberWheelNavigatesSemantically();
    void mediaViewportHostLoadsOnlyActiveDelegate();
    void panelActionsToggleResizablePanels();
    void infoPanelAdvancedMetadataSectionFoldsRows();
    void infoPanelUsesFixedWidthFontForFileAndValues();
    void infoPanelUsesOverlayDrawerOnNarrowWindows();
    void escapeClosesInfoPanelBeforeLeavingFullscreen();
    void panelShortcutsToggleResizablePanels();
    void commandFixedShortcutsUseApplicationActions();
    void configureShortcutActionOpensApplicationOwnedDialog();
    void viewerRightClickOpensContextMenuOnlyFromMediaViewport();
    void toolbarZoomWheelAppliesFineManualStep_data();
    void toolbarZoomWheelAppliesFineManualStep();
    void rightButtonWheelSuppressesContextMenuTap();
    void nativeTouchpadPinchFromFitModes_data();
    void nativeTouchpadPinchFromFitModes();
    void touchscreenPinchZoomsAndTranslates();
    void viewportPinchRuntimeBoundaryBehavior();
    void staleTouchscreenPinchCannotMutateReplacementImage();
    void fullscreenChromeProjectionRendersImmediately();
    void fullscreenReusesSingleToolbarAndHidesApplicationMenuButton();
};

namespace {
std::optional<kiriview::ImageWorkerScheduler> toolbarImageWorkerSchedulerOverride;

kiriview::ThumbnailGenerationProvider disabledThumbnailGenerationProvider()
{
    return [](QObject*, kiriview::ThumbnailGenerationRequest request,
               kiriview::ThumbnailGenerationCallback callback) {
        if (callback) {
            callback(kiriview::ThumbnailGenerationResult {
                kiriview::ThumbnailGenerationStatus::Failed,
                {},
                {},
                request.requestedBucket,
                {},
                QStringLiteral("toolbar test thumbnail generation disabled"),
            });
        }
        return kiriview::ImageIoJob();
    };
}

kiriview::KiriDocumentSessionDependencies toolbarTestDocumentSessionDependencies()
{
    kiriview::KiriDocumentSessionDependencies dependencies;
    dependencies.sessionRuntime.activeNavigationThumbnails.generationProvider
        = disabledThumbnailGenerationProvider();
    if (toolbarImageWorkerSchedulerOverride.has_value()) {
        dependencies.imageDocument.imageDecode.workerScheduler
            = *toolbarImageWorkerSchedulerOverride;
    }
    return dependencies;
}

class ScopedToolbarImageWorkerSchedulerOverride final
{
public:
    explicit ScopedToolbarImageWorkerSchedulerOverride(
        kiriview::ImageWorkerScheduler workerScheduler)
    {
        Q_ASSERT(!toolbarImageWorkerSchedulerOverride.has_value());
        toolbarImageWorkerSchedulerOverride = std::move(workerScheduler);
    }

    ~ScopedToolbarImageWorkerSchedulerOverride() { toolbarImageWorkerSchedulerOverride.reset(); }

    Q_DISABLE_COPY_MOVE(ScopedToolbarImageWorkerSchedulerOverride)
};

class ToolbarTestDocumentSession : public KiriDocumentSession
{
    Q_OBJECT

public:
    explicit ToolbarTestDocumentSession(QObject* parent = nullptr)
        : KiriDocumentSession(toolbarTestDocumentSessionDependencies(), parent)
    {
    }
};

struct MainWindowFixture
{
    std::unique_ptr<QQmlApplicationEngine> engine;
    KiriViewApplication* application = nullptr;
    KiriWindowShell* windowShell = nullptr;
    QQuickWindow* window = nullptr;
    QString errorString;

    bool isValid() const
    {
        return engine != nullptr && application != nullptr && windowShell != nullptr
            && window != nullptr;
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
    qmlRegisterType<KiriViewApplication>("org.hnjae.kiriview", 1, 0, "KiriViewApplication");
    qmlRegisterUncreatableType<KiriWindowShell>("org.hnjae.kiriview", 1, 0, "KiriWindowShell",
        "KiriWindowShell is created by the application runtime");
    qmlRegisterType<ToolbarTestDocumentSession>("org.hnjae.kiriview", 1, 0, "KiriDocumentSession");
    qmlRegisterType<KiriImageDocument>("org.hnjae.kiriview", 1, 0, "KiriImageDocument");
    qmlRegisterType<KiriImageViewportSurface>(
        "org.hnjae.kiriview", 1, 0, "KiriImageViewportSurface");
    qmlRegisterUncreatableType<KiriMediaInformation>("org.hnjae.kiriview", 1, 0,
        "KiriMediaInformation", "KiriMediaInformation is owned by KiriDocumentSession");
    qmlRegisterType<KiriVideoDocument>("org.hnjae.kiriview", 1, 0, "KiriVideoDocument");
    qmlRegisterType<MenuAccessKeyRouter>("org.hnjae.kiriview", 1, 0, "MenuAccessKeyRouter");
    registered = true;
}

void resetConfig()
{
    KiriViewState* state = KiriViewState::self();
    state->config()->deleteGroup(QStringLiteral("Interface"));
    state->config()->sync();
    state->config()->reparseConfiguration();
    state->read();
}

QUrl mainQmlUrl()
{
    return QUrl::fromLocalFile(QDir(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR))
            .absoluteFilePath(QStringLiteral("../../src/qml/Main.qml")));
}

QList<QQuickItem*> controlToolBars(QObject* root)
{
    QList<QQuickItem*> toolbars;
    const QList<QQuickItem*> items
        = root->findChildren<QQuickItem*>(QString(), Qt::FindChildrenRecursively);
    for (QQuickItem* item : items) {
        if (item->inherits("QQuickToolBar")) {
            toolbars.append(item);
        }
    }
    return toolbars;
}

bool effectivelyVisible(QQuickItem* item)
{
    for (QQuickItem* current = item; current != nullptr; current = current->parentItem()) {
        if (!current->isVisible()) {
            return false;
        }
    }

    return item != nullptr;
}

QList<QQuickItem*> visibleItemsByObjectName(QObject* root, const QString& objectName)
{
    QList<QQuickItem*> visibleItems;
    const QList<QQuickItem*> items
        = root->findChildren<QQuickItem*>(objectName, Qt::FindChildrenRecursively);
    for (QQuickItem* item : items) {
        if (effectivelyVisible(item)) {
            visibleItems.append(item);
        }
    }
    return visibleItems;
}

void appendVisualItemsByObjectName(
    QQuickItem* root, const QString& objectName, QList<QQuickItem*>* items)
{
    if (root == nullptr) {
        return;
    }
    if (root->objectName() == objectName) {
        items->append(root);
    }
    const QList<QQuickItem*> children = root->childItems();
    for (QQuickItem* child : children) {
        appendVisualItemsByObjectName(child, objectName, items);
    }
}

QList<QQuickItem*> visualItemsByObjectName(QObject* root, const QString& objectName)
{
    QList<QQuickItem*> items;
    if (QQuickWindow* window = qobject_cast<QQuickWindow*>(root)) {
        appendVisualItemsByObjectName(window->contentItem(), objectName, &items);
    } else {
        if (QQuickItem* item = qobject_cast<QQuickItem*>(root)) {
            appendVisualItemsByObjectName(item, objectName, &items);
        }
    }
    return items;
}

void appendVisibleItemsByText(QQuickItem* root, const QString& text, QList<QQuickItem*>* items)
{
    if (root == nullptr) {
        return;
    }
    if (root->property("text").toString() == text && effectivelyVisible(root)) {
        items->append(root);
    }
    const QList<QQuickItem*> children = root->childItems();
    for (QQuickItem* child : children) {
        appendVisibleItemsByText(child, text, items);
    }
}

QList<QQuickItem*> visibleItemsByText(QObject* root, const QString& text)
{
    QList<QQuickItem*> items;
    if (QQuickWindow* window = qobject_cast<QQuickWindow*>(root)) {
        appendVisibleItemsByText(window->contentItem(), text, &items);
    } else {
        if (QQuickItem* item = qobject_cast<QQuickItem*>(root)) {
            appendVisibleItemsByText(item, text, &items);
        }
    }
    return items;
}

QQuickItem* findQuickItem(QObject* root, const QString& objectName)
{
    return root->findChild<QQuickItem*>(objectName, Qt::FindChildrenRecursively);
}

QObject* findObject(QObject* root, const QString& objectName)
{
    return root->findChild<QObject*>(objectName, Qt::FindChildrenRecursively);
}

QString fontFamily(QQuickItem* item)
{
    return qvariant_cast<QFont>(item->property("font")).family();
}

KiriDocumentSession* findDocumentSession(QObject* root)
{
    KiriDocumentSession* documentSession
        = qvariant_cast<KiriDocumentSession*>(root->property("documentSession"));
    if (documentSession != nullptr) {
        return documentSession;
    }
    return root->findChild<KiriDocumentSession*>(
        QStringLiteral("documentSession"), Qt::FindChildrenRecursively);
}

KiriViewApplication* findApplication(QObject* root)
{
    return root->property("kiriApplication").value<KiriViewApplication*>();
}

bool writeTestPng(const QString& path)
{
    QImage image(QSize(2, 2), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    return image.save(path, "PNG");
}

void appendLe16(QByteArray* data, quint16 value)
{
    data->append(static_cast<char>(value & 0xff));
    data->append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(QByteArray* data, quint32 value)
{
    appendLe16(data, static_cast<quint16>(value & 0xffff));
    appendLe16(data, static_cast<quint16>((value >> 16) & 0xffff));
}

void appendBe16(QByteArray* data, quint16 value)
{
    data->append(static_cast<char>((value >> 8) & 0xff));
    data->append(static_cast<char>(value & 0xff));
}

QByteArray testExifSegmentWithArtist()
{
    constexpr quint16 artistTag = 0x013b;
    const QByteArray artist = QByteArrayLiteral("Kiri Tester\0");
    QByteArray tiff;
    tiff.append("II", 2);
    appendLe16(&tiff, 42);
    appendLe32(&tiff, 8);
    appendLe16(&tiff, 1);
    appendLe16(&tiff, artistTag);
    appendLe16(&tiff, 2);
    appendLe32(&tiff, artist.size());
    appendLe32(&tiff, 26);
    appendLe32(&tiff, 0);
    tiff.append(artist);

    const QByteArray payload = QByteArrayLiteral("Exif\0\0") + tiff;
    QByteArray segment;
    segment.append(static_cast<char>(0xff));
    segment.append(static_cast<char>(0xe1));
    appendBe16(&segment, static_cast<quint16>(payload.size() + 2));
    segment.append(payload);
    return segment;
}

bool writeAdvancedMetadataJpeg(const QString& path)
{
    QImage image(QSize(2, 2), QImage::Format_RGB888);
    image.fill(Qt::red);

    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "JPEG")) {
        return false;
    }
    if (jpegData.size() < 2 || static_cast<uchar>(jpegData.at(0)) != 0xff
        || static_cast<uchar>(jpegData.at(1)) != 0xd8) {
        return false;
    }

    jpegData.insert(2, testExifSegmentWithArtist());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(jpegData) == jpegData.size();
}

QByteArray encodedTestPng(Qt::GlobalColor color, QSize size = QSize(2, 2))
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(color);

    QByteArray data;
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        return {};
    }

    return data;
}

bool writeEmptyFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

std::unique_ptr<QTemporaryDir> createMediaDirectory(
    QString* imageSourcePath, QString* videoSourcePath, QString* errorString)
{
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        *errorString = QStringLiteral("temporary media directory was not created");
        return nullptr;
    }

    const QString firstImage = directory->filePath(QStringLiteral("01.png"));
    const QString secondVideo = directory->filePath(QStringLiteral("02.mp4"));
    const QString thirdImage = directory->filePath(QStringLiteral("03.png"));
    if (!writeTestPng(firstImage) || !writeEmptyFile(secondVideo) || !writeTestPng(thirdImage)) {
        *errorString = QStringLiteral("failed to create test media files");
        return nullptr;
    }

    *imageSourcePath = thirdImage;
    *videoSourcePath = secondVideo;
    return directory;
}

std::unique_ptr<QTemporaryDir> createDirectoryCollection(QString* sourcePath, QString* errorString)
{
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        *errorString = QStringLiteral("temporary image document directory was not created");
        return nullptr;
    }

    for (int index = 1; index <= 3; ++index) {
        const QString path
            = directory->filePath(QStringLiteral("%1.png").arg(index, 2, 10, QLatin1Char('0')));
        if (!writeTestPng(path)) {
            *errorString = QStringLiteral("failed to write test image %1").arg(path);
            return nullptr;
        }
    }

    *sourcePath = directory->path();
    return directory;
}

std::unique_ptr<QTemporaryDir> createComicBookArchive(QString* sourcePath, QString* errorString)
{
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        *errorString = QStringLiteral("temporary comic archive directory was not created");
        return nullptr;
    }

    const QByteArray firstPage = encodedTestPng(Qt::blue);
    const QByteArray secondPage = encodedTestPng(Qt::green);
    if (firstPage.isEmpty() || secondPage.isEmpty()) {
        *errorString = QStringLiteral("failed to encode test archive pages");
        return nullptr;
    }

    const QString archivePath = directory->filePath(QStringLiteral("book.cbz"));
    KZip archive(archivePath);
    if (!archive.open(QIODevice::WriteOnly)) {
        *errorString = QStringLiteral("failed to open test comic archive for writing");
        return nullptr;
    }
    if (!archive.writeFile(QStringLiteral("01.png"), firstPage)
        || !archive.writeFile(QStringLiteral("02.png"), secondPage)) {
        *errorString = QStringLiteral("failed to write test comic archive pages");
        return nullptr;
    }
    archive.close();

    *sourcePath = archivePath;
    return directory;
}

std::unique_ptr<QTemporaryDir> createPortraitComicBookArchive(
    QString* sourcePath, QString* errorString)
{
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        *errorString = QStringLiteral("temporary portrait comic archive directory was not created");
        return nullptr;
    }

    const QSize portraitSize(60, 80);
    const QList<QByteArray> pages {
        encodedTestPng(Qt::blue, portraitSize),
        encodedTestPng(Qt::green, portraitSize),
        encodedTestPng(Qt::red, portraitSize),
        encodedTestPng(Qt::cyan, portraitSize),
        encodedTestPng(Qt::magenta, portraitSize),
    };
    for (const QByteArray& page : pages) {
        if (page.isEmpty()) {
            *errorString = QStringLiteral("failed to encode portrait comic archive pages");
            return nullptr;
        }
    }

    const QString archivePath = directory->filePath(QStringLiteral("portrait-book.cbz"));
    KZip archive(archivePath);
    if (!archive.open(QIODevice::WriteOnly)) {
        *errorString = QStringLiteral("failed to open portrait comic archive for writing");
        return nullptr;
    }
    for (qsizetype index = 0; index < pages.size(); ++index) {
        if (!archive.writeFile(QStringLiteral("%1.png").arg(index + 1, 2, 10, QLatin1Char('0')),
                pages.at(index))) {
            *errorString = QStringLiteral("failed to write portrait comic archive pages");
            archive.close();
            return nullptr;
        }
    }
    archive.close();

    *sourcePath = archivePath;
    return directory;
}

MainWindowFixture createMainWindowFixture(
    const QUrl& initialSourceUrl, kiriview::TimerScheduler timerScheduler);

MainWindowFixture createMainWindowFixture(const QUrl& initialSourceUrl)
{
    return createMainWindowFixture(initialSourceUrl, {});
}

MainWindowFixture createMainWindowFixture() { return createMainWindowFixture(QUrl()); }

MainWindowFixture createMainWindowFixture(
    const QUrl& initialSourceUrl, kiriview::TimerScheduler timerScheduler)
{
    MainWindowFixture fixture;
    registerKiriViewQmlTypes();
    fixture.engine = std::make_unique<QQmlApplicationEngine>();
    addEnvironmentImportPaths(*fixture.engine);
    fixture.engine->addImportPath(QDir(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR))
            .absoluteFilePath(QStringLiteral("../../src/qml")));
    KLocalization::setupLocalizedContext(fixture.engine.get());
    kiriview::registerApplicationImageProviders(*fixture.engine);
    auto* documentSession = new ToolbarTestDocumentSession(fixture.engine.get());
    documentSession->setObjectName(QStringLiteral("documentSession"));
    fixture.windowShell = new KiriWindowShell(std::move(timerScheduler), fixture.engine.get());
    fixture.application = new KiriViewApplication(fixture.engine.get());
    kiriview::composeApplicationRuntimeGraph(
        *fixture.application, *documentSession, *fixture.windowShell);
    QVariantMap initialProperties;
    initialProperties.insert(
        QStringLiteral("kiriApplication"), QVariant::fromValue(fixture.application));
    initialProperties.insert(
        QStringLiteral("documentSession"), QVariant::fromValue(documentSession));
    initialProperties.insert(
        QStringLiteral("windowShell"), QVariant::fromValue(fixture.windowShell));
    fixture.engine->setInitialProperties(initialProperties);

    fixture.engine->load(mainQmlUrl());
    if (fixture.engine->rootObjects().isEmpty()) {
        fixture.errorString = QStringLiteral("Main.qml did not create a root object");
        return fixture;
    }

    fixture.window = qobject_cast<QQuickWindow*>(fixture.engine->rootObjects().constFirst());
    if (fixture.window == nullptr) {
        fixture.errorString = QStringLiteral("Main.qml root object is not a QQuickWindow");
        return fixture;
    }
    kiriview::attachApplicationRuntimeWindow(
        *fixture.application, *fixture.windowShell, *fixture.window);
    if (!initialSourceUrl.isEmpty()) {
        documentSession->setSourceUrl(initialSourceUrl);
    }

    if (!QTest::qWaitForWindowExposed(fixture.window)) {
        fixture.errorString = QStringLiteral("main window was not exposed");
        return fixture;
    }

    return fixture;
}

KiriImageViewportSurface* readyImageViewportSurface(QObject* root)
{
    const QList<KiriImageViewportSurface*> surfaces = root->findChildren<KiriImageViewportSurface*>(
        QStringLiteral("imageViewportSurface"), Qt::FindChildrenRecursively);
    for (KiriImageViewportSurface* surface : surfaces) {
        if (effectivelyVisible(surface) && surface->viewport() != nullptr
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready
            && surface->width() > 0 && surface->height() > 0) {
            return surface;
        }
    }
    return nullptr;
}

QString imageViewportStateReport(QObject* root)
{
    QStringList states;
    KiriDocumentSession* documentSession = findDocumentSession(root);
    if (documentSession != nullptr && documentSession->imageDocument() != nullptr) {
        KiriImageDocument* imageDocument = documentSession->imageDocument();
        states.append(QStringLiteral("document status=%1 displayed=%2")
                .arg(static_cast<int>(imageDocument->status()))
                .arg(imageDocument->displayedUrl().toString()));
    }
    const QList<KiriImageViewportSurface*> surfaces = root->findChildren<KiriImageViewportSurface*>(
        QStringLiteral("imageViewportSurface"), Qt::FindChildrenRecursively);
    for (KiriImageViewportSurface* surface : surfaces) {
        QStringList ancestors;
        for (QQuickItem* ancestor = surface->parentItem(); ancestor != nullptr;
            ancestor = ancestor->parentItem()) {
            ancestors.append(QStringLiteral("%1:%2x%3")
                    .arg(ancestor->objectName())
                    .arg(ancestor->width())
                    .arg(ancestor->height()));
        }
        const int status = surface->viewport() == nullptr
            ? -1
            : static_cast<int>(surface->viewport()->state().request().status());
        states.append(QStringLiteral("visible=%1 effectiveVisible=%2 status=%3 size=%4x%5 "
                                     "ancestors=%6")
                .arg(surface->isVisible())
                .arg(effectivelyVisible(surface))
                .arg(status)
                .arg(surface->width())
                .arg(surface->height())
                .arg(ancestors.join(QStringLiteral(" > "))));
    }
    return states.join(QStringLiteral("; "));
}

void openSourceUrl(MainWindowFixture& fixture, const QString& sourcePath)
{
    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    documentSession->setSourceUrl(QUrl::fromLocalFile(sourcePath));
}

void resizeWindow(MainWindowFixture& fixture, const QSize& size)
{
    fixture.window->resize(size);
    QTRY_COMPARE(fixture.window->size(), size);
    QCoreApplication::processEvents();
}

void compareToolbarPageReadout(
    MainWindowFixture& fixture, const QString& currentText, const QString& countText, bool enabled)
{
    QQuickItem* pageNumberField = findQuickItem(fixture.window, QStringLiteral("pageNumberField"));
    QQuickItem* pageCountLabel = findQuickItem(fixture.window, QStringLiteral("pageCountLabel"));
    QQuickItem* leftPageButton
        = findQuickItem(fixture.window, QStringLiteral("leftPageNavigationButton"));
    QQuickItem* rightPageButton
        = findQuickItem(fixture.window, QStringLiteral("rightPageNavigationButton"));
    QVERIFY(pageNumberField != nullptr);
    QVERIFY(pageCountLabel != nullptr);
    QVERIFY(leftPageButton != nullptr);
    QVERIFY(rightPageButton != nullptr);

    QTRY_COMPARE(pageNumberField->property("text").toString(), currentText);
    QTRY_COMPARE(pageCountLabel->property("text").toString(), countText);
    QCOMPARE(pageNumberField->isEnabled(), enabled);
}

bool popupOpen(QObject* popup)
{
    return popup->property("visible").toBool() || popup->property("opened").toBool();
}

bool invokeBool(QObject* object, const char* method)
{
    QVariant result;
    const bool invoked = QMetaObject::invokeMethod(
        object, method, Qt::DirectConnection, Q_RETURN_ARG(QVariant, result));
    return invoked && result.toBool();
}

void invokeWithVariant(QObject* object, const char* method, const QVariant& argument)
{
    QVERIFY(
        QMetaObject::invokeMethod(object, method, Qt::DirectConnection, Q_ARG(QVariant, argument)));
}

QPoint itemCenter(QQuickItem* item)
{
    if (item == nullptr || item->width() <= 0 || item->height() <= 0) {
        return QPoint(-1, -1);
    }

    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
}

void clickItem(QQuickWindow* window, QQuickItem* item, Qt::MouseButton button)
{
    const QPoint point = itemCenter(item);
    QVERIFY(point.x() >= 0);
    QVERIFY(point.y() >= 0);
    QTest::mouseClick(window, button, Qt::NoModifier, point);
    QCoreApplication::processEvents();
}

QQuickItem* findAdvancedMetadataSection(QObject* root)
{
    const QList<QQuickItem*> titles = visibleItemsByText(root, QStringLiteral("Advanced Metadata"));
    for (QQuickItem* title : titles) {
        QQuickItem* row = title->parentItem();
        if (row == nullptr) {
            continue;
        }
        QQuickItem* section = row->parentItem();
        if (section != nullptr && section->property("expanded").isValid()) {
            return section;
        }
    }
    return nullptr;
}

void wheelItem(QQuickWindow* window, QQuickItem* item, int angleDeltaY)
{
    const QPoint point = itemCenter(item);
    QVERIFY(point.x() >= 0);
    QVERIFY(point.y() >= 0);

    QWheelEvent event(QPointF(point), window->mapToGlobal(point), QPoint(), QPoint(0, angleDeltaY),
        Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
    QCoreApplication::sendEvent(window, &event);
    QCoreApplication::processEvents();
}

void rightButtonWheelItem(QQuickWindow* window, QQuickItem* item, int angleDeltaY)
{
    const QPoint point = itemCenter(item);
    QVERIFY(point.x() >= 0);
    QVERIFY(point.y() >= 0);

    QTest::mousePress(window, Qt::RightButton, Qt::NoModifier, point);
    QWheelEvent event(QPointF(point), window->mapToGlobal(point), QPoint(), QPoint(0, angleDeltaY),
        Qt::RightButton, Qt::NoModifier, Qt::ScrollUpdate, false);
    QCoreApplication::sendEvent(window, &event);
    QTest::mouseRelease(window, Qt::RightButton, Qt::NoModifier, point);
    QCoreApplication::processEvents();
}

ImageViewportCoordinateResult mapViewportPointToDisplayedSpread(
    const ImageViewport& viewport, QPointF viewportPoint)
{
    ImageViewportCoordinateInput input;
    input.setSourceSpace(ImageViewportCoordinateSpace::Item);
    input.setTargetSpace(ImageViewportCoordinateSpace::DisplayedSpread);
    input.setPoint(viewportPoint);
    return viewport.mapPoint(input);
}

bool pointsApproximatelyEqual(QPointF left, QPointF right, qreal tolerance = 0.001)
{
    return std::abs(left.x() - right.x()) <= tolerance
        && std::abs(left.y() - right.y()) <= tolerance;
}

bool valuesApproximatelyEqual(double left, double right, double relativeTolerance = 0.000001)
{
    const double scale = std::max({ 1.0, std::abs(left), std::abs(right) });
    return std::abs(left - right) <= relativeTolerance * scale;
}

void sendNativeGesture(QQuickWindow* window, const QPointingDevice* device,
    Qt::NativeGestureType type, QPointF scenePosition, qreal value, quint64 sequenceId)
{
    const QPointF globalPosition(window->mapToGlobal(scenePosition.toPoint()));
    QNativeGestureEvent event(type, device, 2, scenePosition, scenePosition, globalPosition, value,
        QPointF(), sequenceId);
    QCoreApplication::sendEvent(window, &event);
    QCoreApplication::processEvents();
}

void flushTouchEvents(QQuickWindow& window)
{
    window.update();
    static_cast<void>(window.grabWindow());
    QCoreApplication::processEvents();
}

void moveMouse(QQuickWindow* window, const QPoint& point)
{
    QTest::mouseMove(window, point);
    QCoreApplication::processEvents();
}

void closePopup(QObject* popup)
{
    popup->setProperty("visible", false);
    QCoreApplication::processEvents();
}

bool zoomApproximatelyEqual(double left, double right) { return std::abs(left - right) < 0.001; }
}

void TestMainWindowToolBar::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QGuiApplication::setApplicationDisplayName(QStringLiteral("KiriView"));
    resetConfig();

    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
}

void TestMainWindowToolBar::init()
{
    resetConfig();
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral(".*Created graphical object was not placed in the graphics scene.*")));
}

void TestMainWindowToolBar::startupCreatesOneVisibleToolbarWithDisabledMediaControls()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    QTRY_COMPARE(controlToolBars(fixture.window).size(), 1);
    QQuickItem* toolbar = controlToolBars(fixture.window).constFirst();
    QVERIFY(toolbar->isVisible());
    QCOMPARE(toolbar->objectName(), QStringLiteral("mainImageToolBar"));

    QQuickItem* leftPageButton
        = findQuickItem(fixture.window, QStringLiteral("leftPageNavigationButton"));
    QQuickItem* rightPageButton
        = findQuickItem(fixture.window, QStringLiteral("rightPageNavigationButton"));
    QVERIFY(leftPageButton != nullptr);
    QVERIFY(rightPageButton != nullptr);
    QVERIFY(!leftPageButton->isEnabled());
    QVERIFY(!rightPageButton->isEnabled());
    compareToolbarPageReadout(fixture, QStringLiteral("–"), QStringLiteral("–"), false);

    QQuickItem* zoomSpinBox = findQuickItem(fixture.window, QStringLiteral("zoomSpinBox"));
    QQuickItem* zoomTextInput = findQuickItem(fixture.window, QStringLiteral("zoomTextInput"));
    QVERIFY(zoomSpinBox != nullptr);
    QVERIFY(zoomTextInput != nullptr);
    QVERIFY(!zoomSpinBox->isEnabled());
    QTRY_COMPARE(zoomSpinBox->property("value").toInt(), 0);
    QTRY_COMPARE(zoomTextInput->property("text").toString(), QStringLiteral("    -"));

    const QList<QQuickItem*> visibleApplicationMenuButtons
        = visibleItemsByObjectName(fixture.window, QStringLiteral("toolbarApplicationMenuButton"));
    QCOMPARE(visibleApplicationMenuButtons.size(), 1);
    QVERIFY(visibleApplicationMenuButtons.constFirst()->isEnabled());
}

void TestMainWindowToolBar::shellOwnsFinalWindowTitleProjection()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("title.png"));
    QVERIFY(writeTestPng(imagePath));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QCOMPARE(fixture.windowShell->property("windowTitle").toString(), QStringLiteral("KiriView"));
    QCOMPARE(fixture.window->title(), fixture.windowShell->property("windowTitle").toString());

    openSourceUrl(fixture, imagePath);
    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    QTRY_VERIFY(documentSession->activeImageReady());
    QTRY_VERIFY(fixture.windowShell->property("windowTitle")
            .toString()
            .contains(QStringLiteral("title.png")));
    QTRY_COMPARE(fixture.window->title(), fixture.windowShell->property("windowTitle").toString());
}

void TestMainWindowToolBar::toolbarPageReadoutUsesSystemFixedWidthFont()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    QQuickItem* pageNumberField = findQuickItem(fixture.window, QStringLiteral("pageNumberField"));
    QQuickItem* pageCountLabel = findQuickItem(fixture.window, QStringLiteral("pageCountLabel"));
    const QList<QQuickItem*> pageCountSeparatorLabels
        = visibleItemsByText(fixture.window, QStringLiteral("of"));
    QVERIFY(pageNumberField != nullptr);
    QVERIFY(pageCountLabel != nullptr);
    QCOMPARE(pageCountSeparatorLabels.size(), 1);

    const QString fixedWidthFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    QVERIFY(!fixedWidthFamily.isEmpty());
    QCOMPARE(fontFamily(pageNumberField), fixedWidthFamily);
    QCOMPARE(fontFamily(pageCountSeparatorLabels.constFirst()), fixedWidthFamily);
    QCOMPARE(fontFamily(pageCountLabel), fixedWidthFamily);
}

void TestMainWindowToolBar::startupInitialDirectImageRendersMainViewport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("startup.png"));
    QVERIFY(writeTestPng(imagePath));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(imagePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    QTRY_VERIFY(documentSession->activeImageReady());
    QTRY_COMPARE(documentSession->imageDocument()->status(), KiriImageDocument::Status::Ready);

    QQuickItem* thumbnailPanel = findQuickItem(fixture.window, QStringLiteral("thumbnailPanel"));
    QVERIFY(thumbnailPanel != nullptr);
    QVERIFY(!thumbnailPanel->isVisible());

    KiriImageViewportSurface* viewportSurface = nullptr;
    QTRY_VERIFY2((viewportSurface = readyImageViewportSurface(fixture.window)) != nullptr,
        qPrintable(imageViewportStateReport(fixture.window)));
    QVERIFY(viewportSurface->document() == documentSession->imageDocument());
    QVERIFY(viewportSurface->width() > 0);
    QVERIFY(viewportSurface->height() > 0);
}

void TestMainWindowToolBar::startupInitialComicArchiveRendersAndNavigatesMainViewport()
{
    QString archivePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> archiveDirectory
        = createComicBookArchive(&archivePath, &errorString);
    QVERIFY2(archiveDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(archivePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    QTRY_VERIFY(documentSession->activeImageReady());
    QTRY_COMPARE(documentSession->imageDocument()->status(), KiriImageDocument::Status::Ready);
    compareToolbarPageReadout(fixture, QStringLiteral("1"), QStringLiteral("2"), true);

    QQuickItem* thumbnailPanel = findQuickItem(fixture.window, QStringLiteral("thumbnailPanel"));
    QVERIFY(thumbnailPanel != nullptr);
    QVERIFY(!thumbnailPanel->isVisible());

    KiriImageViewportSurface* viewportSurface = nullptr;
    QTRY_VERIFY2((viewportSurface = readyImageViewportSurface(fixture.window)) != nullptr,
        qPrintable(imageViewportStateReport(fixture.window)));
    const QUrl firstPageSource = documentSession->imageDocument()->displayedUrl();
    QVERIFY(!firstPageSource.isEmpty());

    documentSession->imageDocument()->openNextPage();
    compareToolbarPageReadout(fixture, QStringLiteral("2"), QStringLiteral("2"), true);
    QTRY_VERIFY(readyImageViewportSurface(fixture.window) == viewportSurface
        && documentSession->imageDocument()->displayedUrl() != firstPageSource);
    const QUrl secondPageSource = documentSession->imageDocument()->displayedUrl();
    QVERIFY(!secondPageSource.isEmpty());

    documentSession->imageDocument()->openPreviousPage();
    compareToolbarPageReadout(fixture, QStringLiteral("1"), QStringLiteral("2"), true);
    QTRY_VERIFY(readyImageViewportSurface(fixture.window) == viewportSurface
        && documentSession->imageDocument()->displayedUrl() != secondPageSource);
    QVERIFY(!documentSession->imageDocument()->displayedUrl().isEmpty());
    QVERIFY(!thumbnailPanel->isVisible());
}

void TestMainWindowToolBar::outsideScopePresentationCommitResetsFitSelection()
{
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    ScopedToolbarImageWorkerSchedulerOverride workerSchedulerOverride(workerScheduler.scheduler());
    QString archivePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> archiveDirectory
        = createComicBookArchive(&archivePath, &errorString);
    QVERIFY2(archiveDirectory != nullptr, qPrintable(errorString));
    QTemporaryDir directImageDirectory;
    QVERIFY(directImageDirectory.isValid());
    const QString directImagePath
        = directImageDirectory.filePath(QStringLiteral("outside-scope.png"));
    const QString missingImagePath
        = directImageDirectory.filePath(QStringLiteral("missing-outside-scope.png"));
    QVERIFY(writeTestPng(directImagePath));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(archivePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QObject* toolbar = findObject(fixture.window, QStringLiteral("mainImageToolBar"));
    QVERIFY(toolbar != nullptr);

    std::size_t nextWorkerSchedule = 0;
    const auto driveUntilReady = [&]() {
        for (int attempt = 0; attempt < 2'000 && !documentSession->activeImageReady(); ++attempt) {
            kiriview::TestSupport::runOutstandingImageWorkerSchedules(
                workerScheduler, nextWorkerSchedule);
            fixture.window->update();
            static_cast<void>(fixture.window->grabWindow());
            QCoreApplication::processEvents();
            QTest::qWait(1);
        }
        QVERIFY2(documentSession->activeImageReady(),
            qPrintable(imageViewportStateReport(fixture.window)));
    };

    driveUntilReady();
    QVERIFY(imageDocument->requestFitMode(KiriImageDocument::ZoomMode::FitWidth));
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);
    QCOMPARE(imageDocument->fitModeSelection(), KiriImageDocument::ZoomMode::FitWidth);
    QTRY_COMPARE(toolbar->property("presentedFitModeSelection").toInt(),
        static_cast<int>(KiriImageDocument::ZoomMode::FitWidth));

    QVERIFY(imageDocument->requestManualZoomPercent(150.0));
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
    QCOMPARE(imageDocument->fitModeSelection(), KiriImageDocument::ZoomMode::FitWidth);
    QCOMPARE(toolbar->property("presentedFitModeSelection").toInt(),
        static_cast<int>(KiriImageDocument::ZoomMode::FitWidth));

    imageDocument->openNextPage();
    driveUntilReady();
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
    QCOMPARE(imageDocument->fitModeSelection(), KiriImageDocument::ZoomMode::FitWidth);
    QCOMPARE(toolbar->property("presentedFitModeSelection").toInt(),
        static_cast<int>(KiriImageDocument::ZoomMode::FitWidth));

    openSourceUrl(fixture, missingImagePath);
    for (int attempt = 0;
        attempt < 2'000 && imageDocument->status() != KiriImageDocument::Status::Error; ++attempt) {
        kiriview::TestSupport::runOutstandingImageWorkerSchedules(
            workerScheduler, nextWorkerSchedule);
        fixture.window->update();
        static_cast<void>(fixture.window->grabWindow());
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }
    QCOMPARE(imageDocument->status(), KiriImageDocument::Status::Error);
    QCOMPARE(imageDocument->fitModeSelection(), KiriImageDocument::ZoomMode::FitWidth);
    QTRY_COMPARE(toolbar->property("presentedFitModeSelection").toInt(),
        static_cast<int>(KiriImageDocument::ZoomMode::FitWidth));

    openSourceUrl(fixture, directImagePath);
    QCOMPARE(imageDocument->status(), KiriImageDocument::Status::Loading);
    QCOMPARE(imageDocument->fitModeSelection(), KiriImageDocument::ZoomMode::FitWidth);
    QCOMPARE(toolbar->property("presentedFitModeSelection").toInt(),
        static_cast<int>(KiriImageDocument::ZoomMode::FitWidth));
    driveUntilReady();
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Fit);
    QCOMPARE(imageDocument->fitModeSelection(), KiriImageDocument::ZoomMode::Fit);
    QTRY_COMPARE(toolbar->property("presentedFitModeSelection").toInt(),
        static_cast<int>(KiriImageDocument::ZoomMode::Fit));
}

void TestMainWindowToolBar::comicPageReplacementKeepsRightToolbarPresentationStable()
{
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    ScopedToolbarImageWorkerSchedulerOverride workerSchedulerOverride(workerScheduler.scheduler());
    QString archivePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> archiveDirectory
        = createComicBookArchive(&archivePath, &errorString);
    QVERIFY2(archiveDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(archivePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);

    std::size_t nextWorkerSchedule = 0;
    for (int attempt = 0; attempt < 2'000 && !documentSession->activeImageReady(); ++attempt) {
        if (nextWorkerSchedule < workerScheduler.scheduleCount()) {
            workerScheduler.runWork(nextWorkerSchedule);
            workerScheduler.finish(nextWorkerSchedule);
            ++nextWorkerSchedule;
        }
        fixture.window->update();
        static_cast<void>(fixture.window->grabWindow());
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }
    QVERIFY2(
        documentSession->activeImageReady(), qPrintable(imageViewportStateReport(fixture.window)));

    QObject* toolbar = findObject(fixture.window, QStringLiteral("mainImageToolBar"));
    QVERIFY(toolbar != nullptr);
    const auto toolbarAction = [toolbar](const char* propertyName) {
        return qvariant_cast<QObject*>(toolbar->property(propertyName));
    };
    const QList<QObject*> presentedActions {
        toolbarAction("rightToLeftToolbarAction"),
        toolbarAction("twoPageToolbarAction"),
        toolbarAction("fitMenuAction"),
        toolbarAction("zoomLevelAction"),
    };
    for (QObject* action : presentedActions) {
        QVERIFY(action != nullptr);
        QVERIFY(action->property("presentationEnabled").toBool());
        QVERIFY(action->property("enabled").toBool());
    }

    QQuickItem* zoomTextInput = findQuickItem(fixture.window, QStringLiteral("zoomTextInput"));
    QVERIFY(zoomTextInput != nullptr);
    const QString readyZoomText = zoomTextInput->property("text").toString();
    QVERIFY(!readyZoomText.isEmpty());
    QVERIFY(toolbar->property("presentedZoomPercentAvailable").toBool());
    QVERIFY(toolbar->property("presentedZoomPercentKnown").toBool());
    QVERIFY(documentSession->imageDocument()->completeAuthoritativeDisplayAvailable());
    QVERIFY(!documentSession->activeImageReplacementFallbackAvailable());

    const QUrl firstPageUrl = documentSession->imageDocument()->displayedUrl();
    documentSession->imageDocument()->openNextPage();

    QCOMPARE(documentSession->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QVERIFY(!documentSession->activeImageReady());
    QVERIFY(documentSession->activeImageReplacementFallbackAvailable());
    fixture.window->update();
    static_cast<void>(fixture.window->grabWindow());
    for (QObject* action : presentedActions) {
        QVERIFY(action->property("presentationEnabled").toBool());
        QVERIFY(!action->property("enabled").toBool());
    }
    QCOMPARE(zoomTextInput->property("text").toString(), readyZoomText);

    QVERIFY(!documentSession->activeZoomEditable());
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    const KiriImageDocument::ZoomMode replacementZoomModeBefore = imageDocument->zoomMode();
    const double replacementZoomPercentBefore = imageDocument->zoomPercent();
    QVERIFY(
        !imageDocument->requestViewportPinchUpdate(1.25, QPointF(10.0, 10.0), QPointF(10.0, 10.0)));
    QCOMPARE(imageDocument->status(), KiriImageDocument::Status::Loading);
    QVERIFY(documentSession->activeImageReplacementFallbackAvailable());
    QCOMPARE(imageDocument->zoomMode(), replacementZoomModeBefore);
    QCOMPARE(imageDocument->zoomPercent(), replacementZoomPercentBefore);
    const bool rightToLeftBefore = imageDocument->rightToLeftReadingEnabled();
    const bool twoPageBefore = imageDocument->twoPageModeEnabled();
    const KiriImageDocument::ZoomMode fitModeBefore = imageDocument->fitModeSelection();
    QVERIFY(QMetaObject::invokeMethod(presentedActions.at(0), "trigger", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(presentedActions.at(1), "trigger", Qt::DirectConnection));
    invokeWithVariant(
        toolbar, "triggerFitMode", static_cast<int>(KiriImageDocument::ZoomMode::FitWidth));
    QCOMPARE(imageDocument->rightToLeftReadingEnabled(), rightToLeftBefore);
    QCOMPARE(imageDocument->twoPageModeEnabled(), twoPageBefore);
    QCOMPARE(imageDocument->fitModeSelection(), fitModeBefore);

    for (int attempt = 0; attempt < 2'000 && !documentSession->activeImageReady(); ++attempt) {
        if (nextWorkerSchedule < workerScheduler.scheduleCount()) {
            workerScheduler.runWork(nextWorkerSchedule);
            workerScheduler.finish(nextWorkerSchedule);
            ++nextWorkerSchedule;
        }
        fixture.window->update();
        static_cast<void>(fixture.window->grabWindow());
        QCoreApplication::processEvents();
        QTest::qWait(1);
    }

    QVERIFY(documentSession->activeImageReady());
    QVERIFY(documentSession->imageDocument()->displayedUrl() != firstPageUrl);
    for (QObject* action : presentedActions) {
        QVERIFY(action->property("presentationEnabled").toBool());
        QVERIFY(action->property("enabled").toBool());
    }
}

void TestMainWindowToolBar::spreadPageReplacementKeepsRightToolbarPresentationStable()
{
    QString archivePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> archiveDirectory
        = createPortraitComicBookArchive(&archivePath, &errorString);
    QVERIFY2(archiveDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(archivePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    KiriViewApplication* application = findApplication(fixture.window);
    QVERIFY(documentSession != nullptr);
    QVERIFY(application != nullptr);

    const auto renderFrame = [&fixture]() {
        fixture.window->update();
        static_cast<void>(fixture.window->grabWindow());
        QCoreApplication::processEvents();
    };

    QTRY_VERIFY2(
        documentSession->activeImageReady(), qPrintable(imageViewportStateReport(fixture.window)));
    QTRY_COMPARE(documentSession->imageDocument()->currentPageNumber(), 1);

    documentSession->imageDocument()->openNextPage();
    QTRY_VERIFY2(
        documentSession->activeImageReady(), qPrintable(imageViewportStateReport(fixture.window)));
    QTRY_COMPARE(documentSession->imageDocument()->currentPageNumber(), 2);

    documentSession->imageDocument()->requestToggleTwoPageMode();
    QTRY_VERIFY2(documentSession->activeImageReady()
            && documentSession->imageDocument()->secondaryPageVisible()
            && documentSession->imageDocument()->currentPageNumber() == 2
            && documentSession->imageDocument()->currentLastPageNumber() == 3,
        qPrintable(imageViewportStateReport(fixture.window)));
    QTRY_COMPARE(application->imageToolbarPresentationPhase(),
        KiriViewApplication::ImageToolbarPresentationCurrent);

    QObject* toolbar = findObject(fixture.window, QStringLiteral("mainImageToolBar"));
    QVERIFY(toolbar != nullptr);
    const auto toolbarAction = [toolbar](const char* propertyName) {
        return qvariant_cast<QObject*>(toolbar->property(propertyName));
    };
    const QList<QObject*> presentedActions {
        toolbarAction("rightToLeftToolbarAction"),
        toolbarAction("twoPageToolbarAction"),
        toolbarAction("fitMenuAction"),
        toolbarAction("zoomLevelAction"),
    };
    for (QObject* action : presentedActions) {
        QVERIFY(action != nullptr);
        QVERIFY(action->property("presentationEnabled").toBool());
        QVERIFY(action->property("enabled").toBool());
    }
    QObject* twoPageAction = presentedActions.at(1);
    QVERIFY(twoPageAction->property("presentationChecked").toBool());
    QVERIFY(application->imageToolbarCollectionControlsVisible());

    QQuickItem* zoomTextInput = findQuickItem(fixture.window, QStringLiteral("zoomTextInput"));
    QVERIFY(zoomTextInput != nullptr);
    const QString spreadZoomText = zoomTextInput->property("text").toString();
    const qreal spreadZoomPercent = toolbar->property("presentedZoomPercent").toReal();
    QVERIFY(!spreadZoomText.isEmpty());
    QVERIFY(toolbar->property("presentedZoomPercentAvailable").toBool());
    QVERIFY(toolbar->property("presentedZoomPercentKnown").toBool());

    bool observedUnavailablePresentation = false;
    bool observedRightActionPresentationLoss = false;
    bool observedCollectionControlsHidden = false;
    bool observedZoomPresentationChange = false;
    bool observedRetainedPreviousPresentation = false;
    const auto observePresentation = [&]() {
        observedUnavailablePresentation = observedUnavailablePresentation
            || application->imageToolbarPresentationPhase()
                == KiriViewApplication::ImageToolbarPresentationUnavailable;
        observedRetainedPreviousPresentation = observedRetainedPreviousPresentation
            || application->imageToolbarPresentationPhase()
                == KiriViewApplication::ImageToolbarPresentationRetainedPrevious;
        observedRightActionPresentationLoss = observedRightActionPresentationLoss
            || !application->imageToolbarActionAppearanceEnabled(
                KiriViewApplication::ViewToggleRightToLeftReadingAction)
            || !application->imageToolbarActionAppearanceEnabled(
                KiriViewApplication::ViewToggleTwoPageModeAction)
            || !application->imageToolbarActionAppearanceEnabled(KiriViewApplication::ViewFitAction)
            || !application->imageToolbarZoomAppearanceEnabled()
            || std::ranges::any_of(presentedActions,
                [](QObject* action) { return !action->property("presentationEnabled").toBool(); });
        observedCollectionControlsHidden = observedCollectionControlsHidden
            || !application->imageToolbarCollectionControlsVisible();
        observedZoomPresentationChange = observedZoomPresentationChange
            || !toolbar->property("presentedZoomPercentAvailable").toBool()
            || !toolbar->property("presentedZoomPercentKnown").toBool()
            || zoomTextInput->property("text").toString() != spreadZoomText
            || !zoomApproximatelyEqual(
                toolbar->property("presentedZoomPercent").toReal(), spreadZoomPercent);
    };
    const QMetaObject::Connection presentationObservation = QObject::connect(application,
        &KiriViewApplication::actionStateRevisionChanged, application, observePresentation);

    documentSession->imageDocument()->openNextPage();
    observePresentation();
    for (int attempt = 0; attempt < 2'000
        && !(documentSession->activeImageReady()
            && documentSession->imageDocument()->secondaryPageVisible()
            && documentSession->imageDocument()->currentPageNumber() == 4
            && documentSession->imageDocument()->currentLastPageNumber() == 5
            && application->imageToolbarPresentationPhase()
                == KiriViewApplication::ImageToolbarPresentationCurrent);
        ++attempt) {
        renderFrame();
        observePresentation();
        QTest::qWait(1);
    }
    observePresentation();
    QObject::disconnect(presentationObservation);

    QVERIFY2(documentSession->activeImageReady()
            && documentSession->imageDocument()->secondaryPageVisible()
            && documentSession->imageDocument()->currentPageNumber() == 4
            && documentSession->imageDocument()->currentLastPageNumber() == 5,
        qPrintable(imageViewportStateReport(fixture.window)));
    QVERIFY(!documentSession->activeImageReplacementFallbackAvailable());
    QVERIFY(observedRetainedPreviousPresentation);
    QVERIFY(!observedUnavailablePresentation);
    QVERIFY(!observedRightActionPresentationLoss);
    QVERIFY(!observedCollectionControlsHidden);
    QVERIFY(!observedZoomPresentationChange);
    QVERIFY(application->imageToolbarCollectionControlsVisible());
    for (QObject* action : presentedActions) {
        QVERIFY(action->property("presentationEnabled").toBool());
        QVERIFY(action->property("enabled").toBool());
    }
    QVERIFY(twoPageAction->property("presentationChecked").toBool());
}

void TestMainWindowToolBar::dropOpensFirstUrlOnly()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    const QUrl firstUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/kiriview/drop-first.png"));
    const QUrl secondUrl = QUrl::fromLocalFile(QStringLiteral("/tmp/kiriview/drop-second.png"));

    QVariantList urls;
    urls.append(firstUrl);
    urls.append(secondUrl);
    invokeWithVariant(fixture.window, "openDroppedUrls", urls);

    QTRY_COMPARE(documentSession->sourceUrl(), firstUrl);
}

void TestMainWindowToolBar::fileDialogUsesSingleSelectionMode()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    QObject* fileDialog = findObject(fixture.window, QStringLiteral("openFileDialog"));
    QVERIFY(fileDialog != nullptr);

    const int propertyIndex = fileDialog->metaObject()->indexOfProperty("fileMode");
    QVERIFY(propertyIndex >= 0);
    const QMetaProperty fileModeProperty = fileDialog->metaObject()->property(propertyIndex);
    const QMetaEnum fileModeEnum = fileModeProperty.enumerator();
    QVERIFY(fileModeEnum.isValid());

    bool ok = false;
    const int openFileMode = fileModeEnum.keyToValue("OpenFile", &ok);
    QVERIFY(ok);
    QCOMPARE(fileDialog->property("fileMode").toInt(), openFileMode);
}

void TestMainWindowToolBar::directImageShowsMediaPositionAfterSiblingListing()
{
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    openSourceUrl(fixture, imageSourcePath);

    compareToolbarPageReadout(fixture, QStringLiteral("3"), QStringLiteral("3"), true);
}

void TestMainWindowToolBar::goToPageShortcutFocusesPageNumberInput()
{
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    openSourceUrl(fixture, imageSourcePath);
    compareToolbarPageReadout(fixture, QStringLiteral("3"), QStringLiteral("3"), true);

    QQuickItem* pageNumberField = findQuickItem(fixture.window, QStringLiteral("pageNumberField"));
    QVERIFY(pageNumberField != nullptr);
    QVERIFY(!pageNumberField->hasActiveFocus());

    QTest::keyClick(fixture.window, Qt::Key_G, Qt::ControlModifier);

    QTRY_VERIFY(pageNumberField->hasActiveFocus());
    QCOMPARE(pageNumberField->property("selectedText").toString(), QStringLiteral("3"));
}

void TestMainWindowToolBar::zoomShortcutFocusesZoomInput()
{
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    openSourceUrl(fixture, imageSourcePath);

    QQuickItem* zoomTextInput = findQuickItem(fixture.window, QStringLiteral("zoomTextInput"));
    QVERIFY(zoomTextInput != nullptr);
    QTRY_VERIFY(zoomTextInput->isEnabled());
    QVERIFY(!zoomTextInput->hasActiveFocus());

    QTest::keyClick(fixture.window, Qt::Key_Y, Qt::ControlModifier);

    QTRY_VERIFY(zoomTextInput->hasActiveFocus());
    QCOMPARE(zoomTextInput->property("selectedText").toString(),
        zoomTextInput->property("text").toString());
}

void TestMainWindowToolBar::fullscreenZoomShortcutRevealsToolbarAndFocusesInput()
{
    kiriview::TestSupport::ManualTimerScheduler timerScheduler;
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture(QUrl(), timerScheduler.scheduler());
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));
    openSourceUrl(fixture, imageSourcePath);

    QQuickItem* toolbar = findQuickItem(fixture.window, QStringLiteral("mainImageToolBar"));
    QQuickItem* zoomTextInput = findQuickItem(fixture.window, QStringLiteral("zoomTextInput"));
    QVERIFY(toolbar != nullptr);
    QVERIFY(zoomTextInput != nullptr);
    QTRY_VERIFY(zoomTextInput->isEnabled());

    fixture.window->setVisibility(QWindow::FullScreen);
    QTRY_COMPARE(fixture.window->visibility(), QWindow::FullScreen);
    QTRY_VERIFY(fixture.windowShell->fullscreen());
    QVERIFY(fixture.windowShell->toolbarRevealed());
    moveMouse(fixture.window, QPoint(fixture.window->width() / 2, fixture.window->height() / 2));
    fixture.windowShell->reportToolbarInteractionActive(false);
    fixture.windowShell->reportPointerMoved(false);
    QVERIFY(timerScheduler.timerCount() > 0);
    {
        kiriview::TestSupport::ManualRuntimeTimer& toolbarHideTimer
            = timerScheduler.timerAt(timerScheduler.timerCount() - 1);
        QVERIFY(toolbarHideTimer.active());
        toolbarHideTimer.fire();
    }
    QTRY_VERIFY(!fixture.windowShell->toolbarRevealed());
    QVERIFY(!toolbar->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_Y, Qt::ControlModifier);

    QTRY_VERIFY(fixture.windowShell->toolbarRevealed());
    QTRY_VERIFY(toolbar->isVisible());
    QTRY_VERIFY(zoomTextInput->hasActiveFocus());
    QCOMPARE(zoomTextInput->property("selectedText").toString(),
        zoomTextInput->property("text").toString());
}

void TestMainWindowToolBar::directoryImageDocumentShowsPagePosition()
{
    QString sourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> imageDirectory
        = createDirectoryCollection(&sourcePath, &errorString);
    QVERIFY2(imageDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    openSourceUrl(fixture, sourcePath);

    compareToolbarPageReadout(fixture, QStringLiteral("1"), QStringLiteral("3"), true);
}

void TestMainWindowToolBar::pageNumberWheelNavigatesSemantically()
{
    QString sourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> imageDirectory
        = createDirectoryCollection(&sourcePath, &errorString);
    QVERIFY2(imageDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    openSourceUrl(fixture, sourcePath);
    compareToolbarPageReadout(fixture, QStringLiteral("1"), QStringLiteral("3"), true);

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QQuickItem* pageNumberField = findQuickItem(fixture.window, QStringLiteral("pageNumberField"));
    QVERIFY(documentSession != nullptr);
    QVERIFY(pageNumberField != nullptr);

    wheelItem(fixture.window, pageNumberField, -120);
    QTRY_COMPARE(documentSession->activeNavigationCurrentNumber(), 2);
    QTRY_COMPARE(pageNumberField->property("text").toString(), QStringLiteral("2"));
    QTRY_VERIFY(documentSession->activeImageReady());

    wheelItem(fixture.window, pageNumberField, 120);
    QTRY_COMPARE(documentSession->activeNavigationCurrentNumber(), 1);
    QTRY_COMPARE(pageNumberField->property("text").toString(), QStringLiteral("1"));
    QTRY_VERIFY(documentSession->activeImageReady());

    wheelItem(fixture.window, pageNumberField, 120);
    QCOMPARE(documentSession->activeNavigationCurrentNumber(), 1);
}

void TestMainWindowToolBar::mediaViewportHostLoadsOnlyActiveDelegate()
{
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    QVERIFY(findQuickItem(fixture.window, QStringLiteral("mediaViewportSlot")) != nullptr);
    QVERIFY(findQuickItem(fixture.window, QStringLiteral("imageViewport")) == nullptr);
    QVERIFY(findQuickItem(fixture.window, QStringLiteral("videoViewport")) == nullptr);

    openSourceUrl(fixture, imageSourcePath);
    QTRY_VERIFY(findQuickItem(fixture.window, QStringLiteral("imageViewport")) != nullptr);
    QVERIFY(findQuickItem(fixture.window, QStringLiteral("videoViewport")) == nullptr);

    openSourceUrl(fixture, videoSourcePath);
    QTRY_VERIFY(findQuickItem(fixture.window, QStringLiteral("videoViewport")) != nullptr);
    QTRY_VERIFY(findQuickItem(fixture.window, QStringLiteral("imageViewport")) == nullptr);

    openSourceUrl(fixture, imageSourcePath);
    QTRY_VERIFY(findQuickItem(fixture.window, QStringLiteral("imageViewport")) != nullptr);
    QTRY_VERIFY(findQuickItem(fixture.window, QStringLiteral("videoViewport")) == nullptr);
}

void TestMainWindowToolBar::panelActionsToggleResizablePanels()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    KiriViewApplication* application = findApplication(fixture.window);
    QVERIFY(application != nullptr);
    QAction* infoPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleInfoPanelAction);
    QAction* thumbnailPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleThumbnailPanelAction);
    QVERIFY(infoPanelAction != nullptr);
    QVERIFY(thumbnailPanelAction != nullptr);

    QQuickItem* contentSplitView
        = findQuickItem(fixture.window, QStringLiteral("contentSplitView"));
    QQuickItem* mediaPanelSplitView
        = findQuickItem(fixture.window, QStringLiteral("mediaPanelSplitView"));
    QQuickItem* infoPanel = findQuickItem(fixture.window, QStringLiteral("infoPanel"));
    QQuickItem* infoPanelOverlay
        = findQuickItem(fixture.window, QStringLiteral("infoPanelOverlayContent"));
    QQuickItem* thumbnailPanel = findQuickItem(fixture.window, QStringLiteral("thumbnailPanel"));
    QVERIFY(contentSplitView != nullptr);
    QVERIFY(mediaPanelSplitView != nullptr);
    QVERIFY(infoPanel != nullptr);
    QVERIFY(infoPanelOverlay != nullptr);
    QVERIFY(thumbnailPanel != nullptr);
    QVERIFY(!infoPanel->isVisible());
    QVERIFY(!infoPanelOverlay->isVisible());
    QVERIFY(!thumbnailPanel->isVisible());

    infoPanelAction->trigger();
    QTRY_VERIFY(infoPanel->isVisible());
    QVERIFY(!infoPanelOverlay->isVisible());
    QVERIFY(!thumbnailPanel->isVisible());
    QVERIFY(infoPanel->width() > 0);
    QVERIFY(infoPanel->width() >= 16 * 16);
    QVERIFY(infoPanel->width() <= 20 * 32);
    QTRY_VERIFY(qAbs(infoPanel->height() - contentSplitView->height()) <= 1.0);

    thumbnailPanelAction->trigger();
    QTRY_VERIFY(thumbnailPanel->isVisible());
    QVERIFY(thumbnailPanel->height() > 0);
    QVERIFY(thumbnailPanel->width() <= mediaPanelSplitView->width());

    fixture.window->setVisibility(QWindow::FullScreen);
    QTRY_COMPARE(fixture.window->visibility(), QWindow::FullScreen);
    QTRY_VERIFY(infoPanel->isVisible());
    QTRY_VERIFY(thumbnailPanel->isVisible());

    infoPanelAction->trigger();
    thumbnailPanelAction->trigger();
    QTRY_VERIFY(!infoPanel->isVisible());
    QTRY_VERIFY(!infoPanelOverlay->isVisible());
    QTRY_VERIFY(!thumbnailPanel->isVisible());
}

void TestMainWindowToolBar::infoPanelAdvancedMetadataSectionFoldsRows()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("metadata.jpg"));
    QVERIFY(writeAdvancedMetadataJpeg(imagePath));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(imagePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    QTRY_VERIFY(documentSession->activeImageReady());
    QTRY_VERIFY(documentSession->mediaInformation()->hasAdvancedSection());
    QTRY_COMPARE(documentSession->mediaInformation()->advancedRows()->rowCount(), 1);

    KiriViewApplication* application = findApplication(fixture.window);
    QVERIFY(application != nullptr);
    QAction* infoPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleInfoPanelAction);
    QVERIFY(infoPanelAction != nullptr);
    infoPanelAction->trigger();

    QQuickItem* infoPanel = findQuickItem(fixture.window, QStringLiteral("infoPanel"));
    QVERIFY(infoPanel != nullptr);
    QTRY_VERIFY(infoPanel->isVisible());

    QTRY_COMPARE(visibleItemsByText(infoPanel, QStringLiteral("Advanced Metadata")).size(), 1);
    QCOMPARE(visibleItemsByText(infoPanel, QStringLiteral("Kiri Tester")).size(), 0);

    QQuickItem* advancedMetadataSection = findAdvancedMetadataSection(infoPanel);
    QVERIFY(advancedMetadataSection != nullptr);
    QVERIFY(advancedMetadataSection->setProperty("expanded", true));
    QCoreApplication::processEvents();
    QTRY_COMPARE(visibleItemsByText(infoPanel, QStringLiteral("Kiri Tester")).size(), 1);

    QVERIFY(advancedMetadataSection->setProperty("expanded", false));
    QCoreApplication::processEvents();
    QTRY_COMPARE(visibleItemsByText(infoPanel, QStringLiteral("Kiri Tester")).size(), 0);
}

void TestMainWindowToolBar::infoPanelUsesFixedWidthFontForFileAndValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("fixed-width-info.png"));
    QVERIFY(writeTestPng(imagePath));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(imagePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    QTRY_VERIFY(documentSession->mediaInformation()->available());

    KiriViewApplication* application = findApplication(fixture.window);
    QVERIFY(application != nullptr);
    QAction* infoPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleInfoPanelAction);
    QVERIFY(infoPanelAction != nullptr);
    infoPanelAction->trigger();

    QQuickItem* infoPanel = findQuickItem(fixture.window, QStringLiteral("infoPanel"));
    QVERIFY(infoPanel != nullptr);
    QTRY_VERIFY(infoPanel->isVisible());

    const QString fixedWidthFamily = infoPanel->property("fixedWidthFontFamily").toString();
    QVERIFY(!fixedWidthFamily.isEmpty());

    QQuickItem* title = findQuickItem(infoPanel, QStringLiteral("infoPanelTitle"));
    QQuickItem* summary = findQuickItem(infoPanel, QStringLiteral("infoPanelSummary"));
    QVERIFY(title != nullptr);
    QVERIFY(summary != nullptr);
    QCOMPARE(fontFamily(title), fixedWidthFamily);
    QCOMPARE(fontFamily(summary), fixedWidthFamily);

    QList<QQuickItem*> valueLabels;
    QTRY_VERIFY(!(
        valueLabels = visualItemsByObjectName(infoPanel, QStringLiteral("infoPanelMetadataValue")))
            .isEmpty());
    for (QQuickItem* valueLabel : valueLabels) {
        QCOMPARE(fontFamily(valueLabel), fixedWidthFamily);
    }
}

void TestMainWindowToolBar::infoPanelUsesOverlayDrawerOnNarrowWindows()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(520, 420));

    KiriViewApplication* application = findApplication(fixture.window);
    QVERIFY(application != nullptr);
    QAction* infoPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleInfoPanelAction);
    QVERIFY(infoPanelAction != nullptr);

    QQuickItem* mediaViewportSlot
        = findQuickItem(fixture.window, QStringLiteral("mediaViewportSlot"));
    QQuickItem* inlineInfoPanel = findQuickItem(fixture.window, QStringLiteral("infoPanel"));
    QQuickItem* overlayInfoPanel
        = findQuickItem(fixture.window, QStringLiteral("infoPanelOverlayContent"));
    QObject* overlayDrawer = findObject(fixture.window, QStringLiteral("infoPanelOverlayDrawer"));
    QVERIFY(mediaViewportSlot != nullptr);
    QVERIFY(inlineInfoPanel != nullptr);
    QVERIFY(overlayInfoPanel != nullptr);
    QVERIFY(overlayDrawer != nullptr);
    const qreal viewportWidthBeforeOpen = mediaViewportSlot->width();

    infoPanelAction->trigger();

    QTRY_VERIFY(!inlineInfoPanel->isVisible());
    QTRY_VERIFY(overlayInfoPanel->isVisible());
    QVERIFY(overlayDrawer->property("drawerOpen").toBool());
    QVERIFY(qAbs(mediaViewportSlot->width() - viewportWidthBeforeOpen) <= 1.0);

    const QList<QQuickItem*> closeButtons
        = visibleItemsByObjectName(fixture.window, QStringLiteral("infoPanelCloseButton"));
    QCOMPARE(closeButtons.size(), 1);
    QTRY_VERIFY(itemCenter(closeButtons.constFirst()).x() < fixture.window->width());
    clickItem(fixture.window, closeButtons.constFirst(), Qt::LeftButton);
    QTRY_VERIFY(!overlayInfoPanel->isVisible());
    QVERIFY(!overlayDrawer->property("drawerOpen").toBool());
}

void TestMainWindowToolBar::escapeClosesInfoPanelBeforeLeavingFullscreen()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    KiriViewApplication* application = findApplication(fixture.window);
    QVERIFY(application != nullptr);
    QAction* infoPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleInfoPanelAction);
    QVERIFY(infoPanelAction != nullptr);

    fixture.window->setVisibility(QWindow::FullScreen);
    QTRY_COMPARE(fixture.window->visibility(), QWindow::FullScreen);

    QQuickItem* infoPanel = findQuickItem(fixture.window, QStringLiteral("infoPanel"));
    QVERIFY(infoPanel != nullptr);
    infoPanelAction->trigger();
    QTRY_VERIFY(infoPanel->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_Escape);
    QTRY_VERIFY(!infoPanel->isVisible());
    QCOMPARE(fixture.window->visibility(), QWindow::FullScreen);

    QTest::keyClick(fixture.window, Qt::Key_Escape);
    QTRY_COMPARE(fixture.window->visibility(), QWindow::Windowed);
}

void TestMainWindowToolBar::panelShortcutsToggleResizablePanels()
{
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    openSourceUrl(fixture, imageSourcePath);
    QTRY_VERIFY(findQuickItem(fixture.window, QStringLiteral("imageViewport")) != nullptr);

    QQuickItem* infoPanel = findQuickItem(fixture.window, QStringLiteral("infoPanel"));
    QQuickItem* thumbnailPanel = findQuickItem(fixture.window, QStringLiteral("thumbnailPanel"));
    QVERIFY(infoPanel != nullptr);
    QVERIFY(thumbnailPanel != nullptr);
    QVERIFY(!infoPanel->isVisible());
    QVERIFY(!thumbnailPanel->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_I, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QVERIFY(!infoPanel->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_T, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QVERIFY(!thumbnailPanel->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_I);
    QTRY_VERIFY(infoPanel->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_T);
    QTRY_VERIFY(thumbnailPanel->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_I);
    QTRY_VERIFY(!infoPanel->isVisible());

    QTest::keyClick(fixture.window, Qt::Key_T);
    QTRY_VERIFY(!thumbnailPanel->isVisible());
}

void TestMainWindowToolBar::commandFixedShortcutsUseApplicationActions()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    KiriViewApplication* application = findApplication(fixture.window);
    QQuickItem* toolbar = findQuickItem(fixture.window, QStringLiteral("mainImageToolBar"));
    QVERIFY(application != nullptr);
    QVERIFY(toolbar != nullptr);

    QAction* showMenubarAction
        = application->actionForId(KiriViewApplication::OptionsShowMenubarAction);
    QAction* openApplicationMenuAction
        = application->actionForId(KiriViewApplication::OpenApplicationMenuAction);
    QVERIFY(showMenubarAction != nullptr);
    QVERIFY(openApplicationMenuAction != nullptr);
    QSignalSpy showMenubarSpy(showMenubarAction, &QAction::triggered);
    QSignalSpy openApplicationMenuSpy(openApplicationMenuAction, &QAction::triggered);

    QCOMPARE(application->menuPresentation(), KiriViewApplication::HamburgerMenu);
    QVERIFY(!invokeBool(toolbar, "applicationMenuOpen"));

    QTest::keyClick(fixture.window, Qt::Key_M);
    QCoreApplication::processEvents();
    QCOMPARE(showMenubarSpy.count(), 0);
    QCOMPARE(application->menuPresentation(), KiriViewApplication::HamburgerMenu);

    QTest::keyClick(fixture.window, Qt::Key_M, Qt::ControlModifier);
    QTRY_COMPARE(showMenubarSpy.count(), 1);
    QCOMPARE(application->menuPresentation(), KiriViewApplication::MenuBar);

    QTest::keyClick(fixture.window, Qt::Key_F10);
    QCoreApplication::processEvents();
    QCOMPARE(openApplicationMenuSpy.count(), 0);
    QVERIFY(!invokeBool(toolbar, "applicationMenuOpen"));

    QTest::keyClick(fixture.window, Qt::Key_M, Qt::ControlModifier);
    QTRY_COMPARE(showMenubarSpy.count(), 2);
    QCOMPARE(application->menuPresentation(), KiriViewApplication::HamburgerMenu);

    QTest::keyClick(fixture.window, Qt::Key_F10);
    QTRY_COMPARE(openApplicationMenuSpy.count(), 1);
    QTRY_VERIFY(invokeBool(toolbar, "applicationMenuOpen"));
}

void TestMainWindowToolBar::configureShortcutActionOpensApplicationOwnedDialog()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));

    QObject* dialog = fixture.window->findChild<QObject*>(
        QStringLiteral("shortcutConfigurationDialog"), Qt::FindChildrenRecursively);
    QVERIFY(dialog != nullptr);
    QVERIFY(!popupOpen(dialog));

    QAction* configureAction
        = fixture.application->actionForId(KiriViewApplication::OptionsConfigureKeybindingAction);
    QVERIFY(configureAction != nullptr);
    configureAction->trigger();

    QTRY_VERIFY(popupOpen(dialog));
    QTRY_VERIFY(fixture.window->property("helpDialogOpen").toBool());
}

void TestMainWindowToolBar::viewerRightClickOpensContextMenuOnlyFromMediaViewport()
{
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));
    openSourceUrl(fixture, imageSourcePath);
    compareToolbarPageReadout(fixture, QStringLiteral("3"), QStringLiteral("3"), true);

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QTRY_COMPARE(imageDocument->status(), KiriImageDocument::Status::Ready);
    QTRY_VERIFY(documentSession->activeImageReady());

    QObject* contextMenu = findObject(fixture.window, QStringLiteral("viewerContextMenu"));
    QQuickItem* mediaViewportSlot
        = findQuickItem(fixture.window, QStringLiteral("mediaViewportSlot"));
    QQuickItem* toolbar = findQuickItem(fixture.window, QStringLiteral("mainImageToolBar"));
    QVERIFY(contextMenu != nullptr);
    QVERIFY(mediaViewportSlot != nullptr);
    QVERIFY(toolbar != nullptr);
    QVERIFY(!popupOpen(contextMenu));

    clickItem(fixture.window, toolbar, Qt::RightButton);
    QTRY_VERIFY(!popupOpen(contextMenu));

    clickItem(fixture.window, mediaViewportSlot, Qt::RightButton);
    QTRY_VERIFY(popupOpen(contextMenu));
    closePopup(contextMenu);
    QTRY_VERIFY(!popupOpen(contextMenu));

    KiriViewApplication* application = findApplication(fixture.window);
    QVERIFY(application != nullptr);
    QAction* infoPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleInfoPanelAction);
    QAction* thumbnailPanelAction
        = application->actionForId(KiriViewApplication::ViewToggleThumbnailPanelAction);
    QVERIFY(infoPanelAction != nullptr);
    QVERIFY(thumbnailPanelAction != nullptr);
    infoPanelAction->trigger();
    thumbnailPanelAction->trigger();

    QQuickItem* infoPanel = findQuickItem(fixture.window, QStringLiteral("infoPanel"));
    QQuickItem* thumbnailPanel = findQuickItem(fixture.window, QStringLiteral("thumbnailPanel"));
    QVERIFY(infoPanel != nullptr);
    QVERIFY(thumbnailPanel != nullptr);
    QTRY_VERIFY(infoPanel->isVisible());
    QTRY_VERIFY(thumbnailPanel->isVisible());

    clickItem(fixture.window, infoPanel, Qt::RightButton);
    QTRY_VERIFY(!popupOpen(contextMenu));
    clickItem(fixture.window, thumbnailPanel, Qt::RightButton);
    QTRY_VERIFY(!popupOpen(contextMenu));

    infoPanelAction->trigger();
    thumbnailPanelAction->trigger();
    QTRY_VERIFY(!infoPanel->isVisible());
    QTRY_VERIFY(!thumbnailPanel->isVisible());

    resizeWindow(fixture, QSize(520, 420));
    fixture.window->setVisibility(QWindow::FullScreen);
    QTRY_COMPARE(fixture.window->visibility(), QWindow::FullScreen);
    clickItem(fixture.window, mediaViewportSlot, Qt::RightButton);
    QTRY_VERIFY(popupOpen(contextMenu));
}

void TestMainWindowToolBar::toolbarZoomWheelAppliesFineManualStep_data()
{
    QTest::addColumn<int>("zoomMode");

    QTest::newRow("fit") << static_cast<int>(KiriImageDocument::ZoomMode::Fit);
    QTest::newRow("fit width") << static_cast<int>(KiriImageDocument::ZoomMode::FitWidth);
    QTest::newRow("fit height") << static_cast<int>(KiriImageDocument::ZoomMode::FitHeight);
}

void TestMainWindowToolBar::toolbarZoomWheelAppliesFineManualStep()
{
    QFETCH(int, zoomMode);
    const auto requestedZoomMode = static_cast<KiriImageDocument::ZoomMode>(zoomMode);
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    openSourceUrl(fixture, imageSourcePath);

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QTRY_COMPARE(imageDocument->status(), KiriImageDocument::Status::Ready);

    QQuickItem* zoomSpinBox = findQuickItem(fixture.window, QStringLiteral("zoomSpinBox"));
    QVERIFY(zoomSpinBox != nullptr);
    QTRY_VERIFY(zoomSpinBox->isEnabled());

    QVERIFY(imageDocument->requestManualZoomPercent(100.0));
    QTRY_VERIFY(zoomApproximatelyEqual(imageDocument->zoomPercent(), 100.0));
    QVERIFY(imageDocument->requestFitMode(requestedZoomMode));
    QTRY_COMPARE(imageDocument->zoomMode(), requestedZoomMode);
    const double fitZoomPercent = imageDocument->zoomPercent();
    QVERIFY(!zoomApproximatelyEqual(fitZoomPercent, 100.0));
    const double zoomedInPercent = imageDocument->steppedManualZoomPercent(0.5);

    wheelItem(fixture.window, zoomSpinBox, 120);
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
    QTRY_VERIFY(zoomApproximatelyEqual(imageDocument->zoomPercent(), zoomedInPercent));

    wheelItem(fixture.window, zoomSpinBox, -120);
    QTRY_VERIFY(zoomApproximatelyEqual(imageDocument->zoomPercent(), fitZoomPercent));
}

void TestMainWindowToolBar::rightButtonWheelSuppressesContextMenuTap()
{
    QString imageSourcePath;
    QString videoSourcePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> mediaDirectory
        = createMediaDirectory(&imageSourcePath, &videoSourcePath, &errorString);
    QVERIFY2(mediaDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    openSourceUrl(fixture, imageSourcePath);
    compareToolbarPageReadout(fixture, QStringLiteral("3"), QStringLiteral("3"), true);

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QTRY_COMPARE(imageDocument->status(), KiriImageDocument::Status::Ready);
    QTRY_VERIFY(documentSession->activeImageReady());

    QObject* contextMenu = findObject(fixture.window, QStringLiteral("viewerContextMenu"));
    QQuickItem* mediaViewportSlot
        = findQuickItem(fixture.window, QStringLiteral("mediaViewportSlot"));
    QVERIFY(contextMenu != nullptr);
    QVERIFY(mediaViewportSlot != nullptr);
    QVERIFY(!popupOpen(contextMenu));

    rightButtonWheelItem(fixture.window, mediaViewportSlot, 120);
    QTRY_VERIFY(!popupOpen(contextMenu));

    clickItem(fixture.window, mediaViewportSlot, Qt::RightButton);
    QTRY_VERIFY(popupOpen(contextMenu));
}

void TestMainWindowToolBar::nativeTouchpadPinchFromFitModes_data()
{
    QTest::addColumn<int>("zoomMode");

    QTest::newRow("fit") << static_cast<int>(KiriImageDocument::ZoomMode::Fit);
    QTest::newRow("fit width") << static_cast<int>(KiriImageDocument::ZoomMode::FitWidth);
    QTest::newRow("fit height") << static_cast<int>(KiriImageDocument::ZoomMode::FitHeight);
}

void TestMainWindowToolBar::nativeTouchpadPinchFromFitModes()
{
    QFETCH(int, zoomMode);
    const auto requestedZoomMode = static_cast<KiriImageDocument::ZoomMode>(zoomMode);
    auto touchpad = std::unique_ptr<QPointingDevice>(
        QTest::createTouchDevice(QInputDevice::DeviceType::TouchPad));
    QVERIFY(touchpad != nullptr);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("native-pinch.png"));
    QVERIFY(writeTestPng(imagePath));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(imagePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QTRY_COMPARE(imageDocument->status(), KiriImageDocument::Status::Ready);

    KiriImageViewportSurface* viewportSurface = nullptr;
    QTRY_VERIFY2((viewportSurface = readyImageViewportSurface(fixture.window)) != nullptr,
        qPrintable(imageViewportStateReport(fixture.window)));
    ImageViewport* viewport = viewportSurface->viewport();
    QVERIFY(viewport != nullptr);

    QVERIFY(imageDocument->requestFitMode(requestedZoomMode));
    QTRY_COMPARE(imageDocument->zoomMode(), requestedZoomMode);
    const double initialZoomPercent = imageDocument->zoomPercent();
    QVERIFY(initialZoomPercent > 0.0);

    const QRectF contentRect = viewport->state().display().contentRect();
    QVERIFY(!contentRect.isEmpty());
    const QPointF sceneCentroid = viewport->mapToScene(contentRect.center().toPoint());
    const QPointF viewportCentroid = viewport->mapFromScene(sceneCentroid);
    const ImageViewportCoordinateResult anchoredSpreadPoint
        = mapViewportPointToDisplayedSpread(*viewport, viewportCentroid);
    QVERIFY(anchoredSpreadPoint.isValid());

    constexpr qreal nativeZoomValue = 0.25;
    constexpr double expectedScaleFactor = 1.0 + nativeZoomValue;
    const double expectedZoomPercent = initialZoomPercent * expectedScaleFactor;
    QVERIFY(expectedZoomPercent < imageDocument->maximumManualZoomPercent());

    constexpr quint64 gestureSequence = 17;
    sendNativeGesture(fixture.window, touchpad.get(), Qt::BeginNativeGesture, sceneCentroid, 0.0,
        gestureSequence);
    sendNativeGesture(fixture.window, touchpad.get(), Qt::RotateNativeGesture, sceneCentroid, 30.0,
        gestureSequence);
    QCOMPARE(imageDocument->zoomMode(), requestedZoomMode);
    QVERIFY(valuesApproximatelyEqual(imageDocument->zoomPercent(), initialZoomPercent));
    QCOMPARE(viewport->state().presentation().rotationDegrees(), 0);

    sendNativeGesture(fixture.window, touchpad.get(), Qt::ZoomNativeGesture, sceneCentroid,
        nativeZoomValue, gestureSequence);

    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
    QTRY_VERIFY(valuesApproximatelyEqual(imageDocument->zoomPercent(), expectedZoomPercent));
    const ImageViewportCoordinateResult spreadPointAfterZoom
        = mapViewportPointToDisplayedSpread(*viewport, viewport->mapFromScene(sceneCentroid));
    QVERIFY(spreadPointAfterZoom.isValid());
    QVERIFY(pointsApproximatelyEqual(spreadPointAfterZoom.point(), anchoredSpreadPoint.point()));

    constexpr qreal secondNativeZoomValue = -0.1;
    const double expectedSecondZoomPercent = expectedZoomPercent * (1.0 + secondNativeZoomValue);
    sendNativeGesture(fixture.window, touchpad.get(), Qt::ZoomNativeGesture, sceneCentroid,
        secondNativeZoomValue, gestureSequence);
    QTRY_VERIFY(valuesApproximatelyEqual(imageDocument->zoomPercent(), expectedSecondZoomPercent));
    const ImageViewportCoordinateResult spreadPointAfterSecondZoom
        = mapViewportPointToDisplayedSpread(*viewport, viewport->mapFromScene(sceneCentroid));
    QVERIFY(spreadPointAfterSecondZoom.isValid());
    QVERIFY(
        pointsApproximatelyEqual(spreadPointAfterSecondZoom.point(), anchoredSpreadPoint.point()));

    sendNativeGesture(
        fixture.window, touchpad.get(), Qt::EndNativeGesture, sceneCentroid, 0.0, gestureSequence);
}

void TestMainWindowToolBar::touchscreenPinchZoomsAndTranslates()
{
    auto touchscreen = std::unique_ptr<QPointingDevice>(QTest::createTouchDevice());
    QVERIFY(touchscreen != nullptr);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("touchscreen-pinch.png"));
    QVERIFY(writeTestPng(imagePath));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(imagePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QTRY_COMPARE(imageDocument->status(), KiriImageDocument::Status::Ready);

    KiriImageViewportSurface* viewportSurface = nullptr;
    QTRY_VERIFY2((viewportSurface = readyImageViewportSurface(fixture.window)) != nullptr,
        qPrintable(imageViewportStateReport(fixture.window)));
    ImageViewport* viewport = viewportSurface->viewport();
    QVERIFY(viewport != nullptr);

    QVERIFY(imageDocument->requestFitMode(KiriImageDocument::ZoomMode::FitWidth));
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);
    const double initialZoomPercent = imageDocument->zoomPercent();
    QVERIFY(initialZoomPercent > 0.0);

    const QPoint sceneCenter
        = viewport->mapToScene(QPointF(viewport->width() / 2.0, viewport->height() / 2.0))
              .toPoint();
    QPoint firstPoint = sceneCenter - QPoint(80, 0);
    QPoint secondPoint = sceneCenter + QPoint(80, 0);
    QTest::QTouchEventSequence sequence
        = QTest::touchEvent(fixture.window, touchscreen.get(), false);
    QVERIFY(sequence.press(0, firstPoint, fixture.window)
            .press(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    const int activationExpansion
        = std::max(48, QGuiApplication::styleHints()->startDragDistance() * 4 + 8);
    firstPoint -= QPoint(activationExpansion, 0);
    secondPoint += QPoint(activationExpansion, 0);
    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);
    QVERIFY(valuesApproximatelyEqual(imageDocument->zoomPercent(), initialZoomPercent));

    const QPoint previousSceneCentroid = (firstPoint + secondPoint) / 2;
    const ImageViewportCoordinateResult anchoredSpreadPoint = mapViewportPointToDisplayedSpread(
        *viewport, viewport->mapFromScene(QPointF(previousSceneCentroid)));
    QVERIFY(anchoredSpreadPoint.isValid());
    const int gestureBaselineSeparation = secondPoint.x() - firstPoint.x();

    const QPoint translation(30, 24);
    constexpr int expansion = 20;
    firstPoint += translation - QPoint(expansion, 0);
    secondPoint += translation + QPoint(expansion, 0);
    const QPoint currentSceneCentroid = (firstPoint + secondPoint) / 2;
    const int currentSeparation = secondPoint.x() - firstPoint.x();
    const double expectedScaleFactor
        = static_cast<double>(currentSeparation) / static_cast<double>(gestureBaselineSeparation);
    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
    QTRY_VERIFY(valuesApproximatelyEqual(
        imageDocument->zoomPercent(), initialZoomPercent * expectedScaleFactor));
    const ImageViewportCoordinateResult spreadPointAfterUpdate = mapViewportPointToDisplayedSpread(
        *viewport, viewport->mapFromScene(QPointF(currentSceneCentroid)));
    QVERIFY(spreadPointAfterUpdate.isValid());
    QVERIFY2(
        pointsApproximatelyEqual(spreadPointAfterUpdate.point(), anchoredSpreadPoint.point(), 0.01),
        qPrintable(QStringLiteral("expected anchored spread point %1,%2; got %3,%4")
                .arg(anchoredSpreadPoint.point().x())
                .arg(anchoredSpreadPoint.point().y())
                .arg(spreadPointAfterUpdate.point().x())
                .arg(spreadPointAfterUpdate.point().y())));

    const QPoint secondTranslation(-18, 12);
    constexpr int secondExpansion = 16;
    firstPoint += secondTranslation - QPoint(secondExpansion, 0);
    secondPoint += secondTranslation + QPoint(secondExpansion, 0);
    const QPoint secondSceneCentroid = (firstPoint + secondPoint) / 2;
    const int secondSeparation = secondPoint.x() - firstPoint.x();
    const double expectedTotalScaleFactor
        = static_cast<double>(secondSeparation) / static_cast<double>(gestureBaselineSeparation);
    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    QTRY_VERIFY(valuesApproximatelyEqual(
        imageDocument->zoomPercent(), initialZoomPercent * expectedTotalScaleFactor));
    const ImageViewportCoordinateResult spreadPointAfterSecondUpdate
        = mapViewportPointToDisplayedSpread(
            *viewport, viewport->mapFromScene(QPointF(secondSceneCentroid)));
    QVERIFY(spreadPointAfterSecondUpdate.isValid());
    QVERIFY2(pointsApproximatelyEqual(
                 spreadPointAfterSecondUpdate.point(), anchoredSpreadPoint.point(), 0.01),
        qPrintable(QStringLiteral("expected anchored spread point %1,%2; got %3,%4")
                .arg(anchoredSpreadPoint.point().x())
                .arg(anchoredSpreadPoint.point().y())
                .arg(spreadPointAfterSecondUpdate.point().x())
                .arg(spreadPointAfterSecondUpdate.point().y())));

    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .release(0, firstPoint, fixture.window)
            .release(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);
}

void TestMainWindowToolBar::viewportPinchRuntimeBoundaryBehavior()
{
    MainWindowFixture emptyFixture = createMainWindowFixture();
    QVERIFY2(emptyFixture.isValid(), qPrintable(emptyFixture.errorString));
    KiriDocumentSession* emptySession = findDocumentSession(emptyFixture.window);
    QVERIFY(emptySession != nullptr);
    KiriImageDocument* emptyDocument = emptySession->imageDocument();
    QVERIFY(emptyDocument != nullptr);
    QCOMPARE(emptyDocument->status(), KiriImageDocument::Status::Null);
    const double emptyZoomPercent = emptyDocument->zoomPercent();
    const KiriImageDocument::ZoomMode emptyZoomMode = emptyDocument->zoomMode();
    QVERIFY(
        !emptyDocument->requestViewportPinchUpdate(1.25, QPointF(10.0, 10.0), QPointF(10.0, 10.0)));
    QCOMPARE(emptyDocument->zoomPercent(), emptyZoomPercent);
    QCOMPARE(emptyDocument->zoomMode(), emptyZoomMode);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("runtime-pinch.png"));
    QVERIFY(writeTestPng(imagePath));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(imagePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));
    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QTRY_COMPARE(imageDocument->status(), KiriImageDocument::Status::Ready);

    KiriImageViewportSurface* viewportSurface = nullptr;
    QTRY_VERIFY2((viewportSurface = readyImageViewportSurface(fixture.window)) != nullptr,
        qPrintable(imageViewportStateReport(fixture.window)));
    ImageViewport* viewport = viewportSurface->viewport();
    QVERIFY(viewport != nullptr);

    QVERIFY(imageDocument->requestFitMode(KiriImageDocument::ZoomMode::FitWidth));
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);
    QTRY_VERIFY(imageDocument->viewportVerticallyPannable());
    const double fitZoomPercent = imageDocument->zoomPercent();
    const QPointF previousCentroid(viewport->width() / 2.0, viewport->height() / 2.0);
    const QPointF currentCentroid = previousCentroid - QPointF(0.0, 40.0);
    const QPointF contentPositionBeforeTranslation = viewport->state().display().contentPosition();
    QVERIFY(imageDocument->requestViewportPinchUpdate(1.0, previousCentroid, currentCentroid));
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);
    QVERIFY(valuesApproximatelyEqual(imageDocument->zoomPercent(), fitZoomPercent));
    QVERIFY(viewport->state().display().contentPosition() != contentPositionBeforeTranslation);

    for (int margin = 0; margin < 4; ++margin) {
        QVERIFY(imageDocument->requestManualZoomPercent(imageDocument->minimumManualZoomPercent()));
        QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
        QTRY_VERIFY(valuesApproximatelyEqual(
            imageDocument->zoomPercent(), imageDocument->minimumManualZoomPercent()));
        const QRectF contentRect = viewport->state().display().contentRect();
        QVERIFY(!contentRect.isEmpty());
        QPointF outsideCentroid = contentRect.center();
        switch (margin) {
        case 0:
            outsideCentroid.setX(contentRect.left() - 50.0);
            break;
        case 1:
            outsideCentroid.setX(contentRect.right() + 50.0);
            break;
        case 2:
            outsideCentroid.setY(contentRect.top() - 50.0);
            break;
        case 3:
            outsideCentroid.setY(contentRect.bottom() + 50.0);
            break;
        }

        const QPointF nearestViewportAnchor
            = imageDocument->nearestImageViewportPoint(outsideCentroid);
        QVERIFY(std::isfinite(nearestViewportAnchor.x()));
        QVERIFY(std::isfinite(nearestViewportAnchor.y()));
        const ImageViewportCoordinateResult nearestAnchoredSpreadPoint
            = mapViewportPointToDisplayedSpread(*viewport, nearestViewportAnchor);
        QVERIFY(nearestAnchoredSpreadPoint.isValid());
        const double zoomBeforeOutsideAnchoredUpdate = imageDocument->zoomPercent();
        QVERIFY(imageDocument->requestViewportPinchUpdate(1.1, outsideCentroid, outsideCentroid));
        QVERIFY(valuesApproximatelyEqual(
            imageDocument->zoomPercent(), zoomBeforeOutsideAnchoredUpdate * 1.1));
    }

    QVERIFY(imageDocument->requestFitMode(KiriImageDocument::ZoomMode::FitWidth));
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);
    const QPointF batchedPreviousCentroid(viewport->width() / 2.0, viewport->height() / 2.0);
    const QPointF batchedCurrentCentroid = batchedPreviousCentroid - QPointF(20.0, 20.0);
    const ImageViewportCoordinateResult batchedAnchoredSpreadPoint
        = mapViewportPointToDisplayedSpread(*viewport, batchedPreviousCentroid);
    QVERIFY(batchedAnchoredSpreadPoint.isValid());
    const double zoomBeforeBatchedUpdate = imageDocument->zoomPercent();
    int zoomPublicationCount = 0;
    KiriImageDocument::ZoomMode observedZoomMode = imageDocument->zoomMode();
    double observedZoomPercent = 0.0;
    ImageViewportCoordinateResult observedSpreadPoint;
    const QMetaObject::Connection zoomPublicationConnection
        = connect(imageDocument, &KiriImageDocument::zoomPercentChanged, this, [&]() {
              ++zoomPublicationCount;
              observedZoomMode = imageDocument->zoomMode();
              observedZoomPercent = imageDocument->zoomPercent();
              observedSpreadPoint
                  = mapViewportPointToDisplayedSpread(*viewport, batchedCurrentCentroid);
          });
    QVERIFY(imageDocument->requestViewportPinchUpdate(
        1.1, batchedPreviousCentroid, batchedCurrentCentroid));
    disconnect(zoomPublicationConnection);
    QCOMPARE(zoomPublicationCount, 1);
    QCOMPARE(observedZoomMode, KiriImageDocument::ZoomMode::Manual);
    QVERIFY(valuesApproximatelyEqual(observedZoomPercent, zoomBeforeBatchedUpdate * 1.1));
    QVERIFY(observedSpreadPoint.isValid());
    QVERIFY(pointsApproximatelyEqual(
        observedSpreadPoint.point(), batchedAnchoredSpreadPoint.point(), 0.01));

    const double maximumZoomPercent = imageDocument->maximumManualZoomPercent();
    const double minimumZoomPercent = imageDocument->minimumManualZoomPercent();
    QVERIFY(maximumZoomPercent >= minimumZoomPercent);
    QVERIFY(imageDocument->requestViewportPinchUpdate(
        std::numeric_limits<double>::max(), previousCentroid, previousCentroid));
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
    QVERIFY(valuesApproximatelyEqual(imageDocument->zoomPercent(), maximumZoomPercent));

    QVERIFY(imageDocument->requestViewportPinchUpdate(
        std::numeric_limits<double>::min(), previousCentroid, previousCentroid));
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);
    QVERIFY(valuesApproximatelyEqual(imageDocument->zoomPercent(), minimumZoomPercent));

    const auto verifyRejected = [&](double scaleFactor, QPointF previous, QPointF current) {
        const KiriImageDocument::ZoomMode zoomModeBefore = imageDocument->zoomMode();
        const double zoomPercentBefore = imageDocument->zoomPercent();
        const QPointF contentPositionBefore = viewport->state().display().contentPosition();

        QVERIFY(!imageDocument->requestViewportPinchUpdate(scaleFactor, previous, current));
        QCOMPARE(imageDocument->zoomMode(), zoomModeBefore);
        QCOMPARE(imageDocument->zoomPercent(), zoomPercentBefore);
        QCOMPARE(viewport->state().display().contentPosition(), contentPositionBefore);
    };

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    verifyRejected(nan, previousCentroid, previousCentroid);
    verifyRejected(infinity, previousCentroid, previousCentroid);
    verifyRejected(0.0, previousCentroid, previousCentroid);
    verifyRejected(-1.0, previousCentroid, previousCentroid);
    verifyRejected(1.25, QPointF(nan, previousCentroid.y()), previousCentroid);
    verifyRejected(1.25, previousCentroid, QPointF(previousCentroid.x(), infinity));
}

void TestMainWindowToolBar::staleTouchscreenPinchCannotMutateReplacementImage()
{
    auto touchscreen = std::unique_ptr<QPointingDevice>(QTest::createTouchDevice());
    QVERIFY(touchscreen != nullptr);

    QString archivePath;
    QString errorString;
    std::unique_ptr<QTemporaryDir> archiveDirectory
        = createComicBookArchive(&archivePath, &errorString);
    QVERIFY2(archiveDirectory != nullptr, qPrintable(errorString));

    MainWindowFixture fixture = createMainWindowFixture(QUrl::fromLocalFile(archivePath));
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));
    KiriDocumentSession* documentSession = findDocumentSession(fixture.window);
    QVERIFY(documentSession != nullptr);
    KiriImageDocument* imageDocument = documentSession->imageDocument();
    QVERIFY(imageDocument != nullptr);
    QTRY_COMPARE(imageDocument->status(), KiriImageDocument::Status::Ready);

    KiriImageViewportSurface* viewportSurface = nullptr;
    QTRY_VERIFY2((viewportSurface = readyImageViewportSurface(fixture.window)) != nullptr,
        qPrintable(imageViewportStateReport(fixture.window)));
    ImageViewport* viewport = viewportSurface->viewport();
    QVERIFY(viewport != nullptr);
    QVERIFY(imageDocument->requestFitMode(KiriImageDocument::ZoomMode::FitWidth));
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);

    const QPoint sceneCenter
        = viewport->mapToScene(QPointF(viewport->width() / 2.0, viewport->height() / 2.0))
              .toPoint();
    QPoint firstPoint = sceneCenter - QPoint(80, 0);
    QPoint secondPoint = sceneCenter + QPoint(80, 0);
    QTest::QTouchEventSequence sequence
        = QTest::touchEvent(fixture.window, touchscreen.get(), false);
    QVERIFY(sequence.press(0, firstPoint, fixture.window)
            .press(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    const QPointF contentPositionBeforeActivation = viewport->state().display().contentPosition();
    const int activationDistance
        = std::max(48, QGuiApplication::styleHints()->startDragDistance() * 4 + 8);
    const QPoint activationTranslation(0, activationDistance);
    firstPoint += activationTranslation;
    secondPoint += activationTranslation;
    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::FitWidth);
    QCOMPARE(viewport->state().display().contentPosition(), contentPositionBeforeActivation);
    firstPoint -= QPoint(20, 0);
    secondPoint += QPoint(20, 0);
    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);
    QTRY_COMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);

    const QUrl firstPageUrl = imageDocument->displayedUrl();
    imageDocument->openNextPage();
    QTRY_VERIFY2(imageDocument->status() == KiriImageDocument::Status::Ready
            && imageDocument->displayedUrl() != firstPageUrl,
        qPrintable(imageViewportStateReport(fixture.window)));
    QTRY_VERIFY2((viewportSurface = readyImageViewportSurface(fixture.window)) != nullptr,
        qPrintable(imageViewportStateReport(fixture.window)));
    viewport = viewportSurface->viewport();
    QVERIFY(viewport != nullptr);

    const QUrl replacementUrl = imageDocument->displayedUrl();
    const KiriImageDocument::ZoomMode replacementZoomMode = imageDocument->zoomMode();
    const double replacementZoomPercent = imageDocument->zoomPercent();
    const QPointF replacementContentPosition = viewport->state().display().contentPosition();

    firstPoint -= QPoint(30, 10);
    secondPoint += QPoint(50, 10);
    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    QCOMPARE(imageDocument->displayedUrl(), replacementUrl);
    QCOMPARE(imageDocument->zoomMode(), replacementZoomMode);
    QCOMPARE(imageDocument->zoomPercent(), replacementZoomPercent);
    QCOMPARE(viewport->state().display().contentPosition(), replacementContentPosition);

    firstPoint -= QPoint(15, 5);
    secondPoint += QPoint(25, 5);
    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    QCOMPARE(imageDocument->displayedUrl(), replacementUrl);
    QCOMPARE(imageDocument->zoomMode(), replacementZoomMode);
    QCOMPARE(imageDocument->zoomPercent(), replacementZoomPercent);
    QCOMPARE(viewport->state().display().contentPosition(), replacementContentPosition);

    QVERIFY(sequence.stationary(0)
            .stationary(1)
            .release(0, firstPoint, fixture.window)
            .release(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    firstPoint = sceneCenter - QPoint(80, 0);
    secondPoint = sceneCenter + QPoint(80, 0);
    QTest::QTouchEventSequence freshSequence
        = QTest::touchEvent(fixture.window, touchscreen.get(), false);
    QVERIFY(freshSequence.press(0, firstPoint, fixture.window)
            .press(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    firstPoint -= QPoint(activationDistance, 0);
    secondPoint += QPoint(activationDistance, 0);
    QVERIFY(freshSequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    const double replacementZoomPercentBeforeFreshUpdate = imageDocument->zoomPercent();
    firstPoint -= QPoint(20, 0);
    secondPoint += QPoint(20, 0);
    QVERIFY(freshSequence.stationary(0)
            .stationary(1)
            .move(0, firstPoint, fixture.window)
            .move(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);

    QVERIFY(!valuesApproximatelyEqual(
        imageDocument->zoomPercent(), replacementZoomPercentBeforeFreshUpdate));
    QCOMPARE(imageDocument->zoomMode(), KiriImageDocument::ZoomMode::Manual);

    QVERIFY(freshSequence.stationary(0)
            .stationary(1)
            .release(0, firstPoint, fixture.window)
            .release(1, secondPoint, fixture.window)
            .commit());
    flushTouchEvents(*fixture.window);
}

void TestMainWindowToolBar::fullscreenChromeProjectionRendersImmediately()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    resizeWindow(fixture, QSize(1200, 800));

    QQuickItem* toolbar = findQuickItem(fixture.window, QStringLiteral("mainImageToolBar"));
    QVERIFY(toolbar != nullptr);

    fixture.window->setVisibility(QWindow::FullScreen);
    QTRY_COMPARE(fixture.window->visibility(), QWindow::FullScreen);
    QTRY_VERIFY(fixture.windowShell->fullscreen());
    QTRY_VERIFY(fixture.windowShell->pointerHidden());
    QTRY_VERIFY(fixture.windowShell->toolbarRevealed());
    QTRY_VERIFY(toolbar->isVisible());

    moveMouse(fixture.window, QPoint(fixture.window->width() / 2, fixture.window->height() / 2));
    QTRY_VERIFY(!fixture.windowShell->pointerHidden());
}

void TestMainWindowToolBar::fullscreenReusesSingleToolbarAndHidesApplicationMenuButton()
{
    MainWindowFixture fixture = createMainWindowFixture();
    QVERIFY2(fixture.isValid(), qPrintable(fixture.errorString));
    QTRY_COMPARE(controlToolBars(fixture.window).size(), 1);
    QQuickItem* windowedToolbar = controlToolBars(fixture.window).constFirst();

    fixture.window->setVisibility(QWindow::FullScreen);
    QTRY_COMPARE(fixture.window->visibility(), QWindow::FullScreen);

    QTRY_COMPARE(controlToolBars(fixture.window).size(), 1);
    QCOMPARE(controlToolBars(fixture.window).constFirst(), windowedToolbar);
    QVERIFY(windowedToolbar->isVisible());

    const QList<QQuickItem*> visibleApplicationMenuButtons
        = visibleItemsByObjectName(fixture.window, QStringLiteral("toolbarApplicationMenuButton"));
    QVERIFY(visibleApplicationMenuButtons.isEmpty());
}

QTEST_MAIN(TestMainWindowToolBar)

#include "tst_mainwindowtoolbar.moc"
