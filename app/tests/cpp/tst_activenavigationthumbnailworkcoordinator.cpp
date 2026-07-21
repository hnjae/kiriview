// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailrowstore.h"
#include "session/activenavigationthumbnailworkcoordinator.h"
#include "thumbnail/thumbnailgeneration.h"

#include "image_async_test_support.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;
using Priority = kiriview::ActiveNavigationThumbnailDemandPriority;
using Status = kiriview::ActiveNavigationThumbnailResultStatus;

kiriview::ActiveNavigationThumbnailRow row(int number, const QString& path)
{
    return kiriview::ActiveNavigationThumbnailRow {
        number,
        QUrl::fromLocalFile(path),
        path.section(QLatin1Char('/'), -1),
        kiriview::ActiveNavigationThumbnailKind::Image,
        kiriview::ActiveNavigationThumbnailSourceKind::DirectImage,
        number == 1,
    };
}

kiriview::ActiveNavigationThumbnailRow videoRow(int number, const QString& path)
{
    return kiriview::ActiveNavigationThumbnailRow {
        number,
        QUrl::fromLocalFile(path),
        path.section(QLatin1Char('/'), -1),
        kiriview::ActiveNavigationThumbnailKind::Video,
        kiriview::ActiveNavigationThumbnailSourceKind::DirectVideo,
        number == 1,
    };
}

QImage image(QColor color)
{
    QImage result(QSize(2, 1), QImage::Format_RGBA8888);
    result.fill(color);
    return result;
}

struct Lookup
{
    kiriview::ThumbnailCacheLookupRequest request;
    kiriview::ThumbnailCacheLookupCallback callback;
};

struct Generation
{
    kiriview::ThumbnailGenerationRequest request;
    kiriview::ThumbnailGenerationCallback callback;
};

class ManualProviders
{
public:
    kiriview::ThumbnailCacheLookupProvider lookupProvider()
    {
        return [this](QObject*, kiriview::ThumbnailCacheLookupRequest request,
                   kiriview::ThumbnailCacheLookupCallback callback) {
            lookups.push_back({ std::move(request), std::move(callback) });
            return kiriview::ImageIoJob {};
        };
    }

    kiriview::ThumbnailGenerationProvider generationProvider()
    {
        return [this](QObject*, kiriview::ThumbnailGenerationRequest request,
                   kiriview::ThumbnailGenerationCallback callback) {
            generations.push_back({ std::move(request), std::move(callback) });
            return kiriview::ImageIoJob {};
        };
    }

    void finishLookup(std::size_t index, kiriview::ThumbnailCacheLookupStatus status,
        QImage readyImage = {}, QString errorString = {})
    {
        const Bucket bucket = lookups.at(index).request.requestedBucket;
        kiriview::ThumbnailCacheLookupCallback callback = lookups.at(index).callback;
        callback(kiriview::ThumbnailCacheLookupResult {
            status,
            std::move(readyImage),
            bucket,
            bucket,
            {},
            std::move(errorString),
        });
    }

    void finishGeneration(std::size_t index, kiriview::ThumbnailGenerationStatus status,
        QImage readyImage = {}, QString errorString = {})
    {
        const Bucket bucket = generations.at(index).request.requestedBucket;
        kiriview::ThumbnailGenerationCallback callback = generations.at(index).callback;
        callback(kiriview::ThumbnailGenerationResult {
            status,
            std::move(readyImage),
            bucket,
            {},
            std::move(errorString),
        });
    }

    std::vector<Lookup> lookups;
    std::vector<Generation> generations;
};

struct ManualVideoExtraction
{
    QObject* object = nullptr;
    kiriview::VideoThumbnailExtractionRequest request;
    kiriview::VideoThumbnailExtractionCallback callback;
    kiriview::ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualVideoExtractions
{
public:
    kiriview::VideoThumbnailExtractionProvider provider()
    {
        return [this](QObject* receiver, kiriview::VideoThumbnailExtractionRequest request,
                   kiriview::VideoThumbnailExtractionCallback callback) {
            auto extraction = std::make_shared<ManualVideoExtraction>();
            extraction->request = std::move(request);
            extraction->callback = std::move(callback);
            kiriview::ImageIoJob job
                = kiriview::TestSupport::Detail::startManualIoJob(receiver, extraction);
            extractions.push_back(std::move(extraction));
            maximumActiveCount = std::max(maximumActiveCount, activeCount());
            return job;
        };
    }

    std::size_t activeCount() const
    {
        return static_cast<std::size_t>(std::count_if(extractions.cbegin(), extractions.cend(),
            [](const auto& extraction) { return extraction->completion.isActive(); }));
    }

    void fail(std::size_t index)
    {
        const std::shared_ptr<ManualVideoExtraction> extraction = extractions.at(index);
        kiriview::TestSupport::Detail::finishManualIoJob(
            extraction, [](ManualVideoExtraction& finished) {
                finished.callback({ kiriview::VideoThumbnailExtractionStatus::Failed, {},
                    QStringLiteral("synthetic extraction failure") });
            });
    }

    std::vector<std::shared_ptr<ManualVideoExtraction>> extractions;
    std::size_t maximumActiveCount = 0;
};

kiriview::ThumbnailSourceAdapter localAdapter()
{
    return [](kiriview::ThumbnailSourceAdapterRequest request) {
        const QByteArray path = request.sourceKey.sourceUrl.toLocalFile().toUtf8();
        return kiriview::ThumbnailSourceAdapterPlan {
            kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
            path,
            kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(path),
            {},
        };
    };
}

QString imageId(const QUrl& source) { return source.path().mid(1); }

kiriview::ActiveNavigationThumbnailSchedulingSnapshot setRows(
    kiriview::ActiveNavigationThumbnailRowStore& store,
    std::vector<kiriview::ActiveNavigationThumbnailRow> rows)
{
    store.commitRows(store.prepareRows(std::move(rows)));
    return store.schedulingSnapshot();
}

Status resultStatus(const kiriview::ActiveNavigationThumbnailRowStore& store, int row)
{
    return static_cast<Status>(store.model()
            ->data(store.model()->index(row, 0),
                kiriview::ActiveNavigationThumbnailModel::ThumbnailStatusRole)
            .toInt());
}

QUrl resultSource(const kiriview::ActiveNavigationThumbnailRowStore& store, int row)
{
    return store.model()
        ->data(store.model()->index(row, 0),
            kiriview::ActiveNavigationThumbnailModel::ThumbnailImageSourceRole)
        .toUrl();
}

kiriview::ActiveNavigationThumbnailDemandSnapshot demandSnapshot(
    quint64 generation, std::initializer_list<kiriview::ActiveNavigationThumbnailDemand> demands)
{
    return { generation, demands };
}
}

class TestActiveNavigationThumbnailWorkCoordinator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cacheMissChainsGenerationAndPublishesReadyImage();
    void supersededLookupCompletionIsRejectedByJobIdentity();
    void backgroundResultAndFailedRefinementPreserveForegroundReadyImage();
    void demandWindowAdmitsVisibleBeforeNearbyRegardlessOfReportOrder();
    void videoDemandIsCapacityBoundedAndCancellationReleasesExtractor();
    void queuedContinuationFindsEligibleBackgroundRow();
    void invalidationRejectsQueuedContinuation();
};

void TestActiveNavigationThumbnailWorkCoordinator::cacheMissChainsGenerationAndPublishesReadyImage()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    const auto schedulingRows = setRows(rows, { row(1, QStringLiteral("/media/one.png")) });
    ManualProviders providers;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(
        this, rows, providers.lookupProvider(), providers.generationProvider(), localAdapter());
    QVERIFY(coordinator.resetRows(schedulingRows));

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(rows.navigationGeneration(),
        { { 1, schedulingRows.rows.at(0).sourceUrl, Bucket::Large, Priority::Visible } })));
    QCOMPARE(providers.lookups.size(), std::size_t(1));
    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Missing);
    QCOMPARE(providers.generations.size(), std::size_t(1));
    providers.finishGeneration(0, kiriview::ThumbnailGenerationStatus::Ready, image(Qt::green));

    QCOMPARE(resultStatus(rows, 0), Status::Ready);
    QCOMPARE(images->image(imageId(resultSource(rows, 0))).pixelColor(0, 0), QColor(Qt::green));
}

void TestActiveNavigationThumbnailWorkCoordinator::
    supersededLookupCompletionIsRejectedByJobIdentity()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    const auto schedulingRows = setRows(rows, { row(1, QStringLiteral("/media/one.png")) });
    ManualProviders providers;
    int failureDiagnosticCount = 0;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(this, rows,
        providers.lookupProvider(), providers.generationProvider(), localAdapter(),
        [&failureDiagnosticCount](const kiriview::ActiveNavigationThumbnailFailureDiagnostic&) {
            ++failureDiagnosticCount;
        });
    QVERIFY(coordinator.resetRows(schedulingRows));
    const QUrl url = schedulingRows.rows.at(0).sourceUrl;

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(
        rows.navigationGeneration(), { { 1, url, Bucket::Normal, Priority::Visible } })));
    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(
        rows.navigationGeneration(), { { 1, url, Bucket::Large, Priority::Visible } })));
    QCOMPARE(providers.lookups.size(), std::size_t(2));

    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Failed, {},
        QStringLiteral("superseded lookup failed"));
    QCOMPARE(resultStatus(rows, 0), Status::Pending);
    QCOMPARE(images->size(), qsizetype(0));
    QCOMPARE(failureDiagnosticCount, 0);

    providers.finishLookup(1, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::blue));
    QCOMPARE(resultStatus(rows, 0), Status::Ready);
    QCOMPARE(images->image(imageId(resultSource(rows, 0))).pixelColor(0, 0), QColor(Qt::blue));
    QCOMPARE(failureDiagnosticCount, 0);
}

void TestActiveNavigationThumbnailWorkCoordinator::
    backgroundResultAndFailedRefinementPreserveForegroundReadyImage()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    const auto schedulingRows = setRows(rows, { row(1, QStringLiteral("/media/one.png")) });
    ManualProviders providers;
    int failureDiagnosticCount = 0;
    std::optional<kiriview::ActiveNavigationThumbnailFailureDiagnostic> lastFailureDiagnostic;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(this, rows,
        providers.lookupProvider(), providers.generationProvider(), localAdapter(),
        [&failureDiagnosticCount, &lastFailureDiagnostic](
            const kiriview::ActiveNavigationThumbnailFailureDiagnostic& diagnostic) {
            ++failureDiagnosticCount;
            lastFailureDiagnostic = diagnostic;
        });
    QVERIFY(coordinator.resetRows(schedulingRows));
    const QUrl url = schedulingRows.rows.at(0).sourceUrl;

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(
        rows.navigationGeneration(), { { 1, url, Bucket::Normal, Priority::Visible } })));
    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::green));
    const QUrl foregroundSource = resultSource(rows, 0);
    QCOMPARE(providers.lookups.size(), std::size_t(2));

    providers.finishLookup(1, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::blue));
    QCOMPARE(resultSource(rows, 0), foregroundSource);
    QCOMPARE(images->size(), qsizetype(1));

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(
        rows.navigationGeneration(), { { 1, url, Bucket::XLarge, Priority::Visible } })));
    const std::size_t foregroundLookup = providers.lookups.size() - 1;
    providers.finishLookup(foregroundLookup, kiriview::ThumbnailCacheLookupStatus::Failed, {},
        QStringLiteral("refinement lookup failed"));

    QCOMPARE(resultStatus(rows, 0), Status::Ready);
    QCOMPARE(resultSource(rows, 0), foregroundSource);
    QCOMPARE(failureDiagnosticCount, 1);
    QVERIFY(lastFailureDiagnostic.has_value());
    QVERIFY(lastFailureDiagnostic->workId.isValid());
    QCOMPARE(lastFailureDiagnostic->sourceKey, schedulingRows.rows.at(0));
    QCOMPARE(
        lastFailureDiagnostic->workKind, kiriview::ActiveNavigationThumbnailWorkKind::Foreground);
    QCOMPARE(lastFailureDiagnostic->bucket, Bucket::XLarge);
    QCOMPARE(lastFailureDiagnostic->failureKind,
        kiriview::ActiveNavigationThumbnailFailureKind::CacheLookupFailed);
    QCOMPARE(lastFailureDiagnostic->errorString, QStringLiteral("refinement lookup failed"));
}

void TestActiveNavigationThumbnailWorkCoordinator::
    demandWindowAdmitsVisibleBeforeNearbyRegardlessOfReportOrder()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    const auto schedulingRows = setRows(rows,
        { row(1, QStringLiteral("/media/one.png")), row(2, QStringLiteral("/media/two.png")) });
    ManualProviders providers;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(
        this, rows, providers.lookupProvider(), providers.generationProvider(), localAdapter());
    QVERIFY(coordinator.resetRows(schedulingRows));
    const quint64 generation = rows.navigationGeneration();

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(generation,
        { { 2, schedulingRows.rows.at(1).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 1, schedulingRows.rows.at(0).sourceUrl, Bucket::Normal, Priority::Visible } })));
    QCOMPARE(providers.lookups.size(), std::size_t(1));
    QCOMPARE(providers.lookups.front().request.localPathBytes, QByteArray("/media/one.png"));

    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::green));
    QCOMPARE(providers.lookups.size(), std::size_t(2));
    QCOMPARE(providers.lookups.back().request.localPathBytes, QByteArray("/media/two.png"));
}

void TestActiveNavigationThumbnailWorkCoordinator::
    videoDemandIsCapacityBoundedAndCancellationReleasesExtractor()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    const auto schedulingRows = setRows(rows,
        { videoRow(1, QStringLiteral("/media/one.mp4")),
            videoRow(2, QStringLiteral("/media/two.mp4")),
            videoRow(3, QStringLiteral("/media/three.mp4")),
            videoRow(4, QStringLiteral("/media/four.mp4")),
            videoRow(5, QStringLiteral("/media/five.mp4")) });
    ManualProviders providers;
    ManualVideoExtractions extractions;
    kiriview::ThumbnailGenerationDependencies generationDependencies;
    generationDependencies.videoExtractor = extractions.provider();
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(this, rows,
        providers.lookupProvider(),
        kiriview::defaultThumbnailGenerationProvider({}, std::move(generationDependencies)),
        localAdapter());
    QVERIFY(coordinator.resetRows(schedulingRows));
    coordinator.setCurrentNumber(4);
    const quint64 generation = rows.navigationGeneration();

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(generation,
        { { 5, schedulingRows.rows.at(4).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 3, schedulingRows.rows.at(2).sourceUrl, Bucket::Normal, Priority::Visible },
            { 4, schedulingRows.rows.at(3).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 2, schedulingRows.rows.at(1).sourceUrl, Bucket::Normal, Priority::Visible },
            { 1, schedulingRows.rows.at(0).sourceUrl, Bucket::Normal, Priority::Visible } })));
    QCOMPARE(providers.lookups.size(), std::size_t(2));
    QCOMPARE(providers.lookups.at(0).request.localPathBytes, QByteArray("/media/four.mp4"));
    QCOMPARE(providers.lookups.at(1).request.localPathBytes, QByteArray("/media/one.mp4"));
    QCOMPARE(resultStatus(rows, 1), Status::Pending);
    QCOMPARE(resultStatus(rows, 4), Status::Pending);

    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Missing);
    providers.finishLookup(1, kiriview::ThumbnailCacheLookupStatus::Missing);
    QCOMPARE(extractions.extractions.size(), std::size_t(2));
    QCOMPARE(extractions.extractions.at(0)->request.sourceUrl,
        QUrl::fromLocalFile(QStringLiteral("/media/four.mp4")));
    QCOMPARE(extractions.extractions.at(1)->request.sourceUrl,
        QUrl::fromLocalFile(QStringLiteral("/media/one.mp4")));
    QCOMPARE(extractions.activeCount(), std::size_t(2));
    QCOMPARE(extractions.maximumActiveCount, std::size_t(2));

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(generation,
        { { 5, schedulingRows.rows.at(4).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 4, schedulingRows.rows.at(3).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 2, schedulingRows.rows.at(1).sourceUrl, Bucket::Normal, Priority::Visible } })));
    QVERIFY(extractions.extractions.at(1)->canceled);
    QCOMPARE(extractions.activeCount(), std::size_t(1));
    QCOMPARE(providers.lookups.size(), std::size_t(3));
    QCOMPARE(providers.lookups.back().request.localPathBytes, QByteArray("/media/two.mp4"));

    providers.finishLookup(2, kiriview::ThumbnailCacheLookupStatus::Missing);
    QCOMPARE(extractions.extractions.size(), std::size_t(3));
    QCOMPARE(extractions.extractions.back()->request.sourceUrl,
        QUrl::fromLocalFile(QStringLiteral("/media/two.mp4")));
    QCOMPARE(extractions.activeCount(), std::size_t(2));
    QCOMPARE(extractions.maximumActiveCount, std::size_t(2));

    extractions.fail(0);
    QCOMPARE(providers.lookups.size(), std::size_t(3));
    QCOMPARE(extractions.activeCount(), std::size_t(1));

    extractions.fail(2);
    QCOMPARE(providers.lookups.size(), std::size_t(4));
    QCOMPARE(providers.lookups.back().request.localPathBytes, QByteArray("/media/five.mp4"));
    providers.finishLookup(3, kiriview::ThumbnailCacheLookupStatus::Missing);
    QCOMPARE(extractions.extractions.size(), std::size_t(4));
    QCOMPARE(extractions.extractions.back()->request.sourceUrl,
        QUrl::fromLocalFile(QStringLiteral("/media/five.mp4")));
    QCOMPARE(extractions.activeCount(), std::size_t(1));
    QCOMPARE(extractions.maximumActiveCount, std::size_t(2));
}

void TestActiveNavigationThumbnailWorkCoordinator::queuedContinuationFindsEligibleBackgroundRow()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    std::vector<kiriview::ActiveNavigationThumbnailRow> sourceRows;
    for (int number = 1; number <= 20; ++number) {
        sourceRows.push_back(row(number, QStringLiteral("/media/%1.png").arg(number)));
    }
    const auto schedulingRows = setRows(rows, std::move(sourceRows));
    ManualProviders providers;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(this, rows,
        providers.lookupProvider(), providers.generationProvider(),
        [](kiriview::ThumbnailSourceAdapterRequest request) {
            if (request.sourceKey.row.rowNumber != 20) {
                return kiriview::ThumbnailSourceAdapterPlan {};
            }
            const QByteArray path = request.sourceKey.sourceUrl.toLocalFile().toUtf8();
            return kiriview::ThumbnailSourceAdapterPlan {
                kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
                path,
                kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(path),
                {},
            };
        });
    QVERIFY(coordinator.resetRows(schedulingRows));

    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(rows.navigationGeneration(), {})));
    QVERIFY(providers.lookups.empty());
    QTRY_COMPARE(providers.lookups.size(), std::size_t(1));
    QCOMPARE(providers.lookups.front().request.localPathBytes, QByteArray("/media/20.png"));
}

void TestActiveNavigationThumbnailWorkCoordinator::invalidationRejectsQueuedContinuation()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    std::vector<kiriview::ActiveNavigationThumbnailRow> sourceRows;
    for (int number = 1; number <= 20; ++number) {
        sourceRows.push_back(row(number, QStringLiteral("/media/%1.png").arg(number)));
    }
    const auto schedulingRows = setRows(rows, std::move(sourceRows));
    ManualProviders providers;
    bool eligible = false;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(this, rows,
        providers.lookupProvider(), providers.generationProvider(),
        [&eligible](kiriview::ThumbnailSourceAdapterRequest request) {
            if (!eligible || request.sourceKey.row.rowNumber != 20) {
                return kiriview::ThumbnailSourceAdapterPlan {};
            }
            const QByteArray path = request.sourceKey.sourceUrl.toLocalFile().toUtf8();
            return kiriview::ThumbnailSourceAdapterPlan {
                kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
                path,
                kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(path),
                {},
            };
        });
    QVERIFY(coordinator.resetRows(schedulingRows));
    QVERIFY(coordinator.replaceDemandSnapshot(demandSnapshot(rows.navigationGeneration(), {})));

    eligible = true;
    coordinator.invalidateRows();
    QCoreApplication::processEvents();
    QVERIFY(providers.lookups.empty());
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailWorkCoordinator)

#include "tst_activenavigationthumbnailworkcoordinator.moc"
