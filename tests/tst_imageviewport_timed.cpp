#include "imageviewport_test_support.h"

class ImageViewportTimedTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportTimedTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void stillImageFactoryRejectsInvalidPayloadByteSize();
    void timedFrameListBuilderValidatesEntries();
    void timedFrameListRejectsPublishedDurationLimits();
    void timedFrameListRejectsPublishedFrameCountLimit();
    void timedFrameListAllowsCumulativePayloadsAbovePerFrameLimit();
    void timedFrameListClearDiagnosticOnlyPreservesCountNotification();
    void timedFrameListAssignmentPublishesInitialTimedState();
    void timedFrameListSeekCommandsSelectDocumentedTargets();
    void timedFrameListSecondarySeekCommandsSelectRoleTargets();
    void timedFrameListSecondaryPlaybackAdvancesRoleTarget();
    void timedFrameListSeekWhilePlayingWaitsForRenderCommit();
    void timedFrameListSeekPreservesGeometryObservations();
    void timedFrameListPlaybackCommandsUpdatePhase();
    void timedFrameListSecondaryPauseStopNoopWhenPrimaryPlaying();
    void timedFrameListSecondaryStopRestoresRoleTarget();
    void timedFrameListSecondaryInvalidSeekUsesPresentRolePrecedence();
    void timedFrameListAuthoredAutoplayStartsInitialPlayback();
    void timedFrameListAuthoredInfiniteLoopControlsDefaultPlayback();
    void timedFrameListAuthoredFiniteLoopStopsAfterFinalIteration();
    void timedFrameListPauseWhileStoppedAndRenderWaitingPreservesRequest();
    void timedFrameListPlayCommandPreservesElapsedPosition();
    void timedFrameListBackgroundOnlyChangesPreserveRequestAndPlayback();
    void timedFrameListPlaybackAdvancesDeterministically();
    void timedFrameListPlaybackAdvancesFromRuntimeTimer();
    void timedFrameListPlaybackPreservesGeometryObservations();
    void timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay();
    void timedFrameListPausedRenderWaitingCommitStaysPaused();
    void timedFrameListPlayWhilePausedAndRenderWaitingResumesWaiting();
    void timedFrameListStopAfterPauseWhileRenderWaitingRestoresPreviousDisplay();
};

static ImageViewport::CommandOutcome setBackgroundCommand(
    ImageViewport& item, ImageViewport::BackgroundMode mode, QColor color)
{
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(mode);
    command.setBackgroundColor(color);
    return item.setPresentation(command).outcome();
}

void ImageViewportTimedTest::stillImageFactoryRejectsInvalidPayloadByteSize()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const std::unique_ptr<ImageFrame> frame = makeImageFrameWithPayloadByteSizeForTest(image, -1);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(frame.get()));

    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(result->errorString().contains(QStringLiteral("payload byte size")));
}

void ImageViewportTimedTest::timedFrameListBuilderValidatesEntries()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QImage differentSizeImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    differentSizeImage.fill(Qt::transparent);
    ImageFrame differentSizeFrame(differentSizeImage);

    TimedImageFrameList list;
    const QMetaObject* metaObject = list.metaObject();
    QVERIFY(metaObject->indexOfProperty("count") >= 0);
    QVERIFY(metaObject->indexOfProperty("frames") >= 0);
    QVERIFY(metaObject->indexOfProperty("autoplay") >= 0);
    QVERIFY(metaObject->indexOfProperty("loopMode") >= 0);
    QVERIFY(metaObject->indexOfProperty("loopCount") >= 0);
    QVERIFY(metaObject->indexOfProperty("errorString") >= 0);
    QVERIFY(metaObject->indexOfProperty("warningString") < 0);
    QVERIFY(
        metaObject->indexOfMethod(QMetaObject::normalizedSignature("appendFrame(ImageFrame*,int)"))
        >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("clear()")) >= 0);

    QCOMPARE(list.count(), 0);
    QCOMPARE(list.appendFrame(nullptr, 100), false);
    QCOMPARE(list.count(), 0);
    QVERIFY(list.errorString().contains(QStringLiteral("ImageFrame")));

    QCOMPARE(list.appendFrame(&frame, 0), false);
    QCOMPARE(list.count(), 0);
    QVERIFY(list.errorString().contains(QStringLiteral("duration")));

    QCOMPARE(list.appendFrame(&frame, -1), false);
    QCOMPARE(list.count(), 0);
    QVERIFY(list.errorString().contains(QStringLiteral("duration")));

    QCOMPARE(list.appendFrame(&frame, 100), true);
    QCOMPARE(list.appendFrame(&differentSizeFrame, 100), false);
    QCOMPARE(list.count(), 1);
    QVERIFY(list.errorString().contains(QStringLiteral("logical size")));

    list.clear();
    QCOMPARE(list.count(), 0);
    QCOMPARE(list.errorString(), QString());
}

void ImageViewportTimedTest::timedFrameListRejectsPublishedDurationLimits()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);

    TimedImageFrameList frameDurationList;
    QCOMPARE(frameDurationList.appendFrame(&frame, ImageSequenceLimits::maximumFrameDurationMilliseconds() + 1),
        false);
    QCOMPARE(frameDurationList.count(), 0);
    QVERIFY(frameDurationList.errorString().contains(QStringLiteral("maximumFrameDurationMilliseconds")));

    TimedImageFrameList totalDurationList;
    QCOMPARE(
        totalDurationList.appendFrame(&frame, ImageSequenceLimits::maximumTotalDurationMilliseconds()),
        true);
    QCOMPARE(totalDurationList.appendFrame(&frame, 1), false);
    QCOMPARE(totalDurationList.count(), 1);
    QVERIFY(
        totalDurationList.errorString().contains(QStringLiteral("maximumTotalDurationMilliseconds")));
}

void ImageViewportTimedTest::timedFrameListRejectsPublishedFrameCountLimit()
{
    QImage image(1, 1, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);

    TimedImageFrameList list;
    for (int index = 0; index < ImageSequenceLimits::maximumFrameCount(); ++index) {
        QCOMPARE(list.appendFrame(&frame, 1), true);
    }

    QCOMPARE(list.count(), ImageSequenceLimits::maximumFrameCount());
    QCOMPARE(list.appendFrame(&frame, 1), false);
    QCOMPARE(list.count(), ImageSequenceLimits::maximumFrameCount());
    QVERIFY(list.errorString().contains(QStringLiteral("maximumFrameCount")));
}

void ImageViewportTimedTest::timedFrameListAllowsCumulativePayloadsAbovePerFrameLimit()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const qsizetype admittedPayloadSize
        = ImageSequenceLimits::maximumPayloadBytes() / 2 + 1;
    QVERIFY(admittedPayloadSize <= ImageSequenceLimits::maximumPayloadBytes());
    const std::unique_ptr<ImageFrame> firstFrame
        = makeImageFrameWithPayloadByteSizeForTest(image, admittedPayloadSize);
    const std::unique_ptr<ImageFrame> secondFrame
        = makeImageFrameWithPayloadByteSizeForTest(image, admittedPayloadSize);

    TimedImageFrameList list;
    QCOMPARE(list.appendFrame(firstFrame.get(), 100), true);
    QCOMPARE(list.appendFrame(secondFrame.get(), 100), true);
    QCOMPARE(list.count(), 2);
    QCOMPARE(list.errorString(), QString());

    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);
}

void ImageViewportTimedTest::timedFrameListClearDiagnosticOnlyPreservesCountNotification()
{
    TimedImageFrameList list;

    QVERIFY(!list.appendFrame(nullptr, 100));
    QCOMPARE(list.count(), 0);
    QVERIFY(!list.errorString().isEmpty());

    QSignalSpy countSpy(&list, &TimedImageFrameList::countChanged);
    QSignalSpy diagnosticsSpy(&list, &TimedImageFrameList::diagnosticsChanged);

    list.clear();

    QCOMPARE(list.count(), 0);
    QCOMPARE(list.errorString(), QString());
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 1);
}

void ImageViewportTimedTest::timedFrameListAssignmentPublishesInitialTimedState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(primaryFrameCount(item), 2);
    QCOMPARE(primaryTotalDuration(item), 350);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 1);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), 350);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
}

void ImageViewportTimedTest::timedFrameListSeekCommandsSelectDocumentedTargets()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    const ImageViewportRevisionToken initialRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "requestRevision", initialRequestRevision);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedPosition(item), 100);

    const ImageViewportRevisionToken acceptedFrameSeekRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 2).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), acceptedFrameSeekRevision);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 349).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 349);
    QCOMPARE(primaryDisplayedPosition(item), 100);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 350).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedPosition(item), 100);

    const ImageViewportRevisionToken acceptedPositionSeekRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 351).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), acceptedPositionSeekRevision);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportTimedTest::timedFrameListSecondarySeekCommandsSelectRoleTargets()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);

    TimedImageFrameList primaryList;
    QVERIFY(primaryList.appendFrame(&firstFrame, 100));
    QVERIFY(primaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        factory.fromTimedFrameList(&primaryList));
    QVERIFY(primaryResult->sequence());

    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryRequestedPosition(item), 0);
    QCOMPARE(secondaryDisplayedPosition(item), 0);

    const ImageViewportRevisionToken initialRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewportPageRole::Secondary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "requestRevision", initialRequestRevision);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedPosition(item), 100);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Secondary, 350).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 350);
    QCOMPARE(secondaryDisplayedPosition(item), 100);
}

void ImageViewportTimedTest::timedFrameListSecondaryPlaybackAdvancesRoleTarget()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);

    TimedImageFrameList primaryList;
    QVERIFY(primaryList.appendFrame(&firstFrame, 100));
    QVERIFY(primaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        factory.fromTimedFrameList(&primaryList));
    QVERIFY(primaryResult->sequence());

    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));

    advancePlaybackForTest(item, 100);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedPosition(item), 100);

    QCOMPARE(item.pause(ImageViewportPageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));
}

void ImageViewportTimedTest::timedFrameListSeekWhilePlayingWaitsForRenderCommit()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 100.0));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
}

void ImageViewportTimedTest::timedFrameListSeekPreservesGeometryObservations()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));
    acknowledgePendingRenderCommitForTest(item);
}

void ImageViewportTimedTest::timedFrameListPlaybackCommandsUpdatePhase()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTimedTest::timedFrameListSecondaryPauseStopNoopWhenPrimaryPlaying()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList primaryList;
    QVERIFY(primaryList.appendFrame(&firstFrame, 100));
    QVERIFY(primaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        factory.fromTimedFrameList(&primaryList));
    QVERIFY(primaryResult->sequence());

    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);

    QCOMPARE(item.stop(ImageViewportPageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTimedTest::timedFrameListSecondaryStopRestoresRoleTarget()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList primaryList;
    QVERIFY(primaryList.appendFrame(&firstFrame, 100));
    QVERIFY(primaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        factory.fromTimedFrameList(&primaryList));
    QVERIFY(primaryResult->sequence());

    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 100);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedPosition(item), 100);

    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);

    QCOMPARE(item.stop(ImageViewportPageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryRequestedPosition(item), 0);
    QCOMPARE(secondaryDisplayedPosition(item), 0);
}

void ImageViewportTimedTest::timedFrameListSecondaryInvalidSeekUsesPresentRolePrecedence()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList primaryList;
    QVERIFY(primaryList.appendFrame(&firstFrame, 100));
    QVERIFY(primaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        factory.fromTimedFrameList(&primaryList));
    QVERIFY(primaryResult->sequence());

    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.seek(ImageViewportPageRole::Secondary, -1).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(item.seek(ImageViewportPageRole::Secondary, 2).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Secondary, 351).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Secondary, -1).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
}

void ImageViewportTimedTest::timedFrameListAuthoredAutoplayStartsInitialPlayback()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 100));
    ImageSequenceAuthoredAnimationFacts facts;
    facts.setAutoplay(true);
    list.setAuthoredAnimationFacts(facts);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    advancePlaybackForTest(item, 100);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
}

void ImageViewportTimedTest::timedFrameListAuthoredInfiniteLoopControlsDefaultPlayback()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 100));
    list.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::infiniteLoop());
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 200);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
}

void ImageViewportTimedTest::timedFrameListAuthoredFiniteLoopStopsAfterFinalIteration()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 100));
    list.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(2));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 200);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    advancePlaybackForTest(item, 200);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportTimedTest::timedFrameListPauseWhileStoppedAndRenderWaitingPreservesRequest()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(0.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
}

void ImageViewportTimedTest::timedFrameListPlayCommandPreservesElapsedPosition()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport playingItem;
    playingItem.setSize(QSizeF(100.0, 100.0));
    playingItem.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(playingItem);
    const QMetaObject* metaObject = playingItem.metaObject();

    QCOMPARE(playingItem.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(playingItem, 80);
    QCOMPARE(playingItem.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(playingItem, 20);
    acknowledgePendingRenderCommitForTest(playingItem);
    QCOMPARE(playbackPhaseValue(playingItem), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(playingItem), 1);
    QCOMPARE(primaryDisplayedFrame(playingItem), 1);
    QCOMPARE(primaryRequestedPosition(playingItem), 100);
    QCOMPARE(primaryDisplayedPosition(playingItem), 100);

    ImageViewport pausedItem;
    pausedItem.setSize(QSizeF(100.0, 100.0));
    pausedItem.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(pausedItem);

    QCOMPARE(pausedItem.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(pausedItem, 80);
    QCOMPARE(pausedItem.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(pausedItem.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(pausedItem, 20);
    acknowledgePendingRenderCommitForTest(pausedItem);
    QCOMPARE(playbackPhaseValue(pausedItem), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(pausedItem), 1);
    QCOMPARE(primaryDisplayedFrame(pausedItem), 1);
    QCOMPARE(primaryRequestedPosition(pausedItem), 100);
    QCOMPARE(primaryDisplayedPosition(pausedItem), 100);
}

void ImageViewportTimedTest::timedFrameListBackgroundOnlyChangesPreserveRequestAndPlayback()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));

    const int requestStatus = requestStatusValue(item);
    const int requestReason = requestReasonValue(item);
    const int displayStatus = displayStatusValue(item);
    const int playbackPhase = playbackPhaseValue(item);
    const int requestedFrame = primaryRequestedFrame(item);
    const int displayedFrame = primaryDisplayedFrame(item);
    const int requestedPosition = primaryRequestedPosition(item);
    const int displayedPosition = primaryDisplayedPosition(item);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(requestStatusValue(item), requestStatus);
    QCOMPARE(requestReasonValue(item), requestReason);
    QCOMPARE(displayStatusValue(item), displayStatus);
    QCOMPARE(playbackPhaseValue(item), playbackPhase);
    QCOMPARE(primaryRequestedFrame(item), requestedFrame);
    QCOMPARE(primaryDisplayedFrame(item), displayedFrame);
    QCOMPARE(primaryRequestedPosition(item), requestedPosition);
    QCOMPARE(primaryDisplayedPosition(item), displayedPosition);
}

void ImageViewportTimedTest::timedFrameListPlaybackAdvancesDeterministically()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const quint64 initialRequestId = activeRequestIdForTest(item);
    QVERIFY(initialRequestId > 0);

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 99);
    QCOMPARE(activeRequestIdForTest(item), initialRequestId);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 1);
    const quint64 playbackRequestId = activeRequestIdForTest(item);
    QVERIFY(playbackRequestId > initialRequestId);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);

    advancePlaybackForTest(item, 249);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 1);

    advancePlaybackForTest(item, 1);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportTimedTest::timedFrameListPlaybackAdvancesFromRuntimeTimer()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 20));
    QVERIFY(list.appendFrame(&secondFrame, 1000));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QTRY_VERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 20);
    QCOMPARE(primaryDisplayedPosition(item), 20);
}

void ImageViewportTimedTest::timedFrameListPlaybackPreservesGeometryObservations()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));

    advancePlaybackForTest(item, 100);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));
}

void ImageViewportTimedTest::timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
}

void ImageViewportTimedTest::timedFrameListPausedRenderWaitingCommitStaysPaused()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportTimedTest::timedFrameListPlayWhilePausedAndRenderWaitingResumesWaiting()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportTimedTest::timedFrameListStopAfterPauseWhileRenderWaitingRestoresPreviousDisplay()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
}

QTEST_MAIN(ImageViewportTimedTest)

#include "tst_imageviewport_timed.moc"
