#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

class ImageViewportPublicApiCommandsTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportPublicApiCommandsTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void unsupportedSequencePropertyWritesPreserveState();
    void sequenceAssignmentPreservesCommandDiagnostic();
    void setPageSetAcceptsPrimaryAndSecondaryAtomically();
    void cppTypedPageSetOverloadsCompileAndReplaceSpread();
    void canonicalPageSetValueDefaultsAndConstruction();
    void canonicalPageSetOverloadMatchesPrimarySecondaryPath();
    void canonicalPageSetRejectsSecondaryWithoutPrimary();
    void pageSetAssignmentUpdatesSnapshotGenerationIdentity();
    void qmlCanonicalPageSetValueAssignsAndRejectsArbitraryVariants();
    void compatibilitySequenceAssignmentClearsSecondaryRole();
    void clearStylePageSetWithSecondaryClearsAcceptedRoles();
    void clearStylePageSetWithProviderSecondaryDoesNotStartProvider();
    void clearStylePageSetPolicyPreservesPresentationPreferences();
    void invalidPageSetSecondaryPreservesAcceptedRoles();
    void invalidPageRoleArgumentsPreserveRevisionTokens();
    void roleCommandsWithInvalidRolePublishCommandDiagnostics();
    void secondaryRoleCommandsWithoutSecondaryPublishNoRequestDiagnostics();
    void pageSetTransitionClearBeforeLoadClearsRetainedDisplay();
    void presentationCommandAppliesAndRejectsTransactionally();
    void invalidPageSetTransitionPolicyPreservesState();
    void invalidClearStyleTransitionPolicyPreservesState();
    void invalidPresentationCommandsPreserveDiagnostics();
};

static ImageViewport::CommandOutcome setSpreadDirectionCommand(
    ImageViewport& item, ImageViewport::SpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setPageGapCommand(ImageViewport& item, double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return item.setPresentation(command);
}

static ImageViewport::CommandOutcome setManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(percent);
    return item.setPresentation(command);
}

void ImageViewportPublicApiCommandsTest::unsupportedSequencePropertyWritesPreserveState()
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
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
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
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    }

    QCOMPARE(sequenceSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportPublicApiCommandsTest::sequenceAssignmentPreservesCommandDiagnostic()
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
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    item.setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(item.sequence(), result->sequence());
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(commandSpy.count(), 0);
}

void ImageViewportPublicApiCommandsTest::setPageSetAcceptsPrimaryAndSecondaryAtomically()
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
        secondaryObservedDuringSignal = item.property("secondarySequence").value<ImageSequence*>();
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
    QCOMPARE(rangeProperty(item, "primaryFrameSeekBounds").minimum(), 0);
    QCOMPARE(rangeProperty(item, "secondaryFrameSeekBounds").maximum(), 0);
    QCOMPARE(item.property("primaryFrameSeekSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "True"));
    QCOMPARE(item.property("secondaryFrameSeekSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "True"));
    QCOMPARE(item.property("secondaryTimedPlaybackSupport").toInt(),
        enumValue(item.metaObject(), "TriState", "False"));
}

void ImageViewportPublicApiCommandsTest::cppTypedPageSetOverloadsCompileAndReplaceSpread()
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

    QImage replacementImage(12, 6, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));

    const auto spreadOutcome
        = item.setPageSet(primaryResult->sequence(), secondaryResult->sequence());

    QCOMPARE(spreadOutcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.sequence(), primaryResult->sequence());
    QCOMPARE(item.primarySequence(), primaryResult->sequence());
    QCOMPARE(item.secondarySequence(), secondaryResult->sequence());
    QCOMPARE(item.property("secondaryFrameCount").toInt(), 1);

    PageSetTransitionPolicy policy;
    policy.setPageGapTransition(PageSetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(4.0);

    const auto replacementOutcome = item.setPageSet(replacementResult->sequence(), nullptr, policy);

    QCOMPARE(replacementOutcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(item.primarySequence(), replacementResult->sequence());
    QCOMPARE(item.secondarySequence(), nullptr);
    QCOMPARE(item.pageGap(), 4.0);
}

void ImageViewportPublicApiCommandsTest::canonicalPageSetValueDefaultsAndConstruction()
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

    const ImageViewportPageSet defaultPageSet;
    QVERIFY(defaultPageSet.isClear());
    QVERIFY(defaultPageSet.isValid());
    QCOMPARE(defaultPageSet.primary(), nullptr);
    QCOMPARE(defaultPageSet.secondary(), nullptr);
    QCOMPARE(defaultPageSet, ImageViewportPageSet::clear());

    const ImageViewportPageSet primaryOnly(primaryResult->sequence());
    QVERIFY(!primaryOnly.isClear());
    QVERIFY(primaryOnly.isValid());
    QCOMPARE(primaryOnly.primary(), primaryResult->sequence());
    QCOMPARE(primaryOnly.secondary(), nullptr);

    const ImageViewportPageSet spread(primaryResult->sequence(), secondaryResult->sequence());
    QVERIFY(spread.isValid());
    QCOMPARE(spread.primary(), primaryResult->sequence());
    QCOMPARE(spread.secondary(), secondaryResult->sequence());
    QVERIFY(spread != primaryOnly);

    ImageViewportPageSet secondaryOnly;
    secondaryOnly.setSecondary(secondaryResult->sequence());
    QVERIFY(!secondaryOnly.isClear());
    QVERIFY(!secondaryOnly.isValid());
}

void ImageViewportPublicApiCommandsTest::canonicalPageSetOverloadMatchesPrimarySecondaryPath()
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
    const ImageViewportPageSet spread(primaryResult->sequence(), secondaryResult->sequence());

    QCOMPARE(item.setPageSet(spread), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.primarySequence(), primaryResult->sequence());
    QCOMPARE(item.secondarySequence(), secondaryResult->sequence());
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(item.state().primary().sequence(), primaryResult->sequence());
    QCOMPARE(item.state().secondary().sequence(), secondaryResult->sequence());
    QCOMPARE(item.primaryFrameCount(), 1);
    QCOMPARE(item.secondaryFrameCount(), 1);

    PageSetTransitionPolicy policy;
    policy.setDisplayTransition(PageSetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    QCOMPARE(item.setPageSet(ImageViewportPageSet::clear(), policy),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.primarySequence(), nullptr);
    QCOMPARE(item.secondarySequence(), nullptr);
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(item.displayStatus(), ImageViewport::DisplayStatus::Empty);
}

void ImageViewportPublicApiCommandsTest::canonicalPageSetRejectsSecondaryWithoutPrimary()
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
    QCOMPARE(item.setPageSet(ImageViewportPageSet(primaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const RevisionToken requestRevision = item.requestRevision();
    const RevisionToken displayRevision = item.displayRevision();
    const RevisionToken commandRevision = item.commandRevision();

    ImageViewportPageSet secondaryOnly;
    secondaryOnly.setSecondary(secondaryResult->sequence());

    QCOMPARE(item.setPageSet(secondaryOnly), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.primarySequence(), primaryResult->sequence());
    QCOMPARE(item.secondarySequence(), nullptr);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.displayRevision(), displayRevision);
    QVERIFY(item.commandRevision() != commandRevision);
    QCOMPARE(item.commandReason(), ImageViewport::CommandReason::InvalidRequest);
}

void ImageViewportPublicApiCommandsTest::pageSetAssignmentUpdatesSnapshotGenerationIdentity()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.state().request().acceptedPageSetGeneration().isValid(), false);
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(item.state().request().targetRoleSet(), ImageViewportRoleSet(false, false));

    QCOMPARE(item.setPageSet(ImageViewportPageSet(primaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot primarySnapshot = item.state();
    const ImageViewportPageSetGenerationToken primaryGeneration
        = primarySnapshot.request().acceptedPageSetGeneration();
    QVERIFY(primaryGeneration.isValid());
    QCOMPARE(primarySnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(primarySnapshot.request().targetRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(primarySnapshot.primary().present(), true);
    QCOMPARE(primarySnapshot.primary().sequence(), primaryResult->sequence());
    QCOMPARE(primarySnapshot.primary().request().pageSetGeneration(), primaryGeneration);
    QCOMPARE(primarySnapshot.secondary().present(), false);

    QCOMPARE(item.setPageSet(
                 ImageViewportPageSet(primaryResult->sequence(), secondaryResult->sequence())),
        ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot spreadSnapshot = item.state();
    const ImageViewportPageSetGenerationToken spreadGeneration
        = spreadSnapshot.request().acceptedPageSetGeneration();
    QVERIFY(spreadGeneration.isValid());
    QVERIFY(spreadGeneration != primaryGeneration);
    QCOMPARE(spreadSnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(spreadSnapshot.request().targetRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(spreadSnapshot.primary().request().pageSetGeneration(), spreadGeneration);
    QCOMPARE(spreadSnapshot.secondary().present(), true);
    QCOMPARE(spreadSnapshot.secondary().sequence(), secondaryResult->sequence());
    QCOMPARE(spreadSnapshot.secondary().request().pageSetGeneration(), spreadGeneration);

    QCOMPARE(
        item.setPageSet(ImageViewportPageSet::clear()), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot clearSnapshot = item.state();
    QCOMPARE(clearSnapshot.request().acceptedPageSetGeneration().isValid(), false);
    QCOMPARE(clearSnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(clearSnapshot.request().targetRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(clearSnapshot.primary().present(), false);
    QCOMPARE(clearSnapshot.secondary().present(), false);
}

void ImageViewportPublicApiCommandsTest::
    qmlCanonicalPageSetValueAssignsAndRejectsArbitraryVariants()
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

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter rawProvider(sessionFactory);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty(QStringLiteral("rawProvider"), &rawProvider);
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    width: 100
    height: 100

    property ImageSequence suppliedPrimary
    property ImageSequence suppliedSecondary
    property imageViewportPageSet pageSet
    property pageSetTransitionPolicy policy
    property bool typedAccepted: false
    property bool typedPolicyAccepted: false
    property bool stringRejected: false
    property bool objectRejected: false
    property bool providerRejected: false

    QtObject { id: rawObject }

    function invalidResult(value) {
        try {
            return setPageSet(value, pageSet) === ImageViewport.CommandOutcome.Invalid
        } catch (error) {
            return true
        }
    }

    Component.onCompleted: {
        pageSet.primary = suppliedPrimary
        pageSet.secondary = suppliedSecondary
        typedAccepted = setPageSet(pageSet) === ImageViewport.CommandOutcome.Accepted
            && primarySequence === suppliedPrimary
            && secondarySequence === suppliedSecondary
            && state.request.acceptedRoleSet.primary
            && state.request.acceptedRoleSet.secondary
        typedPolicyAccepted = setPageSet(pageSet, policy) === ImageViewport.CommandOutcome.Accepted
            && primarySequence === suppliedPrimary
            && secondarySequence === suppliedSecondary
        const requestRevisionBefore = requestRevision
        const displayRevisionBefore = displayRevision
        stringRejected = invalidResult("image.png")
            && primarySequence === suppliedPrimary
            && secondarySequence === suppliedSecondary
            && requestRevision === requestRevisionBefore
            && displayRevision === displayRevisionBefore
        objectRejected = invalidResult({ primary: suppliedPrimary })
            && primarySequence === suppliedPrimary
            && secondarySequence === suppliedSecondary
            && requestRevision === requestRevisionBefore
            && displayRevision === displayRevisionBefore
        providerRejected = invalidResult(rawProvider)
            && primarySequence === suppliedPrimary
            && secondarySequence === suppliedSecondary
            && requestRevision === requestRevisionBefore
            && displayRevision === displayRevisionBefore
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
    QCOMPARE(object->property("typedAccepted").toBool(), true);
    QCOMPARE(object->property("typedPolicyAccepted").toBool(), true);
    QCOMPARE(object->property("stringRejected").toBool(), true);
    QCOMPARE(object->property("objectRejected").toBool(), true);
    QCOMPARE(object->property("providerRejected").toBool(), true);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportPublicApiCommandsTest::compatibilitySequenceAssignmentClearsSecondaryRole()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame firstFrame(image);
    ImageFrame secondFrame(image);
    ImageFrame replacementFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondResult(factory.fromFrame(&secondFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
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

void ImageViewportPublicApiCommandsTest::clearStylePageSetWithSecondaryClearsAcceptedRoles()
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

    const auto outcome
        = item.setPageSet(QVariant(), QVariant::fromValue<QObject*>(secondaryResult->sequence()));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("primarySequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("secondarySequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
}

void ImageViewportPublicApiCommandsTest::
    clearStylePageSetWithProviderSecondaryDoesNotStartProvider()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
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
    item.setSequence(primaryResult->sequence());

    const auto outcome
        = item.setPageSet(QVariant(), QVariant::fromValue<QObject*>(secondaryResult->sequence()));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("primarySequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("secondarySequence").value<QObject*>(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(),
        enumValue(item.metaObject(), "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleSpreadRect").toRectF(), QRectF());
    QCOMPARE(item.property("primaryPageRect").toRectF(), QRectF());
    QCOMPARE(item.property("secondaryPageRect").toRectF(), QRectF());
}

void ImageViewportPublicApiCommandsTest::clearStylePageSetPolicyPreservesPresentationPreferences()
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
    QCOMPARE(setManualZoomPercentCommand(item, 250.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 9.0), ImageViewport::CommandOutcome::Accepted);
    ImageViewportPresentationCommand rotationCommand;
    rotationCommand.setRotationDegrees(90);
    QCOMPARE(item.setPresentation(rotationCommand), ImageViewport::CommandOutcome::Accepted);
    ImageViewportPresentationCommand mirrorCommand;
    mirrorCommand.setMirrorHorizontally(true);
    mirrorCommand.setMirrorVertically(true);
    QCOMPARE(item.setPresentation(mirrorCommand), ImageViewport::CommandOutcome::Accepted);
    item.setSmoothing(false);
    item.setMipmap(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setLooping(true);

    const auto fitMode = item.fitMode();
    const double zoomPercent = item.zoomPercent();
    const QPointF contentPosition = item.property("contentPosition").toPointF();
    const auto spreadDirection = item.spreadDirection();
    const double pageGap = item.pageGap();
    const int rotationDegrees = item.rotationDegrees();
    const bool mirrorHorizontally = item.mirrorHorizontally();
    const bool mirrorVertically = item.mirrorVertically();
    const bool smoothing = item.smoothing();
    const bool mipmap = item.mipmap();
    const auto backgroundMode = item.backgroundMode();
    const QColor backgroundColor = item.backgroundColor();
    const bool looping = item.looping();

    PageSetTransitionPolicy policy;
    policy.setDisplayTransition(PageSetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    policy.setZoomTransition(PageSetTransitionPolicy::ZoomTransition::ResetToContain);
    policy.setContentPositionTransition(
        PageSetTransitionPolicy::ContentPositionTransition::ScanEnd);
    policy.setRotationTransition(PageSetTransitionPolicy::RotationTransition::Reset);
    policy.setMirrorTransition(PageSetTransitionPolicy::MirrorTransition::Reset);
    policy.setFitModeTransition(PageSetTransitionPolicy::FitModeTransition::SetExplicit);
    policy.setFitMode(ImageViewport::FitMode::Contain);
    policy.setSpreadDirectionTransition(
        PageSetTransitionPolicy::SpreadDirectionTransition::SetExplicit);
    policy.setSpreadDirection(ImageViewport::SpreadDirection::LeftToRight);
    policy.setPageGapTransition(PageSetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(0.0);
    policy.setReplacementIntent(PageSetTransitionPolicy::ReplacementIntent::SameTargetRefinement);

    const auto outcome = item.setPageSet(QVariant(), QVariant(), policy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestStatus").toInt(),
        enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    QCOMPARE(item.fitMode(), fitMode);
    QCOMPARE(item.zoomPercent(), zoomPercent);
    QCOMPARE(item.property("contentPosition").toPointF(), contentPosition);
    QCOMPARE(item.spreadDirection(), spreadDirection);
    QCOMPARE(item.pageGap(), pageGap);
    QCOMPARE(item.rotationDegrees(), rotationDegrees);
    QCOMPARE(item.mirrorHorizontally(), mirrorHorizontally);
    QCOMPARE(item.mirrorVertically(), mirrorVertically);
    QCOMPARE(item.smoothing(), smoothing);
    QCOMPARE(item.mipmap(), mipmap);
    QCOMPARE(item.backgroundMode(), backgroundMode);
    QCOMPARE(item.backgroundColor(), backgroundColor);
    QCOMPARE(item.looping(), looping);
}

void ImageViewportPublicApiCommandsTest::invalidPageSetSecondaryPreservesAcceptedRoles()
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
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    const auto outcome = item.setPageSet(
        QVariant::fromValue<QObject*>(primaryResult->sequence()), QVariant(QStringLiteral("bad")));

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("primarySequence").value<ImageSequence*>(), primaryResult->sequence());
    QCOMPARE(
        item.property("secondarySequence").value<ImageSequence*>(), secondaryResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
}

void ImageViewportPublicApiCommandsTest::invalidPageRoleArgumentsPreserveRevisionTokens()
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

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    const int requestStatus = item.property("requestStatus").toInt();
    const int displayStatus = item.property("displayStatus").toInt();
    const int playbackPhase = item.property("playbackPhase").toInt();
    const QMetaObject* metaObject = item.metaObject();
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);

    const auto invalidPrimaryOutcome = item.setPageSet(QVariant(QStringLiteral("bad primary")),
        QVariant::fromValue<QObject*>(secondaryResult->sequence()));

    QCOMPARE(invalidPrimaryOutcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    commandRevision = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(item.primarySequence(), primaryResult->sequence());
    QCOMPARE(item.secondarySequence(), secondaryResult->sequence());
    QCOMPARE(item.property("requestStatus").toInt(), requestStatus);
    QCOMPARE(item.property("displayStatus").toInt(), displayStatus);
    QCOMPARE(item.property("playbackPhase").toInt(), playbackPhase);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(commandSpy.count(), 1);

    const auto invalidSecondaryOutcome
        = item.setPageSet(QVariant::fromValue<QObject*>(primaryResult->sequence()),
            QVariant(QStringLiteral("bad secondary")));

    QCOMPARE(invalidSecondaryOutcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(item.primarySequence(), primaryResult->sequence());
    QCOMPARE(item.secondarySequence(), secondaryResult->sequence());
    QCOMPARE(item.property("requestStatus").toInt(), requestStatus);
    QCOMPARE(item.property("displayStatus").toInt(), displayStatus);
    QCOMPARE(item.property("playbackPhase").toInt(), playbackPhase);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(commandSpy.count(), 2);
}

void ImageViewportPublicApiCommandsTest::roleCommandsWithInvalidRolePublishCommandDiagnostics()
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
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const auto invalidRole = static_cast<ImageViewport::PageRole>(
        999); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    const auto verifyInvalidCommand = [&](ImageViewport::CommandOutcome outcome) {
        QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
        QCOMPARE(item.property("commandReason").toInt(),
            enumValue(metaObject, "CommandReason", "InvalidRequest"));
        verifyRevisionChanged(item, "commandRevision", commandRevision);
        commandRevision = revisionTokenProperty(item, "commandRevision");
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
        QCOMPARE(item.property("requestStatus").toInt(),
            enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(item.property("displayStatus").toInt(),
            enumValue(metaObject, "DisplayStatus", "Ready"));
    };

    verifyInvalidCommand(item.play(invalidRole));
    verifyInvalidCommand(item.pause(invalidRole));
    verifyInvalidCommand(item.stop(invalidRole));
    verifyInvalidCommand(item.seek(invalidRole, 0));
    verifyInvalidCommand(item.seekToPosition(invalidRole, 0));
}

void ImageViewportPublicApiCommandsTest::
    secondaryRoleCommandsWithoutSecondaryPublishNoRequestDiagnostics()
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
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    const auto verifyIgnoredCommand = [&](ImageViewport::CommandOutcome outcome) {
        QCOMPARE(outcome, ImageViewport::CommandOutcome::IgnoredNoRequest);
        QCOMPARE(item.property("commandReason").toInt(),
            enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
        verifyRevisionChanged(item, "commandRevision", commandRevision);
        commandRevision = revisionTokenProperty(item, "commandRevision");
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
        QCOMPARE(item.property("requestStatus").toInt(),
            enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(item.property("displayStatus").toInt(),
            enumValue(metaObject, "DisplayStatus", "Ready"));
    };

    verifyIgnoredCommand(item.play(ImageViewport::PageRole::Secondary));
    verifyIgnoredCommand(item.pause(ImageViewport::PageRole::Secondary));
    verifyIgnoredCommand(item.stop(ImageViewport::PageRole::Secondary));
    verifyIgnoredCommand(item.seek(ImageViewport::PageRole::Secondary, 0));
    verifyIgnoredCommand(item.seekToPosition(ImageViewport::PageRole::Secondary, 0));
}

void ImageViewportPublicApiCommandsTest::pageSetTransitionClearBeforeLoadClearsRetainedDisplay()
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
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    PageSetTransitionPolicy policy;
    policy.setDisplayTransition(PageSetTransitionPolicy::DisplayTransition::ClearBeforeLoad);

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

void ImageViewportPublicApiCommandsTest::presentationCommandAppliesAndRejectsTransactionally()
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
    acknowledgePendingRenderCommitForTest(item);

    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(150.0);
    command.setSpreadDirection(ImageViewport::SpreadDirection::RightToLeft);
    command.setPageGap(5.0);
    command.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    command.setBackgroundColor(QColor(10, 20, 30, 255));
    command.setSmoothing(false);
    command.setMipmap(true);
    command.setLooping(true);
    command.setQualityPreference(ImageViewport::QualityPreference::ExactDetail);
    command.setExactnessPreference(ImageViewport::ExactnessPreference::RequireExact);

    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.zoomPercent(), 150.0);
    QCOMPARE(item.spreadDirection(), ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(item.pageGap(), 5.0);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.backgroundColor(), QColor(10, 20, 30, 255));
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.looping(), true);
    QCOMPARE(item.state().presentation().qualityPreference(),
        ImageViewport::QualityPreference::ExactDetail);
    QCOMPARE(item.state().presentation().exactnessPreference(),
        ImageViewport::ExactnessPreference::RequireExact);

    QCOMPARE(setManualZoomPercentCommand(item, 1000.0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.maximumContentPosition().x() > 0.0 || item.maximumContentPosition().y() > 0.0);

    ImageViewportPresentationCommand scanCommand;
    scanCommand.setScanDirection(ImageViewport::ScanDirection::End);
    QCOMPARE(item.setPresentation(scanCommand), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.contentPosition(), item.maximumContentPosition());

    scanCommand = {};
    scanCommand.setScanDirection(ImageViewport::ScanDirection::Start);
    QCOMPARE(item.setPresentation(scanCommand), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.contentPosition(), QPointF());

    const RevisionToken requestRevision = item.requestRevision();
    const RevisionToken displayRevision = item.displayRevision();
    const RevisionToken commandRevision = item.commandRevision();
    const double zoomPercent = item.zoomPercent();
    const double pageGap = item.pageGap();
    const auto spreadDirection = item.spreadDirection();

    ImageViewportPresentationCommand invalidCommand;
    invalidCommand.setManualZoomPercent(125.0);
    invalidCommand.setScanDirection(ImageViewport::ScanDirection::Next);
    invalidCommand.setPageGap(12.0);

    QCOMPARE(item.setPresentation(invalidCommand), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.zoomPercent(), zoomPercent);
    QCOMPARE(item.pageGap(), pageGap);
    QCOMPARE(item.spreadDirection(), spreadDirection);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.displayRevision(), displayRevision);
    QVERIFY(item.commandRevision() != commandRevision);
    QCOMPARE(item.commandReason(), ImageViewport::CommandReason::InvalidRequest);

    const RevisionToken invalidCommandRevision = item.commandRevision();
    ImageViewportPresentationCommand invalidDirectionCommand;
    const auto invalidDirection = static_cast<ImageViewport::SpreadDirection>(
        999); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    invalidDirectionCommand.setSpreadDirection(invalidDirection);
    QCOMPARE(item.setPresentation(invalidDirectionCommand), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.zoomPercent(), zoomPercent);
    QCOMPARE(item.pageGap(), pageGap);
    QCOMPARE(item.spreadDirection(), spreadDirection);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.displayRevision(), displayRevision);
    QVERIFY(item.commandRevision() != invalidCommandRevision);
    QCOMPARE(item.commandReason(), ImageViewport::CommandReason::InvalidRequest);

    RevisionToken previousInvalidCommandRevision = item.commandRevision();
    ImageViewportPresentationCommand invalidPageGapCommand;
    invalidPageGapCommand.setPageGap(-1.0);
    QCOMPARE(item.setPresentation(invalidPageGapCommand), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.zoomPercent(), zoomPercent);
    QCOMPARE(item.pageGap(), pageGap);
    QCOMPARE(item.spreadDirection(), spreadDirection);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.displayRevision(), displayRevision);
    QVERIFY(item.commandRevision() != previousInvalidCommandRevision);
    QCOMPARE(item.commandReason(), ImageViewport::CommandReason::InvalidRequest);

    previousInvalidCommandRevision = item.commandRevision();
    invalidPageGapCommand = {};
    invalidPageGapCommand.setPageGap(std::numeric_limits<double>::infinity());
    QCOMPARE(item.setPresentation(invalidPageGapCommand), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.zoomPercent(), zoomPercent);
    QCOMPARE(item.pageGap(), pageGap);
    QCOMPARE(item.spreadDirection(), spreadDirection);
    QCOMPARE(item.requestRevision(), requestRevision);
    QCOMPARE(item.displayRevision(), displayRevision);
    QVERIFY(item.commandRevision() != previousInvalidCommandRevision);
    QCOMPARE(item.commandReason(), ImageViewport::CommandReason::InvalidRequest);

    ImageViewportPresentationCommand resetCommand
        = ImageViewportPresentationCommand::resetViewCommand();
    resetCommand.setBackgroundMode(ImageViewport::BackgroundMode::Checkerboard);

    QCOMPARE(item.setPresentation(resetCommand), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Checkerboard);

    ImageViewportCoordinateInput mapInput;
    mapInput.setSourceSpace(ImageViewport::CoordinateSpace::Item);
    mapInput.setTargetSpace(ImageViewport::CoordinateSpace::Spread);
    mapInput.setPoint(QPointF(50.0, 50.0));
    const ImageViewportCoordinateResult mapped = item.mapPoint(mapInput);
    QVERIFY(mapped.isValid());
    QCOMPARE(mapped.sourceSpace(), ImageViewport::CoordinateSpace::Item);
    QCOMPARE(mapped.targetSpace(), ImageViewport::CoordinateSpace::Spread);
    QCOMPARE(item.containsPoint(mapInput), true);

    ImageViewportCoordinateInput pageInput = mapInput;
    pageInput.setTargetSpace(ImageViewport::CoordinateSpace::Page);
    pageInput.setPageRole(QVariant::fromValue(ImageViewport::PageRole::Primary));
    QVERIFY(item.mapPoint(pageInput).isValid());

    ImageViewportCoordinateInput nearestInput;
    nearestInput.setSourceSpace(ImageViewport::CoordinateSpace::Spread);
    nearestInput.setTargetSpace(ImageViewport::CoordinateSpace::Item);
    nearestInput.setPoint(QPointF(-10.0, -10.0));
    QVERIFY(item.nearestVisiblePoint(nearestInput).isValid());

    ImageViewportCoordinateInput itemNearestInput;
    itemNearestInput.setSourceSpace(ImageViewport::CoordinateSpace::Item);
    itemNearestInput.setTargetSpace(ImageViewport::CoordinateSpace::Item);
    itemNearestInput.setPoint(QPointF(50.0, 50.0));
    const ImageViewportCoordinateResult itemNearest = item.nearestVisiblePoint(itemNearestInput);
    QVERIFY(itemNearest.isValid());
    QCOMPARE(itemNearest.sourceSpace(), ImageViewport::CoordinateSpace::Item);
    QCOMPARE(itemNearest.targetSpace(), ImageViewport::CoordinateSpace::Item);
    QVERIFY(item.contentRect().contains(itemNearest.point()));
    QVERIFY(qAbs(itemNearest.point().x() - 50.0) < 0.001);
    QVERIFY(qAbs(itemNearest.point().y() - 50.0) < 0.001);
}

void ImageViewportPublicApiCommandsTest::invalidPageSetTransitionPolicyPreservesState()
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
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);

    PageSetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(PageSetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const auto outcome = item.setPageSet(
        QVariant::fromValue<QObject*>(replacementResult->sequence()), {}, invalidPolicy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.sequence(), firstResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportPublicApiCommandsTest::invalidClearStyleTransitionPolicyPreservesState()
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
    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    PageSetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(PageSetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const auto outcome = item.setPageSet(QVariant(), QVariant(), invalidPolicy);

    QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("primarySequence").value<ImageSequence*>(), primaryResult->sequence());
    QCOMPARE(
        item.property("secondarySequence").value<ImageSequence*>(), secondaryResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
}

void ImageViewportPublicApiCommandsTest::invalidPresentationCommandsPreserveDiagnostics()
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
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewport::PageRole::Secondary),
        ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));

    const RevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const RevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const RevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    const auto spreadDirection = item.spreadDirection();
    const double pageGap = item.pageGap();
    const int commandReason = item.property("commandReason").toInt();
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    QCOMPARE(
        setSpreadDirectionCommand(item, spreadDirection), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(), commandReason);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(setPageGapCommand(item, pageGap), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(), commandReason);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(commandSpy.count(), 0);
    QCOMPARE(displayRevisionSpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);

    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.spreadDirection(), ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(item.property("commandReason").toInt(),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(displayRevisionSpy.count(), 1);
    QCOMPARE(presentationSpy.count(), 1);
}
QTEST_MAIN(ImageViewportPublicApiCommandsTest)

#include "tst_imageviewport_public_api_commands.moc"
