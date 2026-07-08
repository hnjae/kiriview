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
    void presentationChangesWithoutDisplayDoNotNotifyGeometryState();
    void backgroundPresentationDoesNotChangeRequestOrPlayback();
    void qualityPresentationDoesNotChangeRequestGeometryOrPlayback();
    void loopingDoesNotChangeRequestDisplayOrGeometry();
    void presentationChangesNotifyGeometryState();
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
    void invalidPageSetTransitionPreservesStateAndRevisions();
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
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setMirrorHorizontallyCommand(ImageViewport& item, bool mirror)
{
    ImageViewportPresentationCommand command;
    command.setMirrorHorizontally(mirror);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setMirrorVerticallyCommand(ImageViewport& item, bool mirror)
{
    ImageViewportPresentationCommand command;
    command.setMirrorVertically(mirror);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setScanDirection(
    ImageViewport& item, ImageViewport::ScanDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setScanDirection(direction);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setPanDelta(ImageViewport& item, QPointF delta)
{
    ImageViewportPresentationCommand command;
    command.setPanDelta(delta);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setSpreadDirectionCommand(
    ImageViewport& item, ImageViewport::SpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setPageGapCommand(ImageViewport& item, double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setFitModeCommand(
    ImageViewport& item, ImageViewport::FitMode mode)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(mode);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(percent);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setQualityTogglesCommand(
    ImageViewport& item, bool smoothing, bool mipmap)
{
    ImageViewportPresentationCommand command;
    command.setSmoothing(smoothing);
    command.setMipmap(mipmap);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setBackgroundCommand(
    ImageViewport& item, ImageViewport::BackgroundMode mode, QColor color)
{
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(mode);
    command.setBackgroundColor(color);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setLoopingCommand(ImageViewport& item, bool looping)
{
    ImageViewportPresentationCommand command;
    command.setLooping(looping);
    return item.setPresentation(command);
}

void ImageViewportPresentationStateTest::invalidPresentationEnumCommandsRejectWithoutDisplayMutation()
{
    ImageViewport item;
    const RevisionToken initialDisplayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken initialCommandRevision = revisionTokenProperty(item, "commandRevision");

    ImageViewportPresentationCommand command;
    command.setBackgroundMode(static_cast<ImageViewport::BackgroundMode>(
        999)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Transparent);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), initialDisplayRevision);
    verifyRevisionChanged(item, "commandRevision", initialCommandRevision);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
}

void ImageViewportPresentationStateTest::presentationChangesWithoutDisplayDoNotNotifyGeometryState()
{
    ImageViewport item;
    const RevisionToken initialDisplayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorVerticallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(item, ImageViewport::BackgroundMode::SolidColor, Qt::red),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    verifyRevisionChanged(item, "displayRevision", initialDisplayRevision);
    QCOMPARE(displayRevisionSpy.count(), 8);
    QCOMPARE(presentationSpy.count(), 8);
    QCOMPARE(geometrySpy.count(), 0);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(geometrySpy.count(), 0);
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const QRectF contentRect = item.property("contentRect").toRectF();
    const QRectF visibleImageRect = item.property("visibleImageRect").toRectF();
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);

    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(item.property("contentRect").toRectF(), contentRect);
    QCOMPARE(item.property("visibleImageRect").toRectF(), visibleImageRect);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(displayRevisionSpy.count(), 2);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 2);
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const QRectF contentRect = item.property("contentRect").toRectF();
    const QRectF visibleImageRect = item.property("visibleImageRect").toRectF();
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(item.property("contentRect").toRectF(), contentRect);
    QCOMPARE(item.property("visibleImageRect").toRectF(), visibleImageRect);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(displayRevisionSpy.count(), 2);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 2);
    QCOMPARE(diagnosticsSpy.count(), 0);
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    const QRectF contentRect = item.property("contentRect").toRectF();
    const QRectF visibleImageRect = item.property("visibleImageRect").toRectF();
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayStateSpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy loopingSpy(&item, &ImageViewport::loopingChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.looping(), true);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(item.property("contentRect").toRectF(), contentRect);
    QCOMPARE(item.property("visibleImageRect").toRectF(), visibleImageRect);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displayStateSpy.count(), 0);
    QCOMPARE(displayRevisionSpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 0);
    QCOMPARE(loopingSpy.count(), 1);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportPresentationStateTest::presentationChangesNotifyGeometryState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    QCOMPARE(setManualZoomPercentCommand(item, 1000.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(geometrySpy.count(), 1);

    QCOMPARE(setPanDelta(item, QPointF(4.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(geometrySpy.count(), 2);

    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(geometrySpy.count(), 3);

    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(geometrySpy.count(), 4);
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.property("displayedSpreadSize").toSizeF(), QSizeF(40.0, 20.0));
    QCOMPARE(item.property("primaryDisplayedImageSize").toSizeF(), QSizeF(10.0, 20.0));
    QCOMPARE(item.property("secondaryDisplayedImageSize").toSizeF(), QSizeF(30.0, 20.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 88.0, 44.0));
    QCOMPARE(item.property("primaryPageRect").toRectF(), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF(10.0, 0.0, 30.0, 20.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(0.0, 0.0, 22.0, 44.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(22.0, 0.0, 66.0, 44.0));
    QCOMPARE(item.property("visibleSpreadRect").toRectF(), QRectF(0.0, 0.0, 40.0, 20.0));
    QCOMPARE(item.property("visiblePrimaryPageRect").toRectF(), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("visibleSecondaryPageRect").toRectF(), QRectF(0.0, 0.0, 30.0, 20.0));

    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("displayedSpreadSize").toSizeF(), QSizeF(44.0, 20.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 2.0, 88.0, 40.0));
    QCOMPARE(item.property("primaryPageRect").toRectF(), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF(14.0, 0.0, 30.0, 20.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(0.0, 2.0, 20.0, 40.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(28.0, 2.0, 60.0, 40.0));

    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("primaryPageRect").toRectF(), QRectF(34.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF(0.0, 0.0, 30.0, 20.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(68.0, 2.0, 20.0, 40.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(0.0, 2.0, 60.0, 40.0));
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(setManualZoomPercentCommand(item, 100.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-12.0, 0.0, 44.0, 20.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(12.0, 0.0));
    QCOMPARE(item.property("maximumContentPosition").toPointF(), QPointF(24.0, 0.0));

    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-24.0, 0.0, 44.0, 20.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(24.0, 0.0));
    QCOMPARE(item.property("visibleSpreadRect").toRectF(), QRectF(24.0, 0.0, 20.0, 20.0));
    QCOMPARE(item.property("visiblePrimaryPageRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleSecondaryPageRect").toRectF(), QRectF(10.0, 0.0, 20.0, 20.0));
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 100.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().rotationDegrees(), 90);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, -12.0, 20.0, 44.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(0.0, 12.0));
    QCOMPARE(item.property("maximumContentPosition").toPointF(), QPointF(0.0, 24.0));

    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, -24.0, 20.0, 44.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(0.0, 24.0));
    QCOMPARE(item.property("primaryPageRect").toRectF(), QRectF(34.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF(0.0, 0.0, 30.0, 20.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(0.0, 10.0, 20.0, 10.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(0.0, -24.0, 20.0, 30.0));
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    item.setSize(QSizeF(0.0, 44.0));

    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedSpreadSize").toSizeF(), QSizeF(44.0, 20.0));
    QCOMPARE(item.property("primaryDisplayedImageSize").toSizeF(), QSizeF(10.0, 20.0));
    QCOMPARE(item.property("secondaryDisplayedImageSize").toSizeF(), QSizeF(30.0, 20.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleSpreadRect").toRectF(), QRectF());
    QCOMPARE(item.property("visiblePrimaryPageRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleSecondaryPageRect").toRectF(), QRectF());
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF());
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF());
    QCOMPARE(item.itemToSpread(0.0, 0.0).isValid(), false);
    verifyInvalidCoordinateResult(item.nearestVisibleSpreadPoint(0.0, 0.0));
    verifyInvalidCoordinateResult(
        item.nearestVisiblePagePoint(ImageViewport::PageRole::Primary, 0.0, 0.0));
    verifyInvalidCoordinateResult(item.nearestVisibleImagePoint(0.0, 0.0));
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("displayedSpreadSize").toSizeF(), QSizeF(44.0, 20.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(28.0, 2.0, 60.0, 40.0));

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

    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(loadingResult->sequence()), {}),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedSpreadSize").toSizeF(), QSizeF(44.0, 20.0));
    QCOMPARE(item.property("primaryDisplayedImageSize").toSizeF(), QSizeF(10.0, 20.0));
    QCOMPARE(item.property("secondaryDisplayedImageSize").toSizeF(), QSizeF(30.0, 20.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 2.0, 88.0, 40.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(0.0, 2.0, 20.0, 40.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(28.0, 2.0, 60.0, 40.0));

    const CoordinateResult retainedSpread = item.nearestVisibleSpreadPoint(44.0, 20.0);
    QCOMPARE(retainedSpread.isValid(), true);
    QCOMPARE(retainedSpread.x(), std::nextafter(44.0, 0.0));
    QCOMPARE(retainedSpread.y(), std::nextafter(20.0, 0.0));
    const CoordinateResult retainedSecondary
        = item.nearestVisiblePagePoint(ImageViewport::PageRole::Secondary, 30.0, 20.0);
    QCOMPARE(retainedSecondary.isValid(), true);
    QCOMPARE(retainedSecondary.x(), std::nextafter(30.0, 0.0));
    QCOMPARE(retainedSecondary.y(), std::nextafter(20.0, 0.0));
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
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

    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(loadingResult->sequence()), {}),
        ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.secondarySequence(), nullptr);
    QCOMPARE(item.property("secondaryRequestedFrame").toInt(), -1);
    QCOMPARE(item.property("secondaryRequestedPosition").toInt(), -1);
    QCOMPARE(item.property("secondaryDisplayedFrame").toInt(), 0);
    QCOMPARE(item.property("secondaryDisplayedPosition").toInt(), -1);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("secondaryDisplayedImageSize").toSizeF(), QSizeF(30.0, 20.0));
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF(14.0, 0.0, 30.0, 20.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(28.0, 2.0, 60.0, 40.0));
    QCOMPARE(item.property("visibleSecondaryPageRect").toRectF(), QRectF(0.0, 0.0, 30.0, 20.0));

    PageSetTransitionPolicy clearBeforeLoad;
    clearBeforeLoad.setDisplayTransition(
        PageSetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    QCOMPARE(item.setPageSet(
                 QVariant::fromValue<QObject*>(loadingResult->sequence()), {}, clearBeforeLoad),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("secondaryDisplayedFrame").toInt(), -1);
    QCOMPARE(item.property("secondaryDisplayedPosition").toInt(), -1);
    QCOMPARE(item.property("secondaryDisplayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF());
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

    const CoordinateResult gapSpreadPoint = item.itemToSpread(22.0, 22.0);
    QCOMPARE(gapSpreadPoint.isValid(), true);
    QCOMPARE(gapSpreadPoint.x(), 11.0);
    QCOMPARE(gapSpreadPoint.y(), 10.0);
    QCOMPARE(item.itemToPage(ImageViewport::PageRole::Primary, 22.0, 22.0).isValid(), false);
    QCOMPARE(item.itemToPage(ImageViewport::PageRole::Secondary, 22.0, 22.0).isValid(), false);

    const CoordinateResult primaryPoint
        = item.itemToPage(ImageViewport::PageRole::Primary, 19.0, 22.0);
    QCOMPARE(primaryPoint.isValid(), true);
    QCOMPARE(primaryPoint.x(), 9.5);
    QCOMPARE(primaryPoint.y(), 10.0);
    QCOMPARE(item.itemToPage(ImageViewport::PageRole::Primary, 20.0, 22.0).isValid(), false);

    const CoordinateResult secondaryOrigin
        = item.pageToItem(ImageViewport::PageRole::Secondary, 0.0, 0.0);
    QCOMPARE(secondaryOrigin.isValid(), true);
    QCOMPARE(secondaryOrigin.x(), 28.0);
    QCOMPARE(secondaryOrigin.y(), 2.0);
    QCOMPARE(item.pageToItem(ImageViewport::PageRole::Secondary, 30.0, 0.0).isValid(), false);
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);

    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    QSignalSpy displaySpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy requestSpy(&item, &ImageViewport::requestRevisionChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandRevisionChanged);

    const double rightEdgeInside = std::nextafter(16.0, 0.0);
    const double bottomEdgeInside = std::nextafter(8.0, 0.0);

    const CoordinateResult leftSpread = item.nearestVisibleSpreadPoint(-5.0, 4.0);
    QCOMPARE(leftSpread.isValid(), true);
    QCOMPARE(leftSpread.x(), 0.0);
    QCOMPARE(leftSpread.y(), 4.0);
    QCOMPARE(item.containsVisibleSpreadPoint(leftSpread.x(), leftSpread.y()), true);

    const CoordinateResult spreadBottomRight = item.nearestVisibleSpreadPoint(16.0, 8.0);
    QCOMPARE(spreadBottomRight.isValid(), true);
    QCOMPARE(spreadBottomRight.x(), rightEdgeInside);
    QCOMPARE(spreadBottomRight.y(), bottomEdgeInside);
    QCOMPARE(item.containsVisibleSpreadPoint(spreadBottomRight.x(), spreadBottomRight.y()), true);

    const CoordinateResult pageBottomRight
        = item.nearestVisiblePagePoint(ImageViewport::PageRole::Primary, 16.0, 8.0);
    QCOMPARE(pageBottomRight.isValid(), true);
    QCOMPARE(pageBottomRight.x(), rightEdgeInside);
    QCOMPARE(pageBottomRight.y(), bottomEdgeInside);
    QCOMPARE(item.containsVisiblePagePoint(
                 ImageViewport::PageRole::Primary, pageBottomRight.x(), pageBottomRight.y()),
        true);

    const CoordinateResult imageBottomRight = item.nearestVisibleImagePoint(16.0, 8.0);
    QCOMPARE(imageBottomRight, pageBottomRight);

    verifyInvalidCoordinateResult(
        item.nearestVisiblePagePoint(ImageViewport::PageRole::Secondary, 1.0, 1.0));
    verifyInvalidCoordinateResult(
        item.nearestVisibleSpreadPoint(std::numeric_limits<double>::infinity(), 1.0));
    verifyInvalidCoordinateResult(item.itemToSpread(100.0, 50.0));
    verifyInvalidCoordinateResult(item.itemToPage(ImageViewport::PageRole::Primary, 100.0, 50.0));

    QCOMPARE(item.displayRevision(), displayRevision);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.commandRevision(), commandRevision);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(commandSpy.count(), 0);
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
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setPageGapCommand(item, 4.0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("visibleSpreadRect").toRectF(), QRectF(0.0, 0.0, 44.0, 20.0));
    QCOMPARE(item.property("visiblePrimaryPageRect").toRectF(), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("visibleSecondaryPageRect").toRectF(), QRectF(0.0, 0.0, 30.0, 20.0));

    const double spreadRightInside = std::nextafter(44.0, 0.0);
    const double primaryRightInside = std::nextafter(10.0, 0.0);
    const double secondaryRightInside = std::nextafter(30.0, 0.0);
    const double bottomInside = std::nextafter(20.0, 0.0);

    const CoordinateResult gapSpread = item.nearestVisibleSpreadPoint(11.0, 10.0);
    QCOMPARE(gapSpread.isValid(), true);
    QCOMPARE(gapSpread.x(), 11.0);
    QCOMPARE(gapSpread.y(), 10.0);
    QCOMPARE(item.containsVisibleSpreadPoint(gapSpread.x(), gapSpread.y()), true);

    verifyInvalidCoordinateResult(item.itemToPage(ImageViewport::PageRole::Primary, 22.0, 22.0));
    verifyInvalidCoordinateResult(item.itemToPage(ImageViewport::PageRole::Secondary, 22.0, 22.0));

    const CoordinateResult primaryFromGap
        = item.nearestVisiblePagePoint(ImageViewport::PageRole::Primary, 11.0, 10.0);
    QCOMPARE(primaryFromGap.isValid(), true);
    QCOMPARE(primaryFromGap.x(), primaryRightInside);
    QCOMPARE(primaryFromGap.y(), 10.0);
    QCOMPARE(item.containsVisiblePagePoint(
                 ImageViewport::PageRole::Primary, primaryFromGap.x(), primaryFromGap.y()),
        true);

    const CoordinateResult secondaryFromGap
        = item.nearestVisiblePagePoint(ImageViewport::PageRole::Secondary, -3.0, 10.0);
    QCOMPARE(secondaryFromGap.isValid(), true);
    QCOMPARE(secondaryFromGap.x(), 0.0);
    QCOMPARE(secondaryFromGap.y(), 10.0);
    QCOMPARE(item.containsVisiblePagePoint(
                 ImageViewport::PageRole::Secondary, secondaryFromGap.x(), secondaryFromGap.y()),
        true);

    const CoordinateResult spreadBottomRight = item.nearestVisibleSpreadPoint(44.0, 20.0);
    QCOMPARE(spreadBottomRight.isValid(), true);
    QCOMPARE(spreadBottomRight.x(), spreadRightInside);
    QCOMPARE(spreadBottomRight.y(), bottomInside);

    const CoordinateResult secondaryBottomRight
        = item.nearestVisiblePagePoint(ImageViewport::PageRole::Secondary, 30.0, 20.0);
    QCOMPARE(secondaryBottomRight.isValid(), true);
    QCOMPARE(secondaryBottomRight.x(), secondaryRightInside);
    QCOMPARE(secondaryBottomRight.y(), bottomInside);
    verifyInvalidCoordinateResult(item.pageToItem(ImageViewport::PageRole::Secondary, 30.0, 0.0));
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
    mirroredItem.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(mirroredItem);
    QCOMPARE(setFitModeCommand(mirroredItem, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(mirroredItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(mirroredItem.property("visibleImageRect").toRectF(), QRectF(8.0, 0.0, 8.0, 8.0));

    const CoordinateResult pannedLeft = mirroredItem.nearestVisibleImagePoint(0.0, 4.0);
    QCOMPARE(pannedLeft.isValid(), true);
    QCOMPARE(pannedLeft.x(), 8.0);
    QCOMPARE(pannedLeft.y(), 4.0);
    const CoordinateResult pannedRight = mirroredItem.nearestVisibleImagePoint(16.0, 4.0);
    QCOMPARE(pannedRight.isValid(), true);
    QCOMPARE(pannedRight.x(), std::nextafter(16.0, 8.0));
    QCOMPARE(pannedRight.y(), 4.0);

    QCOMPARE(setScanDirection(mirroredItem, ImageViewport::ScanDirection::Start),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorHorizontallyCommand(mirroredItem, true),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(mirroredItem.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 8.0, 8.0));
    const CoordinateResult mirroredLeft = mirroredItem.nearestVisibleImagePoint(-1.0, 4.0);
    QCOMPARE(mirroredLeft.isValid(), true);
    QCOMPARE(mirroredLeft.x(), 0.0);
    QCOMPARE(mirroredLeft.y(), 4.0);
    const CoordinateResult mirroredRight = mirroredItem.nearestVisibleImagePoint(16.0, 4.0);
    QCOMPARE(mirroredRight.isValid(), true);
    QCOMPARE(mirroredRight.x(), std::nextafter(8.0, 0.0));
    QCOMPARE(mirroredRight.y(), 4.0);

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
    QCOMPARE(rotatedItem.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(rotatedItem);
    QCOMPARE(setPageGapCommand(rotatedItem, 4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(rotatedItem, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(rotatedItem, 100.0),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(rotatedItem, 90), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(rotatedItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(rotatedItem.property("visibleSpreadRect").toRectF(), QRectF(24.0, 0.0, 20.0, 20.0));

    const CoordinateResult rotatedLeft = rotatedItem.nearestVisibleSpreadPoint(0.0, 10.0);
    QCOMPARE(rotatedLeft.isValid(), true);
    QCOMPARE(rotatedLeft.x(), 24.0);
    QCOMPARE(rotatedLeft.y(), 10.0);
    const CoordinateResult rotatedBottomRight = rotatedItem.nearestVisibleSpreadPoint(44.0, 20.0);
    QCOMPARE(rotatedBottomRight.isValid(), true);
    QCOMPARE(rotatedBottomRight.x(), std::nextafter(44.0, 24.0));
    QCOMPARE(rotatedBottomRight.y(), std::nextafter(20.0, 0.0));
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 30.0, 80.0, 40.0));
    QCOMPARE(item.state().presentation().zoomPercent(), 500.0);
    QCOMPARE(item.property("horizontalPannable").toBool(), false);
    QCOMPARE(item.property("verticalPannable").toBool(), false);

    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-60.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.state().presentation().zoomPercent(), 1250.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(60.0, 0.0));
    QCOMPARE(item.property("maximumContentPosition").toPointF(), QPointF(120.0, 0.0));
    QCOMPARE(item.property("horizontalPannable").toBool(), true);
    QCOMPARE(item.property("verticalPannable").toBool(), false);

    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-120.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(120.0, 0.0));

    QCOMPARE(setPanDelta(item, QPointF(-500.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(0.0, 0.0));

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(24.0, 42.0, 32.0, 16.0));
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
        item.setSequence(firstResult->sequence());
        acknowledgePendingRenderCommitForTest(item);
        QCOMPARE(
            setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
            ImageViewport::CommandOutcome::Accepted);
        QCOMPARE(item.fitMode(), ImageViewport::FitMode::FitHeight);
        QCOMPARE(item.zoomPercent(), 1250.0);
    };

    PageSetTransitionPolicy preservePolicy;
    preservePolicy.setZoomTransition(PageSetTransitionPolicy::ZoomTransition::Preserve);
    preservePolicy.setFitModeTransition(PageSetTransitionPolicy::FitModeTransition::SetExplicit);
    preservePolicy.setFitMode(ImageViewport::FitMode::Manual);

    ImageViewport preserveItem;
    configureItem(preserveItem);
    QCOMPARE(preserveItem.setPageSet(preserveResult->sequence(), nullptr, preservePolicy),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(preserveItem);
    QCOMPARE(preserveItem.fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(preserveItem.zoomPercent(), 1250.0);

    PageSetTransitionPolicy manualPercentPolicy = preservePolicy;
    manualPercentPolicy.setZoomTransition(
        PageSetTransitionPolicy::ZoomTransition::PreserveManualPercent);

    ImageViewport manualPercentItem;
    configureItem(manualPercentItem);
    QCOMPARE(
        manualPercentItem.setPageSet(manualPercentResult->sequence(), nullptr, manualPercentPolicy),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(manualPercentItem);
    QCOMPARE(manualPercentItem.fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(manualPercentItem.zoomPercent(), 200.0);
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(50.0, 50.0));
    QCOMPARE(setManualZoomPercentCommand(item, std::numeric_limits<double>::infinity()),
        ImageViewport::CommandOutcome::Invalid);
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    QCOMPARE(setManualZoomPercentCommand(item, 300.0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.zoomPercent(), 300.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(100.0, 100.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-100.0, -100.0, 300.0, 300.0));
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(item.metaObject(), "CommandReason", "NoCommand"));
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
    const RevisionToken displayAfterGeometry = item.displayRevision();
    QVERIFY(displayAfterGeometry.isValid());
    QCOMPARE(revisionTokenValueForTest(displayAfterGeometry), firstLargeToken);

    QCOMPARE(setManualZoomPercentCommand(item, std::numeric_limits<double>::infinity()),
        ImageViewport::CommandOutcome::Invalid);
    const RevisionToken commandAfterInvalid = item.commandRevision();
    QVERIFY(commandAfterInvalid.isValid());
    QVERIFY(commandAfterInvalid != displayAfterGeometry);
    QCOMPARE(revisionTokenValueForTest(commandAfterInvalid), firstLargeToken + 1);

    item.setSequence(result->sequence());
    const RevisionToken requestAfterAssignment = item.requestRevision();
    QVERIFY(requestAfterAssignment.isValid());
    QVERIFY(requestAfterAssignment != displayAfterGeometry);
    QVERIFY(requestAfterAssignment != commandAfterInvalid);
    QVERIFY(item.displayRevision() != requestAfterAssignment);
    QCOMPARE(revisionTokenValueForTest(requestAfterAssignment), firstLargeToken + 2);
}

void ImageViewportPresentationStateTest::invalidPageSetTransitionPreservesStateAndRevisions()
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(item, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorHorizontallyCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 12.0), ImageViewport::CommandOutcome::Accepted);

    ImageSequence* sequence = item.sequence();
    const auto fitMode = item.fitMode();
    const double zoomPercent = item.zoomPercent();
    const QPointF contentPosition = item.property("contentPosition").toPointF();
    const QPointF maximumContentPosition = item.property("maximumContentPosition").toPointF();
    const QRectF contentRect = item.property("contentRect").toRectF();
    const QRectF visibleSpreadRect = item.property("visibleSpreadRect").toRectF();
    const int rotationDegrees = item.rotationDegrees();
    const bool mirrorHorizontally = item.mirrorHorizontally();
    const auto spreadDirection = item.spreadDirection();
    const double pageGap = item.pageGap();
    const int requestStatus = item.property("requestStatus").toInt();
    const int displayStatus = item.property("displayStatus").toInt();
    const int playbackPhase = item.property("playbackPhase").toInt();
    const QString errorString = item.property("errorString").toString();
    const QString warningString = item.property("warningString").toString();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    PageSetTransitionPolicy invalidPolicy;
    invalidPolicy.setZoomTransition(PageSetTransitionPolicy::ZoomTransition::ResetToContain);
    invalidPolicy.setFitModeTransition(PageSetTransitionPolicy::FitModeTransition::SetExplicit);
    invalidPolicy.setFitMode(ImageViewport::FitMode::FitHeight);

    const auto outcome = item.setPageSet(replacementResult->sequence(), nullptr, invalidPolicy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.sequence(), sequence);
    QCOMPARE(item.fitMode(), fitMode);
    QCOMPARE(item.zoomPercent(), zoomPercent);
    QCOMPARE(item.property("contentPosition").toPointF(), contentPosition);
    QCOMPARE(item.property("maximumContentPosition").toPointF(), maximumContentPosition);
    QCOMPARE(item.property("contentRect").toRectF(), contentRect);
    QCOMPARE(item.property("visibleSpreadRect").toRectF(), visibleSpreadRect);
    QCOMPARE(item.rotationDegrees(), rotationDegrees);
    QCOMPARE(item.mirrorHorizontally(), mirrorHorizontally);
    QCOMPARE(item.spreadDirection(), spreadDirection);
    QCOMPARE(item.pageGap(), pageGap);
    QCOMPARE(item.property("requestStatus").toInt(), requestStatus);
    QCOMPARE(item.property("displayStatus").toInt(), displayStatus);
    QCOMPARE(item.property("playbackPhase").toInt(), playbackPhase);
    QCOMPARE(item.property("errorString").toString(), errorString);
    QCOMPARE(item.property("warningString").toString(), warningString);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const RevisionToken invalidSeekCommandRevision = revisionTokenProperty(item, "commandRevision");

    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", invalidSeekCommandRevision);
    QCOMPARE(commandSpy.count(), 1);

    const int rotationDegrees = item.state().presentation().rotationDegrees();
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(setRotationDegrees(item, 45), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().rotationDegrees(), rotationDegrees);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", invalidSeekCommandRevision);
    QCOMPARE(commandSpy.count(), 2);
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
    QCOMPARE(emptyItem.maximumManualZoomPercent(), displayDemandCeiling);
    emptyItem.setSize(QSizeF(80.0, 100.0));
    QCOMPARE(emptyItem.maximumManualZoomPercent(), displayDemandCeiling);

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
    loadingItem.setSequence(loadingResult->sequence());
    QCOMPARE(loadingItem.maximumManualZoomPercent(), displayDemandCeiling);

    ImageViewport readyItem;
    readyItem.setSize(QSizeF(80.0, 100.0));
    readyItem.setSequence(stillResult->sequence());
    acknowledgePendingRenderCommitForTest(readyItem);
    QCOMPARE(readyItem.maximumManualZoomPercent(), displayDemandCeiling);
    QCOMPARE(setManualZoomPercentCommand(readyItem, 200.0),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(readyItem.maximumManualZoomPercent(), displayDemandCeiling);

    readyItem.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(readyItem.maximumManualZoomPercent(), displayDemandCeiling);

    ImageViewport retainedItem;
    retainedItem.setSize(QSizeF(80.0, 100.0));
    retainedItem.setSequence(stillResult->sequence());
    acknowledgePendingRenderCommitForTest(retainedItem);
    retainedItem.setSequence(loadingResult->sequence());
    QCOMPARE(retainedItem.property("displayStatus").toInt(),
        enumValue(retainedItem.metaObject(), "DisplayStatus", "Retained"));
    QCOMPARE(retainedItem.maximumManualZoomPercent(), displayDemandCeiling);
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();
    const auto fitMode = item.state().presentation().fitMode();
    const double zoomPercent = item.state().presentation().zoomPercent();
    const QRectF contentRect = item.property("contentRect").toRectF();
    const QPointF contentPosition = item.property("contentPosition").toPointF();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);

    QCOMPARE(setManualZoomPercentCommand(item, item.maximumManualZoomPercent() + 1.0),
        ImageViewport::CommandOutcome::Invalid);

    QCOMPARE(item.state().presentation().fitMode(), fitMode);
    QCOMPARE(item.state().presentation().zoomPercent(), zoomPercent);
    QCOMPARE(item.property("contentRect").toRectF(), contentRect);
    QCOMPARE(item.property("contentPosition").toPointF(), contentPosition);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(commandSpy.count(), 1);
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.fitMode(), ImageViewport::FitMode::Contain);
    verifyClose(item.zoomPercent(), 625.0);

    ImageViewportPresentationCommand command;
    command.setZoomStepDelta(0);
    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.fitMode(), ImageViewport::FitMode::Manual);
    verifyClose(item.zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(1);
    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Accepted);
    verifyClose(item.zoomPercent(), 781.25);

    command = {};
    command.setZoomStepDelta(-1);
    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Accepted);
    verifyClose(item.zoomPercent(), 625.0);

    command = {};
    command.setZoomStepDelta(std::numeric_limits<int>::max());
    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.zoomPercent(), item.maximumManualZoomPercent());
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
    horizontalItem.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(horizontalItem);
    QCOMPARE(setManualZoomPercentCommand(horizontalItem, 200.0),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(horizontalItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

    const QPointF horizontalAnchor(50.0, 50.0);
    const CoordinateResult horizontalBefore
        = horizontalItem.itemToSpread(horizontalAnchor.x(), horizontalAnchor.y());
    QCOMPARE(horizontalBefore.isValid(), true);
    QCOMPARE(setMirrorHorizontallyCommand(horizontalItem, true),
        ImageViewport::CommandOutcome::Accepted);
    const CoordinateResult horizontalAfter
        = horizontalItem.itemToSpread(horizontalAnchor.x(), horizontalAnchor.y());
    QCOMPARE(horizontalAfter.isValid(), true);
    verifyClose(horizontalAfter.x(), horizontalBefore.x());
    verifyClose(horizontalAfter.y(), horizontalBefore.y());

    ImageViewport verticalItem;
    verticalItem.setSize(QSizeF(100.0, 100.0));
    verticalItem.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(verticalItem);
    QCOMPARE(setManualZoomPercentCommand(verticalItem, 200.0),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setScanDirection(verticalItem, ImageViewport::ScanDirection::End),
        ImageViewport::CommandOutcome::Accepted);

    const QPointF verticalAnchor(50.0, 50.0);
    const CoordinateResult verticalBefore
        = verticalItem.itemToSpread(verticalAnchor.x(), verticalAnchor.y());
    QCOMPARE(verticalBefore.isValid(), true);
    QCOMPARE(
        setMirrorVerticallyCommand(verticalItem, true), ImageViewport::CommandOutcome::Accepted);
    const CoordinateResult verticalAfter
        = verticalItem.itemToSpread(verticalAnchor.x(), verticalAnchor.y());
    QCOMPARE(verticalAfter.isValid(), true);
    verifyClose(verticalAfter.x(), verticalBefore.x());
    verifyClose(verticalAfter.y(), verticalBefore.y());
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setRotationDegrees(item, 90), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().rotationDegrees(), 90);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleSpreadRect").toRectF(), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));

    const CoordinateResult center = item.itemToSpread(50.0, 50.0);
    QCOMPARE(center.isValid(), true);
    QCOMPARE(center.x(), 5.0);
    QCOMPARE(center.y(), 10.0);

    const CoordinateResult imageCenter = item.itemToImage(50.0, 50.0);
    QCOMPARE(imageCenter.isValid(), true);
    QCOMPARE(imageCenter.x(), 5.0);
    QCOMPARE(imageCenter.y(), 10.0);
    verifyInvalidCoordinateResult(item.spreadToItem(10.0, 10.0));
}

QTEST_MAIN(ImageViewportPresentationStateTest)

#include "tst_imageviewport_presentation_state.moc"
