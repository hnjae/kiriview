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
    void stillImageFillModesAndMirroringUseDocumentedGeometry();
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

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);

    item.setZoom(2.0);
    item.setPan(QPointF(4.0, -3.0));
    const uint displayRevisionBeforeReset = item.property("displayRevision").toUInt();

    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.zoom(), 1.0);
    QCOMPARE(item.pan(), QPointF());
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QVERIFY(item.property("displayRevision").toUInt() > displayRevisionBeforeReset);
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
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);
    QCOMPARE(item.property("displayRevision").toUInt(), 0U);

    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.zoom(), 1.0);
    QCOMPARE(item.pan(), QPointF());
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QCOMPARE(item.property("displayRevision").toUInt(), 0U);
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

    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    item.setVerticalAlignment(ImageViewport::VerticalAlignment::AlignTop);
    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setZoom(2.5);
    item.setPan(QPointF(12.0, -6.0));
    item.setLooping(true);
    const uint requestRevision = item.property("requestRevision").toUInt();

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
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Cover);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignLeft);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignTop);
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.zoom(), 1.0);
    QCOMPARE(item.pan(), QPointF());
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
    QCOMPARE(item.property("frameSeekBounds").toMap().value(QStringLiteral("minimum")).toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value(QStringLiteral("maximum")).toInt(), 0);
    QCOMPARE(
        item.property("positionSeekBounds").toMap().value(QStringLiteral("minimum")).toInt(), -1);
    QCOMPARE(
        item.property("positionSeekBounds").toMap().value(QStringLiteral("maximum")).toInt(), -1);
    QCOMPARE(
        item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(
        item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    const double changedWidth = 100.0 + 5.0e-13;
    QVERIFY(changedWidth != 100.0);
    item.setSize(QSizeF(changedWidth, 100.0));
    QCOMPARE(item.property("displayRevision").toUInt(), readyDisplayRevision + 1U);
    QCOMPARE(displayRevisionSpy.count(), 1);
    QCOMPARE(geometrySpy.count(), 1);
    item.setSize(QSizeF(100.0, 100.0));

    const QVariantMap centerImage = item.itemToImage(50.0, 50.0);
    QCOMPARE(centerImage.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(centerImage.value(QStringLiteral("x")).toDouble(), 8.0);
    QCOMPARE(centerImage.value(QStringLiteral("y")).toDouble(), 4.0);

    const QVariantMap rightEdgeImage = item.itemToImage(100.0, 50.0);
    verifyInvalidCoordinateResult(rightEdgeImage);
    const QVariantMap bottomEdgeImage = item.itemToImage(50.0, 75.0);
    verifyInvalidCoordinateResult(bottomEdgeImage);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), true);
    QCOMPARE(item.containsVisibleImagePoint(16.0, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 8.0), false);

    const QVariantMap centerItem = item.imageToItem(8.0, 4.0);
    QCOMPARE(centerItem.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(centerItem.value(QStringLiteral("x")).toDouble(), 50.0);
    QCOMPARE(centerItem.value(QStringLiteral("y")).toDouble(), 50.0);
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
    const uint readyRequestRevision = item.property("requestRevision").toUInt();
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();

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
    QVERIFY(item.property("requestRevision").toUInt() > readyRequestRevision);
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);

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
    const uint commandRevision = item.property("commandRevision").toUInt();

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
    QCOMPARE(item.property("commandRevision").toUInt(), commandRevision);
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
    const uint commandRevision = item.property("commandRevision").toUInt();

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
    QCOMPARE(item.property("commandRevision").toUInt(), commandRevision + 1);
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

    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignRight);
    item.setVerticalAlignment(ImageViewport::VerticalAlignment::AlignBottom);
    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setZoom(2.5);
    item.setPan(QPointF(12.0, -6.0));
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
    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Cover);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignRight);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignBottom);
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.zoom(), 2.5);
    QCOMPARE(item.pan(), QPointF(12.0, -6.0));
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
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    const uint readyRequestRevision = item.property("requestRevision").toUInt();

    item.setSequence(replacementResult->sequence());

    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QVERIFY(item.property("requestRevision").toUInt() > readyRequestRevision);
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
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

    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    item.setVerticalAlignment(ImageViewport::VerticalAlignment::AlignTop);
    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::Checkerboard);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setZoom(1.5);
    item.setPan(QPointF(-8.0, 6.0));
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
    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Cover);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignLeft);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignTop);
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Checkerboard);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.zoom(), 1.5);
    QCOMPARE(item.pan(), QPointF(-8.0, 6.0));
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

    const uint readyRequestRevision = item.property("requestRevision").toUInt();
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > readyRequestRevision);
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
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
    QCOMPARE(item.property("commandRevision").toUInt(), 0U);
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

    const uint afterAcceptedSeekRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);
    QCOMPARE(item.property("requestRevision").toUInt(), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
    QCOMPARE(item.property("requestRevision").toUInt(), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 3U);
    QCOMPARE(item.property("requestRevision").toUInt(), afterAcceptedSeekRequestRevision);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 4U);

    item.setZoom(2.0);
    item.setPan(QPointF(4.0, 8.0));
    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), 5U);
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
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();

    QCOMPARE(item.play(ImageViewport::PageRole::Secondary),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
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
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 0),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportStillTest::stillImageFillModesAndMirroringUseDocumentedGeometry()
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

    item.setFillMode(ImageViewport::FillMode::Cover);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-50.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(4.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(0.0, 50.0).value(QStringLiteral("x")).toDouble(), 4.0);
    QCOMPARE(item.itemToImage(99.0, 50.0).value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(item.containsVisibleImagePoint(3.999, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(12.0, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(11.999, 4.0), true);
    QCOMPARE(item.imageToItem(4.0, 4.0).value(QStringLiteral("x")).toDouble(), 0.0);
    verifyInvalidCoordinateResult(item.imageToItem(12.0, 4.0));

    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(99.0, 50.0).value(QStringLiteral("x")).toDouble(), 7.92);

    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignRight);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-100.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(8.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(0.0, 50.0).value(QStringLiteral("x")).toDouble(), 8.0);
    QVERIFY(item.itemToImage(99.0, 50.0).value(QStringLiteral("x")).toDouble() > 15.9);

    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignHCenter);
    item.setFillMode(ImageViewport::FillMode::Stretch);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 100.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(item.itemToImage(50.0, 50.0).value(QStringLiteral("x")).toDouble(), 8.0);
    QCOMPARE(item.itemToImage(50.0, 50.0).value(QStringLiteral("y")).toDouble(), 4.0);

    item.setFillMode(ImageViewport::FillMode::Center);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignHCenter);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(42.0, 46.0, 16.0, 8.0));
    QCOMPARE(item.itemToImage(42.0, 46.0).value(QStringLiteral("valid")).toBool(), true);
    verifyInvalidCoordinateResult(item.itemToImage(58.0, 50.0));

    item.setMirrorHorizontally(true);
    verifyInvalidCoordinateResult(item.itemToImage(42.0, 50.0));
    verifyInvalidCoordinateResult(item.itemToImage(58.0, 50.0));
    const QVariantMap horizontallyMirrored = item.itemToImage(57.999, 50.0);
    QCOMPARE(horizontallyMirrored.value(QStringLiteral("valid")).toBool(), true);
    QVERIFY(qAbs(horizontallyMirrored.value(QStringLiteral("x")).toDouble() - 0.001) < 0.000001);

    item.setMirrorVertically(true);
    const QVariantMap mirrored = item.itemToImage(42.001, 46.001);
    QCOMPARE(mirrored.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(mirrored.value(QStringLiteral("x")).toDouble(), 15.999);
    QCOMPARE(mirrored.value(QStringLiteral("y")).toDouble(), 7.999);

    const QVariantMap mirroredItem = item.imageToItem(15.999, 7.999);
    QCOMPARE(mirroredItem.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(mirroredItem.value(QStringLiteral("x")).toDouble(), 42.001);
    QCOMPARE(mirroredItem.value(QStringLiteral("y")).toDouble(), 46.001);

    const QVariantMap mirroredOriginItem = item.imageToItem(0.0, 0.0);
    QCOMPARE(mirroredOriginItem.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(mirroredOriginItem.value(QStringLiteral("x")).toDouble(), 58.0);
    QCOMPARE(mirroredOriginItem.value(QStringLiteral("y")).toDouble(), 54.0);
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
    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    item.setMirrorHorizontally(true);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(8.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.containsVisibleImagePoint(7.999, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), true);
    QCOMPARE(item.containsVisibleImagePoint(15.999, 4.0), true);

    const QVariantMap leftItem = item.itemToImage(0.001, 50.0);
    QCOMPARE(leftItem.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(leftItem.value(QStringLiteral("x")).toDouble(), 15.99992);

    const QVariantMap rightHalfImage = item.imageToItem(12.0, 4.0);
    QCOMPARE(rightHalfImage.value(QStringLiteral("valid")).toBool(), true);
    QCOMPARE(rightHalfImage.value(QStringLiteral("x")).toDouble(), 50.0);
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
    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setVerticalAlignment(ImageViewport::VerticalAlignment::AlignBottom);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, -100.0, 100.0, 200.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 8.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(50.0, 0.0).value(QStringLiteral("y")).toDouble(), 8.0);
    QVERIFY(item.itemToImage(50.0, 99.0).value(QStringLiteral("y")).toDouble() > 15.9);
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

    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    item.setSize(QSizeF(0.0, 100.0));
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
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
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
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
    QCOMPARE(item.itemToImage(1.0, 1.0).value(QStringLiteral("valid")).toBool(), false);
    QCOMPARE(item.property("displayRevision").toUInt(), readyDisplayRevision + 1U);
    QCOMPARE(displayRevisionSpy.count(), 1);

    const uint retainedRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > retainedRequestRevision);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    const uint retainedDisplayRevision = item.property("displayRevision").toUInt();
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
    QCOMPARE(item.property("displayRevision").toUInt(), retainedDisplayRevision + 1U);
    QCOMPARE(displayRevisionSpy.count(), retainedDisplayRevisionSignalCount + 1);
}

QTEST_MAIN(ImageViewportStillTest)

#include "tst_imageviewport_still.moc"
