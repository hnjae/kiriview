// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imageanimationplaybacksource.h"
#include "rendering/animationsourceruntime.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QTest>

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

class TestAnimationSourceRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void retainedFirstFrameDoesNotOpenThePlaybackSource();
    void sequentialLoopsReadEachAuthoredFrameOnce();
    void backwardSeekReopensAndReplaysOnlyTheRequestedPrefix();
    void invalidSourceErrorAndCloseRejectFrameRequests();
    void runtimesOwnIndependentPlaybackProgress();
};

namespace {
struct FakePlaybackState
{
    std::vector<QImage> frames;
    int factoryCount = 0;
    int openCount = 0;
    int readCount = 0;
    int errorFrameIndex = -1;
    bool failOpen = false;
};

QImage solidImage(const QColor& color)
{
    QImage image(2, 2, QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

std::vector<QImage> solidFrames(std::initializer_list<QColor> colors)
{
    std::vector<QImage> frames;
    frames.reserve(colors.size());
    for (const QColor& color : colors) {
        frames.push_back(solidImage(color));
    }
    return frames;
}

void compareFrameColor(const kiriview::AnimationSourceFrameResult& result, const QColor& expected)
{
    QVERIFY2(result.has_value(), result ? "" : qPrintable(result.error()));
    QCOMPARE(result->pixelColor(0, 0), expected);
}

class FakePlaybackSource final : public kiriview::ImageAnimationPlaybackSource
{
public:
    explicit FakePlaybackSource(std::shared_ptr<FakePlaybackState> state)
        : m_state(std::move(state))
    {
    }

    kiriview::ImageAnimationPlaybackOpenResult open() override
    {
        ++m_state->openCount;
        m_nextFrameIndex = 1;
        if (m_state->failOpen || m_state->frames.empty()) {
            return {
                kiriview::ImageAnimationPlaybackOpenStatus::Error,
                {},
                0,
                0,
                false,
                QStringLiteral("fake open failure"),
            };
        }

        return {
            kiriview::ImageAnimationPlaybackOpenStatus::Success,
            m_state->frames.front(),
            10,
            0,
            m_state->frames.size() > 1,
            {},
        };
    }

    kiriview::ImageAnimationPlaybackReadResult readNextFrame() override
    {
        ++m_state->readCount;
        if (m_nextFrameIndex == m_state->errorFrameIndex) {
            return {
                kiriview::ImageAnimationPlaybackReadStatus::Error,
                {},
                false,
                QStringLiteral("fake read failure"),
            };
        }
        if (m_nextFrameIndex < 0 || m_nextFrameIndex >= static_cast<int>(m_state->frames.size())) {
            return {
                kiriview::ImageAnimationPlaybackReadStatus::End,
                {},
                false,
                {},
            };
        }

        const int frameIndex = m_nextFrameIndex++;
        return {
            kiriview::ImageAnimationPlaybackReadStatus::Frame,
            kiriview::AnimationFrame {
                m_state->frames.at(static_cast<std::size_t>(frameIndex)),
                10,
            },
            m_nextFrameIndex < static_cast<int>(m_state->frames.size()),
            {},
        };
    }

    [[nodiscard]] bool restartable() const override { return true; }

private:
    std::shared_ptr<FakePlaybackState> m_state;
    int m_nextFrameIndex = 1;
};

kiriview::ImageAnimationPlaybackSourceFactory fakeFactory(
    const std::shared_ptr<FakePlaybackState>& state)
{
    return [state] {
        ++state->factoryCount;
        return std::make_unique<FakePlaybackSource>(state);
    };
}
}

void TestAnimationSourceRuntime::retainedFirstFrameDoesNotOpenThePlaybackSource()
{
    const auto state = std::make_shared<FakePlaybackState>();
    state->frames = solidFrames({ Qt::red, Qt::blue });
    kiriview::AnimationSourceRuntime runtime(
        solidImage(Qt::green), static_cast<int>(state->frames.size()), fakeFactory(state));

    compareFrameColor(runtime.frame(0), Qt::green);
    QCOMPARE(state->openCount, 0);
    QCOMPARE(state->readCount, 0);
}

void TestAnimationSourceRuntime::sequentialLoopsReadEachAuthoredFrameOnce()
{
    const auto state = std::make_shared<FakePlaybackState>();
    state->frames = solidFrames({ Qt::red, Qt::green, Qt::blue, Qt::yellow });
    kiriview::AnimationSourceRuntime runtime(
        state->frames.front(), static_cast<int>(state->frames.size()), fakeFactory(state));

    for (int loop = 1; loop <= 2; ++loop) {
        for (int frameIndex = 0; frameIndex < static_cast<int>(state->frames.size());
            ++frameIndex) {
            compareFrameColor(runtime.frame(frameIndex),
                state->frames.at(static_cast<std::size_t>(frameIndex)).pixelColor(0, 0));
        }
        QCOMPARE(state->openCount, loop);
        QCOMPARE(state->readCount, loop * (static_cast<int>(state->frames.size()) - 1));
    }
}

void TestAnimationSourceRuntime::backwardSeekReopensAndReplaysOnlyTheRequestedPrefix()
{
    const auto state = std::make_shared<FakePlaybackState>();
    state->frames = solidFrames({ Qt::red, Qt::green, Qt::blue, Qt::yellow });
    kiriview::AnimationSourceRuntime runtime(
        state->frames.front(), static_cast<int>(state->frames.size()), fakeFactory(state));

    compareFrameColor(runtime.frame(3), Qt::yellow);
    QCOMPARE(state->openCount, 1);
    QCOMPARE(state->readCount, 3);

    compareFrameColor(runtime.frame(1), Qt::green);
    QCOMPARE(state->openCount, 2);
    QCOMPARE(state->readCount, 4);

    compareFrameColor(runtime.frame(2), Qt::blue);
    QCOMPARE(state->openCount, 2);
    QCOMPARE(state->readCount, 5);
}

void TestAnimationSourceRuntime::invalidSourceErrorAndCloseRejectFrameRequests()
{
    const auto validState = std::make_shared<FakePlaybackState>();
    validState->frames = solidFrames({ Qt::red, Qt::green, Qt::blue });
    kiriview::AnimationSourceRuntime validRuntime(validState->frames.front(),
        static_cast<int>(validState->frames.size()), fakeFactory(validState));

    QVERIFY(!validRuntime.frame(-1).has_value());
    QVERIFY(!validRuntime.frame(static_cast<int>(validState->frames.size())).has_value());
    QCOMPARE(validState->openCount, 0);
    QCOMPARE(validState->readCount, 0);

    const auto openErrorState = std::make_shared<FakePlaybackState>();
    openErrorState->frames = solidFrames({ Qt::red, Qt::green });
    openErrorState->failOpen = true;
    kiriview::AnimationSourceRuntime openErrorRuntime(openErrorState->frames.front(),
        static_cast<int>(openErrorState->frames.size()), fakeFactory(openErrorState));
    const kiriview::AnimationSourceFrameResult openError = openErrorRuntime.frame(1);
    QVERIFY(!openError.has_value());
    QVERIFY(!openError.error().isEmpty());
    QCOMPARE(openErrorState->openCount, 1);
    QCOMPARE(openErrorState->readCount, 0);

    const auto readErrorState = std::make_shared<FakePlaybackState>();
    readErrorState->frames = solidFrames({ Qt::red, Qt::green, Qt::blue });
    readErrorState->errorFrameIndex = 2;
    kiriview::AnimationSourceRuntime readErrorRuntime(readErrorState->frames.front(),
        static_cast<int>(readErrorState->frames.size()), fakeFactory(readErrorState));
    const kiriview::AnimationSourceFrameResult readError = readErrorRuntime.frame(2);
    QVERIFY(!readError.has_value());
    QVERIFY(!readError.error().isEmpty());
    QCOMPARE(readErrorState->openCount, 1);
    QCOMPARE(readErrorState->readCount, 2);

    compareFrameColor(validRuntime.frame(1), Qt::green);
    const int opensBeforeClose = validState->openCount;
    const int readsBeforeClose = validState->readCount;
    validRuntime.close();
    QVERIFY(!validRuntime.frame(0).has_value());
    QVERIFY(!validRuntime.frame(2).has_value());
    QCOMPARE(validState->openCount, opensBeforeClose);
    QCOMPARE(validState->readCount, readsBeforeClose);
}

void TestAnimationSourceRuntime::runtimesOwnIndependentPlaybackProgress()
{
    const auto firstState = std::make_shared<FakePlaybackState>();
    firstState->frames = solidFrames({ Qt::red, Qt::green, Qt::blue });
    const auto secondState = std::make_shared<FakePlaybackState>();
    secondState->frames = solidFrames({ Qt::cyan, Qt::magenta, Qt::yellow });

    kiriview::AnimationSourceRuntime firstRuntime(firstState->frames.front(),
        static_cast<int>(firstState->frames.size()), fakeFactory(firstState));
    kiriview::AnimationSourceRuntime secondRuntime(secondState->frames.front(),
        static_cast<int>(secondState->frames.size()), fakeFactory(secondState));

    compareFrameColor(firstRuntime.frame(1), Qt::green);
    compareFrameColor(secondRuntime.frame(1), Qt::magenta);
    compareFrameColor(firstRuntime.frame(2), Qt::blue);
    compareFrameColor(secondRuntime.frame(2), Qt::yellow);

    QCOMPARE(firstState->openCount, 1);
    QCOMPARE(firstState->readCount, 2);
    QCOMPARE(secondState->openCount, 1);
    QCOMPARE(secondState->readCount, 2);
}

QTEST_GUILESS_MAIN(TestAnimationSourceRuntime)

#include "tst_animationsourceruntime.moc"
