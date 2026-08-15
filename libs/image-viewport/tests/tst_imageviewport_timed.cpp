// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagesequence_p.h"
#include "imageviewport_test_support.h"

class ImageViewportTimedTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportTimedTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:
    void stillImageFactoryRejectsInvalidPayloadByteSize();
    void timedFrameListBuilderValidatesEntries();
    void timedFrameListRejectsPublishedDurationLimits();
    void timedFrameListRejectsPublishedFrameCountLimit();
    void timedFrameListEnforcesAggregatePayloadBudget();
    void timedFrameListClearDiagnosticOnlyPreservesCountNotification();
    void timedFrameListAssignmentPublishesInitialTimedState();
    void timedFrameListSeekCommandsSelectDocumentedTargets();
    void timedFrameListRequireExactRejectsNewInexactFrame();
    void timedFrameListSecondarySeekCommandsSelectRoleTargets();
    void timedFrameListSecondaryPlaybackAdvancesRoleTarget();
    void timedFrameListRolesPlayAndPauseIndependently();
    void timedFrameListSeekWhilePlayingWaitsForRenderCommit();
    void timedFrameListSeekPreservesGeometryObservations();
    void timedFrameListPlaybackCommandsUpdatePhase();
    void timedFrameListSecondaryPauseStopNoopWhenPrimaryPlaying();
    void timedFrameListSecondaryStopRestoresRoleTarget();
    void timedFrameListSecondaryInvalidSeekUsesPresentRolePrecedence();
    void timedFrameListAuthoredAutoplayStartsInitialPlayback();
    void timedFrameListAuthoredAutoplaySelectsEligibleRole_data();
    void timedFrameListAuthoredAutoplaySelectsEligibleRole();
    void timedFrameListAuthoredInfiniteLoopControlsDefaultPlayback();
    void timedFrameListAuthoredFiniteLoopStopsAfterFinalIteration();
    void timedFrameListSupersededWrapDoesNotConsumeFiniteIteration();
    void timedFrameListSiblingRequestRebindsPendingFiniteWrap();
    void timedFrameListSiblingStopPreservesPendingFiniteWrap();
    void timedFrameListSameTargetRefinementRebindsPendingFiniteWrap();
    void timedFrameListPauseWhileStoppedAndRenderWaitingPreservesRequest();
    void timedFrameListPlayCommandPreservesElapsedPosition();
    void timedFrameListSameTargetRefinementPreservesSchedulerElapsed();
    void timedFrameListBackgroundOnlyChangesPreserveRequestAndPlayback();
    void timedFrameListPlaybackAdvancesDeterministically();
    void timedFrameListPlaybackAdvancesFromRuntimeTimer();
    void timedFrameListPlaybackPreservesGeometryObservations();
    void timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay();
    void timedFrameListPausedRenderWaitingCommitStaysPaused();
    void timedFrameListPlayWhilePausedAndRenderWaitingResumesWaiting();
    void timedFrameListStopAfterPauseWhileRenderWaitingRestoresPreviousDisplay();
};

static ImageViewportCommandOutcome setBackgroundCommand(
    ImageViewport& item, ImageViewportBackgroundMode mode, QColor color)
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
    QVERIFY(metaObject->indexOfMethod("appendFrame(ImageFrame*,int)") >= 0);
    QVERIFY(metaObject->indexOfMethod("clear()") >= 0);

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
    QCOMPARE(frameDurationList.appendFrame(
                 &frame, ImageSequenceLimits::maximumFrameDurationMilliseconds() + 1),
        false);
    QCOMPARE(frameDurationList.count(), 0);
    QVERIFY(frameDurationList.errorString().contains(
        QStringLiteral("maximumFrameDurationMilliseconds")));

    TimedImageFrameList totalDurationList;
    QCOMPARE(totalDurationList.appendFrame(
                 &frame, ImageSequenceLimits::maximumTotalDurationMilliseconds()),
        true);
    QCOMPARE(totalDurationList.appendFrame(&frame, 1), false);
    QCOMPARE(totalDurationList.count(), 1);
    QVERIFY(totalDurationList.errorString().contains(
        QStringLiteral("maximumTotalDurationMilliseconds")));
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

void ImageViewportTimedTest::timedFrameListEnforcesAggregatePayloadBudget()
{
    QImage image(1, 1, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const qint64 maximumPayloadBytes = ImageSequenceLimits::maximumTimedListPayloadBytes();
    const qsizetype minimumPayloadBytes = image.sizeInBytes();
    QVERIFY(maximumPayloadBytes - minimumPayloadBytes >= minimumPayloadBytes);
    const std::unique_ptr<ImageFrame> exactBoundaryFirstFrame
        = makeImageFrameWithPayloadByteSizeForTest(
            image, maximumPayloadBytes - minimumPayloadBytes);
    const std::unique_ptr<ImageFrame> overBoundaryFirstFrame
        = makeImageFrameWithPayloadByteSizeForTest(
            image, maximumPayloadBytes - minimumPayloadBytes + 1);
    const std::unique_ptr<ImageFrame> minimumFrame
        = makeImageFrameWithPayloadByteSizeForTest(image, minimumPayloadBytes);

    TimedImageFrameList list;
    QCOMPARE(list.appendFrame(exactBoundaryFirstFrame.get(), 100), true);
    QCOMPARE(list.appendFrame(minimumFrame.get(), 100), true);
    QCOMPARE(list.count(), 2);
    QCOMPARE(list.errorString(), QString());

    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Created);
    QCOMPARE(result->reason(), ImageSequenceFactoryReason::NoError);

    list.clear();
    QCOMPARE(list.appendFrame(overBoundaryFirstFrame.get(), 100), true);
    QSignalSpy countSpy(&list, &TimedImageFrameList::countChanged);
    QCOMPARE(list.appendFrame(minimumFrame.get(), 100), false);
    QCOMPARE(list.count(), 1);
    QCOMPARE(countSpy.count(), 0);
    QVERIFY(!list.errorString().isEmpty());

    list.clear();
    QCOMPARE(list.count(), 0);
    QCOMPARE(list.errorString(), QString());
    QCOMPARE(list.appendFrame(exactBoundaryFirstFrame.get(), 100), true);
    QCOMPARE(list.appendFrame(minimumFrame.get(), 100), true);
    QCOMPARE(list.count(), 2);

    TimedImageFrameList qImageConvenienceList;
    const std::unique_ptr<ImageFrame> nearlyFullFrame = makeImageFrameWithPayloadByteSizeForTest(
        image, maximumPayloadBytes - minimumPayloadBytes + 1);
    QCOMPARE(qImageConvenienceList.appendFrame(nearlyFullFrame.get(), 100), true);
    ImageViewportInternal::ImageFramePrivateAccess::resetPayloadCopyAttemptCountForTest();
    QCOMPARE(qImageConvenienceList.appendFrame(image, 100), false);
    QCOMPARE(qImageConvenienceList.count(), 1);
    QCOMPARE(ImageViewportInternal::ImageFramePrivateAccess::payloadCopyAttemptCountForTest(), 0);
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
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
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
        ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), acceptedFrameSeekRevision);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 349).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 349);
    QCOMPARE(primaryDisplayedPosition(item), 100);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 350).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedPosition(item), 100);

    const ImageViewportRevisionToken acceptedPositionSeekRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 351).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), acceptedPositionSeekRevision);
    QCOMPARE(primaryRequestedPosition(item), 350);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportTimedTest::timedFrameListRequireExactRejectsNewInexactFrame()
{
    QImage exactImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    exactImage.fill(Qt::transparent);
    ImageFrame exactFrame(exactImage);
    QImage previewImage(8, 4, QImage::Format_ARGB32_Premultiplied);
    previewImage.fill(Qt::black);
    ImageFrame previewFrame(previewImage, QSizeF(16.0, 8.0), previewImage.sizeInBytes(),
        ImageViewportPayloadQuality::Preview, ImageViewportPayloadExactness::NotExact,
        ImageFrame::OrientationPolicy::Identity, QStringLiteral("preview/argb32"));
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&exactFrame, 100));
    QVERIFY(list.appendFrame(&previewFrame, 250));
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);

    ImageViewportPresentationCommand requireExact;
    requireExact.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);
    QCOMPARE(item.setPresentation(requireExact).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(item.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(item.state().primary().request().frame(), 1);
    QCOMPARE(item.state().primary().display().frame(), 0);
    QCOMPARE(item.state().primary().display().exactness(),
        ImageViewportPayloadExactness::ExactForSource);

    ImageViewport playbackItem;
    playbackItem.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(playbackItem
                 .setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(playbackItem);
    QCOMPARE(playbackItem.setPresentation(requireExact).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackItem.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(playbackItem, 100);
    QCOMPARE(playbackItem.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(playbackItem.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(playbackItem.state().primary().request().playbackPhase(),
        ImageViewportPlaybackPhase::Stopped);
    QCOMPARE(playbackItem.state().display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(playbackItem.state().primary().display().frame(), 0);
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
        ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Playing"));

    advancePlaybackForTest(item, 100, ImageViewportPageRole::Secondary);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedPosition(item), 100);

    QCOMPARE(item.pause(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Paused"));
}

void ImageViewportTimedTest::timedFrameListRolesPlayAndPauseIndependently()
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
    QVERIFY(secondaryList.appendFrame(&firstFrame, 120));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 230));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Playing);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Secondary),
        ImageViewportPlaybackPhase::Stopped);

    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Playing);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Secondary),
        ImageViewportPlaybackPhase::Playing);

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Paused);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Secondary),
        ImageViewportPlaybackPhase::Playing);
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

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 100.0));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
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

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(
        item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);

    QCOMPARE(item.stop(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 100, ImageViewportPageRole::Secondary);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(secondaryRequestedPosition(item), 100);
    QCOMPARE(secondaryDisplayedPosition(item), 100);

    QCOMPARE(
        item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(secondaryRequestedFrame(item), 1);
    QCOMPARE(secondaryDisplayedFrame(item), 1);

    QCOMPARE(item.stop(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item, ImageViewportPageRole::Secondary),
        enumValue(metaObject, "PlaybackPhase", "Stopped"));
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
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.seek(ImageViewportPageRole::Secondary, -1).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(item.seek(ImageViewportPageRole::Secondary, 2).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Secondary, 351).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Secondary, -1).outcome(),
        ImageViewportCommandOutcome::Invalid);
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

void ImageViewportTimedTest::timedFrameListAuthoredAutoplaySelectsEligibleRole_data()
{
    QTest::addColumn<bool>("primaryAutoplay");
    QTest::addColumn<int>("expectedRole");

    QTest::newRow("primary-precedes-secondary")
        << true << static_cast<int>(ImageViewportPageRole::Primary);
    QTest::newRow("secondary-selected-when-primary-ineligible")
        << false << static_cast<int>(ImageViewportPageRole::Secondary);
}

void ImageViewportTimedTest::timedFrameListAuthoredAutoplaySelectsEligibleRole()
{
    QFETCH(bool, primaryAutoplay);
    QFETCH(int, expectedRole);

    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);

    TimedImageFrameList primaryList;
    QVERIFY(primaryList.appendFrame(&firstFrame, 100));
    QVERIFY(primaryList.appendFrame(&secondFrame, 100));
    primaryList.setAutoplay(primaryAutoplay);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(
        factory.fromTimedFrameList(&primaryList));
    QVERIFY(primaryResult->sequence());

    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 100));
    secondaryList.setAutoplay(true);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(
        (expectedRole == static_cast<int>(ImageViewportPageRole::Primary) ? snapshot.primary()
                                                                          : snapshot.secondary())
            .request()
            .playbackPhase(),
        ImageViewportPlaybackPhase::Waiting);

    acknowledgePendingRenderCommitForTest(item);
    snapshot = item.state();
    QCOMPARE(
        (expectedRole == static_cast<int>(ImageViewportPageRole::Primary) ? snapshot.primary()
                                                                          : snapshot.secondary())
            .request()
            .playbackPhase(),
        ImageViewportPlaybackPhase::Playing);
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

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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

void ImageViewportTimedTest::timedFrameListSupersededWrapDoesNotConsumeFiniteIteration()
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

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 200);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Playing);

    advancePlaybackForTest(item, 200);

    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);
}

void ImageViewportTimedTest::timedFrameListSiblingRequestRebindsPendingFiniteWrap()
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
    QVERIFY(primaryList.appendFrame(&secondFrame, 100));
    primaryList.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(2));
    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 100));
    secondaryList.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(2));
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromTimedFrameList(&primaryList));
    QScopedPointer<ImageSequenceFactoryResult> secondary(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(primary->sequence());
    QVERIFY(secondary->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(primary->sequence(), secondary->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 200, ImageViewportPageRole::Primary);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);

    QCOMPARE(item.seek(ImageViewportPageRole::Secondary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 1);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Playing);

    advancePlaybackForTest(item, 200, ImageViewportPageRole::Primary);

    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);
}

void ImageViewportTimedTest::timedFrameListSiblingStopPreservesPendingFiniteWrap()
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
    QVERIFY(primaryList.appendFrame(&secondFrame, 100));
    primaryList.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(2));
    TimedImageFrameList secondaryList;
    QVERIFY(secondaryList.appendFrame(&firstFrame, 100));
    QVERIFY(secondaryList.appendFrame(&secondFrame, 100));
    secondaryList.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(2));
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromTimedFrameList(&primaryList));
    QScopedPointer<ImageSequenceFactoryResult> secondary(
        factory.fromTimedFrameList(&secondaryList));
    QVERIFY(primary->sequence());
    QVERIFY(secondary->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(primary->sequence(), secondary->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 200, ImageViewportPageRole::Secondary);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Secondary),
        ImageViewportPlaybackPhase::Playing);

    advancePlaybackForTest(item, 100, ImageViewportPageRole::Primary);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryDisplayedFrame(item), 1);

    advancePlaybackForTest(item, 100, ImageViewportPageRole::Primary);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);

    QCOMPARE(item.stop(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QVERIFY(hasPendingRenderCommitForTest(item));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Playing);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Secondary),
        ImageViewportPlaybackPhase::Stopped);

    advancePlaybackForTest(item, 200, ImageViewportPageRole::Primary);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Stopped);
}

void ImageViewportTimedTest::timedFrameListSameTargetRefinementRebindsPendingFiniteWrap()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList originalList;
    QVERIFY(originalList.appendFrame(&firstFrame, 100));
    QVERIFY(originalList.appendFrame(&secondFrame, 100));
    originalList.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(2));
    TimedImageFrameList refinementList;
    QVERIFY(refinementList.appendFrame(&secondFrame, 100));
    QVERIFY(refinementList.appendFrame(&firstFrame, 100));
    refinementList.setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts::finiteLoop(2));
    QScopedPointer<ImageSequenceFactoryResult> original(factory.fromTimedFrameList(&originalList));
    QScopedPointer<ImageSequenceFactoryResult> refinement(
        factory.fromTimedFrameList(&refinementList));
    QVERIFY(original->sequence());
    QVERIFY(refinement->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(ImageViewportPresentationTarget(original->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(item, 200);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);

    PresentationTargetTransitionPolicy refinementPolicy;
    refinementPolicy.setReplacementIntent(
        PresentationTargetTransitionPolicy::ReplacementIntent::SameTargetRefinement);
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(refinement->sequence()), refinementPolicy)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(primaryRequestedFrame(item), 0);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Playing);

    advancePlaybackForTest(item, 200);

    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Waiting);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(rolePlaybackPhase(item, ImageViewportPageRole::Primary),
        ImageViewportPlaybackPhase::Stopped);
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
        ImageViewportCommandOutcome::Accepted);

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
        ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(playingItem, 80);
    QCOMPARE(playingItem.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
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
        ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(pausedItem, 80);
    QCOMPARE(pausedItem.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(pausedItem.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    advancePlaybackForTest(pausedItem, 20);
    acknowledgePendingRenderCommitForTest(pausedItem);
    QCOMPARE(playbackPhaseValue(pausedItem), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(pausedItem), 1);
    QCOMPARE(primaryDisplayedFrame(pausedItem), 1);
    QCOMPARE(primaryRequestedPosition(pausedItem), 100);
    QCOMPARE(primaryDisplayedPosition(pausedItem), 100);
}

void ImageViewportTimedTest::timedFrameListSameTargetRefinementPreservesSchedulerElapsed()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::red);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::blue);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList originalList;
    QVERIFY(originalList.appendFrame(&firstFrame, 100));
    QVERIFY(originalList.appendFrame(&secondFrame, 250));
    TimedImageFrameList refinementList;
    QVERIFY(refinementList.appendFrame(&secondFrame, 100));
    QVERIFY(refinementList.appendFrame(&firstFrame, 250));
    TimedImageFrameList incompatibleList;
    QVERIFY(incompatibleList.appendFrame(&secondFrame, 100));
    QVERIFY(incompatibleList.appendFrame(&firstFrame, 251));
    QScopedPointer<ImageSequenceFactoryResult> original(factory.fromTimedFrameList(&originalList));
    QScopedPointer<ImageSequenceFactoryResult> refinement(
        factory.fromTimedFrameList(&refinementList));
    QScopedPointer<ImageSequenceFactoryResult> incompatible(
        factory.fromTimedFrameList(&incompatibleList));
    QVERIFY(original->sequence());
    QVERIFY(refinement->sequence());
    QVERIFY(incompatible->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(original->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    setPendingPlaybackSchedulerElapsedForTest(item, 80);

    PresentationTargetTransitionPolicy refinementPolicy;
    refinementPolicy.setReplacementIntent(
        PresentationTargetTransitionPolicy::ReplacementIntent::SameTargetRefinement);
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(incompatible->sequence()), refinementPolicy)
                 .outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(refinement->sequence()), refinementPolicy)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(primaryRequestedFrame(item), 0);
    acknowledgePendingRenderCommitForTest(item);

    advancePlaybackForTest(item, 19);
    QCOMPARE(primaryRequestedFrame(item), 0);
    advancePlaybackForTest(item, 1);
    QCOMPARE(primaryRequestedFrame(item), 1);
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
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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
                 item, ImageViewportBackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().backgroundMode(), ImageViewportBackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    verifyRevisionChanged(item, "commandRevision", commandRevision);
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

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    QCOMPARE(
        item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

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
        ImageViewportCommandOutcome::Accepted);
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
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

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
    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    advancePlaybackForTest(item, 100);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 0);

    QCOMPARE(
        item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
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
