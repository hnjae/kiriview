#include "framepreparation_p.h"
#include "imagesequencesource_p.h"
#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"
#include "imageviewporttoken_p.h"
#include "timingintervals_p.h"

#include <cmath>

class ImageSequenceFactoryTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageSequenceFactoryTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void exposesTypedSequenceFactorySurface();
    void factoryRejectsNullTypedInputs();
    void timedFrameListNativeFactoryRejectsMismatchedCounts();
    void timedFrameListNativeFactoryClassifiesSemanticFailuresBeforeLimits();
    void timedFrameListNativeFactoryRejectsEmptyInput();
    void qmlTimedFrameListExposesBuilderState();
    void qmlFactoryCreatesSequencesFromSuppliedTypedHelpers();
    void factoryResultDiagnosticsArePublicSafe();
    void exposesImageSequenceLimits();
    void exposesImageViewportDisplayLimits();
    void factoryResultSequenceSurvivesFactoryDestruction();
    void factorySequenceSourceCarriesOwnerAndConstructionFacts();
    void assignedFactorySequenceSurvivesResultDestruction();
    void assignedProviderSequenceSurvivesResultDestruction();
    void sharedFactorySequenceSurvivesFirstViewportDestruction();
    void clearReleasesAssignedFactorySequenceOwner();
    void imageFrameRetainsImmutablePayload();
    void imageFrameExposesPayloadMetadata();
    void imageFrameExplicitEnvelopeSeparatesLogicalAndPayloadSize();
    void imageFrameOrientationPoliciesNormalizePayload();
    void imageFrameUsesDeviceIndependentLogicalSize();
    void providerMetadataAdmissionAcceptsTimedMetadata();
    void providerMetadataAdmissionRejectsInvalidTiming();
    void providerKnownFactsAdmissionAcceptsTimedFacts();
    void providerKnownFactsAdmissionRejectsDurationLimits();
    void providerConstructionRejectsStillFactsWithFrameSeekDeclaredFalse();
    void providerFrameAdmissionUsesResolvedFrameIdentity();
    void providerFrameAdmissionRejectsStaleDemandAndRequiredInexactPayload();
    void timingIntervalsResolveHalfOpenBoundaries();
    void timingIntervalsRejectInvalidDurations();
    void stillImageSequenceRetainsFactoryPayload();
    void timedFrameListSequenceRetainsFactoryPayloads();
    void timedFrameListSequencePreservesExplicitPayloadFacts();
    void commandsWithoutRequestAreIgnoredDiagnostics();
};

void ImageSequenceFactoryTest::exposesTypedSequenceFactorySurface()
{
    ImageSequenceFactory factory;
    const QMetaObject* metaObject = factory.metaObject();

    QVERIFY(
        metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromFrame(ImageFrame*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(
                QMetaObject::normalizedSignature("fromTimedFrameList(TimedImageFrameList*)"))
        >= 0);
    QVERIFY(metaObject->indexOfMethod(
                QMetaObject::normalizedSignature("fromProvider(ImageSequenceProviderAdapter*)"))
        >= 0);

    QScopedPointer<QObject> result(factory.fromFrame(nullptr));
    QVERIFY(result);
    const QMetaObject* resultMetaObject = result->metaObject();
    QVERIFY(resultMetaObject->indexOfProperty("sequence") >= 0);
    QVERIFY(resultMetaObject->indexOfProperty("outcome") >= 0);
    QVERIFY(resultMetaObject->indexOfProperty("reason") >= 0);
    QVERIFY(resultMetaObject->indexOfProperty("errorString") >= 0);
    QVERIFY(resultMetaObject->indexOfProperty("warningString") < 0);
    verifyEnumValues(
        &ImageSequenceFactoryEnums::staticMetaObject, "FactoryOutcome", { "Created", "Rejected" });
    verifyEnumValues(&ImageSequenceFactoryEnums::staticMetaObject, "FactoryReason",
        { "NoError", "InvalidFrame", "InvalidTiming", "InvalidAnimationMetadata",
            "InvalidProviderDescriptor", "LimitExceeded" });
    QCOMPARE(result->property("sequence").value<QObject*>(), nullptr);
    QCOMPARE(result->property("outcome").toInt(),
        enumValue(&ImageSequenceFactoryEnums::staticMetaObject, "FactoryOutcome", "Rejected"));
    QVERIFY(!result->property("errorString").toString().isEmpty());
}

void ImageSequenceFactoryTest::factoryRejectsNullTypedInputs()
{
    ImageSequenceFactory factory;

    QScopedPointer<ImageSequenceFactoryResult> frameResult(factory.fromFrame(nullptr));
    QVERIFY(frameResult);
    QCOMPARE(frameResult->sequence(), nullptr);
    QCOMPARE(frameResult->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(!frameResult->errorString().isEmpty());

    QScopedPointer<ImageSequenceFactoryResult> listResult(factory.fromTimedFrameList(nullptr));
    QVERIFY(listResult);
    QCOMPARE(listResult->sequence(), nullptr);
    QCOMPARE(listResult->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(!listResult->errorString().isEmpty());

    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(nullptr));
    QVERIFY(providerResult);
    QCOMPARE(providerResult->sequence(), nullptr);
    QCOMPARE(providerResult->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(!providerResult->errorString().isEmpty());
}

void ImageSequenceFactoryTest::timedFrameListNativeFactoryRejectsMismatchedCounts()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QScopedPointer<ImageSequenceFactoryResult> missingDurationResult(
        factory.fromTimedFrameList({ image }, {}));
    QVERIFY(missingDurationResult);
    QCOMPARE(missingDurationResult->sequence(), nullptr);
    QCOMPARE(missingDurationResult->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(missingDurationResult->errorString().contains(QStringLiteral("same count")));

    QScopedPointer<ImageSequenceFactoryResult> missingImageResult(
        factory.fromTimedFrameList({}, { 100 }));
    QVERIFY(missingImageResult);
    QCOMPARE(missingImageResult->sequence(), nullptr);
    QCOMPARE(missingImageResult->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(missingImageResult->errorString().contains(QStringLiteral("same count")));
}

void ImageSequenceFactoryTest::timedFrameListNativeFactoryClassifiesSemanticFailuresBeforeLimits()
{
    ImageSequenceFactory factory;
    QImage validImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    validImage.fill(Qt::transparent);

    QScopedPointer<ImageSequenceFactoryResult> invalidFrameResult(
        factory.fromTimedFrameList({ QImage() }, { 0 }));
    QCOMPARE(invalidFrameResult->reason(), ImageSequenceFactoryReason::InvalidFrame);

    QScopedPointer<ImageSequenceFactoryResult> invalidTimingResult(
        factory.fromTimedFrameList({ validImage }, { 0 }));
    QCOMPARE(invalidTimingResult->reason(), ImageSequenceFactoryReason::InvalidTiming);

    QImage oversizedImage(ImageSequenceLimits::maximumSourceLogicalWidth() + 1, 1,
        QImage::Format_ARGB32_Premultiplied);
    oversizedImage.fill(Qt::transparent);
    QScopedPointer<ImageSequenceFactoryResult> limitResult(
        factory.fromTimedFrameList({ oversizedImage }, { 1 }));
    QCOMPARE(limitResult->reason(), ImageSequenceFactoryReason::LimitExceeded);
}

void ImageSequenceFactoryTest::timedFrameListNativeFactoryRejectsEmptyInput()
{
    ImageSequenceFactory factory;

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList({}, {}));

    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(result->errorString().contains(QStringLiteral("at least one frame")));
}

void ImageSequenceFactoryTest::qmlTimedFrameListExposesBuilderState()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

Item {
    TimedImageFrameList {
        id: list
    }

    property bool initialCountIsZero: list.count === 0
    property bool appendNullRejected: false
    property bool appendPreservedCount: false
    property bool appendSetDiagnostic: false
    property bool factoryRejectsEmptyList: false
    property bool clearResetsDiagnostic: false

    Component.onCompleted: {
        appendNullRejected = list.appendFrame(null, 100) === false
        appendPreservedCount = list.count === 0
        appendSetDiagnostic = list.errorString.indexOf("ImageFrame") >= 0
        factoryRejectsEmptyList = ImageSequenceFactory.fromTimedFrameList(list).sequence === null
        list.clear()
        clearResetsDiagnostic = list.count === 0 && list.errorString === ""
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    QCOMPARE(object->property("initialCountIsZero").toBool(), true);
    QCOMPARE(object->property("appendNullRejected").toBool(), true);
    QCOMPARE(object->property("appendPreservedCount").toBool(), true);
    QCOMPARE(object->property("appendSetDiagnostic").toBool(), true);
    QCOMPARE(object->property("factoryRejectsEmptyList").toBool(), true);
    QCOMPARE(object->property("clearResetsDiagnostic").toBool(), true);
}

void ImageSequenceFactoryTest::qmlFactoryCreatesSequencesFromSuppliedTypedHelpers()
{
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

Item {
    property ImageFrame suppliedFrame
    property imageViewportPresentationTarget presentationTarget
    property presentationTargetTransitionPolicy policy
    property bool frameFactoryCreated: false
    property bool timedListAcceptedFrame: false
    property bool timedFactoryCreated: false
    property bool viewportReady: viewport.state.request.status === ImageViewport.RequestStatus.Ready
        && viewport.state.request.reason === ImageViewport.RequestReason.Ready
        && viewport.state.display.status === ImageViewport.DisplayStatus.Ready
        && viewport.state.primary.metadata.frameCount === 1
        && viewport.state.primary.metadata.totalDuration === 100
        && viewport.state.primary.display.sourceLogicalSize.width === 4
        && viewport.state.primary.display.sourceLogicalSize.height === 2

    ImageViewport {
        id: viewport
        objectName: "viewport"
        width: 40
        height: 20
    }

    TimedImageFrameList {
        id: list
    }

    Component.onCompleted: {
        const frameResult = ImageSequenceFactory.fromFrame(suppliedFrame)
        frameFactoryCreated = frameResult.sequence !== null
            && frameResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Created
            && frameResult.errorString === ""

        timedListAcceptedFrame = list.appendFrame(suppliedFrame, 100) === true
            && list.count === 1
            && list.errorString === ""

        const timedResult = ImageSequenceFactory.fromTimedFrameList(list)
        timedFactoryCreated = timedResult.sequence !== null
            && timedResult.outcome === ImageSequenceFactoryResult.FactoryOutcome.Created
            && timedResult.errorString === ""

        presentationTarget.primary = timedResult.sequence
        viewport.setPresentationTarget(presentationTarget, policy)
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QVariantMap initialProperties;
    initialProperties.insert(
        QStringLiteral("suppliedFrame"), QVariant::fromValue<QObject*>(&frame));
    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(object, qPrintable(componentErrors(component)));
    auto* viewport = object->findChild<ImageViewport*>(QStringLiteral("viewport"));
    QVERIFY(viewport);
    acknowledgePendingPrimaryRenderCommitForTest(*viewport);
    QCOMPARE(object->property("frameFactoryCreated").toBool(), true);
    QCOMPARE(object->property("timedListAcceptedFrame").toBool(), true);
    QCOMPARE(object->property("timedFactoryCreated").toBool(), true);
    QCOMPARE(object->property("viewportReady").toBool(), true);
}

void ImageSequenceFactoryTest::factoryResultDiagnosticsArePublicSafe()
{
    const int limit = ImageSequenceLimits::maximumDiagnosticCharacters();
    QString diagnostic = QStringLiteral("failed for https://user:secret@example.test/image.png "
                                        "token=abc123 path /home/ops/private/image.png ");
    diagnostic += QString(limit + 100, QLatin1Char('x'));

    ImageSequenceFactoryResult result(nullptr, ImageSequenceFactoryOutcome::Rejected,
        ImageSequenceFactoryReason::InvalidFrame, diagnostic);

    const QString errorString = result.errorString();
    QCOMPARE(errorString.toUcs4().size(), limit);
    QVERIFY(!errorString.isEmpty());
    QVERIFY(!errorString.contains(QStringLiteral("https://")));
    QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
    QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
    QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
}

void ImageSequenceFactoryTest::exposesImageSequenceLimits()
{
    ImageSequenceLimits limits;
    const QMetaObject* metaObject = limits.metaObject();

    struct LimitExpectation
    {
        const char* name;
        qint64 cppValue;
        qint64 minimum;
    };

    const LimitExpectation expectations[] = {
        { "maximumSourceLogicalWidth", ImageSequenceLimits::maximumSourceLogicalWidth(), 8192 },
        { "maximumSourceLogicalHeight", ImageSequenceLimits::maximumSourceLogicalHeight(), 8192 },
        { "maximumSourceLogicalPixels", ImageSequenceLimits::maximumSourceLogicalPixels(),
            67108864LL },
        { "maximumPayloadRasterWidth", ImageSequenceLimits::maximumPayloadRasterWidth(), 8192 },
        { "maximumPayloadRasterHeight", ImageSequenceLimits::maximumPayloadRasterHeight(), 8192 },
        { "maximumPayloadBytes", ImageSequenceLimits::maximumPayloadBytes(), 268435456LL },
        { "maximumFrameCount", ImageSequenceLimits::maximumFrameCount(), 10000 },
        { "maximumFrameDurationMilliseconds",
            ImageSequenceLimits::maximumFrameDurationMilliseconds(), 86400000 },
        { "maximumTotalDurationMilliseconds",
            ImageSequenceLimits::maximumTotalDurationMilliseconds(), 86400000 },
        { "maximumDiagnosticCharacters", ImageSequenceLimits::maximumDiagnosticCharacters(), 4096 },
        { "maximumFormatIdentifierCharacters",
            ImageSequenceLimits::maximumFormatIdentifierCharacters(), 1 },
    };

    for (const LimitExpectation& expectation : expectations) {
        const int propertyIndex = metaObject->indexOfProperty(expectation.name);
        QVERIFY2(propertyIndex >= 0, expectation.name);
        const QMetaProperty property = metaObject->property(propertyIndex);
        QCOMPARE(property.isWritable(), false);
        QCOMPARE(property.isConstant(), true);
        QCOMPARE(limits.property(expectation.name).toLongLong(), expectation.cppValue);
        QVERIFY2(expectation.cppValue >= expectation.minimum, expectation.name);
    }
}

void ImageSequenceFactoryTest::exposesImageViewportDisplayLimits()
{
    ImageViewportDisplayLimits limits;
    const QMetaObject* metaObject = limits.metaObject();
    struct LimitExpectation
    {
        const char* name;
        double cppValue;
        double expected;
    };
    const QList<LimitExpectation> expectations = {
        { "minimumManualZoomPercent", ImageViewportDisplayLimits::minimumManualZoomPercent(), 1.0 },
        { "maximumManualZoomPercent", ImageViewportDisplayLimits::maximumManualZoomPercent(),
            10000.0 },
        { "manualZoomStepFactor", ImageViewportDisplayLimits::manualZoomStepFactor(), 1.25 },
        { "maximumPageGap", ImageViewportDisplayLimits::maximumPageGap(), 8192.0 },
        { "minimumCheckerboardCellSize", ImageViewportDisplayLimits::minimumCheckerboardCellSize(),
            1.0 },
        { "maximumCheckerboardCellSize", ImageViewportDisplayLimits::maximumCheckerboardCellSize(),
            256.0 },
    };

    for (const LimitExpectation& expectation : expectations) {
        const int propertyIndex = metaObject->indexOfProperty(expectation.name);
        QVERIFY2(propertyIndex >= 0, expectation.name);
        const QMetaProperty property = metaObject->property(propertyIndex);
        QCOMPARE(property.isWritable(), false);
        QCOMPARE(property.isConstant(), true);
        QCOMPARE(limits.property(expectation.name).toDouble(), expectation.cppValue);
        QCOMPARE(expectation.cppValue, expectation.expected);
        QVERIFY2(std::isfinite(expectation.cppValue), expectation.name);
    }
}

void ImageSequenceFactoryTest::providerMetadataAdmissionAcceptsTimedMetadata()
{
    const auto admission = FramePreparation::admitProviderMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 }));

    QVERIFY(admission.accepted());
    QCOMPARE(admission.cause, FramePreparation::ProviderMetadataAdmissionResult::Cause::Accepted);
    QCOMPARE(admission.status, ImageViewportRequestStatus::Ready);
    QCOMPARE(admission.reason, ImageViewportRequestReason::Ready);
    QCOMPARE(admission.logicalSize, QSizeF(16.0, 8.0));
    QCOMPARE(admission.timedMetadata, true);
    QVERIFY(admission.timingIntervals.isValid());
    QCOMPARE(admission.timingIntervals.frameCount(), 2);
    QCOMPARE(admission.timingIntervals.totalDuration(), 350);
    QCOMPARE(admission.timingIntervals.frameStartPosition(1), 100);
    QCOMPARE(admission.diagnostic, QString());
}

void ImageSequenceFactoryTest::providerMetadataAdmissionRejectsInvalidTiming()
{
    const auto admission = FramePreparation::admitProviderMetadata(
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 0 }));

    QVERIFY(!admission.accepted());
    QCOMPARE(admission.cause,
        FramePreparation::ProviderMetadataAdmissionResult::Cause::InvalidFrameDuration);
    QCOMPARE(admission.status, ImageViewportRequestStatus::Error);
    QCOMPARE(admission.reason, ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(admission.timedMetadata, false);
    QVERIFY(!admission.timingIntervals.isValid());
    QVERIFY(admission.diagnostic.contains(QStringLiteral("duration")));
}

void ImageSequenceFactoryTest::providerKnownFactsAdmissionAcceptsTimedFacts()
{
    const auto admission = FramePreparation::admitProviderKnownFacts(
        ImageViewportInternal::ImageSequenceProviderKnownFacts::timedFrameList(
            QSizeF(16.0, 8.0), { 100, 250 }));

    QVERIFY(admission.accepted());
    QCOMPARE(admission.cause, FramePreparation::ProviderKnownFactsAdmissionResult::Cause::Accepted);
    QCOMPARE(admission.outcome, ImageSequenceFactoryOutcome::Created);
    QCOMPARE(admission.logicalSize, QSizeF(16.0, 8.0));
    QCOMPARE(admission.frameCount, 2);
    QVERIFY(admission.timingIntervals.isValid());
    QCOMPARE(admission.timingIntervals.totalDuration(), 350);
    QCOMPARE(admission.diagnostic, QString());
}

void ImageSequenceFactoryTest::providerKnownFactsAdmissionRejectsDurationLimits()
{
    const auto admission = FramePreparation::admitProviderKnownFacts(
        ImageViewportInternal::ImageSequenceProviderKnownFacts::fixedDurationFrames(
            QSizeF(16.0, 8.0), 1, ImageSequenceLimits::maximumFrameDurationMilliseconds() + 1));

    QVERIFY(!admission.accepted());
    QCOMPARE(admission.cause,
        FramePreparation::ProviderKnownFactsAdmissionResult::Cause::FrameDurationTooLarge);
    QCOMPARE(admission.outcome, ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(!admission.timingIntervals.isValid());
    QVERIFY(admission.diagnostic.contains(QStringLiteral("maximumFrameDurationMilliseconds")));
}

void ImageSequenceFactoryTest::providerConstructionRejectsStillFactsWithFrameSeekDeclaredFalse()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageViewportCapabilitySupport::Unavailable, ImageViewportCapabilitySupport::False,
        ImageViewportCapabilitySupport::Unavailable);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryOutcome::Rejected);
    QVERIFY(result->errorString().contains(QStringLiteral("construction facts")));
    QCOMPARE(*sessionCount, 0);
}

void ImageSequenceFactoryTest::providerFrameAdmissionUsesResolvedFrameIdentity()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);

    FramePreparation::ProviderFrameState state;
    state.metadataReady = true;
    state.timedMetadata = true;
    state.logicalSize = QSizeF(16.0, 8.0);
    state.timingIntervals = TimingIntervals::fromFrameDurations({ 100, 250 });
    state.resolvedFrame.frame = 1;
    state.resolvedFrame.position = 100;
    state.demandRevision = ImageViewportInternal::DemandRevisionTokenPrivateAccess::fromValue(1);
    state.preparedPayload.generation = 7;
    state.preparedPayload.payloadId = 13;

    ImageSequenceProviderFrameEnvelope envelope = ImageSequenceProviderFrameEnvelope::stillFrame();
    envelope.setFrame(1);
    envelope.setFrameStartPosition(100);
    envelope.setFrameDuration(250);
    envelope.setDemandRevision(state.demandRevision);
    const auto admission = FramePreparation::admitProviderFrame(&frame, envelope, state);

    QVERIFY(admission.accepted());
    QCOMPARE(admission.cause, FramePreparation::ProviderFrameAdmissionResult::Cause::Accepted);
    QCOMPARE(admission.preparedPayload.generation, 7);
    QCOMPARE(admission.preparedPayload.payloadId, 13);
    QCOMPARE(admission.preparedPayload.image.size(), image.size());
    QCOMPARE(admission.preparedPayload.image.format(), image.format());
    QCOMPARE(admission.preparedPayload.image.pixelColor(0, 0), image.pixelColor(0, 0));
    QCOMPARE(admission.preparedPayload.roleValid, true);
    QCOMPARE(admission.preparedPayload.role, ImageViewportPageRole::Primary);
    QCOMPARE(admission.preparedPayload.resolvedFrame.frame, 1);
    QCOMPARE(admission.preparedPayload.resolvedFrame.position, 100);
    QCOMPARE(admission.preparedPayload.frameDuration, 250);
    QCOMPARE(admission.preparedPayload.hasAlpha, true);
    QCOMPARE(admission.preparedPayload.orientationPolicy, ImageFrame::OrientationPolicy::Identity);
    QCOMPARE(admission.preparedPayload.formatIdentifier, QString());
}

void ImageSequenceFactoryTest::providerFrameAdmissionRejectsStaleDemandAndRequiredInexactPayload()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageSequenceProviderFrameEnvelope envelope = ImageSequenceProviderFrameEnvelope::stillFrame();
    envelope.setDemandRevision(
        ImageViewportInternal::DemandRevisionTokenPrivateAccess::fromValue(3));
    ImageFrame frame(image, QSizeF(16.0, 8.0), QSizeF(16.0, 8.0), QSizeF(1.0, 1.0),
        image.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QStringLiteral("argb32"));

    FramePreparation::ProviderFrameState state;
    state.metadataReady = true;
    state.logicalSize = QSizeF(16.0, 8.0);
    state.resolvedFrame = { 0, -1 };
    state.demandRevision = ImageViewportInternal::DemandRevisionTokenPrivateAccess::fromValue(5);

    const auto stale = FramePreparation::admitProviderFrame(&frame, envelope, state);
    QCOMPARE(
        stale.cause, FramePreparation::ProviderFrameAdmissionResult::Cause::DemandRevisionMismatch);

    state.demandRevision = envelope.demandRevision();
    state.exactnessPreference = ImageViewportExactnessPreference::RequireExact;
    const auto inexact = FramePreparation::admitProviderFrame(&frame, envelope, state);
    QCOMPARE(
        inexact.cause, FramePreparation::ProviderFrameAdmissionResult::Cause::ExactnessMismatch);
    QCOMPARE(inexact.status, ImageViewportRequestStatus::Unsupported);
}

void ImageSequenceFactoryTest::timedFrameListSequencePreservesExplicitPayloadFacts()
{
    QImage preview(8, 4, QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::transparent);
    ImageFrame previewFrame(preview, QSizeF(16.0, 8.0), QSizeF(8.0, 4.0), QSizeF(0.5, 0.5),
        preview.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QStringLiteral("preview/argb32"));
    QImage exact(16, 8, QImage::Format_ARGB32_Premultiplied);
    exact.fill(Qt::transparent);
    ImageFrame exactFrame(exact);

    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&previewFrame, 100));
    QVERIFY(list.appendFrame(&exactFrame, 250));
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    const ImageViewportInternal::ImageSequenceSource source
        = ImageViewportInternal::makeImageSequenceSource(result->sequence());
    const ImageViewportInternal::FramePayloadFacts previewFacts
        = ImageViewportInternal::sourceFramePayloadFacts(source, 0);
    QCOMPARE(previewFacts.sourceLogicalSize, QSizeF(16.0, 8.0));
    QCOMPARE(previewFacts.payloadRasterSize, QSizeF(8.0, 4.0));
    QCOMPARE(previewFacts.sourceToPayloadScale, QSizeF(0.5, 0.5));
    QCOMPARE(previewFacts.payloadByteSize, qint64(preview.sizeInBytes()));
    QCOMPARE(previewFacts.quality, ImageViewportPayloadQuality::Preview);
    QCOMPARE(previewFacts.exactness, ImageViewportPayloadExactness::NotExact);
    QCOMPARE(previewFacts.hasAlpha, true);
    QCOMPARE(previewFacts.orientationPolicy, ImageFrame::OrientationPolicy::Identity);
    QCOMPARE(previewFacts.formatIdentifier, QStringLiteral("preview/argb32"));

    const ImageViewportInternal::FramePayloadFacts exactFacts
        = ImageViewportInternal::sourceFramePayloadFacts(source, 1);
    QCOMPARE(exactFacts.quality, ImageViewportPayloadQuality::Exact);
    QCOMPARE(exactFacts.exactness, ImageViewportPayloadExactness::ExactForSource);

    ImageViewportInternal::PreparedPayload seed;
    seed.generation = 3;
    seed.payloadId = 7;
    const auto admission = FramePreparation::admitBuiltInFrame(source, 0, seed,
        ImageViewportExactnessPreference::Default, ImageViewportPageRole::Secondary);
    QVERIFY(admission.accepted());
    QCOMPARE(admission.preparedPayload.roleValid, true);
    QCOMPARE(admission.preparedPayload.role, ImageViewportPageRole::Secondary);
    QCOMPARE(admission.preparedPayload.resolvedFrame.frame, 0);
    QCOMPARE(admission.preparedPayload.resolvedFrame.position, 0);
    QCOMPARE(admission.preparedPayload.frameDuration, 100);
    QCOMPARE(admission.preparedPayload.hasAlpha, true);
    QCOMPARE(admission.preparedPayload.formatIdentifier, QStringLiteral("preview/argb32"));
}

void ImageSequenceFactoryTest::factoryResultSequenceSurvivesFactoryDestruction()
{
    ImageSequenceFactoryResult* rawResult = nullptr;
    QPointer<ImageSequenceFactoryResult> observedResult;
    QPointer<ImageSequence> observedSequence;
    {
        ImageSequenceFactory factory;
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        ImageFrame frame(image);
        rawResult = factory.fromFrame(&frame);
        observedResult = rawResult;
        QVERIFY(rawResult);
        observedSequence = rawResult->sequence();
        QVERIFY(observedSequence);
    }

    QVERIFY(observedResult);
    QVERIFY(observedSequence);
    QScopedPointer<ImageSequenceFactoryResult> result(rawResult);

    ImageViewport item;
    item.setSize(QSizeF(100.0, 50.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(viewportPrimarySequence(item), result->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
}

void ImageSequenceFactoryTest::factorySequenceSourceCarriesOwnerAndConstructionFacts()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame firstFrame(image);
    ImageFrame secondFrame(image);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> timedResult(factory.fromTimedFrameList(&list));
    QVERIFY(timedResult->sequence());

    const ImageViewportInternal::ImageSequenceSource timedSource
        = ImageViewportInternal::makeImageSequenceSource(timedResult->sequence());
    QCOMPARE(timedSource.sequence, timedResult->sequence());
    QVERIFY(timedSource.owner);
    QCOMPARE(timedSource.facts.present, true);
    QCOMPARE(timedSource.facts.provider, false);
    QCOMPARE(timedSource.facts.timed, true);
    QCOMPARE(timedSource.facts.frameCount, 2);
    QCOMPARE(timedSource.facts.totalDuration, 350);
    QCOMPARE(timedSource.facts.firstFramePosition, 0);
    QCOMPARE(timedSource.facts.timingIntervals.frameStartPosition(1), 100);

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    const ImageSequenceProviderMetadata knownFacts
        = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), { 100, 250 });
    CountingProviderAdapter adapter(sessionFactory, knownFacts,
        ImageViewportCapabilitySupport::True, ImageViewportCapabilitySupport::True,
        ImageViewportCapabilitySupport::True, ImageSequenceProviderThreadingContract::ThreadSafe);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    const ImageViewportInternal::ImageSequenceSource providerSource
        = ImageViewportInternal::makeImageSequenceSource(providerResult->sequence());
    QCOMPARE(providerSource.sequence, providerResult->sequence());
    QVERIFY(providerSource.owner);
    QVERIFY(providerSource.providerSessionFactory);
    QCOMPARE(providerSource.facts.present, true);
    QCOMPARE(providerSource.facts.provider, true);
    QCOMPARE(providerSource.facts.hasCompleteProviderKnownMetadata, true);
    QCOMPARE(providerSource.facts.providerKnownFacts.isTimedFrameList(), true);
    QCOMPARE(providerSource.facts.providerKnownFacts.frameDurations(), QVector<int>({ 100, 250 }));
    QCOMPARE(ImageViewportInternal::providerCapabilitySupport(
                 providerSource.facts.providerTimedPlaybackCapability),
        ImageViewportCapabilitySupport::True);
    QCOMPARE(ImageViewportInternal::providerCapabilitySupport(
                 providerSource.facts.providerFrameSeekCapability),
        ImageViewportCapabilitySupport::True);
    QCOMPARE(ImageViewportInternal::providerCapabilitySupport(
                 providerSource.facts.providerPositionSeekCapability),
        ImageViewportCapabilitySupport::True);
    QCOMPARE(providerSource.facts.providerThreadingContract,
        ImageSequenceProviderThreadingContract::ThreadSafe);
}

void ImageSequenceFactoryTest::assignedFactorySequenceSurvivesResultDestruction()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 50.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    result.reset();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingPrimaryRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
}

void ImageSequenceFactoryTest::assignedProviderSequenceSurvivesResultDestruction()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());
    QPointer<ImageSequence> observedSequence = result->sequence();

    ImageViewport item;
    item.setSize(QSizeF(100.0, 50.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    result.reset();

    QVERIFY(observedSequence);
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QVERIFY(sessionFactory->lastSession());

    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(primaryFrameCount(item), 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));

    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QVERIFY(!observedSequence);
}

void ImageSequenceFactoryTest::sharedFactorySequenceSurvivesFirstViewportDestruction()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport second;
    second.setSize(QSizeF(100.0, 50.0));
    const QMetaObject* metaObject = second.metaObject();
    {
        ImageViewport first;
        first.setSize(QSizeF(100.0, 50.0));
        first.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});
        second.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
            PresentationTargetTransitionPolicy {});
        acknowledgePendingPrimaryRenderCommitForTest(first);
        acknowledgePendingPrimaryRenderCommitForTest(second);

        QCOMPARE(requestStatusValue(first), enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(requestStatusValue(second), enumValue(metaObject, "RequestStatus", "Ready"));
    }

    QCOMPARE(requestStatusValue(second), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(second), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(second), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(second), 0);
    QCOMPARE(primaryDisplayedFrame(second), 0);
    QCOMPARE(primaryFrameCount(second), 1);
    QCOMPARE(displayedImageSize(second), QSizeF(16.0, 8.0));
    QCOMPARE(second.seek(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingPrimaryRenderCommitForTest(second);
    QCOMPARE(requestStatusValue(second), enumValue(metaObject, "RequestStatus", "Ready"));
}

void ImageSequenceFactoryTest::clearReleasesAssignedFactorySequenceOwner()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());
    QPointer<ImageSequence> observedSequence = result->sequence();

    ImageViewport item;
    item.setSize(QSizeF(100.0, 50.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    result.reset();

    QVERIFY(observedSequence);
    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QVERIFY(!observedSequence);
}

void ImageSequenceFactoryTest::imageFrameRetainsImmutablePayload()
{
    QImage image(2, 1, QImage::Format_ARGB32_Premultiplied);
    image.setPixelColor(0, 0, QColor(255, 0, 0, 255));
    image.setPixelColor(1, 0, QColor(0, 255, 0, 255));

    ImageFrame frame(image);
    image.fill(QColor(0, 0, 255, 255));

    const QImage retained = imageForTest(frame);
    QCOMPARE(retained.size(), QSize(2, 1));
    QCOMPARE(retained.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(retained.pixelColor(1, 0), QColor(0, 255, 0, 255));
}

void ImageSequenceFactoryTest::imageFrameExposesPayloadMetadata()
{
    QImage transparentImage(2, 1, QImage::Format_ARGB32_Premultiplied);
    transparentImage.fill(Qt::transparent);
    const ImageFrame transparentFrame(transparentImage);
    const QMetaObject* metaObject = transparentFrame.metaObject();

    QVERIFY(metaObject->indexOfProperty("valid") >= 0);
    QVERIFY(metaObject->indexOfProperty("sourceLogicalSize") >= 0);
    QVERIFY(metaObject->indexOfProperty("payloadByteSize") >= 0);
    QVERIFY(metaObject->indexOfProperty("payloadRasterSize") >= 0);
    QVERIFY(metaObject->indexOfProperty("sourceToPayloadScale") >= 0);
    QVERIFY(metaObject->indexOfProperty("quality") >= 0);
    QVERIFY(metaObject->indexOfProperty("exactness") >= 0);
    QVERIFY(metaObject->indexOfProperty("formatIdentifier") >= 0);
    QVERIFY(metaObject->indexOfProperty("hasAlpha") >= 0);
    QVERIFY(metaObject->indexOfProperty("orientationPolicy") >= 0);
    verifyEnumValues(metaObject, "OrientationPolicy",
        { "Identity", "MirrorHorizontally", "MirrorVertically", "Rotate180", "Rotate90",
            "MirrorHorizontallyAndRotate90", "MirrorVerticallyAndRotate90", "Rotate270" });

    QCOMPARE(transparentFrame.property("valid").toBool(), true);
    QCOMPARE(transparentFrame.property("sourceLogicalSize").toSizeF(), QSizeF(2.0, 1.0));
    QCOMPARE(transparentFrame.property("payloadByteSize").toLongLong(),
        transparentFrame.payloadByteSize());
    QCOMPARE(transparentFrame.property("payloadRasterSize").toSizeF(), QSizeF(2.0, 1.0));
    QCOMPARE(transparentFrame.property("sourceToPayloadScale").toSizeF(), QSizeF(1.0, 1.0));
    QCOMPARE(transparentFrame.property("quality").toInt(),
        static_cast<int>(ImageViewportPayloadQuality::Exact));
    QCOMPARE(transparentFrame.property("exactness").toInt(),
        static_cast<int>(ImageViewportPayloadExactness::ExactForSource));
    QCOMPARE(transparentFrame.property("hasAlpha").toBool(), true);
    QCOMPARE(transparentFrame.property("orientationPolicy").toInt(),
        enumValue(metaObject, "OrientationPolicy", "Identity"));

    QImage opaqueImage(2, 1, QImage::Format_RGB32);
    opaqueImage.fill(Qt::black);
    const ImageFrame opaqueFrame(opaqueImage);
    QCOMPARE(opaqueFrame.property("valid").toBool(), true);
    QCOMPARE(opaqueFrame.property("hasAlpha").toBool(), false);

    const ImageFrame emptyFrame;
    QCOMPARE(emptyFrame.property("valid").toBool(), false);
    QCOMPARE(emptyFrame.property("sourceLogicalSize").toSizeF(), QSizeF());
    QCOMPARE(emptyFrame.property("payloadByteSize").toLongLong(), 0);
    QCOMPARE(emptyFrame.property("payloadRasterSize").toSizeF(), QSizeF());
    QCOMPARE(emptyFrame.property("sourceToPayloadScale").toSizeF(), QSizeF());
    QCOMPARE(emptyFrame.property("hasAlpha").toBool(), false);
    QCOMPARE(emptyFrame.property("orientationPolicy").toInt(),
        enumValue(metaObject, "OrientationPolicy", "Identity"));
}

void ImageSequenceFactoryTest::imageFrameExplicitEnvelopeSeparatesLogicalAndPayloadSize()
{
    QImage previewPayload(8, 4, QImage::Format_ARGB32_Premultiplied);
    previewPayload.fill(Qt::transparent);

    ImageFrame previewFrame(previewPayload, QSizeF(16.0, 8.0), QSizeF(8.0, 4.0), QSizeF(0.5, 0.5),
        previewPayload.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QStringLiteral("argb32"));
    QCOMPARE(previewFrame.isValid(), true);
    QCOMPARE(previewFrame.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(previewFrame.payloadRasterSize(), QSizeF(8.0, 4.0));
    QCOMPARE(previewFrame.sourceToPayloadScale(), QSizeF(0.5, 0.5));
    QCOMPARE(previewFrame.payloadByteSize(), static_cast<qint64>(previewPayload.sizeInBytes()));
    QCOMPARE(previewFrame.quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(previewFrame.exactness(), ImageViewportPayloadExactness::NotExact);
    QCOMPARE(previewFrame.hasAlpha(), true);

    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> previewResult(factory.fromFrame(&previewFrame));
    QVERIFY(previewResult);
    QVERIFY(previewResult->sequence());
    QCOMPARE(previewResult->outcome(), ImageSequenceFactoryOutcome::Created);

    ImageFrame invalidFrame(previewPayload, QSizeF(16.0, 8.0), QSizeF(4.0, 4.0), QSizeF(0.5, 0.5),
        previewPayload.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QStringLiteral("argb32"));
    QCOMPARE(invalidFrame.isValid(), false);
    QScopedPointer<ImageSequenceFactoryResult> invalidResult(factory.fromFrame(&invalidFrame));
    QVERIFY(invalidResult);
    QCOMPARE(invalidResult->sequence(), nullptr);
    QCOMPARE(invalidResult->outcome(), ImageSequenceFactoryOutcome::Rejected);
}

void ImageSequenceFactoryTest::imageFrameOrientationPoliciesNormalizePayload()
{
    QImage image(3, 2, QImage::Format_ARGB32);
    image.setPixelColor(0, 0, QColor(255, 0, 0, 255));
    image.setPixelColor(1, 0, QColor(0, 255, 0, 255));
    image.setPixelColor(2, 0, QColor(0, 0, 255, 255));
    image.setPixelColor(0, 1, QColor(255, 255, 0, 255));
    image.setPixelColor(1, 1, QColor(0, 255, 255, 255));
    image.setPixelColor(2, 1, QColor(255, 0, 255, 255));

    const auto expectedImage = [&image](ImageFrame::OrientationPolicy policy) {
        switch (policy) {
        case ImageFrame::OrientationPolicy::Identity:
            return image;
        case ImageFrame::OrientationPolicy::MirrorHorizontally:
            return image.flipped(Qt::Horizontal);
        case ImageFrame::OrientationPolicy::MirrorVertically:
            return image.flipped(Qt::Vertical);
        case ImageFrame::OrientationPolicy::Rotate180:
            return image.transformed(QTransform().rotate(180));
        case ImageFrame::OrientationPolicy::Rotate90:
            return image.transformed(QTransform().rotate(90));
        case ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90:
            return image.flipped(Qt::Horizontal).transformed(QTransform().rotate(90));
        case ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90:
            return image.flipped(Qt::Vertical).transformed(QTransform().rotate(90));
        case ImageFrame::OrientationPolicy::Rotate270:
            return image.transformed(QTransform().rotate(270));
        }
        return QImage();
    };

    const QList<ImageFrame::OrientationPolicy> policies = {
        ImageFrame::OrientationPolicy::Identity,
        ImageFrame::OrientationPolicy::MirrorHorizontally,
        ImageFrame::OrientationPolicy::MirrorVertically,
        ImageFrame::OrientationPolicy::Rotate180,
        ImageFrame::OrientationPolicy::Rotate90,
        ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90,
        ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90,
        ImageFrame::OrientationPolicy::Rotate270,
    };

    for (ImageFrame::OrientationPolicy policy : policies) {
        const QImage expected = expectedImage(policy);
        const ImageFrame frame(image, policy);
        QCOMPARE(frame.isValid(), true);
        QCOMPARE(frame.orientationPolicy(), policy);
        QCOMPARE(frame.sourceLogicalSize(), expected.deviceIndependentSize());
        QCOMPARE(imageForTest(frame).size(), expected.size());
        for (int y = 0; y < expected.height(); ++y) {
            for (int x = 0; x < expected.width(); ++x) {
                QCOMPARE(imageForTest(frame).pixelColor(x, y), expected.pixelColor(x, y));
            }
        }
    }

    ImageFrame rotatedFrame(image, ImageFrame::OrientationPolicy::Rotate90);
    QImage matchingImage(2, 3, QImage::Format_ARGB32);
    matchingImage.fill(Qt::black);
    ImageFrame matchingFrame(matchingImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&rotatedFrame, 100));
    QVERIFY(list.appendFrame(&matchingFrame, 250));
    QCOMPARE(list.count(), 2);

    ImageFrame mismatchedFrame(image);
    QVERIFY(!list.appendFrame(&mismatchedFrame, 100));
    QVERIFY(list.errorString().contains(QStringLiteral("logical size")));
}

void ImageSequenceFactoryTest::imageFrameUsesDeviceIndependentLogicalSize()
{
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);
    image.fill(Qt::transparent);

    ImageFrame frame(image);

    QCOMPARE(frame.isValid(), true);
    QCOMPARE(frame.sourceLogicalSize(), QSizeF(2.0, 1.0));
    QCOMPARE(frame.payloadByteSize(), static_cast<qint64>(image.sizeInBytes()));

    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(20.0, 20.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(item);

    QCOMPARE(displayedImageSize(item), QSizeF(2.0, 1.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 5.0, 20.0, 10.0));

    QScopedPointer<ImageSequenceFactoryResult> nativeFrameResult(factory.fromFrame(image));
    QVERIFY(nativeFrameResult->sequence());
    ImageViewport nativeFrameItem;
    nativeFrameItem.setSize(QSizeF(20.0, 20.0));
    nativeFrameItem.setPresentationTarget(
        ImageViewportPresentationTarget(nativeFrameResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(nativeFrameItem);
    QCOMPARE(displayedImageSize(nativeFrameItem), QSizeF(2.0, 1.0));
    QCOMPARE(contentRect(nativeFrameItem), QRectF(0.0, 5.0, 20.0, 10.0));

    QScopedPointer<ImageSequenceFactoryResult> nativeTimedResult(
        factory.fromTimedFrameList({ image, image }, { 100, 200 }));
    QVERIFY(nativeTimedResult->sequence());
    ImageViewport nativeTimedItem;
    nativeTimedItem.setSize(QSizeF(20.0, 20.0));
    nativeTimedItem.setPresentationTarget(
        ImageViewportPresentationTarget(nativeTimedResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingPrimaryRenderCommitForTest(nativeTimedItem);
    QCOMPARE(displayedImageSize(nativeTimedItem), QSizeF(2.0, 1.0));
    QCOMPARE(primaryFrameCount(nativeTimedItem), 2);
    QCOMPARE(primaryTotalDuration(nativeTimedItem), 300);

    QImage fractionalLogicalImage(3, 2, QImage::Format_ARGB32_Premultiplied);
    fractionalLogicalImage.setDevicePixelRatio(2.0);
    fractionalLogicalImage.fill(Qt::transparent);
    ImageFrame fractionalLogicalFrame(fractionalLogicalImage);
    QCOMPARE(fractionalLogicalFrame.isValid(), false);
}

void ImageSequenceFactoryTest::timingIntervalsResolveHalfOpenBoundaries()
{
    const TimingIntervals timing = TimingIntervals::fromFrameDurations({ 100, 250, 50 });

    QVERIFY(timing.isValid());
    QCOMPARE(timing.frameCount(), 3);
    QCOMPARE(timing.totalDuration(), 400);
    QCOMPARE(timing.frameStartPosition(0), 0);
    QCOMPARE(timing.frameStartPosition(1), 100);
    QCOMPARE(timing.frameStartPosition(2), 350);
    QCOMPARE(timing.frameStartPosition(3), -1);
    QCOMPARE(timing.frameDuration(0), 100);
    QCOMPARE(timing.frameDuration(1), 250);
    QCOMPARE(timing.frameDuration(2), 50);
    QCOMPARE(timing.frameDuration(3), -1);
    QCOMPARE(timing.frameIndexForPosition(-1), -1);
    QCOMPARE(timing.frameIndexForPosition(0), 0);
    QCOMPARE(timing.frameIndexForPosition(99), 0);
    QCOMPARE(timing.frameIndexForPosition(100), 1);
    QCOMPARE(timing.frameIndexForPosition(349), 1);
    QCOMPARE(timing.frameIndexForPosition(350), 2);
    QCOMPARE(timing.frameIndexForPosition(399), 2);
    QCOMPARE(timing.frameIndexForPosition(400), 2);
    QCOMPARE(timing.frameIndexForPosition(401), -1);
}

void ImageSequenceFactoryTest::timingIntervalsRejectInvalidDurations()
{
    QVERIFY(!TimingIntervals::fromFrameDurations({}).isValid());
    QVERIFY(!TimingIntervals::fromFrameDurations({ 100, 0 }).isValid());
    QVERIFY(!TimingIntervals::fromFrameDurations({ -1 }).isValid());
}

void ImageSequenceFactoryTest::stillImageSequenceRetainsFactoryPayload()
{
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result;
    {
        QImage image(2, 1, QImage::Format_ARGB32_Premultiplied);
        image.setPixelColor(0, 0, QColor(255, 0, 0, 255));
        image.setPixelColor(1, 0, QColor(0, 255, 0, 255));
        ImageFrame frame(image);
        result.reset(factory.fromFrame(&frame));
        QVERIFY(result->sequence());
    }

    const QImage retained = ImageViewportInternal::sourceFrameImage(
        ImageViewportInternal::factorySequenceSource(result->sequence()), 0);
    QCOMPARE(retained.size(), QSize(2, 1));
    QCOMPARE(retained.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(retained.pixelColor(1, 0), QColor(0, 255, 0, 255));
}

void ImageSequenceFactoryTest::timedFrameListSequenceRetainsFactoryPayloads()
{
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result;
    {
        QImage firstImage(2, 1, QImage::Format_ARGB32_Premultiplied);
        firstImage.fill(QColor(255, 0, 0, 255));
        QImage secondImage(2, 1, QImage::Format_ARGB32_Premultiplied);
        secondImage.fill(QColor(0, 255, 0, 255));
        ImageFrame firstFrame(firstImage);
        ImageFrame secondFrame(secondImage);
        TimedImageFrameList list;
        QVERIFY(list.appendFrame(&firstFrame, 100));
        QVERIFY(list.appendFrame(&secondFrame, 250));

        result.reset(factory.fromTimedFrameList(&list));
        QVERIFY(result->sequence());
    }

    const ImageViewportInternal::ImageSequenceSource source
        = ImageViewportInternal::factorySequenceSource(result->sequence());
    const QImage firstRetained = ImageViewportInternal::sourceFrameImage(source, 0);
    QCOMPARE(firstRetained.size(), QSize(2, 1));
    QCOMPARE(firstRetained.pixelColor(0, 0), QColor(255, 0, 0, 255));

    const QImage secondRetained = ImageViewportInternal::sourceFrameImage(source, 1);
    QCOMPARE(secondRetained.size(), QSize(2, 1));
    QCOMPARE(secondRetained.pixelColor(0, 0), QColor(0, 255, 0, 255));
}

void ImageSequenceFactoryTest::commandsWithoutRequestAreIgnoredDiagnostics()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));

    QCOMPARE(item.seek(ImageViewportPageRole::Primary, -1).outcome(),
        ImageViewportCommandOutcome::IgnoredNoRequest);
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());

    QCOMPARE(item.stop(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());

    QCOMPARE(item.seekToPosition(ImageViewportPageRole::Primary, 0).outcome(),
        ImageViewportCommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    QCOMPARE(item.clear().outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QVERIFY(revisionTokenProperty(item, "requestRevision").isValid());
    QVERIFY(revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(stateSpy.count(), 0);
}

QTEST_MAIN(ImageSequenceFactoryTest)

#include "tst_imagesequence_factory.moc"
