// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "navigation/imagedocumentpagecandidatedirectoryentry.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {
QUrl directoryUrl()
{
    return QUrl::fromLocalFile(QStringLiteral("/tmp/kiriview-directory-entry-test/"));
}

QUrl fileUrl(const QString& fileName)
{
    return QUrl::fromLocalFile(
        QStringLiteral("/tmp/kiriview-directory-entry-test/%1").arg(fileName));
}

kiriview::ImageDocumentPageCandidate candidate(const QString& fileName)
{
    return kiriview::ImageDocumentPageCandidate {
        fileUrl(fileName),
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
    QUrl watchedUrl;
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback initialSnapshot;
    kiriview::ImageDocumentPageCandidateWatchSnapshotCallback changedSnapshot;
    kiriview::ImageDocumentPageCandidateLoadErrorCallback errorCallback;
    int startCount = 0;

    kiriview::ImageDocumentPageCandidateWatchProvider provider()
    {
        return [this](QObject* receiver, QUrl directory,
                   kiriview::ImageDocumentPageCandidateWatchSnapshotCallback initial,
                   kiriview::ImageDocumentPageCandidateWatchSnapshotCallback changed,
                   kiriview::ImageDocumentPageCandidateLoadErrorCallback error) {
            ++startCount;
            watchedUrl = std::move(directory);
            initialSnapshot = std::move(initial);
            changedSnapshot = std::move(changed);
            errorCallback = std::move(error);
            auto* token = new QObject(receiver);
            return kiriview::ImageIoJob(token, [](QObject* object) { object->deleteLater(); });
        };
    }

    void complete(std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        if (initialSnapshot) {
            initialSnapshot(std::move(candidates));
        }
    }

    void change(std::vector<kiriview::ImageDocumentPageCandidate> candidates)
    {
        if (changedSnapshot) {
            changedSnapshot(std::move(candidates));
        }
    }

    void fail(kiriview::ImageDocumentPageCandidateLoadError error)
    {
        if (errorCallback) {
            errorCallback(std::move(error));
        }
    }
};
}

class TestImageDocumentPageCandidateDirectoryEntry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void providerCompletionPublishesPendingLoad();
    void providerChangesPublishSubscriberUpdates();
    void providerFailurePublishesPendingLoadError();
    void nonKioWatcherFailureRemainsString();
};

void TestImageDocumentPageCandidateDirectoryEntry::providerCompletionPublishesPendingLoad()
{
    FakeWatchProvider provider;
    int errorCount = 0;
    kiriview::ImageDocumentPageCandidateDirectoryEntry entry(
        directoryUrl(), provider.provider(), this);

    std::vector<kiriview::ImageDocumentPageCandidate> loadedCandidates;
    bool loaded = false;
    kiriview::ImageIoJob loadJob = entry.addPendingLoad(
        [&loadedCandidates, &loaded](std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            loadedCandidates = std::move(candidates);
            loaded = true;
        },
        [&errorCount](const kiriview::ImageDocumentPageCandidateLoadError&) { ++errorCount; }, this,
        [](QObject*) {});

    QVERIFY(loadJob.isActive());
    QVERIFY(entry.open());
    QCOMPARE(provider.startCount, 1);
    QCOMPARE(provider.watchedUrl, directoryUrl());

    provider.complete({ candidate(QStringLiteral("01.png")) });
    QVERIFY(loaded);
    QCOMPARE(errorCount, 0);
    QCOMPARE(
        candidateUrls(loadedCandidates), std::vector<QUrl>({ fileUrl(QStringLiteral("01.png")) }));
    QVERIFY(entry.listed());
    QVERIFY(!entry.failed());
}

void TestImageDocumentPageCandidateDirectoryEntry::providerChangesPublishSubscriberUpdates()
{
    FakeWatchProvider provider;
    int errorCount = 0;
    kiriview::ImageDocumentPageCandidateDirectoryEntry entry(
        directoryUrl(), provider.provider(), this);

    bool loaded = false;
    kiriview::ImageIoJob loadJob = entry.addPendingLoad(
        [&loaded](std::vector<kiriview::ImageDocumentPageCandidate>) { loaded = true; },
        [&errorCount](const kiriview::ImageDocumentPageCandidateLoadError&) { ++errorCount; }, this,
        [](QObject*) {});
    QVERIFY(loadJob.isActive());
    QVERIFY(entry.open());
    provider.complete({ candidate(QStringLiteral("01.png")) });
    QVERIFY(loaded);
    QCOMPARE(errorCount, 0);

    std::vector<kiriview::ImageDocumentPageCandidate> changedCandidates;
    int changeCount = 0;
    kiriview::ImageIoJob watchJob = entry.addSubscriber(
        [&changedCandidates, &changeCount](
            std::vector<kiriview::ImageDocumentPageCandidate> candidates) {
            changedCandidates = std::move(candidates);
            ++changeCount;
        },
        [&errorCount](const kiriview::ImageDocumentPageCandidateLoadError&) { ++errorCount; }, this,
        [](QObject*) {});

    QVERIFY(watchJob.isActive());
    provider.change({ candidate(QStringLiteral("01.png")), candidate(QStringLiteral("02.png")) });
    const std::vector<QUrl> expectedAddedUrls {
        fileUrl(QStringLiteral("01.png")),
        fileUrl(QStringLiteral("02.png")),
    };
    QCOMPARE(candidateUrls(changedCandidates), expectedAddedUrls);
    QCOMPARE(changeCount, 1);

    provider.change({ candidate(QStringLiteral("02.png")) });
    const std::vector<QUrl> expectedDeletedUrls { fileUrl(QStringLiteral("02.png")) };
    QCOMPARE(candidateUrls(changedCandidates), expectedDeletedUrls);
    QCOMPARE(changeCount, 2);
}

void TestImageDocumentPageCandidateDirectoryEntry::providerFailurePublishesPendingLoadError()
{
    FakeWatchProvider provider;
    std::optional<kiriview::ImageDocumentPageCandidateLoadError> actualError;
    kiriview::ImageDocumentPageCandidateDirectoryEntry entry(
        directoryUrl(), provider.provider(), this);

    bool loaded = false;
    kiriview::ImageIoJob loadJob = entry.addPendingLoad(
        [&loaded](std::vector<kiriview::ImageDocumentPageCandidate>) { loaded = true; },
        [&actualError](kiriview::ImageDocumentPageCandidateLoadError error) {
            actualError = std::move(error);
        },
        this, [](QObject*) {});

    QVERIFY(loadJob.isActive());
    QVERIFY(entry.open());
    const kiriview::KioOperationFailure expected {
        kiriview::KioOperationKind::DirectoryListing,
        directoryUrl(),
        73,
        true,
        QString(),
        QStringLiteral("listing canceled"),
        false,
    };
    provider.fail(kiriview::ImageDocumentPageCandidateLoadError { expected });

    QVERIFY(!loaded);
    QVERIFY(actualError.has_value());
    const auto* actual = std::get_if<kiriview::KioOperationFailure>(&*actualError);
    QVERIFY(actual != nullptr);
    QCOMPARE(actual->operationKind, expected.operationKind);
    QCOMPARE(actual->targetUrl, expected.targetUrl);
    QCOMPARE(actual->rawErrorCode, expected.rawErrorCode);
    QCOMPARE(actual->canceled, expected.canceled);
    QCOMPARE(actual->userMessage, expected.userMessage);
    QCOMPARE(actual->diagnosticDetail, expected.diagnosticDetail);
    QCOMPARE(actual->retryable, expected.retryable);
    QVERIFY(entry.failed());
    const auto* cached = std::get_if<kiriview::KioOperationFailure>(&entry.error());
    QVERIFY(cached != nullptr);
    QCOMPARE(cached->rawErrorCode, expected.rawErrorCode);
    QVERIFY(cached->canceled);
}

void TestImageDocumentPageCandidateDirectoryEntry::nonKioWatcherFailureRemainsString()
{
    FakeWatchProvider provider;
    kiriview::ImageDocumentPageCandidateDirectoryEntry entry(
        directoryUrl(), provider.provider(), this);
    QVERIFY(entry.open());
    provider.complete({ candidate(QStringLiteral("01.png")) });

    std::optional<kiriview::ImageDocumentPageCandidateLoadError> actualError;
    kiriview::ImageIoJob watchJob
        = entry.addSubscriber([](std::vector<kiriview::ImageDocumentPageCandidate>) {},
            [&actualError](kiriview::ImageDocumentPageCandidateLoadError error) {
                actualError = std::move(error);
            },
            this, [](QObject*) {});

    provider.fail(kiriview::ImageDocumentPageCandidateLoadError {
        QStringLiteral("watch bookkeeping failed") });

    QVERIFY(watchJob.isActive());
    QVERIFY(actualError.has_value());
    const auto* errorString = std::get_if<QString>(&*actualError);
    QVERIFY(errorString != nullptr);
    QCOMPARE(*errorString, QStringLiteral("watch bookkeeping failed"));
}

QTEST_GUILESS_MAIN(TestImageDocumentPageCandidateDirectoryEntry)

#include "tst_imagedocumentpagecandidatedirectoryentry.moc"
