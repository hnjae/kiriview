// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videoplaybackcontrolruntime.h"

#include "image_async_test_support.h"

#include <QObject>
#include <QTest>
#include <optional>
#include <vector>

class TestVideoPlaybackControlRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void environmentFactsSelectPresentationMode();
    void invalidAndNonSeekableTimelinesHaveTypedProjections();
    void activeScrubRejectsBackendPositionProjection();
    void autoHideUsesInjectedTimerEvents();
    void interactionAndFixedModePreventAutoHide();
    void sourceReplacementAtomicallyResetsProjection();
};

namespace {
kiriview::VideoPlaybackControlEnvironment floatingEnvironment()
{
    return kiriview::VideoPlaybackControlEnvironment {
        1280.0,
        720.0,
        18.0,
        false,
        false,
        200,
        1500,
    };
}

kiriview::VideoPlaybackControlMediaSnapshot playableMedia()
{
    return kiriview::VideoPlaybackControlMediaSnapshot {
        true,
        90000,
        12000,
        true,
        true,
        false,
        false,
    };
}
}

void TestVideoPlaybackControlRuntime::environmentFactsSelectPresentationMode()
{
    QObject owner;
    kiriview::VideoPlaybackControlRuntime runtime(&owner);

    runtime.replaceSource(1);
    runtime.acceptEnvironment(floatingEnvironment());
    runtime.acceptMediaSnapshot(playableMedia());
    QCOMPARE(runtime.projection().presentationMode,
        kiriview::VideoPlaybackControlPresentationMode::Floating);
    QVERIFY(!runtime.projection().reserveSpace);

    auto compact = floatingEnvironment();
    compact.viewportWidth = compact.gridUnit * 31.0;
    runtime.acceptEnvironment(compact);
    QCOMPARE(runtime.projection().presentationMode,
        kiriview::VideoPlaybackControlPresentationMode::Fixed);
    QVERIFY(runtime.projection().reserveSpace);

    auto reducedMotion = floatingEnvironment();
    reducedMotion.longAnimationDurationMsec = 0;
    runtime.acceptEnvironment(reducedMotion);
    QCOMPARE(runtime.projection().presentationMode,
        kiriview::VideoPlaybackControlPresentationMode::Fixed);
}

void TestVideoPlaybackControlRuntime::invalidAndNonSeekableTimelinesHaveTypedProjections()
{
    QObject owner;
    kiriview::VideoPlaybackControlRuntime runtime(&owner);
    runtime.replaceSource(1);
    runtime.acceptEnvironment(floatingEnvironment());

    auto media = playableMedia();
    media.durationMsec = -1;
    runtime.acceptMediaSnapshot(media);
    QCOMPARE(runtime.projection().timelineKind, kiriview::VideoPlaybackTimelineKind::Unavailable);
    QVERIFY(!runtime.projection().timelineInteractive);
    QCOMPARE(runtime.projection().sliderValueMsec, qint64(0));
    QCOMPARE(runtime.projection().sliderMaximumMsec, qint64(1));

    media.durationMsec = 90000;
    media.seekable = false;
    runtime.acceptMediaSnapshot(media);
    QCOMPARE(runtime.projection().timelineKind, kiriview::VideoPlaybackTimelineKind::NonSeekable);
    QVERIFY(!runtime.projection().timelineInteractive);

    media.seekable = true;
    runtime.acceptMediaSnapshot(media);
    QCOMPARE(runtime.projection().timelineKind, kiriview::VideoPlaybackTimelineKind::Seekable);
    QVERIFY(runtime.projection().timelineInteractive);
    QCOMPARE(runtime.projection().sliderValueMsec, qint64(12000));
    QCOMPARE(runtime.projection().sliderMaximumMsec, qint64(90000));
}

void TestVideoPlaybackControlRuntime::activeScrubRejectsBackendPositionProjection()
{
    QObject owner;
    kiriview::VideoPlaybackControlRuntime runtime(&owner);
    runtime.replaceSource(1);
    runtime.acceptEnvironment(floatingEnvironment());
    runtime.acceptMediaSnapshot(playableMedia());

    runtime.beginScrub();
    runtime.updateScrub(45000);
    QVERIFY(runtime.projection().scrubbing);
    QCOMPARE(runtime.projection().sliderValueMsec, qint64(45000));

    auto backendUpdate = playableMedia();
    backendUpdate.positionMsec = 13000;
    runtime.acceptMediaSnapshot(backendUpdate);
    QCOMPARE(runtime.projection().sliderValueMsec, qint64(45000));

    const std::optional<kiriview::VideoPlaybackSeekIntent> seekIntent = runtime.commitScrub();
    QVERIFY(seekIntent.has_value());
    QCOMPARE(seekIntent->positionMsec, qint64(45000));
    QVERIFY(runtime.acceptsSeekIntent(*seekIntent));
    QVERIFY(!runtime.projection().scrubbing);

    runtime.beginScrub();
    runtime.updateScrub(120000);
    const std::optional<kiriview::VideoPlaybackSeekIntent> clampedIntent = runtime.commitScrub();
    QVERIFY(clampedIntent.has_value());
    QCOMPARE(clampedIntent->positionMsec, qint64(90000));
    QVERIFY(runtime.acceptsSeekIntent(*clampedIntent));
}

void TestVideoPlaybackControlRuntime::autoHideUsesInjectedTimerEvents()
{
    QObject owner;
    kiriview::TestSupport::ManualTimerScheduler timers;
    kiriview::VideoPlaybackControlRuntime runtime(&owner, timers.scheduler());
    runtime.replaceSource(1);
    runtime.acceptEnvironment(floatingEnvironment());
    runtime.acceptMediaSnapshot(playableMedia());

    QVERIFY(runtime.projection().autoHideEligible);
    QVERIFY(runtime.projection().shown);
    QCOMPARE(timers.timerCount(), std::size_t(1));
    QCOMPARE(timers.timerAt(0).intervalMsec(), 1500);
    QVERIFY(timers.timerAt(0).active());

    timers.timerAt(0).fire();
    QVERIFY(!runtime.projection().shown);

    runtime.reveal();
    QVERIFY(runtime.projection().shown);
    QVERIFY(timers.timerAt(0).active());
}

void TestVideoPlaybackControlRuntime::interactionAndFixedModePreventAutoHide()
{
    QObject owner;
    kiriview::TestSupport::ManualTimerScheduler timers;
    kiriview::VideoPlaybackControlRuntime runtime(&owner, timers.scheduler());
    runtime.replaceSource(1);
    runtime.acceptEnvironment(floatingEnvironment());
    runtime.acceptMediaSnapshot(playableMedia());

    runtime.setInteractionActive(true);
    QVERIFY(runtime.projection().shown);
    QVERIFY(!timers.timerAt(0).active());
    timers.timerAt(0).fire();
    QVERIFY(runtime.projection().shown);

    auto fixed = floatingEnvironment();
    fixed.transientTouchInput = true;
    runtime.acceptEnvironment(fixed);
    QVERIFY(!runtime.projection().autoHideEligible);
    QVERIFY(runtime.projection().shown);
    QVERIFY(runtime.projection().reserveSpace);
}

void TestVideoPlaybackControlRuntime::sourceReplacementAtomicallyResetsProjection()
{
    QObject owner;
    kiriview::TestSupport::ManualTimerScheduler timers;
    std::vector<kiriview::VideoPlaybackControlProjection> projections;
    kiriview::VideoPlaybackControlRuntime runtime(&owner, timers.scheduler(),
        [&projections](const kiriview::VideoPlaybackControlProjection& projection) {
            projections.push_back(projection);
        });
    runtime.replaceSource(1);
    runtime.acceptEnvironment(floatingEnvironment());
    auto media = playableMedia();
    media.muted = true;
    runtime.acceptMediaSnapshot(media);
    runtime.beginScrub();
    runtime.updateScrub(50000);
    projections.clear();

    runtime.replaceSource(2);

    QCOMPARE(projections.size(), std::size_t(1));
    QCOMPARE(projections.front().sourceRevision, quint64(2));
    QVERIFY(!projections.front().ready);
    QVERIFY(!projections.front().scrubbing);
    QVERIFY(!projections.front().shown);
    QVERIFY(projections.front().muted);
    QCOMPARE(projections.front().timelineKind, kiriview::VideoPlaybackTimelineKind::Unavailable);
    QVERIFY(!timers.timerAt(0).active());
}

QTEST_GUILESS_MAIN(TestVideoPlaybackControlRuntime)

#include "tst_videoplaybackcontrolruntime.moc"
