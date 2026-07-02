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
    void nullSequenceAssignmentPreservesCommandDiagnostic();
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

void ImageViewportStillTest::resetViewWithoutRequestClearsTransformAndCommandDiagnostic()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.setZoomPercent(200.0, QPointF()), ImageViewport::CommandOutcome::Accepted);
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

    QCOMPARE(item.property("fitMode").toInt(), enumValue(metaObject, "FitMode", "Contain"));
    QCOMPARE(item.property("zoomPercent").toDouble(), 100.0);
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

    QCOMPARE(item.property("fitMode").toInt(), enumValue(metaObject, "FitMode", "Contain"));
    QCOMPARE(item.property("zoomPercent").toDouble(), 100.0);
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
    const QMetaObject* metaObject = item.metaObject();

    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    QCOMPARE(
        item.setZoomPercent(250.0, QPointF(50.0, 50.0)), ImageViewport::CommandOutcome::Accepted);
    item.setLooping(true);
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
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.property("fitMode").toInt(), enumValue(metaObject, "FitMode", "Contain"));
    QCOMPARE(item.property("zoomPercent").toDouble(), 625.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.looping(), true);
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

    const CoordinateResult centerImage = item.itemToImage(50.0, 50.0);
    QCOMPARE(centerImage.isValid(), true);
    QCOMPARE(centerImage.x(), 8.0);
    QCOMPARE(centerImage.y(), 4.0);

    const CoordinateResult rightEdgeImage = item.itemToImage(100.0, 50.0);
    verifyInvalidCoordinateResult(rightEdgeImage);
    const CoordinateResult bottomEdgeImage = item.itemToImage(50.0, 75.0);
    verifyInvalidCoordinateResult(bottomEdgeImage);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), true);
    QCOMPARE(item.containsVisibleImagePoint(16.0, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 8.0), false);

    const CoordinateResult centerItem = item.imageToItem(8.0, 4.0);
    QCOMPARE(centerItem.isValid(), true);
    QCOMPARE(centerItem.x(), 50.0);
    QCOMPARE(centerItem.y(), 50.0);
    verifyInvalidCoordinateResult(item.imageToItem(8.0, 8.0));
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

void ImageViewportStillTest::nullSequenceAssignmentPreservesCommandDiagnostic()
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
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(commandSpy.count(), 0);
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

    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    QCOMPARE(
        item.setZoomPercent(250.0, QPointF(50.0, 50.0)), ImageViewport::CommandOutcome::Accepted);
    item.setLooping(true);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.property("fitMode").toInt(), enumValue(metaObject, "FitMode", "Manual"));
    QCOMPARE(item.property("zoomPercent").toDouble(), 250.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.looping(), true);
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
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken readyRequestRevision = revisionTokenProperty(item, "requestRevision");

    item.setSequence(replacementResult->sequence());

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
    const QMetaObject* metaObject = item.metaObject();

    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::Checkerboard);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    QCOMPARE(
        item.setZoomPercent(150.0, QPointF(50.0, 50.0)), ImageViewport::CommandOutcome::Accepted);
    item.setLooping(true);

    item.setSequence(replacementResult->sequence());

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Checkerboard);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.property("fitMode").toInt(), enumValue(metaObject, "FitMode", "Manual"));
    QCOMPARE(item.property("zoomPercent").toDouble(), 150.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF());
    QCOMPARE(item.looping(), true);
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
    const QMetaObject* metaObject = item.metaObject();

    const RevisionToken readyRequestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken readyDisplayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "requestRevision", readyRequestRevision);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
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

    QCOMPARE(
        item.setZoomPercent(200.0, QPointF(50.0, 50.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.panBy(QPointF(4.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("fitMode").toInt(), enumValue(metaObject, "FitMode", "Contain"));
    QCOMPARE(item.property("zoomPercent").toDouble(), 625.0);
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

    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    verifyInvalidCoordinateResult(item.itemToImage(infinity, 50.0));
    verifyInvalidCoordinateResult(item.itemToImage(50.0, nan));
    verifyInvalidCoordinateResult(item.imageToItem(infinity, 4.0));
    verifyInvalidCoordinateResult(item.imageToItem(8.0, nan));
    QCOMPARE(item.containsVisibleImagePoint(infinity, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, nan), false);
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
    QCOMPARE(item.setFitMode(ImageViewport::FitMode::FitHeight, QPointF(50.0, 50.0)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.panBy(QPointF(-50.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    item.setMirrorHorizontally(true);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(8.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.containsVisibleImagePoint(7.999, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), true);
    QCOMPARE(item.containsVisibleImagePoint(15.999, 4.0), true);

    const CoordinateResult leftItem = item.itemToImage(0.001, 50.0);
    QCOMPARE(leftItem.isValid(), true);
    QCOMPARE(leftItem.x(), 15.99992);

    const CoordinateResult rightHalfImage = item.imageToItem(12.0, 4.0);
    QCOMPARE(rightHalfImage.isValid(), true);
    QCOMPARE(rightHalfImage.x(), 50.0);
    verifyInvalidCoordinateResult(item.imageToItem(4.0, 4.0));
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
    QCOMPARE(item.setFitMode(ImageViewport::FitMode::FitWidth, QPointF(50.0, 50.0)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.panToEnd(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, -100.0, 100.0, 200.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 8.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(50.0, 0.0).y(), 8.0);
    QVERIFY(item.itemToImage(50.0, 99.0).y() > 15.9);
    QCOMPARE(item.containsVisibleImagePoint(4.0, 7.999), false);
    QCOMPARE(item.containsVisibleImagePoint(4.0, 8.0), true);
    QCOMPARE(item.containsVisibleImagePoint(4.0, 15.999), true);
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
    verifyInvalidCoordinateResult(item.itemToImage(0.0, 0.0));

    item.setSize(QSizeF(100.0, 100.0));
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
    verifyInvalidCoordinateResult(item.imageToItem(8.0, 4.0));
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), false);
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
    item.setLooping(true);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(350);
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

    item.advancePlaybackForTest(100);
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
    QCOMPARE(item.itemToImage(1.0, 1.0).isValid(), false);
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
