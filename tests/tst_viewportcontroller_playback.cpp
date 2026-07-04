#include "imageviewport.h"
#include "viewportcontroller_p.h"

#include <QtTest/QTest>

#include <memory>

namespace {

class PlaybackControllerContext final : public ViewportControllerContext
{
public:
    ImageSequence* sequence = nullptr;
    bool timed = true;
    QSizeF size { 100.0, 100.0 };
    QSizeF logicalSize { 16.0, 8.0 };
    QVector<int> durations { 100, 250 };
    QVector<QImage> images;

    QRectF contentRect() const override
    {
        return itemBounds().isEmpty() ? QRectF() : QRectF(0.0, 25.0, size.width(), 50.0);
    }

    QRectF visibleImageRect() const override
    {
        return itemBounds().isEmpty() ? QRectF() : QRectF(0.0, 0.0, 16.0, 8.0);
    }

    QRectF itemBounds() const override
    {
        if (size.width() <= 0.0 || size.height() <= 0.0) {
            return {};
        }
        return QRectF(0.0, 0.0, size.width(), size.height());
    }

    bool hasActiveRequest() const override { return sequence != nullptr; }
    bool hasDisplayableSequence() const override { return sequence != nullptr; }
    bool hasTimedSequence() const override { return sequence != nullptr && timed; }
    int frameCount() const override { return durations.size(); }
    int totalDuration() const override { return sequenceTotalDuration(); }
    int sequenceFrameCount() const override { return durations.size(); }
    QSizeF sequenceLogicalSize() const override { return logicalSize; }
    double width() const override { return size.width(); }
    double height() const override { return size.height(); }

    int sequenceFrameStartPosition(int frame) const override
    {
        if (frame < 0 || frame >= durations.size()) {
            return -1;
        }
        int position = 0;
        for (int index = 0; index < frame; ++index) {
            position += durations.at(index);
        }
        return position;
    }

    int sequenceFrameIndexForPosition(int position) const override
    {
        if (position < 0) {
            return -1;
        }
        int start = 0;
        for (int index = 0; index < durations.size(); ++index) {
            const int end = start + durations.at(index);
            if (position >= start && position < end) {
                return index;
            }
            start = end;
        }
        return -1;
    }

    QImage sequenceFrameImage(int frame) const override
    {
        return frame >= 0 && frame < images.size() ? images.at(frame) : QImage();
    }

private:
    int sequenceTotalDuration() const override
    {
        int total = 0;
        for (int duration : durations) {
            total += duration;
        }
        return total;
    }
};

std::unique_ptr<ImageSequenceFactoryResult> makeTimedSequence(
    ImageSequenceFactory& factory, PlaybackControllerContext& context)
{
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    if (!list.appendFrame(&firstFrame, context.durations.at(0))
        || !list.appendFrame(&secondFrame, context.durations.at(1))) {
        return {};
    }
    context.images = { firstImage, secondImage };
    auto result = std::unique_ptr<ImageSequenceFactoryResult>(factory.fromTimedFrameList(&list));
    if (!result || !result->sequence()) {
        return {};
    }
    context.sequence = result->sequence();
    return result;
}

void acknowledgePendingRenderCommit(ViewportController& controller)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    if (!synchronization.pendingTargetCommit) {
        return;
    }
    const ImageViewportInternal::PreparedPayloadIdentity primaryPayload
        = controller.displayState().pendingRenderPayload.identity().isValid()
        ? controller.displayState().pendingRenderPayload.identity()
        : synchronization.preparedPayload.identity();
    QVector<ViewportRenderRolePayload> rolePayloads {
        { ImageViewport::PageRole::Primary, primaryPayload }
    };
    if (controller.requestState().secondarySequence
        && controller.requestState().secondaryActiveRequest.target.frame >= 0) {
        const ImageViewportInternal::PreparedPayloadIdentity secondaryPayload
            = controller.displayState().secondaryPendingRenderPayload.identity().isValid()
            ? controller.displayState().secondaryPendingRenderPayload.identity()
            : primaryPayload;
        rolePayloads.append({ ImageViewport::PageRole::Secondary, secondaryPayload });
    }
    controller.acknowledgeRenderCommit(
        { primaryPayload, rolePayloads }, true, synchronization);
}

} // namespace

class ViewportControllerPlaybackTest : public QObject
{
    Q_OBJECT

public:
    explicit ViewportControllerPlaybackTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void builtInPlaybackAdvanceUsesExplicitElapsedWithoutTimer();
    void pauseWhileRenderWaitingCommitsWithoutResumingPlayback();
    void explicitSeekWhilePlayingWaitsForRenderCommit();
    void loopingPlaybackWrapsToStart();
    void unsupportedPlayForUntimedSequencePreservesStoppedPhase();
    void invalidSeekWhilePlayingPreservesPlaybackPhase();
};

void ViewportControllerPlaybackTest::builtInPlaybackAdvanceUsesExplicitElapsedWithoutTimer()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().playbackPosition, 0);

    const ViewportCommandResult play = controller.play();
    QCOMPARE(play.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);

    controller.advancePlayback(99);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().playbackPosition, 99);

    context.size = QSizeF(0.0, 100.0);
    controller.advancePlayback(1);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::RenderWaiting);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().playbackPosition, 100);

    controller.advancePlayback(1000);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().playbackPosition, 100);

    context.size = QSizeF(100.0, 100.0);
    controller.handleGeometryChanged({}, {});
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.displayState().displayedRequest.request.target.frame, 1);

    controller.advancePlayback(249);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().playbackPosition, 349);
    controller.advancePlayback(1);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Stopped);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().playbackPosition, 350);
}

void ViewportControllerPlaybackTest::pauseWhileRenderWaitingCommitsWithoutResumingPlayback()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    controller.play();
    context.size = QSizeF(0.0, 100.0);
    controller.advancePlayback(100);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Waiting);

    const ViewportCommandResult pause = controller.pause();
    QCOMPARE(pause.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Paused);

    context.size = QSizeF(100.0, 100.0);
    controller.handleGeometryChanged({}, {});
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Paused);
    QCOMPARE(controller.displayState().displayedRequest.request.target.frame, 1);
}

void ViewportControllerPlaybackTest::explicitSeekWhilePlayingWaitsForRenderCommit()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    controller.play();
    context.size = QSizeF(0.0, 100.0);

    const ViewportCommandResult seek = controller.seek(1);
    QCOMPARE(seek.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Waiting);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Loading);
    QCOMPARE(controller.requestState().reason, ImageViewport::RequestReason::RenderWaiting);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 1);
    QCOMPARE(controller.requestState().activeRequest.target.position, 100);
}

void ViewportControllerPlaybackTest::loopingPlaybackWrapsToStart()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    controller.setLooping(true);
    controller.play();

    controller.advancePlayback(350);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().status, ImageViewport::RequestStatus::Ready);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().activeRequest.target.position, 0);
    QCOMPARE(controller.requestState().playbackPosition, 0);
}

void ViewportControllerPlaybackTest::unsupportedPlayForUntimedSequencePreservesStoppedPhase()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    context.timed = false;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);

    const ViewportCommandResult play = controller.play();
    QCOMPARE(play.outcome, ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Stopped);
    QCOMPARE(
        controller.requestState().commandReason, ImageViewport::CommandReason::UnsupportedRequest);
}

void ViewportControllerPlaybackTest::invalidSeekWhilePlayingPreservesPlaybackPhase()
{
    ImageSequenceFactory factory;
    PlaybackControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeTimedSequence(factory, context);
    QVERIFY(sequence);
    ViewportController controller(context);

    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);
    controller.play();

    const ViewportCommandResult invalidSeek = controller.seek(-1);
    QCOMPARE(invalidSeek.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(controller.requestState().playbackPhase, ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(controller.requestState().activeRequest.target.frame, 0);
    QCOMPARE(controller.requestState().playbackPosition, 0);
}

QTEST_MAIN(ViewportControllerPlaybackTest)

#include "tst_viewportcontroller_playback.moc"
