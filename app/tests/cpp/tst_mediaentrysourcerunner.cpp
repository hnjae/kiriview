// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archive/mediaentrysourcerunner.h"

#include "image_test_support.h"
#include "media_entry_source_test_support.h"

#include <QByteArray>
#include <QTest>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace {
using kiriview::MediaEntrySourceEntry;
using kiriview::MediaEntrySourceError;
using kiriview::MediaEntrySourceImageData;
using kiriview::TestSupport::addInstrumentedMediaEntrySourceFixture;
using kiriview::TestSupport::archiveCollectionForLocalArchiveUrl;
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::instrumentedMediaEntrySourceFactory;
using kiriview::TestSupport::InstrumentedMediaEntrySourceState;
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::videoCandidate;
}

class TestMediaEntrySourceRunner : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ownsOpenedCollectionScope();
    void candidateLoadsAreCachedAfterLazyOpen();
    void dataLoadsReuseLazyOpenSource();
    void playbackDeviceLoadsReuseLazyOpenSourceAndAttachOwner();
    void failedOpenIsMemoized();
};

void TestMediaEntrySourceRunner::ownsOpenedCollectionScope()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());

    kiriview::MediaEntrySourceRunner runner(
        *archiveCollection, instrumentedMediaEntrySourceFactory(state));

    QVERIFY(kiriview::sameOpenedCollectionScopeLocation(
        runner.openedCollectionScope(), *archiveCollection));
}

void TestMediaEntrySourceRunner::candidateLoadsAreCachedAfterLazyOpen()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("02.png"));
    addInstrumentedMediaEntrySourceFixture(state, *archiveCollection,
        { imageDocumentPageCandidate(firstUrl), imageDocumentPageCandidate(secondUrl) });

    kiriview::MediaEntrySourceRunner runner(
        *archiveCollection, instrumentedMediaEntrySourceFactory(state));

    kiriview::MediaEntrySourceEntriesResult firstResult = runner.loadEntries();
    kiriview::MediaEntrySourceEntriesResult secondResult = runner.loadEntries();

    const auto* firstCandidates = kiriview::mediaEntrySourceResultValue(firstResult);
    const auto* secondCandidates = kiriview::mediaEntrySourceResultValue(secondResult);
    QVERIFY(firstCandidates != nullptr);
    QVERIFY(secondCandidates != nullptr);
    QCOMPARE(firstCandidates->entries.size(), std::size_t(2));
    QCOMPARE(secondCandidates->entries.size(), std::size_t(2));
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->candidateLoadCount.load(), 1);

    const std::optional<std::vector<MediaEntrySourceEntry>> cached = runner.cachedEntries();
    QVERIFY(cached.has_value());
    QCOMPARE(cached->size(), std::size_t(2));
}

void TestMediaEntrySourceRunner::dataLoadsReuseLazyOpenSource()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());
    const QUrl pageUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    addInstrumentedMediaEntrySourceFixture(
        state, *archiveCollection, { imageDocumentPageCandidate(pageUrl) });

    kiriview::MediaEntrySourceRunner runner(
        *archiveCollection, instrumentedMediaEntrySourceFactory(state));

    kiriview::MediaEntrySourceImageDataResult firstResult = runner.loadImageData(pageUrl);
    kiriview::MediaEntrySourceImageDataResult secondResult = runner.loadImageData(pageUrl);

    const auto* firstData = kiriview::mediaEntrySourceResultValue(firstResult);
    const auto* secondData = kiriview::mediaEntrySourceResultValue(secondResult);
    QVERIFY(firstData != nullptr);
    QVERIFY(secondData != nullptr);
    QCOMPARE(firstData->data, QByteArrayLiteral("image"));
    QCOMPARE(secondData->data, QByteArrayLiteral("image"));
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->dataLoadCount.load(), 2);
}

void TestMediaEntrySourceRunner::playbackDeviceLoadsReuseLazyOpenSourceAndAttachOwner()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());
    const QUrl videoUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.mp4"));
    addInstrumentedMediaEntrySourceFixture(state, *archiveCollection, { videoCandidate(videoUrl) });

    kiriview::MediaEntrySourceRunner runner(
        *archiveCollection, instrumentedMediaEntrySourceFactory(state));

    kiriview::MediaEntrySourceVideoPlaybackDeviceResult firstResult
        = runner.loadVideoPlaybackDevice(videoUrl);
    kiriview::MediaEntrySourceVideoPlaybackDeviceResult secondResult
        = runner.loadVideoPlaybackDevice(videoUrl);

    auto* firstDevice = kiriview::mediaEntrySourceResultValue(firstResult);
    auto* secondDevice = kiriview::mediaEntrySourceResultValue(secondResult);
    QVERIFY(firstDevice != nullptr);
    QVERIFY(secondDevice != nullptr);
    QVERIFY(firstDevice->sourceOwner != nullptr);
    QVERIFY(secondDevice->sourceOwner != nullptr);
    QCOMPARE(firstDevice->device->readAll(), QByteArrayLiteral("image"));
    QCOMPARE(secondDevice->device->readAll(), QByteArrayLiteral("image"));
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->playbackDeviceLoadCount.load(), 2);
}

void TestMediaEntrySourceRunner::failedOpenIsMemoized()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/missing.cbz")));
    QVERIFY(archiveCollection.has_value());

    kiriview::MediaEntrySourceRunner runner(
        *archiveCollection, instrumentedMediaEntrySourceFactory(state));

    kiriview::MediaEntrySourceEntriesResult candidatesResult = runner.loadEntries();
    kiriview::MediaEntrySourceImageDataResult dataResult = runner.loadImageData(
        archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png")));

    QVERIFY(kiriview::mediaEntrySourceResultError(candidatesResult) != nullptr);
    QVERIFY(kiriview::mediaEntrySourceResultError(dataResult) != nullptr);
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->candidateLoadCount.load(), 0);
    QCOMPARE(state->dataLoadCount.load(), 0);
}

QTEST_GUILESS_MAIN(TestMediaEntrySourceRunner)

#include "tst_mediaentrysourcerunner.moc"
