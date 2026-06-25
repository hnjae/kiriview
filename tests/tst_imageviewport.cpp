#include "imageviewport.h"

#include <QtCore/QMetaEnum>
#include <QtCore/QPointF>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

class ImageViewportTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructsAsQuickItem();
    void doesNotExposeSourceProperty();
    void exposesDocumentedQmlSurface();
    void hasDocumentedDefaultState();
    void qmlImportsDocumentedSurface();
    void imageSequenceIsNotQmlCreatable();
    void imageSequenceProviderAdapterIsNotQmlCreatable();
    void exposesTypedSequenceFactorySurface();
    void exposesImageSequenceLimits();
    void commandsWithoutRequestAreIgnoredDiagnostics();
    void presentationChangesNotifyGeometryState();
};

namespace {

QString componentErrors(const QQmlComponent &component)
{
    QStringList messages;
    for (const QQmlError &error : component.errors()) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

int enumValue(const QMetaObject *metaObject, const char *enumName, const char *key)
{
    const int index = metaObject->indexOfEnumerator(enumName);
    if (index < 0) {
        return -1;
    }
    return metaObject->enumerator(index).keyToValue(key);
}

}

void ImageViewportTest::defaultConstructsAsQuickItem()
{
    ImageViewport item;

    QVERIFY(item.flags().testFlag(QQuickItem::ItemHasContents));
}

void ImageViewportTest::doesNotExposeSourceProperty()
{
    ImageViewport item;

    QCOMPARE(item.metaObject()->indexOfProperty("source"), -1);
}

void ImageViewportTest::exposesDocumentedQmlSurface()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    const QList<QByteArray> properties = {
        "sequence",
        "requestStatus",
        "requestReason",
        "commandReason",
        "displayStatus",
        "playbackPhase",
        "displayedFrame",
        "requestedFrame",
        "displayedPosition",
        "requestedPosition",
        "frameCount",
        "totalDuration",
        "frameSeekBounds",
        "positionSeekBounds",
        "timedPlaybackSupport",
        "frameSeekSupport",
        "positionSeekSupport",
        "displayedImageSize",
        "fillMode",
        "horizontalAlignment",
        "verticalAlignment",
        "contentRect",
        "visibleImageRect",
        "displayRevision",
        "requestRevision",
        "commandRevision",
        "errorString",
        "warningString",
        "zoom",
        "pan",
        "smoothing",
        "mipmap",
        "mirrorHorizontally",
        "mirrorVertically",
        "backgroundMode",
        "backgroundColor",
        "looping",
    };

    for (const QByteArray &property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QList<QByteArray> enumerators = {
        "RequestStatus",
        "RequestReason",
        "CommandReason",
        "DisplayStatus",
        "PlaybackPhase",
        "TriState",
        "RequestOutcome",
        "FillMode",
        "HorizontalAlignment",
        "VerticalAlignment",
        "BackgroundMode",
    };

    for (const QByteArray &enumerator : enumerators) {
        QVERIFY2(metaObject->indexOfEnumerator(enumerator.constData()) >= 0, enumerator.constData());
    }

    const QList<QByteArray> methods = {
        "play()",
        "pause()",
        "stop()",
        "seek(int)",
        "seekToPosition(int)",
        "clear()",
        "resetView()",
        "itemToImage(double,double)",
        "imageToItem(double,double)",
        "containsVisibleImagePoint(double,double)",
    };

    for (const QByteArray &method : methods) {
        QVERIFY2(metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0, method.constData());
    }
}

void ImageViewportTest::hasDocumentedDefaultState()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("sequence").value<QObject *>(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(item.property("displayRevision").toUInt(), 0U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QCOMPARE(item.property("commandRevision").toUInt(), 0U);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(item.property("fillMode").toInt(), enumValue(metaObject, "FillMode", "Contain"));
    QCOMPARE(item.property("horizontalAlignment").toInt(), enumValue(metaObject, "HorizontalAlignment", "AlignHCenter"));
    QCOMPARE(item.property("verticalAlignment").toInt(), enumValue(metaObject, "VerticalAlignment", "AlignVCenter"));
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("smoothing").toBool(), true);
    QCOMPARE(item.property("mipmap").toBool(), false);
    QCOMPARE(item.property("mirrorHorizontally").toBool(), false);
    QCOMPARE(item.property("mirrorVertically").toBool(), false);
    QCOMPARE(item.property("backgroundMode").toInt(), enumValue(metaObject, "BackgroundMode", "Transparent"));
    QCOMPARE(item.property("backgroundColor").value<QColor>(), QColor(Qt::transparent));
    QCOMPARE(item.property("looping").toBool(), false);
}

void ImageViewportTest::qmlImportsDocumentedSurface()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    property int noRequest: ImageViewport.RequestStatus.NoRequest
    property int loading: ImageViewport.RequestStatus.Loading
    property int retained: ImageViewport.DisplayStatus.Retained
    property int waiting: ImageViewport.PlaybackPhase.Waiting
    property int accepted: ImageViewport.RequestOutcome.Accepted
    property int unsupported: ImageViewport.RequestOutcome.Unsupported
    property int invalid: ImageViewport.RequestOutcome.Invalid
    property int ignoredNoRequest: ImageViewport.RequestOutcome.IgnoredNoRequest
    property int cover: ImageViewport.FillMode.Cover
    property int center: ImageViewport.FillMode.Center
    property bool factoryReturnsNull: ImageSequenceFactory.fromFrame(null).sequence === null
    property bool mappingInvalid: itemToImage(1, 1).valid === false
    property bool mappingHasFlatFields: imageToItem(1, 1).x === 0 && imageToItem(1, 1).y === 0
    property bool limitsAvailable: ImageSequenceLimits.maximumLogicalWidth >= 8192
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(object->property("noRequest").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(object->property("loading").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(object->property("retained").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(object->property("waiting").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(object->property("accepted").toInt(), enumValue(metaObject, "RequestOutcome", "Accepted"));
    QCOMPARE(object->property("unsupported").toInt(), enumValue(metaObject, "RequestOutcome", "Unsupported"));
    QCOMPARE(object->property("invalid").toInt(), enumValue(metaObject, "RequestOutcome", "Invalid"));
    QCOMPARE(object->property("ignoredNoRequest").toInt(), enumValue(metaObject, "RequestOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("cover").toInt(), enumValue(metaObject, "FillMode", "Cover"));
    QCOMPARE(object->property("center").toInt(), enumValue(metaObject, "FillMode", "Center"));
    QCOMPARE(object->property("factoryReturnsNull").toBool(), true);
    QCOMPARE(object->property("mappingInvalid").toBool(), true);
    QCOMPARE(object->property("mappingHasFlatFields").toBool(), true);
    QCOMPARE(object->property("limitsAvailable").toBool(), true);
}

void ImageViewportTest::imageSequenceIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequence {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportTest::imageSequenceProviderAdapterIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequenceProviderAdapter {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportTest::exposesTypedSequenceFactorySurface()
{
    ImageSequenceFactory factory;
    const QMetaObject *metaObject = factory.metaObject();

    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromFrame(ImageFrame*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromTimedFrameList(TimedImageFrameList*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromProvider(ImageSequenceProviderAdapter*)")) >= 0);

    QObject *result = factory.fromFrame(nullptr);
    QVERIFY(result);
    QCOMPARE(result->property("sequence").value<QObject *>(), nullptr);
    QCOMPARE(result->property("outcome").toInt(), enumValue(result->metaObject(), "FactoryOutcome", "Invalid"));
    QVERIFY(!result->property("errorString").toString().isEmpty());
}

void ImageViewportTest::exposesImageSequenceLimits()
{
    ImageSequenceLimits limits;

    QCOMPARE(limits.property("maximumLogicalWidth").toInt(), ImageSequenceLimits::maximumLogicalWidth());
    QVERIFY(limits.property("maximumLogicalWidth").toInt() >= 8192);
    QVERIFY(limits.property("maximumLogicalHeight").toInt() >= 8192);
    QVERIFY(limits.property("maximumPixelsPerFrame").toLongLong() >= 67108864LL);
    QVERIFY(limits.property("maximumPayloadBytesPerFrame").toLongLong() >= 268435456LL);
    QVERIFY(limits.property("maximumTimedListFrameCount").toInt() >= 10000);
    QVERIFY(limits.property("maximumFrameDuration").toInt() >= 86400000);
    QVERIFY(limits.property("maximumTotalSequenceDuration").toInt() >= 86400000);
    QVERIFY(limits.property("maximumDiagnosticStringLength").toInt() >= 4096);
}

void ImageViewportTest::commandsWithoutRequestAreIgnoredDiagnostics()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::RequestOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));

    QCOMPARE(item.seek(-1), ImageViewport::RequestOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
}

void ImageViewportTest::presentationChangesNotifyGeometryState()
{
    ImageViewport item;
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

QTEST_MAIN(ImageViewportTest)

#include "tst_imageviewport.moc"
