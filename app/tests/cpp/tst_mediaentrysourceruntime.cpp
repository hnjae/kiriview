// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "archive/mediaentrysourceruntime.h"

#include "async/imageworkerscheduler.h"
#include "image_test_support.h"
#include "media_entry_source_test_support.h"

#include <QByteArray>
#include <QSemaphore>
#include <QTest>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace {
using kiriview::ImageDocumentPageCandidate;
using kiriview::TestSupport::addInstrumentedMediaEntrySourceFixture;
using kiriview::TestSupport::archiveCollectionForLocalArchiveUrl;
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::blockInstrumentedMediaEntrySourceCandidateLoads;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::instrumentedMediaEntrySourceFactory;
using kiriview::TestSupport::InstrumentedMediaEntrySourceState;
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::releaseInstrumentedMediaEntrySourceLoads;
using kiriview::TestSupport::videoCandidate;

struct ManualImageWorkerSchedule
{
    kiriview::ImageWorkerOperation work;
    kiriview::ImageWorkerCompletion completion;
};

class ManualImageWorkerScheduler
{
public:
    kiriview::ImageWorkerScheduler scheduler()
    {
        return kiriview::ImageWorkerScheduler([this](QObject*, kiriview::ImageWorkerOperation work,
                                                  kiriview::ImageWorkerCompletion completion) {
            m_schedules.push_back(
                ManualImageWorkerSchedule { std::move(work), std::move(completion) });
            return kiriview::ImageWorkerTask {};
        });
    }

    std::size_t scheduleCount() const { return m_schedules.size(); }

    void runWork(std::size_t index)
    {
        if (m_schedules.at(index).work) {
            m_schedules.at(index).work();
        }
    }

    void finish(std::size_t index)
    {
        if (m_schedules.at(index).completion) {
            m_schedules.at(index).completion();
        }
    }

private:
    std::vector<ManualImageWorkerSchedule> m_schedules;
};
}

class TestMediaEntrySourceRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void synchronousLoadsShareLazyOpenAndCandidateCache();
    void simultaneousCandidateLoadsSharePendingBatch();
    void candidateLoadAddedDuringActiveBatchSharesWorker();
    void candidateBatchCancellationPreventsStaleCallbacks();
    void candidateBatchCancellationRequestsBackendStop();
    void dataCompletionAfterOpenedCollectionSwitchIsIgnored();
    void nonCurrentScopeAccessIsRejectedWithoutReplacingCurrentSnapshot();
    void errorsRemainTypedThroughRuntimeCallbacks();
};

void TestMediaEntrySourceRuntime::synchronousLoadsShareLazyOpenAndCandidateCache()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());
    const QUrl firstUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("02.png"));
    addInstrumentedMediaEntrySourceFixture(state, *archiveCollection,
        { imageDocumentPageCandidate(firstUrl), imageDocumentPageCandidate(secondUrl) });

    kiriview::MediaEntrySourceRuntime runtime(this, instrumentedMediaEntrySourceFactory(state));
    std::vector<ImageDocumentPageCandidate> firstCandidates;
    std::vector<ImageDocumentPageCandidate> cachedCandidates;
    QByteArray data;

    runtime.loadOpenedCollectionCandidates(nullptr, *archiveCollection,
        [&firstCandidates](auto loaded) { firstCandidates = std::move(loaded); }, {});
    runtime.loadOpenedCollectionCandidates(nullptr, *archiveCollection,
        [&cachedCandidates](auto loaded) { cachedCandidates = std::move(loaded); }, {});
    runtime.loadOpenedCollectionImageData(nullptr,
        kiriview::ImageDecodeRequest::fromLocation(1,
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                firstUrl, *archiveCollection)),
        [&data](kiriview::ImageSourceData loaded) { data = std::move(loaded.data); }, {});

    QCOMPARE(firstCandidates.size(), std::size_t(2));
    QCOMPARE(cachedCandidates.size(), std::size_t(2));
    QCOMPARE(data, QByteArrayLiteral("image"));
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->candidateLoadCount.load(), 1);
    QCOMPARE(state->dataLoadCount.load(), 1);
}

void TestMediaEntrySourceRuntime::simultaneousCandidateLoadsSharePendingBatch()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());
    const QUrl pageUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    addInstrumentedMediaEntrySourceFixture(
        state, *archiveCollection, { imageDocumentPageCandidate(pageUrl) });
    blockInstrumentedMediaEntrySourceCandidateLoads(state);

    kiriview::MediaEntrySourceRuntime runtime(this, instrumentedMediaEntrySourceFactory(state));
    int callbackCount = 0;
    kiriview::ImageIoJob firstJob = runtime.loadOpenedCollectionCandidates(this, *archiveCollection,
        [&callbackCount](std::vector<ImageDocumentPageCandidate>) { ++callbackCount; }, {});
    kiriview::ImageIoJob secondJob
        = runtime.loadOpenedCollectionCandidates(this, *archiveCollection,
            [&callbackCount](std::vector<ImageDocumentPageCandidate>) { ++callbackCount; }, {});
    QTRY_COMPARE(state->waitingCandidateLoadCount.load(), 1);

    releaseInstrumentedMediaEntrySourceLoads(state);

    QTRY_COMPARE(callbackCount, 2);
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->candidateLoadCount.load(), 1);
}

void TestMediaEntrySourceRuntime::candidateLoadAddedDuringActiveBatchSharesWorker()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());
    const QUrl pageUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));
    addInstrumentedMediaEntrySourceFixture(
        state, *archiveCollection, { imageDocumentPageCandidate(pageUrl) });

    ManualImageWorkerScheduler workerScheduler;
    kiriview::MediaEntrySourceRuntime runtime(
        this, instrumentedMediaEntrySourceFactory(state), workerScheduler.scheduler());
    int callbackCount = 0;
    kiriview::ImageIoJob firstJob = runtime.loadOpenedCollectionCandidates(this, *archiveCollection,
        [&callbackCount](std::vector<ImageDocumentPageCandidate>) { ++callbackCount; }, {});

    kiriview::ImageIoJob secondJob
        = runtime.loadOpenedCollectionCandidates(this, *archiveCollection,
            [&callbackCount](std::vector<ImageDocumentPageCandidate>) { ++callbackCount; }, {});

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(state->candidateLoadCount.load(), 0);
    QCOMPARE(callbackCount, 0);
    QVERIFY(firstJob.isActive());
    QVERIFY(secondJob.isActive());

    workerScheduler.runWork(0);

    QCOMPARE(state->candidateLoadCount.load(), 1);
    QCOMPARE(callbackCount, 0);
    QVERIFY(firstJob.isActive());
    QVERIFY(secondJob.isActive());

    workerScheduler.finish(0);

    QCOMPARE(callbackCount, 2);
    QVERIFY(!firstJob.isActive());
    QVERIFY(!secondJob.isActive());
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->candidateLoadCount.load(), 1);
}

void TestMediaEntrySourceRuntime::candidateBatchCancellationPreventsStaleCallbacks()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> firstArchiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/a.cbz")));
    const std::optional<kiriview::OpenedCollectionScopeLocation> secondArchiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/b.cbz")));
    QVERIFY(firstArchiveCollection.has_value());
    QVERIFY(secondArchiveCollection.has_value());
    const QUrl firstPageUrl
        = archivePageUrl(firstArchiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondPageUrl
        = archivePageUrl(secondArchiveCollection->rootUrl(), QStringLiteral("01.png"));
    addInstrumentedMediaEntrySourceFixture(
        state, *firstArchiveCollection, { imageDocumentPageCandidate(firstPageUrl) });
    addInstrumentedMediaEntrySourceFixture(
        state, *secondArchiveCollection, { imageDocumentPageCandidate(secondPageUrl) });
    blockInstrumentedMediaEntrySourceCandidateLoads(state);

    kiriview::MediaEntrySourceRuntime runtime(this, instrumentedMediaEntrySourceFactory(state));
    int staleCallbackCount = 0;
    kiriview::ImageIoJob staleJob = runtime.loadOpenedCollectionCandidates(this,
        *firstArchiveCollection,
        [&staleCallbackCount](std::vector<ImageDocumentPageCandidate>) { ++staleCallbackCount; },
        {});
    QTRY_COMPARE(state->waitingCandidateLoadCount.load(), 1);

    runtime.switchToOpenedCollectionScope(*secondArchiveCollection);
    releaseInstrumentedMediaEntrySourceLoads(state);

    QTRY_VERIFY(!staleJob.isActive());
    QCOMPARE(staleCallbackCount, 0);
    QVERIFY(runtime.hasCurrentOpenedCollectionScope(*secondArchiveCollection));
}

void TestMediaEntrySourceRuntime::candidateBatchCancellationRequestsBackendStop()
{
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/book.cbz")));
    QVERIFY(archiveCollection.has_value());

    QSemaphore openStarted;
    QSemaphore stopRequested;
    std::atomic<bool> backendObservedStop = false;
    kiriview::MediaEntrySourceFactory sourceFactory
        = [&openStarted, &stopRequested, &backendObservedStop](
              const kiriview::OpenedCollectionScopeLocation& scope,
              const kiriview::MediaEntrySourceOpenContext& context)
        -> kiriview::MediaEntrySourceOpenResult {
        std::stop_callback callback(
            context.stopToken, [&stopRequested]() { stopRequested.release(); });
        openStarted.release();
        stopRequested.acquire();
        backendObservedStop.store(context.stopToken.stop_requested());
        return std::unexpected(kiriview::MediaEntrySourceError {
            kiriview::MediaEntrySourceErrorCause::OperationCancelled,
            kiriview::MediaEntrySourceBackendKind::Unknown,
            kiriview::MediaEntrySourceOperation::ListCandidates, scope.fileUrl(), {},
            QStringLiteral("test collection enumeration canceled") });
    };

    kiriview::MediaEntrySourceRuntime runtime(this, std::move(sourceFactory));
    int completionCount = 0;
    kiriview::ImageIoJob load = runtime.loadOpenedCollectionCandidates(
        this, *archiveCollection,
        [&completionCount](std::vector<ImageDocumentPageCandidate>) { ++completionCount; },
        [&completionCount](kiriview::MediaEntrySourceError) { ++completionCount; });

    QTRY_VERIFY_WITH_TIMEOUT(openStarted.available() > 0, 1000);
    runtime.clear();
    QTRY_VERIFY_WITH_TIMEOUT(backendObservedStop.load(), 1000);

    QCOMPARE(completionCount, 0);
    QVERIFY(!load.isActive());
}

void TestMediaEntrySourceRuntime::dataCompletionAfterOpenedCollectionSwitchIsIgnored()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> firstArchiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/a.cbz")));
    const std::optional<kiriview::OpenedCollectionScopeLocation> secondArchiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/b.cbz")));
    QVERIFY(firstArchiveCollection.has_value());
    QVERIFY(secondArchiveCollection.has_value());
    const QUrl firstPageUrl
        = archivePageUrl(firstArchiveCollection->rootUrl(), QStringLiteral("01.png"));
    const QUrl secondPageUrl
        = archivePageUrl(secondArchiveCollection->rootUrl(), QStringLiteral("01.png"));
    addInstrumentedMediaEntrySourceFixture(
        state, *firstArchiveCollection, { imageDocumentPageCandidate(firstPageUrl) });
    addInstrumentedMediaEntrySourceFixture(
        state, *secondArchiveCollection, { imageDocumentPageCandidate(secondPageUrl) });

    ManualImageWorkerScheduler workerScheduler;
    kiriview::MediaEntrySourceRuntime runtime(
        this, instrumentedMediaEntrySourceFactory(state), workerScheduler.scheduler());
    runtime.switchToOpenedCollectionScope(*firstArchiveCollection);
    int staleCallbackCount = 0;
    kiriview::ImageIoJob staleJob = runtime.loadOpenedCollectionImageData(this,
        kiriview::ImageDecodeRequest::fromLocation(1,
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                firstPageUrl, *firstArchiveCollection)),
        [&staleCallbackCount](kiriview::ImageSourceData) { ++staleCallbackCount; }, {});

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(state->dataLoadCount.load(), 0);
    QVERIFY(staleJob.isActive());

    runtime.switchToOpenedCollectionScope(*secondArchiveCollection);
    workerScheduler.runWork(0);

    QCOMPARE(state->dataLoadCount.load(), 1);
    QCOMPARE(staleCallbackCount, 0);

    workerScheduler.finish(0);

    QVERIFY(!staleJob.isActive());
    QCOMPARE(staleCallbackCount, 0);
    QVERIFY(runtime.hasCurrentOpenedCollectionScope(*secondArchiveCollection));
}

void TestMediaEntrySourceRuntime::nonCurrentScopeAccessIsRejectedWithoutReplacingCurrentSnapshot()
{
    auto state = std::make_shared<InstrumentedMediaEntrySourceState>();
    const std::optional<kiriview::OpenedCollectionScopeLocation> currentCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/current.cbz")));
    const std::optional<kiriview::OpenedCollectionScopeLocation> foreignCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/foreign.cbz")));
    QVERIFY(currentCollection.has_value());
    QVERIFY(foreignCollection.has_value());
    const QUrl currentUrl
        = archivePageUrl(currentCollection->rootUrl(), QStringLiteral("current.png"));
    const QUrl foreignImageUrl
        = archivePageUrl(foreignCollection->rootUrl(), QStringLiteral("foreign.png"));
    const QUrl foreignVideoUrl
        = archivePageUrl(foreignCollection->rootUrl(), QStringLiteral("foreign.mp4"));
    addInstrumentedMediaEntrySourceFixture(
        state, *currentCollection, { imageDocumentPageCandidate(currentUrl) });
    addInstrumentedMediaEntrySourceFixture(state, *foreignCollection,
        { imageDocumentPageCandidate(foreignImageUrl), videoCandidate(foreignVideoUrl) });

    kiriview::MediaEntrySourceRuntime runtime(this, instrumentedMediaEntrySourceFactory(state));
    std::vector<ImageDocumentPageCandidate> initialCandidates;
    runtime.loadOpenedCollectionCandidates(nullptr, *currentCollection,
        [&initialCandidates](auto candidates) { initialCandidates = std::move(candidates); }, {});
    QCOMPARE(initialCandidates.size(), std::size_t(1));
    QCOMPARE(state->openCount.load(), 1);
    QCOMPARE(state->candidateLoadCount.load(), 1);
    QVERIFY(runtime.hasCurrentOpenedCollectionScope(*currentCollection));

    bool imageAccessSucceeded = false;
    std::optional<kiriview::MediaEntrySourceError> imageAccessError;
    runtime.loadOpenedCollectionImageData(
        nullptr,
        kiriview::ImageDecodeRequest::fromLocation(1,
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                foreignImageUrl, *foreignCollection)),
        [&imageAccessSucceeded](kiriview::ImageSourceData) { imageAccessSucceeded = true; },
        [&imageAccessError](
            kiriview::MediaEntrySourceError error) { imageAccessError = std::move(error); });
    const kiriview::MediaEntrySourceVideoPlaybackDeviceResult videoResult
        = runtime.loadOpenedCollectionVideoPlaybackDevice(*foreignCollection, foreignVideoUrl);
    const bool videoAccessSucceeded = kiriview::mediaEntrySourceResultValue(videoResult) != nullptr;
    const kiriview::MediaEntrySourceError* videoAccessError
        = kiriview::mediaEntrySourceResultError(videoResult);

    const bool currentScopePreserved = runtime.hasCurrentOpenedCollectionScope(*currentCollection)
        && !runtime.hasCurrentOpenedCollectionScope(*foreignCollection);
    std::vector<ImageDocumentPageCandidate> candidatesAfterRejectedAccess;
    runtime.loadOpenedCollectionCandidates(nullptr, *currentCollection,
        [&candidatesAfterRejectedAccess](
            auto candidates) { candidatesAfterRejectedAccess = std::move(candidates); },
        {});
    const bool currentSnapshotPreserved
        = candidatesAfterRejectedAccess.size() == initialCandidates.size()
        && !candidatesAfterRejectedAccess.empty()
        && candidatesAfterRejectedAccess.front().url == initialCandidates.front().url
        && state->openCount.load() == 1 && state->candidateLoadCount.load() == 1;
    const QString failureDetail
        = QStringLiteral("imageSucceeded=%1 imageFailed=%2 videoSucceeded=%3 videoFailed=%4 "
                         "currentScopePreserved=%5 currentSnapshotPreserved=%6 openCount=%7 "
                         "candidateLoadCount=%8 dataLoadCount=%9 playbackDeviceLoadCount=%10")
              .arg(imageAccessSucceeded)
              .arg(imageAccessError.has_value())
              .arg(videoAccessSucceeded)
              .arg(videoAccessError != nullptr)
              .arg(currentScopePreserved)
              .arg(currentSnapshotPreserved)
              .arg(state->openCount.load())
              .arg(state->candidateLoadCount.load())
              .arg(state->dataLoadCount.load())
              .arg(state->playbackDeviceLoadCount.load());

    QVERIFY2(!imageAccessSucceeded && imageAccessError.has_value() && !videoAccessSucceeded
            && videoAccessError != nullptr && currentScopePreserved && currentSnapshotPreserved
            && state->dataLoadCount.load() == 0 && state->playbackDeviceLoadCount.load() == 0,
        qPrintable(failureDetail));
    QCOMPARE(imageAccessError->cause, kiriview::MediaEntrySourceErrorCause::EntryNotFound);
    QCOMPARE(imageAccessError->operation, kiriview::MediaEntrySourceOperation::ReadImageData);
    QCOMPARE(imageAccessError->collectionUrl, foreignCollection->fileUrl());
    QCOMPARE(videoAccessError->cause, kiriview::MediaEntrySourceErrorCause::EntryNotFound);
    QCOMPARE(
        videoAccessError->operation, kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice);
    QCOMPARE(videoAccessError->collectionUrl, foreignCollection->fileUrl());
}

void TestMediaEntrySourceRuntime::errorsRemainTypedThroughRuntimeCallbacks()
{
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = archiveCollectionForLocalArchiveUrl(localUrl(QStringLiteral("/books/broken.cbr")));
    QVERIFY(archiveCollection.has_value());
    const QString diagnostic = QStringLiteral("libarchive header scan failed");
    kiriview::MediaEntrySourceRuntime runtime(this,
        [diagnostic](const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
            const kiriview::MediaEntrySourceOpenContext&) -> kiriview::MediaEntrySourceOpenResult {
            return std::unexpected(kiriview::MediaEntrySourceError {
                kiriview::MediaEntrySourceErrorCause::CandidateListingFailed,
                kiriview::MediaEntrySourceBackendKind::LibArchive,
                kiriview::MediaEntrySourceOperation::ListCandidates,
                openedCollectionScope.fileUrl(),
                {},
                diagnostic,
            });
        });

    bool candidatesReported = false;
    std::optional<kiriview::MediaEntrySourceError> failure;
    runtime.loadOpenedCollectionCandidates(
        nullptr, *archiveCollection,
        [&candidatesReported](
            std::vector<kiriview::ImageDocumentPageCandidate>) { candidatesReported = true; },
        [&failure](kiriview::MediaEntrySourceError error) { failure = std::move(error); });

    QVERIFY(!candidatesReported);
    QVERIFY(failure.has_value());
    QCOMPARE(failure->cause, kiriview::MediaEntrySourceErrorCause::CandidateListingFailed);
    QCOMPARE(failure->backend, kiriview::MediaEntrySourceBackendKind::LibArchive);
    QCOMPARE(failure->operation, kiriview::MediaEntrySourceOperation::ListCandidates);
    QCOMPARE(failure->collectionUrl, archiveCollection->fileUrl());
    QVERIFY(failure->entryPath.isEmpty());
    QCOMPARE(failure->diagnosticDetail, diagnostic);
}

QTEST_GUILESS_MAIN(TestMediaEntrySourceRuntime)

#include "tst_mediaentrysourceruntime.moc"
