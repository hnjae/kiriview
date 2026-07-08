#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

#include <cmath>
#include <limits>
#include <type_traits>

static_assert(!std::is_constructible_v<ImageSequenceProviderRequestToken, quint64>,
    "provider request tokens must not expose numeric construction as public API");
static_assert(!std::is_constructible_v<RevisionToken, quint64>,
    "revision tokens must not expose numeric construction as public API");

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
    void exposesDocumentedQmlSurface();
    void exposesFinalApiScaffold();
    void exposesTypedPublicValueSurfaces();
    void hasDocumentedDefaultState();
    void manualZoomLimitPropertiesExposeDefaultsAndDoNotAdvanceRevisions();
    void nearestVisibleHelpersExposeInvalidDefaultsAndDoNotAdvanceRevisions();
    void typedPublicValueDefaultsExposeDocumentedFields();
    void pageGeometryValueTypeFields();
    void revisionTokensExposeValidityAndEquality();
    void typedPageSetTransitionPolicyPreservesStateWhenInvalid();
    void emptyGeometryChangeIncrementsDisplayRevision();
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
        "fillMode",
        "horizontalAlignment",
        "verticalAlignment",
        "zoom",
        "pan",
        "spreadDirection",
        "pageGap",
        "fitMode",
        "zoomPercent",
        "minimumManualZoomPercent",
        "maximumManualZoomPercent",
        "manualZoomStepFactor",
        "rotationDegrees",
        "smoothing",
        "mipmap",
        "mirrorHorizontally",
        "mirrorVertically",
        "backgroundMode",
        "backgroundColor",
        "looping",
    };

    for (const QByteArray& property : absentProperties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) < 0, property.constData());
    }

    const QList<QByteArray> absentEnumerators = {
        "FillMode",
        "HorizontalAlignment",
        "VerticalAlignment",
    };

    for (const QByteArray& enumerator : absentEnumerators) {
        QVERIFY2(metaObject->indexOfEnumerator(enumerator.constData()) < 0, enumerator.constData());
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
        "clampedManualZoomPercent(double)",
        "steppedManualZoomPercent(int)",
        "zoomByStep(int,QPointF)",
        "scanNext()",
        "scanPrevious()",
        "rotateClockwise(QPointF)",
        "rotateCounterClockwise(QPointF)",
        "setMirrorHorizontally(bool,QPointF)",
        "setMirrorVertically(bool,QPointF)",
        "panToStart()",
        "panToEnd()",
        "panBy(QPointF)",
        "setSpreadDirection(ImageViewport::SpreadDirection)",
        "setPageGap(double)",
        "setFitMode(ImageViewport::FitMode,QPointF)",
        "setZoomPercent(double,QPointF)",
    };

    for (const QByteArray& method : absentMethods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) < 0,
            method.constData());
    }

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
        "contentRect",
        "visibleImageRect",
        "state",
        "displayRevision",
        "requestRevision",
        "commandRevision",
        "errorString",
        "warningString",
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
        "primaryPageGeometry",
        "secondaryPageGeometry",
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
    };

    for (const QByteArray& property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QMetaObject& presentationMetaObject = ImageViewportPresentationSnapshot::staticMetaObject;
    const QList<QByteArray> presentationProperties = {
        "fitMode",
        "zoomPercent",
        "minimumManualZoomPercent",
        "maximumManualZoomPercent",
        "manualZoomStepFactor",
        "rotationDegrees",
        "mirrorHorizontally",
        "mirrorVertically",
        "spreadDirection",
        "pageGap",
        "backgroundMode",
        "backgroundColor",
        "smoothing",
        "mipmap",
        "looping",
        "qualityPreference",
        "exactnessPreference",
    };
    for (const QByteArray& property : presentationProperties) {
        QVERIFY2(presentationMetaObject.indexOfProperty(property.constData()) >= 0,
            property.constData());
    }

    const QList<QByteArray> enumerators = {
        "PageRole",
        "SpreadDirection",
        "FitMode",
        "ScanDirection",
    };

    for (const QByteArray& enumerator : enumerators) {
        QVERIFY2(
            metaObject->indexOfEnumerator(enumerator.constData()) >= 0, enumerator.constData());
    }

    verifyEnumValues(metaObject, "PageRole", { "Primary", "Secondary" });
    verifyEnumValues(metaObject, "SpreadDirection", { "LeftToRight", "RightToLeft" });
    verifyEnumValues(metaObject, "FitMode", { "Contain", "FitWidth", "FitHeight", "Manual" });
    verifyEnumValues(metaObject, "ScanDirection", { "Start", "Previous", "Next", "End" });

    const QList<QByteArray> methods = {
        "setPageSet(QVariant)",
        "setPageSet(QVariant,QVariant)",
        "setPageSet(QVariant,QVariant,PageSetTransitionPolicy)",
        "pageGeometry(ImageViewport::PageRole)",
        "itemToSpread(double,double)",
        "spreadToItem(double,double)",
        "containsVisibleSpreadPoint(double,double)",
        "nearestVisibleSpreadPoint(double,double)",
        "nearestVisiblePagePoint(ImageViewport::PageRole,double,double)",
        "nearestVisibleImagePoint(double,double)",
    };

    for (const QByteArray& method : methods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0,
            method.constData());
    }
}

void ImageViewportPublicApiTest::exposesTypedPublicValueSurfaces()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    const QList<QByteArray> rangeProperties = {
        "frameSeekBounds",
        "positionSeekBounds",
        "primaryFrameSeekBounds",
        "secondaryFrameSeekBounds",
        "primaryPositionSeekBounds",
        "secondaryPositionSeekBounds",
    };
    for (const QByteArray& propertyName : rangeProperties) {
        const int index = metaObject->indexOfProperty(propertyName.constData());
        QVERIFY2(index >= 0, propertyName.constData());
        QCOMPARE(
            QByteArray(metaObject->property(index).typeName()), QByteArray("ImageViewportRange"));
    }

    const QList<QByteArray> revisionProperties = {
        "displayRevision",
        "requestRevision",
        "commandRevision",
    };
    for (const QByteArray& propertyName : revisionProperties) {
        const int index = metaObject->indexOfProperty(propertyName.constData());
        QVERIFY2(index >= 0, propertyName.constData());
        QCOMPARE(QByteArray(metaObject->property(index).typeName()), QByteArray("RevisionToken"));
    }

    const QList<QByteArray> coordinateMethods = {
        "itemToSpread(double,double)",
        "spreadToItem(double,double)",
        "itemToPage(ImageViewport::PageRole,double,double)",
        "pageToItem(ImageViewport::PageRole,double,double)",
        "nearestVisibleSpreadPoint(double,double)",
        "nearestVisiblePagePoint(ImageViewport::PageRole,double,double)",
        "itemToImage(double,double)",
        "imageToItem(double,double)",
        "nearestVisibleImagePoint(double,double)",
    };
    for (const QByteArray& methodName : coordinateMethods) {
        const int index = metaObject->indexOfMethod(QMetaObject::normalizedSignature(methodName));
        QVERIFY2(index >= 0, methodName.constData());
        QCOMPARE(QByteArray(metaObject->method(index).typeName()), QByteArray("CoordinateResult"));
    }

    const QList<QByteArray> pageGeometryProperties = {
        "primaryPageGeometry",
        "secondaryPageGeometry",
    };
    for (const QByteArray& propertyName : pageGeometryProperties) {
        const int index = metaObject->indexOfProperty(propertyName.constData());
        QVERIFY2(index >= 0, propertyName.constData());
        QCOMPARE(QByteArray(metaObject->property(index).typeName()), QByteArray("PageGeometry"));
    }

    const int pageGeometryMethodIndex = metaObject->indexOfMethod(
        QMetaObject::normalizedSignature("pageGeometry(ImageViewport::PageRole)"));
    QVERIFY(pageGeometryMethodIndex >= 0);
    QCOMPARE(QByteArray(metaObject->method(pageGeometryMethodIndex).typeName()),
        QByteArray("PageGeometry"));

    const QMetaObject& policyMetaObject = PageSetTransitionPolicy::staticMetaObject;
    const QList<QByteArray> policyProperties = {
        "displayTransition",
        "zoomTransition",
        "contentPositionTransition",
        "rotationTransition",
        "mirrorTransition",
        "fitModeTransition",
        "fitMode",
        "spreadDirectionTransition",
        "spreadDirection",
        "pageGapTransition",
        "pageGap",
        "replacementIntent",
    };
    for (const QByteArray& propertyName : policyProperties) {
        QVERIFY2(policyMetaObject.indexOfProperty(propertyName.constData()) >= 0,
            propertyName.constData());
    }

    verifyEnumValues(&policyMetaObject, "FitModeTransition", { "Preserve", "SetExplicit" });
    verifyEnumValues(&policyMetaObject, "SpreadDirectionTransition", { "Preserve", "SetExplicit" });
    verifyEnumValues(&policyMetaObject, "PageGapTransition", { "Preserve", "SetExplicit" });

    const QMetaObject& presentationCommandMetaObject
        = ImageViewportPresentationCommand::staticMetaObject;
    const QList<QByteArray> presentationCommandProperties = {
        "scanDirectionSet",
        "scanDirection",
        "qualityPreferenceSet",
        "qualityPreference",
        "exactnessPreferenceSet",
        "exactnessPreference",
    };
    for (const QByteArray& propertyName : presentationCommandProperties) {
        QVERIFY2(presentationCommandMetaObject.indexOfProperty(propertyName.constData()) >= 0,
            propertyName.constData());
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
    QCOMPARE(rangeProperty(item, "frameSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "frameSeekBounds").maximum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").minimum(), -1);
    QCOMPARE(rangeProperty(item, "positionSeekBounds").maximum(), -1);
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
    verifyInvalidCoordinateResult(item.nearestVisibleSpreadPoint(1.0, 1.0));
    verifyInvalidCoordinateResult(
        item.nearestVisiblePagePoint(ImageViewport::PageRole::Primary, 1.0, 1.0));
    verifyInvalidCoordinateResult(item.nearestVisibleImagePoint(1.0, 1.0));
    QCOMPARE(item.containsVisibleImagePoint(1.0, 1.0), false);
    QVERIFY(!revisionTokenProperty(item, "displayRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "requestRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    QCOMPARE(presentation.smoothing(), true);
    QCOMPARE(presentation.mipmap(), false);
    QCOMPARE(presentation.mirrorHorizontally(), false);
    QCOMPARE(presentation.mirrorVertically(), false);
    QCOMPARE(presentation.backgroundMode(), ImageViewport::BackgroundMode::Transparent);
    QCOMPARE(presentation.backgroundColor(), QColor(Qt::transparent));
    QCOMPARE(presentation.looping(), false);
    QVERIFY(presentation.minimumManualZoomPercent() > 0.0);
    QCOMPARE(presentation.maximumManualZoomPercent(),
        ImageViewportDisplayLimits::maximumManualZoomPercent());
    QCOMPARE(presentation.manualZoomStepFactor(), 1.25);
}

void ImageViewportPublicApiTest::manualZoomLimitPropertiesExposeDefaultsAndDoNotAdvanceRevisions()
{
    ImageViewport item;
    const RevisionToken displayRevision = item.displayRevision();
    const RevisionToken requestRevision = item.requestRevision();
    const RevisionToken commandRevision = item.commandRevision();
    QSignalSpy displaySpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy requestSpy(&item, &ImageViewport::requestRevisionChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandRevisionChanged);

    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    const double minimum = presentation.minimumManualZoomPercent();
    const double maximum = presentation.maximumManualZoomPercent();
    const double stepFactor = presentation.manualZoomStepFactor();

    QVERIFY(std::isfinite(minimum));
    QVERIFY(minimum > 0.0);
    QCOMPARE(maximum, ImageViewportDisplayLimits::maximumManualZoomPercent());
    QCOMPARE(stepFactor, 1.25);

    QCOMPARE(item.displayRevision(), displayRevision);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.commandRevision(), commandRevision);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(commandSpy.count(), 0);
}

void ImageViewportPublicApiTest::
    nearestVisibleHelpersExposeInvalidDefaultsAndDoNotAdvanceRevisions()
{
    ImageViewport item;
    const RevisionToken displayRevision = item.displayRevision();
    const RevisionToken requestRevision = item.requestRevision();
    const RevisionToken commandRevision = item.commandRevision();
    QSignalSpy displaySpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy requestSpy(&item, &ImageViewport::requestRevisionChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandRevisionChanged);

    const double infinity = std::numeric_limits<double>::infinity();
    verifyInvalidCoordinateResult(item.nearestVisibleSpreadPoint(1.0, 1.0));
    verifyInvalidCoordinateResult(item.nearestVisibleSpreadPoint(infinity, 1.0));
    verifyInvalidCoordinateResult(
        item.nearestVisiblePagePoint(ImageViewport::PageRole::Primary, 1.0, 1.0));
    verifyInvalidCoordinateResult(
        item.nearestVisiblePagePoint(static_cast<ImageViewport::PageRole>(-1), 1.0, 1.0));
    verifyInvalidCoordinateResult(item.nearestVisibleImagePoint(1.0, 1.0));

    QCOMPARE(item.displayRevision(), displayRevision);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.commandRevision(), commandRevision);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(commandSpy.count(), 0);
}

void ImageViewportPublicApiTest::typedPublicValueDefaultsExposeDocumentedFields()
{
    ImageViewport item;

    const ImageViewportRange frameBounds = item.frameSeekBounds();
    QCOMPARE(frameBounds.minimum(), -1);
    QCOMPARE(frameBounds.maximum(), -1);

    const ImageViewportRange positionBounds = item.positionSeekBounds();
    QCOMPARE(positionBounds.minimum(), -1);
    QCOMPARE(positionBounds.maximum(), -1);

    const CoordinateResult itemToImage = item.itemToImage(1.0, 1.0);
    QCOMPARE(itemToImage.isValid(), false);
    QCOMPARE(itemToImage.x(), 0.0);
    QCOMPARE(itemToImage.y(), 0.0);

    const CoordinateResult imageToItem = item.imageToItem(1.0, 1.0);
    QCOMPARE(imageToItem.isValid(), false);
    QCOMPARE(imageToItem.x(), 0.0);
    QCOMPARE(imageToItem.y(), 0.0);

    const PageGeometry primaryGeometry = item.primaryPageGeometry();
    QCOMPARE(primaryGeometry.role(), ImageViewport::PageRole::Primary);
    QCOMPARE(primaryGeometry.isAvailable(), false);
    QCOMPARE(primaryGeometry.pageRect(), QRectF());
    QCOMPARE(primaryGeometry.itemRect(), QRectF());
    QCOMPARE(primaryGeometry.visiblePageRect(), QRectF());

    const PageGeometry secondaryGeometry = item.secondaryPageGeometry();
    QCOMPARE(secondaryGeometry.role(), ImageViewport::PageRole::Secondary);
    QCOMPARE(secondaryGeometry.isAvailable(), false);
    QCOMPARE(secondaryGeometry.pageRect(), QRectF());
    QCOMPARE(secondaryGeometry.itemRect(), QRectF());
    QCOMPARE(secondaryGeometry.visiblePageRect(), QRectF());
}

void ImageViewportPublicApiTest::pageGeometryValueTypeFields()
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
    item.setSize(QSizeF(26.0, 20.0));
    QCOMPARE(item.setPageSet(primaryResult->sequence(), secondaryResult->sequence()),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);

    const PageGeometry primaryGeometry = item.primaryPageGeometry();
    QCOMPARE(primaryGeometry.role(), ImageViewport::PageRole::Primary);
    QCOMPARE(primaryGeometry.isAvailable(), true);
    QCOMPARE(primaryGeometry.pageRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(primaryGeometry.itemRect(), item.primaryItemRect());
    QCOMPARE(primaryGeometry.itemRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(primaryGeometry.visiblePageRect(), item.visiblePrimaryPageRect());
    QCOMPARE(primaryGeometry.visiblePageRect(), QRectF(0.0, 0.0, 16.0, 8.0));

    const PageGeometry secondaryGeometry = item.secondaryPageGeometry();
    QCOMPARE(secondaryGeometry.role(), ImageViewport::PageRole::Secondary);
    QCOMPARE(secondaryGeometry.isAvailable(), true);
    QCOMPARE(secondaryGeometry.pageRect(), QRectF(16.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryGeometry.itemRect(), item.secondaryItemRect());
    QCOMPARE(secondaryGeometry.itemRect(), QRectF(16.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryGeometry.visiblePageRect(), item.visibleSecondaryPageRect());
    QCOMPARE(secondaryGeometry.visiblePageRect(), QRectF(0.0, 0.0, 10.0, 20.0));

    const PageGeometry selectedSecondaryGeometry
        = item.pageGeometry(ImageViewport::PageRole::Secondary);
    QCOMPARE(selectedSecondaryGeometry, secondaryGeometry);

    ImageViewport primaryOnly;
    primaryOnly.setSize(QSizeF(16.0, 8.0));
    primaryOnly.setSequence(primaryResult->sequence());
    acknowledgePendingRenderCommitForTest(primaryOnly);

    const PageGeometry unavailableSecondary = primaryOnly.secondaryPageGeometry();
    QCOMPARE(unavailableSecondary.role(), ImageViewport::PageRole::Secondary);
    QCOMPARE(unavailableSecondary.isAvailable(), false);
    QCOMPARE(unavailableSecondary.pageRect(), QRectF());
    QCOMPARE(unavailableSecondary.itemRect(), QRectF());
    QCOMPARE(unavailableSecondary.visiblePageRect(), QRectF());
    QCOMPARE(primaryOnly.pageGeometry(ImageViewport::PageRole::Secondary), unavailableSecondary);
}

void ImageViewportPublicApiTest::revisionTokensExposeValidityAndEquality()
{
    const QMetaObject& revisionTokenMetaObject = RevisionToken::staticMetaObject;
    QVERIFY(revisionTokenMetaObject.indexOfProperty("valid") >= 0);
    QVERIFY(revisionTokenMetaObject.indexOfProperty("value") < 0);

    ImageViewport item;
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);

    const RevisionToken initialDisplayRevision = item.displayRevision();
    const RevisionToken initialRequestRevision = item.requestRevision();
    const RevisionToken initialCommandRevision = item.commandRevision();
    QVERIFY(!initialDisplayRevision.isValid());
    QVERIFY(!initialRequestRevision.isValid());
    QVERIFY(!initialCommandRevision.isValid());

    item.setSize(QSizeF(100.0, 50.0));

    const RevisionToken changedDisplayRevision = item.displayRevision();
    QVERIFY(changedDisplayRevision.isValid());
    QVERIFY(changedDisplayRevision != initialDisplayRevision);
    QCOMPARE(item.displayRevision(), changedDisplayRevision);
    QCOMPARE(item.requestRevision(), initialRequestRevision);
    QCOMPARE(item.commandRevision(), initialCommandRevision);
    QCOMPARE(displayRevisionSpy.count(), 1);
}

void ImageViewportPublicApiTest::typedPageSetTransitionPolicyPreservesStateWhenInvalid()
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
    const RevisionToken requestRevision = item.requestRevision();
    const RevisionToken displayRevision = item.displayRevision();
    const RevisionToken commandRevision = item.commandRevision();
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);

    PageSetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(PageSetTransitionPolicy::PageGapTransition::SetExplicit);

    const auto outcome = item.setPageSet(
        QVariant::fromValue<QObject*>(replacementResult->sequence()), {}, invalidPolicy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.sequence(), firstResult->sequence());
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.displayRevision(), displayRevision);
    QVERIFY(item.commandRevision().isValid());
    QVERIFY(item.commandRevision() != commandRevision);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
}

void ImageViewportPublicApiTest::emptyGeometryChangeIncrementsDisplayRevision()
{
    ImageViewport item;
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setSize(QSizeF(100.0, 50.0));

    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(displayRevisionSpy.count(), 1);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(item.property("displayStatus").toInt(),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());

    item.setSize(QSizeF(100.0, 50.0));

    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(displayRevisionSpy.count(), 1);

    const double changedWidth = 100.0 + 5.0e-13;
    QVERIFY(changedWidth != 100.0);
    item.setSize(QSizeF(changedWidth, 50.0));

    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(displayRevisionSpy.count(), 2);
}
QTEST_MAIN(ImageViewportPublicApiTest)

#include "tst_imageviewport_public_api.moc"
