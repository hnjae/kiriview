// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailscheduler.h"

#include <QObject>
#include <QTest>
#include <QUrl>
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
};

void TestActiveNavigationThumbnailScheduler::demandWindowIsAtomicAndRejectsOutsideReports()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1) }, 1);
    QVERIFY(!scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Visible, 1));
    QVERIFY(scheduler.beginDemandWindow(1));
    QVERIFY(scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Visible, 1));
    QVERIFY(effectsOfKind(scheduler.finishDemandWindow(1), Effect::StartWork).size() == 1);
}

void TestActiveNavigationThumbnailScheduler::visibleRunsBeforeNearbyRegardlessOfReportOrder()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    QVERIFY(scheduler.beginDemandWindow(1));
    scheduler.reportDemand(2, key(2).url, Bucket::Normal, Priority::Nearby, 1);
    scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Visible, 1);
    auto effects = scheduler.finishDemandWindow(1);
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
    scheduler.beginDemandWindow(1);
    scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Nearby, 1);
    const auto starts = effectsOfKind(scheduler.finishDemandWindow(1), Effect::StartWork);
    QCOMPARE(starts.size(), std::size_t(1));

    const auto effects = scheduler.setCurrentNumber(1);
    QVERIFY(effectsOfKind(effects, Effect::CancelWork).empty());
    QVERIFY(effectsOfKind(effects, Effect::StartWork).empty());
}

void TestActiveNavigationThumbnailScheduler::newerForegroundCancelsActiveNearby()
{
    kiriview::ActiveNavigationThumbnailScheduler scheduler(adapter());
    scheduler.reset({ key(1), key(2) }, 1);
    scheduler.beginDemandWindow(1);
    scheduler.reportDemand(2, key(2).url, Bucket::Normal, Priority::Nearby, 1);
    const auto nearbyStart
        = effectsOfKind(scheduler.finishDemandWindow(1), Effect::StartWork).front();

    scheduler.beginDemandWindow(1);
    scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Visible, 1);
    scheduler.reportDemand(2, key(2).url, Bucket::Normal, Priority::Nearby, 1);
    const auto effects = scheduler.finishDemandWindow(1);
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
    scheduler.beginDemandWindow(1);
    scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Visible, 1);
    scheduler.reportDemand(2, key(2).url, Bucket::Normal, Priority::Visible, 1);
    QCOMPARE(
        effectsOfKind(scheduler.finishDemandWindow(1), Effect::StartWork).size(), std::size_t(2));

    scheduler.beginDemandWindow(1);
    scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Visible, 1);
    const auto effects = scheduler.finishDemandWindow(1);
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
    scheduler.beginDemandWindow(1);
    scheduler.reportDemand(1, key(1).url, Bucket::Normal, Priority::Visible, 1);
    const auto foreground
        = effectsOfKind(scheduler.finishDemandWindow(1), Effect::StartWork).front();
    auto effects = scheduler.acceptCompletion(ready(foreground));
    const auto background = effectsOfKind(effects, Effect::StartWork);
    QCOMPARE(background.size(), std::size_t(1));
    QCOMPARE(background.front().workRequest.workKind,
        kiriview::ActiveNavigationThumbnailWorkKind::Background);
    QCOMPARE(background.front().workRequest.bucket, Bucket::Large);

    scheduler.beginDemandWindow(1);
    scheduler.reportDemand(1, key(1).url, Bucket::XLarge, Priority::Visible, 1);
    effects = scheduler.finishDemandWindow(1);
    QCOMPARE(effectsOfKind(effects, Effect::CancelWork).size(), std::size_t(1));
    const auto starts = effectsOfKind(effects, Effect::StartWork);
    QCOMPARE(starts.size(), std::size_t(1));
    QCOMPARE(starts.front().workRequest.workKind,
        kiriview::ActiveNavigationThumbnailWorkKind::Foreground);
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailScheduler)

#include "test_activenavigationthumbnailscheduler.moc"
