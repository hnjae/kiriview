// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidatewatchprovider.h"

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
}

class TestImageDocumentPageCandidateWatchProvider : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void overLimitInitialSnapshotRecoversThroughBoundedRelisting();
    void nonLocalSnapshotRetiresAfterInitialBoundedAcquisition();
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
            {}, kiriview::SiblingCandidateAdmissionLimits { 1, 1'024 });
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
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(std::move(listingProvider));
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
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(std::move(listingProvider));
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
        = kiriview::defaultImageDocumentPageCandidateWatchProvider(std::move(listingProvider));
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
