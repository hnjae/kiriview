#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

#include <QtCore/QCoreApplication>

class BindingRefreshRecorder : public QObject
{
    Q_OBJECT

public:
    explicit BindingRefreshRecorder(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    Q_INVOKABLE double rememberMaximum(double value)
    {
        ++m_count;
        return value;
    }

    int count() const { return m_count; }

private:
    int m_count = 0;
};

class ImageViewportPublicApiQmlTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportPublicApiQmlTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void qmlRemovedSequencePropertyPreservesDefaultState();
    void qmlRemovedSequencePropertyPreservesReadyState();
    void qmlFinalApiScaffoldDefaultsAndCommands();
    void manualZoomLimitQmlBindingRefreshesWithPresentationState();
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

void ImageViewportPublicApiQmlTest::qmlRemovedSequencePropertyPreservesDefaultState()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    property bool removedSequencePropertyPreservedState: false

    Component.onCompleted: {
        removedSequencePropertyPreservedState = typeof viewport.sequence === "undefined"
            && state.primary.sequence === null
            && state.request.status === ImageViewport.RequestStatus.NoRequest
            && state.display.status === ImageViewport.DisplayStatus.Empty
            && !state.revisions.request.valid
            && !state.revisions.display.valid
            && state.diagnostics.errorString === ""
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    QCOMPARE(object->property("removedSequencePropertyPreservedState").toBool(), true);
}

void ImageViewportPublicApiQmlTest::qmlRemovedSequencePropertyPreservesReadyState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
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
    property imageViewportPageSet pageSet
    property pageSetTransitionPolicy policy
    property bool initialSetAccepted: false
    property bool removedSequencePropertyPreservedReadyState: false

    function readyStatePreserved(requestRevisionBefore, displayRevisionBefore) {
        return typeof viewport.sequence === "undefined"
            && initialSetAccepted
            && state.primary.sequence === suppliedSequence
            && state.request.status === ImageViewport.RequestStatus.Ready
            && state.request.reason === ImageViewport.RequestReason.Ready
            && state.display.status === ImageViewport.DisplayStatus.Ready
            && state.primary.request.frame === 0
            && state.primary.display.frame === 0
            && state.revisions.request === requestRevisionBefore
            && state.revisions.display === displayRevisionBefore
            && state.diagnostics.errorString === ""
    }

    function verifyRemovedSequencePropertyPreservesReadyState() {
        const requestRevisionBefore = state.revisions.request
        const displayRevisionBefore = state.revisions.display
        removedSequencePropertyPreservedReadyState =
            readyStatePreserved(requestRevisionBefore, displayRevisionBefore)
    }

    Component.onCompleted: {
        pageSet.primary = suppliedSequence
        initialSetAccepted = setPageSet(pageSet, policy) === ImageViewport.CommandOutcome.Accepted
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
    QVERIFY(QMetaObject::invokeMethod(
        object.data(), "verifyRemovedSequencePropertyPreservesReadyState"));
    QCOMPARE(object->property("removedSequencePropertyPreservedReadyState").toBool(), true);
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
    property bool manualZoomHelpersRemoved: false
    property bool coordinateAliasesAvailable: false
    property imageViewportPageSet pageSet
    property pageSetTransitionPolicy policy
    property imageViewportPresentationCommand spreadDirectionCommand
    property imageViewportPresentationCommand pageGapCommand
    property imageViewportPresentationCommand fitModeCommand
    property imageViewportPresentationCommand manualZoomCommand
    property imageViewportPresentationCommand zoomStepCommand
    property imageViewportPresentationCommand panCommand
    property imageViewportPresentationCommand scanStartCommand
    property imageViewportPresentationCommand scanEndCommand
    property imageViewportPresentationCommand scanNextCommand
    property imageViewportPresentationCommand scanPreviousCommand
    property imageViewportPresentationCommand rotationCommand
    property imageViewportPresentationCommand horizontalMirrorCommand
    property imageViewportPresentationCommand verticalMirrorCommand
    property imageViewportCoordinateInput coordinateInput

    function nearlyEqual(left, right) {
        return Math.abs(left - right) < 0.000001
    }

    Component.onCompleted: {
        defaultsValid = typeof viewport.sequence === "undefined"
            && state.primary.sequence === null
            && state.secondary.sequence === null
            && state.presentation.spreadDirection === ImageViewport.SpreadDirection.LeftToRight
            && state.presentation.pageGap === 0
            && state.primary.display.frame === -1
            && state.secondary.display.frame === -1
            && state.primary.request.position === -1
            && state.secondary.request.position === -1
            && state.primary.metadata.frameCount === -1
            && state.secondary.metadata.frameCount === -1
            && state.primary.metadata.frameSeekBounds.minimum === -1
            && state.secondary.metadata.frameSeekBounds.maximum === -1
            && state.primary.metadata.timedPlaybackSupport === ImageViewport.CapabilitySupport.Unavailable
            && state.secondary.metadata.timedPlaybackSupport === ImageViewport.CapabilitySupport.Unavailable
            && state.display.spreadSize.width === 0
            && state.primary.display.sourceLogicalSize.height <= 0
            && state.secondary.display.sourceLogicalSize.width <= 0
            && state.display.visibleSpreadRect.width === 0
            && state.primary.geometry.acceptedPageRect.height === 0
            && state.secondary.geometry.acceptedItemRect.width === 0
            && state.primary.geometry.acceptedVisiblePageRect.height === 0
            && state.display.contentSize.width === 0
            && state.display.contentPosition.x === 0
            && state.display.maximumContentPosition.y === 0
            && state.display.horizontalPannable === false
            && state.display.verticalPannable === false
            && state.presentation.fitMode === ImageViewport.FitMode.Contain
            && state.presentation.zoomPercent === 100
            && state.presentation.minimumManualZoomPercent > 0
            && state.presentation.maximumManualZoomPercent === ImageViewportDisplayLimits.maximumManualZoomPercent
            && state.presentation.manualZoomStepFactor === 1.25
            && state.presentation.rotationDegrees === 0

        const requestRevisionBefore = state.revisions.request
        const displayRevisionBefore = state.revisions.display
        const commandRevisionBefore = state.revisions.command
        const minimum = state.presentation.minimumManualZoomPercent
        const maximum = state.presentation.maximumManualZoomPercent
        manualZoomHelpersRemoved = typeof viewport.clampedManualZoomPercent === "undefined"
            && typeof viewport.steppedManualZoomPercent === "undefined"
            && typeof viewport.zoomByStep === "undefined"
            && typeof viewport.scanNext === "undefined"
            && typeof viewport.scanPrevious === "undefined"
            && typeof viewport.rotateClockwise === "undefined"
            && typeof viewport.rotateCounterClockwise === "undefined"
            && typeof viewport.setMirrorHorizontally === "undefined"
            && typeof viewport.setMirrorVertically === "undefined"
            && typeof viewport.panToStart === "undefined"
            && typeof viewport.panToEnd === "undefined"
            && typeof viewport.panBy === "undefined"
            && typeof viewport.setSpreadDirection === "undefined"
            && typeof viewport.setPageGap === "undefined"
            && typeof viewport.setFitMode === "undefined"
            && typeof viewport.setZoomPercent === "undefined"
            && typeof viewport.spreadDirection === "undefined"
            && typeof viewport.pageGap === "undefined"
            && typeof viewport.fitMode === "undefined"
            && typeof viewport.zoomPercent === "undefined"
            && typeof viewport.minimumManualZoomPercent === "undefined"
            && typeof viewport.maximumManualZoomPercent === "undefined"
            && typeof viewport.manualZoomStepFactor === "undefined"
            && typeof viewport.rotationDegrees === "undefined"
            && minimum > 0
            && maximum === ImageViewportDisplayLimits.maximumManualZoomPercent
            && state.revisions.request === requestRevisionBefore
            && state.revisions.display === displayRevisionBefore
            && state.revisions.command === commandRevisionBefore

        policy.pageGap = -1
        policy.pageGapTransition = 1
        const invalidPageSetOutcome = setPageSet(pageSet, policy)
        pageSetValidationPreservedState = invalidPageSetOutcome === ImageViewport.CommandOutcome.Invalid
            && state.primary.sequence === null
            && state.secondary.sequence === null
            && state.request.status === ImageViewport.RequestStatus.NoRequest
            && state.display.status === ImageViewport.DisplayStatus.Empty
            && state.revisions.request === requestRevisionBefore
            && state.revisions.display === displayRevisionBefore

        roleCommandsReachViewport = play(ImageViewport.PageRole.Secondary) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && pause(ImageViewport.PageRole.Secondary) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && stop(ImageViewport.PageRole.Secondary) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && seek(ImageViewport.PageRole.Secondary, 0) === ImageViewport.CommandOutcome.IgnoredNoRequest
            && seekToPosition(ImageViewport.PageRole.Secondary, 0) === ImageViewport.CommandOutcome.IgnoredNoRequest

        spreadDirectionCommand.spreadDirection = ImageViewport.SpreadDirection.LeftToRight
        pageGapCommand.pageGap = 0
        fitModeCommand.fitMode = ImageViewport.FitMode.Contain
        manualZoomCommand.manualZoomPercent = 100
        zoomStepCommand.zoomStepDelta = 1
        scanStartCommand.scanDirection = ImageViewport.ScanDirection.Start
        scanEndCommand.scanDirection = ImageViewport.ScanDirection.End
        scanNextCommand.scanDirection = ImageViewport.ScanDirection.Next
        scanPreviousCommand.scanDirection = ImageViewport.ScanDirection.Previous
        rotationCommand.rotationDegrees = 0
        horizontalMirrorCommand.mirrorHorizontally = false
        verticalMirrorCommand.mirrorVertically = false
        panCommand.panDelta = Qt.point(0, 0)
        presentationCommandsReachViewport = setPresentation(spreadDirectionCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(pageGapCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(fitModeCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(manualZoomCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(zoomStepCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(panCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(scanStartCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(scanEndCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(scanNextCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(scanPreviousCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(rotationCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(horizontalMirrorCommand) === ImageViewport.CommandOutcome.Accepted
            && setPresentation(verticalMirrorCommand) === ImageViewport.CommandOutcome.Accepted
            && resetView() === ImageViewport.CommandOutcome.Accepted

        coordinateInput.sourceSpace = ImageViewport.CoordinateSpace.Spread
        coordinateInput.targetSpace = ImageViewport.CoordinateSpace.Spread
        coordinateInput.point = Qt.point(1, 1)
        const spreadNearestInvalid = nearestVisiblePoint(coordinateInput).valid === false
        const spreadContainsInvalid = containsPoint(coordinateInput) === false
        coordinateInput.sourceSpace = ImageViewport.CoordinateSpace.Item
        coordinateInput.targetSpace = ImageViewport.CoordinateSpace.Spread
        const itemToSpreadInvalid = mapPoint(coordinateInput).valid === false
        coordinateInput.sourceSpace = ImageViewport.CoordinateSpace.Spread
        coordinateInput.targetSpace = ImageViewport.CoordinateSpace.Item
        const spreadToItemInvalid = mapPoint(coordinateInput).valid === false
        coordinateInput.sourceSpace = ImageViewport.CoordinateSpace.Page
        coordinateInput.targetSpace = ImageViewport.CoordinateSpace.Page
        coordinateInput.pageRole = ImageViewport.PageRole.Primary
        const pageNearestInvalid = nearestVisiblePoint(coordinateInput).valid === false
        const pageContainsInvalid = containsPoint(coordinateInput) === false
        coordinateInput.sourceSpace = ImageViewport.CoordinateSpace.Item
        coordinateInput.targetSpace = ImageViewport.CoordinateSpace.Page
        const itemToPageInvalid = mapPoint(coordinateInput).valid === false
        coordinateInput.sourceSpace = ImageViewport.CoordinateSpace.Page
        coordinateInput.targetSpace = ImageViewport.CoordinateSpace.Item
        const pageToItemInvalid = mapPoint(coordinateInput).valid === false
        coordinateAliasesAvailable = itemToSpreadInvalid
            && spreadToItemInvalid
            && itemToPageInvalid
            && pageToItemInvalid
            && spreadNearestInvalid
            && pageNearestInvalid
            && spreadContainsInvalid
            && pageContainsInvalid
            && nearestVisiblePoint(coordinateInput).valid === false
            && containsPoint(coordinateInput) === false
            && typeof viewport.itemToImage === "undefined"
            && typeof viewport.imageToItem === "undefined"
            && typeof viewport.nearestVisibleImagePoint === "undefined"
            && typeof viewport.containsVisibleImagePoint === "undefined"
            && typeof viewport.itemToSpread === "undefined"
            && typeof viewport.spreadToItem === "undefined"
            && typeof viewport.itemToPage === "undefined"
            && typeof viewport.pageToItem === "undefined"
            && typeof viewport.primaryPageGeometry === "undefined"
            && typeof viewport.secondaryPageGeometry === "undefined"
            && typeof viewport.pageGeometry === "undefined"
            && typeof viewport.nearestVisibleSpreadPoint === "undefined"
            && typeof viewport.nearestVisiblePagePoint === "undefined"
            && typeof viewport.containsVisibleSpreadPoint === "undefined"
            && typeof viewport.containsVisiblePagePoint === "undefined"
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
    QCOMPARE(object->property("manualZoomHelpersRemoved").toBool(), true);
    QCOMPARE(object->property("coordinateAliasesAvailable").toBool(), true);
}

void ImageViewportPublicApiQmlTest::manualZoomLimitQmlBindingRefreshesWithPresentationState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QVERIFY(result->sequence());

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));
    BindingRefreshRecorder recorder;
    engine.rootContext()->setContextProperty(QStringLiteral("recorder"), &recorder);

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    width: 100
    height: 100

    property ImageSequence suppliedSequence
    property imageViewportPageSet pageSet
    property pageSetTransitionPolicy policy
    property real observedMaximum: recorder.rememberMaximum(state.presentation.maximumManualZoomPercent)

    Component.onCompleted: {
        pageSet.primary = suppliedSequence
        setPageSet(pageSet, policy)
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
    QCoreApplication::processEvents();
    const int refreshCountBefore = recorder.count();

    ImageViewportPresentationCommand zoomCommand;
    zoomCommand.setManualZoomPercent(200.0);
    QCOMPARE(viewport->setPresentation(zoomCommand), ImageViewport::CommandOutcome::Accepted);
    QCoreApplication::processEvents();

    QVERIFY(recorder.count() > refreshCountBefore);
    QCOMPARE(object->property("observedMaximum").toDouble(),
        viewport->state().presentation().maximumManualZoomPercent());
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
    property imageViewportCoordinateInput mappingInput
    property bool mappingInvalid: mapPoint(mappingInput).valid === false
    property bool mappingHasPointField: mapPoint(mappingInput).point.x === 0
        && mapPoint(mappingInput).point.y === 0
    property bool unavailableValuesHaveDocumentedFields: state.primary.metadata.frameSeekBounds.minimum === -1
        && state.primary.metadata.frameSeekBounds.maximum === -1
        && state.primary.metadata.positionSeekBounds.minimum === -1
        && state.primary.metadata.positionSeekBounds.maximum === -1
        && state.display.contentRect.width === 0
        && state.primary.geometry.displayedVisiblePageRect.height === 0
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
    QCOMPARE(object->property("mappingHasPointField").toBool(), true);
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
    property imageViewportPageSet pageSet
    property pageSetTransitionPolicy policy
    property bool readyValuesHaveDocumentedFields: state.primary.display.sourceLogicalSize.width === 16
        && state.primary.display.sourceLogicalSize.height === 8
        && state.display.contentRect.x === 0
        && state.display.contentRect.y === 25
        && state.display.contentRect.width === 100
        && state.display.contentRect.height === 50
        && state.primary.geometry.displayedVisiblePageRect.x === 0
        && state.primary.geometry.displayedVisiblePageRect.y === 0
        && state.primary.geometry.displayedVisiblePageRect.width === 16
        && state.primary.geometry.displayedVisiblePageRect.height === 8
        && state.primary.metadata.frameSeekBounds.minimum === 0
        && state.primary.metadata.frameSeekBounds.maximum === 0
        && state.primary.metadata.positionSeekBounds.minimum === -1
        && state.primary.metadata.positionSeekBounds.maximum === -1

    Component.onCompleted: {
        pageSet.primary = suppliedSequence
        setPageSet(pageSet, policy)
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
