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
    void removedSequencePropertyWritesAreRejected();
    void presentationTargetAssignmentPreservesCommandDiagnostic();
    void commandResultsExposeSnapshotRevisionsAndReasons();
    void setPresentationTargetAcceptsPrimaryAndSecondaryAtomically();
    void cppTypedPresentationTargetOverloadsCompileAndReplaceSpread();
    void canonicalPresentationTargetValueDefaultsAndConstruction();
    void canonicalPresentationTargetOverloadMatchesPrimarySecondaryPath();
    void canonicalPresentationTargetRejectsSecondaryWithoutPrimary();
    void presentationTargetAssignmentUpdatesSnapshotGenerationIdentity();
    void qmlCanonicalPresentationTargetValueAssignsAndRejectsArbitraryVariants();
    void primaryOnlyPresentationTargetClearsSecondaryRole();
    void clearStylePresentationTargetWithSecondaryClearsAcceptedRoles();
    void clearStylePresentationTargetWithProviderSecondaryDoesNotStartProvider();
    void clearStylePresentationTargetPolicyPreservesPresentationPreferences();
    void invalidPresentationTargetSecondaryPreservesAcceptedRoles();
    void invalidPresentationTargetCommandsPreserveRevisionTokens();
    void roleCommandsWithInvalidRolePublishCommandDiagnostics();
    void secondaryRoleCommandsWithoutSecondaryPublishNoRequestDiagnostics();
    void presentationTargetTransitionClearBeforeLoadClearsRetainedDisplay();
    void presentationCommandAppliesAndRejectsTransactionally();
    void invalidPresentationTargetTransitionPolicyPreservesState();
    void invalidClearStyleTransitionPolicyPreservesState();
    void invalidPresentationCommandsPreserveDiagnostics();
};

static ImageViewport::CommandOutcome setSpreadDirectionCommand(
    ImageViewport& item, ImageViewport::SpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setPageGapCommand(ImageViewport& item, double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(percent);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setQualityTogglesCommand(
    ImageViewport& item, bool smoothing, bool mipmap)
{
    ImageViewportPresentationCommand command;
    command.setSmoothing(smoothing);
    command.setMipmap(mipmap);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setBackgroundCommand(
    ImageViewport& item, ImageViewport::BackgroundMode mode, QColor color)
{
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(mode);
    command.setBackgroundColor(color);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setLoopingCommand(ImageViewport& item, bool looping)
{
    ImageViewportPresentationCommand command;
    command.setLooping(looping);
    return item.setPresentation(command).outcome();
}

void ImageViewportPublicApiCommandsTest::removedSequencePropertyWritesAreRejected()
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
    item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    const QList<QVariant> unsupportedValues = {
        QVariant(QStringLiteral("image.png")),
        QVariant(QUrl(QStringLiteral("file:///tmp/image.png"))),
        QVariant(QByteArray("not image data")),
        QVariantMap { { QStringLiteral("url"), QStringLiteral("image.png") } },
        QVariant::fromValue<QObject*>(&adapter),
    };

    for (const QVariant& value : unsupportedValues) {
        QCOMPARE(item.setProperty("sequence", value), false);
        QCOMPARE(viewportPrimarySequence(item), result->sequence());
        QCOMPARE(requestStatusValue(item),
            enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(requestReasonValue(item),
            enumValue(metaObject, "RequestReason", "Ready"));
        QCOMPARE(displayStatusValue(item),
            enumValue(metaObject, "DisplayStatus", "Ready"));
        QCOMPARE(primaryRequestedFrame(item), 0);
        QCOMPARE(primaryDisplayedFrame(item), 0);
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    }

    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportPublicApiCommandsTest::presentationTargetAssignmentPreservesCommandDiagnostic()
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

    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    const ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(viewportPrimarySequence(item), result->sequence());
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
}

void ImageViewportPublicApiCommandsTest::commandResultsExposeSnapshotRevisionsAndReasons()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));

    auto verifyResult = [&](const ImageViewportCommandResult& commandResult,
                            ImageViewport::CommandOutcome outcome,
                            ImageViewport::CommandReason reason) {
        const ImageViewportStateSnapshot snapshot = item.state();
        QCOMPARE(commandResult.outcome(), outcome);
        QCOMPARE(commandResult.reason(), reason);
        QCOMPARE(commandResult.reason(), snapshot.diagnostics().commandReason());
        QCOMPARE(commandResult.commandRevision(), snapshot.revisions().command());
        QCOMPARE(commandResult.snapshotRevision(), snapshot.revisions().snapshot());
    };

    const ImageViewportCommandResult ignoredResult = item.play(ImageViewport::PageRole::Primary);
    verifyResult(ignoredResult, ImageViewport::CommandOutcome::IgnoredNoRequest,
        ImageViewport::CommandReason::IgnoredNoRequest);
    QVERIFY(ignoredResult.commandRevision().isValid());
    QVERIFY(ignoredResult.snapshotRevision().isValid());

    const ImageViewportRevisionToken ignoredCommandRevision = ignoredResult.commandRevision();
    const ImageViewportCommandResult acceptedResult = item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    verifyResult(acceptedResult, ImageViewport::CommandOutcome::Accepted,
        ImageViewport::CommandReason::IgnoredNoRequest);
    QCOMPARE(acceptedResult.commandRevision(), ignoredCommandRevision);
    QVERIFY(acceptedResult.snapshotRevision().isValid());

    ImageViewportPresentationTarget invalidPresentationTarget;
    invalidPresentationTarget.setSecondary(result->sequence());
    const ImageViewportCommandResult invalidResult
        = item.setPresentationTarget(invalidPresentationTarget, PresentationTargetTransitionPolicy {});
    verifyResult(invalidResult, ImageViewport::CommandOutcome::Invalid,
        ImageViewport::CommandReason::InvalidRequest);
    QVERIFY(invalidResult.commandRevision().isValid());
    QVERIFY(invalidResult.commandRevision() != acceptedResult.commandRevision());
    QVERIFY(invalidResult.snapshotRevision().isValid());
}

void ImageViewportPublicApiCommandsTest::setPresentationTargetAcceptsPrimaryAndSecondaryAtomically()
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
    ImageSequence* secondaryObservedDuringStateSignal = nullptr;
    connect(&item, &ImageViewport::stateChanged, &item, [&] {
        secondaryObservedDuringStateSignal = viewportSecondarySequence(item);
    });

    const auto outcome = item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(
        viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(secondaryObservedDuringStateSignal, secondaryResult->sequence());
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(secondaryFrameCount(item), 1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(secondaryFrameSeekBounds(item).maximum(), 0);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::True);
    QCOMPARE(secondaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::True);
    QCOMPARE(secondaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::False);
}

void ImageViewportPublicApiCommandsTest::cppTypedPresentationTargetOverloadsCompileAndReplaceSpread()
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
        = item.setPresentationTarget(
            ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()),
            PresentationTargetTransitionPolicy {});

    QCOMPARE(spreadOutcome.outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(secondaryFrameCount(item), 1);

    PresentationTargetTransitionPolicy policy;
    policy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(4.0);

    const auto replacementOutcome
        = item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()), policy);

    QCOMPARE(replacementOutcome.outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(item.state().presentation().pageGap(), 4.0);
}

void ImageViewportPublicApiCommandsTest::canonicalPresentationTargetValueDefaultsAndConstruction()
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

    const ImageViewportPresentationTarget defaultPresentationTarget;
    QVERIFY(defaultPresentationTarget.isClear());
    QVERIFY(defaultPresentationTarget.isValid());
    QCOMPARE(defaultPresentationTarget.primary(), nullptr);
    QCOMPARE(defaultPresentationTarget.secondary(), nullptr);
    QCOMPARE(defaultPresentationTarget, ImageViewportPresentationTarget::clear());

    const ImageViewportPresentationTarget primaryOnly(primaryResult->sequence());
    QVERIFY(!primaryOnly.isClear());
    QVERIFY(primaryOnly.isValid());
    QCOMPARE(primaryOnly.primary(), primaryResult->sequence());
    QCOMPARE(primaryOnly.secondary(), nullptr);

    const ImageViewportPresentationTarget spread(primaryResult->sequence(), secondaryResult->sequence());
    QVERIFY(spread.isValid());
    QCOMPARE(spread.primary(), primaryResult->sequence());
    QCOMPARE(spread.secondary(), secondaryResult->sequence());
    QVERIFY(spread != primaryOnly);

    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(secondaryResult->sequence());
    QVERIFY(!secondaryOnly.isClear());
    QVERIFY(!secondaryOnly.isValid());
}

void ImageViewportPublicApiCommandsTest::canonicalPresentationTargetOverloadMatchesPrimarySecondaryPath()
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
    const ImageViewportPresentationTarget spread(primaryResult->sequence(), secondaryResult->sequence());

    QCOMPARE(item.setPresentationTarget(spread, PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(item.state().primary().sequence(), primaryResult->sequence());
    QCOMPARE(item.state().secondary().sequence(), secondaryResult->sequence());
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(secondaryFrameCount(item), 1);

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget::clear(), policy).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(displayStatus(item), ImageViewport::DisplayStatus::Empty);
}

void ImageViewportPublicApiCommandsTest::canonicalPresentationTargetRejectsSecondaryWithoutPrimary()
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()), PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);

    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(secondaryResult->sequence());

    QCOMPARE(item.setPresentationTarget(secondaryOnly, PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != commandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewport::CommandReason::InvalidRequest);
}

void ImageViewportPublicApiCommandsTest::presentationTargetAssignmentUpdatesSnapshotGenerationIdentity()
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
    QCOMPARE(item.state().request().acceptedPresentationTargetGeneration().isValid(), false);
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(item.state().request().targetRoleSet(), ImageViewportRoleSet(false, false));

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()), PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot primarySnapshot = item.state();
    const ImageViewportPresentationTargetGenerationToken primaryGeneration
        = primarySnapshot.request().acceptedPresentationTargetGeneration();
    QVERIFY(primaryGeneration.isValid());
    QCOMPARE(primarySnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(primarySnapshot.request().targetRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(primarySnapshot.primary().present(), true);
    QCOMPARE(primarySnapshot.primary().sequence(), primaryResult->sequence());
    QCOMPARE(primarySnapshot.primary().request().presentationTargetGeneration(), primaryGeneration);
    QCOMPARE(primarySnapshot.secondary().present(), false);

    QCOMPARE(item.setPresentationTarget(
                 ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()),
                 PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot spreadSnapshot = item.state();
    const ImageViewportPresentationTargetGenerationToken spreadGeneration
        = spreadSnapshot.request().acceptedPresentationTargetGeneration();
    QVERIFY(spreadGeneration.isValid());
    QVERIFY(spreadGeneration != primaryGeneration);
    QCOMPARE(spreadSnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(spreadSnapshot.request().targetRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(spreadSnapshot.primary().request().presentationTargetGeneration(), spreadGeneration);
    QCOMPARE(spreadSnapshot.secondary().present(), true);
    QCOMPARE(spreadSnapshot.secondary().sequence(), secondaryResult->sequence());
    QCOMPARE(spreadSnapshot.secondary().request().presentationTargetGeneration(), spreadGeneration);

    QCOMPARE(
        item.setPresentationTarget(ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {}).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot clearSnapshot = item.state();
    QCOMPARE(clearSnapshot.request().acceptedPresentationTargetGeneration().isValid(), false);
    QCOMPARE(clearSnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(clearSnapshot.request().targetRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(clearSnapshot.primary().present(), false);
    QCOMPARE(clearSnapshot.secondary().present(), false);
}

void ImageViewportPublicApiCommandsTest::
    qmlCanonicalPresentationTargetValueAssignsAndRejectsArbitraryVariants()
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
    property imageViewportPresentationTarget presentationTarget
    property presentationTargetTransitionPolicy policy
    property bool typedAccepted: false
    property bool typedPolicyAccepted: false
    property bool stringRejected: false
    property bool objectRejected: false
    property bool providerRejected: false

    QtObject { id: rawObject }

    function invalidResult(value) {
        try {
            return setPresentationTarget(value, policy).outcome === ImageViewport.CommandOutcome.Invalid
        } catch (error) {
            return true
        }
    }

    Component.onCompleted: {
        presentationTarget.primary = suppliedPrimary
        presentationTarget.secondary = suppliedSecondary
        typedAccepted = setPresentationTarget(presentationTarget, policy).outcome === ImageViewport.CommandOutcome.Accepted
            && state.primary.sequence === suppliedPrimary
            && state.secondary.sequence === suppliedSecondary
            && state.request.acceptedRoleSet.primary
            && state.request.acceptedRoleSet.secondary
        typedPolicyAccepted = setPresentationTarget(presentationTarget, policy).outcome === ImageViewport.CommandOutcome.Accepted
            && state.primary.sequence === suppliedPrimary
            && state.secondary.sequence === suppliedSecondary
        const requestRevisionBefore = state.revisions.request
        const displayRevisionBefore = state.revisions.display
        stringRejected = invalidResult("image.png")
            && state.primary.sequence === suppliedPrimary
            && state.secondary.sequence === suppliedSecondary
            && state.revisions.request === requestRevisionBefore
            && state.revisions.display === displayRevisionBefore
        objectRejected = invalidResult({ primary: suppliedPrimary })
            && state.primary.sequence === suppliedPrimary
            && state.secondary.sequence === suppliedSecondary
            && state.revisions.request === requestRevisionBefore
            && state.revisions.display === displayRevisionBefore
        providerRejected = invalidResult(rawProvider)
            && state.primary.sequence === suppliedPrimary
            && state.secondary.sequence === suppliedSecondary
            && state.revisions.request === requestRevisionBefore
            && state.revisions.display === displayRevisionBefore
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

void ImageViewportPublicApiCommandsTest::primaryOnlyPresentationTargetClearsSecondaryRole()
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence(), secondResult->sequence()), PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()), PresentationTargetTransitionPolicy {});

    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(
        viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(secondaryFrameCount(item), -1);
}

void ImageViewportPublicApiCommandsTest::clearStylePresentationTargetWithSecondaryClearsAcceptedRoles()
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()), PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    const auto outcome
        = item.setPresentationTarget(ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item),
        enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
}

void ImageViewportPublicApiCommandsTest::
    clearStylePresentationTargetWithProviderSecondaryDoesNotStartProvider()
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
    item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()), PresentationTargetTransitionPolicy {});

    const auto outcome
        = item.setPresentationTarget(ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item),
        enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item),
        enumValue(item.metaObject(), "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleSpreadRect(item), QRectF());
    QCOMPARE(primaryPageRect(item), QRectF());
    QCOMPARE(secondaryPageRect(item), QRectF());
}

void ImageViewportPublicApiCommandsTest::clearStylePresentationTargetPolicyPreservesPresentationPreferences()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QCOMPARE(setManualZoomPercentCommand(item, 250.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 9.0), ImageViewport::CommandOutcome::Accepted);
    ImageViewportPresentationCommand rotationCommand;
    rotationCommand.setRotationDegrees(90);
    QCOMPARE(item.setPresentation(rotationCommand).outcome(), ImageViewport::CommandOutcome::Accepted);
    ImageViewportPresentationCommand mirrorCommand;
    mirrorCommand.setMirrorHorizontally(true);
    mirrorCommand.setMirrorVertically(true);
    QCOMPARE(item.setPresentation(mirrorCommand).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);

    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    const auto fitMode = presentation.fitMode();
    const double zoomPercent = presentation.zoomPercent();
    const QPointF preservedContentPosition = contentPosition(item);
    const auto spreadDirection = presentation.spreadDirection();
    const double pageGap = presentation.pageGap();
    const int rotationDegrees = presentation.rotationDegrees();
    const bool mirrorHorizontally = presentation.mirrorHorizontally();
    const bool mirrorVertically = presentation.mirrorVertically();
    const bool smoothing = presentation.smoothing();
    const bool mipmap = presentation.mipmap();
    const auto backgroundMode = presentation.backgroundMode();
    const QColor backgroundColor = presentation.backgroundColor();
    const bool looping = presentation.looping();

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    policy.setZoomTransition(PresentationTargetTransitionPolicy::ZoomTransition::ResetToContain);
    policy.setContentPositionTransition(
        PresentationTargetTransitionPolicy::ContentPositionTransition::ScanEnd);
    policy.setRotationTransition(PresentationTargetTransitionPolicy::RotationTransition::Reset);
    policy.setMirrorTransition(PresentationTargetTransitionPolicy::MirrorTransition::Reset);
    policy.setFitModeTransition(PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    policy.setFitMode(ImageViewport::FitMode::Contain);
    policy.setSpreadDirectionTransition(
        PresentationTargetTransitionPolicy::SpreadDirectionTransition::SetExplicit);
    policy.setSpreadDirection(ImageViewport::SpreadDirection::LeftToRight);
    policy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(0.0);
    policy.setReplacementIntent(PresentationTargetTransitionPolicy::ReplacementIntent::SameTargetRefinement);

    const auto outcome = item.setPresentationTarget(ImageViewportPresentationTarget::clear(), policy);

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item),
        enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item),
        enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    const ImageViewportPresentationSnapshot afterClearPresentation = item.state().presentation();
    QCOMPARE(afterClearPresentation.fitMode(), fitMode);
    QCOMPARE(afterClearPresentation.zoomPercent(), zoomPercent);
    QCOMPARE(contentPosition(item), preservedContentPosition);
    QCOMPARE(afterClearPresentation.spreadDirection(), spreadDirection);
    QCOMPARE(afterClearPresentation.pageGap(), pageGap);
    QCOMPARE(afterClearPresentation.rotationDegrees(), rotationDegrees);
    QCOMPARE(afterClearPresentation.mirrorHorizontally(), mirrorHorizontally);
    QCOMPARE(afterClearPresentation.mirrorVertically(), mirrorVertically);
    QCOMPARE(afterClearPresentation.smoothing(), smoothing);
    QCOMPARE(afterClearPresentation.mipmap(), mipmap);
    QCOMPARE(afterClearPresentation.backgroundMode(), backgroundMode);
    QCOMPARE(afterClearPresentation.backgroundColor(), backgroundColor);
    QCOMPARE(afterClearPresentation.looping(), looping);
}

void ImageViewportPublicApiCommandsTest::invalidPresentationTargetSecondaryPreservesAcceptedRoles()
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()), PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    ImageViewportPresentationTarget invalidPresentationTarget;
    invalidPresentationTarget.setSecondary(secondaryResult->sequence());
    const auto outcome = item.setPresentationTarget(invalidPresentationTarget, PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(
        viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
}

void ImageViewportPublicApiCommandsTest::invalidPresentationTargetCommandsPreserveRevisionTokens()
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()), PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);

    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    const int requestStatus = requestStatusValue(item);
    const int displayStatus = displayStatusValue(item);
    const int playbackPhase = playbackPhaseValue(item);
    const QMetaObject* metaObject = item.metaObject();
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(secondaryResult->sequence());
    const auto invalidSecondaryOnlyOutcome
        = item.setPresentationTarget(secondaryOnly, PresentationTargetTransitionPolicy {});

    QCOMPARE(invalidSecondaryOnlyOutcome.outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    commandRevision = revisionTokenProperty(item, "commandRevision");
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(requestStatusValue(item), requestStatus);
    QCOMPARE(displayStatusValue(item), displayStatus);
    QCOMPARE(playbackPhaseValue(item), playbackPhase);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(stateSpy.count(), 1);

    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);
    const auto invalidPolicyOutcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()), invalidPolicy);

    QCOMPARE(invalidPolicyOutcome.outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(requestStatusValue(item), requestStatus);
    QCOMPARE(displayStatusValue(item), displayStatus);
    QCOMPARE(playbackPhaseValue(item), playbackPhase);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(stateSpy.count(), 2);
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
    item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const auto invalidRole = static_cast<ImageViewport::PageRole>(
        999); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    const auto verifyInvalidCommand = [&](ImageViewport::CommandOutcome outcome) {
        QCOMPARE(outcome, ImageViewport::CommandOutcome::Invalid);
        QCOMPARE(commandReasonValue(item),
            enumValue(metaObject, "CommandReason", "InvalidRequest"));
        verifyRevisionChanged(item, "commandRevision", commandRevision);
        commandRevision = revisionTokenProperty(item, "commandRevision");
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
        QCOMPARE(requestStatusValue(item),
            enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(displayStatusValue(item),
            enumValue(metaObject, "DisplayStatus", "Ready"));
    };

    verifyInvalidCommand(item.play(invalidRole).outcome());
    verifyInvalidCommand(item.pause(invalidRole).outcome());
    verifyInvalidCommand(item.stop(invalidRole).outcome());
    verifyInvalidCommand(item.seek(invalidRole, 0).outcome());
    verifyInvalidCommand(item.seekToPosition(invalidRole, 0).outcome());
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
    item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    const auto verifyIgnoredCommand = [&](ImageViewport::CommandOutcome outcome) {
        QCOMPARE(outcome, ImageViewport::CommandOutcome::IgnoredNoRequest);
        QCOMPARE(commandReasonValue(item),
            enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
        verifyRevisionChanged(item, "commandRevision", commandRevision);
        commandRevision = revisionTokenProperty(item, "commandRevision");
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
        QCOMPARE(requestStatusValue(item),
            enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(displayStatusValue(item),
            enumValue(metaObject, "DisplayStatus", "Ready"));
    };

    verifyIgnoredCommand(item.play(ImageViewport::PageRole::Secondary).outcome());
    verifyIgnoredCommand(item.pause(ImageViewport::PageRole::Secondary).outcome());
    verifyIgnoredCommand(item.stop(ImageViewport::PageRole::Secondary).outcome());
    verifyIgnoredCommand(item.seek(ImageViewport::PageRole::Secondary, 0).outcome());
    verifyIgnoredCommand(item.seekToPosition(ImageViewport::PageRole::Secondary, 0).outcome());
}

void ImageViewportPublicApiCommandsTest::presentationTargetTransitionClearBeforeLoadClearsRetainedDisplay()
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
    item.setPresentationTarget(ImageViewportPresentationTarget(readyResult->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);

    const auto outcome
        = item.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()), policy);

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), loadingResult->sequence());
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item),
        enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
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
    item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
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

    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.state().presentation().zoomPercent(), 150.0);
    QCOMPARE(item.state().presentation().spreadDirection(),
        ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(item.state().presentation().pageGap(), 5.0);
    QCOMPARE(item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(10, 20, 30, 255));
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().looping(), true);
    QCOMPARE(item.state().presentation().qualityPreference(),
        ImageViewport::QualityPreference::ExactDetail);
    QCOMPARE(item.state().presentation().exactnessPreference(),
        ImageViewport::ExactnessPreference::RequireExact);

    QCOMPARE(setManualZoomPercentCommand(item, 1000.0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(maximumContentPosition(item).x() > 0.0 || maximumContentPosition(item).y() > 0.0);

    ImageViewportPresentationCommand scanCommand;
    scanCommand.setScanDirection(ImageViewport::ScanDirection::End);
    QCOMPARE(item.setPresentation(scanCommand).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentPosition(item), maximumContentPosition(item));

    scanCommand = {};
    scanCommand.setScanDirection(ImageViewport::ScanDirection::Start);
    QCOMPARE(item.setPresentation(scanCommand).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(contentPosition(item), QPointF());

    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);
    const ImageViewportPresentationSnapshot preservedPresentation = item.state().presentation();

    ImageViewportPresentationCommand invalidCommand;
    invalidCommand.setManualZoomPercent(125.0);
    invalidCommand.setScanDirection(ImageViewport::ScanDirection::Next);
    invalidCommand.setPageGap(12.0);

    QCOMPARE(item.setPresentation(invalidCommand).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != commandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewport::CommandReason::InvalidRequest);

    const ImageViewportRevisionToken invalidCommandRevision = viewportCommandRevision(item);
    ImageViewportPresentationCommand invalidDirectionCommand;
    const auto invalidDirection = static_cast<ImageViewport::SpreadDirection>(
        999); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    invalidDirectionCommand.setSpreadDirection(invalidDirection);
    QCOMPARE(item.setPresentation(invalidDirectionCommand).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != invalidCommandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewport::CommandReason::InvalidRequest);

    ImageViewportRevisionToken previousInvalidCommandRevision = viewportCommandRevision(item);
    ImageViewportPresentationCommand invalidPageGapCommand;
    invalidPageGapCommand.setPageGap(-1.0);
    QCOMPARE(item.setPresentation(invalidPageGapCommand).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != previousInvalidCommandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewport::CommandReason::InvalidRequest);

    previousInvalidCommandRevision = viewportCommandRevision(item);
    invalidPageGapCommand = {};
    invalidPageGapCommand.setPageGap(std::numeric_limits<double>::infinity());
    QCOMPARE(item.setPresentation(invalidPageGapCommand).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != previousInvalidCommandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewport::CommandReason::InvalidRequest);

    ImageViewportPresentationCommand transformCommand;
    transformCommand.setRotationDegrees(90);
    transformCommand.setMirrorHorizontally(true);
    transformCommand.setMirrorVertically(true);
    QCOMPARE(
        item.setPresentation(transformCommand).outcome(), ImageViewport::CommandOutcome::Accepted);

    ImageViewportPresentationCommand resetCommand
        = ImageViewportPresentationCommand::resetViewCommand();
    resetCommand.setBackgroundMode(ImageViewport::BackgroundMode::Checkerboard);

    QCOMPARE(item.setPresentation(resetCommand).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().rotationDegrees(), 0);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), false);
    QCOMPARE(item.state().presentation().mirrorVertically(), false);
    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::Checkerboard);

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
    QVERIFY(contentRect(item).contains(itemNearest.point()));
    QVERIFY(qAbs(itemNearest.point().x() - 50.0) < 0.001);
    QVERIFY(qAbs(itemNearest.point().y() - 50.0) < 0.001);
}

void ImageViewportPublicApiCommandsTest::invalidPresentationTargetTransitionPolicyPreservesState()
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
    item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const auto outcome = item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()), invalidPolicy);

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), firstResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(
        requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(
        displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()), PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");

    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const auto outcome = item.setPresentationTarget(ImageViewportPresentationTarget::clear(), invalidPolicy);

    QCOMPARE(outcome.outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(
        viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(commandReasonValue(item),
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
    item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewport::PageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));

    const ImageViewportRevisionToken requestRevision = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");
    const auto spreadDirection = item.state().presentation().spreadDirection();
    const double pageGap = item.state().presentation().pageGap();
    const int commandReason = commandReasonValue(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(
        setSpreadDirectionCommand(item, spreadDirection), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), commandReason);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(setPageGapCommand(item, pageGap), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), commandReason);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(stateSpy.count(), 0);

    QCOMPARE(setSpreadDirectionCommand(item, ImageViewport::SpreadDirection::RightToLeft),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().spreadDirection(),
        ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(commandReasonValue(item),
        enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(stateSpy.count(), 1);
}
QTEST_MAIN(ImageViewportPublicApiCommandsTest)

#include "tst_imageviewport_public_api_commands.moc"
