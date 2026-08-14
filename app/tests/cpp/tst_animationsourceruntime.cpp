// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imageanimationplaybacksource.h"
#include "rendering/animationsourceruntime.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QTest>

#include <future>
#include <initializer_list>
#include <memory>
#include <thread>
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
    void playbackSourceOperationsStayOnOneOwnerThread();
    void frameResultRetainsWorkspaceUntilImageRelease();
    void preparedFramesDeclareOpenAndPersistentDecoderEnvelopes();
};

namespace {
struct FakePlaybackState
{
    std::vector<QImage> frames;
    int factoryCount = 0;
    int openCount = 0;
    int readCount = 0;
    int errorFrameIndex = -1;
    int resourceErrorFrameIndex = -1;
    bool failOpen = false;
    bool requireSingleOwnerThread = false;
    std::thread::id ownerThread;
};

struct ImageCleanupObservation
{
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> budget;
    uchar* pixels = nullptr;
    bool cleanupCalled = false;
    bool reservationHeldDuringCleanup = false;

    ~ImageCleanupObservation() { delete[] pixels; }
};

void observeImageCleanup(void* context)
{
    auto* observation = static_cast<ImageCleanupObservation*>(context);
    observation->cleanupCalled = true;
    observation->reservationHeldDuringCleanup = observation->budget->reservedByteCount() > 0;
    delete[] observation->pixels;
    observation->pixels = nullptr;
}

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
    QVERIFY2(result.has_value(), result ? "" : qPrintable(result.error().errorString));
    QCOMPARE(result->image.pixelColor(0, 0), expected);
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
        if (m_state->requireSingleOwnerThread) {
            m_state->ownerThread = std::this_thread::get_id();
        }
        if (m_state->failOpen || m_state->frames.empty()) {
            return {
                kiriview::ImageAnimationPlaybackOpenStatus::Error,
                {},
                {},
                0,
                0,
                false,
                QStringLiteral("fake open failure"),
            };
        }

        return {
            kiriview::ImageAnimationPlaybackOpenStatus::Success,
            {},
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
        if (m_state->requireSingleOwnerThread
            && m_state->ownerThread != std::this_thread::get_id()) {
            return {
                kiriview::ImageAnimationPlaybackReadStatus::Error,
                {},
                false,
                QStringLiteral("fake source changed owner thread"),
            };
        }
        if (m_nextFrameIndex == m_state->errorFrameIndex) {
            return {
                kiriview::ImageAnimationPlaybackReadStatus::Error,
                {},
                false,
                QStringLiteral("fake read failure"),
            };
        }
        if (m_nextFrameIndex == m_state->resourceErrorFrameIndex) {
            return {
                kiriview::ImageAnimationPlaybackReadStatus::ResourceLimitExceeded,
                {},
                false,
                QStringLiteral("fake resource limit failure"),
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
    QVERIFY(!openError.error().errorString.isEmpty());
    QCOMPARE(openErrorState->openCount, 1);
    QCOMPARE(openErrorState->readCount, 0);

    const auto readErrorState = std::make_shared<FakePlaybackState>();
    readErrorState->frames = solidFrames({ Qt::red, Qt::green, Qt::blue });
    readErrorState->errorFrameIndex = 2;
    kiriview::AnimationSourceRuntime readErrorRuntime(readErrorState->frames.front(),
        static_cast<int>(readErrorState->frames.size()), fakeFactory(readErrorState));
    const kiriview::AnimationSourceFrameResult readError = readErrorRuntime.frame(2);
    QVERIFY(!readError.has_value());
    QVERIFY(!readError.error().errorString.isEmpty());
    QCOMPARE(readErrorState->openCount, 1);
    QCOMPARE(readErrorState->readCount, 2);

    const auto resourceErrorState = std::make_shared<FakePlaybackState>();
    resourceErrorState->frames = solidFrames({ Qt::red, Qt::green, Qt::blue });
    resourceErrorState->resourceErrorFrameIndex = 1;
    kiriview::AnimationSourceRuntime resourceErrorRuntime(resourceErrorState->frames.front(),
        static_cast<int>(resourceErrorState->frames.size()), fakeFactory(resourceErrorState));
    const kiriview::AnimationSourceFrameResult resourceError = resourceErrorRuntime.frame(1);
    QVERIFY(!resourceError.has_value());
    QCOMPARE(resourceError.error().cause,
        kiriview::AnimationSourceFrameFailureCause::ResourceLimitExceeded);
    QCOMPARE(resourceError.error().errorString, QStringLiteral("fake resource limit failure"));

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

void TestAnimationSourceRuntime::playbackSourceOperationsStayOnOneOwnerThread()
{
    const auto state = std::make_shared<FakePlaybackState>();
    state->frames = solidFrames({ Qt::red, Qt::green, Qt::blue });
    state->requireSingleOwnerThread = true;
    kiriview::AnimationSourceRuntime runtime(
        state->frames.front(), static_cast<int>(state->frames.size()), fakeFactory(state));

    kiriview::AnimationSourceFrameResult firstResult
        = std::unexpected(kiriview::AnimationSourceFrameFailure {
            kiriview::AnimationSourceFrameFailureCause::Unavailable,
            QStringLiteral("first frame was not requested"),
        });
    kiriview::AnimationSourceFrameResult secondResult
        = std::unexpected(kiriview::AnimationSourceFrameFailure {
            kiriview::AnimationSourceFrameFailureCause::Unavailable,
            QStringLiteral("second frame was not requested"),
        });
    std::promise<void> firstFinishedPromise;
    std::future<void> firstFinished = firstFinishedPromise.get_future();
    std::promise<void> releaseFirstPromise;
    std::shared_future<void> releaseFirst = releaseFirstPromise.get_future().share();
    std::jthread firstCaller([&]() {
        firstResult = runtime.frame(1);
        firstFinishedPromise.set_value();
        releaseFirst.wait();
    });
    firstFinished.wait();

    std::jthread secondCaller([&]() { secondResult = runtime.frame(2); });
    secondCaller.join();
    releaseFirstPromise.set_value();
    firstCaller.join();

    compareFrameColor(firstResult, Qt::green);
    compareFrameColor(secondResult, Qt::blue);
    QCOMPARE(state->openCount, 1);
    QCOMPARE(state->readCount, 2);
}

void TestAnimationSourceRuntime::frameResultRetainsWorkspaceUntilImageRelease()
{
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(16, 16);
    kiriview::ImageDecodeWorkspaceLease lease
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*budget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(lease, 4));
    kiriview::ImageDecodeWorkspaceHold hold = lease.retainOnly(4);
    QVERIFY(hold.isManaged());
    ImageCleanupObservation observation { budget, new uchar[4] { 0, 255, 0, 255 } };
    QImage retainedFrame(
        observation.pixels, 1, 1, 4, QImage::Format_RGBA8888, observeImageCleanup, &observation);
    kiriview::AnimationSourceFrameResult result
        = std::unexpected(kiriview::AnimationSourceFrameFailure {});

    {
        kiriview::AnimationSourceRuntime runtime(std::move(retainedFrame), 1, {}, std::move(hold));
        result = runtime.frame(0);
        QVERIFY(result.has_value());
        QVERIFY(!result->image.isNull());
    }

    QVERIFY(!observation.cleanupCalled);
    QCOMPARE(budget->reservedByteCount(), qsizetype(4));
    result = std::unexpected(kiriview::AnimationSourceFrameFailure {});
    QVERIFY(observation.cleanupCalled);
    QVERIFY(observation.reservationHeldDuringCleanup);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestAnimationSourceRuntime::preparedFramesDeclareOpenAndPersistentDecoderEnvelopes()
{
    const auto state = std::make_shared<FakePlaybackState>();
    state->frames = solidFrames({ Qt::red, Qt::green, Qt::blue });
    constexpr qsizetype retainedInputByteCount = 2;
    constexpr qsizetype persistentDecoderByteCount = 10;
    constexpr qsizetype frameOutputByteCount = 4;
    kiriview::AnimationSourceRuntime runtime(state->frames.front(),
        static_cast<int>(state->frames.size()), fakeFactory(state), {},
        kiriview::ImageAnimationPlaybackWorkspacePlan {
            retainedInputByteCount,
            persistentDecoderByteCount,
            frameOutputByteCount,
        });

    const kiriview::AnimationSourceFramePreparationResult opening = runtime.prepareFrame(1);
    QVERIFY(opening.has_value());
    QVERIFY(opening->workspaceAdmission.has_value());
    QCOMPARE(
        opening->workspaceAdmission->priority, kiriview::ImageDecodeWorkspacePriority::Interactive);
    QCOMPARE(opening->workspaceAdmission->additionalPeakByteCount,
        persistentDecoderByteCount + frameOutputByteCount);
    QCOMPARE(opening->workspaceAdmission->perOperationBaselineByteCount, retainedInputByteCount);
    QCOMPARE(opening->outputByteCount, frameOutputByteCount);
    QVERIFY(!opening->retirePlaybackSourceBeforeProduction);

    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(64, 64);
    kiriview::ImageDecodeWorkspaceLease openingLease
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*budget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(
        openingLease, persistentDecoderByteCount + frameOutputByteCount));
    compareFrameColor(runtime.frame(1, std::move(openingLease), retainedInputByteCount), Qt::green);

    const kiriview::AnimationSourceFramePreparationResult sequential = runtime.prepareFrame(2);
    QVERIFY(sequential.has_value());
    QVERIFY(sequential->workspaceAdmission.has_value());
    QCOMPARE(sequential->workspaceAdmission->additionalPeakByteCount, frameOutputByteCount);
    QCOMPARE(sequential->workspaceAdmission->perOperationBaselineByteCount,
        retainedInputByteCount + persistentDecoderByteCount);
    QCOMPARE(sequential->outputByteCount, frameOutputByteCount);
    QVERIFY(!sequential->retirePlaybackSourceBeforeProduction);

    const kiriview::AnimationSourceFramePreparationResult rewind = runtime.prepareFrame(1);
    QVERIFY(rewind.has_value());
    QVERIFY(rewind->workspaceAdmission.has_value());
    QCOMPARE(rewind->workspaceAdmission->additionalPeakByteCount,
        persistentDecoderByteCount + frameOutputByteCount);
    QCOMPARE(rewind->workspaceAdmission->perOperationBaselineByteCount, retainedInputByteCount);
    QVERIFY(rewind->retirePlaybackSourceBeforeProduction);
}

QTEST_GUILESS_MAIN(TestAnimationSourceRuntime)

#include "tst_animationsourceruntime.moc"
