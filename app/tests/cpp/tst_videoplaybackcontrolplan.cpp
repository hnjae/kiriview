// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videoplaybackcontrolplan.h"

#include <QObject>
#include <QTest>
#include <variant>

class TestVideoPlaybackControlPlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyPlanReportsEmpty();
    void playRestartsEndedSeekableMedia();
    void pauseAndStopRespectBackendAvailability();
    void seekingClampsToDuration();
};

namespace {
kiriview::VideoPlaybackControlSnapshot playableSnapshot()
{
    return { false, true, false, false, true, 5000, 10000 };
}

template <typename Operation>
const Operation* operationAt(const kiriview::VideoPlaybackControlPlan& plan, std::size_t index)
{
    return std::get_if<Operation>(&plan.backendOperations.at(index));
}
}

void TestVideoPlaybackControlPlan::emptyPlanReportsEmpty()
{
    kiriview::VideoPlaybackControlPlan plan;
    QVERIFY(plan.isEmpty());
    plan.stateDelta.playing = false;
    QVERIFY(!plan.isEmpty());
}

void TestVideoPlaybackControlPlan::playRestartsEndedSeekableMedia()
{
    kiriview::VideoPlaybackControlSnapshot snapshot = playableSnapshot();
    snapshot.mediaEnded = true;
    snapshot.position = snapshot.duration;
    const kiriview::VideoPlaybackControlPlan plan = kiriview::videoPlaybackPlayPlan(snapshot);

    QCOMPARE(plan.stateDelta.mediaEnded, std::optional<bool>(false));
    QCOMPARE(plan.stateDelta.position, std::optional<qint64>(0));
    QCOMPARE(plan.backendOperations.size(), std::size_t(3));
    QVERIFY(operationAt<kiriview::EnsureVideoPlaybackBackendOperation>(plan, 0) != nullptr);
    const auto* position = operationAt<kiriview::SetVideoPlaybackPositionOperation>(plan, 1);
    QVERIFY(position != nullptr);
    QCOMPARE(position->position, qint64(0));
    QVERIFY(operationAt<kiriview::PlayVideoPlaybackOperation>(plan, 2) != nullptr);
}

void TestVideoPlaybackControlPlan::pauseAndStopRespectBackendAvailability()
{
    kiriview::VideoPlaybackControlSnapshot snapshot = playableSnapshot();
    snapshot.mediaBackendAvailable = false;
    QVERIFY(kiriview::videoPlaybackPausePlan(snapshot).isEmpty());

    const kiriview::VideoPlaybackControlPlan stop = kiriview::videoPlaybackStopPlan(snapshot);
    QCOMPARE(stop.stateDelta.mediaEnded, std::optional<bool>(false));
    QCOMPARE(stop.stateDelta.playing, std::optional<bool>(false));
    QCOMPARE(stop.stateDelta.position, std::optional<qint64>(0));
    QVERIFY(stop.backendOperations.empty());
}

void TestVideoPlaybackControlPlan::seekingClampsToDuration()
{
    const kiriview::VideoPlaybackControlSnapshot snapshot = playableSnapshot();
    const kiriview::VideoPlaybackControlPlan absolute
        = kiriview::videoPlaybackSetPositionPlan(snapshot, 12000);
    QCOMPARE(absolute.stateDelta.position, std::optional<qint64>(10000));
    const auto* absolutePosition
        = operationAt<kiriview::SetVideoPlaybackPositionOperation>(absolute, 1);
    QVERIFY(absolutePosition != nullptr);
    QCOMPARE(absolutePosition->position, qint64(10000));

    const kiriview::VideoPlaybackControlPlan relative
        = kiriview::videoPlaybackSeekByPlan(snapshot, -7000);
    QCOMPARE(relative.stateDelta.position, std::optional<qint64>(0));
}

QTEST_GUILESS_MAIN(TestVideoPlaybackControlPlan)

#include "tst_videoplaybackcontrolplan.moc"
