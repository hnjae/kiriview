#include "imageviewport_provider_test_support.h"

#include <QtCore/QElapsedTimer>

class ImageViewportPresentationStateTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportPresentationStateTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void invalidPresentationEnumValuesAreIgnored();
    void invalidPresentationTransformsAreIgnored();
    void presentationZoomUsesExactValueChanges();
    void presentationPanUsesExactValueChanges();
    void presentationChangesWithoutDisplayDoNotNotifyGeometryState();
    void backgroundPresentationDoesNotChangeRequestOrPlayback();
    void qualityPresentationDoesNotChangeRequestGeometryOrPlayback();
    void loopingDoesNotChangeRequestDisplayOrGeometry();
    void presentationChangesNotifyGeometryState();
    void twoPageSpreadGeometryUsesDirectionAndGap();
    void spreadCoordinateHelpersRejectGapAndEdges();
    void fitModesExposeZoomAndPannability();
    void presentationCommandsUpdateCommandDiagnostics();
    void manualZoomAbovePublishedLimitIsInvalid();
    void rotationAffectsSpreadMapping();
};

void ImageViewportPresentationStateTest::invalidPresentationEnumValuesAreIgnored()
{
    ImageViewport item;
    const RevisionToken initialDisplayRevision = revisionTokenProperty(item, "displayRevision");

    QVERIFY(item.setProperty("fillMode", 999));
    QVERIFY(item.setProperty("horizontalAlignment", 999));
    QVERIFY(item.setProperty("verticalAlignment", 999));
    QVERIFY(item.setProperty("backgroundMode", 999));

    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Contain);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignHCenter);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignVCenter);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Transparent);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), initialDisplayRevision);
}

void ImageViewportPresentationStateTest::invalidPresentationTransformsAreIgnored()
{
    ImageViewport item;
    item.setZoom(2.0);
    item.setPan(QPointF(3.0, 4.0));
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setZoom(0.0);
    item.setZoom(-1.0);
    item.setZoom(std::numeric_limits<double>::infinity());
    item.setZoom(std::numeric_limits<double>::quiet_NaN());
    item.setPan(QPointF(std::numeric_limits<double>::infinity(), 0.0));
    item.setPan(QPointF(0.0, std::numeric_limits<double>::quiet_NaN()));

    QCOMPARE(item.zoom(), 2.0);
    QCOMPARE(item.pan(), QPointF(3.0, 4.0));
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(displayRevisionSpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportPresentationStateTest::presentationZoomUsesExactValueChanges()
{
    ImageViewport item;
    const double changedZoom = 1.0 + 5.0e-13;
    QVERIFY(changedZoom != 1.0);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);

    item.setZoom(changedZoom);

    QCOMPARE(item.zoom(), changedZoom);
    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(presentationSpy.count(), 1);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.zoom(), 1.0);
    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(presentationSpy.count(), 2);
}

void ImageViewportPresentationStateTest::presentationPanUsesExactValueChanges()
{
    ImageViewport item;
    const QPointF changedPan(5.0e-13, -5.0e-13);
    QVERIFY(changedPan.x() != 0.0);
    QVERIFY(changedPan.y() != 0.0);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);

    item.setPan(changedPan);

    QCOMPARE(item.pan(), changedPan);
    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(presentationSpy.count(), 1);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.pan(), QPointF());
    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(presentationSpy.count(), 2);
}

void ImageViewportPresentationStateTest::presentationChangesWithoutDisplayDoNotNotifyGeometryState()
{
    ImageViewport item;
    const RevisionToken initialDisplayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);

    item.setZoom(2.0);
    item.setPan(QPointF(4.0, 8.0));
    item.setFillMode(ImageViewport::FillMode::Stretch);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    item.setVerticalAlignment(ImageViewport::VerticalAlignment::AlignTop);
    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(Qt::red);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    verifyRevisionChanged(item, "displayRevision", initialDisplayRevision);
    QCOMPARE(displayRevisionSpy.count(), 11);
    QCOMPARE(presentationSpy.count(), 11);
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

    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));

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

    item.setSmoothing(false);
    item.setMipmap(true);

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

    item.setLooping(true);

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
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setZoom(2.0);
    QCOMPARE(geometrySpy.count(), 1);

    item.setPan(QPointF(4.0, 8.0));
    QCOMPARE(geometrySpy.count(), 2);

    item.setFillMode(ImageViewport::FillMode::Stretch);
    QCOMPARE(geometrySpy.count(), 3);

    item.setMirrorHorizontally(true);
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

    QCOMPARE(item.setPageGap(4.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("displayedSpreadSize").toSizeF(), QSizeF(44.0, 20.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 2.0, 88.0, 40.0));
    QCOMPARE(item.property("primaryPageRect").toRectF(), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF(14.0, 0.0, 30.0, 20.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(0.0, 2.0, 20.0, 40.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(28.0, 2.0, 60.0, 40.0));

    QCOMPARE(item.setSpreadDirection(ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("primaryPageRect").toRectF(), QRectF(34.0, 0.0, 10.0, 20.0));
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF(0.0, 0.0, 30.0, 20.0));
    QCOMPARE(item.property("primaryItemRect").toRectF(), QRectF(68.0, 2.0, 20.0, 40.0));
    QCOMPARE(item.property("secondaryItemRect").toRectF(), QRectF(0.0, 2.0, 60.0, 40.0));
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
    QCOMPARE(item.setPageGap(4.0), ImageViewport::CommandOutcome::Accepted);

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
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 30.0, 80.0, 40.0));
    QCOMPARE(item.property("zoomPercent").toDouble(), 500.0);
    QCOMPARE(item.property("horizontalPannable").toBool(), false);
    QCOMPARE(item.property("verticalPannable").toBool(), false);

    QCOMPARE(item.setFitMode(ImageViewport::FitMode::FitHeight, QPointF(40.0, 50.0)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-60.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("zoomPercent").toDouble(), 1250.0);
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(60.0, 0.0));
    QCOMPARE(item.property("maximumContentPosition").toPointF(), QPointF(120.0, 0.0));
    QCOMPARE(item.property("horizontalPannable").toBool(), true);
    QCOMPARE(item.property("verticalPannable").toBool(), false);

    QCOMPARE(item.panToEnd(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-120.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(120.0, 0.0));

    QCOMPARE(item.panBy(QPointF(-500.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("contentPosition").toPointF(), QPointF(0.0, 0.0));

    QCOMPARE(
        item.setZoomPercent(200.0, QPointF(40.0, 50.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("fitMode").toInt(), enumValue(item.metaObject(), "FitMode", "Manual"));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(24.0, 42.0, 32.0, 16.0));
    QCOMPARE(item.property("zoomPercent").toDouble(), 200.0);
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const RevisionToken invalidSeekCommandRevision = revisionTokenProperty(item, "commandRevision");

    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QCOMPARE(item.setFitMode(ImageViewport::FitMode::FitHeight, QPointF(40.0, 50.0)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", invalidSeekCommandRevision);
    QCOMPARE(commandSpy.count(), 1);

    const int rotationDegrees = item.property("rotationDegrees").toInt();
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(item.rotateClockwise(QPointF(std::numeric_limits<double>::infinity(), 0.0)),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("rotationDegrees").toInt(), rotationDegrees);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", invalidSeekCommandRevision);
    QCOMPARE(commandSpy.count(), 2);
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
    QCOMPARE(
        item.setZoomPercent(200.0, QPointF(40.0, 50.0)), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();
    const int fitMode = item.property("fitMode").toInt();
    const double zoomPercent = item.property("zoomPercent").toDouble();
    const QRectF contentRect = item.property("contentRect").toRectF();
    const QPointF contentPosition = item.property("contentPosition").toPointF();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);

    QCOMPARE(item.setZoomPercent(
                 ImageViewportDisplayLimits::maximumManualZoomPercent() + 1.0, QPointF(40.0, 50.0)),
        ImageViewport::CommandOutcome::Invalid);

    QCOMPARE(item.property("fitMode").toInt(), fitMode);
    QCOMPARE(item.property("zoomPercent").toDouble(), zoomPercent);
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
    QCOMPARE(item.rotateClockwise(QPointF(50.0, 50.0)), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("rotationDegrees").toInt(), 90);
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
