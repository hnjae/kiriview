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
    void exposesMinimumQmlSurface();
    void hasDocumentedDefaultState();
    void qmlImportsWithUniqueEnumKeys();
    void imageSequenceIsNotQmlCreatable();
    void imageSequenceProviderAdapterIsNotQmlCreatable();
    void exposesTypedSequenceFactorySurface();
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

void ImageViewportTest::exposesMinimumQmlSurface()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    const QList<QByteArray> properties = {
        "sequence",
        "playbackState",
        "requestedFrame",
        "requestStatus",
        "displayStatus",
        "retentionPolicy",
        "fillMode",
        "horizontalAlignment",
        "verticalAlignment",
        "contentRect",
        "visibleImageRect",
        "paintedWidth",
        "paintedHeight",
        "zoom",
        "pan",
        "smooth",
        "mipmap",
        "mirror",
        "mirrorVertically",
        "orientationPolicy",
        "backgroundMode",
        "backgroundColor",
        "colorPolicy",
    };

    for (const QByteArray &property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QList<QByteArray> enumerators = {
        "PlaybackState",
        "RequestStatus",
        "RequestStatusReason",
        "DisplayStatus",
        "LoopMode",
        "RequestOutcome",
        "FillMode",
        "HorizontalAlignment",
        "VerticalAlignment",
        "OrientationPolicy",
        "ColorPolicy",
        "BackgroundMode",
        "RetentionPolicy",
    };

    for (const QByteArray &enumerator : enumerators) {
        QVERIFY2(metaObject->indexOfEnumerator(enumerator.constData()) >= 0, enumerator.constData());
    }

    const QList<QByteArray> methods = {
        "play()",
        "pause()",
        "stop()",
        "seek(int)",
        "seekToPosition(double)",
        "clear()",
        "mapItemToImage(QPointF)",
        "mapImageToItem(QPointF)",
        "containsVisibleImagePoint(QPointF)",
    };

    for (const QByteArray &method : methods) {
        QVERIFY2(metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0, method.constData());
    }
}

void ImageViewportTest::hasDocumentedDefaultState()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("playbackState").toInt(), enumValue(metaObject, "PlaybackState", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toDouble(), -1.0);
    QCOMPARE(item.property("displayedPosition").toDouble(), -1.0);
    QCOMPARE(item.property("speed").toDouble(), 1.0);
    QCOMPARE(item.property("loopMode").toInt(), enumValue(metaObject, "LoopMode", "SourceLoop"));
    QCOMPARE(item.property("loopCount").toInt(), 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestStatusReason").toInt(), enumValue(metaObject, "RequestStatusReason", "None"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("retentionPolicy").toInt(), enumValue(metaObject, "RetentionPolicy", "RetainThroughFailure"));
    QCOMPARE(item.property("fillMode").toInt(), enumValue(metaObject, "FillMode", "PreserveAspectFit"));
    QCOMPARE(item.property("horizontalAlignment").toInt(), enumValue(metaObject, "HorizontalAlignment", "AlignHCenter"));
    QCOMPARE(item.property("verticalAlignment").toInt(), enumValue(metaObject, "VerticalAlignment", "AlignVCenter"));
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("smooth").toBool(), true);
    QCOMPARE(item.property("mipmap").toBool(), false);
    QCOMPARE(item.property("mirror").toBool(), false);
    QCOMPARE(item.property("mirrorVertically").toBool(), false);
    QCOMPARE(item.property("orientationPolicy").toInt(), enumValue(metaObject, "OrientationPolicy", "ApplyOrientationBestEffort"));
    QCOMPARE(item.property("backgroundMode").toInt(), enumValue(metaObject, "BackgroundMode", "Transparent"));
    QCOMPARE(item.property("colorPolicy").toInt(), enumValue(metaObject, "ColorPolicy", "AssumeSrgbColor"));
}

void ImageViewportTest::qmlImportsWithUniqueEnumKeys()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    property int noRequest: ImageViewport.NoRequest
    property int requestUnsupported: ImageViewport.RequestUnsupported
    property int outcomeAccepted: ImageViewport.OutcomeAccepted
    property int outcomeUnsupported: ImageViewport.OutcomeUnsupported
    property int outcomeInvalid: ImageViewport.OutcomeInvalid
    property int preserveOrientation: ImageViewport.PreserveOrientationMetadata
    property int preserveColor: ImageViewport.PreserveColorMetadata
    property bool factoryReturnsNull: ImageSequenceFactory.fromImage(null) === null
    property bool mappingInvalid: mapItemToImage(Qt.point(1, 1)).valid === false
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(object->property("noRequest").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(object->property("requestUnsupported").toInt(), enumValue(metaObject, "RequestStatus", "RequestUnsupported"));
    QCOMPARE(object->property("outcomeAccepted").toInt(), enumValue(metaObject, "RequestOutcome", "OutcomeAccepted"));
    QCOMPARE(object->property("outcomeUnsupported").toInt(), enumValue(metaObject, "RequestOutcome", "OutcomeUnsupported"));
    QCOMPARE(object->property("outcomeInvalid").toInt(), enumValue(metaObject, "RequestOutcome", "OutcomeInvalid"));
    QCOMPARE(object->property("preserveOrientation").toInt(), enumValue(metaObject, "OrientationPolicy", "PreserveOrientationMetadata"));
    QCOMPARE(object->property("preserveColor").toInt(), enumValue(metaObject, "ColorPolicy", "PreserveColorMetadata"));
    QCOMPARE(object->property("factoryReturnsNull").toBool(), true);
    QCOMPARE(object->property("mappingInvalid").toBool(), true);
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

    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromImage(ImageFrame*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromFrames(QList<TimedImageFrame*>)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromProvider(ImageSequenceProviderAdapter*)")) >= 0);
}

void ImageViewportTest::presentationChangesNotifyGeometryState()
{
    ImageViewport item;
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setZoom(2.0);
    QCOMPARE(geometrySpy.count(), 1);

    item.setPan(QPointF(4.0, 8.0));
    QCOMPARE(geometrySpy.count(), 2);

    item.setFillMode(ImageViewport::Stretch);
    QCOMPARE(geometrySpy.count(), 3);

    item.setMirror(true);
    QCOMPARE(geometrySpy.count(), 4);
}

QTEST_MAIN(ImageViewportTest)

#include "tst_imageviewport.moc"
