// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "bridge/videodocumentpolicyconversion.h"
#include "kiriview/src/policy/videodocumentpolicy.cxx.h"
#include "video/videoplaybackcontrolplan.h"

#include <QObject>
#include <QTest>
#include <cstddef>
#include <optional>
#include <variant>

class TestVideoPlaybackControlPlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyPlanReportsEmpty();
    void wrapperFunctionsRouteThroughRustPolicy();
    void clampedSeekWrapperRoutesThroughRustPolicy();
};

namespace {
kiriview::VideoPlaybackControlSnapshot playableSnapshot()
{
    return kiriview::VideoPlaybackControlSnapshot {
        false,
        true,
        false,
        true,
        true,
        5000,
        10000,
    };
}

kiriview::RustVideoPlaybackControlSnapshot rustSnapshot(
    const kiriview::VideoPlaybackControlSnapshot& snapshot)
{
    return kiriview::Bridge::rustVideoPlaybackControlSnapshot(snapshot);
}

kiriview::VideoPlaybackControlPlan convertedRustPlan(kiriview::RustVideoPlaybackControlPlan plan)
{
    return kiriview::Bridge::videoPlaybackControlPlanFromRust(plan);
}

void compareOptionalBool(std::optional<bool> actual, std::optional<bool> expected)
{
    QCOMPARE(actual.has_value(), expected.has_value());
    if (actual.has_value()) {
        QCOMPARE(actual.value(), expected.value());
    }
}

void compareOptionalPosition(std::optional<qint64> actual, std::optional<qint64> expected)
{
    QCOMPARE(actual.has_value(), expected.has_value());
    if (actual.has_value()) {
        QCOMPARE(actual.value(), expected.value());
    }
}

void comparePlans(const kiriview::VideoPlaybackControlPlan& actual,
    const kiriview::VideoPlaybackControlPlan& expected)
{
    compareOptionalBool(actual.stateDelta.mediaEnded, expected.stateDelta.mediaEnded);
    compareOptionalBool(actual.stateDelta.playing, expected.stateDelta.playing);
    compareOptionalPosition(actual.stateDelta.position, expected.stateDelta.position);
    QCOMPARE(actual.backendOperations.size(), expected.backendOperations.size());
    for (std::size_t index = 0; index < actual.backendOperations.size(); ++index) {
        const kiriview::VideoPlaybackBackendOperation& actualOperation
            = actual.backendOperations.at(index);
        const kiriview::VideoPlaybackBackendOperation& expectedOperation
            = expected.backendOperations.at(index);
        QCOMPARE(actualOperation.index(), expectedOperation.index());
        const auto* actualSetPosition
            = std::get_if<kiriview::SetVideoPlaybackPositionOperation>(&actualOperation);
        if (actualSetPosition != nullptr) {
            const auto* expectedSetPosition
                = std::get_if<kiriview::SetVideoPlaybackPositionOperation>(&expectedOperation);
            QVERIFY(expectedSetPosition != nullptr);
            QCOMPARE(actualSetPosition->position, expectedSetPosition->position);
        }
    }
    QCOMPARE(actual.isEmpty(), expected.isEmpty());
}
}

void TestVideoPlaybackControlPlan::emptyPlanReportsEmpty()
{
    kiriview::VideoPlaybackControlPlan plan;
    QVERIFY(plan.isEmpty());

    plan.stateDelta.playing = false;
    QVERIFY(!plan.isEmpty());
}

void TestVideoPlaybackControlPlan::wrapperFunctionsRouteThroughRustPolicy()
{
    kiriview::VideoPlaybackControlSnapshot snapshot = playableSnapshot();
    snapshot.mediaEnded = true;
    snapshot.position = 10000;
    comparePlans(kiriview::videoPlaybackPlayPlan(snapshot),
        convertedRustPlan(kiriview::rustVideoPlaybackPlayPlan(rustSnapshot(snapshot))));

    snapshot = playableSnapshot();
    comparePlans(kiriview::videoPlaybackPausePlan(snapshot),
        convertedRustPlan(kiriview::rustVideoPlaybackPausePlan(rustSnapshot(snapshot))));

    comparePlans(kiriview::videoPlaybackStopPlan(snapshot),
        convertedRustPlan(kiriview::rustVideoPlaybackStopPlan(rustSnapshot(snapshot))));

    snapshot.playing = true;
    comparePlans(kiriview::videoPlaybackTogglePlan(snapshot),
        convertedRustPlan(kiriview::rustVideoPlaybackTogglePlan(rustSnapshot(snapshot))));

    snapshot = playableSnapshot();
    comparePlans(kiriview::videoPlaybackSetPositionPlan(snapshot, 12000),
        convertedRustPlan(
            kiriview::rustVideoPlaybackSetPositionPlan(rustSnapshot(snapshot), 12000)));

    comparePlans(kiriview::videoPlaybackSeekByPlan(snapshot, 7000),
        convertedRustPlan(kiriview::rustVideoPlaybackSeekByPlan(rustSnapshot(snapshot), 7000)));
}

void TestVideoPlaybackControlPlan::clampedSeekWrapperRoutesThroughRustPolicy()
{
    QCOMPARE(kiriview::videoPlaybackClampedSeekPosition(5000, 7000, 10000, true),
        kiriview::rustVideoPlaybackClampedSeekPosition(5000, 7000, 10000, true));
    QCOMPARE(kiriview::videoPlaybackClampedSeekPosition(5000, 1000, 10000, false),
        kiriview::rustVideoPlaybackClampedSeekPosition(5000, 1000, 10000, false));
}

QTEST_GUILESS_MAIN(TestVideoPlaybackControlPlan)

#include "tst_videoplaybackcontrolplan.moc"
