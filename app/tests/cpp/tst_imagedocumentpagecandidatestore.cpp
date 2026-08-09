// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidatestore.h"

#include <QList>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <algorithm>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace {
QUrl directoryUrl(const QString& directoryName = {})
{
    const QString suffix
        = directoryName.isEmpty() ? QString() : QStringLiteral("%1/").arg(directoryName);
    return QUrl::fromLocalFile(QStringLiteral("/tmp/kiriview-candidate-store-test/%1").arg(suffix));
}

QUrl fileUrl(const QString& fileName, const QString& directoryName = {})
{
    return directoryUrl(directoryName).resolved(QUrl(fileName));
}

kiriview::ImageDocumentPageCandidate candidate(
    const QString& fileName, const QString& directoryName = {})
{
    return kiriview::ImageDocumentPageCandidate {
        fileUrl(fileName, directoryName),
        fileName,
        kiriview::ImageDocumentPageKind::Image,
    };
}

std::vector<QUrl> candidateUrls(const std::vector<kiriview::ImageDocumentPageCandidate>& candidates)
{
    std::vector<QUrl> urls;
    urls.reserve(candidates.size());
    for (const kiriview::ImageDocumentPageCandidate& candidate : candidates) {
        urls.push_back(candidate.url);
    }
    return urls;
}

struct FakeWatchProvider
{
    struct Watch
    {
        QUrl watchedUrl;
        kiriview::ImageDocumentPageCandidateWatchSnapshotCallback initialSnapshot;
        kiriview::ImageDocumentPageCandidateWatchSnapshotCallback changedSnapshot;
        kiriview::ImageDocumentPageCandidateWatchDeletedCallback deletedUrls;
        kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback;
        bool active = true;
    };

    std::vector<std::shared_ptr<Watch>> watches;
    int startCount = 0;
    int liveCount = 0;
    int maximumLiveCount = 0;

    kiriview::ImageDocumentPageCandidateWatchProvider provider()
    {
        return [this](QObject* receiver, QUrl directory,
                   kiriview::ImageDocumentPageCandidateWatchSnapshotCallback initial,
                   kiriview::ImageDocumentPageCandidateWatchSnapshotCallback changed,
                   kiriview::ImageDocumentPageCandidateWatchDeletedCallback deleted,
                   kiriview::ImageDocumentPageCandidateLoadErrorCallback error) {
            auto watch = std::make_shared<Watch>();
            ++startCount;
            ++liveCount;
            maximumLiveCount = std::max(maximumLiveCount, liveCount);
            watch->watchedUrl = std::move(directory);
            watch->initialSnapshot = std::move(initial);
            watch->changedSnapshot = std::move(changed);
            watch->deletedUrls = std::move(deleted);
            watch->errorCallback = std::move(error);
            watches.push_back(watch);
            auto* token = new QObject(receiver);
            return kiriview::ImageIoJob(token, [this, watch](QObject* object) {
                if (watch->active) {
                    watch->active = false;
                    watch->initialSnapshot = {};
                    watch->changedSnapshot = {};
                    watch->deletedUrls = {};
                    watch->errorCallback = {};
                    --liveCount;
                }
                object->deleteLater();
            });
        };
    }

    const QUrl& watchedUrl(std::size_t index) const { return watches.at(index)->watchedUrl; }

    void complete(std::size_t index, std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        const kiriview::ImageDocumentPageCandidateWatchSnapshotCallback callback
            = watches.at(index)->initialSnapshot;
        if (callback) {
            callback(std::move(candidates));
        }
    }

    void change(std::size_t index, std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        const kiriview::ImageDocumentPageCandidateWatchSnapshotCallback callback
            = watches.at(index)->changedSnapshot;
        if (callback) {
            callback(std::move(candidates));
        }
    }

    void fail(std::size_t index, const QString& errorString)
    {
        const kiriview::ImageDocumentPageCandidateLoadErrorCallback callback
            = watches.at(index)->errorCallback;
        if (callback) {
            callback(kiriview::ImageDocumentPageCandidateLoadError { errorString });
        }
    }
};
}

class TestImageDocumentPageCandidateStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void localDirectoryPublishesProviderSnapshotsAndReusesCache();
    void inactiveDirectoriesReleaseWatches();
    void destroyedClientReleasesWatch();
    void revisitingReleasedDirectoryStartsFreshListing();
    void completionCallbackCanDestroyStore();
    void refreshAdmissionCoalescesBurstAndOneFollowUp();
    void earlierSubscriberCanCancelLaterChangedDelivery();
    void earlierSubscriberCanCancelLaterFailureDelivery();
    void terminalWatchStartFailureDoesNotOfferRecoverySubscription();
    void initialWatchFailureRecoversOnFirstAdmittedSnapshot();
    void failedLiveWatchMakesCachedListingUnavailableUntilRecovery();
    void liveDirectoryWatchJobCanOutliveStore();
};

void TestImageDocumentPageCandidateStore::localDirectoryPublishesProviderSnapshotsAndReusesCache()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    std::vector<kiriview::ImageDocumentPageCandidate> loadedCandidates;
    int loadErrorCount = 0;
    bool loaded = false;
    kiriview::ImageIoJob loadJob = store.loadDirectoryImages(
        this, directoryUrl(),
        [&loadedCandidates, &loaded](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            loadedCandidates = std::move(candidates);
            loaded = true;
        },
        [&loadErrorCount](
            const kiriview::ImageDocumentPageCandidateLoadError&) { ++loadErrorCount; });

    QVERIFY(loadJob.isActive());
    QCOMPARE(provider.startCount, 1);
    QCOMPARE(provider.watchedUrl(0), directoryUrl());
    provider.complete(0, { candidate(QStringLiteral("01.png")) });
    QVERIFY(loaded);
    QCOMPARE(loadErrorCount, 0);
    QCOMPARE(static_cast<int>(loadedCandidates.size()), 1);
    QCOMPARE(loadedCandidates.front().url, fileUrl(QStringLiteral("01.png")));

    std::vector<kiriview::ImageDocumentPageCandidate> changedCandidates;
    int changeCount = 0;
    kiriview::ImageIoJob watchJob = store.watchDirectoryImages(
        this, directoryUrl(),
        [&changedCandidates, &changeCount](
            std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            changedCandidates = std::move(candidates);
            ++changeCount;
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});

    QCoreApplication::processEvents();
    QCOMPARE(provider.liveCount, 1);
    QCOMPARE(provider.startCount, 1);

    provider.change(
        0, { candidate(QStringLiteral("01.png")), candidate(QStringLiteral("02.png")) });
    const std::vector<QUrl> addedUrls {
        fileUrl(QStringLiteral("01.png")),
        fileUrl(QStringLiteral("02.png")),
    };
    QCOMPARE(candidateUrls(changedCandidates), addedUrls);
    QCOMPARE(changeCount, 1);

    std::vector<kiriview::ImageDocumentPageCandidate> cachedCandidates;
    store.loadDirectoryImages(
        this, directoryUrl(),
        [&cachedCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            cachedCandidates = std::move(candidates);
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});
    QCOMPARE(candidateUrls(cachedCandidates), addedUrls);
    QCOMPARE(provider.startCount, 1);

    watchJob.cancel();
}

void TestImageDocumentPageCandidateStore::inactiveDirectoriesReleaseWatches()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    constexpr int directoryCount = 24;

    for (int index = 0; index < directoryCount; ++index) {
        const QString directoryName = QStringLiteral("directory-%1").arg(index);
        kiriview::ImageIoJob watchJob = store.watchDirectoryImages(
            this, directoryUrl(directoryName),
            [](std::vector<kiriview::ImageDocumentPageCandidate>) { },
            [](const kiriview::ImageDocumentPageCandidateLoadError&) { });

        QVERIFY(watchJob.isActive());
        QCOMPARE(provider.liveCount, 1);
        watchJob.cancel();
        QTRY_COMPARE(provider.liveCount, 0);
    }

    QCOMPARE(provider.startCount, directoryCount);
    QCOMPARE(provider.maximumLiveCount, 1);
}

void TestImageDocumentPageCandidateStore::destroyedClientReleasesWatch()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    auto receiver = std::make_unique<QObject>();
    kiriview::ImageIoJob watchJob = store.watchDirectoryImages(
        receiver.get(), directoryUrl(QStringLiteral("destroyed-client")),
        [](std::vector<kiriview::ImageDocumentPageCandidate>) { },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) { });

    QVERIFY(watchJob.isActive());
    QCOMPARE(provider.liveCount, 1);
    receiver.reset();

    QVERIFY(!watchJob.isActive());
    QTRY_COMPARE(provider.liveCount, 0);
}

void TestImageDocumentPageCandidateStore::revisitingReleasedDirectoryStartsFreshListing()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    const QString directoryName = QStringLiteral("revisited");
    const QUrl revisitedDirectory = directoryUrl(directoryName);
    std::vector<kiriview::ImageDocumentPageCandidate> firstCandidates;

    kiriview::ImageIoJob firstWatch = store.watchDirectoryImages(
        this, revisitedDirectory, [](std::vector<kiriview::ImageDocumentPageCandidate>) { },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) { });
    kiriview::ImageIoJob firstLoad = store.loadDirectoryImages(
        this, revisitedDirectory,
        [&firstCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            firstCandidates = std::move(candidates);
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});

    QCOMPARE(provider.startCount, 1);
    provider.complete(0, { candidate(QStringLiteral("old.png"), directoryName) });
    QCOMPARE(candidateUrls(firstCandidates),
        std::vector<QUrl> { fileUrl(QStringLiteral("old.png"), directoryName) });
    QVERIFY(!firstLoad.isActive());

    firstWatch.cancel();
    QTRY_COMPARE(provider.liveCount, 0);

    std::vector<kiriview::ImageDocumentPageCandidate> revisitedCandidates;
    kiriview::ImageIoJob secondWatch = store.watchDirectoryImages(
        this, revisitedDirectory, [](std::vector<kiriview::ImageDocumentPageCandidate>) { },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) { });
    kiriview::ImageIoJob secondLoad = store.loadDirectoryImages(
        this, revisitedDirectory,
        [&revisitedCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            revisitedCandidates = std::move(candidates);
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});

    QCOMPARE(provider.startCount, 2);
    QVERIFY(secondLoad.isActive());
    QVERIFY(revisitedCandidates.empty());
    provider.complete(1, { candidate(QStringLiteral("new.png"), directoryName) });
    QCOMPARE(candidateUrls(revisitedCandidates),
        std::vector<QUrl> { fileUrl(QStringLiteral("new.png"), directoryName) });
    QVERIFY(!secondLoad.isActive());

    secondWatch.cancel();
}

void TestImageDocumentPageCandidateStore::completionCallbackCanDestroyStore()
{
    FakeWatchProvider provider;
    auto store = std::make_unique<kiriview::ImageDocumentPageCandidateStore>(provider.provider());
    int completionCount = 0;
    kiriview::ImageIoJob loadJob = store->loadDirectoryImages(
        this, directoryUrl(QStringLiteral("destroyed-from-callback")),
        [&store, &completionCount](std::vector<kiriview::ImageDocumentPageCandidate>) {
            ++completionCount;
            store.reset();
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});

    provider.complete(
        0, { candidate(QStringLiteral("01.png"), QStringLiteral("destroyed-from-callback")) });

    QCOMPARE(completionCount, 1);
    QVERIFY(store == nullptr);
    QVERIFY(!loadJob.isActive());
    QCOMPARE(provider.liveCount, 0);
}

void TestImageDocumentPageCandidateStore::refreshAdmissionCoalescesBurstAndOneFollowUp()
{
    kiriview::ImageDocumentPageCandidateRefreshAdmission admission;

    QVERIFY(admission.requestRefresh());
    for (int index = 0; index < 128; ++index) {
        QVERIFY(!admission.requestRefresh());
    }

    admission.beginRefresh();
    QVERIFY(!admission.requestRefresh());
    QVERIFY(admission.finishRefresh());

    admission.beginRefresh();
    QVERIFY(!admission.finishRefresh());
    QVERIFY(admission.requestRefresh());

    const quint64 staleEpoch = admission.epoch();
    admission.invalidatePendingRefresh();
    QVERIFY(!admission.acceptsEpoch(staleEpoch));
    QVERIFY(admission.requestRefresh());
    QVERIFY(admission.acceptsEpoch(admission.epoch()));
}

void TestImageDocumentPageCandidateStore::earlierSubscriberCanCancelLaterChangedDelivery()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    kiriview::ImageIoJob laterSubscriber;
    int earlierDeliveryCount = 0;
    int laterDeliveryCount = 0;
    kiriview::ImageIoJob earlierSubscriber = store.watchDirectoryImages(
        this, directoryUrl(QStringLiteral("cancel-changed")),
        [&laterSubscriber, &earlierDeliveryCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) {
            ++earlierDeliveryCount;
            laterSubscriber.cancel();
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});
    laterSubscriber = store.watchDirectoryImages(
        this, directoryUrl(QStringLiteral("cancel-changed")),
        [&laterDeliveryCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++laterDeliveryCount; },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});

    provider.complete(0, { candidate(QStringLiteral("01.png"), QStringLiteral("cancel-changed")) });
    provider.change(0,
        { candidate(QStringLiteral("01.png"), QStringLiteral("cancel-changed")),
            candidate(QStringLiteral("02.png"), QStringLiteral("cancel-changed")) });

    QCOMPARE(earlierDeliveryCount, 1);
    QCOMPARE(laterDeliveryCount, 0);
    QVERIFY(!laterSubscriber.isActive());
    earlierSubscriber.cancel();
}

void TestImageDocumentPageCandidateStore::earlierSubscriberCanCancelLaterFailureDelivery()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    kiriview::ImageIoJob laterSubscriber;
    int earlierFailureCount = 0;
    int laterFailureCount = 0;
    kiriview::ImageIoJob earlierSubscriber = store.watchDirectoryImages(
        this, directoryUrl(QStringLiteral("cancel-failure")),
        [](std::vector<kiriview::ImageDocumentPageCandidate>) {},
        [&laterSubscriber, &earlierFailureCount](
            const kiriview::ImageDocumentPageCandidateLoadError& error) {
            QVERIFY(std::holds_alternative<QString>(error));
            ++earlierFailureCount;
            laterSubscriber.cancel();
        });
    laterSubscriber = store.watchDirectoryImages(
        this, directoryUrl(QStringLiteral("cancel-failure")),
        [](std::vector<kiriview::ImageDocumentPageCandidate>) {},
        [&laterFailureCount](
            const kiriview::ImageDocumentPageCandidateLoadError&) { ++laterFailureCount; });

    provider.fail(0, QStringLiteral("listing failed"));

    QCOMPARE(earlierFailureCount, 1);
    QCOMPARE(laterFailureCount, 0);
    QVERIFY(!laterSubscriber.isActive());
    earlierSubscriber.cancel();
}

void TestImageDocumentPageCandidateStore::
    terminalWatchStartFailureDoesNotOfferRecoverySubscription()
{
    kiriview::ImageDocumentPageCandidateWatchProvider provider
        = [](QObject*, QUrl, kiriview::ImageDocumentPageCandidateWatchSnapshotCallback,
              kiriview::ImageDocumentPageCandidateWatchSnapshotCallback,
              kiriview::ImageDocumentPageCandidateWatchDeletedCallback,
              kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback) {
              errorCallback(kiriview::ImageDocumentPageCandidateLoadError {
                  QStringLiteral("watch failed to start") });
              return kiriview::ImageIoJob();
          };
    kiriview::ImageDocumentPageCandidateStore store(std::move(provider));
    int failureCount = 0;

    kiriview::ImageIoJob watch = store.watchDirectoryImages(
        this, directoryUrl(QStringLiteral("terminal-watch-failure")),
        [](std::vector<kiriview::ImageDocumentPageCandidate>) {},
        [&failureCount](const kiriview::ImageDocumentPageCandidateLoadError&) { ++failureCount; });

    QVERIFY(!watch.isActive());
    QCOMPARE(failureCount, 1);
}

void TestImageDocumentPageCandidateStore::initialWatchFailureRecoversOnFirstAdmittedSnapshot()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    const QString directoryName = QStringLiteral("initial-watch-recovery");
    int failureCount = 0;
    std::vector<kiriview::ImageDocumentPageCandidate> recoveredCandidates;
    kiriview::ImageIoJob watch = store.watchDirectoryImages(
        this, directoryUrl(directoryName),
        [&recoveredCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            recoveredCandidates = std::move(candidates);
        },
        [&failureCount](const kiriview::ImageDocumentPageCandidateLoadError&) { ++failureCount; });

    provider.fail(0, QStringLiteral("initial watch failed"));
    QCOMPARE(failureCount, 1);
    QVERIFY(recoveredCandidates.empty());

    provider.change(0, { candidate(QStringLiteral("01.png"), directoryName) });
    QCOMPARE(candidateUrls(recoveredCandidates),
        std::vector<QUrl> { fileUrl(QStringLiteral("01.png"), directoryName) });

    watch.cancel();
}

void TestImageDocumentPageCandidateStore::
    failedLiveWatchMakesCachedListingUnavailableUntilRecovery()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    const QString directoryName = QStringLiteral("recovering-watch");
    const QUrl watchedDirectory = directoryUrl(directoryName);
    int originalFailureCount = 0;
    kiriview::ImageIoJob originalWatch = store.watchDirectoryImages(
        this, watchedDirectory, [](std::vector<kiriview::ImageDocumentPageCandidate>) {},
        [&originalFailureCount](
            const kiriview::ImageDocumentPageCandidateLoadError&) { ++originalFailureCount; });
    std::vector<kiriview::ImageDocumentPageCandidate> initiallyLoaded;
    kiriview::ImageIoJob initialLoad = store.loadDirectoryImages(
        this, watchedDirectory,
        [&initiallyLoaded](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            initiallyLoaded = std::move(candidates);
        },
        [](const kiriview::ImageDocumentPageCandidateLoadError&) {});
    const std::vector<QUrl> initialUrls {
        fileUrl(QStringLiteral("01.png"), directoryName),
        fileUrl(QStringLiteral("02.png"), directoryName),
    };
    provider.complete(0,
        { candidate(QStringLiteral("01.png"), directoryName),
            candidate(QStringLiteral("02.png"), directoryName) });
    QVERIFY(!initialLoad.isActive());
    QCOMPARE(candidateUrls(initiallyLoaded), initialUrls);

    provider.fail(0, QStringLiteral("live watch failed"));
    QCOMPARE(originalFailureCount, 1);

    int cachedCandidateCount = 0;
    int cachedLoadErrorCount = 0;
    store.loadDirectoryImages(
        this, watchedDirectory,
        [&cachedCandidateCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++cachedCandidateCount; },
        [&cachedLoadErrorCount](
            const kiriview::ImageDocumentPageCandidateLoadError&) { ++cachedLoadErrorCount; });
    QCOMPARE(cachedCandidateCount, 0);
    QCOMPARE(cachedLoadErrorCount, 1);

    std::vector<kiriview::ImageDocumentPageCandidate> recoveredCandidates;
    int reboundFailureCount = 0;
    kiriview::ImageIoJob reboundWatch = store.watchDirectoryImages(
        this, watchedDirectory,
        [&recoveredCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            recoveredCandidates = std::move(candidates);
        },
        [&reboundFailureCount](
            const kiriview::ImageDocumentPageCandidateLoadError&) { ++reboundFailureCount; });
    QVERIFY(reboundWatch.isActive());
    QCOMPARE(reboundFailureCount, 1);

    const std::vector<QUrl> recoveredUrls {
        fileUrl(QStringLiteral("01.png"), directoryName),
        fileUrl(QStringLiteral("03.png"), directoryName),
    };
    provider.change(0,
        { candidate(QStringLiteral("01.png"), directoryName),
            candidate(QStringLiteral("03.png"), directoryName) });
    QCOMPARE(candidateUrls(recoveredCandidates), recoveredUrls);

    reboundWatch.cancel();
    originalWatch.cancel();
}

void TestImageDocumentPageCandidateStore::liveDirectoryWatchJobCanOutliveStore()
{
    FakeWatchProvider provider;
    kiriview::ImageIoJob watchJob;
    {
        kiriview::ImageDocumentPageCandidateStore store(provider.provider());
        watchJob = store.watchDirectoryImages(
            this, directoryUrl(), [](std::vector<kiriview::ImageDocumentPageCandidate>) { },
            [](const kiriview::ImageDocumentPageCandidateLoadError&) { });
        QVERIFY(watchJob.isActive());
    }

    QVERIFY(watchJob.isActive());
    watchJob.cancel();
    QVERIFY(!watchJob.isActive());
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateStore)

#include "tst_imagedocumentpagecandidatestore.moc"
