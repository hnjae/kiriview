// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "predecode/mediapredecodescheduleplan.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace {
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::staticDisplayTestImagePayload;
using kiriview::TestSupport::testImage;

struct UnrelatedPredecodeSchedulePayload final : kiriview::PredecodeSchedulePayload
{
};

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

kiriview::DirectMediaNavigationCandidateSnapshot directMediaNavigationCandidateSnapshot(
    std::vector<kiriview::DirectMediaNavigationCandidate> candidates, quint64 revision = 1)
{
    kiriview::DirectMediaNavigationCandidateSnapshot snapshot;
    if (!candidates.empty()) {
        snapshot.source = kiriview::DirectMediaScope::fromSource(
            kiriview::ResolvedNavigationSource(candidates.front().url, {}, candidates.front().url),
            7);
    }
    snapshot.revision = revision;
    snapshot.candidates
        = std::make_shared<const std::vector<kiriview::DirectMediaNavigationCandidate>>(
            std::move(candidates));
    snapshot.boundaryState.currentNumber = 2;
    snapshot.boundaryState.count = static_cast<int>(snapshot.candidates->size());
    snapshot.known = true;
    return snapshot;
}

kiriview::DisplayedPredecodeImage displayedImage(const QUrl& url)
{
    return kiriview::DisplayedPredecodeImage {
        kiriview::DisplayedImageLocation::fromUrl(url),
        true,
        staticDisplayTestImagePayload(testImage()),
    };
}

const kiriview::DirectMediaNavigationCandidateSnapshot* scheduleCandidateSnapshot(
    const kiriview::MediaPredecodeSchedulePlan& plan)
{
    return kiriview::mediaPredecodeScheduleCandidateSnapshot(
        kiriview::PredecodePendingSchedule { plan.context, 1 });
}

const kiriview::MediaPredecodeEligibilitySnapshot* scheduleEligibility(
    const kiriview::MediaPredecodeSchedulePlan& plan)
{
    return kiriview::mediaPredecodeScheduleEligibility(
        kiriview::PredecodePendingSchedule { plan.context, 1 });
}
}

class TestMediaPredecodeSchedulePlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void videoCursorBuildsScheduleContextAndCarriesCandidates();
    void missingCurrentCandidateKeepsUnknownPageIndex();
    void invalidCursorYieldsEmptyScheduleContext();
    void mediaPayloadAccessorsRejectWrongPayloadType();
};

void TestMediaPredecodeSchedulePlan::videoCursorBuildsScheduleContextAndCarriesCandidates()
{
    const QUrl displayedUrl = localUrl(QStringLiteral("/media/00.png"));
    const QUrl videoUrl(QStringLiteral("file:///media/chapter/../01.mp4"));
    const QUrl normalizedVideoUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));
    const kiriview::DirectMediaNavigationCandidateSnapshot candidatesSnapshot
        = directMediaNavigationCandidateSnapshot(
            {
                directMediaNavigationCandidate(displayedUrl),
                directMediaNavigationCandidate(normalizedVideoUrl),
                directMediaNavigationCandidate(nextUrl),
            },
            42);

    const kiriview::MediaPredecodeSchedulePlan plan
        = kiriview::mediaPredecodeSchedulePlan(kiriview::MediaPredecodeScheduleRequest {
            videoUrl,
            candidatesSnapshot,
            { displayedImage(displayedUrl) },
            kiriview::ImageFirstDisplayDecodeContext {},
        });

    QVERIFY(plan.shouldSchedule());
    QCOMPARE(plan.context.currentLocation.imageUrl(), normalizedVideoUrl);
    QCOMPARE(plan.context.displayedImages.size(), std::size_t(1));
    QCOMPARE(plan.context.displayedImages.front().location.imageUrl(), displayedUrl);
    QCOMPARE(plan.context.pageIndex, 1);

    const kiriview::DirectMediaNavigationCandidateSnapshot* scheduleSnapshot
        = scheduleCandidateSnapshot(plan);
    QVERIFY(scheduleSnapshot != nullptr);
    QCOMPARE(scheduleSnapshot->revision, candidatesSnapshot.revision);
    QVERIFY(scheduleSnapshot->candidates == candidatesSnapshot.candidates);
    QCOMPARE(scheduleSnapshot->candidates->size(), std::size_t(3));
    QCOMPARE(scheduleSnapshot->candidates->at(1).url, normalizedVideoUrl);
    const kiriview::MediaPredecodeEligibilitySnapshot* eligibility = scheduleEligibility(plan);
    QVERIFY(eligibility != nullptr);
    QCOMPARE(eligibility->directMediaNavigationCandidateCount, std::size_t(3));
    QVERIFY(eligibility->currentMediaIndex.has_value());
    QCOMPARE(*eligibility->currentMediaIndex, std::size_t(1));
    QCOMPARE(eligibility->images.size(), std::size_t(2));
    QCOMPARE(eligibility->images.at(0).url, displayedUrl);
    QCOMPARE(eligibility->images.at(0).mediaIndex, std::size_t(0));
    QCOMPARE(eligibility->images.at(1).url, nextUrl);
    QCOMPARE(eligibility->images.at(1).mediaIndex, std::size_t(2));
}

void TestMediaPredecodeSchedulePlan::missingCurrentCandidateKeepsUnknownPageIndex()
{
    const QUrl videoUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const kiriview::DirectMediaNavigationCandidateSnapshot candidatesSnapshot
        = directMediaNavigationCandidateSnapshot({
            directMediaNavigationCandidate(localUrl(QStringLiteral("/media/00.png"))),
            directMediaNavigationCandidate(localUrl(QStringLiteral("/media/02.png"))),
        });
    const kiriview::MediaPredecodeSchedulePlan plan
        = kiriview::mediaPredecodeSchedulePlan(kiriview::MediaPredecodeScheduleRequest {
            videoUrl,
            candidatesSnapshot,
            {},
            kiriview::ImageFirstDisplayDecodeContext {},
        });

    QVERIFY(plan.shouldSchedule());
    QCOMPARE(plan.context.currentLocation.imageUrl(), videoUrl);
    QCOMPARE(plan.context.pageIndex, -1);
    QVERIFY(scheduleCandidateSnapshot(plan) != nullptr);
    const kiriview::MediaPredecodeEligibilitySnapshot* eligibility = scheduleEligibility(plan);
    QVERIFY(eligibility != nullptr);
    QCOMPARE(eligibility->directMediaNavigationCandidateCount, std::size_t(2));
    QVERIFY(!eligibility->currentMediaIndex.has_value());
}

void TestMediaPredecodeSchedulePlan::invalidCursorYieldsEmptyScheduleContext()
{
    const kiriview::MediaPredecodeSchedulePlan plan
        = kiriview::mediaPredecodeSchedulePlan(kiriview::MediaPredecodeScheduleRequest {
            QUrl(),
            directMediaNavigationCandidateSnapshot({
                directMediaNavigationCandidate(localUrl(QStringLiteral("/media/00.png"))),
            }),
            {},
            kiriview::ImageFirstDisplayDecodeContext {},
        });

    QVERIFY(!plan.shouldSchedule());
    QVERIFY(plan.context.currentLocation.isEmpty());
    QVERIFY(scheduleCandidateSnapshot(plan) == nullptr);
    QVERIFY(scheduleEligibility(plan) == nullptr);
}

void TestMediaPredecodeSchedulePlan::mediaPayloadAccessorsRejectWrongPayloadType()
{
    kiriview::PredecodeScheduleContext context;
    context.currentLocation
        = kiriview::DisplayedImageLocation::fromUrl(localUrl(QStringLiteral("/media/01.mp4")));
    context.payload = std::make_shared<UnrelatedPredecodeSchedulePayload>();
    const kiriview::PredecodePendingSchedule schedule { std::move(context), 1 };

    QVERIFY(kiriview::mediaPredecodeScheduleCandidateSnapshot(schedule) == nullptr);
    QVERIFY(kiriview::mediaPredecodeScheduleEligibility(schedule) == nullptr);
}

QTEST_GUILESS_MAIN(TestMediaPredecodeSchedulePlan)

#include "tst_mediapredecodescheduleplan.moc"
