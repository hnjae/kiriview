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

private slots:
    void invalidPresentationEnumCommandsRejectWithoutDisplayMutation();
    void presentationChangesWithoutDisplayKeepEmptyGeometry();
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
    void revisionTokensUseSharedNonWrappingAllocator();
    void invalidPresentationTargetTransitionPreservesStateAndRevisions();
    void presentationCommandsUpdateCommandDiagnostics();
    void manualZoomMaximumFallsBackAcrossDisplayStates();
    void manualZoomAbovePublishedLimitIsInvalid();
    void presentationCommandZoomStepDeltaUsesSharedSetZoomPath();
    void mirrorPresentationCommandsPreserveItemCenterAnchor();
    void rotationAffectsSpreadMapping();
};

static ImageViewportCommandOutcome setRotationDegrees(ImageViewport& item, int degrees)
{
    ImageViewportPresentationCommand command;
    command.setRotationDegrees(degrees);
    return item.setPresentation(command).outcome();
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

static ImageViewportCommandOutcome setManualZoomPercentCommand(ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(percent);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome activateManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(ImageViewportFitMode::Manual);
    command.setManualZoomPercent(percent);
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

    QCOMPARE(activateManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
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

    QCOMPARE(setManualZoomPercentCommand(item, 1000.0), ImageViewportCommandOutcome::Accepted);
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
    QCOMPARE(displayedSpreadSize(item), QSizeF(44.0, 20.0));
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

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::FitHeight);
    QCOMPARE(item.state().presentation().manualZoomPercent(), 200.0);
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
        QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
        QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Contain);
        QCOMPARE(item.state().presentation().manualZoomPercent(), 200.0);
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
    QCOMPARE(setManualZoomPercentCommand(item, std::numeric_limits<double>::infinity()),
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

    item.setSize(QSizeF(100.0, 50.0));
    const ImageViewportRevisionToken displayAfterGeometry = viewportDisplayRevision(item);
    QVERIFY(displayAfterGeometry.isValid());
    QCOMPARE(revisionTokenValueForTest(displayAfterGeometry), firstLargeToken);

    QCOMPARE(setManualZoomPercentCommand(item, std::numeric_limits<double>::infinity()),
        ImageViewportCommandOutcome::Invalid);
    const ImageViewportRevisionToken commandAfterInvalid = viewportCommandRevision(item);
    QVERIFY(commandAfterInvalid.isValid());
    QVERIFY(commandAfterInvalid != displayAfterGeometry);
    QVERIFY(revisionTokenValueForTest(commandAfterInvalid) > firstLargeToken);

    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const ImageViewportRevisionToken requestAfterAssignment = viewportRequestRevision(item);
    QVERIFY(requestAfterAssignment.isValid());
    QVERIFY(requestAfterAssignment != displayAfterGeometry);
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

void ImageViewportPresentationStateTest::manualZoomMaximumFallsBackAcrossDisplayStates()
{
    const double displayDemandCeiling = ImageViewportDisplayLimits::maximumManualZoomPercent();
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> stillResult(factory.fromFrame(&frame));
    QVERIFY(stillResult->sequence());

    ImageViewport emptyItem;
    QCOMPARE(emptyItem.state().presentation().maximumManualZoomPercent(), displayDemandCeiling);
    emptyItem.setSize(QSizeF(80.0, 100.0));
    QCOMPARE(emptyItem.state().presentation().maximumManualZoomPercent(), displayDemandCeiling);

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
    QCOMPARE(loadingItem.state().presentation().maximumManualZoomPercent(), displayDemandCeiling);

    ImageViewport readyItem;
    readyItem.setSize(QSizeF(80.0, 100.0));
    readyItem.setPresentationTarget(ImageViewportPresentationTarget(stillResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(readyItem);
    QCOMPARE(readyItem.state().presentation().maximumManualZoomPercent(), displayDemandCeiling);
    QCOMPARE(setManualZoomPercentCommand(readyItem, 200.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(readyItem.state().presentation().maximumManualZoomPercent(), displayDemandCeiling);

    readyItem.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(readyItem.state().presentation().maximumManualZoomPercent(), displayDemandCeiling);

    ImageViewport retainedItem;
    retainedItem.setSize(QSizeF(80.0, 100.0));
    retainedItem.setPresentationTarget(ImageViewportPresentationTarget(stillResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(retainedItem);
    retainedItem.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(displayStatusValue(retainedItem),
        enumValue(retainedItem.metaObject(), "DisplayStatus", "Retained"));
    QCOMPARE(retainedItem.state().presentation().maximumManualZoomPercent(), displayDemandCeiling);
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
    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewportCommandOutcome::Accepted);
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

    QCOMPARE(setManualZoomPercentCommand(
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
    verifyClose(item.state().presentation().manualZoomPercent(), 100.0);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(1);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Contain);
    verifyClose(item.state().presentation().manualZoomPercent(), 125.0);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(-1);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    verifyClose(item.state().presentation().manualZoomPercent(), 100.0);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(std::numeric_limits<int>::max());
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().manualZoomPercent(),
        item.state().presentation().maximumManualZoomPercent());
    QCOMPARE(item.state().presentation().zoomPercent(), 625.0);
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
