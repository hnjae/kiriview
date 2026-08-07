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
        kiriview::ErrorCallback errorCallback;
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
                   kiriview::ErrorCallback error) {
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
        const kiriview::ErrorCallback callback = watches.at(index)->errorCallback;
        if (callback) {
            callback(errorString);
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
    void earlierSubscriberCanCancelLaterChangedDelivery();
    void earlierSubscriberCanCancelLaterFailureDelivery();
    void liveDirectoryWatchJobCanOutliveStore();
};

void TestImageDocumentPageCandidateStore::localDirectoryPublishesProviderSnapshotsAndReusesCache()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateStore store(provider.provider());
    std::vector<kiriview::ImageDocumentPageCandidate> loadedCandidates;
    QString loadError;
    bool loaded = false;
    kiriview::ImageIoJob loadJob = store.loadDirectoryImages(
        this, directoryUrl(),
        [&loadedCandidates, &loaded](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            loadedCandidates = std::move(candidates);
            loaded = true;
        },
        [&loadError](const QString& errorString) { loadError = errorString; });

    QVERIFY(loadJob.isActive());
    QCOMPARE(provider.startCount, 1);
    QCOMPARE(provider.watchedUrl(0), directoryUrl());
    provider.complete(0, { candidate(QStringLiteral("01.png")) });
    QVERIFY(loaded);
    QVERIFY2(loadError.isEmpty(), qPrintable(loadError));
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
        [](const QString&) {});

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
        [](const QString&) {});
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
            [](std::vector<kiriview::ImageDocumentPageCandidate>) { }, [](const QString&) { });

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
        [](std::vector<kiriview::ImageDocumentPageCandidate>) { }, [](const QString&) { });

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
        [](const QString&) { });
    kiriview::ImageIoJob firstLoad = store.loadDirectoryImages(
        this, revisitedDirectory,
        [&firstCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            firstCandidates = std::move(candidates);
        },
        [](const QString&) {});

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
        [](const QString&) { });
    kiriview::ImageIoJob secondLoad = store.loadDirectoryImages(
        this, revisitedDirectory,
        [&revisitedCandidates](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            revisitedCandidates = std::move(candidates);
        },
        [](const QString&) {});

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
        [](const QString&) {});

    provider.complete(
        0, { candidate(QStringLiteral("01.png"), QStringLiteral("destroyed-from-callback")) });

    QCOMPARE(completionCount, 1);
    QVERIFY(store == nullptr);
    QVERIFY(!loadJob.isActive());
    QCOMPARE(provider.liveCount, 0);
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
        [](const QString&) {});
    laterSubscriber = store.watchDirectoryImages(
        this, directoryUrl(QStringLiteral("cancel-changed")),
        [&laterDeliveryCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++laterDeliveryCount; },
        [](const QString&) {});

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
        [&laterSubscriber, &earlierFailureCount](const QString&) {
            ++earlierFailureCount;
            laterSubscriber.cancel();
        });
    laterSubscriber = store.watchDirectoryImages(
        this, directoryUrl(QStringLiteral("cancel-failure")),
        [](std::vector<kiriview::ImageDocumentPageCandidate>) {},
        [&laterFailureCount](const QString&) { ++laterFailureCount; });

    provider.fail(0, QStringLiteral("listing failed"));

    QCOMPARE(earlierFailureCount, 1);
    QCOMPARE(laterFailureCount, 0);
    QVERIFY(!laterSubscriber.isActive());
    earlierSubscriber.cancel();
}

void TestImageDocumentPageCandidateStore::liveDirectoryWatchJobCanOutliveStore()
{
    FakeWatchProvider provider;
    kiriview::ImageIoJob watchJob;
    {
        kiriview::ImageDocumentPageCandidateStore store(provider.provider());
        watchJob = store.watchDirectoryImages(
            this, directoryUrl(), [](std::vector<kiriview::ImageDocumentPageCandidate>) { },
            [](const QString&) { });
        QVERIFY(watchJob.isActive());
    }

    QVERIFY(watchJob.isActive());
    watchJob.cancel();
    QVERIFY(!watchJob.isActive());
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateStore)

#include "tst_imagedocumentpagecandidatestore.moc"
