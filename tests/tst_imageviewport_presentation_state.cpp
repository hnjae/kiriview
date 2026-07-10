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
    void nearestVisibleHelpersClampPrimaryOnlyVisibleGeometry();
    void nearestVisibleHelpersClampTwoPageSpreadAndPageDomains();
    void nearestVisibleHelpersFollowPanRotationMirrorAndRetainedGeometry();
    void fitModesExposeZoomAndPannability();
    void preserveManualPercentDiffersFromPreserveWhenResultingFitIsManual();
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

static ImageViewport::CommandOutcome setRotationDegrees(ImageViewport& item, int degrees)
{
    ImageViewportPresentationCommand command;
    command.setRotationDegrees(degrees);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setMirrorHorizontallyCommand(ImageViewport& item, bool mirror)
{
    ImageViewportPresentationCommand command;
    command.setMirrorHorizontally(mirror);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setMirrorVerticallyCommand(ImageViewport& item, bool mirror)
{
    ImageViewportPresentationCommand command;
    command.setMirrorVertically(mirror);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setScanDirection(
    ImageViewport& item, ImageViewport::ScanDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setScanDirection(direction);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setPanDelta(ImageViewport& item, QPointF delta)
{
    ImageViewportPresentationCommand command;
    command.setPanDelta(delta);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setSpreadDirectionCommand(
    ImageViewport& item, ImageViewport::SpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setPageGapCommand(ImageViewport& item, double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setFitModeCommand(
    ImageViewport& item, ImageViewport::FitMode mode)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(mode);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(percent);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setQualityTogglesCommand(
    ImageViewport& item, bool smoothing, bool mipmap)
{
    ImageViewportPresentationCommand command;
    command.setSmoothing(smoothing);
    command.setMipmap(mipmap);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setBackgroundCommand(
    ImageViewport& item, ImageViewport::BackgroundMode mode, QColor color)
{
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(mode);
    command.setBackgroundColor(color);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setLoopingCommand(ImageViewport& item, bool looping)
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
    command.setBackgroundMode(static_cast<ImageViewport::BackgroundMode>(
        999)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::Transparent);
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

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorVerticallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(item, ImageViewport::BackgroundMode::SolidColor, Qt::red),
        ImageViewport::CommandOutcome::Accepted);

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
    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const QRectF preservedContentRect = contentRect(item);
    const QRectF preservedVisibleImageRect = visibleImageRect(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);

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
    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const QRectF preservedContentRect = contentRect(item);
    const QRectF preservedVisibleImageRect = visibleImageRect(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);

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
    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    const QRectF preservedContentRect = contentRect(item);
    const QRectF preservedVisibleImageRect = visibleImageRect(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);

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
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
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

    QCOMPARE(setManualZoomPercentCommand(item, 1000.0), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(stateSpy.count(), 1);

    QCOMPARE(setPanDelta(item, QPointF(4.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(stateSpy.count(), 2);

    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(stateSpy.count(), 3);

    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(stateSpy.count(), 4);
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
        ImageViewport::CommandOutcome::Accepted);
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

    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(displayedSpreadSize(item), QSizeF(44.0, 20.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 2.0, 88.0, 40.0));
    QCOMPARE(primaryPageRect(item), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryPageRect(item), QRectF(14.0, 0.0, 30.0, 20.0));
    QCOMPARE(primaryItemRect(item), QRectF(0.0, 2.0, 20.0, 40.0));
    QCOMPARE(secondaryItemRect(item), QRectF(28.0, 2.0, 60.0, 40.0));

    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
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
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(setManualZoomPercentCommand(item, 100.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(-12.0, 0.0, 44.0, 20.0));
    QCOMPARE(contentPosition(item), QPointF(12.0, 0.0));
    QCOMPARE(maximumContentPosition(item), QPointF(24.0, 0.0));

    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

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
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 100.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().rotationDegrees(), 90);
    QCOMPARE(contentRect(item), QRectF(0.0, -12.0, 20.0, 44.0));
    QCOMPARE(contentPosition(item), QPointF(0.0, 12.0));
    QCOMPARE(maximumContentPosition(item), QPointF(0.0, 24.0));

    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

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
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
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
    verifyInvalidCoordinateResult(nearestVisibleSpreadCoordinate(item, 0.0, 0.0));
    verifyInvalidCoordinateResult(
        nearestVisiblePageCoordinate(item, ImageViewport::PageRole::Primary, 0.0, 0.0));
    verifyInvalidCoordinateResult(nearestVisiblePrimaryPagePoint(item, 0.0, 0.0));
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
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
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

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(displayedSpreadSize(item), QSizeF(44.0, 20.0));
    QCOMPARE(primaryDisplayedImageSize(item), QSizeF(10.0, 20.0));
    QCOMPARE(secondaryDisplayedImageSize(item), QSizeF(30.0, 20.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 2.0, 88.0, 40.0));
    QCOMPARE(primaryItemRect(item), QRectF(0.0, 2.0, 20.0, 40.0));
    QCOMPARE(secondaryItemRect(item), QRectF(28.0, 2.0, 60.0, 40.0));

    const ImageViewportCoordinateResult retainedSpread
        = nearestVisibleSpreadCoordinate(item, 44.0, 20.0);
    QCOMPARE(retainedSpread.isValid(), true);
    QCOMPARE(retainedSpread.point().x(), std::nextafter(44.0, 0.0));
    QCOMPARE(retainedSpread.point().y(), std::nextafter(20.0, 0.0));
    const ImageViewportCoordinateResult retainedSecondary
        = nearestVisiblePageCoordinate(item, ImageViewport::PageRole::Secondary, 30.0, 20.0);
    QCOMPARE(retainedSecondary.isValid(), true);
    QCOMPARE(retainedSecondary.point().x(), std::nextafter(30.0, 0.0));
    QCOMPARE(retainedSecondary.point().y(), std::nextafter(20.0, 0.0));
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
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

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
        ImageViewport::CommandOutcome::Accepted);
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
        ImageViewport::CommandOutcome::Accepted);
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
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

    const ImageViewportCoordinateResult gapSpreadPoint = mapItemToSpread(item, 22.0, 22.0);
    QCOMPARE(gapSpreadPoint.isValid(), true);
    QCOMPARE(gapSpreadPoint.point().x(), 11.0);
    QCOMPARE(gapSpreadPoint.point().y(), 10.0);
    QCOMPARE(mapItemToPage(item, ImageViewport::PageRole::Primary, 22.0, 22.0).isValid(), false);
    QCOMPARE(mapItemToPage(item, ImageViewport::PageRole::Secondary, 22.0, 22.0).isValid(), false);

    const ImageViewportCoordinateResult primaryPoint
        = mapItemToPage(item, ImageViewport::PageRole::Primary, 19.0, 22.0);
    QCOMPARE(primaryPoint.isValid(), true);
    QCOMPARE(primaryPoint.point().x(), 9.5);
    QCOMPARE(primaryPoint.point().y(), 10.0);
    QCOMPARE(mapItemToPage(item, ImageViewport::PageRole::Primary, 20.0, 22.0).isValid(), false);

    const ImageViewportCoordinateResult secondaryOrigin
        = mapPageToItem(item, ImageViewport::PageRole::Secondary, 0.0, 0.0);
    QCOMPARE(secondaryOrigin.isValid(), true);
    QCOMPARE(secondaryOrigin.point().x(), 28.0);
    QCOMPARE(secondaryOrigin.point().y(), 2.0);
    QCOMPARE(mapPageToItem(item, ImageViewport::PageRole::Secondary, 30.0, 0.0).isValid(), false);
}

void ImageViewportPresentationStateTest::nearestVisibleHelpersClampPrimaryOnlyVisibleGeometry()
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

    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    const double rightEdgeInside = std::nextafter(16.0, 0.0);
    const double bottomEdgeInside = std::nextafter(8.0, 0.0);

    const ImageViewportCoordinateResult leftSpread
        = nearestVisibleSpreadCoordinate(item, -5.0, 4.0);
    QCOMPARE(leftSpread.isValid(), true);
    QCOMPARE(leftSpread.point().x(), 0.0);
    QCOMPARE(leftSpread.point().y(), 4.0);
    QCOMPARE(containsVisibleSpreadCoordinate(item, leftSpread.point().x(), leftSpread.point().y()),
        true);

    const ImageViewportCoordinateResult spreadBottomRight
        = nearestVisibleSpreadCoordinate(item, 16.0, 8.0);
    QCOMPARE(spreadBottomRight.isValid(), true);
    QCOMPARE(spreadBottomRight.point().x(), rightEdgeInside);
    QCOMPARE(spreadBottomRight.point().y(), bottomEdgeInside);
    QCOMPARE(containsVisibleSpreadCoordinate(
                 item, spreadBottomRight.point().x(), spreadBottomRight.point().y()),
        true);

    const ImageViewportCoordinateResult pageBottomRight
        = nearestVisiblePageCoordinate(item, ImageViewport::PageRole::Primary, 16.0, 8.0);
    QCOMPARE(pageBottomRight.isValid(), true);
    QCOMPARE(pageBottomRight.point().x(), rightEdgeInside);
    QCOMPARE(pageBottomRight.point().y(), bottomEdgeInside);
    QCOMPARE(containsVisiblePageCoordinate(item, ImageViewport::PageRole::Primary,
                 pageBottomRight.point().x(), pageBottomRight.point().y()),
        true);

    const ImageViewportCoordinateResult imageBottomRight
        = nearestVisiblePrimaryPagePoint(item, 16.0, 8.0);
    QCOMPARE(imageBottomRight.isValid(), pageBottomRight.isValid());
    QCOMPARE(imageBottomRight.point(), pageBottomRight.point());

    verifyInvalidCoordinateResult(
        nearestVisiblePageCoordinate(item, ImageViewport::PageRole::Secondary, 1.0, 1.0));
    verifyInvalidCoordinateResult(
        nearestVisibleSpreadCoordinate(item, std::numeric_limits<double>::infinity(), 1.0));
    verifyInvalidCoordinateResult(mapItemToSpread(item, 100.0, 50.0));
    verifyInvalidCoordinateResult(
        mapItemToPage(item, ImageViewport::PageRole::Primary, 100.0, 50.0));

    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportCommandRevision(item), commandRevision);
    QCOMPARE(stateSpy.count(), 0);
}

void ImageViewportPresentationStateTest::nearestVisibleHelpersClampTwoPageSpreadAndPageDomains()
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
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(visibleSpreadRect(item), QRectF(0.0, 0.0, 44.0, 20.0));
    QCOMPARE(visiblePrimaryPageRect(item), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(visibleSecondaryPageRect(item), QRectF(0.0, 0.0, 30.0, 20.0));

    const double spreadRightInside = std::nextafter(44.0, 0.0);
    const double primaryRightInside = std::nextafter(10.0, 0.0);
    const double secondaryRightInside = std::nextafter(30.0, 0.0);
    const double bottomInside = std::nextafter(20.0, 0.0);

    const ImageViewportCoordinateResult gapSpread
        = nearestVisibleSpreadCoordinate(item, 11.0, 10.0);
    QCOMPARE(gapSpread.isValid(), true);
    QCOMPARE(gapSpread.point().x(), 11.0);
    QCOMPARE(gapSpread.point().y(), 10.0);
    QCOMPARE(
        containsVisibleSpreadCoordinate(item, gapSpread.point().x(), gapSpread.point().y()), true);

    verifyInvalidCoordinateResult(
        mapItemToPage(item, ImageViewport::PageRole::Primary, 22.0, 22.0));
    verifyInvalidCoordinateResult(
        mapItemToPage(item, ImageViewport::PageRole::Secondary, 22.0, 22.0));

    const ImageViewportCoordinateResult primaryFromGap
        = nearestVisiblePageCoordinate(item, ImageViewport::PageRole::Primary, 11.0, 10.0);
    QCOMPARE(primaryFromGap.isValid(), true);
    QCOMPARE(primaryFromGap.point().x(), primaryRightInside);
    QCOMPARE(primaryFromGap.point().y(), 10.0);
    QCOMPARE(containsVisiblePageCoordinate(item, ImageViewport::PageRole::Primary,
                 primaryFromGap.point().x(), primaryFromGap.point().y()),
        true);

    const ImageViewportCoordinateResult secondaryFromGap
        = nearestVisiblePageCoordinate(item, ImageViewport::PageRole::Secondary, -3.0, 10.0);
    QCOMPARE(secondaryFromGap.isValid(), true);
    QCOMPARE(secondaryFromGap.point().x(), 0.0);
    QCOMPARE(secondaryFromGap.point().y(), 10.0);
    QCOMPARE(containsVisiblePageCoordinate(item, ImageViewport::PageRole::Secondary,
                 secondaryFromGap.point().x(), secondaryFromGap.point().y()),
        true);

    const ImageViewportCoordinateResult spreadBottomRight
        = nearestVisibleSpreadCoordinate(item, 44.0, 20.0);
    QCOMPARE(spreadBottomRight.isValid(), true);
    QCOMPARE(spreadBottomRight.point().x(), spreadRightInside);
    QCOMPARE(spreadBottomRight.point().y(), bottomInside);

    const ImageViewportCoordinateResult secondaryBottomRight
        = nearestVisiblePageCoordinate(item, ImageViewport::PageRole::Secondary, 30.0, 20.0);
    QCOMPARE(secondaryBottomRight.isValid(), true);
    QCOMPARE(secondaryBottomRight.point().x(), secondaryRightInside);
    QCOMPARE(secondaryBottomRight.point().y(), bottomInside);
    verifyInvalidCoordinateResult(
        mapPageToItem(item, ImageViewport::PageRole::Secondary, 30.0, 0.0));
}

void ImageViewportPresentationStateTest::
    nearestVisibleHelpersFollowPanRotationMirrorAndRetainedGeometry()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport mirroredItem;
    mirroredItem.setSize(QSizeF(100.0, 100.0));
    mirroredItem.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(mirroredItem);
    QCOMPARE(setFitModeCommand(mirroredItem, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(mirroredItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(visibleImageRect(mirroredItem), QRectF(8.0, 0.0, 8.0, 8.0));

    const ImageViewportCoordinateResult pannedLeft
        = nearestVisiblePrimaryPagePoint(mirroredItem, 0.0, 4.0);
    QCOMPARE(pannedLeft.isValid(), true);
    QCOMPARE(pannedLeft.point().x(), 8.0);
    QCOMPARE(pannedLeft.point().y(), 4.0);
    const ImageViewportCoordinateResult pannedRight
        = nearestVisiblePrimaryPagePoint(mirroredItem, 16.0, 4.0);
    QCOMPARE(pannedRight.isValid(), true);
    QCOMPARE(pannedRight.point().x(), std::nextafter(16.0, 8.0));
    QCOMPARE(pannedRight.point().y(), 4.0);

    QCOMPARE(setScanDirection(mirroredItem, ImageViewport::ScanDirection::Start),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        setMirrorHorizontallyCommand(mirroredItem, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(visibleImageRect(mirroredItem), QRectF(0.0, 0.0, 8.0, 8.0));
    const ImageViewportCoordinateResult mirroredLeft
        = nearestVisiblePrimaryPagePoint(mirroredItem, -1.0, 4.0);
    QCOMPARE(mirroredLeft.isValid(), true);
    QCOMPARE(mirroredLeft.point().x(), 0.0);
    QCOMPARE(mirroredLeft.point().y(), 4.0);
    const ImageViewportCoordinateResult mirroredRight
        = nearestVisiblePrimaryPagePoint(mirroredItem, 16.0, 4.0);
    QCOMPARE(mirroredRight.isValid(), true);
    QCOMPARE(mirroredRight.point().x(), std::nextafter(8.0, 0.0));
    QCOMPARE(mirroredRight.point().y(), 4.0);

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

    ImageViewport rotatedItem;
    rotatedItem.setSize(QSizeF(20.0, 20.0));
    QCOMPARE(rotatedItem
                 .setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(rotatedItem);
    QCOMPARE(setPageGapCommand(rotatedItem, 4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(rotatedItem, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        setManualZoomPercentCommand(rotatedItem, 100.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(rotatedItem, 90), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(rotatedItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(visibleSpreadRect(rotatedItem), QRectF(24.0, 0.0, 20.0, 20.0));

    const ImageViewportCoordinateResult rotatedLeft
        = nearestVisibleSpreadCoordinate(rotatedItem, 0.0, 10.0);
    QCOMPARE(rotatedLeft.isValid(), true);
    QCOMPARE(rotatedLeft.point().x(), 24.0);
    QCOMPARE(rotatedLeft.point().y(), 10.0);
    const ImageViewportCoordinateResult rotatedBottomRight
        = nearestVisibleSpreadCoordinate(rotatedItem, 44.0, 20.0);
    QCOMPARE(rotatedBottomRight.isValid(), true);
    QCOMPARE(rotatedBottomRight.point().x(), std::nextafter(44.0, 24.0));
    QCOMPARE(rotatedBottomRight.point().y(), std::nextafter(20.0, 0.0));
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

    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(-60.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.state().presentation().zoomPercent(), 1250.0);
    QCOMPARE(contentPosition(item), QPointF(60.0, 0.0));
    QCOMPARE(maximumContentPosition(item), QPointF(120.0, 0.0));
    QCOMPARE(horizontalPannable(item), true);
    QCOMPARE(verticalPannable(item), false);

    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(-120.0, 0.0, 200.0, 100.0));
    QCOMPARE(contentPosition(item), QPointF(120.0, 0.0));

    QCOMPARE(setPanDelta(item, QPointF(-500.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentRect(item), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(contentPosition(item), QPointF(0.0, 0.0));

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(contentRect(item), QRectF(24.0, 42.0, 32.0, 16.0));
    QCOMPARE(item.state().presentation().zoomPercent(), 200.0);
}

void ImageViewportPresentationStateTest::
    preserveManualPercentDiffersFromPreserveWhenResultingFitIsManual()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame firstFrame(image);
    ImageFrame replacementFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> preserveResult(factory.fromFrame(&replacementFrame));
    QScopedPointer<ImageSequenceFactoryResult> manualPercentResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(firstResult->sequence());
    QVERIFY(preserveResult->sequence());
    QVERIFY(manualPercentResult->sequence());

    auto configureItem = [&](ImageViewport& item) {
        item.setSize(QSizeF(80.0, 100.0));
        item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
            PresentationTargetTransitionPolicy {});
        acknowledgePendingRenderCommitForTest(item);
        QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
            ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::FitHeight);
        QCOMPARE(item.state().presentation().zoomPercent(), 1250.0);
    };

    PresentationTargetTransitionPolicy preservePolicy;
    preservePolicy.setZoomTransition(PresentationTargetTransitionPolicy::ZoomTransition::Preserve);
    preservePolicy.setFitModeTransition(
        PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    preservePolicy.setFitMode(ImageViewport::FitMode::Manual);

    ImageViewport preserveItem;
    configureItem(preserveItem);
    QCOMPARE(preserveItem
                 .setPresentationTarget(
                     ImageViewportPresentationTarget(preserveResult->sequence()), preservePolicy)
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(preserveItem);
    QCOMPARE(preserveItem.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(preserveItem.state().presentation().zoomPercent(), 1250.0);

    PresentationTargetTransitionPolicy manualPercentPolicy = preservePolicy;
    manualPercentPolicy.setZoomTransition(
        PresentationTargetTransitionPolicy::ZoomTransition::PreserveManualPercent);

    ImageViewport manualPercentItem;
    configureItem(manualPercentItem);
    QCOMPARE(
        manualPercentItem
            .setPresentationTarget(ImageViewportPresentationTarget(manualPercentResult->sequence()),
                manualPercentPolicy)
            .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(manualPercentItem);
    QCOMPARE(manualPercentItem.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(manualPercentItem.state().presentation().zoomPercent(), 200.0);
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
    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentPosition(item), QPointF(50.0, 50.0));
    QCOMPARE(setManualZoomPercentCommand(item, std::numeric_limits<double>::infinity()),
        ImageViewport::CommandOutcome::Invalid);
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(setManualZoomPercentCommand(item, 300.0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
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
        ImageViewport::CommandOutcome::Invalid);
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
    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 12.0), ImageViewport::CommandOutcome::Accepted);

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
    invalidPolicy.setFitMode(ImageViewport::FitMode::FitHeight);

    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(replacementResult->sequence()), invalidPolicy);

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Invalid);
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

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, -1).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const ImageViewportRevisionToken invalidSeekCommandRevision
        = revisionTokenProperty(item, "commandRevision");

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", invalidSeekCommandRevision);
    QCOMPARE(stateSpy.count(), 1);

    const int rotationDegrees = item.state().presentation().rotationDegrees();
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(setRotationDegrees(item, 45), ImageViewport::CommandOutcome::Invalid);
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
    QCOMPARE(
        setManualZoomPercentCommand(readyItem, 200.0), ImageViewport::CommandOutcome::Accepted);
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
    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
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
        ImageViewport::CommandOutcome::Invalid);

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

    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    ImageViewportPresentationCommand command;
    command.setZoomStepDelta(0);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(1);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Accepted);
    verifyClose(item.state().presentation().zoomPercent(), 781.25);

    command = {};
    command.setZoomStepDelta(-1);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Accepted);
    verifyClose(item.state().presentation().zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(std::numeric_limits<int>::max());
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().zoomPercent(),
        item.state().presentation().maximumManualZoomPercent());
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
    QCOMPARE(setManualZoomPercentCommand(horizontalItem, 200.0),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(horizontalItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

    const QPointF horizontalAnchor(50.0, 50.0);
    const ImageViewportCoordinateResult horizontalBefore
        = mapItemToSpread(horizontalItem, horizontalAnchor.x(), horizontalAnchor.y());
    QCOMPARE(horizontalBefore.isValid(), true);
    QCOMPARE(setMirrorHorizontallyCommand(horizontalItem, true),
        ImageViewport::CommandOutcome::Accepted);
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
    QCOMPARE(
        setManualZoomPercentCommand(verticalItem, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(verticalItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

    const QPointF verticalAnchor(50.0, 50.0);
    const ImageViewportCoordinateResult verticalBefore
        = mapItemToSpread(verticalItem, verticalAnchor.x(), verticalAnchor.y());
    QCOMPARE(verticalBefore.isValid(), true);
    QCOMPARE(
        setMirrorVerticallyCommand(verticalItem, true), ImageViewport::CommandOutcome::Accepted);
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
    QCOMPARE(setRotationDegrees(item, 90), ImageViewport::CommandOutcome::Accepted);

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
