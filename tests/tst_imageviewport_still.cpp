#include "imageviewport_provider_test_support.h"

class ImageViewportStillTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportStillTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void resetViewWithoutRequestClearsTransformAndCommandDiagnostic();
    void resetViewWithoutTransformChangeOnlyClearsCommandDiagnostic();
    void resetViewPreservesNonTransformPresentationState();
    void stillImageSequenceAssignmentPublishesReadyState();
    void nullSequenceAssignmentClearsDisplayObservations();
    void nullSequenceAssignmentClearsCommandDiagnostic();
    void clearActiveRequestClearsCommandDiagnostic();
    void clearPreservesPresentationState();
    void clearReadyDisplayEmitsGeometryStateChanged();
    void clearNonPresentableDisplayDoesNotEmitGeometryStateChanged();
    void stillImageReadyReplacementIncrementsDisplayRevision();
    void stillImageReplacementPreservesPresentationState();
    void stillImageCommandsPreserveOrReplaceDocumentedState();
    void secondaryStillImagePlayReportsUnsupported();
    void secondaryStillImagePositionSeekReportsUnsupported();
    void coordinateHelpersRejectNonFiniteInputs();
    void stillImageMirroredCoverUsesMirroredVisibleImageRect();
    void stillImageCoverUsesBottomAlignmentAsCropFocus();
    void stillImageAssignmentWaitsForPositiveGeometry();
    void stillImageFactoryRejectsPublishedLimitViolations();
    void timedFrameListLoopingPlaybackWrapsToFirstFrame();
    void replacementRetainsPreviousDisplayWhileWaitingForGeometry();
};

static ImageViewport::CommandOutcome setPanDelta(ImageViewport& item, QPointF delta)
{
    ImageViewportPresentationCommand command;
    command.setPanDelta(delta);
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

static ImageViewport::CommandOutcome setMirrorCommand(
    ImageViewport& item, bool mirrorHorizontally, bool mirrorVertically)
{
    ImageViewportPresentationCommand command;
    command.setMirrorHorizontally(mirrorHorizontally);
    command.setMirrorVertically(mirrorVertically);
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

void ImageViewportStillTest::resetViewWithoutRequestClearsTransformAndCommandDiagnostic()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    const RevisionToken displayRevisionBeforeReset = revisionTokenProperty(item, "displayRevision");

    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 100.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "requestRevision").isValid());
    verifyRevisionChanged(item, "displayRevision", displayRevisionBeforeReset);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displayRevisionSpy.count(), 1);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 1);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportStillTest::resetViewWithoutTransformChangeOnlyClearsCommandDiagnostic()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "displayRevision").isValid());

    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 100.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "requestRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(displayRevisionSpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 0);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportStillTest::resetViewPreservesNonTransformPresentationState()
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 250.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.sequence(), result->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), true);
    QCOMPARE(item.state().presentation().mirrorVertically(), true);
    QCOMPARE(item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 625.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.state().presentation().looping(), true);
}

void ImageViewportStillTest::stillImageSequenceAssignmentPublishesReadyState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), 0);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    const double changedWidth = 100.0 + 5.0e-13;
    QVERIFY(changedWidth != 100.0);
    item.setSize(QSizeF(changedWidth, 100.0));
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QCOMPARE(displayRevisionSpy.count(), 1);
    QCOMPARE(geometrySpy.count(), 1);
    item.setSize(QSizeF(100.0, 100.0));

    const ImageViewportCoordinateResult centerImage = mapItemToPrimaryPage(item, 50.0, 50.0);
    QCOMPARE(centerImage.isValid(), true);
    QCOMPARE(centerImage.point().x(), 8.0);
    QCOMPARE(centerImage.point().y(), 4.0);

    const ImageViewportCoordinateResult rightEdgeImage = mapItemToPrimaryPage(item, 100.0, 50.0);
    verifyInvalidCoordinateResult(rightEdgeImage);
    const ImageViewportCoordinateResult bottomEdgeImage = mapItemToPrimaryPage(item, 50.0, 75.0);
    verifyInvalidCoordinateResult(bottomEdgeImage);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 4.0), true);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 16.0, 4.0), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 8.0), false);

    const ImageViewportCoordinateResult centerItem = mapPrimaryPageToItem(item, 8.0, 4.0);
    QCOMPARE(centerItem.isValid(), true);
    QCOMPARE(centerItem.point().x(), 50.0);
    QCOMPARE(centerItem.point().y(), 50.0);
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 8.0, 8.0));
}

void ImageViewportStillTest::nullSequenceAssignmentClearsDisplayObservations()
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
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken readyRequestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    verifyRevisionChanged(item, "requestRevision", readyRequestRevision);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    item.setSequence(providerResult->sequence());

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
}

void ImageViewportStillTest::nullSequenceAssignmentClearsCommandDiagnostic()
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    item.setSequence(nullptr);

    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(commandSpy.count(), 1);
}

void ImageViewportStillTest::clearActiveRequestClearsCommandDiagnostic()
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(commandSpy.count(), 1);
}

void ImageViewportStillTest::clearPreservesPresentationState()
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 250.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), true);
    QCOMPARE(item.state().presentation().mirrorVertically(), true);
    QCOMPARE(item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.state().presentation().zoomPercent(), 250.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.state().presentation().looping(), true);
}

void ImageViewportStillTest::clearReadyDisplayEmitsGeometryStateChanged()
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
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setSequence(nullptr);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(geometrySpy.count(), 1);
}

void ImageViewportStillTest::clearNonPresentableDisplayDoesNotEmitGeometryStateChanged()
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
    item.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setSequence(nullptr);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportStillTest::stillImageReadyReplacementIncrementsDisplayRevision()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QVERIFY(firstResult->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken readyRequestRevision = revisionTokenProperty(item, "requestRevision");

    item.setSequence(replacementResult->sequence());

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    verifyRevisionChanged(item, "requestRevision", readyRequestRevision);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
}

void ImageViewportStillTest::stillImageReplacementPreservesPresentationState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QVERIFY(firstResult->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::Checkerboard, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 150.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);

    item.setSequence(replacementResult->sequence());

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), true);
    QCOMPARE(item.state().presentation().mirrorVertically(), true);
    QCOMPARE(item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::Checkerboard);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.state().presentation().zoomPercent(), 150.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.state().presentation().looping(), true);
}

void ImageViewportStillTest::stillImageCommandsPreserveOrReplaceDocumentedState()
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
    const QMetaObject* metaObject = item.metaObject();

    const RevisionToken readyRequestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "requestRevision", readyRequestRevision);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QVERIFY(!revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    const RevisionToken afterAcceptedSeekRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), afterAcceptedSeekRequestRevision);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPanDelta(item, QPointF(4.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 625.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportStillTest::secondaryStillImagePlayReportsUnsupported()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(12, 8, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::black);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(
        item.play(ImageViewport::PageRole::Secondary), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportStillTest::secondaryStillImagePositionSeekReportsUnsupported()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(12, 8, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::black);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 0),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportStillTest::coordinateHelpersRejectNonFiniteInputs()
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

    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    verifyInvalidCoordinateResult(mapItemToPrimaryPage(item, infinity, 50.0));
    verifyInvalidCoordinateResult(mapItemToPrimaryPage(item, 50.0, nan));
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, infinity, 4.0));
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 8.0, nan));
    QCOMPARE(containsVisiblePrimaryPagePoint(item, infinity, 4.0), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, nan), false);
}

void ImageViewportStillTest::stillImageMirroredCoverUsesMirroredVisibleImageRect()
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
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPanDelta(item, QPointF(-50.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, false), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-100.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 8.0, 8.0));
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 7.999, 4.0), true);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 4.0), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 15.999, 4.0), false);

    const ImageViewportCoordinateResult leftItem = mapItemToPrimaryPage(item, 0.001, 50.0);
    QCOMPARE(leftItem.isValid(), true);
    QCOMPARE(leftItem.point().x(), 7.99992);

    const ImageViewportCoordinateResult leftHalfImage = mapPrimaryPageToItem(item, 4.0, 4.0);
    QCOMPARE(leftHalfImage.isValid(), true);
    QCOMPARE(leftHalfImage.point().x(), 50.0);
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 12.0, 4.0));
}

void ImageViewportStillTest::stillImageCoverUsesBottomAlignmentAsCropFocus()
{
    ImageSequenceFactory factory;
    QImage image(8, 16, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitWidth),
        ImageViewport::CommandOutcome::Accepted);
    ImageViewportPresentationCommand scanCommand;
    scanCommand.setScanDirection(ImageViewport::ScanDirection::End);
    QCOMPARE(item.setPresentation(scanCommand), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, -100.0, 100.0, 200.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 8.0, 8.0, 8.0));
    QCOMPARE(mapItemToPrimaryPage(item, 50.0, 0.0).point().y(), 8.0);
    QVERIFY(mapItemToPrimaryPage(item, 50.0, 99.0).point().y() > 15.9);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 4.0, 7.999), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 4.0, 8.0), true);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 4.0, 15.999), true);
}

void ImageViewportStillTest::stillImageAssignmentWaitsForPositiveGeometry()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(0.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    verifyInvalidCoordinateResult(mapItemToPrimaryPage(item, 0.0, 0.0));

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");
    item.setSize(QSizeF(0.0, 100.0));
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 8.0, 4.0));
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 4.0), false);
}

void ImageViewportStillTest::stillImageFactoryRejectsPublishedLimitViolations()
{
    ImageSequenceFactory factory;
    QImage oversized(
        ImageSequenceLimits::maximumLogicalWidth() + 1, 1, QImage::Format_ARGB32_Premultiplied);
    oversized.fill(Qt::transparent);
    ImageFrame frame(oversized);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("maximumLogicalWidth")));
}

void ImageViewportStillTest::timedFrameListLoopingPlaybackWrapsToFirstFrame()
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
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 350);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    advancePlaybackForTest(item, 100);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportStillTest::replacementRetainsPreviousDisplayWhileWaitingForGeometry()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage replacementImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(firstResult->sequence());
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    item.setSize(QSizeF(0.0, 100.0));
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    item.setSequence(replacementResult->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(mapItemToPrimaryPage(item, 1.0, 1.0).isValid(), false);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QCOMPARE(displayRevisionSpy.count(), 1);

    const RevisionToken retainedRequestRevision = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "requestRevision", retainedRequestRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    const RevisionToken retainedDisplayRevision = revisionTokenProperty(item, "displayRevision");
    const int retainedDisplayRevisionSignalCount = displayRevisionSpy.count();

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(8.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 100.0, 100.0));
    verifyRevisionChanged(item, "displayRevision", retainedDisplayRevision);
    QCOMPARE(displayRevisionSpy.count(), retainedDisplayRevisionSignalCount + 1);
}

QTEST_MAIN(ImageViewportStillTest)

#include "tst_imageviewport_still.moc"
