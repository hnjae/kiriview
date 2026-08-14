// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include "decoding/imagedecodeworkspace.h"

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

kiriview::ThumbnailSourceAdapter foregroundOnlyAdapter()
{
    return [](kiriview::ThumbnailSourceAdapterRequest request) {
        if (request.priority != Priority::Visible) {
            return kiriview::ThumbnailSourceAdapterPlan {};
        }
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
    void defaultAdapterBypassesPersistentCacheForOwnerFreshness();
    void demandWindowIsAtomicAndRejectsOutsideReports();
    void visibleRunsBeforeNearbyRegardlessOfReportOrder();
    void foregroundAdmissionIsBoundedAndPrioritizesCurrentThenVisible();
    void currentPreemptsVisibleWhenForegroundCapacityIsFull();
    void resetKeepsCanceledWorkCapacityUntilPhysicalRetirement();
    void currentPromotionRestartsWithDemandedPriorityAfterRetirement();
    void visiblePromotionRestartsWithDemandedPriorityAfterRetirement();
    void currentPromotionKeepsAlreadyDemandedWork();
    void nearbyDemotionRestartsWithSpeculativePriorityAfterRetirement();
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
    void residencyLossWaitsForAdmissionOpportunity();
    void meaningfulDemandChangeReleasesResidencyBlock();
    void priorityAndCurrentChangesReleaseResidencyBlock();
    void sourceRefreshReleasesResidencyBlockWithRefreshedPlan();
    void residencyLossIgnoresStaleAndUnsupportedRows();
    void residencyLossDoesNotClearBackgroundCompletion();
    void malformedSchedulingSnapshotIsRejectedAtomically();
};

void TestActiveNavigationThumbnailScheduler::
    defaultAdapterBypassesPersistentCacheForOwnerFreshness()
{
    const QUrl url = QUrl::fromLocalFile(QStringLiteral("/media/1.png"));
    const auto unchanged = kiriview::thumbnailSourceRevisionKey(1, url, QStringLiteral("1.png"),
        QStringLiteral("image"),
        kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
        1);
    const auto refreshed = kiriview::thumbnailSourceRevisionKey(1, url, QStringLiteral("1.png"),
        QStringLiteral("image"),
        kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
        1, 7);
    const auto adapter = kiriview::defaultThumbnailSourceAdapter();

    const kiriview::ThumbnailSourceAdapterPlan unchangedPlan
        = adapter({ unchanged, Bucket::Normal, Priority::Visible });
    QCOMPARE(unchangedPlan.kind, kiriview::ThumbnailSourceAdapterPlanKind::CacheableLocalFile);

    const kiriview::ThumbnailSourceAdapterPlan refreshedPlan
        = adapter({ refreshed, Bucket::Normal, Priority::Visible });
    QCOMPARE(refreshedPlan.kind, kiriview::ThumbnailSourceAdapterPlanKind::InMemoryOnly);
    QCOMPARE(refreshedPlan.localPathBytes, unchangedPlan.localPathBytes);
    QCOMPARE(refreshedPlan.originalIdentity.localPathBytes,
        unchangedPlan.originalIdentity.localPathBytes);
}

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
    QCOMPARE(
        starts.front().request.workspacePriority, kiriview::ImageDecodeWorkspacePriority::Demanded);

    effects = scheduler.acceptCompletion(ready(starts.front()));
    starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 2);
    QCOMPARE(starts.front().request.workspacePriority,
        kiriview::ImageDecodeWorkspacePriority::Speculative);
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
    const auto cancels
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects);
    QCOMPARE(cancels.size(), std::size_t(1));
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects).empty());

    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
        scheduler.acceptRetirement(cancels.front().workId));
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 3);
}

void TestActiveNavigationThumbnailScheduler::resetKeepsCanceledWorkCapacityUntilPhysicalRetirement()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 1);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto first = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(first.has_value());
    const auto firstStart
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*first).front();

    const auto reset = scheduler.reset(schedulingSnapshot(2, { key(2, 2) }));
    QVERIFY(reset.has_value());
    const auto cancels = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(*reset);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId, firstStart.request.workId);
    const auto replacement = scheduler.replaceDemandSnapshot(
        snapshot(2, { { 2, key(2, 2).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(replacement.has_value());
    QVERIFY(
        effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*replacement).empty());

    const auto replacementStarts
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
            scheduler.acceptRetirement(firstStart.request.workId));
    QCOMPARE(replacementStarts.size(), std::size_t(1));
    QCOMPARE(replacementStarts.front().request.sourceKey, key(2, 2));
    QVERIFY(scheduler.acceptRetirement(firstStart.request.workId).empty());
}

void TestActiveNavigationThumbnailScheduler::
    currentPromotionRestartsWithDemandedPriorityAfterRetirement()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto starts
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.workspacePriority,
        kiriview::ImageDecodeWorkspacePriority::Speculative);

    const auto effects = scheduler.setCurrentNumber(1);
    const auto cancels
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId, starts.front().request.workId);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects).empty());

    const auto replacements = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
        scheduler.acceptRetirement(cancels.front().workId));
    QCOMPARE(replacements.size(), std::size_t(1));
    QCOMPARE(replacements.front().request.sourceKey, starts.front().request.sourceKey);
    QCOMPARE(replacements.front().request.workspacePriority,
        kiriview::ImageDecodeWorkspacePriority::Demanded);
}

void TestActiveNavigationThumbnailScheduler::
    visiblePromotionRestartsWithDemandedPriorityAfterRetirement()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto speculative
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).front();

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto cancels
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(*committed);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId, speculative.request.workId);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).empty());

    const auto replacements = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
        scheduler.acceptRetirement(cancels.front().workId));
    QCOMPARE(replacements.size(), std::size_t(1));
    QCOMPARE(replacements.front().request.workspacePriority,
        kiriview::ImageDecodeWorkspacePriority::Demanded);
}

void TestActiveNavigationThumbnailScheduler::currentPromotionKeepsAlreadyDemandedWork()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto demanded
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).front();
    QCOMPARE(demanded.request.workspacePriority, kiriview::ImageDecodeWorkspacePriority::Demanded);

    const auto promoted = scheduler.setCurrentNumber(1);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(promoted).empty());
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(promoted).empty());
}

void TestActiveNavigationThumbnailScheduler::
    nearbyDemotionRestartsWithSpeculativePriorityAfterRetirement()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    auto committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(committed.has_value());
    const auto demanded
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).front();

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(committed.has_value());
    const auto cancels
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(*committed);
    QCOMPARE(cancels.size(), std::size_t(1));
    QCOMPARE(cancels.front().workId, demanded.request.workId);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*committed).empty());

    const auto replacements = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
        scheduler.acceptRetirement(cancels.front().workId));
    QCOMPARE(replacements.size(), std::size_t(1));
    QCOMPARE(replacements.front().request.workspacePriority,
        kiriview::ImageDecodeWorkspacePriority::Speculative);
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
    QCOMPARE(background.front().request.workspacePriority,
        kiriview::ImageDecodeWorkspacePriority::Speculative);

    committed = scheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::XLarge, Priority::Visible } }));
    QVERIFY(committed.has_value());
    effects = std::move(*committed);
    const auto cancels
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(effects);
    QCOMPARE(cancels.size(), std::size_t(1));
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(effects).empty());

    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
        scheduler.acceptRetirement(cancels.front().workId));
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

void TestActiveNavigationThumbnailScheduler::residencyLossWaitsForAdmissionOpportunity()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(foregroundOnlyAdapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto demand = snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } });
    const auto accepted = scheduler.replaceDemandSnapshot(demand);
    QVERIFY(accepted.has_value());
    const auto firstStart
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).front();
    scheduler.acceptCompletion(ready(firstStart));

    const auto blocked = scheduler.reconcileImageResidency({ key(1) }, false);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(blocked).empty());

    const auto repeated = scheduler.replaceDemandSnapshot(demand);
    QVERIFY(repeated.has_value());
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*repeated).empty());

    const auto retried = scheduler.reconcileImageResidency({}, true);
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailApplyPendingEffect>(retried).size(),
        std::size_t(1));
    const auto retryStarts
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(retried);
    QCOMPARE(retryStarts.size(), std::size_t(1));
    QCOMPARE(retryStarts.front().request.sourceKey, key(1));
}

void TestActiveNavigationThumbnailScheduler::meaningfulDemandChangeReleasesResidencyBlock()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(foregroundOnlyAdapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto normalDemand
        = snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } });
    auto effects = scheduler.replaceDemandSnapshot(normalDemand);
    QVERIFY(effects.has_value());
    scheduler.acceptCompletion(
        ready(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects).front()));
    scheduler.reconcileImageResidency({ key(1) }, false);

    const auto largerDemand
        = snapshot(1, { { 1, key(1).sourceUrl, Bucket::Large, Priority::Visible } });
    effects = scheduler.replaceDemandSnapshot(largerDemand);

    QVERIFY(effects.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailApplyPendingEffect>(*effects).size(),
        std::size_t(1));
    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.bucket, Bucket::Large);
}

void TestActiveNavigationThumbnailScheduler::priorityAndCurrentChangesReleaseResidencyBlock()
{
    kiriview::ActiveNavigationThumbnailScheduler priorityScheduler(adapter(), 2);
    QVERIFY(priorityScheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    auto effects = priorityScheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(effects.has_value());
    priorityScheduler.acceptCompletion(
        ready(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects).front()));
    const auto priorityBlocked = priorityScheduler.reconcileImageResidency({ key(1) }, false);
    const auto priorityCancellations
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(priorityBlocked);
    QCOMPARE(priorityCancellations.size(), std::size_t(1));

    effects = priorityScheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } }));

    QVERIFY(effects.has_value());
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects).empty());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
                 priorityScheduler.acceptRetirement(priorityCancellations.front().workId))
                 .size(),
        std::size_t(1));

    kiriview::ActiveNavigationThumbnailScheduler currentScheduler(adapter(), 2);
    QVERIFY(currentScheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    effects = currentScheduler.replaceDemandSnapshot(
        snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Nearby } }));
    QVERIFY(effects.has_value());
    currentScheduler.acceptCompletion(
        ready(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects).front()));
    const auto currentBlocked = currentScheduler.reconcileImageResidency({ key(1) }, false);
    const auto currentCancellations
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(currentBlocked);
    QCOMPARE(currentCancellations.size(), std::size_t(1));

    const auto promoted = currentScheduler.setCurrentNumber(1);

    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(promoted).empty());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
                 currentScheduler.acceptRetirement(currentCancellations.front().workId))
                 .size(),
        std::size_t(1));
}

void TestActiveNavigationThumbnailScheduler::sourceRefreshReleasesResidencyBlockWithRefreshedPlan()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    const auto original = kiriview::thumbnailSourceRevisionKey(1,
        QUrl(QStringLiteral("file:///media/folder/../1.png")), QStringLiteral("1.png"),
        QStringLiteral("image"),
        kiriview::activeNavigationThumbnailSourceKindIdentity(
            kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
        1);
    const auto refreshed
        = kiriview::thumbnailSourceRevisionKey(1, QUrl(QStringLiteral("file:///media/1.png")),
            QStringLiteral("1.png"), QStringLiteral("image"),
            kiriview::activeNavigationThumbnailSourceKindIdentity(
                kiriview::ActiveNavigationThumbnailSourceKind::DirectImage),
            1);
    QCOMPARE(original, refreshed);
    QVERIFY(original.sourceUrl != refreshed.sourceUrl);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { original, key(2) })).has_value());
    auto effects = scheduler.replaceDemandSnapshot(snapshot(1,
        { { 1, original.sourceUrl, Bucket::Normal, Priority::Visible },
            { 2, key(2).sourceUrl, Bucket::Normal, Priority::Visible } }));
    QVERIFY(effects.has_value());
    const auto initialStarts
        = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects);
    QCOMPARE(initialStarts.size(), std::size_t(2));
    scheduler.acceptCompletion(ready(initialStarts.at(0)));
    scheduler.acceptCompletion(ready(initialStarts.at(1)));
    const auto blocked = scheduler.reconcileImageResidency({ original, key(2) }, false);
    const auto cancellations
        = effectsOfType<kiriview::ActiveNavigationThumbnailCancelWorkEffect>(blocked);
    QCOMPARE(cancellations.size(), std::size_t(1));

    effects = scheduler.refreshRows(schedulingSnapshot(1, { refreshed, key(2) }));

    QVERIFY(effects.has_value());
    QCOMPARE(effectsOfType<kiriview::ActiveNavigationThumbnailApplyPendingEffect>(*effects).size(),
        std::size_t(1));
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects).empty());
    const auto starts = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(
        scheduler.acceptRetirement(cancellations.front().workId));
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().request.sourceKey.row.rowNumber, 1);
    QCOMPARE(starts.front().request.sourceKey.sourceUrl, refreshed.sourceUrl);
    QCOMPARE(starts.front().request.sourcePlan.localPathBytes,
        refreshed.sourceUrl.toLocalFile().toUtf8());
}

void TestActiveNavigationThumbnailScheduler::residencyLossIgnoresStaleAndUnsupportedRows()
{
    kiriview::ActiveNavigationThumbnailScheduler supported(foregroundOnlyAdapter(), 2);
    QVERIFY(supported.reset(schedulingSnapshot(1, { key(1) })).has_value());
    const auto demand = snapshot(1, { { 1, key(1).sourceUrl, Bucket::Normal, Priority::Visible } });
    auto accepted = supported.replaceDemandSnapshot(demand);
    QVERIFY(accepted.has_value());

    const auto incomplete = supported.reconcileImageResidency({ key(1) }, false);
    QVERIFY(
        effectsOfType<kiriview::ActiveNavigationThumbnailApplyPendingEffect>(incomplete).empty());
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(incomplete).empty());

    supported.acceptCompletion(ready(
        effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*accepted).front()));

    kiriview::ThumbnailSourceRevisionKey staleKey = key(1);
    ++staleKey.navigationGeneration;
    const auto stale = supported.reconcileImageResidency({ staleKey }, false);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(stale).empty());

    kiriview::ActiveNavigationThumbnailScheduler unsupported(
        [](kiriview::ThumbnailSourceAdapterRequest) {
            return kiriview::ThumbnailSourceAdapterPlan {};
        },
        2);
    QVERIFY(unsupported.reset(schedulingSnapshot(1, { key(1) })).has_value());
    accepted = unsupported.replaceDemandSnapshot(demand);
    QVERIFY(accepted.has_value());
    const auto repeated = unsupported.reconcileImageResidency({ key(1) }, true);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailApplyPendingEffect>(repeated).empty());
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(repeated).empty());
}

void TestActiveNavigationThumbnailScheduler::residencyLossDoesNotClearBackgroundCompletion()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter(), 2);
    QVERIFY(scheduler.reset(schedulingSnapshot(1, { key(1) })).has_value());
    auto effects = scheduler.replaceDemandSnapshot(snapshot(1, {}));
    QVERIFY(effects.has_value());
    for (int completed = 0; completed < 4; ++completed) {
        const auto starts
            = effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects);
        QCOMPARE(starts.size(), std::size_t(1));
        QCOMPARE(starts.front().request.workKind,
            kiriview::ActiveNavigationThumbnailWorkKind::Background);
        const auto completionEffects = scheduler.acceptCompletion(ready(starts.front()));
        const auto continuations
            = effectsOfType<kiriview::ActiveNavigationThumbnailScheduleContinuationEffect>(
                completionEffects);
        QCOMPARE(continuations.size(), std::size_t(1));
        effects = scheduler.continueAdmission(continuations.front().admissionEpoch);
    }
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(*effects).empty());

    const auto repeated = scheduler.reconcileImageResidency({ key(1) }, true);
    QVERIFY(effectsOfType<kiriview::ActiveNavigationThumbnailStartWorkEffect>(repeated).empty());
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
