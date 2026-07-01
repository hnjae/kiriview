#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

class ImageViewportPublicApiTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportPublicApiTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void defaultConstructsAsQuickItem();
    void doesNotExposeOutOfScopePublicState();
    void unsupportedSequencePropertyWritesPreserveState();
    void sequenceAssignmentPreservesCommandDiagnostic();
    void qmlUnsupportedSequenceAssignmentsPreserveState();
    void qmlUnsupportedSequenceAssignmentsPreserveReadyState();
    void exposesDocumentedQmlSurface();
    void exposesFinalApiScaffold();
    void hasDocumentedDefaultState();
    void qmlFinalApiScaffoldDefaultsAndCommands();
    void setPageSetAcceptsPrimaryAndSecondaryAtomically();
    void compatibilitySequenceAssignmentClearsSecondaryRole();
    void clearStylePageSetWithSecondaryClearsAcceptedRoles();
    void invalidPageSetSecondaryPreservesAcceptedRoles();
    void pageSetTransitionClearBeforeLoadClearsRetainedDisplay();
    void invalidPageSetTransitionPolicyPreservesState();
    void invalidClearStyleTransitionPolicyPreservesState();
    void secondaryProviderPageSetOpensRoleLocalSessionAndWaits();
    void secondaryProviderMetadataUpdatesRoleObservations();
    void emptyGeometryChangeIncrementsDisplayRevision();
    void qmlImportsDocumentedSurface();
    void qmlReadyValuesExposeDocumentedFields();
    void qmlCommandsReturnDocumentedOutcomes();
    void qmlFactoryFailuresReturnDocumentedDiagnostics();
    void imageSequenceIsNotQmlCreatable();
    void imageFrameIsNotQmlCreatable();
    void imageSequenceProviderFrameHandleIsNotQmlCreatable();
    void imageSequenceProviderAdapterIsNotQmlCreatable();
    void imageSequenceFactoryResultIsNotQmlCreatable();
    void imageSequenceFactoryIsQmlSingletonOnly();
    void imageSequenceLimitsIsQmlSingletonOnly();
};

void ImageViewportPublicApiTest::defaultConstructsAsQuickItem()
{
    ImageViewport item;

    QVERIFY(item.flags().testFlag(QQuickItem::ItemHasContents));
}

void ImageViewportPublicApiTest::doesNotExposeOutOfScopePublicState()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    const QList<QByteArray> absentProperties = {
        "source",
        "url",
        "sourceUrl",
        "file",
        "filePath",
        "path",
        "progress",
        "providerProgress",
        "acceptedSequence",
        "displayedSequence",
        "loopProgress",
    };

    for (const QByteArray& property : absentProperties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) < 0, property.constData());
    }

    const QList<QByteArray> absentMethods = {
        "load(QString)",
        "load(QUrl)",
        "open(QString)",
        "open(QUrl)",
        "setSource(QString)",
        "setSource(QUrl)",
        "setProvider(QObject*)",
        "setProgress(double)",
        "setTexture(QVariant)",
        "setTileProvider(QObject*)",
        "setRegion(QRectF)",
        "setColorManagementPolicy(QVariant)",
    };

    for (const QByteArray& method : absentMethods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) < 0,
            method.constData());
    }
}

void ImageViewportPublicApiTest::unsupportedSequencePropertyWritesPreserveState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject* metaObject = item.metaObject();
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();
    QSignalSpy sequenceSpy(&item, &ImageViewport::sequenceChanged);
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);

    const QList<QVariant> unsupportedValues = {
        QVariant(QStringLiteral("image.png")),
        QVariant(QUrl(QStringLiteral("file:///tmp/image.png"))),
        QVariant(QByteArray("not image data")),
        QVariantMap { { QStringLiteral("url"), QStringLiteral("image.png") } },
        QVariant::fromValue<QObject*>(&adapter),
    };

    for (const QVariant& value : unsupportedValues) {
        QCOMPARE(item.setProperty("sequence", value), false);
        QCOMPARE(item.sequence(), result->sequence());
        QCOMPARE(item.property("requestStatus").toInt(),
            enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(item.property("requestReason").toInt(),
            enumValue(metaObject, "RequestReason", "Ready"));
        QCOMPARE(item.property("displayStatus").toInt(),
            enumValue(metaObject, "DisplayStatus", "Ready"));
        QCOMPARE(item.property("requestedFrame").toInt(), 0);
        QCOMPARE(item.property("displayedFrame").toInt(), 0);
        QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
        QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
    }

    QCOMPARE(sequenceSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportPublicApiTest::sequenceAssignmentPreservesCommandDiagnostic()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    const uint commandRevision = item.property("commandRevision").toUInt();

    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    item.setSequence(result->sequence());

    QCOMPARE(item.sequence(), result->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), commandRevision);
    QCOMPARE(commandSpy.count(), 0);
}

void ImageViewportPublicApiTest::qmlUnsupportedSequenceAssignmentsPreserveState()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    engine.rootContext()->setContextProperty(QStringLiteral("rawProvider"), &adapter);

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    QtObject { id: rawObject }
    property bool stringAssignmentPreserved: false
    property bool urlAssignmentPreserved: false
    property bool byteBufferAssignmentPreserved: false
    property bool jsObjectAssignmentPreserved: false
    property bool objectAssignmentPreserved: false
    property bool providerAssignmentPreserved: false

    Component.onCompleted: {
        try {
            sequence = "image.png"
        } catch (error) {
        }
        stringAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = Qt.resolvedUrl("image.png")
        } catch (error) {
        }
        urlAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = new ArrayBuffer(4)
        } catch (error) {
        }
        byteBufferAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = ({ url: "image.png" })
        } catch (error) {
        }
        jsObjectAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = rawObject
        } catch (error) {
        }
        objectAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = rawProvider
        } catch (error) {
        }
        providerAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    QCOMPARE(object->property("stringAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("urlAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("byteBufferAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("jsObjectAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("objectAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("providerAssignmentPreserved").toBool(), true);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportPublicApiTest::qmlUnsupportedSequenceAssignmentsPreserveReadyState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    engine.rootContext()->setContextProperty(QStringLiteral("rawProvider"), &adapter);

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    width: 100
    height: 100

    property ImageSequence suppliedSequence
    QtObject { id: rawObject }

    property bool stringAssignmentPreserved: false
    property bool urlAssignmentPreserved: false
    property bool byteBufferAssignmentPreserved: false
    property bool jsObjectAssignmentPreserved: false
    property bool objectAssignmentPreserved: false
    property bool providerAssignmentPreserved: false

    function readyStatePreserved(requestRevisionBefore, displayRevisionBefore) {
        return sequence === suppliedSequence
            && requestStatus === ImageViewport.RequestStatus.Ready
            && requestReason === ImageViewport.RequestReason.Ready
            && displayStatus === ImageViewport.DisplayStatus.Ready
            && requestedFrame === 0
            && displayedFrame === 0
            && requestRevision === requestRevisionBefore
            && displayRevision === displayRevisionBefore
            && errorString === ""
    }

    Component.onCompleted: {
        sequence = suppliedSequence
        const requestRevisionBefore = requestRevision
        const displayRevisionBefore = displayRevision
        try {
            sequence = "image.png"
        } catch (error) {
        }
        stringAssignmentPreserved = readyStatePreserved(requestRevisionBefore, displayRevisionBefore)
        try {
            sequence = Qt.resolvedUrl("image.png")
        } catch (error) {
        }
        urlAssignmentPreserved = readyStatePreserved(requestRevisionBefore, displayRevisionBefore)
        try {
            sequence = new ArrayBuffer(4)
        } catch (error) {
        }
        byteBufferAssignmentPreserved = readyStatePreserved(requestRevisionBefore, displayRevisionBefore)
        try {
            sequence = ({ url: "image.png" })
        } catch (error) {
        }
        jsObjectAssignmentPreserved = readyStatePreserved(requestRevisionBefore, displayRevisionBefore)
        try {
            sequence = rawObject
        } catch (error) {
        }
        objectAssignmentPreserved = readyStatePreserved(requestRevisionBefore, displayRevisionBefore)
        try {
            sequence = rawProvider
        } catch (error) {
        }
        providerAssignmentPreserved = readyStatePreserved(requestRevisionBefore, displayRevisionBefore)
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QVariantMap initialProperties;
    initialProperties.insert(
        QStringLiteral("suppliedSequence"), QVariant::fromValue<QObject*>(result->sequence()));
    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(object, qPrintable(componentErrors(component)));
    QCOMPARE(object->property("stringAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("urlAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("byteBufferAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("jsObjectAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("objectAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("providerAssignmentPreserved").toBool(), true);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportPublicApiTest::exposesDocumentedQmlSurface()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

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

    for (const QByteArray& property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QList<QByteArray> enumerators = {
        "RequestStatus",
        "RequestReason",
        "CommandReason",
        "DisplayStatus",
        "PlaybackPhase",
        "TriState",
        "CommandOutcome",
        "FillMode",
        "HorizontalAlignment",
        "VerticalAlignment",
        "BackgroundMode",
    };

    for (const QByteArray& enumerator : enumerators) {
        QVERIFY2(
            metaObject->indexOfEnumerator(enumerator.constData()) >= 0, enumerator.constData());
    }

    verifyEnumValues(
        metaObject, "RequestStatus", { "NoRequest", "Loading", "Ready", "Unsupported", "Error" });
    verifyEnumValues(metaObject, "RequestReason",
        { "NoRequest", "ProviderWaiting", "RequestQueued", "UploadPending", "RenderWaiting",
            "Ready", "UnsupportedRequest", "InvalidRequest", "ProviderFailure", "PayloadRejection",
            "RenderFailure" });
    verifyEnumValues(metaObject, "CommandReason",
        { "NoCommand", "IgnoredNoRequest", "InvalidRequest", "UnsupportedRequest" });
    verifyEnumValues(metaObject, "DisplayStatus", { "Empty", "Ready", "Retained" });
    verifyEnumValues(metaObject, "PlaybackPhase", { "Stopped", "Playing", "Waiting", "Paused" });
    verifyEnumValues(metaObject, "TriState", { "Unavailable", "False", "True" });
    verifyEnumValues(
        metaObject, "CommandOutcome", { "Accepted", "Invalid", "Unsupported", "IgnoredNoRequest" });
    verifyEnumValues(metaObject, "FillMode", { "Contain", "Cover", "Stretch", "Center" });
    verifyEnumValues(
        metaObject, "HorizontalAlignment", { "AlignLeft", "AlignHCenter", "AlignRight" });
    verifyEnumValues(
        metaObject, "VerticalAlignment", { "AlignTop", "AlignVCenter", "AlignBottom" });
    verifyEnumValues(metaObject, "BackgroundMode", { "Transparent", "SolidColor", "Checkerboard" });

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

    for (const QByteArray& method : methods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0,
            method.constData());
    }
}

void ImageViewportPublicApiTest::exposesFinalApiScaffold()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    const QList<QByteArray> properties = {
        "primarySequence",
        "secondarySequence",
        "spreadDirection",
        "pageGap",
        "primaryDisplayedFrame",
        "primaryRequestedFrame",
        "secondaryDisplayedFrame",
        "secondaryRequestedFrame",
        "primaryDisplayedPosition",
        "primaryRequestedPosition",
        "secondaryDisplayedPosition",
        "secondaryRequestedPosition",
        "primaryFrameCount",
        "secondaryFrameCount",
        "primaryTotalDuration",
        "secondaryTotalDuration",
        "primaryFrameSeekBounds",
        "secondaryFrameSeekBounds",
        "primaryPositionSeekBounds",
        "secondaryPositionSeekBounds",
        "primaryTimedPlaybackSupport",
        "secondaryTimedPlaybackSupport",
        "primaryFrameSeekSupport",
        "secondaryFrameSeekSupport",
        "primaryPositionSeekSupport",
        "secondaryPositionSeekSupport",
        "displayedSpreadSize",
        "primaryDisplayedImageSize",
        "secondaryDisplayedImageSize",
        "visibleSpreadRect",
        "primaryPageRect",
        "secondaryPageRect",
        "primaryItemRect",
        "secondaryItemRect",
        "visiblePrimaryPageRect",
        "visibleSecondaryPageRect",
        "contentSize",
        "contentPosition",
        "maximumContentPosition",
        "horizontalPannable",
        "verticalPannable",
        "fitMode",
        "zoomPercent",
        "rotationDegrees",
    };

    for (const QByteArray& property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QList<QByteArray> enumerators = {
        "PageRole",
        "SpreadDirection",
        "FitMode",
    };

    for (const QByteArray& enumerator : enumerators) {
        QVERIFY2(
            metaObject->indexOfEnumerator(enumerator.constData()) >= 0, enumerator.constData());
    }

    verifyEnumValues(metaObject, "PageRole", { "Primary", "Secondary" });
    verifyEnumValues(metaObject, "SpreadDirection", { "LeftToRight", "RightToLeft" });
    verifyEnumValues(metaObject, "FitMode", { "Contain", "FitWidth", "FitHeight", "Manual" });

    const QList<QByteArray> methods = {
        "setPageSet(QVariant,QVariant,QVariant)",
        "setSpreadDirection(SpreadDirection)",
        "setPageGap(double)",
        "panToStart()",
        "panToEnd()",
        "scanNext()",
        "scanPrevious()",
        "itemToSpread(double,double)",
        "spreadToItem(double,double)",
        "containsVisibleSpreadPoint(double,double)",
    };

    for (const QByteArray& method : methods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0,
            method.constData());
    }
}

void ImageViewportPublicApiTest::hasDocumentedDefaultState()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.property("sequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "NoRequest"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(
        item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value(QStringLiteral("minimum")).toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value(QStringLiteral("maximum")).toInt(), -1);
    QCOMPARE(
        item.property("positionSeekBounds").toMap().value(QStringLiteral("minimum")).toInt(), -1);
    QCOMPARE(
        item.property("positionSeekBounds").toMap().value(QStringLiteral("maximum")).toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(),
        enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    verifyInvalidCoordinateResult(item.itemToImage(1.0, 1.0));
    verifyInvalidCoordinateResult(item.imageToItem(1.0, 1.0));
    QCOMPARE(item.containsVisibleImagePoint(1.0, 1.0), false);
    QCOMPARE(item.property("displayRevision").toUInt(), 0U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QCOMPARE(item.property("commandRevision").toUInt(), 0U);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(item.property("fillMode").toInt(), enumValue(metaObject, "FillMode", "Contain"));
    QCOMPARE(item.property("horizontalAlignment").toInt(),
        enumValue(metaObject, "HorizontalAlignment", "AlignHCenter"));
    QCOMPARE(item.property("verticalAlignment").toInt(),
        enumValue(metaObject, "VerticalAlignment", "AlignVCenter"));
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("smoothing").toBool(), true);
    QCOMPARE(item.property("mipmap").toBool(), false);
    QCOMPARE(item.property("mirrorHorizontally").toBool(), false);
    QCOMPARE(item.property("mirrorVertically").toBool(), false);
    QCOMPARE(item.property("backgroundMode").toInt(),
        enumValue(metaObject, "BackgroundMode", "Transparent"));
    QCOMPARE(item.property("backgroundColor").value<QColor>(), QColor(Qt::transparent));
    QCOMPARE(item.property("looping").toBool(), false);
}

void ImageViewportPublicApiTest::qmlFinalApiScaffoldDefaultsAndCommands()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport

    property bool defaultsValid: false
    property bool roleCommandsReachViewport: false
    property bool pageSetValidationPreservedState: false
    property bool presentationCommandsReachViewport: false
    property bool coordinateAliasesAvailable: false

    Component.onCompleted: {
        defaultsValid = sequence === null
            && primarySequence === null
            && secondarySequence === null
            && spreadDirection === ImageViewport.SpreadDirection.LeftToRight
            && pageGap === 0
            && primaryDisplayedFrame === -1
            && secondaryDisplayedFrame === -1
            && primaryRequestedPosition === -1
            && secondaryRequestedPosition === -1
            && primaryFrameCount === -1
            && secondaryFrameCount === -1
            && primaryFrameSeekBounds.minimum === -1
            && secondaryFrameSeekBounds.maximum === -1
            && primaryTimedPlaybackSupport === ImageViewport.TriState.Unavailable
            && secondaryTimedPlaybackSupport === ImageViewport.TriState.Unavailable
            && displayedSpreadSize.width === 0
            && primaryDisplayedImageSize.height === 0
            && secondaryDisplayedImageSize.width === 0
            && visibleSpreadRect.width === 0
            && primaryPageRect.height === 0
            && secondaryItemRect.width === 0
            && visiblePrimaryPageRect.height === 0
            && contentSize.width === 0
            && contentPosition.x === 0
            && maximumContentPosition.y === 0
            && horizontalPannable === false
            && verticalPannable === false
            && fitMode === ImageViewport.FitMode.Contain
            && zoomPercent === 100
            && rotationDegrees === 0

        const requestRevisionBefore = requestRevision
        const displayRevisionBefore = displayRevision
        const invalidPageSetOutcome = setPageSet("image.png", null)
        pageSetValidationPreservedState = invalidPageSetOutcome === ImageViewport.CommandOutcome.Invalid
            && sequence === null
            && primarySequence === null
            && secondarySequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === requestRevisionBefore
            && displayRevision === displayRevisionBefore

        roleCommandsReachViewport = play(ImageViewport.PageRole.Secondary) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && pause(ImageViewport.PageRole.Secondary) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && stop(ImageViewport.PageRole.Secondary) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && seek(ImageViewport.PageRole.Secondary, 0) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && seekToPosition(ImageViewport.PageRole.Secondary, 0) === ImageViewport.CommandOutcome.IgnoredNoRequest

        presentationCommandsReachViewport = setSpreadDirection(ImageViewport.SpreadDirection.LeftToRight) === ImageViewport.CommandOutcome.Accepted
            && setPageGap(0) === ImageViewport.CommandOutcome.Accepted
            && setFitMode(ImageViewport.FitMode.Contain, Qt.point(0, 0)) === ImageViewport.CommandOutcome.Accepted
            && setZoomPercent(100, Qt.point(0, 0)) === ImageViewport.CommandOutcome.Accepted
            && panBy(Qt.point(0, 0)) === ImageViewport.CommandOutcome.Accepted
            && panToStart() === ImageViewport.CommandOutcome.Accepted
            && panToEnd() === ImageViewport.CommandOutcome.Accepted
            && scanNext() === ImageViewport.CommandOutcome.Accepted
            && scanPrevious() === ImageViewport.CommandOutcome.Accepted
            && rotateClockwise(Qt.point(0, 0)) === ImageViewport.CommandOutcome.Accepted
            && rotateCounterClockwise(Qt.point(0, 0)) === ImageViewport.CommandOutcome.Accepted
            && setMirrorHorizontally(false, Qt.point(0, 0)) === ImageViewport.CommandOutcome.Accepted
            && setMirrorVertically(false, Qt.point(0, 0)) === ImageViewport.CommandOutcome.Accepted
            && resetView() === ImageViewport.CommandOutcome.Accepted

        coordinateAliasesAvailable = itemToSpread(1, 1).valid === false
            && spreadToItem(1, 1).valid === false
            && itemToPage(ImageViewport.PageRole.Primary, 1, 1).valid === false
            && pageToItem(ImageViewport.PageRole.Primary, 1, 1).valid === false
            && containsVisibleSpreadPoint(1, 1) === false
            && containsVisiblePagePoint(ImageViewport.PageRole.Primary, 1, 1) === false
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));

    QCOMPARE(object->property("defaultsValid").toBool(), true);
    QCOMPARE(object->property("pageSetValidationPreservedState").toBool(), true);
    QCOMPARE(object->property("roleCommandsReachViewport").toBool(), true);
    QCOMPARE(object->property("presentationCommandsReachViewport").toBool(), true);
    QCOMPARE(object->property("coordinateAliasesAvailable").toBool(), true);
}

void ImageViewportPublicApiTest::setPageSetAcceptsPrimaryAndSecondaryAtomically()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    ImageSequence* secondaryObservedDuringSignal = nullptr;
    connect(&item, &ImageViewport::sequenceChanged, &item, [&] {
        secondaryObservedDuringSignal
            = item.property("secondarySequence").value<ImageSequence*>();
    });

    const auto outcome = item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
        QVariant::fromValue<QObject*>(secondaryResult->sequence()));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.sequence(), primaryResult->sequence());
    QCOMPARE(item.property("primarySequence").value<ImageSequence*>(), primaryResult->sequence());
    QCOMPARE(
        item.property("secondarySequence").value<ImageSequence*>(), secondaryResult->sequence());
    QCOMPARE(secondaryObservedDuringSignal, secondaryResult->sequence());
    QCOMPARE(item.property("primaryFrameCount").toInt(), 1);
    QCOMPARE(item.property("secondaryFrameCount").toInt(), 1);
    QCOMPARE(item.property("primaryFrameSeekBounds").toMap().value(QStringLiteral("minimum")).toInt(),
        0);
    QCOMPARE(
        item.property("secondaryFrameSeekBounds").toMap().value(QStringLiteral("maximum")).toInt(),
        0);
    QCOMPARE(item.property("primaryFrameSeekSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "True"));
    QCOMPARE(item.property("secondaryFrameSeekSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "True"));
    QCOMPARE(item.property("secondaryTimedPlaybackSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "False"));
}

void ImageViewportPublicApiTest::compatibilitySequenceAssignmentClearsSecondaryRole()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame firstFrame(image);
    ImageFrame secondFrame(image);
    ImageFrame replacementFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondResult(factory.fromFrame(&secondFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromFrame(&replacementFrame));
    QVERIFY(firstResult->sequence());
    QVERIFY(secondResult->sequence());
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(firstResult->sequence()),
                 QVariant::fromValue<QObject*>(secondResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);

    item.setSequence(replacementResult->sequence());

    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(
        item.property("primarySequence").value<ImageSequence*>(), replacementResult->sequence());
    QCOMPARE(item.property("secondarySequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("secondaryFrameCount").toInt(), -1);
}

void ImageViewportPublicApiTest::clearStylePageSetWithSecondaryClearsAcceptedRoles()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    ImageFrame secondaryFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);

    const auto outcome = item.setPageSet(QVariant(), QVariant::fromValue<QObject*>(
                                                       secondaryResult->sequence()));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("primarySequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("secondarySequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
}

void ImageViewportPublicApiTest::invalidPageSetSecondaryPreservesAcceptedRoles()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    ImageFrame secondaryFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();

    const auto outcome = item.setPageSet(
        QVariant::fromValue<QObject*>(primaryResult->sequence()), QVariant(QStringLiteral("bad")));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("primarySequence").value<ImageSequence*>(), primaryResult->sequence());
    QCOMPARE(
        item.property("secondarySequence").value<ImageSequence*>(), secondaryResult->sequence());
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
}

void ImageViewportPublicApiTest::pageSetTransitionClearBeforeLoadClearsRetainedDisplay()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> readyResult(factory.fromFrame(&frame));
    QVERIFY(readyResult->sequence());

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

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(readyResult->sequence());
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    QVariantMap policy;
    policy.insert(QStringLiteral("displayTransition"),
        static_cast<int>(PageSetTransitionPolicy::DisplayTransition::ClearBeforeLoad));

    const auto outcome
        = item.setPageSet(QVariant::fromValue<QObject*>(loadingResult->sequence()), {}, policy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.sequence(), loadingResult->sequence());
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
}

void ImageViewportPublicApiTest::invalidPageSetTransitionPolicyPreservesState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame firstFrame(image);
    ImageFrame replacementFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(firstResult->sequence());
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    const QMetaObject* metaObject = item.metaObject();
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();
    const uint commandRevision = item.property("commandRevision").toUInt();
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);

    QVariantMap invalidPolicy;
    invalidPolicy.insert(QStringLiteral("displayTransition"), 999);

    const auto outcome = item.setPageSet(
        QVariant::fromValue<QObject*>(replacementResult->sequence()), {}, invalidPolicy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.sequence(), firstResult->sequence());
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
    QVERIFY(item.property("commandRevision").toUInt() > commandRevision);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportPublicApiTest::invalidClearStyleTransitionPolicyPreservesState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    ImageFrame secondaryFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(primaryResult->sequence());
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();

    QVariantMap invalidPolicy;
    invalidPolicy.insert(QStringLiteral("pageGap"), -1.0);

    const auto outcome = item.setPageSet(QVariant(), QVariant(), invalidPolicy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("primarySequence").value<ImageSequence*>(), primaryResult->sequence());
    QCOMPARE(
        item.property("secondarySequence").value<ImageSequence*>(), secondaryResult->sequence());
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
}

void ImageViewportPublicApiTest::secondaryProviderPageSetOpensRoleLocalSessionAndWaits()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    const auto outcome = item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
        QVariant::fromValue<QObject*>(secondaryResult->sequence()));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("primarySequence").value<ImageSequence*>(), primaryResult->sequence());
    QCOMPARE(
        item.property("secondarySequence").value<ImageSequence*>(), secondaryResult->sequence());
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(item.metaObject(), "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(item.metaObject(), "RequestReason", "ProviderWaiting"));
}

void ImageViewportPublicApiTest::secondaryProviderMetadataUpdatesRoleObservations()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromProvider(&adapter));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
                 QVariant::fromValue<QObject*>(secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    QVERIFY(sessionFactory->lastSession());

    emit sessionFactory->lastSession()->metadataReady(
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(10.0, 20.0)));
    drainQueuedProviderResults();

    QCOMPARE(item.property("secondaryFrameCount").toInt(), 1);
    QCOMPARE(item.property("secondaryFrameSeekBounds").toMap().value(QStringLiteral("minimum")).toInt(),
        0);
    QCOMPARE(item.property("secondaryFrameSeekBounds").toMap().value(QStringLiteral("maximum")).toInt(),
        0);
    QCOMPARE(item.property("secondaryFrameSeekSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "True"));
    QCOMPARE(item.property("secondaryTimedPlaybackSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "False"));
}

void ImageViewportPublicApiTest::emptyGeometryChangeIncrementsDisplayRevision()
{
    ImageViewport item;
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setSize(QSizeF(100.0, 50.0));

    QCOMPARE(item.property("displayRevision").toUInt(), 1U);
    QCOMPARE(displayRevisionSpy.count(), 1);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(item.property("displayStatus").toInt(),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());

    item.setSize(QSizeF(100.0, 50.0));

    QCOMPARE(item.property("displayRevision").toUInt(), 1U);
    QCOMPARE(displayRevisionSpy.count(), 1);

    const double changedWidth = 100.0 + 5.0e-13;
    QVERIFY(changedWidth != 100.0);
    item.setSize(QSizeF(changedWidth, 50.0));

    QCOMPARE(item.property("displayRevision").toUInt(), 2U);
    QCOMPARE(displayRevisionSpy.count(), 2);
}

void ImageViewportPublicApiTest::qmlImportsDocumentedSurface()
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
    property int accepted: ImageViewport.CommandOutcome.Accepted
    property int unsupported: ImageViewport.CommandOutcome.Unsupported
    property int invalid: ImageViewport.CommandOutcome.Invalid
    property int ignoredNoRequest: ImageViewport.CommandOutcome.IgnoredNoRequest
    property int factoryCreated: ImageSequenceFactoryResult.FactoryOutcome.Created
    property int factoryInvalid: ImageSequenceFactoryResult.FactoryOutcome.Invalid
    property int factoryUnsupported: ImageSequenceFactoryResult.FactoryOutcome.Unsupported
    property int factoryError: ImageSequenceFactoryResult.FactoryOutcome.Error
    property int cover: ImageViewport.FillMode.Cover
    property int center: ImageViewport.FillMode.Center
    property bool factoryReturnsNull: ImageSequenceFactory.fromFrame(null).sequence === null
    property bool mappingInvalid: itemToImage(1, 1).valid === false
    property bool mappingHasFlatFields: imageToItem(1, 1).x === 0 && imageToItem(1, 1).y === 0
    property bool unavailableValuesHaveDocumentedFields: frameSeekBounds.minimum === -1
        && frameSeekBounds.maximum === -1
        && positionSeekBounds.minimum === -1
        && positionSeekBounds.maximum === -1
        && contentRect.width === 0
        && visibleImageRect.height === 0
    property bool limitsAvailable: ImageSequenceLimits.maximumLogicalWidth >= 8192
        && ImageSequenceLimits.maximumLogicalHeight >= 8192
        && ImageSequenceLimits.maximumPixelsPerFrame >= 67108864
        && ImageSequenceLimits.maximumPayloadBytesPerFrame >= 268435456
        && ImageSequenceLimits.maximumTimedListFrameCount >= 10000
        && ImageSequenceLimits.maximumFrameDuration >= 86400000
        && ImageSequenceLimits.maximumTotalSequenceDuration >= 86400000
        && ImageSequenceLimits.maximumDiagnosticStringLength >= 4096
        && ImageViewportDisplayLimits.maximumManualZoomPercent >= 100
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        object->property("noRequest").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(
        object->property("loading").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(
        object->property("retained").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(
        object->property("waiting").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(
        object->property("accepted").toInt(), enumValue(metaObject, "CommandOutcome", "Accepted"));
    QCOMPARE(object->property("unsupported").toInt(),
        enumValue(metaObject, "CommandOutcome", "Unsupported"));
    QCOMPARE(
        object->property("invalid").toInt(), enumValue(metaObject, "CommandOutcome", "Invalid"));
    QCOMPARE(object->property("ignoredNoRequest").toInt(),
        enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    ImageSequenceFactoryResult result(nullptr, ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    const QMetaObject* resultMetaObject = result.metaObject();
    QCOMPARE(object->property("factoryCreated").toInt(),
        enumValue(resultMetaObject, "FactoryOutcome", "Created"));
    QCOMPARE(object->property("factoryInvalid").toInt(),
        enumValue(resultMetaObject, "FactoryOutcome", "Invalid"));
    QCOMPARE(object->property("factoryUnsupported").toInt(),
        enumValue(resultMetaObject, "FactoryOutcome", "Unsupported"));
    QCOMPARE(object->property("factoryError").toInt(),
        enumValue(resultMetaObject, "FactoryOutcome", "Error"));
    QCOMPARE(object->property("cover").toInt(), enumValue(metaObject, "FillMode", "Cover"));
    QCOMPARE(object->property("center").toInt(), enumValue(metaObject, "FillMode", "Center"));
    QCOMPARE(object->property("factoryReturnsNull").toBool(), true);
    QCOMPARE(object->property("mappingInvalid").toBool(), true);
    QCOMPARE(object->property("mappingHasFlatFields").toBool(), true);
    QCOMPARE(object->property("unavailableValuesHaveDocumentedFields").toBool(), true);
    QCOMPARE(object->property("limitsAvailable").toBool(), true);
}

void ImageViewportPublicApiTest::qmlReadyValuesExposeDocumentedFields()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    width: 100
    height: 100

    property ImageSequence suppliedSequence
    property bool readyValuesHaveDocumentedFields: false

    Component.onCompleted: {
        sequence = suppliedSequence
        readyValuesHaveDocumentedFields = displayedImageSize.width === 16
            && displayedImageSize.height === 8
            && contentRect.x === 0
            && contentRect.y === 25
            && contentRect.width === 100
            && contentRect.height === 50
            && visibleImageRect.x === 0
            && visibleImageRect.y === 0
            && visibleImageRect.width === 16
            && visibleImageRect.height === 8
            && frameSeekBounds.minimum === 0
            && frameSeekBounds.maximum === 0
            && positionSeekBounds.minimum === -1
            && positionSeekBounds.maximum === -1
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QVariantMap initialProperties;
    initialProperties.insert(
        QStringLiteral("suppliedSequence"), QVariant::fromValue<QObject*>(result->sequence()));
    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(object, qPrintable(componentErrors(component)));
    QCOMPARE(object->property("readyValuesHaveDocumentedFields").toBool(), true);
}

void ImageViewportPublicApiTest::qmlCommandsReturnDocumentedOutcomes()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    property int playOutcome: play()
    property int pauseOutcome: pause()
    property int stopOutcome: stop()
    property int seekOutcome: seek(0)
    property int positionSeekOutcome: seekToPosition(0)
    property int clearOutcome: clear()
    property int resetViewOutcome: resetView()
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));

    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(object->property("playOutcome").toInt(),
        enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("pauseOutcome").toInt(),
        enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("stopOutcome").toInt(),
        enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("seekOutcome").toInt(),
        enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("positionSeekOutcome").toInt(),
        enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("clearOutcome").toInt(),
        enumValue(metaObject, "CommandOutcome", "Accepted"));
    QCOMPARE(object->property("resetViewOutcome").toInt(),
        enumValue(metaObject, "CommandOutcome", "Accepted"));
}

void ImageViewportPublicApiTest::qmlFactoryFailuresReturnDocumentedDiagnostics()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQml
import ImageViewport 1.0

QtObject {
    readonly property var frameResult: ImageSequenceFactory.fromFrame(null)
    readonly property var listResult: ImageSequenceFactory.fromTimedFrameList(null)
    readonly property var providerResult: ImageSequenceFactory.fromProvider(null)
    property bool frameRejected: frameResult.sequence === null
        && frameResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Invalid
        && frameResult.errorString.length > 0
        && frameResult.warningString === ""
    property bool listRejected: listResult.sequence === null
        && listResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Invalid
        && listResult.errorString.length > 0
        && listResult.warningString === ""
    property bool providerRejected: providerResult.sequence === null
        && providerResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Invalid
        && providerResult.errorString.length > 0
        && providerResult.warningString === ""
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));

    QCOMPARE(object->property("frameRejected").toBool(), true);
    QCOMPARE(object->property("listRejected").toBool(), true);
    QCOMPARE(object->property("providerRejected").toBool(), true);
}

void ImageViewportPublicApiTest::imageSequenceIsNotQmlCreatable()
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

void ImageViewportPublicApiTest::imageFrameIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageFrame {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportPublicApiTest::imageSequenceProviderFrameHandleIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequenceProviderFrameHandle {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportPublicApiTest::imageSequenceProviderAdapterIsNotQmlCreatable()
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

void ImageViewportPublicApiTest::imageSequenceFactoryResultIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequenceFactoryResult {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportPublicApiTest::imageSequenceFactoryIsQmlSingletonOnly()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequenceFactory {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportPublicApiTest::imageSequenceLimitsIsQmlSingletonOnly()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequenceLimits {}
)",
        QUrl());

    QVERIFY(component.isError());
}

QTEST_MAIN(ImageViewportPublicApiTest)

#include "tst_imageviewport_public_api.moc"
