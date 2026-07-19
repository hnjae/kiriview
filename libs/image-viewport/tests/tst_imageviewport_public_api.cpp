// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

#include <QtCore/QMetaProperty>

#include <cmath>
#include <limits>
#include <type_traits>

static_assert(!std::is_constructible_v<ImageSequenceProviderRequestToken, quint64>,
    "provider request tokens must not expose numeric construction as public API");
static_assert(!std::is_constructible_v<ImageViewportRevisionToken, quint64>,
    "revision tokens must not expose numeric construction as public API");
static_assert(!std::is_constructible_v<ImageViewportAllocationGenerationToken, quint64>,
    "allocation generation tokens must not expose numeric construction as public API");
static_assert(!std::is_same_v<ImageViewportAllocationGenerationToken,
                  ImageViewportPresentationTargetGenerationToken>,
    "allocation and presentation-target generations must remain distinct identity domains");

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
    void coordinateHelpersExposeInvalidDefaultsAndDoNotAdvanceRevisions();
    void typedPublicValueDefaultsExposeDocumentedFields();
    void roleMetadataSurfaceMatchesContract();
    void roleGeometrySnapshotFields();
    void revisionTokensExposeValidityAndEquality();
    void typedPresentationTargetTransitionPolicyPreservesStateWhenInvalid();
    void targetlessGeometryChangeIsRevisionNeutral();
};

void ImageViewportPublicApiTest::roleMetadataSurfaceMatchesContract()
{
    const QMetaObject& metaObject = ImageViewportRoleMetadataSnapshot::staticMetaObject;
    const int autoplayIndex = metaObject.indexOfProperty("autoplay");
    QVERIFY(autoplayIndex >= 0);
    QCOMPARE(metaObject.property(autoplayIndex).metaType(),
        QMetaType::fromType<ImageViewportCapabilitySupport>());
    QCOMPARE(metaObject.indexOfProperty("progressiveAnimationReadiness"), -1);
}

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
        "primaryPageGeometry",
        "secondaryPageGeometry",
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
        "displayedImageSize",
        "contentRect",
        "visibleImageRect",
        "primaryFrameCount",
        "secondaryFrameCount",
        "primaryTotalDuration",
        "secondaryTotalDuration",
        "primaryFrameSeekBounds",
        "secondaryFrameSeekBounds",
        "primaryPositionSeekBounds",
        "secondaryPositionSeekBounds",
        "frameCount",
        "totalDuration",
        "frameSeekBounds",
        "positionSeekBounds",
        "timedPlaybackSupport",
        "frameSeekSupport",
        "positionSeekSupport",
        "primaryDisplayedFrame",
        "primaryRequestedFrame",
        "secondaryDisplayedFrame",
        "secondaryRequestedFrame",
        "primaryDisplayedPosition",
        "primaryRequestedPosition",
        "secondaryDisplayedPosition",
        "secondaryRequestedPosition",
        "displayedFrame",
        "requestedFrame",
        "displayedPosition",
        "requestedPosition",
        "requestStatus",
        "requestReason",
        "displayStatus",
        "playbackPhase",
        "commandReason",
        "errorString",
        "warningString",
        "displayRevision",
        "requestRevision",
        "commandRevision",
        "sequence",
        "primarySequence",
        "secondarySequence",
        "primaryTimedPlaybackSupport",
        "secondaryTimedPlaybackSupport",
        "primaryFrameSeekSupport",
        "secondaryFrameSeekSupport",
        "primaryPositionSeekSupport",
        "secondaryPositionSeekSupport",
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
        "itemToImage(double,double)",
        "imageToItem(double,double)",
        "nearestVisibleImagePoint(double,double)",
        "containsVisibleImagePoint(double,double)",
        "itemToSpread(double,double)",
        "spreadToItem(double,double)",
        "itemToPage(ImageViewportPageRole,double,double)",
        "pageToItem(ImageViewportPageRole,double,double)",
        "pageGeometry(ImageViewportPageRole)",
        "nearestVisibleSpreadPoint(double,double)",
        "nearestVisiblePagePoint(ImageViewportPageRole,double,double)",
        "nearestVisiblePoint(ImageViewportCoordinateInput)",
        "containsVisibleSpreadPoint(double,double)",
        "containsVisiblePagePoint(ImageViewportPageRole,double,double)",
        "setSpreadDirection(ImageViewportSpreadDirection)",
        "setPageGap(double)",
        "setFitMode(ImageViewportFitMode,QPointF)",
        "setZoomPercent(double,QPointF)",
    };

    for (const QByteArray& method : absentMethods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) < 0,
            method.constData());
    }

    const QList<QByteArray> absentSignals = {
        "requestStateChanged()",
        "displayStateChanged()",
        "diagnosticsChanged()",
        "geometryStateChanged()",
        "commandStateChanged()",
        "commandRevisionChanged()",
        "playbackPhaseChanged()",
        "displayRevisionChanged()",
        "requestRevisionChanged()",
        "presentationChanged()",
        "loopingChanged()",
    };

    for (const QByteArray& signal : absentSignals) {
        QVERIFY2(
            metaObject->indexOfSignal(QMetaObject::normalizedSignature(signal.constData())) < 0,
            signal.constData());
    }
}

void ImageViewportPublicApiTest::exposesDocumentedQmlSurface()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    const QList<QByteArray> properties = {
        "state",
    };

    for (const QByteArray& property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QList<QByteArray> enumerators = {
        "RequestStatus",
        "RequestReason",
        "CommandReason",
        "DisplayStatus",
        "DisplayPhase",
        "PlaybackPhase",
        "CommandOutcome",
        "BackgroundMode",
        "CoordinateSpace",
    };

    for (const QByteArray& enumerator : enumerators) {
        QVERIFY2(
            ImageViewportEnums::staticMetaObject.indexOfEnumerator(enumerator.constData()) >= 0,
            enumerator.constData());
        QVERIFY2(metaObject->indexOfEnumerator(enumerator.constData()) < 0, enumerator.constData());
    }

    QVERIFY(ImageViewportEnums::staticMetaObject.indexOfEnumerator("CapabilitySupport") >= 0);

    verifyEnumValues(
        metaObject, "RequestStatus", { "NoRequest", "Loading", "Ready", "Unsupported", "Error" });
    verifyEnumValues(metaObject, "RequestReason",
        { "NoRequest", "ProviderWaiting", "RequestQueued", "UploadPending", "RenderWaiting",
            "Ready", "UnsupportedRequest", "InvalidRequest", "ProviderFailure", "PayloadRejection",
            "RenderFailure" });
    verifyEnumValues(metaObject, "CommandReason",
        { "NoCommand", "IgnoredNoRequest", "InvalidRequest", "UnsupportedRequest" });
    verifyEnumValues(metaObject, "DisplayStatus", { "Empty", "Ready", "Retained" });
    verifyEnumValues(metaObject, "DisplayPhase",
        { "NoPresentation", "PreviousActive", "TransitioningPlaceholder", "CommittedActive" });
    verifyEnumValues(metaObject, "PlaybackPhase", { "Stopped", "Playing", "Waiting", "Paused" });
    verifyEnumValues(&ImageViewportEnums::staticMetaObject, "CapabilitySupport",
        { "Unavailable", "False", "True" });
    verifyEnumValues(
        metaObject, "CommandOutcome", { "Accepted", "Invalid", "Unsupported", "IgnoredNoRequest" });
    verifyEnumValues(metaObject, "BackgroundMode", { "Transparent", "SolidColor", "Checkerboard" });
    verifyEnumValues(metaObject, "CoordinateSpace", { "Item", "DisplayedSpread", "DisplayedPage" });

    const QList<QByteArray> absentMethods = {
        "play()",
        "pause()",
        "stop()",
        "seek(int)",
        "seekToPosition(int)",
    };

    for (const QByteArray& method : absentMethods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) < 0,
            method.constData());
    }

    const QList<QByteArray> presentMethods = {
        "clear()",
        "resetView()",
        "play(ImageViewportPageRole)",
        "pause(ImageViewportPageRole)",
        "stop(ImageViewportPageRole)",
        "seek(ImageViewportPageRole,int)",
        "seekToPosition(ImageViewportPageRole,int)",
        "mapPoint(ImageViewportCoordinateInput)",
        "containsPoint(ImageViewportCoordinateInput)",
    };

    for (const QByteArray& method : presentMethods) {
        QVERIFY2(
            metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0,
            method.constData());
    }
}

void ImageViewportPublicApiTest::exposesFinalApiScaffold()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    const QMetaObject& presentationMetaObject = ImageViewportPresentationSnapshot::staticMetaObject;
    const QList<QByteArray> presentationProperties = {
        "fitMode",
        "zoomPercent",
        "preferredManualZoomPercent",
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
        "checkerboardLightColor",
        "checkerboardDarkColor",
        "checkerboardCellSize",
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
        "SpreadDirection",
        "FitMode",
        "ContentAnchor",
    };

    for (const QByteArray& enumerator : enumerators) {
        QVERIFY2(metaObject->indexOfEnumerator(enumerator.constData()) < 0, enumerator.constData());
        QVERIFY2(
            ImageViewportEnums::staticMetaObject.indexOfEnumerator(enumerator.constData()) >= 0,
            enumerator.constData());
    }

    verifyEnumValues(&ImageViewportEnums::staticMetaObject, "PageRole", { "Primary", "Secondary" });
    verifyEnumValues(
        &ImageViewportEnums::staticMetaObject, "SpreadDirection", { "LeftToRight", "RightToLeft" });
    verifyEnumValues(&ImageViewportEnums::staticMetaObject, "FitMode",
        { "Contain", "FitWidth", "FitHeight", "Manual" });
    verifyEnumValues(&ImageViewportEnums::staticMetaObject, "ContentAnchor", { "Start", "End" });

    const QList<QByteArray> methods = {
        "setPresentationTarget(ImageViewportPresentationTarget,PresentationTargetTransitionPolicy)",
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

    const QMetaObject& roleMetadataMetaObject = ImageViewportRoleMetadataSnapshot::staticMetaObject;
    const QList<QByteArray> rangeProperties = {
        "frameSeekBounds",
        "positionSeekBounds",
    };
    for (const QByteArray& propertyName : rangeProperties) {
        const int index = roleMetadataMetaObject.indexOfProperty(propertyName.constData());
        QVERIFY2(index >= 0, propertyName.constData());
        QCOMPARE(QByteArray(roleMetadataMetaObject.property(index).typeName()),
            QByteArray("ImageViewportRange"));
    }

    const QMetaObject& revisionsMetaObject = ImageViewportRevisionsSnapshot::staticMetaObject;
    const QList<QByteArray> revisionProperties = {
        "display",
        "request",
        "command",
    };
    for (const QByteArray& propertyName : revisionProperties) {
        const int index = revisionsMetaObject.indexOfProperty(propertyName.constData());
        QVERIFY2(index >= 0, propertyName.constData());
        QCOMPARE(QByteArray(revisionsMetaObject.property(index).typeName()),
            QByteArray("ImageViewportRevisionToken"));
    }

    const QMetaObject& roleGeometryMetaObject = ImageViewportRoleGeometrySnapshot::staticMetaObject;
    const QList<QByteArray> roleGeometryProperties = {
        "acceptedPageRect",
        "acceptedItemRect",
        "acceptedVisiblePageRect",
        "displayedPageRect",
        "displayedItemRect",
        "displayedVisiblePageRect",
    };
    for (const QByteArray& propertyName : roleGeometryProperties) {
        const int index = roleGeometryMetaObject.indexOfProperty(propertyName.constData());
        QVERIFY2(index >= 0, propertyName.constData());
        QCOMPARE(
            QByteArray(roleGeometryMetaObject.property(index).typeName()), QByteArray("QRectF"));
    }

    const QMetaObject& policyMetaObject = PresentationTargetTransitionPolicy::staticMetaObject;
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
        "zoomStepDeltaSet",
        "zoomStepDelta",
        "zoomAnchorSet",
        "zoomAnchor",
        "checkerboardLightColorSet",
        "checkerboardLightColor",
        "checkerboardDarkColorSet",
        "checkerboardDarkColor",
        "checkerboardCellSizeSet",
        "checkerboardCellSize",
        "contentAnchorSet",
        "contentAnchor",
        "qualityPreferenceSet",
        "qualityPreference",
        "exactnessPreferenceSet",
        "exactnessPreference",
    };
    for (const QByteArray& propertyName : presentationCommandProperties) {
        QVERIFY2(presentationCommandMetaObject.indexOfProperty(propertyName.constData()) >= 0,
            propertyName.constData());
    }
    const int zoomStepDeltaIndex = presentationCommandMetaObject.indexOfProperty("zoomStepDelta");
    QCOMPARE(presentationCommandMetaObject.property(zoomStepDeltaIndex).metaType(),
        QMetaType::fromType<double>());

    const QMetaObject& displayLimitsMetaObject = ImageViewportDisplayLimits::staticMetaObject;
    QCOMPARE(displayLimitsMetaObject.indexOfProperty("maximumManualZoomPercent"), -1);

    const QMetaObject& requestSnapshotMetaObject = ImageViewportRequestSnapshot::staticMetaObject;
    const QList<QByteArray> requestSnapshotProperties = {
        "status",
        "reason",
        "playbackPhase",
        "acceptedPresentationTargetGeneration",
        "acceptedRoleSet",
        "playbackRole",
    };
    for (const QByteArray& propertyName : requestSnapshotProperties) {
        QVERIFY2(requestSnapshotMetaObject.indexOfProperty(propertyName.constData()) >= 0,
            propertyName.constData());
    }
    QCOMPARE(requestSnapshotMetaObject.indexOfProperty("targetRoleSet"), -1);
    QCOMPARE(requestSnapshotMetaObject.indexOfProperty("activeRole"), -1);

    const QMetaObject& coordinateInputMetaObject = ImageViewportCoordinateInput::staticMetaObject;
    const QList<QByteArray> coordinateInputProperties = {
        "sourceSpace",
        "targetSpace",
        "role",
        "point",
    };
    for (const QByteArray& propertyName : coordinateInputProperties) {
        QVERIFY2(coordinateInputMetaObject.indexOfProperty(propertyName.constData()) >= 0,
            propertyName.constData());
    }
    QCOMPARE(coordinateInputMetaObject.indexOfProperty("pageRole"), -1);

    const QMetaObject& coordinateResultMetaObject = ImageViewportCoordinateResult::staticMetaObject;
    const QList<QByteArray> coordinateResultProperties = {
        "valid",
        "space",
        "role",
        "point",
    };
    for (const QByteArray& propertyName : coordinateResultProperties) {
        QVERIFY2(coordinateResultMetaObject.indexOfProperty(propertyName.constData()) >= 0,
            propertyName.constData());
    }
    QCOMPARE(coordinateResultMetaObject.indexOfProperty("sourceSpace"), -1);
    QCOMPARE(coordinateResultMetaObject.indexOfProperty("targetSpace"), -1);
    QCOMPARE(coordinateResultMetaObject.indexOfProperty("pageRole"), -1);
}

void ImageViewportPublicApiTest::hasDocumentedDefaultState()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), -1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(item.state().primary().display().sourceLogicalSize(), QSizeF());
    QCOMPARE(item.state().display().contentRect(), QRectF());
    QCOMPARE(item.state().primary().geometry().displayedVisiblePageRect(), QRectF());
    verifyInvalidCoordinateResult(mapItemToPrimaryPage(item, 1.0, 1.0));
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 1.0, 1.0));
    verifyInvalidCoordinateResult(mapItemToSpread(item, 1.0, 1.0));
    verifyInvalidCoordinateResult(mapSpreadToPage(item, ImageViewportPageRole::Primary, 1.0, 1.0));
    QCOMPARE(containsPrimaryPagePoint(item, 1.0, 1.0), false);
    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QVERIFY(revisionTokenProperty(item, "requestRevision").isValid());
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(viewportErrorString(item), QString());
    QCOMPARE(viewportWarningString(item), QString());
    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    QCOMPARE(presentation.smoothing(), true);
    QCOMPARE(presentation.mipmap(), false);
    QCOMPARE(presentation.mirrorHorizontally(), false);
    QCOMPARE(presentation.mirrorVertically(), false);
    QCOMPARE(presentation.backgroundMode(), ImageViewportBackgroundMode::Transparent);
    QCOMPARE(presentation.backgroundColor(), QColor(QStringLiteral("#ffffff")));
    QCOMPARE(presentation.checkerboardLightColor(), QColor(QStringLiteral("#ffffff")));
    QCOMPARE(presentation.checkerboardDarkColor(), QColor(QStringLiteral("#dcdcdc")));
    QCOMPARE(presentation.checkerboardCellSize(), 8.0);
    QCOMPARE(presentation.looping(), false);
    QCOMPARE(presentation.preferredManualZoomPercent(), 100.0);
    QCOMPARE(presentation.minimumManualZoomPercent(),
        ImageViewportDisplayLimits::minimumManualZoomPercent());
    QCOMPARE(presentation.maximumManualZoomPercent(), 0.0);
    QCOMPARE(presentation.manualZoomStepFactor(), 1.0905077326652577);
}

void ImageViewportPublicApiTest::manualZoomLimitPropertiesExposeDefaultsAndDoNotAdvanceRevisions()
{
    ImageViewport item;
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    const double minimum = presentation.minimumManualZoomPercent();
    const double maximum = presentation.maximumManualZoomPercent();
    const double stepFactor = presentation.manualZoomStepFactor();

    QCOMPARE(minimum, 10.0);
    QCOMPARE(maximum, 0.0);
    QCOMPARE(stepFactor, 1.0905077326652577);
    QCOMPARE(ImageViewportDisplayLimits::manualZoomStepFactor(), 1.0905077326652577);
    QCOMPARE(ImageViewportDisplayLimits::maximumPageGap(), 8192.0);
    QCOMPARE(ImageViewportDisplayLimits::minimumCheckerboardCellSize(), 1.0);
    QCOMPARE(ImageViewportDisplayLimits::maximumCheckerboardCellSize(), 256.0);

    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportCommandRevision(item), commandRevision);
    QCOMPARE(stateSpy.count(), 0);
}

void ImageViewportPublicApiTest::coordinateHelpersExposeInvalidDefaultsAndDoNotAdvanceRevisions()
{
    ImageViewport item;
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    const double infinity = std::numeric_limits<double>::infinity();
    verifyInvalidCoordinateResult(mapItemToSpread(item, 1.0, 1.0));
    ImageViewportCoordinateInput nonFinite
        = coordinateInput(ImageViewportCoordinateSpace::DisplayedSpread,
            ImageViewportCoordinateSpace::DisplayedSpread, QPointF(infinity, 1.0));
    verifyInvalidCoordinateResult(item.mapPoint(nonFinite));
    QCOMPARE(item.containsPoint(nonFinite), false);

    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportCommandRevision(item), commandRevision);
    QCOMPARE(stateSpy.count(), 0);
}

void ImageViewportPublicApiTest::typedPublicValueDefaultsExposeDocumentedFields()
{
    ImageViewport item;

    const ImageViewportRange frameBounds = primaryFrameSeekBounds(item);
    QCOMPARE(frameBounds.minimum(), -1);
    QCOMPARE(frameBounds.maximum(), -1);

    const ImageViewportRange positionBounds = primaryPositionSeekBounds(item);
    QCOMPARE(positionBounds.minimum(), -1);
    QCOMPARE(positionBounds.maximum(), -1);

    const ImageViewportCoordinateResult itemToPrimaryPage = mapItemToPrimaryPage(item, 1.0, 1.0);
    QCOMPARE(itemToPrimaryPage.isValid(), false);
    QCOMPARE(itemToPrimaryPage.point(), QPointF());

    const ImageViewportCoordinateResult primaryPageToItem = mapPrimaryPageToItem(item, 1.0, 1.0);
    QCOMPARE(primaryPageToItem.isValid(), false);
    QCOMPARE(primaryPageToItem.point(), QPointF());

    const ImageViewportRoleGeometrySnapshot primaryGeometry = item.state().primary().geometry();
    QCOMPARE(primaryGeometry.acceptedPageRect(), QRectF());
    QCOMPARE(primaryGeometry.acceptedItemRect(), QRectF());
    QCOMPARE(primaryGeometry.acceptedVisiblePageRect(), QRectF());
    QCOMPARE(primaryGeometry.displayedPageRect(), QRectF());
    QCOMPARE(primaryGeometry.displayedItemRect(), QRectF());
    QCOMPARE(primaryGeometry.displayedVisiblePageRect(), QRectF());

    const ImageViewportRoleGeometrySnapshot secondaryGeometry = item.state().secondary().geometry();
    QCOMPARE(secondaryGeometry.acceptedPageRect(), QRectF());
    QCOMPARE(secondaryGeometry.acceptedItemRect(), QRectF());
    QCOMPARE(secondaryGeometry.acceptedVisiblePageRect(), QRectF());
    QCOMPARE(secondaryGeometry.displayedPageRect(), QRectF());
    QCOMPARE(secondaryGeometry.displayedItemRect(), QRectF());
    QCOMPARE(secondaryGeometry.displayedVisiblePageRect(), QRectF());
}

void ImageViewportPublicApiTest::roleGeometrySnapshotFields()
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);

    const ImageViewportRoleGeometrySnapshot primaryGeometry = item.state().primary().geometry();
    QCOMPARE(primaryGeometry.acceptedPageRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(primaryGeometry.acceptedItemRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(primaryGeometry.acceptedVisiblePageRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(primaryGeometry.displayedPageRect(), primaryGeometry.acceptedPageRect());
    QCOMPARE(primaryGeometry.displayedItemRect(), primaryGeometry.acceptedItemRect());
    QCOMPARE(primaryGeometry.displayedVisiblePageRect(), primaryGeometry.acceptedVisiblePageRect());

    const ImageViewportRoleGeometrySnapshot secondaryGeometry = item.state().secondary().geometry();
    QCOMPARE(secondaryGeometry.acceptedPageRect(), QRectF(16.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryGeometry.acceptedItemRect(), QRectF(16.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryGeometry.acceptedVisiblePageRect(), QRectF(0.0, 0.0, 10.0, 20.0));
    QCOMPARE(secondaryGeometry.displayedPageRect(), secondaryGeometry.acceptedPageRect());
    QCOMPARE(secondaryGeometry.displayedItemRect(), secondaryGeometry.acceptedItemRect());
    QCOMPARE(
        secondaryGeometry.displayedVisiblePageRect(), secondaryGeometry.acceptedVisiblePageRect());

    ImageViewport primaryOnly;
    primaryOnly.setSize(QSizeF(16.0, 8.0));
    primaryOnly.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(primaryOnly);

    const ImageViewportRoleGeometrySnapshot unavailableSecondary
        = primaryOnly.state().secondary().geometry();
    QCOMPARE(unavailableSecondary.acceptedPageRect(), QRectF());
    QCOMPARE(unavailableSecondary.acceptedItemRect(), QRectF());
    QCOMPARE(unavailableSecondary.acceptedVisiblePageRect(), QRectF());
    QCOMPARE(unavailableSecondary.displayedPageRect(), QRectF());
    QCOMPARE(unavailableSecondary.displayedItemRect(), QRectF());
    QCOMPARE(unavailableSecondary.displayedVisiblePageRect(), QRectF());
}

void ImageViewportPublicApiTest::revisionTokensExposeValidityAndEquality()
{
    const QMetaObject& revisionTokenMetaObject = ImageViewportRevisionToken::staticMetaObject;
    QVERIFY(revisionTokenMetaObject.indexOfProperty("valid") >= 0);
    QVERIFY(revisionTokenMetaObject.indexOfProperty("value") < 0);
    QVERIFY(revisionTokenMetaObject.indexOfMethod("equals(ImageViewportRevisionToken)") >= 0);

    const QMetaObject& presentationGenerationMetaObject
        = ImageViewportPresentationTargetGenerationToken::staticMetaObject;
    QVERIFY(presentationGenerationMetaObject.indexOfProperty("valid") >= 0);
    QVERIFY(presentationGenerationMetaObject.indexOfProperty("value") < 0);
    QVERIFY(presentationGenerationMetaObject.indexOfMethod(
                "equals(ImageViewportPresentationTargetGenerationToken)")
        >= 0);

    const QMetaObject& demandRevisionMetaObject
        = ImageViewportDemandRevisionToken::staticMetaObject;
    QVERIFY(demandRevisionMetaObject.indexOfProperty("valid") >= 0);
    QVERIFY(demandRevisionMetaObject.indexOfProperty("value") < 0);
    QVERIFY(
        demandRevisionMetaObject.indexOfMethod("equals(ImageViewportDemandRevisionToken)") >= 0);

    const QMetaObject& allocationGenerationMetaObject
        = ImageViewportAllocationGenerationToken::staticMetaObject;
    QVERIFY(allocationGenerationMetaObject.indexOfProperty("valid") >= 0);
    QVERIFY(allocationGenerationMetaObject.indexOfProperty("value") < 0);
    QVERIFY(allocationGenerationMetaObject.indexOfMethod(
                "equals(ImageViewportAllocationGenerationToken)")
        >= 0);

    const ImageViewportRevisionToken invalidRevision;
    const ImageViewportPresentationTargetGenerationToken invalidPresentationGeneration;
    const ImageViewportDemandRevisionToken invalidDemandRevision;
    const ImageViewportAllocationGenerationToken invalidAllocationGeneration;
    QVERIFY(invalidRevision.equals(ImageViewportRevisionToken()));
    QVERIFY(invalidPresentationGeneration.equals(ImageViewportPresentationTargetGenerationToken()));
    QVERIFY(invalidDemandRevision.equals(ImageViewportDemandRevisionToken()));
    QVERIFY(invalidAllocationGeneration.equals(ImageViewportAllocationGenerationToken()));

    ImageViewport item;

    const ImageViewportRevisionToken initialDisplayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken initialRequestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken initialCommandRevision = viewportCommandRevision(item);
    QVERIFY(initialDisplayRevision.isValid());
    QVERIFY(initialRequestRevision.isValid());
    QVERIFY(initialCommandRevision.isValid());

    ImageViewportPresentationCommand command;
    command.setBackgroundMode(ImageViewportBackgroundMode::SolidColor);
    command.setBackgroundColor(Qt::black);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);

    const ImageViewportRevisionToken changedDisplayRevision = viewportDisplayRevision(item);
    QVERIFY(changedDisplayRevision.isValid());
    QVERIFY(changedDisplayRevision != initialDisplayRevision);
    QVERIFY(changedDisplayRevision.equals(changedDisplayRevision));
    QVERIFY(!changedDisplayRevision.equals(initialDisplayRevision));
    QCOMPARE(viewportDisplayRevision(item), changedDisplayRevision);
    QCOMPARE(viewportRequestRevision(item), initialRequestRevision);
    QVERIFY(viewportCommandRevision(item) != initialCommandRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportPublicApiTest::typedPresentationTargetTransitionPolicyPreservesStateWhenInvalid()
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
    item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
        PresentationTargetTransitionPolicy {});
    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(
        PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);

    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(replacementResult->sequence()), invalidPolicy);

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), firstResult->sequence());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item).isValid());
    QVERIFY(viewportCommandRevision(item) != commandRevision);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(
        commandReasonValue(item), enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
}

void ImageViewportPublicApiTest::targetlessGeometryChangeIsRevisionNeutral()
{
    ImageViewport item;
    const ImageViewportStateSnapshot initialState = item.state();
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    item.setSize(QSizeF(100.0, 50.0));

    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(item.state(), initialState);
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(displayStatusValue(item), enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleImageRect(item), QRectF());

    item.setSize(QSizeF(100.0, 50.0));

    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(item.state(), initialState);
    QCOMPARE(stateSpy.count(), 0);

    const double changedWidth = 100.0 + 5.0e-13;
    QVERIFY(changedWidth != 100.0);
    item.setSize(QSizeF(changedWidth, 50.0));

    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(item.state(), initialState);
    QCOMPARE(stateSpy.count(), 0);
}
QTEST_MAIN(ImageViewportPublicApiTest)

#include "tst_imageviewport_public_api.moc"
