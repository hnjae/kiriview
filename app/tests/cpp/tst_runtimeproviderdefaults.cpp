// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "async/directorylistingjob.h"
#include "async/imageiojob.h"
#include "async/imageworkerscheduler.h"
#include "candidate_test_support.h"
#include "decoding/imagedecodedependencies.h"
#include "decoding/imagedecodeworkspace.h"
#include "document/imagedocumentruntimedependencies.h"
#include "location/imagedocumentlocation.h"
#include "navigation/directmedianavigationcandidateprovider.h"
#include "navigation/imagedocumentpagecandidateitems.h"
#include "navigation/imagedocumentpagecandidateloading.h"
#include "navigation/imagedocumentpagecandidateprovider.h"
#include "system/filedeletion.h"
#include "system/powersaverprovider.h"

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::localUrl;

QByteArray fixtureData(const QString& fileName)
{
    QFile file(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/") + fileName);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

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

class TestRuntimeProviderDefaults : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void candidateProviderDefaultsFillMissingLoadersAndPreserveOverrides();
    void candidateProviderDefaultsBindContainerLoaderToDirectoryProvider();
    void candidateLoadingPreservesTypedDirectoryFailure();
    void siblingProvidersRejectOverLimitListingsWithTypedFailure();
    void candidateProviderDefaultsBindOpenedCollectionLoaderToWorkerScheduler();
    void directMediaProviderDefaultBindsDirectoryProvider();
    void directMediaProviderProductionDefaultOffersLiveChanges();
    void directMediaProviderCustomLoaderDoesNotGainAProductionWatch();
    void directMediaProviderDefaultPreservesTypedDirectoryFailure();
    void imageDocumentRuntimeDependenciesBindContainerLoaderToDirectoryProvider();
    void imageDocumentRuntimeDependenciesBindMediaEntryStoreToWorkerScheduler();
    void decodeDependencyDefaultsFillMissingFunctionsAndPreserveOverrides();
    void decodeDependencyDefaultsBindPlannerToWorkspaceBudget();
    void decodeDependencyDefaultsBindDataLoaderToWorkerScheduler();
    void decodeDependencyDefaultsBindThumbnailLookupToWorkerScheduler();
    void fileDeletionDefaultFillsMissingProviderAndPreservesOverride();
};

void TestRuntimeProviderDefaults::candidateProviderDefaultsFillMissingLoadersAndPreserveOverrides()
{
    int directoryLoadCount = 0;
    int directoryChangeSubscriptionCount = 0;
    kiriview::ImageDocumentPageCandidateProvider provider;
    provider.directoryImageDocumentPages
        = [&directoryLoadCount](QObject*, QUrl, kiriview::ImageDocumentPageCandidatesCallback,
              kiriview::ImageDocumentPageCandidateLoadErrorCallback) {
              ++directoryLoadCount;
              return kiriview::ImageIoJob();
          };
    provider.directoryImageDocumentPageChanges
        = [&directoryChangeSubscriptionCount](QObject*, QUrl,
              kiriview::ImageDocumentPageCandidatesCallback,
              kiriview::ImageDocumentPageCandidateLoadErrorCallback) {
              ++directoryChangeSubscriptionCount;
              return kiriview::ImageIoJob();
          };

    kiriview::ImageDocumentPageCandidateProvider resolved
        = kiriview::imageDocumentPageNavigationCandidateProviderWithDefaults(std::move(provider));

    QVERIFY(resolved.directoryImageDocumentPages);
    QVERIFY(resolved.directoryContainers);
    QVERIFY(resolved.openedCollectionCandidates);
    QVERIFY(resolved.directoryImageDocumentPageChanges);

    resolved.directoryImageDocumentPages(nullptr, QUrl(), {}, {});
    resolved.directoryImageDocumentPageChanges(nullptr, QUrl(), {}, {});
    QCOMPARE(directoryLoadCount, 1);
    QCOMPARE(directoryChangeSubscriptionCount, 1);
}

void TestRuntimeProviderDefaults::candidateProviderDefaultsBindContainerLoaderToDirectoryProvider()
{
    const QUrl requestedUrl = localUrl(QStringLiteral("/containers/"));
    QUrl providerUrl;
    int providerCallCount = 0;
    kiriview::DirectoryItemListProvider directoryItemListProvider
        = [&providerCallCount, &providerUrl](QObject*, QUrl directoryUrl,
              kiriview::DirectoryItemListCallback callback, kiriview::KioOperationFailureCallback) {
              ++providerCallCount;
              providerUrl = std::move(directoryUrl);
              callback({});
              return kiriview::ImageIoJob();
          };

    kiriview::ImageDocumentPageCandidateProvider resolved
        = kiriview::imageDocumentPageNavigationCandidateProviderWithDefaults(
            kiriview::ImageDocumentPageCandidateProvider {}, {},
            std::move(directoryItemListProvider));
    QVERIFY(resolved.directoryContainers);

    int callbackCount = 0;
    int errorCallbackCount = 0;
    resolved.directoryContainers(
        this, requestedUrl,
        [&callbackCount](std::vector<kiriview::ContainerNavigationCandidate>) { ++callbackCount; },
        [&errorCallbackCount](kiriview::KioOperationFailure) { ++errorCallbackCount; });

    QCOMPARE(providerCallCount, 1);
    QCOMPARE(providerUrl, requestedUrl);
    QCOMPARE(callbackCount, 1);
    QCOMPARE(errorCallbackCount, 0);
}

void TestRuntimeProviderDefaults::candidateLoadingPreservesTypedDirectoryFailure()
{
    const kiriview::KioOperationFailure expected {
        kiriview::KioOperationKind::DirectoryListing,
        localUrl(QStringLiteral("/containers/")),
        73,
        true,
        QString(),
        QStringLiteral("listing canceled"),
        false,
    };
    kiriview::DirectoryItemListProvider directoryItemListProvider
        = [expected](QObject*, QUrl, kiriview::DirectoryItemListCallback,
              kiriview::KioOperationFailureCallback errorCallback) {
              errorCallback(expected);
              return kiriview::ImageIoJob();
          };
    std::optional<kiriview::KioOperationFailure> pageFailure;
    std::optional<kiriview::KioOperationFailure> containerFailure;

    kiriview::startDirectoryImageDocumentPageCandidateList(
        this, expected.targetUrl, {},
        [&pageFailure](kiriview::KioOperationFailure failure) { pageFailure = std::move(failure); },
        directoryItemListProvider);
    kiriview::startDirectoryContainerCandidateList(
        this, expected.targetUrl, {},
        [&containerFailure](
            kiriview::KioOperationFailure failure) { containerFailure = std::move(failure); },
        std::move(directoryItemListProvider));

    QVERIFY(pageFailure.has_value());
    QVERIFY(containerFailure.has_value());
    for (const kiriview::KioOperationFailure* actual : { &*pageFailure, &*containerFailure }) {
        QCOMPARE(actual->operationKind, expected.operationKind);
        QCOMPARE(actual->targetUrl, expected.targetUrl);
        QCOMPARE(actual->rawErrorCode, expected.rawErrorCode);
        QCOMPARE(actual->canceled, expected.canceled);
        QCOMPARE(actual->userMessage, expected.userMessage);
        QCOMPARE(actual->diagnosticDetail, expected.diagnosticDetail);
        QCOMPARE(actual->retryable, expected.retryable);
    }
}

void TestRuntimeProviderDefaults::siblingProvidersRejectOverLimitListingsWithTypedFailure()
{
    const QUrl directoryUrl = localUrl(QStringLiteral("/media/"));
    const kiriview::DirectoryItem repeatedItem { localUrl(QStringLiteral("/media/01.png")),
        QStringLiteral("01.png"), true };
    const qsizetype itemCount
        = kiriview::defaultSiblingCandidateAdmissionLimits().maximumEntryCount + 1;
    kiriview::DirectoryItemListProvider directoryItemListProvider
        = [repeatedItem, itemCount](QObject*, QUrl, kiriview::DirectoryItemListCallback callback,
              kiriview::KioOperationFailureCallback) {
              kiriview::DirectoryItemList items;
              items.reserve(itemCount);
              for (qsizetype index = 0; index < itemCount; ++index) {
                  items.push_back(repeatedItem);
              }
              callback(std::move(items));
              return kiriview::ImageIoJob();
          };
    int pageCandidateCount = 0;
    int directCandidateCount = 0;
    int containerCandidateCount = 0;
    std::optional<kiriview::KioOperationFailure> pageFailure;
    std::optional<kiriview::KioOperationFailure> directFailure;
    std::optional<kiriview::KioOperationFailure> containerFailure;

    kiriview::startDirectoryImageDocumentPageCandidateList(
        this, directoryUrl,
        [&pageCandidateCount](
            std::vector<kiriview::ImageDocumentPageCandidate>) { ++pageCandidateCount; },
        [&pageFailure](kiriview::KioOperationFailure failure) { pageFailure = std::move(failure); },
        directoryItemListProvider);
    kiriview::startDirectoryContainerCandidateList(
        this, directoryUrl,
        [&containerCandidateCount](
            std::vector<kiriview::ContainerNavigationCandidate>) { ++containerCandidateCount; },
        [&containerFailure](
            kiriview::KioOperationFailure failure) { containerFailure = std::move(failure); },
        directoryItemListProvider);
    kiriview::DirectMediaNavigationCandidateProvider directProvider
        = kiriview::defaultDirectMediaNavigationCandidateProvider(
            std::move(directoryItemListProvider));
    directProvider.directoryCandidateLoader(
        this, directoryUrl,
        [&directCandidateCount](
            std::vector<kiriview::DirectMediaNavigationCandidate>) { ++directCandidateCount; },
        [&directFailure](
            kiriview::KioOperationFailure failure) { directFailure = std::move(failure); });

    QCOMPARE(pageCandidateCount, 0);
    QCOMPARE(directCandidateCount, 0);
    QCOMPARE(containerCandidateCount, 0);
    QVERIFY(pageFailure.has_value());
    QVERIFY(directFailure.has_value());
    QVERIFY(containerFailure.has_value());
    for (const kiriview::KioOperationFailure* failure :
        { &*pageFailure, &*directFailure, &*containerFailure }) {
        QCOMPARE(failure->operationKind, kiriview::KioOperationKind::DirectoryListing);
        QCOMPARE(failure->targetUrl, directoryUrl);
        QCOMPARE(failure->cause, kiriview::KioOperationFailureCause::ResourceLimitExceeded);
        QVERIFY(!failure->rawErrorCode.has_value());
    }
}

void TestRuntimeProviderDefaults::
    candidateProviderDefaultsBindOpenedCollectionLoaderToWorkerScheduler()
{
    ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDocumentPageCandidateProvider resolved
        = kiriview::imageDocumentPageNavigationCandidateProviderWithDefaults(
            kiriview::ImageDocumentPageCandidateProvider {}, workerScheduler.scheduler());
    QVERIFY(resolved.openedCollectionCandidates);

    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(localUrl(QStringLiteral("/books/book.cbz")), {}));
    QVERIFY(archiveCollection.has_value());

    int callbackCount = 0;
    int errorCallbackCount = 0;
    kiriview::ImageIoJob job = resolved.openedCollectionCandidates(
        this, *archiveCollection,
        [&callbackCount](std::vector<kiriview::ImageDocumentPageCandidate>) { ++callbackCount; },
        [&errorCallbackCount](kiriview::MediaEntrySourceError) { ++errorCallbackCount; });

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(callbackCount, 0);
    QCOMPARE(errorCallbackCount, 0);
    QVERIFY(job.isActive());

    job.cancel();
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QVERIFY(!job.isActive());
    QCOMPARE(callbackCount, 0);
    QCOMPARE(errorCallbackCount, 0);
}

void TestRuntimeProviderDefaults::directMediaProviderDefaultBindsDirectoryProvider()
{
    const QUrl requestedUrl = localUrl(QStringLiteral("/media/"));
    QUrl providerUrl;
    int providerCallCount = 0;
    kiriview::DirectoryItemListProvider directoryItemListProvider
        = [&providerCallCount, &providerUrl](QObject*, QUrl directoryUrl,
              kiriview::DirectoryItemListCallback callback, kiriview::KioOperationFailureCallback) {
              ++providerCallCount;
              providerUrl = std::move(directoryUrl);
              callback({});
              return kiriview::ImageIoJob();
          };

    kiriview::DirectMediaNavigationCandidateProvider resolved
        = kiriview::directMediaNavigationCandidateProviderWithDefault(
            kiriview::DirectMediaNavigationCandidateProvider {},
            std::move(directoryItemListProvider));
    QVERIFY(resolved.directoryCandidateLoader);
    QVERIFY(!resolved.directoryCandidateChanges);

    int callbackCount = 0;
    int errorCallbackCount = 0;
    resolved.directoryCandidateLoader(
        this, requestedUrl,
        [&callbackCount](
            std::vector<kiriview::DirectMediaNavigationCandidate>) { ++callbackCount; },
        [&errorCallbackCount](kiriview::KioOperationFailure) { ++errorCallbackCount; });

    QCOMPARE(providerCallCount, 1);
    QCOMPARE(providerUrl, requestedUrl);
    QCOMPARE(callbackCount, 1);
    QCOMPARE(errorCallbackCount, 0);
}

void TestRuntimeProviderDefaults::directMediaProviderProductionDefaultOffersLiveChanges()
{
    kiriview::DirectMediaNavigationCandidateProvider resolved
        = kiriview::defaultDirectMediaNavigationCandidateProvider();

    QVERIFY(resolved.directoryCandidateLoader);
    QVERIFY(resolved.directoryCandidateChanges);
}

void TestRuntimeProviderDefaults::directMediaProviderCustomLoaderDoesNotGainAProductionWatch()
{
    int loadCount = 0;
    kiriview::DirectMediaNavigationCandidateProvider provider {
        [&loadCount](QObject*, QUrl, kiriview::DirectMediaNavigationCandidatesCallback,
            kiriview::KioOperationFailureCallback) {
            ++loadCount;
            return kiriview::ImageIoJob();
        },
    };

    kiriview::DirectMediaNavigationCandidateProvider resolved
        = kiriview::directMediaNavigationCandidateProviderWithDefault(std::move(provider));

    QVERIFY(resolved.directoryCandidateLoader);
    QVERIFY(!resolved.directoryCandidateChanges);
    resolved.directoryCandidateLoader(nullptr, QUrl(), {}, {});
    QCOMPARE(loadCount, 1);
}

void TestRuntimeProviderDefaults::directMediaProviderDefaultPreservesTypedDirectoryFailure()
{
    const kiriview::KioOperationFailure expected {
        kiriview::KioOperationKind::DirectoryListing,
        localUrl(QStringLiteral("/media/")),
        73,
        false,
        QStringLiteral("Could not list media"),
        QStringLiteral("backend diagnostic"),
        true,
    };
    kiriview::DirectoryItemListProvider directoryItemListProvider
        = [expected](QObject*, QUrl, kiriview::DirectoryItemListCallback,
              kiriview::KioOperationFailureCallback errorCallback) {
              errorCallback(expected);
              return kiriview::ImageIoJob();
          };
    kiriview::DirectMediaNavigationCandidateProvider resolved
        = kiriview::defaultDirectMediaNavigationCandidateProvider(
            std::move(directoryItemListProvider));
    std::optional<kiriview::KioOperationFailure> actual;

    resolved.directoryCandidateLoader(this, expected.targetUrl, {},
        [&actual](kiriview::KioOperationFailure failure) { actual = std::move(failure); });

    QVERIFY(actual.has_value());
    QCOMPARE(actual->operationKind, expected.operationKind);
    QCOMPARE(actual->targetUrl, expected.targetUrl);
    QCOMPARE(actual->rawErrorCode, expected.rawErrorCode);
    QCOMPARE(actual->canceled, expected.canceled);
    QCOMPARE(actual->userMessage, expected.userMessage);
    QCOMPARE(actual->diagnosticDetail, expected.diagnosticDetail);
    QCOMPARE(actual->retryable, expected.retryable);
}

void TestRuntimeProviderDefaults::
    imageDocumentRuntimeDependenciesBindContainerLoaderToDirectoryProvider()
{
    const QUrl requestedUrl = localUrl(QStringLiteral("/containers/"));
    QUrl providerUrl;
    int providerCallCount = 0;
    kiriview::ImageDocumentRuntimeDependencyOverrides overrides;
    overrides.directoryItemListProvider
        = [&providerCallCount, &providerUrl](QObject*, QUrl directoryUrl,
              kiriview::DirectoryItemListCallback callback, kiriview::KioOperationFailureCallback) {
              ++providerCallCount;
              providerUrl = std::move(directoryUrl);
              callback({});
              return kiriview::ImageIoJob();
          };

    kiriview::ImageDocumentRuntimeDependencies resolved
        = kiriview::resolveImageDocumentRuntimeDependencies(std::move(overrides));
    QVERIFY(resolved.candidateProvider.directoryContainers);

    int callbackCount = 0;
    int errorCallbackCount = 0;
    resolved.candidateProvider.directoryContainers(
        this, requestedUrl,
        [&callbackCount](std::vector<kiriview::ContainerNavigationCandidate>) { ++callbackCount; },
        [&errorCallbackCount](kiriview::KioOperationFailure) { ++errorCallbackCount; });

    QCOMPARE(providerCallCount, 1);
    QCOMPARE(providerUrl, requestedUrl);
    QCOMPARE(callbackCount, 1);
    QCOMPARE(errorCallbackCount, 0);
}

void TestRuntimeProviderDefaults::
    imageDocumentRuntimeDependenciesBindMediaEntryStoreToWorkerScheduler()
{
    ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDocumentRuntimeDependencyOverrides overrides;
    overrides.imageDecode.workerScheduler = workerScheduler.scheduler();

    kiriview::ImageDocumentRuntimeDependencies resolved
        = kiriview::resolveImageDocumentRuntimeDependencies(std::move(overrides));
    QVERIFY(resolved.candidateProvider.openedCollectionCandidates);

    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(localUrl(QStringLiteral("/books/book.cbz")), {}));
    QVERIFY(archiveCollection.has_value());

    int callbackCount = 0;
    int errorCallbackCount = 0;
    kiriview::ImageIoJob job = resolved.candidateProvider.openedCollectionCandidates(
        this, *archiveCollection,
        [&callbackCount](std::vector<kiriview::ImageDocumentPageCandidate>) { ++callbackCount; },
        [&errorCallbackCount](kiriview::MediaEntrySourceError) { ++errorCallbackCount; });

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(callbackCount, 0);
    QCOMPARE(errorCallbackCount, 0);
    QVERIFY(job.isActive());

    job.cancel();
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QVERIFY(!job.isActive());
    QCOMPARE(callbackCount, 0);
    QCOMPARE(errorCallbackCount, 0);
}

void TestRuntimeProviderDefaults::decodeDependencyDefaultsFillMissingFunctionsAndPreserveOverrides()
{
    int dataLoadCount = 0;
    kiriview::ImageDataLoader dataLoader
        = [&dataLoadCount](QObject*, kiriview::ImageDecodeRequest, kiriview::ImageDataCallback,
              kiriview::ImageDataLoadErrorCallback) {
              ++dataLoadCount;
              return kiriview::ImageIoJob();
          };

    kiriview::ImageDecodeDependencies resolved
        = kiriview::imageDecodeDependenciesWithDefaults({ std::move(dataLoader), {} });

    QVERIFY(resolved.dataLoader);
    QVERIFY(resolved.dataPlanner);

    resolved.dataLoader(nullptr, kiriview::ImageDecodeRequest(), {}, {});
    QCOMPARE(dataLoadCount, 1);
}

void TestRuntimeProviderDefaults::decodeDependencyDefaultsBindPlannerToWorkspaceBudget()
{
    const QByteArray apng = fixtureData(QStringLiteral("animated-smoke.apng"));
    QVERIFY(!apng.isEmpty());
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(1, 1);
    kiriview::ImageDecodeDependencies dependencies;
    dependencies.workspaceBudget = budget;

    kiriview::ImageDecodeDependencies resolved
        = kiriview::imageDecodeDependenciesWithDefaults(std::move(dependencies));
    kiriview::PreparedImageDecodeResult plan = resolved.dataPlanner(kiriview::ImageSourceData(apng),
        kiriview::ImageDecodeRequest::fromUrl(
            1, QUrl::fromLocalFile(QStringLiteral("/tmp/animated-smoke.apng"))),
        kiriview::ImageDecodeWorkspacePriority::Interactive);
    const auto* work = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&plan);

    QVERIFY(work != nullptr);
    QVERIFY(*work != nullptr);
    QVERIFY(work->get()->admissionRequest().additionalPeakByteCount > budget->aggregateByteLimit());
    QCOMPARE(work->get()->hardLimitFailure().route, kiriview::DecodedImageFailureRoute::Apng);
    QCOMPARE(work->get()->hardLimitFailure().cause,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestRuntimeProviderDefaults::decodeDependencyDefaultsBindDataLoaderToWorkerScheduler()
{
    ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies;
    dependencies.workerScheduler = workerScheduler.scheduler();

    kiriview::ImageDecodeDependencies resolved
        = kiriview::imageDecodeDependenciesWithDefaults(std::move(dependencies));
    QVERIFY(resolved.dataLoader);

    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(localUrl(QStringLiteral("/books/book.cbz")), {}));
    QVERIFY(archiveCollection.has_value());
    const QUrl pageUrl = archivePageUrl(archiveCollection->rootUrl(), QStringLiteral("01.png"));

    int dataCallbackCount = 0;
    int errorCallbackCount = 0;
    kiriview::ImageIoJob job = resolved.dataLoader(
        this,
        kiriview::ImageDecodeRequest::fromLocation(17,
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                pageUrl, *archiveCollection)),
        [&dataCallbackCount](kiriview::ImageSourceData) { ++dataCallbackCount; },
        [&errorCallbackCount](kiriview::ImageDataLoadError) { ++errorCallbackCount; });

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(dataCallbackCount, 0);
    QCOMPARE(errorCallbackCount, 0);
    QVERIFY(job.isActive());

    job.cancel();
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QVERIFY(!job.isActive());
    QCOMPARE(dataCallbackCount, 0);
    QCOMPARE(errorCallbackCount, 0);
}

void TestRuntimeProviderDefaults::decodeDependencyDefaultsBindThumbnailLookupToWorkerScheduler()
{
    ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies;
    dependencies.workerScheduler = workerScheduler.scheduler();

    kiriview::ImageDecodeDependencies resolved
        = kiriview::imageDecodeDependenciesWithDefaults(std::move(dependencies));
    QVERIFY(resolved.thumbnailPreviewLookupProvider);

    int callbackCount = 0;
    kiriview::ImageIoJob job
        = resolved.thumbnailPreviewLookupProvider(this, kiriview::ThumbnailCacheLookupRequest {},
            [&callbackCount](kiriview::ThumbnailCacheLookupResult) { ++callbackCount; });

    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(callbackCount, 0);
    QVERIFY(job.isActive());

    job.cancel();
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QVERIFY(!job.isActive());
    QCOMPARE(callbackCount, 0);
}

void TestRuntimeProviderDefaults::fileDeletionDefaultFillsMissingProviderAndPreservesOverride()
{
    int fileDeletionCount = 0;
    kiriview::FileDeletionProvider fileDeletionProvider
        = [&fileDeletionCount](
              QObject*, kiriview::FileDeletionRequest, kiriview::FileDeletionCallback) {
              ++fileDeletionCount;
              return kiriview::ImageIoJob();
          };

    kiriview::FileDeletionProvider resolved
        = kiriview::fileDeletionProviderWithDefault(std::move(fileDeletionProvider));
    QVERIFY(resolved);
    resolved(nullptr, kiriview::FileDeletionRequest(), {});
    QCOMPARE(fileDeletionCount, 1);

    QVERIFY(kiriview::fileDeletionProviderWithDefault({}));
}

QTEST_GUILESS_MAIN(TestRuntimeProviderDefaults)
#include "tst_runtimeproviderdefaults.moc"
