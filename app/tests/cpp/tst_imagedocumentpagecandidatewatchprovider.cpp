// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidatewatchprovider.h"

#include "image_async_test_support.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {
bool createFile(const QTemporaryDir& directory, const QString& fileName)
{
    QFile file(directory.filePath(fileName));
    return file.open(QIODevice::WriteOnly);
}

QUrl directoryUrl(const QTemporaryDir& directory)
{
    return QUrl::fromLocalFile(directory.path() + QLatin1Char('/'));
}

class ManualDirectoryChanges final
{
public:
    kiriview::ImageDocumentPageCandidateDirectoryChangeProvider provider()
    {
        return [this](QObject* receiver, const QUrl&,
                   kiriview::ImageDocumentPageCandidateDirectoryChangeCallback callback) {
            m_callback = std::move(callback);
            auto* token = new QObject(receiver);
            kiriview::ImageIoJob subscription(
                token, [](QObject* object) { object->deleteLater(); });
            m_completion = subscription.completion();
            return kiriview::ImageDocumentPageCandidateDirectoryChangeSubscription {
                std::move(subscription),
                []() { },
            };
        };
    }

    void notify()
    {
        if (m_completion.isActive() && m_callback) {
            m_callback();
        }
    }

    void deliverIgnoringCancellation()
    {
        if (m_callback) {
            m_callback();
        }
    }

    [[nodiscard]] bool active() const { return m_completion.isActive(); }

private:
    kiriview::ImageDocumentPageCandidateDirectoryChangeCallback m_callback;
    kiriview::ImageIoJobCompletion m_completion;
};

class ManualDirectoryListings final
{
public:
    struct Listing
    {
        kiriview::DirectoryItemListCallback callback;
        kiriview::KioOperationFailureCallback errorCallback;
        kiriview::ImageIoJobCompletion completion;
    };

    kiriview::DirectoryItemListProvider provider()
    {
        return [this](QObject* receiver, QUrl, kiriview::DirectoryItemListCallback callback,
                   kiriview::KioOperationFailureCallback errorCallback) {
            auto* token = new QObject(receiver);
            kiriview::ImageIoJob job(token, [](QObject* object) { object->deleteLater(); });
            m_listings.push_back(Listing {
                std::move(callback),
                std::move(errorCallback),
                job.completion(),
            });
            return job;
        };
    }

    [[nodiscard]] std::size_t count() const { return m_listings.size(); }
    [[nodiscard]] bool active(std::size_t index) const
    {
        return m_listings.at(index).completion.isActive();
    }

    void complete(std::size_t index, kiriview::DirectoryItemList items)
    {
        Listing& listing = m_listings.at(index);
        listing.completion.claimAndDelete([&]() { listing.callback(std::move(items)); });
    }

    void deliverIgnoringCancellation(std::size_t index, kiriview::DirectoryItemList items)
    {
        Listing& listing = m_listings.at(index);
        listing.callback(std::move(items));
    }

private:
    std::vector<Listing> m_listings;
};

kiriview::DirectoryItem imageItem(const QUrl& directoryUrl, const QString& name)
{
    return kiriview::DirectoryItem { directoryUrl.resolved(QUrl(name)), name, true };
}
}

class TestImageDocumentPageCandidateWatchProvider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void overLimitInitialSnapshotRecoversThroughBoundedRelisting();
    void nonLocalSnapshotRetiresAfterInitialBoundedAcquisition();
    void changeBurstsDuringListingScheduleOneFollowupRefresh();
    void cancellationRevokesScheduledAndLateRefreshWork();
    void initialCallbackCanCancelLocalWatchReentrantly();
    void initialCallbackCanDestroyReceiverReentrantly();
};

void TestImageDocumentPageCandidateWatchProvider::
    overLimitInitialSnapshotRecoversThroughBoundedRelisting()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(createFile(directory, QStringLiteral("01.png")));
    QVERIFY(createFile(directory, QStringLiteral("02.png")));

    kiriview::ImageDocumentPageCandidateWatchProvider provider
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(
            kiriview::ImageDocumentPageCandidateWatchDependencies {},
            kiriview::SiblingCandidateAdmissionLimits { 1, 1'024 });
    int initialSnapshotCount = 0;
    int changedSnapshotCount = 0;
    std::vector<kiriview::ImageDocumentPageCandidate> recoveredCandidates;
    std::optional<kiriview::ImageDocumentPageCandidateLoadError> failure;

    kiriview::ImageIoJob watch = provider(
        this, directoryUrl(directory),
        [&initialSnapshotCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++initialSnapshotCount; },
        [&changedSnapshotCount, &recoveredCandidates](
            std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            ++changedSnapshotCount;
            recoveredCandidates = std::move(candidates);
        },
        [&failure](
            kiriview::ImageDocumentPageCandidateLoadError error) { failure = std::move(error); });

    QTRY_VERIFY_WITH_TIMEOUT(failure.has_value(), 10'000);
    QCOMPARE(initialSnapshotCount, 0);
    QVERIFY(watch.isActive());
    QVERIFY(std::holds_alternative<kiriview::KioOperationFailure>(*failure));
    QCOMPARE(std::get<kiriview::KioOperationFailure>(*failure).cause,
        kiriview::KioOperationFailureCause::ResourceLimitExceeded);

    QVERIFY(QFile::remove(directory.filePath(QStringLiteral("02.png"))));
    QTRY_VERIFY_WITH_TIMEOUT(changedSnapshotCount > 0, 10'000);
    QCOMPARE(recoveredCandidates.size(), std::size_t(1));
    QCOMPARE(recoveredCandidates.front().url,
        QUrl::fromLocalFile(directory.filePath(QStringLiteral("01.png"))));
    QVERIFY(watch.isActive());

    watch.cancel();
    QVERIFY(!watch.isActive());
}

void TestImageDocumentPageCandidateWatchProvider::
    nonLocalSnapshotRetiresAfterInitialBoundedAcquisition()
{
    const QUrl directoryUrl(QStringLiteral("smb://server.example/media/"));
    int listingCount = 0;
    kiriview::DirectoryItemListProvider listingProvider
        = [&listingCount](QObject*, QUrl, kiriview::DirectoryItemListCallback callback,
              kiriview::KioOperationFailureCallback) {
              ++listingCount;
              callback({ kiriview::DirectoryItem {
                  QUrl(QStringLiteral("smb://server.example/media/01.png")),
                  QStringLiteral("01.png"), true } });
              return kiriview::ImageIoJob {};
          };
    kiriview::ImageDocumentPageCandidateWatchProvider provider
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(
            kiriview::ImageDocumentPageCandidateWatchDependencies {
                std::move(listingProvider),
            });
    int initialSnapshotCount = 0;

    kiriview::ImageIoJob snapshot = provider(this, directoryUrl,
        [&initialSnapshotCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++initialSnapshotCount; },
        {}, {});

    QVERIFY(snapshot.isActive());
    QTRY_COMPARE_WITH_TIMEOUT(initialSnapshotCount, 1, 10'000);
    QVERIFY(!snapshot.isActive());
    QCOMPARE(listingCount, 1);
}

void TestImageDocumentPageCandidateWatchProvider::
    changeBurstsDuringListingScheduleOneFollowupRefresh()
{
    const QUrl watchedDirectory(QStringLiteral("file:///media/"));
    ManualDirectoryChanges changes;
    ManualDirectoryListings listings;
    kiriview::TestSupport::ManualTimerScheduler timers;
    kiriview::ImageDocumentPageCandidateWatchDependencies dependencies {
        listings.provider(),
        changes.provider(),
        timers.scheduler(),
    };
    kiriview::ImageDocumentPageCandidateWatchProvider provider
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(std::move(dependencies));
    int initialSnapshotCount = 0;
    int changedSnapshotCount = 0;

    kiriview::ImageIoJob watch = provider(
        this, watchedDirectory,
        [&initialSnapshotCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++initialSnapshotCount; },
        [&changedSnapshotCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++changedSnapshotCount; },
        {});

    QCOMPARE(timers.timerCount(), std::size_t(1));
    QVERIFY(timers.timerAt(0).active());
    QCOMPARE(listings.count(), std::size_t(0));

    timers.timerAt(0).fire();
    QCOMPARE(listings.count(), std::size_t(1));
    QVERIFY(listings.active(0));

    changes.notify();
    changes.notify();
    changes.notify();
    QCOMPARE(listings.count(), std::size_t(1));
    QVERIFY(!timers.timerAt(0).active());

    listings.complete(0, { imageItem(watchedDirectory, QStringLiteral("01.png")) });
    QCOMPARE(initialSnapshotCount, 1);
    QCOMPARE(changedSnapshotCount, 0);
    QVERIFY(timers.timerAt(0).active());

    timers.timerAt(0).fire();
    QCOMPARE(listings.count(), std::size_t(2));
    listings.complete(1, { imageItem(watchedDirectory, QStringLiteral("02.png")) });

    QCOMPARE(initialSnapshotCount, 1);
    QCOMPARE(changedSnapshotCount, 1);
    QVERIFY(!timers.timerAt(0).active());
    QCOMPARE(listings.count(), std::size_t(2));
    QVERIFY(watch.isActive());
}

void TestImageDocumentPageCandidateWatchProvider::cancellationRevokesScheduledAndLateRefreshWork()
{
    const QUrl watchedDirectory(QStringLiteral("file:///media/"));
    ManualDirectoryChanges changes;
    ManualDirectoryListings listings;
    kiriview::TestSupport::ManualTimerScheduler timers;
    kiriview::ImageDocumentPageCandidateWatchDependencies dependencies {
        listings.provider(),
        changes.provider(),
        timers.scheduler(),
    };
    kiriview::ImageDocumentPageCandidateWatchProvider provider
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(std::move(dependencies));
    int snapshotCount = 0;
    int failureCount = 0;

    kiriview::ImageIoJob watch = provider(
        this, watchedDirectory,
        [&snapshotCount](std::vector<kiriview::ImageDocumentPageCandidate>) { ++snapshotCount; },
        [&snapshotCount](std::vector<kiriview::ImageDocumentPageCandidate>) { ++snapshotCount; },
        [&failureCount](kiriview::ImageDocumentPageCandidateLoadError) { ++failureCount; });

    timers.timerAt(0).fire();
    QCOMPARE(listings.count(), std::size_t(1));
    changes.notify();
    watch.cancel();

    QVERIFY(!watch.isActive());
    QVERIFY(!changes.active());
    QVERIFY(!listings.active(0));
    QVERIFY(!timers.timerAt(0).active());

    listings.deliverIgnoringCancellation(
        0, { imageItem(watchedDirectory, QStringLiteral("late.png")) });
    timers.timerAt(0).fire();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    changes.deliverIgnoringCancellation();

    QCOMPARE(snapshotCount, 0);
    QCOMPARE(failureCount, 0);
    QCOMPARE(listings.count(), std::size_t(1));
}

void TestImageDocumentPageCandidateWatchProvider::initialCallbackCanCancelLocalWatchReentrantly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl itemUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("01.png")));
    kiriview::DirectoryItemListProvider listingProvider
        = [itemUrl](QObject*, QUrl, kiriview::DirectoryItemListCallback callback,
              kiriview::KioOperationFailureCallback) {
              callback({ kiriview::DirectoryItem { itemUrl, QStringLiteral("01.png"), true } });
              return kiriview::ImageIoJob {};
          };
    kiriview::ImageDocumentPageCandidateWatchProvider provider
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(
            kiriview::ImageDocumentPageCandidateWatchDependencies {
                std::move(listingProvider),
            });
    int initialSnapshotCount = 0;
    bool callbackContinuedAfterCancel = false;
    kiriview::ImageIoJob watch;
    auto callbackState = std::make_shared<bool>(false);

    watch = provider(this, directoryUrl(directory),
        [&watch, &initialSnapshotCount, &callbackContinuedAfterCancel,
            callbackState = std::move(callbackState)](
            std::vector<kiriview::ImageDocumentPageCandidate>) {
            ++initialSnapshotCount;
            watch.cancel();
            *callbackState = true;
            callbackContinuedAfterCancel = *callbackState;
        },
        {}, {});

    QTRY_COMPARE_WITH_TIMEOUT(initialSnapshotCount, 1, 10'000);
    QVERIFY(callbackContinuedAfterCancel);
    QVERIFY(!watch.isActive());
}

void TestImageDocumentPageCandidateWatchProvider::initialCallbackCanDestroyReceiverReentrantly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl itemUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("01.png")));
    kiriview::DirectoryItemListProvider listingProvider
        = [itemUrl](QObject*, QUrl, kiriview::DirectoryItemListCallback callback,
              kiriview::KioOperationFailureCallback) {
              callback({ kiriview::DirectoryItem { itemUrl, QStringLiteral("01.png"), true } });
              return kiriview::ImageIoJob {};
          };
    kiriview::ImageDocumentPageCandidateWatchProvider provider
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(
            kiriview::ImageDocumentPageCandidateWatchDependencies {
                std::move(listingProvider),
            });
    auto* receiver = new QObject;
    const QPointer<QObject> guardedReceiver(receiver);
    int initialSnapshotCount = 0;

    kiriview::ImageIoJob watch = provider(receiver, directoryUrl(directory),
        [&receiver, &initialSnapshotCount](std::vector<kiriview::ImageDocumentPageCandidate>) {
            ++initialSnapshotCount;
            delete receiver;
            receiver = nullptr;
        },
        {}, {});

    QTRY_COMPARE_WITH_TIMEOUT(initialSnapshotCount, 1, 10'000);
    QVERIFY(guardedReceiver.isNull());
    QVERIFY(!watch.isActive());
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateWatchProvider)

#include "tst_imagedocumentpagecandidatewatchprovider.moc"
