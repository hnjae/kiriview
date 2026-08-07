// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/directorylistingjob.h"

#include <KFileItem>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <optional>
#include <utility>

namespace {
QUrl directoryUrl(const QTemporaryDir& directory)
{
    return QUrl::fromLocalFile(directory.path() + QLatin1Char('/'));
}

bool createFile(const QTemporaryDir& directory, const QString& fileName)
{
    QFile file(directory.filePath(fileName));
    return file.open(QIODevice::WriteOnly);
}

QStringList itemNames(const KFileItemList& items)
{
    QStringList names;
    for (const KFileItem& item : items) {
        names.push_back(item.name());
    }
    names.sort();
    return names;
}
}

class TestDirectoryListingJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void injectedProviderCompletesWithoutFilesystem();
    void injectedProviderPreservesTypedFailure();
    void injectedProviderCancellationSuppressesCompletion();
    void localDirectoryReturnsItemSnapshot();
    void cancelDeactivatesDefaultProviderJob();
    void openUrlFailureLeavesDiagnosticWarning();
    void backendErrorLeavesDiagnosticWarning();
};

void TestDirectoryListingJob::injectedProviderCompletesWithoutFilesystem()
{
    const QUrl requestedUrl = QUrl::fromLocalFile(QStringLiteral("/synthetic/"));
    QUrl providerUrl;
    int providerCallCount = 0;
    kiriview::DirectoryItemListProvider provider
        = [&providerCallCount, &providerUrl](QObject*, QUrl directoryUrl,
              kiriview::DirectoryItemListCallback callback, kiriview::KioOperationFailureCallback) {
              ++providerCallCount;
              providerUrl = std::move(directoryUrl);
              callback({});
              return kiriview::ImageIoJob();
          };

    bool listed = false;
    std::optional<kiriview::KioOperationFailure> failure;
    kiriview::ImageIoJob job = kiriview::startDirectoryItemList(
        this, requestedUrl, [&listed](KFileItemList) { listed = true; },
        [&failure](kiriview::KioOperationFailure error) { failure = std::move(error); },
        std::move(provider));

    QCOMPARE(providerCallCount, 1);
    QCOMPARE(providerUrl, requestedUrl);
    QVERIFY(listed);
    QVERIFY(!failure.has_value());
    QVERIFY(!job.isActive());
}

void TestDirectoryListingJob::injectedProviderPreservesTypedFailure()
{
    const kiriview::KioOperationFailure expected {
        kiriview::KioOperationKind::DirectoryListing,
        QUrl(QStringLiteral("smb://server/share/")),
        73,
        false,
        QStringLiteral("Could not list the folder"),
        QStringLiteral("backend diagnostic"),
        true,
    };
    kiriview::DirectoryItemListProvider provider
        = [expected](QObject*, QUrl, kiriview::DirectoryItemListCallback,
              kiriview::KioOperationFailureCallback errorCallback) {
              errorCallback(expected);
              return kiriview::ImageIoJob();
          };
    std::optional<kiriview::KioOperationFailure> actual;

    kiriview::startDirectoryItemList(
        this, expected.targetUrl, {},
        [&actual](kiriview::KioOperationFailure failure) { actual = std::move(failure); },
        std::move(provider));

    QVERIFY(actual.has_value());
    QCOMPARE(actual->operationKind, expected.operationKind);
    QCOMPARE(actual->targetUrl, expected.targetUrl);
    QCOMPARE(actual->rawErrorCode, expected.rawErrorCode);
    QCOMPARE(actual->canceled, expected.canceled);
    QCOMPARE(actual->userMessage, expected.userMessage);
    QCOMPARE(actual->diagnosticDetail, expected.diagnosticDetail);
    QCOMPARE(actual->retryable, expected.retryable);
}

void TestDirectoryListingJob::injectedProviderCancellationSuppressesCompletion()
{
    kiriview::ImageIoJobCompletion completion;
    kiriview::DirectoryItemListCallback capturedCallback;
    kiriview::DirectoryItemListProvider provider
        = [&completion, &capturedCallback](QObject* receiver, QUrl,
              kiriview::DirectoryItemListCallback callback, kiriview::KioOperationFailureCallback) {
              auto* token = new QObject(receiver);
              kiriview::ImageIoJob job(token, [](QObject* object) { object->deleteLater(); });
              completion = job.completion();
              capturedCallback = std::move(callback);
              return job;
          };

    bool listed = false;
    kiriview::ImageIoJob job = kiriview::startDirectoryItemList(
        this, QUrl::fromLocalFile(QStringLiteral("/synthetic/")),
        [&listed](KFileItemList) { listed = true; }, {}, std::move(provider));

    QVERIFY(job.isActive());
    job.cancel();

    QVERIFY(!completion.claimAndDelete([&capturedCallback]() { capturedCallback({}); }));
    QVERIFY(!listed);
}

void TestDirectoryListingJob::localDirectoryReturnsItemSnapshot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(createFile(directory, QStringLiteral("01.png")));
    QVERIFY(createFile(directory, QStringLiteral("clip.mp4")));

    KFileItemList listedItems;
    std::optional<kiriview::KioOperationFailure> failure;
    bool listed = false;
    kiriview::ImageIoJob job = kiriview::startDirectoryItemList(
        this, directoryUrl(directory),
        [&listedItems, &listed](KFileItemList items) {
            listedItems = std::move(items);
            listed = true;
        },
        [&failure](kiriview::KioOperationFailure error) { failure = std::move(error); });

    QTRY_VERIFY_WITH_TIMEOUT(listed || failure.has_value(), 10000);
    QVERIFY(!failure.has_value());
    QVERIFY(!job.isActive());
    QCOMPARE(itemNames(listedItems),
        QStringList({ QStringLiteral("01.png"), QStringLiteral("clip.mp4") }));
}

void TestDirectoryListingJob::cancelDeactivatesDefaultProviderJob()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(createFile(directory, QStringLiteral("01.png")));

    bool listed = false;
    std::optional<kiriview::KioOperationFailure> failure;
    kiriview::ImageIoJob job = kiriview::startDirectoryItemList(
        this, directoryUrl(directory), [&listed](KFileItemList) { listed = true; },
        [&failure](kiriview::KioOperationFailure error) { failure = std::move(error); });

    QVERIFY(job.isActive());
    job.cancel();
    QVERIFY(!job.isActive());

    QVERIFY(!listed);
    QVERIFY(!failure.has_value());
}

void TestDirectoryListingJob::openUrlFailureLeavesDiagnosticWarning()
{
    bool listed = false;
    std::optional<kiriview::KioOperationFailure> failure;

    QTest::ignoreMessage(QtWarningMsg,
        QRegularExpression(QStringLiteral("KiriView directory listing rejected empty URL")));
    kiriview::ImageIoJob job = kiriview::startDirectoryItemList(
        this, QUrl(), [&listed](KFileItemList) { listed = true; },
        [&failure](kiriview::KioOperationFailure error) { failure = std::move(error); });

    QVERIFY(!job.isActive());
    QVERIFY(!listed);
    QVERIFY(failure.has_value());
    QCOMPARE(failure->operationKind, kiriview::KioOperationKind::DirectoryListing);
    QVERIFY(failure->targetUrl.isEmpty());
    QCOMPARE(failure->rawErrorCode, std::nullopt);
}

void TestDirectoryListingJob::backendErrorLeavesDiagnosticWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl missingDirectoryUrl
        = QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing")) + QLatin1Char('/'));

    bool listed = false;
    bool errorReceived = false;
    std::optional<kiriview::KioOperationFailure> failure;

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(".*KiriView directory listing job failed.*"));
    kiriview::ImageIoJob job = kiriview::startDirectoryItemList(
        this, missingDirectoryUrl, [&listed](KFileItemList) { listed = true; },
        [&errorReceived, &failure](kiriview::KioOperationFailure error) {
            errorReceived = true;
            failure = std::move(error);
        });

    QTRY_VERIFY_WITH_TIMEOUT(errorReceived, 10000);
    QVERIFY(!job.isActive());
    QVERIFY(!listed);
    QVERIFY(failure.has_value());
    QCOMPARE(failure->operationKind, kiriview::KioOperationKind::DirectoryListing);
    QCOMPARE(failure->targetUrl, missingDirectoryUrl);
    QVERIFY(failure->rawErrorCode.has_value());
    QVERIFY(!failure->userMessage.isEmpty());
    QVERIFY(!failure->diagnosticDetail.isEmpty());
}

QTEST_GUILESS_MAIN(TestDirectoryListingJob)

#include "tst_directorylistingjob.moc"
