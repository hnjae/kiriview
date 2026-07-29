// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

#include <cmath>
#include <limits>

class ImageViewportPresentationStateTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportPresentationStateTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private Q_SLOTS:
    void invalidPresentationEnumCommandsRejectWithoutDisplayMutation();
    void failureTransitionPolicyValidity();
    void restorePreviousRejectsWithoutCommittedPresentation();
    void restorePreviousRestoresCompleteCommittedPresentation();
    void presentationChangesWithoutDisplayKeepEmptyGeometry();
    void viewportGeometryWithoutTargetIsRevisionNeutral();
    void backgroundPresentationDoesNotChangeRequestOrPlayback();
    void qualityPresentationDoesNotChangeRequestGeometryOrPlayback();
    void loopingDoesNotChangeRequestDisplayOrGeometry();
    void presentationChangesUpdateDisplayRevision();
    void twoPageSpreadGeometryUsesDirectionAndGap();
    void twoPageManualPanUsesSpreadGeometry();
    void rotatedTwoPageManualPanUsesSpreadGeometry();
    void twoPageNonPositiveItemGeometrySuppressesPresentableRects();
    void retainedTwoPageGeometryUsesDisplayedSecondarySize();
    void retainedPrimaryOnlyReplacementKeepsSecondaryDisplayedObservations();
    void spreadCoordinateHelpersRejectGapAndEdges();
    void logicalCoordinateMappingsUseFullDisplayedDomains();
    void fitModesExposeZoomAndPannability();
    void targetTransitionPreserveKeepsStoredManualZoomDemand();
    void manualZoomCommandUsesItemCenterAnchor();
    void resizeClampsCanonicalContentPosition();
    void revisionTokensUseSharedNonWrappingAllocator();
    void invalidPresentationTargetTransitionPreservesStateAndRevisions();
    void presentationCommandsUpdateCommandDiagnostics();
    void manualZoomMaximumTracksAvailabilityAcrossDisplayStates();
    void manualZoomMaximumTracksVirtualSpreadAndDisplayCaps();
    void maximumDecreaseClampsZoomAndContentAtomically();
    void manualZoomAbovePublishedLimitIsInvalid();
    void presentationCommandZoomStepDeltaUsesSharedSetZoomPath();
    void explicitZoomAnchorPreservesSpreadPoint();
    void invalidZoomAnchorsDoNotPartiallyMutatePresentation();
    void geometryToleranceIsSharedByPanAndContentAnchor();
    void mirrorPresentationCommandsPreserveItemCenterAnchor();
    void rotationAffectsSpreadMapping();
};

static ImageViewportCommandOutcome setRotationDegrees(ImageViewport& item, int degrees)
{
    ImageViewportPresentationCommand command;
    command.setRotationDegrees(degrees);
    return item.setPresentation(command).outcome();
}

void ImageViewportPresentationStateTest::failureTransitionPolicyValidity()
{
    PresentationTargetTransitionPolicy keepFailed;
    QCOMPARE(keepFailed.failureTransition(),
        PresentationTargetTransitionPolicy::FailureTransition::KeepFailedTarget);
    QVERIFY(keepFailed.isValid());

    PresentationTargetTransitionPolicy restore;
    restore.setFailureTransition(
        PresentationTargetTransitionPolicy::FailureTransition::RestorePrevious);
    QCOMPARE(restore.failureTransition(),
        PresentationTargetTransitionPolicy::FailureTransition::RestorePrevious);
    QVERIFY(restore.isValid());

    restore.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    QVERIFY(!restore.isValid());

    restore.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::RetainPrevious);
    restore.setReplacementIntent(
        PresentationTargetTransitionPolicy::ReplacementIntent::SameTargetRefinement);
    QVERIFY(!restore.isValid());
}

void ImageViewportPresentationStateTest::restorePreviousRejectsWithoutCommittedPresentation()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    const ImageViewportStateSnapshot before = item.state();

    PresentationTargetTransitionPolicy restore;
    restore.setFailureTransition(
        PresentationTargetTransitionPolicy::FailureTransition::RestorePrevious);
    QCOMPARE(
        item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), restore)
            .outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().request(), before.request());
    QCOMPARE(item.state().display(), before.display());
    QCOMPARE(item.state().presentation(), before.presentation());
}

void ImageViewportPresentationStateTest::restorePreviousRestoresCompleteCommittedPresentation()
{
    ImageSequenceFactory factory;
    QImage image(40, 20, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> first(factory.fromFrame(&frame));
    QVERIFY(first->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter replacementAdapter(
        sessionFactory, ImageSequenceProviderMetadata::still(QSizeF(20.0, 40.0)));
    QScopedPointer<ImageSequenceFactoryResult> replacement(
        factory.fromProvider(&replacementAdapter));
    QVERIFY(replacement->sequence());

    ImageViewport item;
    useSynchronousProviderExecutorForTest(item);
    useSynchronousProviderEventDeliveryForTest(item);
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(first->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    ImageViewportPresentationCommand manualZoom;
    manualZoom.setPreferredManualZoomPercent(200.0);
    QCOMPARE(item.setPresentation(manualZoom).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewportCommandOutcome::Accepted);
    ImageViewportPresentationCommand endAnchor;
    endAnchor.setContentAnchor(ImageViewportContentAnchor::End);
    QCOMPARE(item.setPresentation(endAnchor).outcome(), ImageViewportCommandOutcome::Accepted);
    const ImageViewportStateSnapshot committed = item.state();

    PresentationTargetTransitionPolicy restore;
    restore.setFailureTransition(
        PresentationTargetTransitionPolicy::FailureTransition::RestorePrevious);
    restore.setRotationTransition(PresentationTargetTransitionPolicy::RotationTransition::Reset);
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(replacement->sequence()), restore)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().request().status(), ImageViewportRequestStatus::Loading);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(item.state().presentation().rotationDegrees(), 0);

    discardRetainedDisplayForResourcePressureForTest(item);
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Retained);

    QVERIFY(sessionFactory->lastSession());
    const auto failureReleaseCount = std::make_shared<int>(0);
    auto* failureHandle = new ImageSequenceProviderFailureHandle(
        [failureReleaseCount]() { ++*failureReleaseCount; });
    const ImageSequenceProviderFailureReference failureReference = failureHandle->reference();
    Q_EMIT sessionFactory->lastSession()->providerEvent(
        ImageSequenceProviderEvent::failed(sessionFactory->lastSession()->lastFrameToken(),
            ImageSequenceProviderFailure(
                ImageSequenceProviderFailureCause::ProviderInternal, failureHandle)));

    QCOMPARE(item.state().request(), committed.request());
    QCOMPARE(item.state().display().status(), ImageViewportDisplayStatus::Ready);
    QCOMPARE(item.state().display().displayedRoleSet(), committed.display().displayedRoleSet());
    QCOMPARE(item.state().presentation(), committed.presentation());
    QCOMPARE(viewportPrimarySequence(item), first->sequence());
    QCOMPARE(*closeCount, 1);
    const ImageViewportFailureSnapshot failure = item.state().diagnostics().failure();
    QVERIFY(failure.available());
    QCOMPARE(failure.context(), ImageViewportFailureContext::RestoredTransition);
    QCOMPARE(failure.reason(), ImageViewportRequestReason::ProviderFailure);
    QCOMPARE(failure.providerCause(), ImageSequenceProviderFailureCause::ProviderInternal);
    QCOMPARE(failure.providerReference(), failureReference);
    QCOMPARE(*failureReleaseCount, 0);

    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(*failureReleaseCount, 1);
}

static ImageViewportCommandOutcome setMirrorHorizontallyCommand(ImageViewport& item, bool mirror)
{
    ImageViewportPresentationCommand command;
    command.setMirrorHorizontally(mirror);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setMirrorVerticallyCommand(ImageViewport& item, bool mirror)
{
    ImageViewportPresentationCommand command;
    command.setMirrorVertically(mirror);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setContentAnchor(
    ImageViewport& item, ImageViewportContentAnchor direction)
{
    ImageViewportPresentationCommand command;
    command.setContentAnchor(direction);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setPanDelta(ImageViewport& item, QPointF delta)
{
    ImageViewportPresentationCommand command;
    command.setPanDelta(delta);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setSpreadDirectionCommand(
    ImageViewport& item, ImageViewportSpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setPageGapCommand(ImageViewport& item, double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setFitModeCommand(ImageViewport& item, ImageViewportFitMode mode)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(mode);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setPreferredManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setPreferredManualZoomPercent(percent);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome activateManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(ImageViewportFitMode::Manual);
    command.setPreferredManualZoomPercent(percent);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setQualityTogglesCommand(
    ImageViewport& item, bool smoothing, bool mipmap)
{
    ImageViewportPresentationCommand command;
    command.setSmoothing(smoothing);
    command.setMipmap(mipmap);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setBackgroundCommand(
    ImageViewport& item, ImageViewportBackgroundMode mode, QColor color)
{
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(mode);
    command.setBackgroundColor(color);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setLoopingCommand(ImageViewport& item, bool looping)
{
    ImageViewportPresentationCommand command;
    command.setLooping(looping);
    return item.setPresentation(command).outcome();
}

void ImageViewportPresentationStateTest::
    invalidPresentationEnumCommandsRejectWithoutDisplayMutation()
{
    ImageViewport item;
    const ImageViewportRevisionToken initialDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken initialCommandRevision
        = revisionTokenProperty(item, "commandRevision");

    ImageViewportPresentationCommand command;
    command.setBackgroundMode(static_cast<ImageViewportBackgroundMode>(
        999)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewportBackgroundMode::Transparent);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), initialDisplayRevision);
    verifyRevisionChanged(item, "commandRevision", initialCommandRevision);
    QCOMPARE(
        commandReasonValue(item), enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
}

void ImageViewportPresentationStateTest::presentationChangesWithoutDisplayKeepEmptyGeometry()
{
    ImageViewport item;
    const ImageViewportRevisionToken initialDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(
        activateManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Unsupported);
    QCOMPARE(setFitModeCommand(item, ImageViewportFitMode::FitHeight),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setMirrorVerticallyCommand(item, true), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(item, ImageViewportBackgroundMode::SolidColor, Qt::red),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleImageRect(item), QRectF());
    verifyRevisionChanged(item, "displayRevision", initialDisplayRevision);
    QCOMPARE(stateSpy.count(), 6);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleImageRect(item), QRectF());
}

void ImageViewportPresentationStateTest::viewportGeometryWithoutTargetIsRevisionNeutral()
{
    ImageViewport item;
    const ImageViewportStateSnapshot before = item.state();
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(item.state(), before);
    QCOMPARE(stateSpy.count(), 0);
}

void ImageViewportPresentationStateTest::backgroundPresentationDoesNotChangeRequestOrPlayback()
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

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const QRectF preservedContentRect = contentRect(item);
    const QRectF preservedVisibleImageRect = visibleImageRect(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(setBackgroundCommand(
                 item, ImageViewportBackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(contentRect(item), preservedContentRect);
    QCOMPARE(visibleImageRect(item), preservedVisibleImageRect);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportPresentationStateTest::qualityPresentationDoesNotChangeRequestGeometryOrPlayback()
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

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const QRectF preservedContentRect = contentRect(item);
    const QRectF preservedVisibleImageRect = visibleImageRect(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(contentRect(item), preservedContentRect);
    QCOMPARE(visibleImageRect(item), preservedVisibleImageRect);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportPresentationStateTest::loopingDoesNotChangeRequestDisplayOrGeometry()
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

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    const QRectF preservedContentRect = contentRect(item);
    const QRectF preservedVisibleImageRect = visibleImageRect(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(setLoopingCommand(item, true), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().looping(), true);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(contentRect(item), preservedContentRect);
    QCOMPARE(visibleImageRect(item), preservedVisibleImageRect);
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportPresentationStateTest::presentationChangesUpdateDisplayRevision()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(
        setPreferredManualZoomPercentCommand(item, 1000.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(stateSpy.count(), 1);

    QCOMPARE(setFitModeCommand(item, ImageViewportFitMode::Manual),
        ImageViewportCommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(stateSpy.count(), 2);

    QCOMPARE(setPanDelta(item, QPointF(4.0, 0.0)), ImageViewportCommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(stateSpy.count(), 3);

    QCOMPARE(setFitModeCommand(item, ImageViewportFitMode::FitHeight),
        ImageViewportCommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(stateSpy.count(), 4);

    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewportCommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(stateSpy.count(), 5);
}

void ImageViewportPresentationStateTest::twoPageSpreadGeometryUsesDirectionAndGap()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(displayedSpreadSize(item), QSizeF(40.0, 20.0));
    QCOMPARE(primaryDisplayedImageSize(item), QSizeF(10.0, 20.0));
    QCOMPARE(secondaryDisplayedImageSize(item), QSizeF(30.0, 20.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 0.0, 88.0, 44.0));
    QCOMPARE(primaryPageRect(item), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryPageRect(item), QRectF(10.0, 0.0, 30.0, 20.0));
    QCOMPARE(primaryItemRect(item), QRectF(0.0, 0.0, 22.0, 44.0));
    QCOMPARE(secondaryItemRect(item), QRectF(22.0, 0.0, 66.0, 44.0));
    QCOMPARE(visibleSpreadRect(item), QRectF(0.0, 0.0, 40.0, 20.0));
    QCOMPARE(visiblePrimaryPageRect(item), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(visibleSecondaryPageRect(item), QRectF(0.0, 0.0, 30.0, 20.0));

    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(displayedSpreadSize(item), QSizeF(44.0, 20.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 2.0, 88.0, 40.0));
    QCOMPARE(primaryPageRect(item), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryPageRect(item), QRectF(14.0, 0.0, 30.0, 20.0));
    QCOMPARE(primaryItemRect(item), QRectF(0.0, 2.0, 20.0, 40.0));
    QCOMPARE(secondaryItemRect(item), QRectF(28.0, 2.0, 60.0, 40.0));

    QCOMPARE(setSpreadDirectionCommand(item, ImageViewportSpreadDirection::RightToLeft),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(primaryPageRect(item), QRectF(34.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryPageRect(item), QRectF(0.0, 0.0, 30.0, 20.0));
    QCOMPARE(primaryItemRect(item), QRectF(68.0, 2.0, 20.0, 40.0));
    QCOMPARE(secondaryItemRect(item), QRectF(0.0, 2.0, 60.0, 40.0));
}

void ImageViewportPresentationStateTest::twoPageManualPanUsesSpreadGeometry()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(20.0, 20.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(activateManualZoomPercentCommand(item, 100.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(-12.0, 0.0, 44.0, 20.0));
    QCOMPARE(contentPosition(item), QPointF(12.0, 0.0));
    QCOMPARE(maximumContentPosition(item), QPointF(24.0, 0.0));

    QCOMPARE(setContentAnchor(item, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(contentRect(item), QRectF(-24.0, 0.0, 44.0, 20.0));
    QCOMPARE(contentPosition(item), QPointF(24.0, 0.0));
    QCOMPARE(visibleSpreadRect(item), QRectF(24.0, 0.0, 20.0, 20.0));
    QCOMPARE(visiblePrimaryPageRect(item), QRectF());
    QCOMPARE(visibleSecondaryPageRect(item), QRectF(10.0, 0.0, 20.0, 20.0));
}

void ImageViewportPresentationStateTest::rotatedTwoPageManualPanUsesSpreadGeometry()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(20.0, 20.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewportSpreadDirection::RightToLeft),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(activateManualZoomPercentCommand(item, 100.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().rotationDegrees(), 90);
    QCOMPARE(contentRect(item), QRectF(0.0, -12.0, 20.0, 44.0));
    QCOMPARE(contentPosition(item), QPointF(0.0, 12.0));
    QCOMPARE(maximumContentPosition(item), QPointF(0.0, 24.0));

    QCOMPARE(setContentAnchor(item, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);

    QCOMPARE(contentRect(item), QRectF(0.0, -24.0, 20.0, 44.0));
    QCOMPARE(contentPosition(item), QPointF(0.0, 24.0));
    QCOMPARE(primaryPageRect(item), QRectF(34.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryPageRect(item), QRectF(0.0, 0.0, 30.0, 20.0));
    QCOMPARE(primaryItemRect(item), QRectF(0.0, 10.0, 20.0, 10.0));
    QCOMPARE(secondaryItemRect(item), QRectF(0.0, -24.0, 20.0, 30.0));
}

void ImageViewportPresentationStateTest::twoPageNonPositiveItemGeometrySuppressesPresentableRects()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    item.setSize(QSizeF(0.0, 44.0));

    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedSpreadSize(item), QSizeF(0.0, 0.0));
    QCOMPARE(primaryDisplayedImageSize(item), QSizeF(10.0, 20.0));
    QCOMPARE(secondaryDisplayedImageSize(item), QSizeF(30.0, 20.0));
    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleSpreadRect(item), QRectF());
    QCOMPARE(visiblePrimaryPageRect(item), QRectF());
    QCOMPARE(visibleSecondaryPageRect(item), QRectF());
    QCOMPARE(primaryItemRect(item), QRectF());
    QCOMPARE(secondaryItemRect(item), QRectF());
    QCOMPARE(mapItemToSpread(item, 0.0, 0.0).isValid(), false);
    verifyInvalidCoordinateResult(mapSpreadToPage(item, ImageViewportPageRole::Primary, 0.0, 0.0));
}

void ImageViewportPresentationStateTest::retainedTwoPageGeometryUsesDisplayedSecondarySize()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(displayedSpreadSize(item), QSizeF(44.0, 20.0));
    QCOMPARE(secondaryItemRect(item), QRectF(28.0, 2.0, 60.0, 40.0));

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> loadingResult(factory.fromProvider(&adapter));
    QVERIFY(loadingResult->sequence());

    PresentationTargetTransitionPolicy retainedReplacement;
    retainedReplacement.setSpreadDirectionTransition(
        PresentationTargetTransitionPolicy::SpreadDirectionTransition::SetExplicit);
    retainedReplacement.setSpreadDirection(ImageViewportSpreadDirection::RightToLeft);
    retainedReplacement.setPageGapTransition(
        PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    retainedReplacement.setPageGap(0.0);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
                     retainedReplacement)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(
        item.state().presentation().spreadDirection(), ImageViewportSpreadDirection::RightToLeft);
    QCOMPARE(item.state().presentation().pageGap(), 0.0);
    QCOMPARE(displayedSpreadSize(item), QSizeF(44.0, 20.0));
    QCOMPARE(primaryDisplayedImageSize(item), QSizeF(10.0, 20.0));
    QCOMPARE(secondaryDisplayedImageSize(item), QSizeF(30.0, 20.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 2.0, 88.0, 40.0));
    QCOMPARE(primaryItemRect(item), QRectF(0.0, 2.0, 20.0, 40.0));
    QCOMPARE(secondaryItemRect(item), QRectF(28.0, 2.0, 60.0, 40.0));

    const ImageViewportCoordinateResult retainedSecondaryOrigin
        = mapPageToSpread(item, ImageViewportPageRole::Secondary, 0.0, 0.0);
    QCOMPARE(retainedSecondaryOrigin.isValid(), true);
    QCOMPARE(retainedSecondaryOrigin.point(), QPointF(14.0, 0.0));
    QCOMPARE(retainedSecondaryOrigin.space(), ImageViewportCoordinateSpace::DisplayedSpread);
    QCOMPARE(retainedSecondaryOrigin.role().value<ImageViewportPageRole>(),
        ImageViewportPageRole::Secondary);
    verifyInvalidCoordinateResult(
        mapPageToSpread(item, ImageViewportPageRole::Secondary, 30.0, 0.0));
}

void ImageViewportPresentationStateTest::
    retainedPrimaryOnlyReplacementKeepsSecondaryDisplayedObservations()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> loadingResult(factory.fromProvider(&adapter));
    QVERIFY(loadingResult->sequence());

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(secondaryRequestedFrame(item), -1);
    QCOMPARE(secondaryRequestedPosition(item), -1);
    QCOMPARE(secondaryDisplayedFrame(item), 0);
    QCOMPARE(secondaryDisplayedPosition(item), -1);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(secondaryDisplayedImageSize(item), QSizeF(30.0, 20.0));
    QCOMPARE(secondaryPageRect(item), QRectF(14.0, 0.0, 30.0, 20.0));
    QCOMPARE(secondaryItemRect(item), QRectF(28.0, 2.0, 60.0, 40.0));
    QCOMPARE(visibleSecondaryPageRect(item), QRectF(0.0, 0.0, 30.0, 20.0));

    PresentationTargetTransitionPolicy clearBeforeLoad;
    clearBeforeLoad.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(loadingResult->sequence()), clearBeforeLoad)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(secondaryDisplayedFrame(item), -1);
    QCOMPARE(secondaryDisplayedPosition(item), -1);
    QCOMPARE(secondaryDisplayedImageSize(item), QSizeF());
    QCOMPARE(secondaryItemRect(item), QRectF());
}

void ImageViewportPresentationStateTest::spreadCoordinateHelpersRejectGapAndEdges()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(88.0, 44.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);

    const ImageViewportCoordinateResult gapSpreadPoint = mapItemToSpread(item, 22.0, 22.0);
    QCOMPARE(gapSpreadPoint.isValid(), true);
    QCOMPARE(gapSpreadPoint.point().x(), 11.0);
    QCOMPARE(gapSpreadPoint.point().y(), 10.0);
    QCOMPARE(mapItemToPage(item, ImageViewportPageRole::Primary, 22.0, 22.0).isValid(), false);
    QCOMPARE(mapItemToPage(item, ImageViewportPageRole::Secondary, 22.0, 22.0).isValid(), false);

    const ImageViewportCoordinateResult primaryPoint
        = mapItemToPage(item, ImageViewportPageRole::Primary, 19.0, 22.0);
    QCOMPARE(primaryPoint.isValid(), true);
    QCOMPARE(primaryPoint.point().x(), 9.5);
    QCOMPARE(primaryPoint.point().y(), 10.0);
    QCOMPARE(mapItemToPage(item, ImageViewportPageRole::Primary, 20.0, 22.0).isValid(), false);

    const ImageViewportCoordinateResult secondaryOrigin
        = mapPageToItem(item, ImageViewportPageRole::Secondary, 0.0, 0.0);
    QCOMPARE(secondaryOrigin.isValid(), true);
    QCOMPARE(secondaryOrigin.point().x(), 28.0);
    QCOMPARE(secondaryOrigin.point().y(), 2.0);
    QCOMPARE(mapPageToItem(item, ImageViewportPageRole::Secondary, 30.0, 0.0).isValid(), false);
}

void ImageViewportPresentationStateTest::logicalCoordinateMappingsUseFullDisplayedDomains()
{
    ImageSequenceFactory factory;
    QImage primaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    QImage secondaryImage(30, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(20.0, 20.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(activateManualZoomPercentCommand(item, 100.0), ImageViewportCommandOutcome::Accepted);

    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    const ImageViewportCoordinateResult offscreenSecondary
        = mapPageToSpread(item, ImageViewportPageRole::Secondary, 20.0, 10.0);
    QCOMPARE(offscreenSecondary.isValid(), true);
    QCOMPARE(offscreenSecondary.point(), QPointF(34.0, 10.0));
    QCOMPARE(offscreenSecondary.space(), ImageViewportCoordinateSpace::DisplayedSpread);
    verifyInvalidCoordinateResult(
        mapPageToItem(item, ImageViewportPageRole::Secondary, 20.0, 10.0));

    const ImageViewportCoordinateResult secondaryLocal
        = mapSpreadToPage(item, ImageViewportPageRole::Secondary, 34.0, 10.0);
    QCOMPARE(secondaryLocal.isValid(), true);
    QCOMPARE(secondaryLocal.point(), QPointF(20.0, 10.0));
    QCOMPARE(secondaryLocal.space(), ImageViewportCoordinateSpace::DisplayedPage);

    verifyInvalidCoordinateResult(
        mapSpreadToPage(item, ImageViewportPageRole::Primary, 11.0, 10.0));
    verifyInvalidCoordinateResult(
        mapSpreadToPage(item, ImageViewportPageRole::Secondary, 44.0, 10.0));
    verifyInvalidCoordinateResult(
        mapPageToSpread(item, ImageViewportPageRole::Secondary, 30.0, 10.0));

    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportCommandRevision(item), commandRevision);
    QCOMPARE(stateSpy.count(), 0);
}

void ImageViewportPresentationStateTest::fitModesExposeZoomAndPannability()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(80.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(contentRect(item), QRectF(0.0, 30.0, 80.0, 40.0));
    QCOMPARE(item.state().presentation().zoomPercent(), 500.0);
    QCOMPARE(horizontalPannable(item), false);
    QCOMPARE(verticalPannable(item), false);

    QCOMPARE(setFitModeCommand(item, ImageViewportFitMode::FitHeight),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(-60.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.state().presentation().zoomPercent(), 1250.0);
    QCOMPARE(contentPosition(item), QPointF(60.0, 0.0));
    QCOMPARE(maximumContentPosition(item), QPointF(120.0, 0.0));
    QCOMPARE(horizontalPannable(item), true);
    QCOMPARE(verticalPannable(item), false);

    QCOMPARE(setContentAnchor(item, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(-120.0, 0.0, 200.0, 100.0));
    QCOMPARE(contentPosition(item), QPointF(120.0, 0.0));

    QCOMPARE(setPanDelta(item, QPointF(-500.0, 0.0)), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(contentPosition(item), QPointF(0.0, 0.0));

    QCOMPARE(
        setPreferredManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::FitHeight);
    QCOMPARE(item.state().presentation().preferredManualZoomPercent(), 200.0);
    QCOMPARE(item.state().presentation().zoomPercent(), 1250.0);
    QCOMPARE(setFitModeCommand(item, ImageViewportFitMode::Manual),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(24.0, 42.0, 32.0, 16.0));
    QCOMPARE(item.state().presentation().zoomPercent(), 200.0);
}

void ImageViewportPresentationStateTest::targetTransitionPreserveKeepsStoredManualZoomDemand()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame firstFrame(image);
    ImageFrame replacementFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> preserveResult(factory.fromFrame(&replacementFrame));
    QVERIFY(firstResult->sequence());
    QVERIFY(preserveResult->sequence());

    auto configureItem = [&](ImageViewport& item) {
        item.setSize(QSizeF(80.0, 100.0));
        item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
            PresentationTargetTransitionPolicy {});
        acknowledgePendingRenderCommitForTest(item);
        QCOMPARE(setPreferredManualZoomPercentCommand(item, 200.0),
            ImageViewportCommandOutcome::Accepted);
        QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Contain);
        QCOMPARE(item.state().presentation().preferredManualZoomPercent(), 200.0);
        QCOMPARE(setFitModeCommand(item, ImageViewportFitMode::FitHeight),
            ImageViewportCommandOutcome::Accepted);
        QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::FitHeight);
        QCOMPARE(item.state().presentation().zoomPercent(), 1250.0);
    };

    PresentationTargetTransitionPolicy preservePolicy;
    preservePolicy.setZoomTransition(PresentationTargetTransitionPolicy::ZoomTransition::Preserve);
    preservePolicy.setFitModeTransition(
        PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    preservePolicy.setFitMode(ImageViewportFitMode::Manual);

    ImageViewport preserveItem;
    configureItem(preserveItem);
    QCOMPARE(preserveItem
                 .setPresentationTarget(
                     ImageViewportPresentationTarget(preserveResult->sequence()), preservePolicy)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(preserveItem);
    QCOMPARE(preserveItem.state().presentation().fitMode(), ImageViewportFitMode::Manual);
    QCOMPARE(preserveItem.state().presentation().zoomPercent(), 200.0);
}

void ImageViewportPresentationStateTest::manualZoomCommandUsesItemCenterAnchor()
{
    ImageSequenceFactory factory;
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(activateManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentPosition(item), QPointF(50.0, 50.0));
    QCOMPARE(setPreferredManualZoomPercentCommand(item, std::numeric_limits<double>::infinity()),
        ImageViewportCommandOutcome::Invalid);
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(activateManualZoomPercentCommand(item, 300.0), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Manual);
    QCOMPARE(item.state().presentation().zoomPercent(), 300.0);
    QCOMPARE(contentPosition(item), QPointF(100.0, 100.0));
    QCOMPARE(contentRect(item), QRectF(-100.0, -100.0, 300.0, 300.0));
    QCOMPARE(commandReasonValue(item), enumValue(item.metaObject(), "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
}

void ImageViewportPresentationStateTest::resizeClampsCanonicalContentPosition()
{
    ImageSequenceFactory factory;
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(activateManualZoomPercentCommand(item, 300.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setPanDelta(item, QPointF(80.0, 80.0)), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentPosition(item), QPointF(180.0, 180.0));

    item.setSize(QSizeF(250.0, 250.0));
    QCOMPARE(contentPosition(item), QPointF(50.0, 50.0));

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(contentPosition(item), QPointF(50.0, 50.0));
}

void ImageViewportPresentationStateTest::revisionTokensUseSharedNonWrappingAllocator()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    const quint64 firstLargeToken = quint64(std::numeric_limits<uint>::max()) + 1;
    setNextRevisionTokenForTest(item, firstLargeToken);

    QCOMPARE(setBackgroundCommand(item, ImageViewportBackgroundMode::SolidColor, Qt::black),
        ImageViewportCommandOutcome::Accepted);
    const ImageViewportRevisionToken commandAfterPresentation = viewportCommandRevision(item);
    const ImageViewportRevisionToken displayAfterPresentation = viewportDisplayRevision(item);
    QVERIFY(commandAfterPresentation.isValid());
    QVERIFY(displayAfterPresentation.isValid());
    QCOMPARE(revisionTokenValueForTest(commandAfterPresentation), firstLargeToken);
    QVERIFY(revisionTokenValueForTest(displayAfterPresentation) > firstLargeToken);

    QCOMPARE(setPreferredManualZoomPercentCommand(item, std::numeric_limits<double>::infinity()),
        ImageViewportCommandOutcome::Invalid);
    const ImageViewportRevisionToken commandAfterInvalid = viewportCommandRevision(item);
    QVERIFY(commandAfterInvalid.isValid());
    QVERIFY(commandAfterInvalid != displayAfterPresentation);
    QVERIFY(revisionTokenValueForTest(commandAfterInvalid) > firstLargeToken);

    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const ImageViewportRevisionToken requestAfterAssignment = viewportRequestRevision(item);
    QVERIFY(requestAfterAssignment.isValid());
    QVERIFY(requestAfterAssignment != displayAfterPresentation);
    QVERIFY(requestAfterAssignment != commandAfterInvalid);
    QVERIFY(viewportDisplayRevision(item) != requestAfterAssignment);
    QVERIFY(revisionTokenValueForTest(requestAfterAssignment)
        > revisionTokenValueForTest(commandAfterInvalid));
}

void ImageViewportPresentationStateTest::
    invalidPresentationTargetTransitionPreservesStateAndRevisions()
{
    ImageSequenceFactory factory;
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QImage replacementImage(200, 100, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame frame(image);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(result->sequence());
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(activateManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setContentAnchor(item, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewportSpreadDirection::RightToLeft),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 12.0), ImageViewportCommandOutcome::Accepted);

    ImageSequence* sequence = viewportPrimarySequence(item);
    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    const auto fitMode = presentation.fitMode();
    const double zoomPercent = presentation.zoomPercent();
    const QPointF preservedContentPosition = contentPosition(item);
    const QPointF preservedMaximumContentPosition = maximumContentPosition(item);
    const QRectF preservedContentRect = contentRect(item);
    const QRectF preservedVisibleSpreadRect = visibleSpreadRect(item);
    const int rotationDegrees = presentation.rotationDegrees();
    const bool mirrorHorizontally = presentation.mirrorHorizontally();
    const auto spreadDirection = presentation.spreadDirection();
    const double pageGap = presentation.pageGap();
    const int requestStatus = requestStatusValue(item);
    const int displayStatus = displayStatusValue(item);
    const int playbackPhase = playbackPhaseValue(item);
    const QString errorString = viewportErrorString(item);
    const QString warningString = viewportWarningString(item);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setZoomTransition(
        PresentationTargetTransitionPolicy::ZoomTransition::ResetToContain);
    invalidPolicy.setFitModeTransition(
        PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    invalidPolicy.setFitMode(ImageViewportFitMode::FitHeight);

    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(replacementResult->sequence()), invalidPolicy);

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), sequence);
    const ImageViewportPresentationSnapshot afterRejectedPresentation = item.state().presentation();
    QCOMPARE(afterRejectedPresentation.fitMode(), fitMode);
    QCOMPARE(afterRejectedPresentation.zoomPercent(), zoomPercent);
    QCOMPARE(contentPosition(item), preservedContentPosition);
    QCOMPARE(maximumContentPosition(item), preservedMaximumContentPosition);
    QCOMPARE(contentRect(item), preservedContentRect);
    QCOMPARE(visibleSpreadRect(item), preservedVisibleSpreadRect);
    QCOMPARE(afterRejectedPresentation.rotationDegrees(), rotationDegrees);
    QCOMPARE(afterRejectedPresentation.mirrorHorizontally(), mirrorHorizontally);
    QCOMPARE(afterRejectedPresentation.spreadDirection(), spreadDirection);
    QCOMPARE(afterRejectedPresentation.pageGap(), pageGap);
    QCOMPARE(requestStatusValue(item), requestStatus);
    QCOMPARE(displayStatusValue(item), displayStatus);
    QCOMPARE(playbackPhaseValue(item), playbackPhase);
    QCOMPARE(viewportErrorString(item), errorString);
    QCOMPARE(viewportWarningString(item), warningString);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(
        commandReasonValue(item), enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
}

void ImageViewportPresentationStateTest::presentationCommandsUpdateCommandDiagnostics()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(80.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, -1).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const ImageViewportRevisionToken invalidSeekCommandRevision
        = revisionTokenProperty(item, "commandRevision");

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    QCOMPARE(setFitModeCommand(item, ImageViewportFitMode::FitHeight),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", invalidSeekCommandRevision);
    QCOMPARE(stateSpy.count(), 1);

    const int rotationDegrees = item.state().presentation().rotationDegrees();
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(setRotationDegrees(item, 45), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().rotationDegrees(), rotationDegrees);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", invalidSeekCommandRevision);
    QCOMPARE(stateSpy.count(), 2);
}

void ImageViewportPresentationStateTest::manualZoomMaximumTracksAvailabilityAcrossDisplayStates()
{
    constexpr double expectedReadyMaximum = 409600.0;
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> stillResult(factory.fromFrame(&frame));
    QVERIFY(stillResult->sequence());

    ImageViewport emptyItem;
    QCOMPARE(emptyItem.state().presentation().maximumManualZoomPercent(), 0.0);
    emptyItem.setSize(QSizeF(80.0, 100.0));
    QCOMPARE(emptyItem.state().presentation().maximumManualZoomPercent(), 0.0);

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> loadingResult(factory.fromProvider(&adapter));
    QVERIFY(loadingResult->sequence());

    ImageViewport loadingItem;
    loadingItem.setSize(QSizeF(80.0, 100.0));
    loadingItem.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(loadingItem.state().presentation().maximumManualZoomPercent(), 0.0);

    CountingProviderAdapter knownAdapter(
        sessionFactory, ImageSequenceProviderMetadata::still(QSizeF(16, 8)));
    QScopedPointer<ImageSequenceFactoryResult> knownLoadingResult(
        factory.fromProvider(&knownAdapter));
    QVERIFY(knownLoadingResult->sequence());
    ImageViewport knownLoadingItem;
    knownLoadingItem.setSize(QSizeF(80.0, 100.0));
    knownLoadingItem.setPresentationTarget(
        ImageViewportPresentationTarget(knownLoadingResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(
        knownLoadingItem.state().presentation().maximumManualZoomPercent(), expectedReadyMaximum);

    ImageViewport readyItem;
    readyItem.setSize(QSizeF(80.0, 100.0));
    readyItem.setPresentationTarget(ImageViewportPresentationTarget(stillResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(readyItem);
    QCOMPARE(readyItem.state().presentation().maximumManualZoomPercent(), expectedReadyMaximum);
    QCOMPARE(setPreferredManualZoomPercentCommand(readyItem, 200.0),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(readyItem.state().presentation().maximumManualZoomPercent(), expectedReadyMaximum);

    readyItem.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(readyItem.state().presentation().maximumManualZoomPercent(), 0.0);
    QCOMPARE(readyItem.state().presentation().preferredManualZoomPercent(), 200.0);
    QCOMPARE(setPreferredManualZoomPercentCommand(readyItem, 100.0),
        ImageViewportCommandOutcome::Unsupported);

    ImageViewport retainedItem;
    retainedItem.setSize(QSizeF(80.0, 100.0));
    retainedItem.setPresentationTarget(ImageViewportPresentationTarget(stillResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(retainedItem);
    retainedItem.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(displayStatusValue(retainedItem),
        enumValue(retainedItem.metaObject(), "DisplayStatus", "Retained"));
    QCOMPARE(retainedItem.state().presentation().maximumManualZoomPercent(), 0.0);
}

void ImageViewportPresentationStateTest::manualZoomMaximumTracksVirtualSpreadAndDisplayCaps()
{
    ImageSequenceFactory factory;

    QImage onePixel(1, 1, QImage::Format_ARGB32_Premultiplied);
    onePixel.fill(Qt::transparent);
    ImageFrame onePixelFrame(onePixel);
    QScopedPointer<ImageSequenceFactoryResult> onePixelResult(factory.fromFrame(&onePixelFrame));
    QVERIFY(onePixelResult->sequence());
    ImageViewport smallSource;
    smallSource.setSize(QSizeF(100, 100));
    smallSource.setPresentationTarget(ImageViewportPresentationTarget(onePixelResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(smallSource.state().presentation().maximumManualZoomPercent(), 6553600.0);
    QVERIFY(smallSource.state().presentation().maximumManualZoomPercent() > 10000.0);

    QImage preview(200, 100, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    ImageFrame largeLogicalFrame(preview, QSizeF(200000, 100000), preview.sizeInBytes(),
        ImageViewportPayloadQuality::Preview, ImageViewportPayloadExactness::NotExact,
        ImageFrame::OrientationPolicy::Identity, {});
    QScopedPointer<ImageSequenceFactoryResult> largeLogicalResult(
        factory.fromFrame(&largeLogicalFrame));
    QVERIFY(largeLogicalResult->sequence());

    ImageViewport longEdgeCapped;
    longEdgeCapped.setSize(QSizeF(1000, 500));
    longEdgeCapped.setPresentationTarget(
        ImageViewportPresentationTarget(largeLogicalResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(longEdgeCapped.state().presentation().maximumManualZoomPercent(), 32.768);
    QCOMPARE(longEdgeCapped.state().presentation().zoomPercent(), 0.5);
    QVERIFY(longEdgeCapped.state().presentation().maximumManualZoomPercent()
        >= longEdgeCapped.state().presentation().zoomPercent());

    longEdgeCapped.setSize(QSizeF(9000, 4500));
    QCOMPARE(longEdgeCapped.state().presentation().maximumManualZoomPercent(), 36.0);

    QImage pageImage(100, 100, QImage::Format_ARGB32_Premultiplied);
    pageImage.fill(Qt::transparent);
    ImageFrame primaryFrame(pageImage);
    ImageFrame secondaryFrame(pageImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport spread;
    spread.setSize(QSizeF(100, 100));
    spread.setPresentationTarget(
        ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(spread.state().presentation().maximumManualZoomPercent(), 32768.0);
    const auto beforeFinalGeometryValidation = spread.state();
    ImageViewportPresentationCommand invalidForFinalGeometry;
    invalidForFinalGeometry.setPreferredManualZoomPercent(25000.0);
    invalidForFinalGeometry.setPageGap(100.0);
    QCOMPARE(spread.setPresentation(invalidForFinalGeometry).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(spread.state().presentation(), beforeFinalGeometryValidation.presentation());
    QCOMPARE(spread.state().display(), beforeFinalGeometryValidation.display());
    QCOMPARE(
        spread.state().revisions().request(), beforeFinalGeometryValidation.revisions().request());
    QCOMPARE(
        spread.state().revisions().display(), beforeFinalGeometryValidation.revisions().display());
    QCOMPARE(setPageGapCommand(spread, 100.0), ImageViewportCommandOutcome::Accepted);
    QVERIFY(
        qAbs(spread.state().presentation().maximumManualZoomPercent() - (65536.0 * 100.0 / 300.0))
        < 0.000001);
    const double beforeRotation = spread.state().presentation().maximumManualZoomPercent();
    QCOMPARE(setRotationDegrees(spread, 90), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(spread.state().presentation().maximumManualZoomPercent(), beforeRotation);

    spread.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(spread.state().presentation().maximumManualZoomPercent(), 65536.0);
}

void ImageViewportPresentationStateTest::maximumDecreaseClampsZoomAndContentAtomically()
{
    ImageSequenceFactory factory;
    QImage onePixel(1, 1, QImage::Format_ARGB32_Premultiplied);
    onePixel.fill(Qt::transparent);
    ImageFrame onePixelFrame(onePixel);
    QScopedPointer<ImageSequenceFactoryResult> smallResult(factory.fromFrame(&onePixelFrame));
    QVERIFY(smallResult->sequence());

    QImage preview(200, 100, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    ImageFrame largeLogicalFrame(preview, QSizeF(200000, 100000), preview.sizeInBytes(),
        ImageViewportPayloadQuality::Preview, ImageViewportPayloadExactness::NotExact,
        ImageFrame::OrientationPolicy::Identity, {});
    QScopedPointer<ImageSequenceFactoryResult> largeResult(factory.fromFrame(&largeLogicalFrame));
    QVERIFY(largeResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100, 100));
    item.setPresentationTarget(ImageViewportPresentationTarget(smallResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(activateManualZoomPercentCommand(
                 item, item.state().presentation().maximumManualZoomPercent()),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setContentAnchor(item, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);
    const auto before = item.state();
    QVERIFY(before.display().contentPosition().y() > 9000.0);
    const double preferredZoom = before.presentation().preferredManualZoomPercent();

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(largeResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const auto after = item.state();

    QCOMPARE(after.presentation().maximumManualZoomPercent(), 32.768);
    QCOMPARE(after.presentation().preferredManualZoomPercent(), preferredZoom);
    QCOMPARE(after.presentation().zoomPercent(), 32.768);
    QCOMPARE(after.display().contentPosition(), after.display().maximumContentPosition());
    QVERIFY(after.display().contentPosition().y() < before.display().contentPosition().y());
    QVERIFY(after.revisions().request() != before.revisions().request());
    QVERIFY(after.revisions().display() != before.revisions().display());
    QVERIFY(after.revisions().presentation() != before.revisions().presentation());

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(smallResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const auto widened = item.state();
    QCOMPARE(widened.presentation().preferredManualZoomPercent(), preferredZoom);
    QCOMPARE(widened.presentation().zoomPercent(), preferredZoom);
}

void ImageViewportPresentationStateTest::manualZoomAbovePublishedLimitIsInvalid()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(80.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        setPreferredManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();
    const auto fitMode = item.state().presentation().fitMode();
    const double zoomPercent = item.state().presentation().zoomPercent();
    const QRectF preservedContentRect = contentRect(item);
    const QPointF preservedContentPosition = contentPosition(item);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(setPreferredManualZoomPercentCommand(
                 item, item.state().presentation().maximumManualZoomPercent() + 1.0),
        ImageViewportCommandOutcome::Invalid);

    QCOMPARE(item.state().presentation().fitMode(), fitMode);
    QCOMPARE(item.state().presentation().zoomPercent(), zoomPercent);
    QCOMPARE(contentRect(item), preservedContentRect);
    QCOMPARE(contentPosition(item), preservedContentPosition);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportPresentationStateTest::presentationCommandZoomStepDeltaUsesSharedSetZoomPath()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    const auto verifyClose = [](double actual, double expected) {
        QVERIFY2(qAbs(actual - expected) < 0.000001,
            qPrintable(QStringLiteral("actual %1 expected %2").arg(actual).arg(expected)));
    };

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Contain);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    ImageViewportPresentationCommand command;
    command.setZoomStepDelta(0);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Contain);
    verifyClose(item.state().presentation().preferredManualZoomPercent(), 100.0);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(1);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Contain);
    verifyClose(item.state().presentation().preferredManualZoomPercent(), 109.05077326652577);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(-1);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    verifyClose(item.state().presentation().preferredManualZoomPercent(), 100.0);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(0.5);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    verifyClose(item.state().presentation().preferredManualZoomPercent(), 104.42737824274138);

    command = {};
    command.setZoomStepDelta(std::numeric_limits<double>::max());
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().preferredManualZoomPercent(),
        item.state().presentation().maximumManualZoomPercent());
    QCOMPARE(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(-std::numeric_limits<double>::max());
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().preferredManualZoomPercent(),
        item.state().presentation().minimumManualZoomPercent());

    const auto beforeInvalid = item.state();
    command = {};
    command.setZoomStepDelta(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().presentation(), beforeInvalid.presentation());
    QCOMPARE(item.state().display(), beforeInvalid.display());
    QCOMPARE(item.state().revisions().request(), beforeInvalid.revisions().request());
    QCOMPARE(item.state().revisions().display(), beforeInvalid.revisions().display());

    command = {};
    command.setZoomStepDelta(std::numeric_limits<double>::infinity());
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Invalid);

    ImageViewport targetless;
    command = {};
    command.setZoomStepDelta(0.0);
    QCOMPARE(targetless.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    command = {};
    command.setZoomStepDelta(0.5);
    QCOMPARE(
        targetless.setPresentation(command).outcome(), ImageViewportCommandOutcome::Unsupported);
}

void ImageViewportPresentationStateTest::explicitZoomAnchorPreservesSpreadPoint()
{
    ImageSequenceFactory factory;
    QImage image(200, 160, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    const auto verifyPointClose = [](QPointF actual, QPointF expected) {
        QVERIFY2(qAbs(actual.x() - expected.x()) < 0.000001,
            qPrintable(
                QStringLiteral("actual x %1 expected %2").arg(actual.x()).arg(expected.x())));
        QVERIFY2(qAbs(actual.y() - expected.y()) < 0.000001,
            qPrintable(
                QStringLiteral("actual y %1 expected %2").arg(actual.y()).arg(expected.y())));
    };

    ImageViewport item;
    item.setSize(QSizeF(200, 160));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(activateManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setPanDelta(item, QPointF(40, 30)), ImageViewportCommandOutcome::Accepted);

    const QPointF anchor(80, 60);
    const ImageViewportCoordinateResult before = mapItemToSpread(item, anchor.x(), anchor.y());
    QVERIFY(before.isValid());
    ImageViewportPresentationCommand anchoredZoom;
    anchoredZoom.setPreferredManualZoomPercent(250.0);
    anchoredZoom.setZoomAnchor(anchor);
    QCOMPARE(item.setPresentation(anchoredZoom).outcome(), ImageViewportCommandOutcome::Accepted);
    const ImageViewportCoordinateResult after = mapItemToSpread(item, anchor.x(), anchor.y());
    QVERIFY(after.isValid());
    verifyPointClose(after.point(), before.point());

    ImageViewport centeredItem;
    centeredItem.setSize(QSizeF(200, 160));
    centeredItem.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(centeredItem);
    QCOMPARE(activateManualZoomPercentCommand(centeredItem, 200.0),
        ImageViewportCommandOutcome::Accepted);
    const QPointF center(100, 80);
    const auto centerBefore = mapItemToSpread(centeredItem, center.x(), center.y());
    ImageViewportPresentationCommand defaultAnchorZoom;
    defaultAnchorZoom.setPreferredManualZoomPercent(250.0);
    QCOMPARE(centeredItem.setPresentation(defaultAnchorZoom).outcome(),
        ImageViewportCommandOutcome::Accepted);
    const auto centerAfter = mapItemToSpread(centeredItem, center.x(), center.y());
    QVERIFY(centerBefore.isValid());
    QVERIFY(centerAfter.isValid());
    verifyPointClose(centerAfter.point(), centerBefore.point());

    ImageViewport fitToManual;
    fitToManual.setSize(QSizeF(200, 160));
    fitToManual.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(fitToManual);
    const QPointF fitAnchor(25, 30);
    const auto fitBefore = mapItemToSpread(fitToManual, fitAnchor.x(), fitAnchor.y());
    ImageViewportPresentationCommand atomicFitToManual;
    atomicFitToManual.setFitMode(ImageViewportFitMode::Manual);
    atomicFitToManual.setPreferredManualZoomPercent(100.0);
    atomicFitToManual.setZoomAnchor(fitAnchor);
    QCOMPARE(fitToManual.setPresentation(atomicFitToManual).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(fitToManual.state().presentation().fitMode(), ImageViewportFitMode::Manual);
    const auto fitAfter = mapItemToSpread(fitToManual, fitAnchor.x(), fitAnchor.y());
    QVERIFY(fitBefore.isValid());
    QVERIFY(fitAfter.isValid());
    verifyPointClose(fitAfter.point(), fitBefore.point());

    ImageViewport marginAnchor;
    marginAnchor.setSize(QSizeF(300, 160));
    marginAnchor.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(marginAnchor);
    ImageViewportPresentationCommand marginZoom;
    marginZoom.setFitMode(ImageViewportFitMode::Manual);
    marginZoom.setPreferredManualZoomPercent(125.0);
    marginZoom.setZoomAnchor(QPointF(0, 0));
    QCOMPARE(
        marginAnchor.setPresentation(marginZoom).outcome(), ImageViewportCommandOutcome::Accepted);
}

void ImageViewportPresentationStateTest::invalidZoomAnchorsDoNotPartiallyMutatePresentation()
{
    ImageSequenceFactory factory;
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100, 100));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    const auto verifyRejectedWithoutPartialMutation
        = [&item](const ImageViewportPresentationCommand& command,
              ImageViewportCommandOutcome expected = ImageViewportCommandOutcome::Invalid) {
              const auto before = item.state();
              QCOMPARE(item.setPresentation(command).outcome(), expected);
              const auto after = item.state();
              QCOMPARE(after.presentation(), before.presentation());
              QCOMPARE(after.display(), before.display());
              QCOMPARE(after.revisions().request(), before.revisions().request());
              QCOMPARE(after.revisions().display(), before.revisions().display());
              QCOMPARE(after.revisions().presentation(), before.revisions().presentation());
          };

    ImageViewportPresentationCommand anchorOnly;
    anchorOnly.setZoomAnchor(QPointF(50, 50));
    verifyRejectedWithoutPartialMutation(anchorOnly);

    ImageViewportPresentationCommand outOfBounds;
    outOfBounds.setPreferredManualZoomPercent(200.0);
    outOfBounds.setZoomAnchor(QPointF(100.0001, 50));
    verifyRejectedWithoutPartialMutation(outOfBounds);

    ImageViewportPresentationCommand nonFinite;
    nonFinite.setZoomStepDelta(1.0);
    nonFinite.setZoomAnchor(QPointF(std::numeric_limits<double>::quiet_NaN(), 50));
    verifyRejectedWithoutPartialMutation(nonFinite);

    ImageViewportPresentationCommand conflictingPan;
    conflictingPan.setPreferredManualZoomPercent(200.0);
    conflictingPan.setZoomAnchor(QPointF(50, 50));
    conflictingPan.setPanDelta(QPointF(1, 0));
    verifyRejectedWithoutPartialMutation(conflictingPan);

    ImageViewportPresentationCommand conflictingRotation;
    conflictingRotation.setZoomStepDelta(0.5);
    conflictingRotation.setZoomAnchor(QPointF(50, 50));
    conflictingRotation.setRotationDegrees(90);
    verifyRejectedWithoutPartialMutation(conflictingRotation);

    ImageViewportPresentationCommand conflictingFitMode;
    conflictingFitMode.setFitMode(ImageViewportFitMode::Contain);
    conflictingFitMode.setPreferredManualZoomPercent(200.0);
    conflictingFitMode.setZoomAnchor(QPointF(50, 50));
    verifyRejectedWithoutPartialMutation(conflictingFitMode);

    ImageViewport targetless;
    targetless.setSize(QSizeF(100, 100));
    ImageViewportPresentationCommand unavailable;
    unavailable.setPreferredManualZoomPercent(200.0);
    unavailable.setZoomAnchor(QPointF(50, 50));
    const auto targetlessBefore = targetless.state();
    QCOMPARE(targetless.setPresentation(unavailable).outcome(),
        ImageViewportCommandOutcome::Unsupported);
    QCOMPARE(targetless.state().presentation(), targetlessBefore.presentation());
    QCOMPARE(targetless.state().display(), targetlessBefore.display());
}

void ImageViewportPresentationStateTest::geometryToleranceIsSharedByPanAndContentAnchor()
{
    ImageSequenceFactory factory;
    QImage image(200, 160, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(200, 160));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    ImageViewportPresentationCommand withinTolerance;
    withinTolerance.setFitMode(ImageViewportFitMode::Manual);
    withinTolerance.setPreferredManualZoomPercent(100.00025);
    withinTolerance.setZoomAnchor(QPointF(100, 80));
    QCOMPARE(
        item.setPresentation(withinTolerance).outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(qAbs(item.state().display().contentSize().width() - 200.0005) < 0.0000001);
    QVERIFY(qAbs(item.state().display().contentSize().height() - 160.0004) < 0.0000001);
    QCOMPARE(item.state().display().maximumContentPosition(), QPointF());
    QCOMPARE(item.state().display().contentPosition(), QPointF());
    QCOMPARE(item.state().display().horizontalPannable(), false);
    QCOMPARE(item.state().display().verticalPannable(), false);

    QCOMPARE(setPanDelta(item, QPointF(10, 10)), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().display().contentPosition(), QPointF());
    QCOMPARE(setContentAnchor(item, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().display().contentPosition(), QPointF());

    ImageViewportPresentationCommand beyondTolerance;
    beyondTolerance.setPreferredManualZoomPercent(100.0006);
    beyondTolerance.setZoomAnchor(QPointF(100, 80));
    QCOMPARE(
        item.setPresentation(beyondTolerance).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().display().horizontalPannable(), true);
    QCOMPARE(item.state().display().verticalPannable(), false);
    QVERIFY(item.state().display().maximumContentPosition().x() > 0.001);

    QCOMPARE(setContentAnchor(item, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().display().contentPosition().x(),
        item.state().display().maximumContentPosition().x());
    QCOMPARE(item.state().display().contentPosition().y(), 0.0);
}

void ImageViewportPresentationStateTest::mirrorPresentationCommandsPreserveItemCenterAnchor()
{
    ImageSequenceFactory factory;
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    const auto verifyClose = [](double actual, double expected) {
        QVERIFY2(qAbs(actual - expected) < 0.000001,
            qPrintable(QStringLiteral("actual %1 expected %2").arg(actual).arg(expected)));
    };

    ImageViewport horizontalItem;
    horizontalItem.setSize(QSizeF(100.0, 100.0));
    horizontalItem.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(horizontalItem);
    QCOMPARE(activateManualZoomPercentCommand(horizontalItem, 200.0),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setContentAnchor(horizontalItem, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);

    const QPointF horizontalAnchor(50.0, 50.0);
    const ImageViewportCoordinateResult horizontalBefore
        = mapItemToSpread(horizontalItem, horizontalAnchor.x(), horizontalAnchor.y());
    QCOMPARE(horizontalBefore.isValid(), true);
    QCOMPARE(
        setMirrorHorizontallyCommand(horizontalItem, true), ImageViewportCommandOutcome::Accepted);
    const ImageViewportCoordinateResult horizontalAfter
        = mapItemToSpread(horizontalItem, horizontalAnchor.x(), horizontalAnchor.y());
    QCOMPARE(horizontalAfter.isValid(), true);
    verifyClose(horizontalAfter.point().x(), horizontalBefore.point().x());
    verifyClose(horizontalAfter.point().y(), horizontalBefore.point().y());

    ImageViewport verticalItem;
    verticalItem.setSize(QSizeF(100.0, 100.0));
    verticalItem.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(verticalItem);
    QCOMPARE(activateManualZoomPercentCommand(verticalItem, 200.0),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setContentAnchor(verticalItem, ImageViewportContentAnchor::End),
        ImageViewportCommandOutcome::Accepted);

    const QPointF verticalAnchor(50.0, 50.0);
    const ImageViewportCoordinateResult verticalBefore
        = mapItemToSpread(verticalItem, verticalAnchor.x(), verticalAnchor.y());
    QCOMPARE(verticalBefore.isValid(), true);
    QCOMPARE(setMirrorVerticallyCommand(verticalItem, true), ImageViewportCommandOutcome::Accepted);
    const ImageViewportCoordinateResult verticalAfter
        = mapItemToSpread(verticalItem, verticalAnchor.x(), verticalAnchor.y());
    QCOMPARE(verticalAfter.isValid(), true);
    verifyClose(verticalAfter.point().x(), verticalBefore.point().x());
    verifyClose(verticalAfter.point().y(), verticalBefore.point().y());
}

void ImageViewportPresentationStateTest::rotationAffectsSpreadMapping()
{
    ImageSequenceFactory factory;
    QImage image(10, 20, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewportCommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().rotationDegrees(), 90);
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleSpreadRect(item), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(primaryItemRect(item), QRectF(0.0, 25.0, 100.0, 50.0));

    const ImageViewportCoordinateResult center = mapItemToSpread(item, 50.0, 50.0);
    QCOMPARE(center.isValid(), true);
    QCOMPARE(center.point().x(), 5.0);
    QCOMPARE(center.point().y(), 10.0);

    const ImageViewportCoordinateResult imageCenter = mapItemToPrimaryPage(item, 50.0, 50.0);
    QCOMPARE(imageCenter.isValid(), true);
    QCOMPARE(imageCenter.point().x(), 5.0);
    QCOMPARE(imageCenter.point().y(), 10.0);
    verifyInvalidCoordinateResult(mapSpreadToItem(item, 10.0, 10.0));
}

QTEST_MAIN(ImageViewportPresentationStateTest)

#include "tst_imageviewport_presentation_state.moc"
