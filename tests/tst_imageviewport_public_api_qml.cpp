#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

class ImageViewportPublicApiQmlTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportPublicApiQmlTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void qmlUnsupportedSequenceAssignmentsPreserveState();
    void qmlUnsupportedSequenceAssignmentsPreserveReadyState();
    void qmlFinalApiScaffoldDefaultsAndCommands();
    void qmlImportsDocumentedSurface();
    void qmlReadyValuesExposeDocumentedFields();
    void pageGeometryQmlValueType();
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

void ImageViewportPublicApiQmlTest::qmlUnsupportedSequenceAssignmentsPreserveState()
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
            && !requestRevision.valid
            && !displayRevision.valid
            && errorString === ""
        try {
            sequence = Qt.resolvedUrl("image.png")
        } catch (error) {
        }
        urlAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && !requestRevision.valid
            && !displayRevision.valid
            && errorString === ""
        try {
            sequence = new ArrayBuffer(4)
        } catch (error) {
        }
        byteBufferAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && !requestRevision.valid
            && !displayRevision.valid
            && errorString === ""
        try {
            sequence = ({ url: "image.png" })
        } catch (error) {
        }
        jsObjectAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && !requestRevision.valid
            && !displayRevision.valid
            && errorString === ""
        try {
            sequence = rawObject
        } catch (error) {
        }
        objectAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && !requestRevision.valid
            && !displayRevision.valid
            && errorString === ""
        try {
            sequence = rawProvider
        } catch (error) {
        }
        providerAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && !requestRevision.valid
            && !displayRevision.valid
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

void ImageViewportPublicApiQmlTest::qmlUnsupportedSequenceAssignmentsPreserveReadyState()
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

    function exerciseUnsupportedAssignments() {
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

    Component.onCompleted: {
        sequence = suppliedSequence
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
    auto* viewport = qobject_cast<ImageViewport*>(object.data());
    QVERIFY(viewport);
    acknowledgePendingRenderCommitForTest(*viewport);
    QVERIFY(QMetaObject::invokeMethod(object.data(), "exerciseUnsupportedAssignments"));
    QCOMPARE(object->property("stringAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("urlAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("byteBufferAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("jsObjectAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("objectAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("providerAssignmentPreserved").toBool(), true);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportPublicApiQmlTest::qmlFinalApiScaffoldDefaultsAndCommands()
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

void ImageViewportPublicApiQmlTest::qmlImportsDocumentedSurface()
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
    QCOMPARE(object->property("factoryReturnsNull").toBool(), true);
    QCOMPARE(object->property("mappingInvalid").toBool(), true);
    QCOMPARE(object->property("mappingHasFlatFields").toBool(), true);
    QCOMPARE(object->property("unavailableValuesHaveDocumentedFields").toBool(), true);
    QCOMPARE(object->property("limitsAvailable").toBool(), true);
}

void ImageViewportPublicApiQmlTest::qmlReadyValuesExposeDocumentedFields()
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
    property bool readyValuesHaveDocumentedFields: displayedImageSize.width === 16
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

    Component.onCompleted: {
        sequence = suppliedSequence
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
    auto* viewport = qobject_cast<ImageViewport*>(object.data());
    QVERIFY(viewport);
    acknowledgePendingRenderCommitForTest(*viewport);
    QCOMPARE(object->property("readyValuesHaveDocumentedFields").toBool(), true);
}

void ImageViewportPublicApiQmlTest::pageGeometryQmlValueType()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult);
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(10, 20, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult);
    QVERIFY(secondaryResult->sequence());

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    width: 26
    height: 20

    property ImageSequence suppliedPrimary
    property ImageSequence suppliedSecondary
    property bool pageGeometryHasFields: false
    property bool secondaryUnavailableHasFields: false

    function capturePageGeometry() {
        const primaryGeometry = primaryPageGeometry
        const secondaryGeometry = pageGeometry(ImageViewport.PageRole.Secondary)
        pageGeometryHasFields = primaryGeometry.available === true
            && primaryGeometry.role === ImageViewport.PageRole.Primary
            && primaryGeometry.pageRect.x === 0
            && primaryGeometry.pageRect.y === 0
            && primaryGeometry.pageRect.width === 16
            && primaryGeometry.pageRect.height === 8
            && primaryGeometry.itemRect.x === primaryItemRect.x
            && primaryGeometry.itemRect.width === primaryItemRect.width
            && primaryGeometry.visiblePageRect.width === visiblePrimaryPageRect.width
            && secondaryGeometry.available === true
            && secondaryGeometry.role === ImageViewport.PageRole.Secondary
            && secondaryGeometry.pageRect.x === 16
            && secondaryGeometry.pageRect.y === 0
            && secondaryGeometry.pageRect.width === 10
            && secondaryGeometry.pageRect.height === 20
            && secondaryGeometry.itemRect.x === secondaryItemRect.x
            && secondaryGeometry.visiblePageRect.height === visibleSecondaryPageRect.height

        clear()
        setPageSet(suppliedPrimary, null)
        const unavailableSecondary = secondaryPageGeometry
        secondaryUnavailableHasFields = unavailableSecondary.available === false
            && unavailableSecondary.role === ImageViewport.PageRole.Secondary
            && unavailableSecondary.pageRect.width === 0
            && unavailableSecondary.itemRect.height === 0
            && unavailableSecondary.visiblePageRect.width === 0
    }

    Component.onCompleted: {
        setPageSet(suppliedPrimary, suppliedSecondary)
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("suppliedPrimary"),
        QVariant::fromValue<QObject*>(primaryResult->sequence()));
    initialProperties.insert(QStringLiteral("suppliedSecondary"),
        QVariant::fromValue<QObject*>(secondaryResult->sequence()));
    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(object, qPrintable(componentErrors(component)));
    auto* viewport = qobject_cast<ImageViewport*>(object.data());
    QVERIFY(viewport);
    acknowledgePendingRenderCommitForTest(*viewport);
    QVERIFY(QMetaObject::invokeMethod(object.data(), "capturePageGeometry"));
    acknowledgePendingRenderCommitForTest(*viewport);
    QCOMPARE(object->property("pageGeometryHasFields").toBool(), true);
    QCOMPARE(object->property("secondaryUnavailableHasFields").toBool(), true);
}

void ImageViewportPublicApiQmlTest::qmlCommandsReturnDocumentedOutcomes()
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

void ImageViewportPublicApiQmlTest::qmlFactoryFailuresReturnDocumentedDiagnostics()
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

void ImageViewportPublicApiQmlTest::imageSequenceIsNotQmlCreatable()
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

void ImageViewportPublicApiQmlTest::imageFrameIsNotQmlCreatable()
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

void ImageViewportPublicApiQmlTest::imageSequenceProviderFrameHandleIsNotQmlCreatable()
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

void ImageViewportPublicApiQmlTest::imageSequenceProviderAdapterIsNotQmlCreatable()
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

void ImageViewportPublicApiQmlTest::imageSequenceFactoryResultIsNotQmlCreatable()
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

void ImageViewportPublicApiQmlTest::imageSequenceFactoryIsQmlSingletonOnly()
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

void ImageViewportPublicApiQmlTest::imageSequenceLimitsIsQmlSingletonOnly()
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
QTEST_MAIN(ImageViewportPublicApiQmlTest)

#include "tst_imageviewport_public_api_qml.moc"
