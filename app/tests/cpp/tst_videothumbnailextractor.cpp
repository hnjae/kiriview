// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_async_test_support.h"
#include "thumbnail/videothumbnailbackend.h"
#include "thumbnail/videothumbnailextractionworkflow.h"
#include "thumbnail/videothumbnailextractor.h"

#include <QColor>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTest>
#include <QVector>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
template <typename Operation>
const Operation* firstOperation(const kiriview::VideoThumbnailExtractionPlan& plan)
{
    for (const kiriview::VideoThumbnailExtractionOperation& operation : plan) {
        if (const auto* match = std::get_if<Operation>(&operation)) {
            return match;
        }
    }
    return nullptr;
}

QImage solidImage(Qt::GlobalColor color)
{
    QImage image(QSize(16, 16), QImage::Format_RGB32);
    image.fill(QColor(color));
    return image;
}

QImage interestingImage()
{
    QImage image(QSize(16, 16), QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(
                x, y, x < image.width() / 2 ? QColor(Qt::black) : QColor(Qt::white));
        }
    }
    return image;
}

class ManualVideoThumbnailBackend final : public kiriview::VideoThumbnailBackend
{
public:
    void setCallbacks(kiriview::VideoThumbnailBackendCallbacks callbacks) override
    {
        m_callbacks = std::move(callbacks);
    }
    void setSource(const QUrl& sourceUrl) override
    {
        commands.push_back(QStringLiteral("source"));
        source = sourceUrl;
    }
    void play() override { commands.push_back(QStringLiteral("play")); }
    void pause() override { commands.push_back(QStringLiteral("pause")); }
    void stop() override
    {
        commands.push_back(QStringLiteral("stop"));
        if (emitErrorDuringStop && m_callbacks.errorOccurred) {
            m_callbacks.errorOccurred(QStringLiteral("late stop error"));
        }
    }
    void setPosition(qint64 position) override
    {
        commands.push_back(QStringLiteral("seek"));
        lastPosition = position;
    }

    void emitFacts(kiriview::VideoThumbnailBackendMediaFacts facts)
    {
        if (m_callbacks.mediaFactsChanged) {
            m_callbacks.mediaFactsChanged(facts);
        }
    }
    void emitPosition(qint64 position)
    {
        if (m_callbacks.positionChanged) {
            m_callbacks.positionChanged(position);
        }
    }
    void emitFrame(QImage image)
    {
        if (m_callbacks.frameAvailable) {
            m_callbacks.frameAvailable(std::move(image));
        }
    }
    void emitMetadata(kiriview::VideoThumbnailEmbeddedImages images)
    {
        if (m_callbacks.metadataAvailable) {
            m_callbacks.metadataAvailable(std::move(images));
        }
    }
    void emitError(QString errorString)
    {
        if (m_callbacks.errorOccurred) {
            m_callbacks.errorOccurred(std::move(errorString));
        }
    }

    kiriview::VideoThumbnailBackendCallbacks m_callbacks;
    std::vector<QString> commands;
    QUrl source;
    qint64 lastPosition = -1;
    bool emitErrorDuringStop = false;
};

class ManualVideoThumbnailBackendFactory
{
public:
    kiriview::VideoThumbnailBackendFactory factory()
    {
        return [this](QObject*) {
            auto backend = std::make_unique<ManualVideoThumbnailBackend>();
            m_backend = backend.get();
            return backend;
        };
    }

    ManualVideoThumbnailBackend& backend() { return *m_backend; }

private:
    ManualVideoThumbnailBackend* m_backend = nullptr;
};
}

class TestVideoThumbnailExtractor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void interestingFrameRejectsSolidImage();
    void interestingFrameAcceptsHighVarianceImage();
    void interestingFrameRejectsNullImage();
    void candidatePositionsUseTotemOrder();
    void candidatePositionsRejectNonPositiveDuration();
    void framePostProcessingScalesToBucketEdge();
    void framePostProcessingRejectsNullImages();
    void embeddedImagePostProcessingPrefersCoverArtImage();
    void embeddedImagePostProcessingUsesThumbnailFallback();
    void workflowRejectsInvalidRequestWithoutStartingRuntime();
    void workflowStartsTimeoutAndSourceWithoutQtObjects();
    void workflowNonSeekableMediaUsesFirstFrameFallback();
    void workflowEndOfMediaFailsFirstFrameFallback();
    void workflowCandidateFrameWaitsForPositionAndAcceptsInterestingFrame();
    void workflowTimeoutUsesLastBoringCandidate();
    void workflowMetadataWinsAndLateEventsAreIgnored();
    void workflowCancellationStopsRuntimeWithoutCompletion();
    void runtimeUsesInjectedBackendAndManualTimer();
    void runtimeCancellationRejectsReentrantAndLateCallbacks();
};

void TestVideoThumbnailExtractor::interestingFrameRejectsSolidImage()
{
    QImage frame(QSize(16, 16), QImage::Format_RGB32);
    frame.fill(QColor(Qt::black));

    QVERIFY(!kiriview::videoThumbnailFrameIsInteresting(frame));
}

void TestVideoThumbnailExtractor::interestingFrameAcceptsHighVarianceImage()
{
    QImage frame(QSize(16, 16), QImage::Format_RGB32);
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            frame.setPixelColor(
                x, y, x < frame.width() / 2 ? QColor(Qt::black) : QColor(Qt::white));
        }
    }

    QVERIFY(kiriview::videoThumbnailFrameIsInteresting(frame));
}

void TestVideoThumbnailExtractor::interestingFrameRejectsNullImage()
{
    QVERIFY(!kiriview::videoThumbnailFrameIsInteresting({}));
}

void TestVideoThumbnailExtractor::candidatePositionsUseTotemOrder()
{
    const QVector<qint64> positions = kiriview::videoThumbnailCandidatePositions(90000);

    QCOMPARE(positions, QVector<qint64>({ 30000, 60000, 9000, 81000, 45000 }));
}

void TestVideoThumbnailExtractor::candidatePositionsRejectNonPositiveDuration()
{
    QVERIFY(kiriview::videoThumbnailCandidatePositions(0).isEmpty());
    QVERIFY(kiriview::videoThumbnailCandidatePositions(-1).isEmpty());
}

void TestVideoThumbnailExtractor::framePostProcessingScalesToBucketEdge()
{
    QImage frame(QSize(400, 200), QImage::Format_RGB32);
    frame.fill(QColor(Qt::green));
    QString errorString;

    const QImage thumbnail
        = kiriview::videoThumbnailImageFromFrameImage(std::move(frame), 128, &errorString);

    QCOMPARE(thumbnail.size(), QSize(128, 64));
    QCOMPARE(thumbnail.format(), QImage::Format_RGB32);
    QVERIFY(errorString.isEmpty());
}

void TestVideoThumbnailExtractor::framePostProcessingRejectsNullImages()
{
    QString errorString;

    const QImage thumbnail = kiriview::videoThumbnailImageFromFrameImage({}, 128, &errorString);

    QVERIFY(thumbnail.isNull());
    QCOMPARE(errorString, QStringLiteral("video frame produced no image"));
}

void TestVideoThumbnailExtractor::embeddedImagePostProcessingPrefersCoverArtImage()
{
    QImage cover(QSize(300, 150), QImage::Format_RGB32);
    cover.fill(QColor(Qt::red));
    QImage thumbnailFallback(QSize(40, 20), QImage::Format_RGB32);
    thumbnailFallback.fill(QColor(Qt::blue));
    QString errorString;

    const QImage thumbnail = kiriview::videoThumbnailImageFromEmbeddedImages(
        { cover, thumbnailFallback }, 128, &errorString);

    QCOMPARE(thumbnail.size(), QSize(128, 64));
    QCOMPARE(thumbnail.pixelColor(0, 0), QColor(Qt::red));
    QVERIFY(errorString.isEmpty());
}

void TestVideoThumbnailExtractor::embeddedImagePostProcessingUsesThumbnailFallback()
{
    QImage embeddedThumbnail(QSize(90, 60), QImage::Format_RGB32);
    embeddedThumbnail.fill(QColor(Qt::blue));
    QString errorString;

    const QImage thumbnail = kiriview::videoThumbnailImageFromEmbeddedImages(
        { {}, embeddedThumbnail }, 45, &errorString);

    QCOMPARE(thumbnail.size(), QSize(45, 30));
    QCOMPARE(thumbnail.pixelColor(0, 0), QColor(Qt::blue));
    QVERIFY(errorString.isEmpty());
}

void TestVideoThumbnailExtractor::workflowStartsTimeoutAndSourceWithoutQtObjects()
{
    kiriview::VideoThumbnailExtractionWorkflow workflow;
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4"));

    const kiriview::VideoThumbnailExtractionPlan plan = workflow.start({ sourceUrl, 128 });

    const auto* timeout = firstOperation<kiriview::StartVideoThumbnailTimeout>(plan);
    const auto* source = firstOperation<kiriview::SetVideoThumbnailSource>(plan);
    QVERIFY(timeout != nullptr);
    QCOMPARE(timeout->intervalMsec, 10000);
    QVERIFY(source != nullptr);
    QCOMPARE(source->sourceUrl, sourceUrl);
    QVERIFY(firstOperation<kiriview::CompleteVideoThumbnailExtraction>(plan) == nullptr);
}

void TestVideoThumbnailExtractor::workflowRejectsInvalidRequestWithoutStartingRuntime()
{
    kiriview::VideoThumbnailExtractionWorkflow emptySourceWorkflow;
    const kiriview::VideoThumbnailExtractionPlan emptySourcePlan
        = emptySourceWorkflow.start({ {}, 128 });
    const auto* emptySourceCompletion
        = firstOperation<kiriview::CompleteVideoThumbnailExtraction>(emptySourcePlan);
    QVERIFY(emptySourceCompletion != nullptr);
    QCOMPARE(
        emptySourceCompletion->result.status, kiriview::VideoThumbnailExtractionStatus::Failed);
    QVERIFY(firstOperation<kiriview::StartVideoThumbnailTimeout>(emptySourcePlan) == nullptr);
    QVERIFY(firstOperation<kiriview::SetVideoThumbnailSource>(emptySourcePlan) == nullptr);

    kiriview::VideoThumbnailExtractionWorkflow invalidSizeWorkflow;
    const kiriview::VideoThumbnailExtractionPlan invalidSizePlan
        = invalidSizeWorkflow.start({ QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 0 });
    QVERIFY(firstOperation<kiriview::CompleteVideoThumbnailExtraction>(invalidSizePlan) != nullptr);
    QVERIFY(firstOperation<kiriview::StartVideoThumbnailTimeout>(invalidSizePlan) == nullptr);
}

void TestVideoThumbnailExtractor::workflowNonSeekableMediaUsesFirstFrameFallback()
{
    kiriview::VideoThumbnailExtractionWorkflow workflow;
    workflow.start({ QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 128 });

    const kiriview::VideoThumbnailExtractionPlan readyPlan = workflow.handleMediaFacts(
        { kiriview::VideoThumbnailBackendMediaStatus::Ready, 90000, false });

    const auto* seek = firstOperation<kiriview::SeekVideoThumbnail>(readyPlan);
    QVERIFY(seek != nullptr);
    QCOMPARE(seek->positionMsec, qint64(0));
    QVERIFY(firstOperation<kiriview::PlayVideoThumbnailBackend>(readyPlan) != nullptr);
    const kiriview::VideoThumbnailExtractionPlan framePlan
        = workflow.handleFrame(solidImage(Qt::black));
    QVERIFY(firstOperation<kiriview::CompleteVideoThumbnailExtraction>(framePlan) != nullptr);
}

void TestVideoThumbnailExtractor::workflowEndOfMediaFailsFirstFrameFallback()
{
    kiriview::VideoThumbnailExtractionWorkflow workflow;
    workflow.start({ QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 128 });
    workflow.handleMediaFacts({ kiriview::VideoThumbnailBackendMediaStatus::Ready, 0, false });

    const kiriview::VideoThumbnailExtractionPlan endPlan = workflow.handleMediaFacts(
        { kiriview::VideoThumbnailBackendMediaStatus::EndOfMedia, 0, false });
    const auto* completion = firstOperation<kiriview::CompleteVideoThumbnailExtraction>(endPlan);
    QVERIFY(completion != nullptr);
    QCOMPARE(completion->result.status, kiriview::VideoThumbnailExtractionStatus::Failed);
}

void TestVideoThumbnailExtractor::workflowCandidateFrameWaitsForPositionAndAcceptsInterestingFrame()
{
    kiriview::VideoThumbnailExtractionWorkflow workflow;
    workflow.start({ QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 128 });
    const kiriview::VideoThumbnailExtractionPlan readyPlan = workflow.handleMediaFacts(
        { kiriview::VideoThumbnailBackendMediaStatus::Ready, 90000, true });
    const auto* seek = firstOperation<kiriview::SeekVideoThumbnail>(readyPlan);
    QVERIFY(seek != nullptr);
    QCOMPARE(seek->positionMsec, qint64(30000));

    const kiriview::VideoThumbnailExtractionPlan earlyFrame
        = workflow.handleFrame(interestingImage());
    QVERIFY(firstOperation<kiriview::CompleteVideoThumbnailExtraction>(earlyFrame) == nullptr);

    workflow.handlePositionChanged(30000);
    const kiriview::VideoThumbnailExtractionPlan framePlan
        = workflow.handleFrame(interestingImage());
    const auto* completion = firstOperation<kiriview::CompleteVideoThumbnailExtraction>(framePlan);
    QVERIFY(completion != nullptr);
    QCOMPARE(completion->result.status, kiriview::VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(completion->result.image.size(), QSize(16, 16));
}

void TestVideoThumbnailExtractor::workflowTimeoutUsesLastBoringCandidate()
{
    kiriview::VideoThumbnailExtractionWorkflow workflow;
    workflow.start({ QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 64 });
    workflow.handleMediaFacts({ kiriview::VideoThumbnailBackendMediaStatus::Ready, 90000, true });
    workflow.handlePositionChanged(30000);
    workflow.handleFrame(solidImage(Qt::black));

    const kiriview::VideoThumbnailExtractionPlan timeoutPlan = workflow.handleTimeout();
    const auto* completion
        = firstOperation<kiriview::CompleteVideoThumbnailExtraction>(timeoutPlan);
    QVERIFY(completion != nullptr);
    QCOMPARE(completion->result.status, kiriview::VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(completion->result.image.size(), QSize(16, 16));
}

void TestVideoThumbnailExtractor::workflowMetadataWinsAndLateEventsAreIgnored()
{
    kiriview::VideoThumbnailExtractionWorkflow workflow;
    workflow.start({ QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 32 });

    const kiriview::VideoThumbnailExtractionPlan metadataPlan
        = workflow.handleMetadata({ solidImage(Qt::red), solidImage(Qt::blue) });
    const auto* completion
        = firstOperation<kiriview::CompleteVideoThumbnailExtraction>(metadataPlan);
    QVERIFY(completion != nullptr);
    QCOMPARE(completion->result.status, kiriview::VideoThumbnailExtractionStatus::Ready);
    QCOMPARE(completion->result.image.pixelColor(0, 0), QColor(Qt::red));

    QVERIFY(workflow.handleBackendError(QStringLiteral("late error")).empty());
    QVERIFY(workflow.handleTimeout().empty());
    QVERIFY(workflow.handleFrame(interestingImage()).empty());
}

void TestVideoThumbnailExtractor::workflowCancellationStopsRuntimeWithoutCompletion()
{
    kiriview::VideoThumbnailExtractionWorkflow workflow;
    workflow.start({ QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 128 });

    const kiriview::VideoThumbnailExtractionPlan cancelPlan = workflow.cancel();

    QVERIFY(firstOperation<kiriview::StopVideoThumbnailTimeout>(cancelPlan) != nullptr);
    QVERIFY(firstOperation<kiriview::StopVideoThumbnailBackend>(cancelPlan) != nullptr);
    QVERIFY(firstOperation<kiriview::CompleteVideoThumbnailExtraction>(cancelPlan) == nullptr);
    QVERIFY(workflow.cancel().empty());
    QVERIFY(workflow.handleBackendError(QStringLiteral("late error")).empty());
}

void TestVideoThumbnailExtractor::runtimeUsesInjectedBackendAndManualTimer()
{
    QObject owner;
    ManualVideoThumbnailBackendFactory backendFactory;
    kiriview::TestSupport::ManualTimerScheduler timerScheduler;
    std::optional<kiriview::VideoThumbnailExtractionResult> result;
    kiriview::VideoThumbnailExtractionDependencies dependencies;
    dependencies.backendFactory = backendFactory.factory();
    dependencies.timerScheduler = timerScheduler.scheduler();

    kiriview::ImageIoJob job = kiriview::startVideoThumbnailExtraction(
        &owner, { QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 128 },
        [&result](kiriview::VideoThumbnailExtractionResult value) { result = std::move(value); },
        std::move(dependencies));

    QVERIFY(job.isActive());
    QCOMPARE(timerScheduler.timerCount(), std::size_t(1));
    QCOMPARE(timerScheduler.timerAt(0).intervalMsec(), 10000);
    QVERIFY(timerScheduler.timerAt(0).active());
    QCOMPARE(
        backendFactory.backend().source, QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")));

    timerScheduler.timerAt(0).fire();
    QVERIFY(result.has_value());
    QCOMPARE(result->status, kiriview::VideoThumbnailExtractionStatus::Failed);
    QVERIFY(!job.isActive());
}

void TestVideoThumbnailExtractor::runtimeCancellationRejectsReentrantAndLateCallbacks()
{
    QObject owner;
    ManualVideoThumbnailBackendFactory backendFactory;
    kiriview::TestSupport::ManualTimerScheduler timerScheduler;
    int completionCount = 0;
    kiriview::VideoThumbnailExtractionDependencies dependencies;
    dependencies.backendFactory = backendFactory.factory();
    dependencies.timerScheduler = timerScheduler.scheduler();

    kiriview::ImageIoJob job = kiriview::startVideoThumbnailExtraction(
        &owner, { QUrl::fromLocalFile(QStringLiteral("/media/clip.mp4")), 128 },
        [&completionCount](kiriview::VideoThumbnailExtractionResult) { ++completionCount; },
        std::move(dependencies));
    backendFactory.backend().emitErrorDuringStop = true;

    job.cancel();
    backendFactory.backend().emitError(QStringLiteral("late error"));
    backendFactory.backend().emitFrame(interestingImage());
    timerScheduler.timerAt(0).fire();

    QCOMPARE(completionCount, 0);
    QVERIFY(!timerScheduler.timerAt(0).active());
}

QTEST_GUILESS_MAIN(TestVideoThumbnailExtractor)

#include "tst_videothumbnailextractor.moc"
