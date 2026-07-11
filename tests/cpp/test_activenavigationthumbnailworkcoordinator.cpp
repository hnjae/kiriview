// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailrowstore.h"
#include "session/activenavigationthumbnailworkcoordinator.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <memory>
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

kiriview::ThumbnailSourceAdapter localAdapter()
{
    return [](kiriview::ThumbnailSourceAdapterRequest request) {
        const QByteArray path = request.sourceKey.url.toLocalFile().toUtf8();
        return kiriview::ThumbnailSourceAdapterPlan {
            kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
            path,
            kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(path),
            {},
        };
    };
}

QString imageId(const QUrl& source) { return source.path().mid(1); }
}

class TestActiveNavigationThumbnailWorkCoordinator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cacheMissChainsGenerationAndPublishesReadyImage();
    void supersededLookupCompletionIsRejectedByJobIdentity();
    void backgroundResultAndFailedRefinementPreserveForegroundReadyImage();
    void demandWindowAdmitsVisibleBeforeNearbyRegardlessOfReportOrder();
};

void TestActiveNavigationThumbnailWorkCoordinator::cacheMissChainsGenerationAndPublishesReadyImage()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    rows.setRows({ row(1, QStringLiteral("/media/one.png")) });
    ManualProviders providers;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(
        this, rows, providers.lookupProvider(), providers.generationProvider(), localAdapter());
    coordinator.resetRows(rows.sourceKeys(), rows.navigationGeneration());

    QVERIFY(coordinator.beginDemandWindow(rows.navigationGeneration()));
    QVERIFY(coordinator.reportDemand(
        1, rows.sourceKeyAt(0).url, Bucket::Large, Priority::Visible, rows.navigationGeneration()));
    coordinator.finishDemandWindow(rows.navigationGeneration());
    QCOMPARE(providers.lookups.size(), std::size_t(1));
    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Missing);
    QCOMPARE(providers.generations.size(), std::size_t(1));
    providers.finishGeneration(0, kiriview::ThumbnailGenerationStatus::Ready, image(Qt::green));

    QCOMPARE(rows.resultAt(0).status, Status::Ready);
    QCOMPARE(
        images->image(imageId(rows.resultAt(0).imageSource)).pixelColor(0, 0), QColor(Qt::green));
}

void TestActiveNavigationThumbnailWorkCoordinator::
    supersededLookupCompletionIsRejectedByJobIdentity()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    rows.setRows({ row(1, QStringLiteral("/media/one.png")) });
    ManualProviders providers;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(
        this, rows, providers.lookupProvider(), providers.generationProvider(), localAdapter());
    coordinator.resetRows(rows.sourceKeys(), rows.navigationGeneration());
    const QUrl url = rows.sourceKeyAt(0).url;

    QVERIFY(coordinator.beginDemandWindow(rows.navigationGeneration()));
    QVERIFY(coordinator.reportDemand(
        1, url, Bucket::Normal, Priority::Visible, rows.navigationGeneration()));
    coordinator.finishDemandWindow(rows.navigationGeneration());
    QVERIFY(coordinator.beginDemandWindow(rows.navigationGeneration()));
    QVERIFY(coordinator.reportDemand(
        1, url, Bucket::Large, Priority::Visible, rows.navigationGeneration()));
    coordinator.finishDemandWindow(rows.navigationGeneration());
    QCOMPARE(providers.lookups.size(), std::size_t(2));

    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::red));
    QCOMPARE(rows.resultAt(0).status, Status::Pending);
    QCOMPARE(images->size(), qsizetype(0));

    providers.finishLookup(1, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::blue));
    QCOMPARE(rows.resultAt(0).status, Status::Ready);
    QCOMPARE(
        images->image(imageId(rows.resultAt(0).imageSource)).pixelColor(0, 0), QColor(Qt::blue));
}

void TestActiveNavigationThumbnailWorkCoordinator::
    backgroundResultAndFailedRefinementPreserveForegroundReadyImage()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    rows.setRows({ row(1, QStringLiteral("/media/one.png")) });
    ManualProviders providers;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(
        this, rows, providers.lookupProvider(), providers.generationProvider(), localAdapter());
    coordinator.resetRows(rows.sourceKeys(), rows.navigationGeneration());
    const QUrl url = rows.sourceKeyAt(0).url;

    QVERIFY(coordinator.beginDemandWindow(rows.navigationGeneration()));
    QVERIFY(coordinator.reportDemand(
        1, url, Bucket::Normal, Priority::Visible, rows.navigationGeneration()));
    coordinator.finishDemandWindow(rows.navigationGeneration());
    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::green));
    const QUrl foregroundSource = rows.resultAt(0).imageSource;
    QCOMPARE(providers.lookups.size(), std::size_t(2));

    providers.finishLookup(1, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::blue));
    QCOMPARE(rows.resultAt(0).imageSource, foregroundSource);
    QCOMPARE(images->size(), qsizetype(1));

    QVERIFY(coordinator.beginDemandWindow(rows.navigationGeneration()));
    QVERIFY(coordinator.reportDemand(
        1, url, Bucket::XLarge, Priority::Visible, rows.navigationGeneration()));
    coordinator.finishDemandWindow(rows.navigationGeneration());
    const std::size_t foregroundLookup = providers.lookups.size() - 1;
    providers.finishLookup(foregroundLookup, kiriview::ThumbnailCacheLookupStatus::Failed, {},
        QStringLiteral("refinement lookup failed"));

    QCOMPARE(rows.resultAt(0).status, Status::Ready);
    QCOMPARE(rows.resultAt(0).imageSource, foregroundSource);
    QVERIFY(!coordinator.failureDiagnostics().empty());
    QCOMPARE(coordinator.failureDiagnostics().back().errorString,
        QStringLiteral("refinement lookup failed"));
}

void TestActiveNavigationThumbnailWorkCoordinator::
    demandWindowAdmitsVisibleBeforeNearbyRegardlessOfReportOrder()
{
    auto images = std::make_shared<kiriview::ThumbnailImageStore>();
    kiriview::ActiveNavigationThumbnailRowStore rows(this, images);
    rows.setRows(
        { row(1, QStringLiteral("/media/one.png")), row(2, QStringLiteral("/media/two.png")) });
    ManualProviders providers;
    kiriview::ActiveNavigationThumbnailWorkCoordinator coordinator(
        this, rows, providers.lookupProvider(), providers.generationProvider(), localAdapter());
    coordinator.resetRows(rows.sourceKeys(), rows.navigationGeneration());
    const quint64 generation = rows.navigationGeneration();

    QVERIFY(coordinator.beginDemandWindow(generation));
    QVERIFY(coordinator.reportDemand(
        2, rows.sourceKeyAt(1).url, Bucket::Normal, Priority::Nearby, generation));
    QVERIFY(coordinator.reportDemand(
        1, rows.sourceKeyAt(0).url, Bucket::Normal, Priority::Visible, generation));
    QCOMPARE(providers.lookups.size(), std::size_t(0));

    coordinator.finishDemandWindow(generation);
    QCOMPARE(providers.lookups.size(), std::size_t(1));
    QCOMPARE(providers.lookups.front().request.localPathBytes, QByteArray("/media/one.png"));

    providers.finishLookup(0, kiriview::ThumbnailCacheLookupStatus::Ready, image(Qt::green));
    QCOMPARE(providers.lookups.size(), std::size_t(2));
    QCOMPARE(providers.lookups.back().request.localPathBytes, QByteArray("/media/two.png"));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailWorkCoordinator)

#include "test_activenavigationthumbnailworkcoordinator.moc"
