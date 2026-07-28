// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiridocumentsession.h"
#include "facade/kiriimagedocument.h"
#include "facade/kiriimageviewportsurface.h"
#include "kiridocumentsession_test_support.h"
#include "qml_component_test_support.h"

#include <ImageViewport/imageviewport.h>

#include <QBuffer>
#include <QImageWriter>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
QByteArray encodedPngData(const QSize& size)
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(QColor(Qt::green));

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, QByteArrayLiteral("png"));
    if (!writer.write(image)) {
        return {};
    }
    return data;
}

kiriview::ThumbnailCacheLookupProvider thumbnailLookupProvider(bool ready)
{
    return [ready](QObject*, kiriview::ThumbnailCacheLookupRequest request,
               kiriview::ThumbnailCacheLookupCallback callback) {
        kiriview::ThumbnailCacheLookupResult result;
        result.requestedBucket = request.requestedBucket;
        if (ready) {
            result.status = kiriview::ThumbnailCacheLookupStatus::Ready;
            result.image = QImage(QSize(400, 300), QImage::Format_RGBA8888);
            result.image.fill(QColor(Qt::blue));
            result.sourceBucket = request.requestedBucket;
            result.sourceCachePath = QStringLiteral("/cache/preview.png");
        }
        if (callback) {
            callback(std::move(result));
        }
        return kiriview::ImageIoJob();
    };
}

struct PendingOpenedCollectionCandidateLoad
{
    QObject* object = nullptr;
    kiriview::OpenedCollectionScopeLocation scope;
    kiriview::ImageDocumentPageCandidatesCallback callback;
    kiriview::ErrorCallback errorCallback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualOpenedCollectionCandidateProvider
{
public:
    kiriview::ImageDocumentPageCandidateProvider provider()
    {
        kiriview::ImageDocumentPageCandidateProvider provider;
        provider.openedCollectionCandidates
            = [this](QObject* receiver, kiriview::OpenedCollectionScopeLocation scope,
                  kiriview::ImageDocumentPageCandidatesCallback callback,
                  kiriview::ErrorCallback errorCallback) {
                  auto load = std::make_shared<PendingOpenedCollectionCandidateLoad>();
                  load->scope = std::move(scope);
                  load->callback = std::move(callback);
                  load->errorCallback = std::move(errorCallback);
                  kiriview::ImageIoJob job
                      = kiriview::TestSupport::Detail::startManualIoJob(receiver, load);
                  m_load = std::move(load);
                  return job;
              };
        return provider;
    }

    bool hasPendingLoad() const { return m_load != nullptr && m_load->object != nullptr; }

    void resolve(std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        kiriview::TestSupport::Detail::finishManualIoJob(m_load,
            [candidates = std::move(candidates)](
                PendingOpenedCollectionCandidateLoad& load) mutable {
                if (load.callback) {
                    load.callback(std::move(candidates));
                }
            });
    }

private:
    std::shared_ptr<PendingOpenedCollectionCandidateLoad> m_load;
};

std::unique_ptr<KiriDocumentSession> createViewportSession(
    kiriview::DirectMediaNavigationCandidateProvider directMediaNavigationCandidateProvider,
    kiriview::ImageDocumentPageCandidateProvider imageDocumentPageCandidateProvider,
    kiriview::TestSupport::ManualImageDataLoader& dataLoader,
    kiriview::TestSupport::ManualImageWorkerScheduler& workerScheduler,
    kiriview::ThumbnailCacheLookupProvider thumbnailPreviewLookupProvider,
    kiriview::TestSupport::ManualImageDataLoader* directMediaPredecodeDataLoader = nullptr,
    kiriview::TestSupport::ManualTimerScheduler* imagePredecodeTimerScheduler = nullptr,
    QSize decodedImageSize = QSize(800, 600))
{
    kiriview::KiriDocumentSessionDependencies dependencies;
    dependencies.sessionRuntime.directMediaNavigationCandidateProvider
        = std::move(directMediaNavigationCandidateProvider);
    dependencies.imageDocument.candidateProvider = std::move(imageDocumentPageCandidateProvider);
    dependencies.imageDocument.imageDecode
        = kiriview::TestSupport::imageDecodeDependenciesFor(dataLoader,
            kiriview::TestSupport::staticImageDataDecoder(
                kiriview::TestSupport::testImage(decodedImageSize)));
    dependencies.imageDocument.imageDecode.workerScheduler = workerScheduler.scheduler();
    dependencies.imageDocument.imageDecode.thumbnailPreviewLookupProvider
        = std::move(thumbnailPreviewLookupProvider);
    dependencies.imageDocument.ordinaryDirectMediaPredecodeEnabled = false;
    if (imagePredecodeTimerScheduler != nullptr) {
        dependencies.imageDocument.predecodeTimerScheduler
            = imagePredecodeTimerScheduler->scheduler();
    }
    if (directMediaPredecodeDataLoader != nullptr) {
        dependencies.sessionRuntime.directMediaPredecodeDependencies.imageDecode
            = kiriview::TestSupport::imageDecodeDependenciesFor(*directMediaPredecodeDataLoader,
                kiriview::TestSupport::staticImageDataDecoder(
                    kiriview::TestSupport::testImage(decodedImageSize)));
    }

    auto session = std::make_unique<KiriDocumentSession>(std::move(dependencies));
    attachTestViewport(*session);
    return session;
}

KiriImageViewportSurface* viewportSurface(KiriDocumentSession& session)
{
    return session.findChild<KiriImageViewportSurface*>();
}

void renderViewportFrame(KiriImageViewportSurface& surface)
{
    QQuickWindow* window = surface.window();
    QVERIFY(window != nullptr);
    window->update();
    window->grabWindow();
    QCoreApplication::processEvents();
}

template <typename Predicate>
bool driveViewportUntil(KiriImageViewportSurface& surface, Predicate predicate)
{
    constexpr int maximumAttempts = 100;
    for (int attempt = 0; attempt < maximumAttempts; ++attempt) {
        if (predicate()) {
            return true;
        }
        renderViewportFrame(surface);
        QTest::qWait(5);
    }
    return predicate();
}

void runOutstandingWorkerSchedules(
    kiriview::TestSupport::ManualImageWorkerScheduler& workerScheduler, std::size_t& nextSchedule)
{
    while (nextSchedule < workerScheduler.scheduleCount()) {
        if (workerScheduler.isActive(nextSchedule)) {
            workerScheduler.runWork(nextSchedule);
            workerScheduler.finish(nextSchedule);
        }
        ++nextSchedule;
    }
}
}

class TestImageViewportComponentBoundary : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void applicationSurfaceStartsWithEmptySnapshot();
    void applicationQmlModuleCreatesSurfaceWithEmptySnapshot();
    void attachedApplicationSurfaceDisplaysDocumentSource();
    void unresolvedCollectionReplacementRevokesPendingTarget();
    void reentrantCollectionResolutionKeepsNewestTarget();
    void detachedCollectionResolutionResumesAfterReattach();
    void replacementThumbnailDoesNotDisplaceAuthoritativeDisplay();
    void reattachedPendingTargetUsesPreviewWithoutRetainedFallback();
    void reattachedTargetRefreshesAuthoritativePredecode();
    void twoPageShapeChangeSuppressesProvisionalSpread();
    void sameUrlSecondaryReplacementRejectsSupersededProjection();
};

void TestImageViewportComponentBoundary::applicationSurfaceStartsWithEmptySnapshot()
{
    KiriImageViewportSurface surface;

    QCOMPARE(surface.viewport()->state().request().status(), ImageViewportRequestStatus::NoRequest);
    QVERIFY(!surface.viewport()->state().request().acceptedRoleSet().primary());
    QVERIFY(!surface.viewport()->state().request().acceptedRoleSet().secondary());
}

void TestImageViewportComponentBoundary::applicationQmlModuleCreatesSurfaceWithEmptySnapshot()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(KIRIVIEW_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import org.hnjae.kiriview

KiriImageViewportSurface {
    width: 32
    height: 24
}
)",
        QUrl(QStringLiteral("memory:imageviewportcomponentboundary.qml")));

    QVERIFY(waitForQmlComponentReady(component));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> surface(component.create());
    QVERIFY2(surface != nullptr, qPrintable(component.errorString()));
    auto* viewportSurface = qobject_cast<KiriImageViewportSurface*>(surface.get());
    QVERIFY(viewportSurface != nullptr);
    QCOMPARE(viewportSurface->viewport()->state().request().status(),
        ImageViewportRequestStatus::NoRequest);
}

void TestImageViewportComponentBoundary::attachedApplicationSurfaceDisplaysDocumentSource()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("surface.png"));
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(QColor(20, 40, 60, 255));
    QVERIFY(image.save(path));

    KiriDocumentSession session;
    QQuickWindow window;
    window.resize(160, 120);
    KiriImageViewportSurface surface;
    surface.setParentItem(window.contentItem());
    surface.setSize(QSizeF(160, 120));
    surface.setDocument(session.imageDocument());
    window.show();
    session.setSourceUrl(QUrl::fromLocalFile(path));

    for (int attempt = 0; attempt < 200
        && surface.viewport()->state().request().status() != ImageViewportRequestStatus::Ready;
        ++attempt) {
        window.update();
        window.grabWindow();
        QTest::qWait(10);
    }
    QCOMPARE(session.imageDocument()->status(), KiriImageDocument::Status::Ready);
    QCOMPARE(surface.viewport()->state().request().status(), ImageViewportRequestStatus::Ready);
    QCOMPARE(surface.viewport()->state().display().displayedRoleSet().primary(), true);
    QCOMPARE(session.imageDocument()->displayedUrl(), QUrl::fromLocalFile(path));
}

void TestImageViewportComponentBoundary::unresolvedCollectionReplacementRevokesPendingTarget()
{
    const QByteArray imageData = encodedPngData(QSize(800, 600));
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    ManualOpenedCollectionCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl initialUrl(QStringLiteral("file:///images/initial.png"));
    const QUrl pendingUrl(QStringLiteral("file:///images/pending.png"));
    const QUrl collectionUrl(QStringLiteral("file:///books/replacement.cbz"));
    directMediaNavigationProvider.setMedia(QUrl(QStringLiteral("file:///images/")),
        { directMediaNavigationCandidate(initialUrl), directMediaNavigationCandidate(pendingUrl) });
    std::unique_ptr<KiriDocumentSession> session = createViewportSession(
        directMediaNavigationProvider.provider(), pageCandidateProvider.provider(), dataLoader,
        workerScheduler, thumbnailLookupProvider(false));
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(initialUrl);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(initialUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(initialUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == initialUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));
    const auto initialDisplayGeneration
        = surface->viewport()->state().display().displayedPresentationTargetGeneration();
    QVERIFY(initialDisplayGeneration.isValid());

    session->setSourceUrl(pendingUrl);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(pendingUrl));
    const kiriview::ImageDataCallback latePendingData = dataLoader.backLoad().dataCallback;
    QVERIFY(latePendingData);
    const auto pendingAcceptedGeneration
        = surface->viewport()->state().request().acceptedPresentationTargetGeneration();
    QVERIFY(pendingAcceptedGeneration.isValid());
    QVERIFY(pendingAcceptedGeneration != initialDisplayGeneration);
    QCOMPARE(surface->viewport()->state().display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(surface->viewport()->state().display().displayedPresentationTargetGeneration(),
        initialDisplayGeneration);

    session->setSourceUrl(collectionUrl);
    QTRY_VERIFY(pageCandidateProvider.hasPendingLoad());
    QCOMPARE(session->imageDocument()->sourceUrl(), collectionUrl);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);

    const auto unresolvedAcceptedGeneration
        = surface->viewport()->state().request().acceptedPresentationTargetGeneration();
    QVERIFY(unresolvedAcceptedGeneration.isValid());
    QVERIFY(unresolvedAcceptedGeneration != pendingAcceptedGeneration);
    QCOMPARE(surface->viewport()->state().display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(surface->viewport()->state().display().displayedPresentationTargetGeneration(),
        initialDisplayGeneration);
    QVERIFY(!surface->viewport()->state().display().belongsToAcceptedPresentationTarget());

    latePendingData(imageData);
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    renderViewportFrame(*surface);
    renderViewportFrame(*surface);

    QCOMPARE(surface->viewport()->state().request().acceptedPresentationTargetGeneration(),
        unresolvedAcceptedGeneration);
    QCOMPARE(surface->viewport()->state().display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(surface->viewport()->state().display().displayedPresentationTargetGeneration(),
        initialDisplayGeneration);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);

    const std::optional<kiriview::OpenedCollectionScopeLocation> collectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(collectionUrl, {}));
    QVERIFY(collectionScope.has_value());
    const QUrl collectionPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("01.png"));
    pageCandidateProvider.resolve(
        { kiriview::TestSupport::imageDocumentPageCandidate(collectionPageUrl) });
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(collectionPageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(collectionPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);

    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == collectionPageUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));
    QCOMPARE(session->imageDocument()->sourceUrl(), collectionUrl);
    QCOMPARE(session->imageDocument()->displayedUrl(), collectionPageUrl);
    QCOMPARE(surface->viewport()->state().display().displayedPresentationTargetGeneration(),
        surface->viewport()->state().request().acceptedPresentationTargetGeneration());
}

void TestImageViewportComponentBoundary::reentrantCollectionResolutionKeepsNewestTarget()
{
    const QByteArray imageData = encodedPngData(QSize(800, 600));
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    ManualOpenedCollectionCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl collectionUrl(QStringLiteral("file:///books/reentrant.cbz"));
    const QUrl replacementUrl(QStringLiteral("file:///images/reentrant-replacement.png"));
    directMediaNavigationProvider.setMedia(QUrl(QStringLiteral("file:///images/")),
        { directMediaNavigationCandidate(replacementUrl) });
    std::unique_ptr<KiriDocumentSession> session = createViewportSession(
        directMediaNavigationProvider.provider(), pageCandidateProvider.provider(), dataLoader,
        workerScheduler, thumbnailLookupProvider(false));
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(collectionUrl);
    QTRY_VERIFY(pageCandidateProvider.hasPendingLoad());
    const auto collectionGeneration
        = surface->viewport()->state().request().acceptedPresentationTargetGeneration();
    QVERIFY(collectionGeneration.isValid());

    const std::optional<kiriview::OpenedCollectionScopeLocation> collectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(collectionUrl, {}));
    QVERIFY(collectionScope.has_value());
    const QUrl collectionPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("01.png"));
    bool replacementStarted = false;
    const QMetaObject::Connection reentrantConnection = QObject::connect(session->imageDocument(),
        &KiriImageDocument::displayedUrlChanged, session->imageDocument(), [&]() {
            if (replacementStarted) {
                return;
            }
            replacementStarted = true;
            session->setSourceUrl(replacementUrl);
        });

    pageCandidateProvider.resolve(
        { kiriview::TestSupport::imageDocumentPageCandidate(collectionPageUrl) });
    QObject::disconnect(reentrantConnection);

    QVERIFY(replacementStarted);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(replacementUrl));
    const auto replacementGeneration
        = surface->viewport()->state().request().acceptedPresentationTargetGeneration();
    QVERIFY(replacementGeneration.isValid());
    QVERIFY(replacementGeneration != collectionGeneration);
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(replacementUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);

    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->sourceUrl() == replacementUrl
            && session->imageDocument()->displayedUrl() == replacementUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));
    QCOMPARE(surface->viewport()->state().request().acceptedPresentationTargetGeneration(),
        replacementGeneration);
}

void TestImageViewportComponentBoundary::detachedCollectionResolutionResumesAfterReattach()
{
    const QByteArray imageData = encodedPngData(QSize(800, 600));
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    ManualOpenedCollectionCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl collectionUrl(QStringLiteral("file:///books/detached-resolution.cbz"));
    std::unique_ptr<KiriDocumentSession> session = createViewportSession(
        directMediaNavigationProvider.provider(), pageCandidateProvider.provider(), dataLoader,
        workerScheduler, thumbnailLookupProvider(false));
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(collectionUrl);
    QTRY_VERIFY(pageCandidateProvider.hasPendingLoad());
    const std::optional<kiriview::OpenedCollectionScopeLocation> collectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(collectionUrl, {}));
    QVERIFY(collectionScope.has_value());
    const QUrl collectionPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("01.png"));

    surface->setDocument(nullptr);
    QCOMPARE(
        surface->viewport()->state().request().status(), ImageViewportRequestStatus::NoRequest);
    pageCandidateProvider.resolve(
        { kiriview::TestSupport::imageDocumentPageCandidate(collectionPageUrl) });
    QCOMPARE(session->imageDocument()->sourceUrl(), collectionUrl);
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);

    surface->setDocument(session->imageDocument());
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(collectionPageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(collectionPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);

    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->sourceUrl() == collectionUrl
            && session->imageDocument()->displayedUrl() == collectionPageUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));
}

void TestImageViewportComponentBoundary::replacementThumbnailDoesNotDisplaceAuthoritativeDisplay()
{
    const QByteArray imageData = encodedPngData(QSize(800, 600));
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl initialUrl(QStringLiteral("file:///images/authoritative.png"));
    const QUrl replacementUrl(QStringLiteral("file:///images/replacement.png"));
    directMediaNavigationProvider.setMedia(QUrl(QStringLiteral("file:///images/")),
        { directMediaNavigationCandidate(initialUrl),
            directMediaNavigationCandidate(replacementUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createViewportSession(directMediaNavigationProvider.provider(), {}, dataLoader,
            workerScheduler, thumbnailLookupProvider(true));
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(initialUrl);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(initialUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(initialUrl, imageData));
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return surface->viewport()->state().request().status()
            == ImageViewportRequestStatus::Loading
            && surface->viewport()->state().display().status() == ImageViewportDisplayStatus::Ready
            && surface->viewport()->state().primary().display().quality()
            == ImageViewportPayloadQuality::Preview;
    }));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == initialUrl
            && surface->viewport()->state().primary().display().quality()
            == ImageViewportPayloadQuality::Exact;
    }));
    QVERIFY(session->imageDocument()->completeAuthoritativeDisplayAvailable());
    QVERIFY(!session->activeImageReplacementFallbackAvailable());
    const auto initialDisplayGeneration
        = surface->viewport()->state().display().displayedPresentationTargetGeneration();
    QVERIFY(initialDisplayGeneration.isValid());

    session->setSourceUrl(replacementUrl);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(replacementUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(replacementUrl, imageData));
    renderViewportFrame(*surface);
    renderViewportFrame(*surface);

    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
    QVERIFY(session->imageDocument()->completeAuthoritativeDisplayAvailable());
    QVERIFY(session->activeImageReplacementFallbackAvailable());
    QCOMPARE(surface->viewport()->state().request().status(), ImageViewportRequestStatus::Loading);
    QCOMPARE(surface->viewport()->state().display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(surface->viewport()->state().display().displayedPresentationTargetGeneration(),
        initialDisplayGeneration);
    QVERIFY(!surface->viewport()->state().display().belongsToAcceptedPresentationTarget());
    QCOMPARE(surface->viewport()->state().primary().display().quality(),
        ImageViewportPayloadQuality::Exact);

    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == replacementUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));
    QCOMPARE(surface->viewport()->state().primary().display().quality(),
        ImageViewportPayloadQuality::Exact);
    QVERIFY(session->imageDocument()->completeAuthoritativeDisplayAvailable());
    QVERIFY(!session->activeImageReplacementFallbackAvailable());
    QCOMPARE(surface->viewport()->state().display().displayedPresentationTargetGeneration(),
        surface->viewport()->state().request().acceptedPresentationTargetGeneration());
}

void TestImageViewportComponentBoundary::reattachedPendingTargetUsesPreviewWithoutRetainedFallback()
{
    const QByteArray imageData = encodedPngData(QSize(800, 600));
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageDataLoader predecodeDataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl initialUrl(QStringLiteral("file:///images/reattach-initial.png"));
    const QUrl replacementUrl(QStringLiteral("file:///images/reattach-pending.png"));
    directMediaNavigationProvider.setMedia(QUrl(QStringLiteral("file:///images/")),
        { directMediaNavigationCandidate(initialUrl),
            directMediaNavigationCandidate(replacementUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createViewportSession(directMediaNavigationProvider.provider(), {}, dataLoader,
            workerScheduler, thumbnailLookupProvider(true), &predecodeDataLoader);
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(initialUrl);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(initialUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(initialUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));

    session->setSourceUrl(replacementUrl);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(replacementUrl));
    QCOMPARE(surface->viewport()->state().display().status(), ImageViewportDisplayStatus::Retained);
    const std::size_t foregroundLoadCountBeforeReattach = dataLoader.loadCount();

    surface->setDocument(nullptr);
    QCOMPARE(
        surface->viewport()->state().request().status(), ImageViewportRequestStatus::NoRequest);
    surface->setDocument(session->imageDocument());
    QTRY_VERIFY(dataLoader.loadCount() > foregroundLoadCountBeforeReattach);
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(replacementUrl, imageData));

    QVERIFY(driveViewportUntil(*surface, [&]() {
        const ImageViewportStateSnapshot snapshot = surface->viewport()->state();
        return snapshot.request().status() == ImageViewportRequestStatus::Loading
            && snapshot.display().status() == ImageViewportDisplayStatus::Ready
            && snapshot.display().belongsToAcceptedPresentationTarget()
            && snapshot.primary().display().quality() == ImageViewportPayloadQuality::Preview;
    }));
    QCOMPARE(session->imageDocument()->status(), KiriImageDocument::Status::Loading);
    QCOMPARE(session->imageDocument()->displayedUrl(), QUrl());
}

void TestImageViewportComponentBoundary::reattachedTargetRefreshesAuthoritativePredecode()
{
    const QByteArray imageData = encodedPngData(QSize(800, 600));
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    const QUrl imageUrl(QStringLiteral("file:///images/reattach-predecoded.png"));
    directMediaNavigationProvider.setMedia(
        QUrl(QStringLiteral("file:///images/")), { directMediaNavigationCandidate(imageUrl) });
    std::unique_ptr<KiriDocumentSession> session
        = createViewportSession(directMediaNavigationProvider.provider(), {}, dataLoader,
            workerScheduler, thumbnailLookupProvider(false));
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(imageUrl);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(imageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(imageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));

    const std::size_t loadCountBeforeReattach = dataLoader.loadCount();

    surface->setDocument(nullptr);
    surface->setDocument(session->imageDocument());
    QVERIFY(driveViewportUntil(*surface, [&]() {
        const ImageViewportStateSnapshot snapshot = surface->viewport()->state();
        return snapshot.request().status() == ImageViewportRequestStatus::Ready
            && snapshot.display().belongsToAcceptedPresentationTarget()
            && snapshot.primary().display().quality() == ImageViewportPayloadQuality::Exact;
    }));
    QCOMPARE(dataLoader.loadCount(), loadCountBeforeReattach);
}

void TestImageViewportComponentBoundary::twoPageShapeChangeSuppressesProvisionalSpread()
{
    const QSize portraitSize(600, 800);
    const QByteArray imageData = encodedPngData(portraitSize);
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    ManualOpenedCollectionCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::TestSupport::ManualTimerScheduler predecodeTimerScheduler;
    const QUrl collectionUrl(QStringLiteral("file:///books/provisional-spread.cbz"));
    std::unique_ptr<KiriDocumentSession> session
        = createViewportSession(directMediaNavigationProvider.provider(),
            pageCandidateProvider.provider(), dataLoader, workerScheduler,
            thumbnailLookupProvider(true), nullptr, &predecodeTimerScheduler, portraitSize);
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(collectionUrl);
    QTRY_VERIFY(pageCandidateProvider.hasPendingLoad());
    const std::optional<kiriview::OpenedCollectionScopeLocation> collectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(collectionUrl, {}));
    QVERIFY(collectionScope.has_value());
    const QUrl firstPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("02.png"));
    const QUrl thirdPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("03.png"));
    pageCandidateProvider.resolve({ kiriview::TestSupport::imageDocumentPageCandidate(firstPageUrl),
        kiriview::TestSupport::imageDocumentPageCandidate(secondPageUrl),
        kiriview::TestSupport::imageDocumentPageCandidate(thirdPageUrl) });

    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(firstPageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(firstPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == firstPageUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));
    QTRY_VERIFY(session->imageDocument()->twoPageModeAvailable());
    QCOMPARE(session->imageDocument()->currentPageNumber(), 1);
    QCOMPARE(session->imageDocument()->pageCount(), 3);

    session->imageDocument()->openNextPage();
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(secondPageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(secondPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == secondPageUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));

    session->imageDocument()->requestToggleTwoPageMode();
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(thirdPageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(thirdPageUrl, imageData));
    renderViewportFrame(*surface);
    renderViewportFrame(*surface);

    QCOMPARE(surface->viewport()->state().request().status(), ImageViewportRequestStatus::Loading);
    QCOMPARE(surface->viewport()->state().display().status(), ImageViewportDisplayStatus::Empty);
    QVERIFY(!session->imageDocument()->completeAuthoritativeDisplayAvailable());
    QVERIFY(!session->activeImageReplacementFallbackAvailable());
    QVERIFY(!surface->viewport()->state().display().displayedRoleSet().primary());
    QVERIFY(!surface->viewport()->state().display().displayedRoleSet().secondary());
}

void TestImageViewportComponentBoundary::sameUrlSecondaryReplacementRejectsSupersededProjection()
{
    const QSize portraitSize(600, 800);
    const QByteArray imageData = encodedPngData(portraitSize);
    QVERIFY(!imageData.isEmpty());

    FakeDirectMediaNavigationCandidateProvider directMediaNavigationProvider;
    ManualOpenedCollectionCandidateProvider pageCandidateProvider;
    kiriview::TestSupport::ManualImageDataLoader dataLoader;
    kiriview::TestSupport::ManualImageWorkerScheduler workerScheduler;
    kiriview::TestSupport::ManualTimerScheduler predecodeTimerScheduler;
    const QUrl collectionUrl(QStringLiteral("file:///books/reentrant-secondary.cbz"));
    std::unique_ptr<KiriDocumentSession> session
        = createViewportSession(directMediaNavigationProvider.provider(),
            pageCandidateProvider.provider(), dataLoader, workerScheduler,
            thumbnailLookupProvider(false), nullptr, &predecodeTimerScheduler, portraitSize);
    KiriImageViewportSurface* surface = viewportSurface(*session);
    QVERIFY(surface != nullptr);
    std::size_t nextWorkerSchedule = 0;

    session->setSourceUrl(collectionUrl);
    QTRY_VERIFY(pageCandidateProvider.hasPendingLoad());
    const std::optional<kiriview::OpenedCollectionScopeLocation> collectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(collectionUrl, {}));
    QVERIFY(collectionScope.has_value());
    const QUrl firstPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("02.png"));
    const QUrl thirdPageUrl = kiriview::TestSupport::archivePageUrl(
        collectionScope->rootUrl(), QStringLiteral("03.png"));
    pageCandidateProvider.resolve({ kiriview::TestSupport::imageDocumentPageCandidate(firstPageUrl),
        kiriview::TestSupport::imageDocumentPageCandidate(secondPageUrl),
        kiriview::TestSupport::imageDocumentPageCandidate(thirdPageUrl) });

    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(firstPageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(firstPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == firstPageUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));

    session->imageDocument()->openNextPage();
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(secondPageUrl));
    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(secondPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->status() == KiriImageDocument::Status::Ready
            && session->imageDocument()->displayedUrl() == secondPageUrl
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready;
    }));

    session->imageDocument()->requestToggleTwoPageMode();
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(thirdPageUrl));
    bool replacementStarted = false;
    const QMetaObject::Connection reentrantConnection = QObject::connect(session->imageDocument(),
        &KiriImageDocument::statusChanged, session->imageDocument(), [&]() {
            const ImageViewportStateSnapshot snapshot = surface->viewport()->state();
            if (replacementStarted
                || snapshot.request().status() != ImageViewportRequestStatus::Ready
                || !snapshot.request().acceptedRoleSet().secondary()) {
                return;
            }

            replacementStarted = true;
            session->imageDocument()->requestToggleTwoPageMode();
            session->imageDocument()->requestToggleTwoPageMode();
        });

    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(thirdPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() { return replacementStarted; }));
    QObject::disconnect(reentrantConnection);

    QVERIFY(session->imageDocument()->twoPageModeEnabled());
    QVERIFY(!session->imageDocument()->secondaryPageVisible());
    QCOMPARE(session->imageDocument()->currentLastPageNumber(), 2);
    QTRY_VERIFY(dataLoader.hasActiveLoadForUrl(thirdPageUrl));

    QVERIFY(dataLoader.finishNewestActiveLoadForUrl(thirdPageUrl, imageData));
    runOutstandingWorkerSchedules(workerScheduler, nextWorkerSchedule);
    QVERIFY(driveViewportUntil(*surface, [&]() {
        return session->imageDocument()->secondaryPageVisible()
            && surface->viewport()->state().request().status() == ImageViewportRequestStatus::Ready
            && surface->viewport()->state().display().displayedRoleSet().secondary();
    }));
    QCOMPARE(session->imageDocument()->currentLastPageNumber(), 3);
}

QTEST_MAIN(TestImageViewportComponentBoundary)

#include "tst_imageviewportcomponentboundary.moc"
