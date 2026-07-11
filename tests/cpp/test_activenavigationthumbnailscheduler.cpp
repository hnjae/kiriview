// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <initializer_list>
#include <variant>
#include <vector>

namespace {
using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;
using Priority = kiriview::ActiveNavigationThumbnailDemandPriority;

kiriview::ThumbnailSourceKey key(int number, quint64 generation = 1)
{
    kiriview::ThumbnailSourceKey result;
    result.rowNumber = number;
    result.url = QUrl::fromLocalFile(QStringLiteral("/media/%1.png").arg(number));
    result.label = QStringLiteral("%1.png").arg(number);
    result.pageKind = QStringLiteral("image");
    result.sourceKind = kiriview::activeNavigationThumbnailSourceKindIdentity(
        kiriview::ActiveNavigationThumbnailSourceKind::DirectImage);
    result.rowIdentity = QString::number(number);
    result.navigationGeneration = generation;
    return result;
}

kiriview::ThumbnailSourceAdapter adapter()
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

template <typename Effect>
std::vector<Effect> effectsOfType(
    const std::vector<kiriview::ActiveNavigationThumbnailScheduleEffect>& effects)
{
    std::vector<Effect> result;
    for (const auto& effect : effects) {
        if (const auto* value = std::get_if<Effect>(&effect)) {
            result.push_back(*value);
        }
    }
    return result;
}

kiriview::ActiveNavigationThumbnailWorkCompletion ready(
    const kiriview::ActiveNavigationThumbnailStartWorkEffect& start)
{
    return { start.request.workId, start.request.sourceKey, start.request.bucket,
        start.request.workKind,
        { kiriview::ActiveNavigationThumbnailWorkResultKind::Ready, {},
            kiriview::ActiveNavigationThumbnailFailureKind::GenerationFailed, {} } };
}

kiriview::ActiveNavigationThumbnailDemandSnapshot snapshot(
    quint64 generation, std::initializer_list<kiriview::ActiveNavigationThumbnailDemand> demands)
{
    return { generation, demands };
}
}

class TestActiveNavigationThumbnailScheduler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void demandWindowIsAtomicAndRejectsOutsideReports();
    void visibleRunsBeforeNearbyRegardlessOfReportOrder();
    void currentPromotesCommittedNearbyWithoutRestart();
    void newerForegroundCancelsActiveNearby();
    void newerWindowExpiresMissingDemandAndDemotesRetention();
    void backgroundRunsOneAtATimeAndYieldsToDemand();
    void invalidSnapshotIsRejectedWithoutReplacingCommittedDemand();
    void duplicateSnapshotFactsMergeBeforeSourcePlanning();
    void emptySnapshotExpiresNonCurrentDemand();
};

void TestActiveNavigationThumbnailScheduler::demandWindowIsAtomicAndRejectsOutsideReports()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    QVERIFY(!scheduler
            .replaceDemandSnapshot(
                snapshot(2, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }))
            .has_value());
    const auto accepted = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(accepted.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).size(),
        std::size_t(1));
}

void TestActiveNavigationThumbnailScheduler::visibleRunsBeforeNearbyRegardlessOfReportOrder()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    auto committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 2, key(2).url, Bucket::Normal, Priority::Nearby },
            { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    auto effects = std::move(*committed);
    auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.rowNumber, 1);

    effects = scheduler.acceptCompletion(ready(starts.front()));
    starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.rowNumber, 2);
}

void TestActiveNavigationThumbnailScheduler::currentPromotesCommittedNearbyWithoutRestart()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    const auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto starts
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed);
    QCOMPARE(starts.size(), std::size_t(1));

    const auto effects = scheduler.setCurrentNumber(1);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects).empty());
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects).empty());
}

void TestActiveNavigationThumbnailScheduler::newerForegroundCancelsActiveNearby()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 2, key(2).url, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto nearbyStart
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).front();

    committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).url, Bucket::Normal, Priority::Visible },
            { 2, key(2).url, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto& effects = *committed;
    const auto cancels
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects);
    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId.value, nearbyStart.request.workId.value);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.rowNumber, 1);
}

void TestActiveNavigationThumbnailScheduler::newerWindowExpiresMissingDemandAndDemotesRetention()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    auto committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).url, Bucket::Normal, Priority::Visible },
            { 2, key(2).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).size(),
        std::size_t(2));

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto& effects = *committed;
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects).size(),
        std::size_t(1));
    const auto retention
        = effectsOfType<kiriview::ActiveNavigationThumbnailUpdateRetentionEffect>(effects);
    QCOMPARE(retention.size(), std::size_t(1));
    QCOMPARE(retention.front().sourceKey.rowNumber, 2);
    QCOMPARE(retention.front().retentionClass,
        kiriview::ActiveNavigationThumbnailRetentionClass::Background);
}

void TestActiveNavigationThumbnailScheduler::backgroundRunsOneAtATimeAndYieldsToDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto foreground
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).front();
    auto effects = scheduler.acceptCompletion(ready(foreground));
    const auto background
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(background.size(), std::size_t(1));
    QCOMPARE(background.front().request.workKind,
        kiriview::ActiveNavigationThumbnailWorkKind::Background);
    QCOMPARE(background.front().request.bucket, Bucket::Large);

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::XLarge, Priority::Visible } }));
    QVERIFY(committed.has_value());
    effects = std::move(*committed);
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects).size(),
        std::size_t(1));
    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(
        starts.front().request.workKind, kiriview::ActiveNavigationThumbnailWorkKind::Foreground);
}

void TestActiveNavigationThumbnailScheduler::
    invalidSnapshotIsRejectedWithoutReplacingCommittedDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    const auto accepted = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(accepted.has_value());
    const auto foreground
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).front();

    const auto rejected = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 2, key(2).url, Bucket::Large, Priority::Visible } }));
    QVERIFY(!rejected.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailAcceptCompletionEffect>(
                 scheduler.acceptCompletion(ready(foreground)))
                 .size(),
        std::size_t(1));
}

void TestActiveNavigationThumbnailScheduler::duplicateSnapshotFactsMergeBeforeSourcePlanning()
{
    int adapterCalls = 0;
    kiriview::ActiveNavigationThumbnailScheduler scheduler(
        [&adapterCalls](kiriview::ThumbnailSourceAdapterRequest request) {
            ++adapterCalls;
            const QByteArray path = request.sourceKey.url.toLocalFile().toUtf8();
            return kiriview::ThumbnailSourceAdapterPlan {
                kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
                path,
                kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(path),
                {},
            };
        });
    scheduler.reset({ key(1) }, 1);
    const auto accepted = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).url, Bucket::Normal, Priority::Nearby },
            { 1, key(1).url, Bucket::XLarge, Priority::Visible } }));
    QVERIFY(accepted.has_value());
    QCOMPARE(adapterCalls, 1);
    const auto starts
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.bucket, Bucket::XLarge);
}

void TestActiveNavigationThumbnailScheduler::emptySnapshotExpiresNonCurrentDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    const auto first = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).url, Bucket::Normal, Priority::Visible },
            { 2, key(2).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(first.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*first).size(),
        std::size_t(2));

    const auto empty = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(empty.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(*empty).size(),
        std::size_t(2));
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailUpdateRetentionEffect>(*empty).size(),
        std::size_t(2));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailScheduler)

#include "test_activenavigationthumbnailscheduler.moc"
