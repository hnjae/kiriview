// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <initializer_list>
#include <optional>
#include <variant>
#include <vector>

namespace {
using Bucket = kiriview::ActiveNavigationThumbnailDemandBucket;
using Priority = kiriview::ActiveNavigationThumbnailDemandPriority;

kiriview::ThumbnailSourceRevisionKey key(int number, quint64 generation = 1)
{
    return kiriview::thumbnailSourceRevisionKey(number,
        QUrl::fromLocalFile(QStringLiteral("/media/%1.png").arg(number)),
        QStringLiteral("%1.png").arg(number), QStringLiteral("image"),
        kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
        generation);
}

kiriview::ThumbnailSourceAdapter adapter()
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
        kiriview::ActiveNavigationThumbnailReadyWorkResult { {}, std::nullopt } };
}

kiriview::ActiveNavigationThumbnailDemandSnapshot snapshot(
    quint64 generation, std::initializer_list<kiriview::ActiveNavigationThumbnailDemand> demands)
{
    return { generation, demands };
}

kiriview::ActiveNavigationThumbnailSchedulingSnapshot schedulingSnapshot(
    quint64 generation, std::initializer_list<kiriview::ThumbnailSourceRevisionKey> rows)
{
    return { generation, rows };
}

kiriview::ActiveNavigationThumbnailSchedulingSnapshot schedulingSnapshot(
    quint64 generation, std::vector<kiriview::ThumbnailSourceRevisionKey> rows)
{
    return { generation, std::move(rows) };
}
}

class TestActiveNavigationThumbnailScheduler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void demandWindowIsAtomicAndRejectsOutsideReports();
    void visibleRunsBeforeNearbyRegardlessOfReportOrder();
    void foregroundAdmissionIsBoundedAndPrioritizesCurrentThenVisible();
    void currentPreemptsVisibleWhenForegroundCapacityIsFull();
    void currentPromotesCommittedNearbyWithoutRestart();
    void newerForegroundCancelsActiveNearby();
    void newerWindowExpiresMissingDemandAndDemotesRetention();
    void backgroundRunsOneAtATimeAndYieldsToDemand();
    void invalidSnapshotIsRejectedWithoutReplacingCommittedDemand();
    void duplicateSnapshotFactsAreRejectedBeforeSourcePlanning();
    void emptySnapshotExpiresNonCurrentDemand();
    void backgroundScanYieldsAndResumesWithEpoch();
    void staleContinuationIsRejectedAfterReset();
    void foregroundDoesNotWaitForBackgroundContinuation();
    void movingCurrentExpiresUnreportedPinnedDemand();
    void malformedSchedulingSnapshotIsRejectedAtomically();
};

void TestActiveNavigationThumbnailScheduler::demandWindowIsAtomicAndRejectsOutsideReports()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    QVERIFY(!scheduler
            .replaceDemandSnapshot(
                snapshot(2, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }))
            .has_value());
    const auto accepted = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(accepted.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).size(),
        std::size_t(1));
}

void TestActiveNavigationThumbnailScheduler::visibleRunsBeforeNearbyRegardlessOfReportOrder()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1), key(2) })).has_value());
    auto committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 2, key(2).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    auto effects = std::move(*committed);
    auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 1);

    effects = scheduler.acceptCompletion(ready(starts.front()));
    starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 2);
}

void TestActiveNavigationThumbnailScheduler::
    foregroundAdmissionIsBoundedAndPrioritizesCurrentThenVisible()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1), key(2), key(3), key(4), key(5) }))
            .has_value());
    scheduler.setCurrentNumber(4);

    const auto committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 5, key(5).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 3, key(3).sourceUrl, Bucket::Normal, Priority::Visible },
            { 4, key(4).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 2, key(2).sourceUrl, Bucket::Normal, Priority::Visible },
            { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed);
    QCOMPARE(starts.size(), std::size_t(2));
    QCOMPARE(starts.at(0).request.sourceKey.row.rowNumber, 4);
    QCOMPARE(starts.at(1).request.sourceKey.row.rowNumber, 1);

    const auto effects = scheduler.acceptCompletion(ready(starts.at(0)));
    starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 2);
}

void TestActiveNavigationThumbnailScheduler::currentPreemptsVisibleWhenForegroundCapacityIsFull()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1), key(2), key(3) })).has_value());
    const auto committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible },
            { 2, key(2).sourceUrl, Bucket::Normal, Priority::Visible },
            { 3, key(3).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).size(),
        std::size_t(2));

    const auto effects = scheduler.setCurrentNumber(3);
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects).size(),
        std::size_t(1));
    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 3);
}

void TestActiveNavigationThumbnailScheduler::currentPromotesCommittedNearbyWithoutRestart()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby } }));
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
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1), key(2) })).has_value());
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 2, key(2).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto nearbyStart
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).front();

    committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible },
            { 2, key(2).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto& effects = *committed;
    const auto cancels
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects);
    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId.value, nearbyStart.request.workId.value);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 1);
}

void TestActiveNavigationThumbnailScheduler::newerWindowExpiresMissingDemandAndDemotesRetention()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1), key(2) })).has_value());
    auto committed = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible },
            { 2, key(2).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).size(),
        std::size_t(2));

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto& effects = *committed;
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects).size(),
        std::size_t(1));
    const auto retention
        = effectsOfType<kiriview::ActiveNavigationThumbnailUpdateRetentionEffect>(effects);
    QCOMPARE(retention.size(), std::size_t(1));
    QCOMPARE(retention.front().sourceKey.row.rowNumber, 2);
    QCOMPARE(retention.front().retentionClass,
        kiriview::ActiveNavigationThumbnailRetentionClass::Background);
}

void TestActiveNavigationThumbnailScheduler::backgroundRunsOneAtATimeAndYieldsToDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
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
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::XLarge, Priority::Visible } }));
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
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto accepted = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(accepted.has_value());
    const auto foreground
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).front();

    const auto rejected = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 2, key(2).sourceUrl, Bucket::Large, Priority::Visible } }));
    QVERIFY(!rejected.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailAcceptCompletionEffect>(
                 scheduler.acceptCompletion(ready(foreground)))
                 .size(),
        std::size_t(1));
}

void TestActiveNavigationThumbnailScheduler::duplicateSnapshotFactsAreRejectedBeforeSourcePlanning()
{
    int adapterCalls = 0;
    kiriview::ActiveNavigationThumbnailScheduler scheduler(
        [&adapterCalls](kiriview::ThumbnailSourceAdapterRequest request) {
            ++adapterCalls;
            const QByteArray path = request.sourceKey.sourceUrl.toLocalFile().toUtf8();
            return kiriview::ThumbnailSourceAdapterPlan {
                kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile,
                path,
                kiriview::ThumbnailOriginalIdentity::fromLocalPathBytes(path),
                {},
            };
        },
        2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto rejected = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby },
            { 1, key(1).sourceUrl, Bucket::XLarge, Priority::Visible } }));
    QVERIFY(!rejected.has_value());
    QCOMPARE(adapterCalls, 0);
}

void TestActiveNavigationThumbnailScheduler::emptySnapshotExpiresNonCurrentDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1), key(2) })).has_value());
    const auto first = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible },
            { 2, key(2).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(first.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*first).size(),
        std::size_t(2));

    const auto empty = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(empty.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(*empty).size(),
        std::size_t(2));
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailUpdateRetentionEffect>(*empty).size(),
        std::size_t(2));

    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*first);
    for (const auto& start : starts) {
        QVERIFY(scheduler.acceptCompletion(ready(start)).empty());
    }
}

void TestActiveNavigationThumbnailScheduler::backgroundScanYieldsAndResumesWithEpoch()
{
    int adapterCalls = 0;
    kiriview::ActiveNavigationThumbnailScheduler scheduler(
        [&adapterCalls](kiriview::ThumbnailSourceAdapterRequest request) {
            ++adapterCalls;
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
        },
        2);
    std::vector<kiriview::ThumbnailSourceRevisionKey> rows;
    for (int number = 1; number <= 20; ++number) {
        rows.push_back(key(number));
    }
    QVERIFY(scheduler.reset(schedulingSnapshot(1, std::move(rows))).has_value());

    const auto accepted = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(accepted.has_value());
    QVERIFY(adapterCalls < 20);
    const auto continuations
        = effectsOfType<kiriview::ActiveNavigationThumbnailScheduleContinuationEffect>(*accepted);
    QCOMPARE(continuations.size(), std::size_t(1));
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).empty());

    const auto resumed = scheduler.continueAdmission(continuations.front().admissionEpoch);
    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(resumed);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 20);
    QCOMPARE(
        starts.front().request.workKind, kiriview::ActiveNavigationThumbnailWorkKind::Background);
}

void TestActiveNavigationThumbnailScheduler::staleContinuationIsRejectedAfterReset()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(
        [](kiriview::ThumbnailSourceAdapterRequest) {
            return kiriview::ThumbnailSourceAdapterPlan {};
        },
        2);
    std::vector<kiriview::ThumbnailSourceRevisionKey> rows;
    for (int number = 1; number <= 20; ++number) {
        rows.push_back(key(number));
    }
    QVERIFY(scheduler.reset(schedulingSnapshot(1, std::move(rows))).has_value());
    const auto accepted = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(accepted.has_value());
    const auto continuation
        = effectsOfType<kiriview::ActiveNavigationThumbnailScheduleContinuationEffect>(*accepted)
              .front();

    QVERIFY(scheduler.reset(schedulingSnapshot(2, { key(1, 2) })).has_value());
    QVERIFY(scheduler.continueAdmission(continuation.admissionEpoch).empty());
}

void TestActiveNavigationThumbnailScheduler::foregroundDoesNotWaitForBackgroundContinuation()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(
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
        },
        2);
    std::vector<kiriview::ThumbnailSourceRevisionKey> rows;
    for (int number = 1; number <= 20; ++number) {
        rows.push_back(key(number));
    }
    QVERIFY(scheduler.reset(schedulingSnapshot(1, std::move(rows))).has_value());
    const auto empty = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(empty.has_value());
    const auto continuation
        = effectsOfType<kiriview::ActiveNavigationThumbnailScheduleContinuationEffect>(*empty)
              .front();

    const auto foreground = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 20, key(20).sourceUrl, Bucket::Large, Priority::Visible } }));
    QVERIFY(foreground.has_value());
    const auto starts
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*foreground);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(
        starts.front().request.workKind, kiriview::ActiveNavigationThumbnailWorkKind::Foreground);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
        scheduler.continueAdmission(continuation.admissionEpoch))
            .empty());
}

void TestActiveNavigationThumbnailScheduler::movingCurrentExpiresUnreportedPinnedDemand()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1), key(2) })).has_value());
    scheduler.setCurrentNumber(1);
    const auto accepted = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(accepted.has_value());
    const auto start
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).front();

    const auto empty = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(empty.has_value());
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(*empty).empty());

    const auto moved = scheduler.setCurrentNumber(2);
    const auto cancels = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(moved);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId, start.request.workId);
    const auto retention
        = effectsOfType<kiriview::ActiveNavigationThumbnailUpdateRetentionEffect>(moved);
    QCOMPARE(retention.size(), std::size_t(1));
    QCOMPARE(retention.front().sourceKey.row.rowNumber, 1);
    QCOMPARE(retention.front().retentionClass,
        kiriview::ActiveNavigationThumbnailRetentionClass::Background);
}

void TestActiveNavigationThumbnailScheduler::malformedSchedulingSnapshotIsRejectedAtomically()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(kiriview::ActiveNavigationThumbnailSchedulingSnapshot { 1, { key(1) } })
            .has_value());
    const auto accepted = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(accepted.has_value());
    const auto foreground
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).front();

    QVERIFY(
        !scheduler.reset(kiriview::ActiveNavigationThumbnailSchedulingSnapshot { 2, { key(1) } })
            .has_value());
    const auto duplicateDemand = kiriview::thumbnailSourceRevisionKey(1, key(1).sourceUrl,
        QStringLiteral("other-label.png"), QStringLiteral("image"),
        kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
        1);
    QVERIFY(!scheduler
            .reset(kiriview::ActiveNavigationThumbnailSchedulingSnapshot {
                1, { key(1), duplicateDemand } })
            .has_value());

    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailAcceptCompletionEffect>(
                 scheduler.acceptCompletion(ready(foreground)))
                 .size(),
        std::size_t(1));
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailScheduler)

#include "tst_activenavigationthumbnailscheduler.moc"
