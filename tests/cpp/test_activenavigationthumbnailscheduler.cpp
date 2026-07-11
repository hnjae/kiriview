// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <initializer_list>
#include <vector>

namespace {
using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;
using Effect = kiriview::ActiveNavigationThumbnailScheduleEffectKind;
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

std::vector<kiriview::ActiveNavigationThumbnailScheduleEffect> effectsOfKind(
    const std::vector<kiriview::ActiveNavigationThumbnailScheduleEffect>& effects, Effect kind)
{
    std::vector<kiriview::ActiveNavigationThumbnailScheduleEffect> result;
    for (const auto& effect : effects) {
        if (effect.kind == kind) {
            result.push_back(effect);
        }
    }
    return result;
}

kiriview::ActiveNavigationThumbnailWorkCompletion ready(
    const kiriview::ActiveNavigationThumbnailScheduleEffect& start)
{
    return { start.workRequest.workId, start.workRequest.sourceKey, start.workRequest.bucket,
        start.workRequest.workKind,
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
    QCOMPARE(effectsOfKind(*accepted, Effect::StartWork).size(), std::size_t(1));
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
    auto starts = effectsOfKind(effects, Effect::StartWork);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().sourceKey.rowNumber, 0);
    QCOMPARE(starts.front().workRequest.sourceKey.rowNumber, 1);

    effects = scheduler.acceptCompletion(ready(starts.front()));
    starts = effectsOfKind(effects, Effect::StartWork);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().workRequest.sourceKey.rowNumber, 2);
}

void TestActiveNavigationThumbnailScheduler::currentPromotesCommittedNearbyWithoutRestart()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    const auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto starts = effectsOfKind(*committed, Effect::StartWork);
    QCOMPARE(starts.size(), std::size_t(1));

    const auto effects = scheduler.setCurrentNumber(1);
    QVERIFY(effectsOfKind(effects, Effect::CancelWork).empty());
    QVERIFY(effectsOfKind(effects, Effect::StartWork).empty());
}

void TestActiveNavigationThumbnailScheduler::newerForegroundCancelsActiveNearby()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 2, key(2).url, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto nearbyStart = effectsOfKind(*committed, Effect::StartWork).front();

    committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).url, Bucket::Normal, Priority::Visible },
            { 2, key(2).url, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto& effects = *committed;
    const auto cancels = effectsOfKind(effects, Effect::CancelWork);
    const auto starts = effectsOfKind(effects, Effect::StartWork);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId.value, nearbyStart.workId.value);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().workRequest.sourceKey.rowNumber, 1);
}

void TestActiveNavigationThumbnailScheduler::newerWindowExpiresMissingDemandAndDemotesRetention()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    auto committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).url, Bucket::Normal, Priority::Visible },
            { 2, key(2).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    QCOMPARE(effectsOfKind(*committed, Effect::StartWork).size(), std::size_t(2));

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto& effects = *committed;
    QCOMPARE(effectsOfKind(effects, Effect::CancelWork).size(), std::size_t(1));
    const auto retention = effectsOfKind(effects, Effect::UpdateRetention);
    QCOMPARE(retention.size(), std::size_t(1));
    QCOMPARE(retention.front().sourceKey.rowNumber, 2);
    QCOMPARE(
        retention.front().retentionPriority, kiriview::ThumbnailImageRetentionPriority::Background);
}

void TestActiveNavigationThumbnailScheduler::backgroundRunsOneAtATimeAndYieldsToDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto foreground = effectsOfKind(*committed, Effect::StartWork).front();
    auto effects = scheduler.acceptCompletion(ready(foreground));
    const auto background = effectsOfKind(effects, Effect::StartWork);
    QCOMPARE(background.size(), std::size_t(1));
    QCOMPARE(background.front().workRequest.workKind,
        kiriview::ActiveNavigationThumbnailWorkKind::Background);
    QCOMPARE(background.front().workRequest.bucket, Bucket::Large);

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::XLarge, Priority::Visible } }));
    QVERIFY(committed.has_value());
    effects = std::move(*committed);
    QCOMPARE(effectsOfKind(effects, Effect::CancelWork).size(), std::size_t(1));
    const auto starts = effectsOfKind(effects, Effect::StartWork);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().workRequest.workKind,
        kiriview::ActiveNavigationThumbnailWorkKind::Foreground);
}

void TestActiveNavigationThumbnailScheduler::
    invalidSnapshotIsRejectedWithoutReplacingCommittedDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    const auto accepted = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(accepted.has_value());
    const auto foreground = effectsOfKind(*accepted, Effect::StartWork).front();

    const auto rejected = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 2, key(2).url, Bucket::Large, Priority::Visible } }));
    QVERIFY(!rejected.has_value());
    QCOMPARE(effectsOfKind(scheduler.acceptCompletion(ready(foreground)), Effect::AcceptCompletion)
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
    const auto starts = effectsOfKind(*accepted, Effect::StartWork);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().workRequest.bucket, Bucket::XLarge);
}

void TestActiveNavigationThumbnailScheduler::emptySnapshotExpiresNonCurrentDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    const auto first = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).url, Bucket::Normal, Priority::Visible },
            { 2, key(2).url, Bucket::Normal, Priority::Visible } }));
    QVERIFY(first.has_value());
    QCOMPARE(effectsOfKind(*first, Effect::StartWork).size(), std::size_t(2));

    const auto empty = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(empty.has_value());
    QCOMPARE(effectsOfKind(*empty, Effect::CancelWork).size(), std::size_t(2));
    QCOMPARE(effectsOfKind(*empty, Effect::UpdateRetention).size(), std::size_t(2));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailScheduler)

#include "test_activenavigationthumbnailscheduler.moc"
