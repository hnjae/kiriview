// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedecodejob.h"
#include "image_test_support.h"
#include "location/imageurl.h"
#include "predecode/predecodeactivedecodestore.h"

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QTest>
#include <optional>
#include <vector>

namespace {
using kiriview::TestSupport::imageDecodeDependenciesFor;
using kiriview::TestSupport::indexedImageUrl;
using kiriview::TestSupport::ManualImageDataLoader;
using kiriview::TestSupport::staticImageDataDecoder;

kiriview::ImageDecodeRequest decodeRequest(quint64 id, const QUrl& url)
{
    return kiriview::ImageDecodeRequest::fromUrl(id, url);
}

kiriview::ImageDecodeRequest decodeRequest(
    quint64 id, const QUrl& url, const kiriview::ImageSourceRevision& revision)
{
    return decodeRequest(id, url).withSourceRevision(revision);
}

kiriview::DisplayedImageLocation displayedLocation(const QUrl& url)
{
    return kiriview::DisplayedImageLocation::fromUrl(url);
}

kiriview::PredecodeWorkKey workKey(const kiriview::ImageDecodeRequest& request,
    kiriview::PredecodeWorkScope scope = kiriview::PredecodeWorkScope::scheduleFallback(7))
{
    return {
        kiriview::PredecodeImageKey { request.location(), request.sourceRevision() },
        scope,
    };
}

void sendDeferredDeletes() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }
}

class TestPredecodeActiveDecodeStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addRejectsDuplicateOrInvalidRequests();
    void activeWorkDistinguishesSourceRevision();
    void unknownRevisionWorkUsesStableScopeInsteadOfGeneration();
    void logicalCompletionRetainsSlotAndJobUntilRetirement();
    void logicalCompletionRejectsStaleGenerationAndUrl();
    void retirementRemovesOnlyMatchingEntry();
    void retiringEntryIsSameLocationWildcard();
    void cancelCancelsDecodeJobsAndRetainsStoreUntilRetirement();
};

void TestPredecodeActiveDecodeStore::addRejectsDuplicateOrInvalidRequests()
{
    kiriview::PredecodeActiveDecodeStore store;
    auto* firstJob = new kiriview::ImageDecodeJob(this);
    auto* duplicateJob = new kiriview::ImageDecodeJob(this);
    auto* unscopedJob = new kiriview::ImageDecodeJob(this);
    const QUrl url = indexedImageUrl(1);

    const kiriview::ImageDecodeRequest request = decodeRequest(7, url);
    QVERIFY(store.add(request, workKey(request), firstJob));
    QVERIFY(!store.add(request, workKey(request), duplicateJob));
    const kiriview::ImageDecodeRequest unscopedRequest = decodeRequest(8, url);
    QVERIFY(!store.add(unscopedRequest,
        kiriview::PredecodeWorkKey {
            kiriview::PredecodeImageKey {
                unscopedRequest.location(), unscopedRequest.sourceRevision() },
            {},
        },
        unscopedJob));
    QVERIFY(!store.add(kiriview::ImageDecodeRequest {}, workKey(request), firstJob));
    const kiriview::ImageDecodeRequest missingJobRequest = decodeRequest(9, indexedImageUrl(2));
    QVERIFY(!store.add(missingJobRequest, workKey(missingJobRequest), nullptr));
    QCOMPARE(store.size(), std::size_t(1));

    duplicateJob->deleteLater();
    unscopedJob->deleteLater();
    store.cancel();
    sendDeferredDeletes();
}

void TestPredecodeActiveDecodeStore::activeWorkDistinguishesSourceRevision()
{
    kiriview::PredecodeActiveDecodeStore store;
    auto* olderJob = new kiriview::ImageDecodeJob(this);
    auto* newerJob = new kiriview::ImageDecodeJob(this);
    auto* duplicateJob = new kiriview::ImageDecodeJob(this);
    const QUrl url = indexedImageUrl(1);
    const kiriview::ImageSourceRevision olderRevision
        = kiriview::ImageSourceRevision::fromData(QByteArrayView("older"));
    const kiriview::ImageSourceRevision newerRevision
        = kiriview::ImageSourceRevision::fromData(QByteArrayView("newer"));
    const kiriview::ImageDecodeRequest older = decodeRequest(7, url, olderRevision);
    const kiriview::ImageDecodeRequest newer = decodeRequest(8, url, newerRevision);

    QVERIFY(store.add(older, workKey(older), olderJob));
    QVERIFY(store.add(newer, workKey(newer), newerJob));
    const kiriview::ImageDecodeRequest duplicate = decodeRequest(9, url, olderRevision);
    QVERIFY(!store.add(duplicate, workKey(duplicate), duplicateJob));
    QCOMPARE(store.size(), std::size_t(2));

    const kiriview::PredecodeActiveLoads active = store.activeLoads();
    QVERIFY(active.contains(kiriview::PredecodeImageKey { older.location(), olderRevision }));
    QVERIFY(active.contains(kiriview::PredecodeImageKey { newer.location(), newerRevision }));

    duplicateJob->deleteLater();
    store.cancel();
    sendDeferredDeletes();
}

void TestPredecodeActiveDecodeStore::unknownRevisionWorkUsesStableScopeInsteadOfGeneration()
{
    kiriview::PredecodeActiveDecodeStore store;
    auto* firstJob = new kiriview::ImageDecodeJob(this);
    auto* duplicateJob = new kiriview::ImageDecodeJob(this);
    auto* changedSnapshotJob = new kiriview::ImageDecodeJob(this);
    auto* fallbackJob = new kiriview::ImageDecodeJob(this);
    const QUrl url = indexedImageUrl(1);
    const kiriview::PredecodeWorkScope firstSnapshot
        = kiriview::PredecodeWorkScope::candidateSnapshot(41);
    const kiriview::PredecodeWorkScope changedSnapshot
        = kiriview::PredecodeWorkScope::candidateSnapshot(42);

    const kiriview::ImageDecodeRequest first = decodeRequest(7, url);
    const kiriview::ImageDecodeRequest newGeneration = decodeRequest(8, url);
    QVERIFY(store.add(first, workKey(first, firstSnapshot), firstJob));
    QVERIFY(!store.add(newGeneration, workKey(newGeneration, firstSnapshot), duplicateJob));
    QVERIFY(store.add(newGeneration, workKey(newGeneration, changedSnapshot), changedSnapshotJob));
    const kiriview::ImageDecodeRequest fallbackGeneration = decodeRequest(9, url);
    QVERIFY(store.add(fallbackGeneration,
        workKey(fallbackGeneration, kiriview::PredecodeWorkScope::scheduleFallback(41)),
        fallbackJob));
    QCOMPARE(store.size(), std::size_t(3));

    duplicateJob->deleteLater();
    store.cancel();
    sendDeferredDeletes();
}

void TestPredecodeActiveDecodeStore::logicalCompletionRetainsSlotAndJobUntilRetirement()
{
    kiriview::PredecodeActiveDecodeStore store;
    auto* job = new kiriview::ImageDecodeJob(this);
    QPointer<kiriview::ImageDecodeJob> jobGuard(job);
    const QUrl url = indexedImageUrl(1);

    const kiriview::ImageDecodeRequest request = decodeRequest(7, url);
    QVERIFY(store.add(request, workKey(request), job));

    QCOMPARE(store.size(), std::size_t(1));
    const kiriview::PredecodeActiveLoads activeLoads = store.activeLoads();
    QCOMPARE(activeLoads.size(), std::size_t(1));
    QVERIFY(activeLoads.contains(workKey(request)));
    QVERIFY(activeLoads.contains(kiriview::PredecodeWorkKey {
        kiriview::PredecodeImageKey {
            displayedLocation(kiriview::normalizedImageUrl(url)),
            {},
        },
        kiriview::PredecodeWorkScope::scheduleFallback(7),
    }));

    const std::optional<kiriview::PredecodeRetiringDecode> finished
        = store.beginRetirement(decodeRequest(7, url));

    QVERIFY(finished.has_value());
    QCOMPARE(finished->request.id(), quint64(7));
    QCOMPARE(finished->request.imageUrl(), url);
    QVERIFY(kiriview::samePredecodeWork(finished->workKey, workKey(request)));
    QCOMPARE(store.size(), std::size_t(1));
    QVERIFY(!jobGuard.isNull());
    QVERIFY(!store.beginRetirement(decodeRequest(7, url)).has_value());

    QVERIFY(store.retire(decodeRequest(7, url)));
    QCOMPARE(store.size(), std::size_t(0));

    sendDeferredDeletes();
    QVERIFY(jobGuard.isNull());
}

void TestPredecodeActiveDecodeStore::logicalCompletionRejectsStaleGenerationAndUrl()
{
    kiriview::PredecodeActiveDecodeStore store;
    auto* job = new kiriview::ImageDecodeJob(this);
    const QUrl url = indexedImageUrl(1);
    const QUrl otherUrl = indexedImageUrl(2);

    const kiriview::ImageDecodeRequest request = decodeRequest(7, url);
    QVERIFY(store.add(request, workKey(request), job));

    QVERIFY(!store.beginRetirement(decodeRequest(8, url)).has_value());
    QVERIFY(!store.beginRetirement(decodeRequest(7, otherUrl)).has_value());
    QCOMPARE(store.size(), std::size_t(1));

    QVERIFY(store.beginRetirement(decodeRequest(7, url)).has_value());
    QVERIFY(store.retire(decodeRequest(7, url)));
    sendDeferredDeletes();
}

void TestPredecodeActiveDecodeStore::retirementRemovesOnlyMatchingEntry()
{
    kiriview::PredecodeActiveDecodeStore store;
    auto* firstJob = new kiriview::ImageDecodeJob(this);
    auto* secondJob = new kiriview::ImageDecodeJob(this);
    QPointer<kiriview::ImageDecodeJob> firstJobGuard(firstJob);
    QPointer<kiriview::ImageDecodeJob> secondJobGuard(secondJob);
    const QUrl firstUrl = indexedImageUrl(1);
    const QUrl secondUrl = indexedImageUrl(2);

    const kiriview::ImageDecodeRequest first = decodeRequest(7, firstUrl);
    const kiriview::ImageDecodeRequest second = decodeRequest(7, secondUrl);
    QVERIFY(store.add(first, workKey(first), firstJob));
    QVERIFY(store.add(second, workKey(second), secondJob));

    QVERIFY(store.beginRetirement(decodeRequest(7, firstUrl)).has_value());
    QVERIFY(store.retire(decodeRequest(7, firstUrl)));

    QCOMPARE(store.size(), std::size_t(1));
    const kiriview::PredecodeActiveLoads active = store.activeLoads();
    QVERIFY(!active.contains(workKey(decodeRequest(7, firstUrl))));
    QVERIFY(active.contains(workKey(decodeRequest(7, secondUrl))));
    sendDeferredDeletes();
    QVERIFY(firstJobGuard.isNull());
    QVERIFY(!secondJobGuard.isNull());

    store.cancel();
    QVERIFY(store.retire(second));
    sendDeferredDeletes();
    QVERIFY(secondJobGuard.isNull());
}

void TestPredecodeActiveDecodeStore::retiringEntryIsSameLocationWildcard()
{
    kiriview::PredecodeActiveDecodeStore store;
    auto* job = new kiriview::ImageDecodeJob(this);
    const QUrl url = indexedImageUrl(1);
    const kiriview::ImageDecodeRequest request = decodeRequest(7, url);
    const kiriview::PredecodeWorkKey publishingKey
        = workKey(request, kiriview::PredecodeWorkScope::candidateSnapshot(41));
    const kiriview::PredecodeWorkKey changedSnapshotKey
        = workKey(decodeRequest(8, url), kiriview::PredecodeWorkScope::candidateSnapshot(42));

    QVERIFY(store.add(request, publishingKey, job));
    QVERIFY(!store.activeLoads().contains(changedSnapshotKey));

    QVERIFY(store.beginRetirement(request).has_value());
    QCOMPARE(store.activeLoads().size(), std::size_t(1));
    QVERIFY(store.activeLoads().contains(changedSnapshotKey));

    QVERIFY(store.retire(request));
    sendDeferredDeletes();
}

void TestPredecodeActiveDecodeStore::cancelCancelsDecodeJobsAndRetainsStoreUntilRetirement()
{
    ManualImageDataLoader dataLoader;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder());
    kiriview::PredecodeActiveDecodeStore store;
    auto* job = new kiriview::ImageDecodeJob(this, dependencies);
    QPointer<kiriview::ImageDecodeJob> jobGuard(job);
    const kiriview::ImageDecodeRequest request = decodeRequest(7, indexedImageUrl(1));

    job->start(request);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QVERIFY(store.add(request, workKey(request), job));

    store.cancel();

    QCOMPARE(store.size(), std::size_t(1));
    QVERIFY(dataLoader.frontLoad().canceled);
    QVERIFY(!jobGuard.isNull());

    store.cancel();
    QCOMPARE(store.size(), std::size_t(1));
    QVERIFY(store.retire(request));
    sendDeferredDeletes();
    QVERIFY(jobGuard.isNull());
}

QTEST_GUILESS_MAIN(TestPredecodeActiveDecodeStore)

#include "tst_predecodeactivedecodestore.moc"
