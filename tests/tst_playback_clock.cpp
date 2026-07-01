#include "playbackclock_p.h"

#include <QtTest/QTest>

#include <limits>

class PlaybackClockTest : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackClockTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void invalidClockTakesZeroElapsed();
    void restartedClockTakesMonotonicElapsedAndInvalidates();
    void negativeElapsedClampsToZero();
    void largeElapsedClampsToIntegerMaximum();
};

void PlaybackClockTest::invalidClockTakesZeroElapsed()
{
    ImageViewportInternal::PlaybackClock clock;

    QCOMPARE(clock.isValid(), false);
    QCOMPARE(clock.takeElapsed(100), 0);
    QCOMPARE(clock.isValid(), false);
}

void PlaybackClockTest::restartedClockTakesMonotonicElapsedAndInvalidates()
{
    ImageViewportInternal::PlaybackClock clock;

    clock.restart(25);
    QCOMPARE(clock.isValid(), true);
    QCOMPARE(clock.takeElapsed(125), 100);
    QCOMPARE(clock.isValid(), false);
    QCOMPARE(clock.takeElapsed(200), 0);
}

void PlaybackClockTest::negativeElapsedClampsToZero()
{
    ImageViewportInternal::PlaybackClock clock;

    clock.restart(125);
    QCOMPARE(clock.takeElapsed(25), 0);
}

void PlaybackClockTest::largeElapsedClampsToIntegerMaximum()
{
    ImageViewportInternal::PlaybackClock clock;

    clock.restart(0);
    QCOMPARE(clock.takeElapsed(qint64 { std::numeric_limits<int>::max() } + 1),
        std::numeric_limits<int>::max());
}

QTEST_MAIN(PlaybackClockTest)

#include "tst_playback_clock.moc"
