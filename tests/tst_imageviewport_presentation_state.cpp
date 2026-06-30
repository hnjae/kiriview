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
};

void ImageViewportPresentationStateTest::invalidPresentationEnumValuesAreIgnored()
{
    ImageViewport item;
    const uint initialDisplayRevision = item.property("displayRevision").toUInt();

    QVERIFY(item.setProperty("fillMode", 999));
    QVERIFY(item.setProperty("horizontalAlignment", 999));
    QVERIFY(item.setProperty("verticalAlignment", 999));
    QVERIFY(item.setProperty("backgroundMode", 999));

    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Contain);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignHCenter);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignVCenter);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Transparent);
    QCOMPARE(item.property("displayRevision").toUInt(), initialDisplayRevision);
}

void ImageViewportPresentationStateTest::invalidPresentationTransformsAreIgnored()
{
    ImageViewport item;
    item.setZoom(2.0);
    item.setPan(QPointF(3.0, 4.0));
    const uint displayRevision = item.property("displayRevision").toUInt();

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
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
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
    QCOMPARE(item.property("displayRevision").toUInt(), 1U);
    QCOMPARE(presentationSpy.count(), 1);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.zoom(), 1.0);
    QCOMPARE(item.property("displayRevision").toUInt(), 2U);
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
    QCOMPARE(item.property("displayRevision").toUInt(), 1U);
    QCOMPARE(presentationSpy.count(), 1);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.pan(), QPointF());
    QCOMPARE(item.property("displayRevision").toUInt(), 2U);
    QCOMPARE(presentationSpy.count(), 2);
}

void ImageViewportPresentationStateTest::presentationChangesWithoutDisplayDoNotNotifyGeometryState()
{
    ImageViewport item;
    const uint initialDisplayRevision = item.property("displayRevision").toUInt();
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
    QCOMPARE(item.property("displayRevision").toUInt(), initialDisplayRevision + 11U);
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

    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();
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
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision + 2U);
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

    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();
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
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision + 2U);
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

    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();
    const uint commandRevision = item.property("commandRevision").toUInt();
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
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
    QCOMPARE(item.property("commandRevision").toUInt(), commandRevision);
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

QTEST_MAIN(ImageViewportPresentationStateTest)

#include "tst_imageviewport_presentation_state.moc"
