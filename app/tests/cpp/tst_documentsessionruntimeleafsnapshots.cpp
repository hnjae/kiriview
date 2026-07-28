// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionruntime.h"

#include "image_test_support.h"

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QTest>
#include <QUrl>
#include <functional>
#include <memory>
#include <utility>

class TestDocumentSessionRuntimeLeafSnapshots : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directImageRoutePublishesImageLeafSnapshot();
    void imageSnapshotChangeRefreshesPublicProjection();
    void openedCollectionVideoLeafRoutesToSourceDeviceWithoutDirectMediaNavigation();
    void videoDetachCanDestroyRuntimeWithoutContinuingRoute();
    void openedCollectionLeaveSupersessionPreservesNewVideoRoute();
    void openedCollectionDeviceLoadSupersessionPreservesNewRoute();
    void openedCollectionDeviceLoadRejectsChangedImageSnapshot();
    void imageSnapshotReadCanDestroyRuntimeWithoutContinuingChange();
    void imageSnapshotReadRouteSupersessionPreservesReplacementRoute();
    void nestedImageSnapshotReadPreservesNewerSnapshot();
    void videoClearPredecodeCancellationPreservesReentrantRoute();
    void queuedSnapshotChangeAfterRuntimeDestructionIsIgnored();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

class SnapshotChangeEmitter : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void imageSnapshotChanged();
    void videoSnapshotChanged();
};

kiriview::DocumentSessionSnapshotConnector imageSnapshotChangedConnector(
    SnapshotChangeEmitter& emitter, Qt::ConnectionType connectionType = Qt::AutoConnection)
{
    return [&emitter, connectionType](
               QObject* context, kiriview::DocumentSessionSnapshotChangeHandler handler) {
        std::vector<QMetaObject::Connection> connections;
        connections.push_back(QObject::connect(
            &emitter, &SnapshotChangeEmitter::imageSnapshotChanged, context,
            [handler = std::move(handler)]() {
                if (handler) {
                    handler();
                }
            },
            connectionType));
        return connections;
    };
}

kiriview::DocumentSessionSnapshotConnector videoSnapshotChangedConnector(
    SnapshotChangeEmitter& emitter)
{
    return [&emitter](QObject* context, kiriview::DocumentSessionSnapshotChangeHandler handler) {
        std::vector<QMetaObject::Connection> connections;
        connections.push_back(
            QObject::connect(&emitter, &SnapshotChangeEmitter::videoSnapshotChanged, context,
                [handler = std::move(handler)]() {
                    if (handler) {
                        handler();
                    }
                }));
        return connections;
    };
}
}

void TestDocumentSessionRuntimeLeafSnapshots::directImageRoutePublishesImageLeafSnapshot()
{
    QObject owner;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/image.png"));
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.setSource
        = [&imageSnapshot](const kiriview::ResolvedNavigationSource& source) {
              const QUrl url = source.requestedUrl();
              imageSnapshot.sourceUrl = url;
              imageSnapshot.displayedUrl = url;
              imageSnapshot.windowTitleFileName = url.fileName();
              imageSnapshot.primaryImageSize = QSize(320, 200);
              imageSnapshot.ready = !url.isEmpty();
              imageSnapshot.completeAuthoritativeDisplayAvailable = !url.isEmpty();
              imageSnapshot.ordinaryDirectMediaScopeActive = !url.isEmpty();
              imageSnapshot.zoomPercentKnown = true;
              imageSnapshot.zoomPercent = 100.0;
          };
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.pageNavigation.openPreviousPage = []() { };
    imageCommands.pageNavigation.openNextPage = []() { };
    imageCommands.pageNavigation.openImageAtPage = [](int) { };
    imageCommands.deletion.deleteDisplayedFile = [](kiriview::FileDeletionMode) { };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.setSource
        = [&videoSnapshot](const kiriview::ResolvedNavigationSource& source) {
              videoSnapshot.sourceUrl = source.requestedUrl();
          };
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot.sourceUrl = QUrl(); };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.playback.stop = []() { };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    kiriview::DocumentSessionRuntime runtime(&owner, std::move(imagePort), std::move(imageCommands),
        std::move(videoPort), std::move(videoCommands));

    runtime.setSourceUrl(imageUrl);

    QCOMPARE(runtime.documentKind(), kiriview::DocumentSessionKind::Image);
    QCOMPARE(runtime.sourceUrl(), imageUrl);
    QVERIFY(runtime.activeImageReady());
    QVERIFY(runtime.windowTitleSubject().startsWith(QStringLiteral("image.png")));
    QVERIFY(runtime.windowTitleSubject().contains(QStringLiteral("320")));
    QVERIFY(runtime.windowTitleSubject().contains(QStringLiteral("200")));
    QVERIFY(runtime.activeZoomPercentKnown());
    QCOMPARE(runtime.activeZoomPercent(), 100.0);
}

void TestDocumentSessionRuntimeLeafSnapshots::imageSnapshotChangeRefreshesPublicProjection()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/image.png"));
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.setSource
        = [&imageSnapshot](const kiriview::ResolvedNavigationSource& source) {
              const QUrl url = source.requestedUrl();
              imageSnapshot.sourceUrl = url;
              imageSnapshot.displayedUrl = url;
              imageSnapshot.windowTitleFileName = url.fileName();
              imageSnapshot.primaryImageSize = QSize(320, 200);
              imageSnapshot.ready = !url.isEmpty();
              imageSnapshot.ordinaryDirectMediaScopeActive = !url.isEmpty();
          };
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.pageNavigation.openPreviousPage = []() { };
    imageCommands.pageNavigation.openNextPage = []() { };
    imageCommands.pageNavigation.openImageAtPage = [](int) { };
    imageCommands.deletion.deleteDisplayedFile = [](kiriview::FileDeletionMode) { };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.setSource
        = [&videoSnapshot](const kiriview::ResolvedNavigationSource& source) {
              videoSnapshot.sourceUrl = source.requestedUrl();
          };
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot.sourceUrl = QUrl(); };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.playback.stop = []() { };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    kiriview::DocumentSessionRuntime runtime(&owner, std::move(imagePort), std::move(imageCommands),
        std::move(videoPort), std::move(videoCommands));
    runtime.setSourceUrl(imageUrl);

    imageSnapshot.primaryImageSize = QSize(640, 480);
    Q_EMIT emitter.imageSnapshotChanged();

    QVERIFY(runtime.windowTitleSubject().contains(QStringLiteral("640")));
    QVERIFY(runtime.windowTitleSubject().contains(QStringLiteral("480")));
}

void TestDocumentSessionRuntimeLeafSnapshots::
    openedCollectionVideoLeafRoutesToSourceDeviceWithoutDirectMediaNavigation()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.zip"));
    const QUrl archiveRootUrl(QStringLiteral("zip:///books/book.zip!/"));
    const QUrl firstImageUrl(QStringLiteral("zip:///books/book.zip!/01.png"));
    const QUrl videoUrl(QStringLiteral("zip:///books/book.zip!/02.mp4"));
    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            archiveUrl, archiveRootUrl, kiriview::OpenedCollectionScopeKind::GeneralArchive);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    imageSnapshot.snapshotSourceKind = kiriview::ImageDocumentPageKind::Video;
    imageSnapshot.sourceUrl = videoUrl;
    imageSnapshot.displayedUrl = videoUrl;
    imageSnapshot.displayedOpenedCollectionScope = archiveCollection;
    imageSnapshot.windowTitleFileName = QStringLiteral("book.zip");
    imageSnapshot.ready = true;
    imageSnapshot.openedCollectionScopeActive = true;
    imageSnapshot.twoPageModeEnabled = true;
    imageSnapshot.twoPageModeAvailable = true;
    imageSnapshot.rightToLeftReadingEnabled = true;
    imageSnapshot.rightToLeftReadingAvailable = true;
    imageSnapshot.fitModeSelected = true;
    imageSnapshot.pageNavigationSnapshot.state = kiriview::PageNavigationState(
        {
            kiriview::ImageDocumentPageTarget {
                firstImageUrl,
                kiriview::ImageDocumentPageKind::Image,
            },
            kiriview::ImageDocumentPageTarget {
                videoUrl,
                kiriview::ImageDocumentPageKind::Video,
            },
        },
        1);
    imageSnapshot.activeNavigationSnapshot = kiriview::ImageDocumentPageActiveNavigationSnapshot {
        true,
        true,
        false,
        false,
        true,
        2,
        2,
    };
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;

    int playbackDeviceLoadCount = 0;
    QUrl loadedPlaybackScopeRootUrl;
    QUrl loadedPlaybackVideoUrl;
    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.setSource
        = [&imageSnapshot](const kiriview::ResolvedNavigationSource& source) {
              const QUrl url = source.requestedUrl();
              imageSnapshot.sourceUrl = url;
              imageSnapshot.displayedUrl = url;
          };
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.source.loadOpenedCollectionVideoPlaybackDevice =
        [&playbackDeviceLoadCount, &loadedPlaybackScopeRootUrl, &loadedPlaybackVideoUrl](
            const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& url) {
            ++playbackDeviceLoadCount;
            loadedPlaybackScopeRootUrl = openedCollectionScope.rootUrl();
            loadedPlaybackVideoUrl = url;
            auto device = std::make_unique<QBuffer>();
            device->setData(QByteArrayLiteral("collection-video-bytes"));
            device->open(QIODevice::ReadOnly);
            return kiriview::MediaEntrySourceVideoPlaybackDevice {
                {},
                std::move(device),
            };
        };
    imageCommands.pageNavigation.openPreviousPage = []() { };
    imageCommands.pageNavigation.openNextPage = []() { };
    imageCommands.pageNavigation.openImageAtPage = [](int) { };
    imageCommands.deletion.deleteDisplayedFile = [](kiriview::FileDeletionMode) { };

    int setSourceUrlCount = 0;
    int setSourceDeviceCount = 0;
    QUrl videoSourceUrl;
    QByteArray videoDeviceBytes;
    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.setSource
        = [&videoSnapshot, &setSourceUrlCount](const kiriview::ResolvedNavigationSource& source) {
              ++setSourceUrlCount;
              videoSnapshot.sourceUrl = source.requestedUrl();
          };
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot.sourceUrl = QUrl(); };
    videoCommands.source.setSourceDevice
        = [&videoSnapshot, &setSourceDeviceCount, &videoSourceUrl, &videoDeviceBytes](
              const QUrl& url, kiriview::VideoPlaybackSourceDevice sourceDevice) {
              ++setSourceDeviceCount;
              videoSourceUrl = url;
              videoSnapshot.sourceUrl = url;
              videoSnapshot.windowTitleFileName = url.fileName();
              videoSnapshot.ready = true;
              videoSnapshot.hasVideo = true;
              videoSnapshot.zoomPercentKnown = true;
              videoSnapshot.zoomPercent = 73;
              if (sourceDevice.device != nullptr) {
                  videoDeviceBytes = sourceDevice.device->readAll();
              }
          };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.playback.stop = []() { };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    kiriview::DocumentSessionRuntime runtime(&owner, std::move(imagePort), std::move(imageCommands),
        std::move(videoPort), std::move(videoCommands));

    Q_EMIT emitter.imageSnapshotChanged();

    QCOMPARE(playbackDeviceLoadCount, 1);
    QCOMPARE(loadedPlaybackScopeRootUrl, archiveCollection.rootUrl());
    QCOMPARE(loadedPlaybackVideoUrl, videoUrl);
    QCOMPARE(setSourceUrlCount, 0);
    QCOMPARE(setSourceDeviceCount, 1);
    QCOMPARE(videoSourceUrl, videoUrl);
    QCOMPARE(videoDeviceBytes, QByteArrayLiteral("collection-video-bytes"));
    QCOMPARE(runtime.documentKind(), kiriview::DocumentSessionKind::Video);
    QCOMPARE(runtime.sourceUrl(), videoUrl);
    QVERIFY(runtime.activeVideoReady());
    QVERIFY(!runtime.activeImageReady());
    QVERIFY(runtime.activeImageOpenedCollectionScopeActive());
    QCOMPARE(runtime.activeNavigationBoundaryScope(),
        kiriview::ActiveNavigationBoundaryScope::ImageDocumentPage);
    QVERIFY(!runtime.directMediaNavigationBoundaryActive());
    QCOMPARE(runtime.activeNavigationCurrentNumber(), 2);
    QCOMPARE(runtime.activeNavigationCount(), 2);
    QCOMPARE(runtime.windowTitleSubject(), QStringLiteral("book.zip – 2/2"));
    QVERIFY(runtime.activeZoomPercentAvailable());
    QVERIFY(runtime.activeZoomPercentKnown());
    QCOMPARE(runtime.activeZoomPercent(), 73.0);
    QVERIFY(!runtime.activeZoomEditable());
    const kiriview::DocumentSessionActionAvailabilityFacts& actionFacts
        = runtime.actionAvailabilityFacts();
    QVERIFY(!actionFacts.imageReady);
    QVERIFY(actionFacts.twoPageModeActive);
    QVERIFY(actionFacts.twoPageModeAvailable);
    QVERIFY(actionFacts.rightToLeftReadingActive);
    QVERIFY(actionFacts.rightToLeftReadingAvailable);
    QVERIFY(actionFacts.fitModeSelected);
}

void TestDocumentSessionRuntimeLeafSnapshots::videoDetachCanDestroyRuntimeWithoutContinuingRoute()
{
    QObject owner;
    QObject surfaceOwner;
    QObject videoOutput;
    const QUrl videoUrl = localUrl(QStringLiteral("/media/movie.mp4"));
    const QUrl imageUrl = localUrl(QStringLiteral("/media/image.png"));
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;
    QObject* attachedVideoOutput = nullptr;
    int imageSetSourceCount = 0;
    int videoStopCount = 0;
    int videoClearSourceCount = 0;
    bool destroyOnDetach = false;
    std::unique_ptr<kiriview::DocumentSessionRuntime> runtime;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.source.setSource
        = [&imageSnapshot, &imageSetSourceCount](const kiriview::ResolvedNavigationSource& source) {
              ++imageSetSourceCount;
              imageSnapshot.sourceUrl = source.requestedUrl();
              imageSnapshot.displayedUrl = source.requestedUrl();
          };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.setSource
        = [&videoSnapshot](const kiriview::ResolvedNavigationSource& source) {
              videoSnapshot.sourceUrl = source.requestedUrl();
          };
    videoCommands.source.clearSource = [&]() {
        ++videoClearSourceCount;
        videoSnapshot = {};
    };
    videoCommands.playback.stop = [&]() { ++videoStopCount; };
    videoCommands.output.videoOutput = [&]() { return attachedVideoOutput; };
    videoCommands.output.setVideoOutputAttachment
        = [&](QObject* output, const QRectF&, const QRectF&) {
              attachedVideoOutput = output;
              if (destroyOnDetach && output == nullptr) {
                  runtime.reset();
              }
          };

    runtime = std::make_unique<kiriview::DocumentSessionRuntime>(&owner, std::move(imagePort),
        std::move(imageCommands), std::move(videoPort), std::move(videoCommands));
    runtime->setSourceUrl(videoUrl);
    QVERIFY(runtime->reportVideoOutputSurfaceClaim(runtime->nextVideoOutputSurfaceClaimToken(),
        runtime->publicProjectionRevision(), &surfaceOwner, &videoOutput, true, {}, {}));
    QCOMPARE(attachedVideoOutput, &videoOutput);

    destroyOnDetach = true;
    kiriview::DocumentSessionRuntime* executingRuntime = runtime.get();
    executingRuntime->setSourceUrl(imageUrl);

    QVERIFY(runtime == nullptr);
    QCOMPARE(imageSetSourceCount, 0);
    QCOMPARE(videoStopCount, 0);
    QCOMPARE(videoClearSourceCount, 0);
}

void TestDocumentSessionRuntimeLeafSnapshots::
    openedCollectionLeaveSupersessionPreservesNewVideoRoute()
{
    QObject owner;
    QObject firstSurfaceOwner;
    QObject firstVideoOutput;
    QObject replacementSurfaceOwner;
    QObject replacementVideoOutput;
    SnapshotChangeEmitter emitter;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.zip"));
    const QUrl archiveRootUrl(QStringLiteral("zip:///books/book.zip!/"));
    const QUrl collectionVideoUrl(QStringLiteral("zip:///books/book.zip!/movie.mp4"));
    const QUrl replacementVideoUrl = localUrl(QStringLiteral("/media/replacement.mp4"));
    const QUrl collectionImageUrl(QStringLiteral("zip:///books/book.zip!/page.png"));
    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            archiveUrl, archiveRootUrl, kiriview::OpenedCollectionScopeKind::GeneralArchive);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    imageSnapshot.snapshotSourceKind = kiriview::ImageDocumentPageKind::Video;
    imageSnapshot.sourceUrl = collectionVideoUrl;
    imageSnapshot.displayedUrl = collectionVideoUrl;
    imageSnapshot.displayedOpenedCollectionScope = archiveCollection;
    imageSnapshot.ready = true;
    imageSnapshot.openedCollectionScopeActive = true;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;
    QObject* attachedVideoOutput = nullptr;
    bool replacementRouteSubmitted = false;
    bool replacementClaimAccepted = false;
    std::unique_ptr<kiriview::DocumentSessionRuntime> runtime;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.source.loadOpenedCollectionVideoPlaybackDevice
        = [](const kiriview::OpenedCollectionScopeLocation&, const QUrl&) {
              auto device = std::make_unique<QBuffer>();
              device->setData(QByteArrayLiteral("collection-video-bytes"));
              device->open(QIODevice::ReadOnly);
              return kiriview::MediaEntrySourceVideoPlaybackDevice {
                  {},
                  std::move(device),
              };
          };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.setSource
        = [&videoSnapshot](const kiriview::ResolvedNavigationSource& source) {
              videoSnapshot.sourceUrl = source.requestedUrl();
          };
    videoCommands.source.setSourceDevice
        = [&videoSnapshot](const QUrl& url, kiriview::VideoPlaybackSourceDevice) {
              videoSnapshot.sourceUrl = url;
          };
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot = {}; };
    videoCommands.playback.stop = []() { };
    videoCommands.output.videoOutput = [&]() { return attachedVideoOutput; };
    videoCommands.output.setVideoOutputAttachment
        = [&](QObject* output, const QRectF&, const QRectF&) {
              attachedVideoOutput = output;
              if (output != nullptr || replacementRouteSubmitted) {
                  return;
              }
              replacementRouteSubmitted = true;
              runtime->setSourceUrl(replacementVideoUrl);
              replacementClaimAccepted = runtime->reportVideoOutputSurfaceClaim(
                  runtime->nextVideoOutputSurfaceClaimToken(), runtime->publicProjectionRevision(),
                  &replacementSurfaceOwner, &replacementVideoOutput, true, {}, {});
          };

    runtime = std::make_unique<kiriview::DocumentSessionRuntime>(&owner, std::move(imagePort),
        std::move(imageCommands), std::move(videoPort), std::move(videoCommands));
    Q_EMIT emitter.imageSnapshotChanged();
    QCOMPARE(runtime->documentKind(), kiriview::DocumentSessionKind::Video);
    QVERIFY(runtime->reportVideoOutputSurfaceClaim(runtime->nextVideoOutputSurfaceClaimToken(),
        runtime->publicProjectionRevision(), &firstSurfaceOwner, &firstVideoOutput, true, {}, {}));

    imageSnapshot.snapshotSourceKind = kiriview::ImageDocumentPageKind::Image;
    imageSnapshot.sourceUrl = collectionImageUrl;
    imageSnapshot.displayedUrl = collectionImageUrl;
    imageSnapshot.ready = true;
    Q_EMIT emitter.imageSnapshotChanged();

    QVERIFY(replacementRouteSubmitted);
    QVERIFY(replacementClaimAccepted);
    QCOMPARE(runtime->documentKind(), kiriview::DocumentSessionKind::Video);
    QCOMPARE(runtime->sourceUrl(), replacementVideoUrl);
    QCOMPARE(attachedVideoOutput, &replacementVideoOutput);
}

void TestDocumentSessionRuntimeLeafSnapshots::
    openedCollectionDeviceLoadSupersessionPreservesNewRoute()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.zip"));
    const QUrl archiveRootUrl(QStringLiteral("zip:///books/book.zip!/"));
    const QUrl collectionVideoUrl(QStringLiteral("zip:///books/book.zip!/movie.mp4"));
    const QUrl replacementVideoUrl = localUrl(QStringLiteral("/media/replacement.mp4"));
    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            archiveUrl, archiveRootUrl, kiriview::OpenedCollectionScopeKind::GeneralArchive);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    imageSnapshot.snapshotSourceKind = kiriview::ImageDocumentPageKind::Video;
    imageSnapshot.sourceUrl = collectionVideoUrl;
    imageSnapshot.displayedUrl = collectionVideoUrl;
    imageSnapshot.displayedOpenedCollectionScope = archiveCollection;
    imageSnapshot.ready = true;
    imageSnapshot.openedCollectionScopeActive = true;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;
    int playbackDeviceLoadCount = 0;
    int staleSourceDeviceCount = 0;
    int replacementSourceCount = 0;
    std::unique_ptr<kiriview::DocumentSessionRuntime> runtime;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.source.loadOpenedCollectionVideoPlaybackDevice = [&](const auto&, const QUrl&) {
        ++playbackDeviceLoadCount;
        runtime->setSourceUrl(replacementVideoUrl);
        auto device = std::make_unique<QBuffer>();
        device->setData(QByteArrayLiteral("stale-collection-video-bytes"));
        device->open(QIODevice::ReadOnly);
        return kiriview::MediaEntrySourceVideoPlaybackDevice {
            {},
            std::move(device),
        };
    };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.setSource = [&](const kiriview::ResolvedNavigationSource& source) {
        ++replacementSourceCount;
        videoSnapshot.sourceUrl = source.requestedUrl();
    };
    videoCommands.source.setSourceDevice
        = [&](const QUrl& url, kiriview::VideoPlaybackSourceDevice) {
              ++staleSourceDeviceCount;
              videoSnapshot.sourceUrl = url;
          };
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot = {}; };
    videoCommands.playback.stop = []() { };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    runtime = std::make_unique<kiriview::DocumentSessionRuntime>(&owner, std::move(imagePort),
        std::move(imageCommands), std::move(videoPort), std::move(videoCommands));

    Q_EMIT emitter.imageSnapshotChanged();

    QCOMPARE(playbackDeviceLoadCount, 1);
    QCOMPARE(replacementSourceCount, 1);
    QCOMPARE(staleSourceDeviceCount, 0);
    QCOMPARE(runtime->documentKind(), kiriview::DocumentSessionKind::Video);
    QCOMPARE(runtime->sourceUrl(), replacementVideoUrl);
}

void TestDocumentSessionRuntimeLeafSnapshots::
    openedCollectionDeviceLoadRejectsChangedImageSnapshot()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.zip"));
    const QUrl archiveRootUrl(QStringLiteral("zip:///books/book.zip!/"));
    const QUrl collectionVideoUrl(QStringLiteral("zip:///books/book.zip!/movie.mp4"));
    const QUrl replacementImageUrl(QStringLiteral("zip:///books/book.zip!/page.png"));
    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            archiveUrl, archiveRootUrl, kiriview::OpenedCollectionScopeKind::GeneralArchive);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    imageSnapshot.snapshotSourceKind = kiriview::ImageDocumentPageKind::Video;
    imageSnapshot.sourceUrl = collectionVideoUrl;
    imageSnapshot.displayedUrl = collectionVideoUrl;
    imageSnapshot.displayedOpenedCollectionScope = archiveCollection;
    imageSnapshot.ready = true;
    imageSnapshot.openedCollectionScopeActive = true;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;
    int playbackDeviceLoadCount = 0;
    int staleSourceDeviceCount = 0;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.source.loadOpenedCollectionVideoPlaybackDevice = [&](const auto&, const QUrl&) {
        ++playbackDeviceLoadCount;
        imageSnapshot.snapshotSourceKind = kiriview::ImageDocumentPageKind::Image;
        imageSnapshot.sourceUrl = replacementImageUrl;
        imageSnapshot.displayedUrl = replacementImageUrl;
        Q_EMIT emitter.imageSnapshotChanged();
        auto device = std::make_unique<QBuffer>();
        device->setData(QByteArrayLiteral("stale-collection-video-bytes"));
        device->open(QIODevice::ReadOnly);
        return kiriview::MediaEntrySourceVideoPlaybackDevice {
            {},
            std::move(device),
        };
    };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.setSourceDevice
        = [&](const QUrl&, kiriview::VideoPlaybackSourceDevice) { ++staleSourceDeviceCount; };
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot = {}; };
    videoCommands.playback.stop = []() { };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    kiriview::DocumentSessionRuntime runtime(&owner, std::move(imagePort), std::move(imageCommands),
        std::move(videoPort), std::move(videoCommands));

    Q_EMIT emitter.imageSnapshotChanged();

    QCOMPARE(playbackDeviceLoadCount, 1);
    QCOMPARE(staleSourceDeviceCount, 0);
}

void TestDocumentSessionRuntimeLeafSnapshots::
    imageSnapshotReadCanDestroyRuntimeWithoutContinuingChange()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.zip"));
    const QUrl archiveRootUrl(QStringLiteral("zip:///books/book.zip!/"));
    const QUrl videoUrl(QStringLiteral("zip:///books/book.zip!/movie.mp4"));
    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            archiveUrl, archiveRootUrl, kiriview::OpenedCollectionScopeKind::GeneralArchive);
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;
    bool destroyOnImageSnapshotRead = false;
    int playbackDeviceLoadCount = 0;
    std::unique_ptr<kiriview::DocumentSessionRuntime> runtime;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&]() {
        const kiriview::DocumentSessionImageDocumentSnapshot snapshot = imageSnapshot;
        if (destroyOnImageSnapshotRead) {
            destroyOnImageSnapshotRead = false;
            runtime.reset();
        }
        return snapshot;
    };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.loadOpenedCollectionVideoPlaybackDevice
        = [&](const kiriview::OpenedCollectionScopeLocation&, const QUrl&) {
              ++playbackDeviceLoadCount;
              auto device = std::make_unique<QBuffer>();
              device->setData(QByteArrayLiteral("unreachable-video-bytes"));
              device->open(QIODevice::ReadOnly);
              return kiriview::MediaEntrySourceVideoPlaybackDevice {
                  {},
                  std::move(device),
              };
          };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };

    runtime = std::make_unique<kiriview::DocumentSessionRuntime>(&owner, std::move(imagePort),
        std::move(imageCommands), std::move(videoPort),
        kiriview::DocumentSessionVideoDocumentCommandPort {});

    imageSnapshot.snapshotSourceKind = kiriview::ImageDocumentPageKind::Video;
    imageSnapshot.sourceUrl = videoUrl;
    imageSnapshot.displayedUrl = videoUrl;
    imageSnapshot.displayedOpenedCollectionScope = archiveCollection;
    imageSnapshot.ready = true;
    imageSnapshot.openedCollectionScopeActive = true;
    destroyOnImageSnapshotRead = true;
    Q_EMIT emitter.imageSnapshotChanged();

    QVERIFY(runtime == nullptr);
    QCOMPARE(playbackDeviceLoadCount, 0);
}

void TestDocumentSessionRuntimeLeafSnapshots::
    imageSnapshotReadRouteSupersessionPreservesReplacementRoute()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    const QUrl staleImageUrl = localUrl(QStringLiteral("/media/stale.png"));
    const QUrl replacementImageUrl = localUrl(QStringLiteral("/media/replacement.png"));
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;
    bool routeReplacementOnImageSnapshotRead = false;
    std::unique_ptr<kiriview::DocumentSessionRuntime> runtime;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&]() {
        const kiriview::DocumentSessionImageDocumentSnapshot snapshot = imageSnapshot;
        if (routeReplacementOnImageSnapshotRead) {
            routeReplacementOnImageSnapshotRead = false;
            runtime->setSourceUrl(replacementImageUrl);
        }
        return snapshot;
    };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.source.setSource
        = [&imageSnapshot](const kiriview::ResolvedNavigationSource& source) {
              imageSnapshot = {};
              imageSnapshot.sourceUrl = source.requestedUrl();
              imageSnapshot.displayedUrl = source.requestedUrl();
              imageSnapshot.windowTitleFileName = source.requestedUrl().fileName();
              imageSnapshot.ready = true;
              imageSnapshot.completeAuthoritativeDisplayAvailable = true;
              imageSnapshot.ordinaryDirectMediaScopeActive = true;
          };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot = {}; };
    videoCommands.source.setSource
        = [&videoSnapshot](const kiriview::ResolvedNavigationSource& source) {
              videoSnapshot.sourceUrl = source.requestedUrl();
              videoSnapshot.ready = true;
              videoSnapshot.hasVideo = true;
          };
    videoCommands.playback.stop = []() { };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    runtime = std::make_unique<kiriview::DocumentSessionRuntime>(&owner, std::move(imagePort),
        std::move(imageCommands), std::move(videoPort), std::move(videoCommands));

    imageSnapshot.sourceUrl = staleImageUrl;
    imageSnapshot.displayedUrl = staleImageUrl;
    imageSnapshot.windowTitleFileName = staleImageUrl.fileName();
    imageSnapshot.ready = true;
    imageSnapshot.ordinaryDirectMediaScopeActive = true;
    imageSnapshot.openedCollectionScopeActive = true;
    routeReplacementOnImageSnapshotRead = true;
    Q_EMIT emitter.imageSnapshotChanged();

    QVERIFY(!routeReplacementOnImageSnapshotRead);
    QCOMPARE(runtime->documentKind(), kiriview::DocumentSessionKind::Image);
    QCOMPARE(runtime->sourceUrl(), replacementImageUrl);
    QVERIFY(runtime->activeImageReady());
    QVERIFY(!runtime->activeImageOpenedCollectionScopeActive());
    QVERIFY(runtime->windowTitleSubject().startsWith(QStringLiteral("replacement.png")));
}

void TestDocumentSessionRuntimeLeafSnapshots::nestedImageSnapshotReadPreservesNewerSnapshot()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/image.png"));
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    bool publishNewerSnapshotDuringRead = false;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&]() {
        const kiriview::DocumentSessionImageDocumentSnapshot snapshot = imageSnapshot;
        if (publishNewerSnapshotDuringRead) {
            publishNewerSnapshotDuringRead = false;
            imageSnapshot.windowTitleFileName = QStringLiteral("newer.png");
            imageSnapshot.primaryImageSize = QSize(640, 480);
            Q_EMIT emitter.imageSnapshotChanged();
        }
        return snapshot;
    };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.setSource
        = [&imageSnapshot](const kiriview::ResolvedNavigationSource& source) {
              imageSnapshot.sourceUrl = source.requestedUrl();
              imageSnapshot.displayedUrl = source.requestedUrl();
              imageSnapshot.windowTitleFileName = QStringLiteral("initial.png");
              imageSnapshot.primaryImageSize = QSize(160, 120);
              imageSnapshot.ready = true;
              imageSnapshot.ordinaryDirectMediaScopeActive = true;
          };
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = []() { return kiriview::DocumentSessionVideoDocumentSnapshot {}; };
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.playback.stop = []() { };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    kiriview::DocumentSessionRuntime runtime(&owner, std::move(imagePort), std::move(imageCommands),
        std::move(videoPort), std::move(videoCommands));
    runtime.setSourceUrl(imageUrl);

    imageSnapshot.windowTitleFileName = QStringLiteral("older.png");
    imageSnapshot.primaryImageSize = QSize(320, 240);
    publishNewerSnapshotDuringRead = true;
    Q_EMIT emitter.imageSnapshotChanged();

    QVERIFY(!publishNewerSnapshotDuringRead);
    QVERIFY(runtime.windowTitleSubject().startsWith(QStringLiteral("newer.png")));
    QVERIFY(runtime.windowTitleSubject().contains(QStringLiteral("640")));
    QVERIFY(runtime.windowTitleSubject().contains(QStringLiteral("480")));
}

void TestDocumentSessionRuntimeLeafSnapshots::
    videoClearPredecodeCancellationPreservesReentrantRoute()
{
    using kiriview::TestSupport::ManualTimerScheduler;
    using kiriview::TestSupport::staticImageDataDecoder;

    QObject owner;
    SnapshotChangeEmitter emitter;
    ManualTimerScheduler timerScheduler;
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl adjacentImageUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl staleVideoUrl = localUrl(QStringLiteral("/media/03.mp4"));
    const QUrl replacementVideoUrl = localUrl(QStringLiteral("/media/04.mp4"));
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    kiriview::DocumentSessionVideoDocumentSnapshot videoSnapshot;
    std::unique_ptr<kiriview::DocumentSessionRuntime> runtime;
    bool routeReplacementOnPredecodeCancel = false;
    int predecodeLoadCount = 0;

    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&imageSnapshot]() { return imageSnapshot; };
    kiriview::DocumentSessionImageDocumentCommandPort imageCommands;
    imageCommands.source.clearSource = [&imageSnapshot]() { imageSnapshot = {}; };
    imageCommands.source.setSource
        = [&imageSnapshot](const kiriview::ResolvedNavigationSource& source) {
              imageSnapshot = {};
              imageSnapshot.sourceUrl = source.requestedUrl();
              imageSnapshot.displayedUrl = source.requestedUrl();
              imageSnapshot.ready = true;
              imageSnapshot.ordinaryDirectMediaScopeActive = true;
          };

    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = [&videoSnapshot]() { return videoSnapshot; };
    videoPort.snapshotChanged = videoSnapshotChangedConnector(emitter);
    kiriview::DocumentSessionVideoDocumentCommandPort videoCommands;
    videoCommands.source.clearSource = [&videoSnapshot]() { videoSnapshot = {}; };
    videoCommands.source.setSource
        = [&videoSnapshot](const kiriview::ResolvedNavigationSource& source) {
              videoSnapshot = {};
              videoSnapshot.sourceUrl = source.requestedUrl();
              videoSnapshot.ready = true;
              videoSnapshot.hasVideo = true;
          };
    videoCommands.playback.stop = []() { };
    videoCommands.output.videoOutput = []() -> QObject* { return nullptr; };
    videoCommands.output.setVideoOutputAttachment = [](QObject*, const QRectF&, const QRectF&) { };

    kiriview::DocumentSessionRuntimeDependencies dependencies;
    dependencies.directMediaNavigationCandidateProvider.directoryCandidateLoader
        = [imageUrl, adjacentImageUrl, staleVideoUrl, replacementVideoUrl](QObject*, const QUrl&,
              kiriview::DirectMediaNavigationCandidatesCallback callback, kiriview::ErrorCallback) {
              callback({
                  { imageUrl, imageUrl.fileName() },
                  { adjacentImageUrl, adjacentImageUrl.fileName() },
                  { staleVideoUrl, staleVideoUrl.fileName() },
                  { replacementVideoUrl, replacementVideoUrl.fileName() },
              });
              return kiriview::ImageIoJob {};
          };
    dependencies.directMediaPredecodeDependencies.imageDecode.dataLoader
        = [&](QObject* receiver, kiriview::ImageDecodeRequest, kiriview::ImageDataCallback,
              kiriview::ErrorCallback) {
              ++predecodeLoadCount;
              auto* token = new QObject(receiver);
              return kiriview::ImageIoJob(token, [&](QObject* object) {
                  const QPointer<QObject> guardedObject(object);
                  if (std::exchange(routeReplacementOnPredecodeCancel, false)) {
                      runtime->setSourceUrl(replacementVideoUrl);
                  }
                  if (guardedObject != nullptr) {
                      guardedObject->deleteLater();
                  }
              });
          };
    dependencies.directMediaPredecodeDependencies.imageDecode.dataDecoder
        = staticImageDataDecoder();
    dependencies.directMediaPredecodeDependencies.timerScheduler = timerScheduler.scheduler();
    dependencies.directMediaPredecodeDependencies.cacheBudgetRequest.predecodeCacheByteBudget
        = 1024 * 1024;

    runtime = std::make_unique<kiriview::DocumentSessionRuntime>(&owner, std::move(imagePort),
        std::move(imageCommands), std::move(videoPort), std::move(videoCommands),
        kiriview::DocumentSessionRuntime::ChangeCallback {}, std::move(dependencies));

    runtime->setSourceUrl(imageUrl);
    QCOMPARE(timerScheduler.timerCount(), std::size_t(2));
    timerScheduler.timerAt(0).fire();
    QVERIFY(predecodeLoadCount > 0);

    runtime->setSourceUrl(staleVideoUrl);
    timerScheduler.timerAt(0).fire();
    const int loadsBeforeClear = predecodeLoadCount;
    QVERIFY(loadsBeforeClear > 0);

    routeReplacementOnPredecodeCancel = true;
    videoSnapshot = {};
    Q_EMIT emitter.videoSnapshotChanged();

    QVERIFY(!routeReplacementOnPredecodeCancel);
    QCOMPARE(runtime->documentKind(), kiriview::DocumentSessionKind::Video);
    QCOMPARE(runtime->sourceUrl(), replacementVideoUrl);
}

void TestDocumentSessionRuntimeLeafSnapshots::queuedSnapshotChangeAfterRuntimeDestructionIsIgnored()
{
    QObject owner;
    SnapshotChangeEmitter emitter;
    kiriview::DocumentSessionImageDocumentSnapshot imageSnapshot;
    int imageSnapshotReadCount = 0;
    kiriview::DocumentSessionImageDocumentSnapshotPort imagePort;
    imagePort.snapshot = [&]() {
        ++imageSnapshotReadCount;
        return imageSnapshot;
    };
    imagePort.snapshotChanged = imageSnapshotChangedConnector(emitter, Qt::QueuedConnection);
    kiriview::DocumentSessionVideoDocumentSnapshotPort videoPort;
    videoPort.snapshot = []() { return kiriview::DocumentSessionVideoDocumentSnapshot {}; };
    auto runtime = std::make_unique<kiriview::DocumentSessionRuntime>(&owner, std::move(imagePort),
        kiriview::DocumentSessionImageDocumentCommandPort {}, std::move(videoPort),
        kiriview::DocumentSessionVideoDocumentCommandPort {});
    const int readsBeforeQueuedChange = imageSnapshotReadCount;

    Q_EMIT emitter.imageSnapshotChanged();
    runtime.reset();
    QCoreApplication::processEvents();

    QCOMPARE(imageSnapshotReadCount, readsBeforeQueuedChange);
}

QTEST_GUILESS_MAIN(TestDocumentSessionRuntimeLeafSnapshots)

#include "tst_documentsessionruntimeleafsnapshots.moc"
