#include "playbacktimeline_p.h"

#include <QtTest/QTest>

namespace {

int frameStartFor(int frame)
{
    switch (frame) {
    case 0:
        return 0;
    case 1:
        return 100;
    default:
        return -1;
    }
}

int frameIndexFor(int position)
{
    if (position >= 0 && position < 100) {
        return 0;
    }
    if (position >= 100 && position < 350) {
        return 1;
    }
    return -1;
}

}

class PlaybackTimelineTest : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackTimelineTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void advancementWithinCurrentFrameKeepsDisplayTarget();
    void advancementAtFrameBoundarySelectsNextFrame();
    void advancementWithoutCurrentPositionSeedsFromCurrentFrame();
    void playOnceEndSelectsFinalFrame();
    void loopingEndWrapsToSequenceStart();
    void invalidFrameLookupRejectsTarget();
};

void PlaybackTimelineTest::advancementWithinCurrentFrameKeepsDisplayTarget()
{
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        99, 0, 0, false, 350, 2, frameStartFor, frameIndexFor);

    QVERIFY(target.valid);
    QCOMPARE(target.displayTarget.frame, 0);
    QCOMPARE(target.displayTarget.position, 0);
    QCOMPARE(target.playbackPosition, 99);
    QCOMPARE(target.reachedEnd, false);
    QCOMPARE(target.looped, false);
}

void PlaybackTimelineTest::advancementAtFrameBoundarySelectsNextFrame()
{
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        100, 0, 0, false, 350, 2, frameStartFor, frameIndexFor);

    QVERIFY(target.valid);
    QCOMPARE(target.displayTarget.frame, 1);
    QCOMPARE(target.displayTarget.position, 100);
    QCOMPARE(target.playbackPosition, 100);
    QCOMPARE(target.reachedEnd, false);
    QCOMPARE(target.looped, false);
}

void PlaybackTimelineTest::advancementWithoutCurrentPositionSeedsFromCurrentFrame()
{
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        50, 1, -1, false, 350, 2, frameStartFor, frameIndexFor);

    QVERIFY(target.valid);
    QCOMPARE(target.displayTarget.frame, 1);
    QCOMPARE(target.displayTarget.position, 100);
    QCOMPARE(target.playbackPosition, 150);
    QCOMPARE(target.reachedEnd, false);
    QCOMPARE(target.looped, false);
}

void PlaybackTimelineTest::playOnceEndSelectsFinalFrame()
{
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        250, 1, 100, false, 350, 2, frameStartFor, frameIndexFor);

    QVERIFY(target.valid);
    QCOMPARE(target.displayTarget.frame, 1);
    QCOMPARE(target.displayTarget.position, 100);
    QCOMPARE(target.playbackPosition, 350);
    QCOMPARE(target.reachedEnd, true);
    QCOMPARE(target.looped, false);
}

void PlaybackTimelineTest::loopingEndWrapsToSequenceStart()
{
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        250, 1, 100, true, 350, 2, frameStartFor, frameIndexFor);

    QVERIFY(target.valid);
    QCOMPARE(target.displayTarget.frame, 0);
    QCOMPARE(target.displayTarget.position, 0);
    QCOMPARE(target.playbackPosition, 0);
    QCOMPARE(target.reachedEnd, false);
    QCOMPARE(target.looped, true);
}

void PlaybackTimelineTest::invalidFrameLookupRejectsTarget()
{
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        50, 0, 0, false, 350, 2, frameStartFor, [](int) { return -1; });

    QCOMPARE(target.valid, false);
}

QTEST_MAIN(PlaybackTimelineTest)

#include "tst_playback_timeline.moc"
