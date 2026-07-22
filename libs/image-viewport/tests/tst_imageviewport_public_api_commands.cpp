// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

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

private Q_SLOTS:
    void presentationTargetAssignmentAdvancesCommandDiagnostic();
    void commandResultsExposeSnapshotRevisionsAndReasons();
    void reentrantStateChangedKeepsCommandResultsScopedToTransaction();
    void setPresentationTargetAcceptsPrimaryAndSecondaryAtomically();
    void cppTypedPresentationTargetOverloadsCompileAndReplaceSpread();
    void canonicalPresentationTargetValueDefaultsAndConstruction();
    void transitionPolicyMatchesDocumentedSurfaceAndCanonicalClear();
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
    void sameTargetRefinementPreservesSelectionAndRejectsIncompatibleTiming();
    void unresolvedTargetAnchorResolvesWhenProviderGeometryArrives();
    void invalidPresentationCommandsPreserveDiagnostics();
};

void ImageViewportPublicApiCommandsTest::transitionPolicyMatchesDocumentedSurfaceAndCanonicalClear()
{
    const PresentationTargetTransitionPolicy clearPolicy
        = PresentationTargetTransitionPolicy::defaultClear();
    QCOMPARE(clearPolicy.displayTransition(),
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    QCOMPARE(clearPolicy.failureTransition(),
        PresentationTargetTransitionPolicy::FailureTransition::KeepFailedTarget);
    QCOMPARE(
        clearPolicy.zoomTransition(), PresentationTargetTransitionPolicy::ZoomTransition::Preserve);
    QCOMPARE(clearPolicy.contentPositionTransition(),
        PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp);
    QCOMPARE(clearPolicy.rotationTransition(),
        PresentationTargetTransitionPolicy::RotationTransition::Preserve);
    QCOMPARE(clearPolicy.mirrorTransition(),
        PresentationTargetTransitionPolicy::MirrorTransition::Preserve);
    QCOMPARE(clearPolicy.fitModeTransition(),
        PresentationTargetTransitionPolicy::FitModeTransition::Preserve);
    QCOMPARE(clearPolicy.spreadDirectionTransition(),
        PresentationTargetTransitionPolicy::SpreadDirectionTransition::Preserve);
    QCOMPARE(clearPolicy.pageGapTransition(),
        PresentationTargetTransitionPolicy::PageGapTransition::Preserve);
    QCOMPARE(clearPolicy.replacementIntent(),
        PresentationTargetTransitionPolicy::ReplacementIntent::NewTarget);
    QVERIFY(clearPolicy.isValid());

    PresentationTargetTransitionPolicy ignoredMalformedValue;
    ignoredMalformedValue.setPageGap(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(!ignoredMalformedValue.isValid());

    PresentationTargetTransitionPolicy excessivePageGap;
    excessivePageGap.setPageGap(ImageViewportDisplayLimits::maximumPageGap() + 1.0);
    QVERIFY(!excessivePageGap.isValid());

    PresentationTargetTransitionPolicy conflictingFit;
    conflictingFit.setZoomTransition(
        PresentationTargetTransitionPolicy::ZoomTransition::ResetToContain);
    conflictingFit.setFitModeTransition(
        PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    conflictingFit.setFitMode(ImageViewportFitMode::Contain);
    QVERIFY(!conflictingFit.isValid());

    PresentationTargetTransitionPolicy anchorPolicy;
    anchorPolicy.setContentPositionTransition(
        PresentationTargetTransitionPolicy::ContentPositionTransition::AnchorStart);
    QVERIFY(anchorPolicy.isValid());
    anchorPolicy.setContentPositionTransition(
        PresentationTargetTransitionPolicy::ContentPositionTransition::AnchorEnd);
    QVERIFY(anchorPolicy.isValid());
}

static ImageViewportCommandOutcome setSpreadDirectionCommand(
    ImageViewport& item, ImageViewportSpreadDirection direction)
{
    ImageViewportPresentationCommand command;
    command.setSpreadDirection(direction);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setPageGapCommand(ImageViewport& item, double gap)
{
    ImageViewportPresentationCommand command;
    command.setPageGap(gap);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setPreferredManualZoomPercentCommand(
    ImageViewport& item, double percent)
{
    ImageViewportPresentationCommand command;
    command.setPreferredManualZoomPercent(percent);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setQualityTogglesCommand(
    ImageViewport& item, bool smoothing, bool mipmap)
{
    ImageViewportPresentationCommand command;
    command.setSmoothing(smoothing);
    command.setMipmap(mipmap);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setBackgroundCommand(
    ImageViewport& item, ImageViewportBackgroundMode mode, QColor color)
{
    ImageViewportPresentationCommand command;
    command.setBackgroundMode(mode);
    command.setBackgroundColor(color);
    return item.setPresentation(command).outcome();
}

static ImageViewportCommandOutcome setLoopingCommand(ImageViewport& item, bool looping)
{
    ImageViewportPresentationCommand command;
    command.setLooping(looping);
    return item.setPresentation(command).outcome();
}

void ImageViewportPublicApiCommandsTest::presentationTargetAssignmentAdvancesCommandDiagnostic()
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

    QCOMPARE(item.play(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");

    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(viewportPrimarySequence(item), result->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
}

void ImageViewportPublicApiCommandsTest::
    reentrantStateChangedKeepsCommandResultsScopedToTransaction()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    ImageViewportRevisionToken assignmentSnapshotRevision;
    ImageViewportCommandResult reentrantResult;
    bool reentered = false;
    connect(&item, &ImageViewport::stateChanged, &item, [&] {
        if (reentered || viewportPrimarySequence(item) != result->sequence()) {
            return;
        }
        reentered = true;
        assignmentSnapshotRevision = item.state().revisions().snapshot();
        ImageViewportPresentationCommand command;
        command.setSmoothing(false);
        reentrantResult = item.setPresentation(command);
    });

    const ImageViewportCommandResult assignmentResult = item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});

    QVERIFY(reentered);
    QCOMPARE(assignmentResult.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(assignmentResult.snapshotRevision(), assignmentSnapshotRevision);
    QCOMPARE(reentrantResult.outcome(), ImageViewportCommandOutcome::Accepted);
    QVERIFY(reentrantResult.snapshotRevision() != assignmentSnapshotRevision);
    QCOMPARE(item.state().revisions().snapshot(), reentrantResult.snapshotRevision());
    QCOMPARE(item.state().presentation().smoothing(), false);
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

    auto verifyResult
        = [&](const ImageViewportCommandResult& commandResult, ImageViewportCommandOutcome outcome,
              ImageViewportCommandReason reason) {
              const ImageViewportStateSnapshot snapshot = item.state();
              QCOMPARE(commandResult.outcome(), outcome);
              QCOMPARE(commandResult.reason(), reason);
              QCOMPARE(commandResult.reason(), snapshot.diagnostics().commandReason());
              QCOMPARE(commandResult.commandRevision(), snapshot.revisions().command());
              QCOMPARE(commandResult.snapshotRevision(), snapshot.revisions().snapshot());
          };

    const ImageViewportCommandResult ignoredResult = item.play(ImageViewportPageRole::Primary);
    verifyResult(ignoredResult, ImageViewportCommandOutcome::IgnoredNoRequest,
        ImageViewportCommandReason::IgnoredNoRequest);
    QVERIFY(ignoredResult.commandRevision().isValid());
    QVERIFY(ignoredResult.snapshotRevision().isValid());

    const ImageViewportRevisionToken ignoredCommandRevision = ignoredResult.commandRevision();
    const ImageViewportCommandResult acceptedResult = item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    verifyResult(acceptedResult, ImageViewportCommandOutcome::Accepted,
        ImageViewportCommandReason::NoCommand);
    QVERIFY(acceptedResult.commandRevision() != ignoredCommandRevision);
    QVERIFY(acceptedResult.snapshotRevision().isValid());

    ImageViewportPresentationTarget invalidPresentationTarget;
    invalidPresentationTarget.setSecondary(result->sequence());
    const ImageViewportCommandResult invalidResult = item.setPresentationTarget(
        invalidPresentationTarget, PresentationTargetTransitionPolicy {});
    verifyResult(invalidResult, ImageViewportCommandOutcome::Invalid,
        ImageViewportCommandReason::InvalidRequest);
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
    connect(&item, &ImageViewport::stateChanged, &item,
        [&] { secondaryObservedDuringStateSignal = viewportSecondarySequence(item); });

    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(secondaryObservedDuringStateSignal, secondaryResult->sequence());
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(secondaryFrameCount(item), 1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(secondaryFrameSeekBounds(item).maximum(), 0);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(secondaryFrameSeekSupport(item), ImageViewportCapabilitySupport::True);
    QCOMPARE(secondaryTimedPlaybackSupport(item), ImageViewportCapabilitySupport::False);
}

void ImageViewportPublicApiCommandsTest::
    cppTypedPresentationTargetOverloadsCompileAndReplaceSpread()
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

    const auto spreadOutcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(spreadOutcome.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(secondaryFrameCount(item), 1);

    PresentationTargetTransitionPolicy policy;
    policy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(4.0);

    const auto replacementOutcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(replacementResult->sequence()), policy);

    QCOMPARE(replacementOutcome.outcome(), ImageViewportCommandOutcome::Accepted);
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

    const ImageViewportPresentationTarget spread(
        primaryResult->sequence(), secondaryResult->sequence());
    QVERIFY(spread.isValid());
    QCOMPARE(spread.primary(), primaryResult->sequence());
    QCOMPARE(spread.secondary(), secondaryResult->sequence());
    QVERIFY(spread != primaryOnly);

    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(secondaryResult->sequence());
    QVERIFY(!secondaryOnly.isClear());
    QVERIFY(!secondaryOnly.isValid());
}

void ImageViewportPublicApiCommandsTest::
    canonicalPresentationTargetOverloadMatchesPrimarySecondaryPath()
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
    const ImageViewportPresentationTarget spread(
        primaryResult->sequence(), secondaryResult->sequence());

    QCOMPARE(item.setPresentationTarget(spread, PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(item.state().primary().sequence(), primaryResult->sequence());
    QCOMPARE(item.state().secondary().sequence(), secondaryResult->sequence());
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(secondaryFrameCount(item), 1);

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget::clear(), policy).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(item.state().request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(displayStatus(item), ImageViewportDisplayStatus::Empty);
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);

    ImageViewportPresentationTarget secondaryOnly;
    secondaryOnly.setSecondary(secondaryResult->sequence());

    QCOMPARE(
        item.setPresentationTarget(secondaryOnly, PresentationTargetTransitionPolicy {}).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != commandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewportCommandReason::InvalidRequest);
}

void ImageViewportPublicApiCommandsTest::
    presentationTargetAssignmentUpdatesSnapshotGenerationIdentity()
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

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ImageViewportStateSnapshot primarySnapshot = item.state();
    const ImageViewportPresentationTargetGenerationToken primaryGeneration
        = primarySnapshot.request().acceptedPresentationTargetGeneration();
    QVERIFY(primaryGeneration.isValid());
    QCOMPARE(primarySnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(primarySnapshot.primary().present(), true);
    QCOMPARE(primarySnapshot.primary().sequence(), primaryResult->sequence());
    QCOMPARE(primarySnapshot.primary().request().presentationTargetGeneration(), primaryGeneration);
    QCOMPARE(primarySnapshot.secondary().present(), false);

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ImageViewportStateSnapshot spreadSnapshot = item.state();
    const ImageViewportPresentationTargetGenerationToken spreadGeneration
        = spreadSnapshot.request().acceptedPresentationTargetGeneration();
    QVERIFY(spreadGeneration.isValid());
    QVERIFY(spreadGeneration != primaryGeneration);
    QCOMPARE(spreadSnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(spreadSnapshot.primary().request().presentationTargetGeneration(), spreadGeneration);
    QCOMPARE(spreadSnapshot.secondary().present(), true);
    QCOMPARE(spreadSnapshot.secondary().sequence(), secondaryResult->sequence());
    QCOMPARE(spreadSnapshot.secondary().request().presentationTargetGeneration(), spreadGeneration);

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget::clear(),
                     PresentationTargetTransitionPolicy::defaultClear())
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ImageViewportStateSnapshot clearSnapshot = item.state();
    QCOMPARE(clearSnapshot.request().acceptedPresentationTargetGeneration().isValid(), false);
    QCOMPARE(clearSnapshot.request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
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
import ImageViewportTest

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
    property bool canonicalClearAccepted: false

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
        canonicalClearAccepted = setPresentationTarget(
            presentationTarget.clear(),
            policy.defaultClear()).outcome === ImageViewport.CommandOutcome.Accepted
            && !state.request.acceptedRoleSet.primary
            && !state.request.acceptedRoleSet.secondary
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
    QCOMPARE(object->property("canonicalClearAccepted").toBool(), true);
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            firstResult->sequence(), secondResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(viewportPrimarySequence(item), replacementResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(secondaryFrameCount(item), -1);
}

void ImageViewportPublicApiCommandsTest::
    clearStylePresentationTargetWithSecondaryClearsAcceptedRoles()
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    const auto outcome = item.setPresentationTarget(ImageViewportPresentationTarget::clear(),
        PresentationTargetTransitionPolicy::defaultClear());

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(item.metaObject(), "DisplayStatus", "Empty"));
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
    item.setPresentationTarget(ImageViewportPresentationTarget(primaryResult->sequence()),
        PresentationTargetTransitionPolicy {});

    const auto outcome = item.setPresentationTarget(ImageViewportPresentationTarget::clear(),
        PresentationTargetTransitionPolicy::defaultClear());

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(viewportSecondarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(item.metaObject(), "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleSpreadRect(item), QRectF());
    QCOMPARE(primaryPageRect(item), QRectF());
    QCOMPARE(secondaryPageRect(item), QRectF());
}

void ImageViewportPublicApiCommandsTest::
    clearStylePresentationTargetPolicyPreservesPresentationPreferences()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QCOMPARE(
        setPreferredManualZoomPercentCommand(item, 250.0), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setSpreadDirectionCommand(item, ImageViewportSpreadDirection::RightToLeft),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setPageGapCommand(item, 9.0), ImageViewportCommandOutcome::Accepted);
    ImageViewportPresentationCommand rotationCommand;
    rotationCommand.setRotationDegrees(90);
    QCOMPARE(
        item.setPresentation(rotationCommand).outcome(), ImageViewportCommandOutcome::Accepted);
    ImageViewportPresentationCommand mirrorCommand;
    mirrorCommand.setMirrorHorizontally(true);
    mirrorCommand.setMirrorVertically(true);
    QCOMPARE(item.setPresentation(mirrorCommand).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewportBackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewportCommandOutcome::Accepted);

    const ImageViewportPresentationSnapshot presentation = item.state().presentation();
    const auto fitMode = presentation.fitMode();
    const double preferredManualZoomPercent = presentation.preferredManualZoomPercent();
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

    const PresentationTargetTransitionPolicy policy
        = PresentationTargetTransitionPolicy::defaultClear();

    const auto outcome
        = item.setPresentationTarget(ImageViewportPresentationTarget::clear(), policy);

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(requestStatusValue(item), enumValue(item.metaObject(), "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(item.metaObject(), "DisplayStatus", "Empty"));
    const ImageViewportPresentationSnapshot afterClearPresentation = item.state().presentation();
    QCOMPARE(afterClearPresentation.fitMode(), fitMode);
    QCOMPARE(afterClearPresentation.zoomPercent(), 0.0);
    QCOMPARE(afterClearPresentation.preferredManualZoomPercent(), preferredManualZoomPercent);
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    ImageViewportPresentationTarget invalidPresentationTarget;
    invalidPresentationTarget.setSecondary(secondaryResult->sequence());
    const auto outcome = item.setPresentationTarget(
        invalidPresentationTarget, PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
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

    QCOMPARE(invalidSecondaryOnlyOutcome.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
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
    invalidPolicy.setPageGapTransition(
        PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);
    const auto invalidPolicyOutcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(primaryResult->sequence(), secondaryResult->sequence()),
        invalidPolicy);

    QCOMPARE(invalidPolicyOutcome.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const auto invalidRole = static_cast<ImageViewportPageRole>(
        999); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    const auto verifyInvalidCommand = [&](ImageViewportCommandOutcome outcome) {
        QCOMPARE(outcome, ImageViewportCommandOutcome::Invalid);
        QCOMPARE(
            commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
        verifyRevisionChanged(item, "commandRevision", commandRevision);
        commandRevision = revisionTokenProperty(item, "commandRevision");
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
        QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    ImageViewportRevisionToken commandRevision = revisionTokenProperty(item, "commandRevision");

    const auto verifyIgnoredCommand = [&](ImageViewportCommandOutcome outcome) {
        QCOMPARE(outcome, ImageViewportCommandOutcome::IgnoredNoRequest);
        QCOMPARE(
            commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
        verifyRevisionChanged(item, "commandRevision", commandRevision);
        commandRevision = revisionTokenProperty(item, "commandRevision");
        QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
        QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
        QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    };

    verifyIgnoredCommand(item.play(ImageViewportPageRole::Secondary).outcome());
    verifyIgnoredCommand(item.pause(ImageViewportPageRole::Secondary).outcome());
    verifyIgnoredCommand(item.stop(ImageViewportPageRole::Secondary).outcome());
    verifyIgnoredCommand(item.seek(ImageViewportPageRole::Secondary, 0).outcome());
    verifyIgnoredCommand(item.seekToPosition(ImageViewportPageRole::Secondary, 0).outcome());
}

void ImageViewportPublicApiCommandsTest::
    presentationTargetTransitionClearBeforeLoadClearsRetainedDisplay()
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
    item.setPresentationTarget(ImageViewportPresentationTarget(readyResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);

    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(loadingResult->sequence()), policy);

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(viewportPrimarySequence(item), loadingResult->sequence());
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    const ImageViewportStateSnapshot beforeEmptyCommand = item.state();
    QSignalSpy emptyCommandStateSpy(&item, &ImageViewport::stateChanged);
    const ImageViewportCommandResult emptyCommandResult
        = item.setPresentation(ImageViewportPresentationCommand {});
    QCOMPARE(emptyCommandResult.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().presentation(), beforeEmptyCommand.presentation());
    QCOMPARE(item.state().request(), beforeEmptyCommand.request());
    QCOMPARE(item.state().display(), beforeEmptyCommand.display());
    QCOMPARE(emptyCommandStateSpy.count(), 1);

    ImageViewportPresentationCommand command;
    command.setFitMode(ImageViewportFitMode::Manual);
    command.setPreferredManualZoomPercent(150.0);
    command.setSpreadDirection(ImageViewportSpreadDirection::RightToLeft);
    command.setPageGap(5.0);
    command.setBackgroundMode(ImageViewportBackgroundMode::SolidColor);
    command.setBackgroundColor(QColor(10, 20, 30, 255));
    command.setCheckerboardLightColor(QColor(240, 240, 240, 255));
    command.setCheckerboardDarkColor(QColor(80, 80, 80, 255));
    command.setCheckerboardCellSize(16.0);
    command.setSmoothing(false);
    command.setMipmap(true);
    command.setLooping(true);
    command.setQualityPreference(ImageViewportQualityPreference::ExactDetail);
    command.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);

    QSignalSpy acceptedCommandStateSpy(&item, &ImageViewport::stateChanged);
    QList<ImageViewportStateSnapshot> observedAcceptedSnapshots;
    const QMetaObject::Connection acceptedStateConnection
        = connect(&item, &ImageViewport::stateChanged, &item,
            [&] { observedAcceptedSnapshots.append(item.state()); });
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);
    disconnect(acceptedStateConnection);
    QCOMPARE(acceptedCommandStateSpy.count(), 1);
    QCOMPARE(observedAcceptedSnapshots.size(), 1);
    QCOMPARE(observedAcceptedSnapshots.constFirst(), item.state());
    QCOMPARE(item.state().diagnostics().commandReason(), ImageViewportCommandReason::NoCommand);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Manual);
    QCOMPARE(item.state().presentation().zoomPercent(), 150.0);
    QCOMPARE(
        item.state().presentation().spreadDirection(), ImageViewportSpreadDirection::RightToLeft);
    QCOMPARE(item.state().presentation().pageGap(), 5.0);
    QCOMPARE(item.state().presentation().backgroundMode(), ImageViewportBackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(10, 20, 30, 255));
    QCOMPARE(item.state().presentation().checkerboardLightColor(), QColor(240, 240, 240, 255));
    QCOMPARE(item.state().presentation().checkerboardDarkColor(), QColor(80, 80, 80, 255));
    QCOMPARE(item.state().presentation().checkerboardCellSize(), 16.0);
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().looping(), true);
    QCOMPARE(item.state().presentation().qualityPreference(),
        ImageViewportQualityPreference::ExactDetail);
    QCOMPARE(item.state().presentation().exactnessPreference(),
        ImageViewportExactnessPreference::RequireExact);

    QCOMPARE(
        setPreferredManualZoomPercentCommand(item, 1000.0), ImageViewportCommandOutcome::Accepted);
    QVERIFY(maximumContentPosition(item).x() > 0.0 || maximumContentPosition(item).y() > 0.0);

    ImageViewportPresentationCommand anchorCommand;
    anchorCommand.setContentAnchor(ImageViewportContentAnchor::End);
    QCOMPARE(item.setPresentation(anchorCommand).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentPosition(item), QPointF(0.0, maximumContentPosition(item).y()));

    anchorCommand = {};
    anchorCommand.setContentAnchor(ImageViewportContentAnchor::Start);
    QCOMPARE(item.setPresentation(anchorCommand).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(contentPosition(item), QPointF(maximumContentPosition(item).x(), 0.0));

    const ImageViewportRevisionToken requestRevision = viewportRequestRevision(item);
    const ImageViewportRevisionToken displayRevision = viewportDisplayRevision(item);
    const ImageViewportRevisionToken commandRevision = viewportCommandRevision(item);
    const ImageViewportPresentationSnapshot preservedPresentation = item.state().presentation();

    ImageViewportPresentationCommand invalidCommand;
    invalidCommand.setPreferredManualZoomPercent(125.0);
    invalidCommand.setContentAnchor(static_cast<ImageViewportContentAnchor>(-1));
    invalidCommand.setPageGap(12.0);

    QCOMPARE(item.setPresentation(invalidCommand).outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(
        item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != commandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewportCommandReason::InvalidRequest);

    const ImageViewportRevisionToken invalidCommandRevision = viewportCommandRevision(item);
    ImageViewportPresentationCommand invalidDirectionCommand;
    const auto invalidDirection = static_cast<ImageViewportSpreadDirection>(
        999); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    invalidDirectionCommand.setSpreadDirection(invalidDirection);
    invalidDirectionCommand.setBackgroundColor(Qt::magenta);
    QCOMPARE(item.setPresentation(invalidDirectionCommand).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().presentation(), preservedPresentation);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(
        item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != invalidCommandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewportCommandReason::InvalidRequest);

    ImageViewportRevisionToken previousInvalidCommandRevision = viewportCommandRevision(item);
    ImageViewportPresentationCommand invalidPageGapCommand;
    invalidPageGapCommand.setPageGap(-1.0);
    QCOMPARE(item.setPresentation(invalidPageGapCommand).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(
        item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != previousInvalidCommandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewportCommandReason::InvalidRequest);

    previousInvalidCommandRevision = viewportCommandRevision(item);
    invalidPageGapCommand = {};
    invalidPageGapCommand.setPageGap(std::numeric_limits<double>::infinity());
    QCOMPARE(item.setPresentation(invalidPageGapCommand).outcome(),
        ImageViewportCommandOutcome::Invalid);
    QCOMPARE(item.state().presentation().zoomPercent(), preservedPresentation.zoomPercent());
    QCOMPARE(item.state().presentation().pageGap(), preservedPresentation.pageGap());
    QCOMPARE(
        item.state().presentation().spreadDirection(), preservedPresentation.spreadDirection());
    QCOMPARE(viewportRequestRevision(item), requestRevision);
    QCOMPARE(viewportDisplayRevision(item), displayRevision);
    QVERIFY(viewportCommandRevision(item) != previousInvalidCommandRevision);
    QCOMPARE(viewportCommandReason(item), ImageViewportCommandReason::InvalidRequest);

    QList<ImageViewportPresentationCommand> invalidBackingCommands;
    ImageViewportPresentationCommand invalidLightColor;
    invalidLightColor.setCheckerboardLightColor(QColor {});
    invalidLightColor.setBackgroundMode(ImageViewportBackgroundMode::Checkerboard);
    invalidBackingCommands.append(invalidLightColor);
    ImageViewportPresentationCommand invalidDarkColor;
    invalidDarkColor.setCheckerboardDarkColor(QColor {});
    invalidDarkColor.setSmoothing(!preservedPresentation.smoothing());
    invalidBackingCommands.append(invalidDarkColor);
    ImageViewportPresentationCommand undersizedCheckerCell;
    undersizedCheckerCell.setCheckerboardCellSize(
        ImageViewportDisplayLimits::minimumCheckerboardCellSize() - 1.0);
    undersizedCheckerCell.setMipmap(!preservedPresentation.mipmap());
    invalidBackingCommands.append(undersizedCheckerCell);
    ImageViewportPresentationCommand oversizedCheckerCell;
    oversizedCheckerCell.setCheckerboardCellSize(
        ImageViewportDisplayLimits::maximumCheckerboardCellSize() + 1.0);
    oversizedCheckerCell.setBackgroundColor(Qt::green);
    invalidBackingCommands.append(oversizedCheckerCell);
    ImageViewportPresentationCommand oversizedPageGap;
    oversizedPageGap.setPageGap(ImageViewportDisplayLimits::maximumPageGap() + 1.0);
    oversizedPageGap.setBackgroundColor(Qt::cyan);
    invalidBackingCommands.append(oversizedPageGap);

    for (const ImageViewportPresentationCommand& invalidBackingCommand : invalidBackingCommands) {
        previousInvalidCommandRevision = viewportCommandRevision(item);
        QCOMPARE(item.setPresentation(invalidBackingCommand).outcome(),
            ImageViewportCommandOutcome::Invalid);
        QCOMPARE(item.state().presentation(), preservedPresentation);
        QCOMPARE(viewportRequestRevision(item), requestRevision);
        QCOMPARE(viewportDisplayRevision(item), displayRevision);
        QVERIFY(viewportCommandRevision(item) != previousInvalidCommandRevision);
        QCOMPARE(viewportCommandReason(item), ImageViewportCommandReason::InvalidRequest);
    }

    ImageViewportPresentationCommand transformCommand;
    transformCommand.setRotationDegrees(90);
    transformCommand.setMirrorHorizontally(true);
    transformCommand.setMirrorVertically(true);
    QCOMPARE(
        item.setPresentation(transformCommand).outcome(), ImageViewportCommandOutcome::Accepted);

    ImageViewportPresentationCommand resetCommand
        = ImageViewportPresentationCommand::resetViewCommand();
    resetCommand.setBackgroundMode(ImageViewportBackgroundMode::Checkerboard);

    QCOMPARE(item.setPresentation(resetCommand).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Contain);
    QCOMPARE(item.state().presentation().rotationDegrees(), 0);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), false);
    QCOMPARE(item.state().presentation().mirrorVertically(), false);
    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewportBackgroundMode::Checkerboard);

    ImageViewportCoordinateInput mapInput;
    mapInput.setSourceSpace(ImageViewportCoordinateSpace::Item);
    mapInput.setTargetSpace(ImageViewportCoordinateSpace::DisplayedSpread);
    mapInput.setPoint(QPointF(50.0, 50.0));
    const ImageViewportCoordinateResult mapped = item.mapPoint(mapInput);
    QVERIFY(mapped.isValid());
    QCOMPARE(mapped.space(), ImageViewportCoordinateSpace::DisplayedSpread);
    QVERIFY(mapped.role().isNull());
    QCOMPARE(item.containsPoint(mapInput), true);

    ImageViewportCoordinateInput pageInput = mapInput;
    pageInput.setTargetSpace(ImageViewportCoordinateSpace::DisplayedPage);
    pageInput.setRole(QVariant::fromValue(ImageViewportPageRole::Primary));
    const ImageViewportCoordinateResult pageResult = item.mapPoint(pageInput);
    QVERIFY(pageResult.isValid());
    QCOMPARE(pageResult.space(), ImageViewportCoordinateSpace::DisplayedPage);
    QCOMPARE(pageResult.role().value<ImageViewportPageRole>(), ImageViewportPageRole::Primary);

    ImageViewportCoordinateInput rolelessPageInput = pageInput;
    rolelessPageInput.setRole({});
    QVERIFY(!item.mapPoint(rolelessPageInput).isValid());
    QCOMPARE(item.containsPoint(rolelessPageInput), false);

    ImageViewportCoordinateInput unnecessaryRoleInput = mapInput;
    unnecessaryRoleInput.setRole(QVariant::fromValue(ImageViewportPageRole::Primary));
    QVERIFY(!item.mapPoint(unnecessaryRoleInput).isValid());
    QCOMPARE(item.containsPoint(unnecessaryRoleInput), false);
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
    item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    PresentationTargetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(
        PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget(replacementResult->sequence()), invalidPolicy);

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), firstResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
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
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    const auto outcome = item.setPresentationTarget(
        ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});

    QCOMPARE(outcome.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), primaryResult->sequence());
    QCOMPARE(viewportSecondarySequence(item), secondaryResult->sequence());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(
        commandReasonValue(item), enumValue(item.metaObject(), "CommandReason", "InvalidRequest"));
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewportPageRole::Secondary).outcome(),
        ImageViewportCommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));

    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    const auto spreadDirection = item.state().presentation().spreadDirection();
    const double pageGap = item.state().presentation().pageGap();
    const int commandReason = commandReasonValue(item);
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(
        setSpreadDirectionCommand(item, spreadDirection), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), commandReason);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(setPageGapCommand(item, pageGap), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), commandReason);
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);

    QCOMPARE(stateSpy.count(), 0);

    const ImageViewportStateSnapshot beforeCompositeNoop = item.state();
    ImageViewportPresentationCommand compositeNoop;
    compositeNoop.setFitMode(beforeCompositeNoop.presentation().fitMode());
    compositeNoop.setRotationDegrees(beforeCompositeNoop.presentation().rotationDegrees());
    compositeNoop.setMirrorHorizontally(beforeCompositeNoop.presentation().mirrorHorizontally());
    compositeNoop.setMirrorVertically(beforeCompositeNoop.presentation().mirrorVertically());
    compositeNoop.setBackgroundMode(beforeCompositeNoop.presentation().backgroundMode());
    compositeNoop.setBackgroundColor(beforeCompositeNoop.presentation().backgroundColor());
    compositeNoop.setCheckerboardLightColor(
        beforeCompositeNoop.presentation().checkerboardLightColor());
    compositeNoop.setCheckerboardDarkColor(
        beforeCompositeNoop.presentation().checkerboardDarkColor());
    compositeNoop.setCheckerboardCellSize(
        beforeCompositeNoop.presentation().checkerboardCellSize());
    compositeNoop.setSmoothing(beforeCompositeNoop.presentation().smoothing());
    compositeNoop.setMipmap(beforeCompositeNoop.presentation().mipmap());
    compositeNoop.setLooping(beforeCompositeNoop.presentation().looping());
    compositeNoop.setQualityPreference(beforeCompositeNoop.presentation().qualityPreference());
    compositeNoop.setExactnessPreference(beforeCompositeNoop.presentation().exactnessPreference());

    const ImageViewportCommandResult compositeNoopResult = item.setPresentation(compositeNoop);
    QCOMPARE(compositeNoopResult.outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(item.state(), beforeCompositeNoop);
    QCOMPARE(compositeNoopResult.snapshotRevision(), beforeCompositeNoop.revisions().snapshot());
    QCOMPARE(stateSpy.count(), 0);

    QCOMPARE(setSpreadDirectionCommand(item, ImageViewportSpreadDirection::RightToLeft),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(
        item.state().presentation().spreadDirection(), ImageViewportSpreadDirection::RightToLeft);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    verifyRevisionChanged(item, "displayRevision", displayRevision);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportPublicApiCommandsTest::
    sameTargetRefinementPreservesSelectionAndRejectsIncompatibleTiming()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::red);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::blue);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);

    TimedImageFrameList originalList;
    QVERIFY(originalList.appendFrame(&firstFrame, 100));
    QVERIFY(originalList.appendFrame(&secondFrame, 250));
    TimedImageFrameList refinementList;
    QVERIFY(refinementList.appendFrame(&secondFrame, 100));
    QVERIFY(refinementList.appendFrame(&firstFrame, 250));
    TimedImageFrameList incompatibleList;
    QVERIFY(incompatibleList.appendFrame(&secondFrame, 100));
    QVERIFY(incompatibleList.appendFrame(&firstFrame, 251));

    QScopedPointer<ImageSequenceFactoryResult> original(factory.fromTimedFrameList(&originalList));
    QScopedPointer<ImageSequenceFactoryResult> refinement(
        factory.fromTimedFrameList(&refinementList));
    QScopedPointer<ImageSequenceFactoryResult> incompatible(
        factory.fromTimedFrameList(&incompatibleList));
    QVERIFY(original->sequence());
    QVERIFY(refinement->sequence());
    QVERIFY(incompatible->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(original->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(item.seek(ImageViewportPageRole::Primary, 1).outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);

    PresentationTargetTransitionPolicy refinementPolicy;
    refinementPolicy.setReplacementIntent(
        PresentationTargetTransitionPolicy::ReplacementIntent::SameTargetRefinement);
    const ImageViewportPresentationTargetGenerationToken generationBefore
        = item.state().request().acceptedPresentationTargetGeneration();
    QCOMPARE(item.setPresentationTarget(
                     ImageViewportPresentationTarget(refinement->sequence()), refinementPolicy)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QVERIFY(item.state().request().acceptedPresentationTargetGeneration() != generationBefore);
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(displayStatusValue(item), enumValue(item.metaObject(), "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(primaryDisplayedFrame(item), 1);

    const ImageViewportStateSnapshot beforeRejected = item.state();
    ImageSequence* const sequenceBeforeRejected = viewportPrimarySequence(item);
    const ImageViewportCommandResult rejected = item.setPresentationTarget(
        ImageViewportPresentationTarget(incompatible->sequence()), refinementPolicy);
    QCOMPARE(rejected.outcome(), ImageViewportCommandOutcome::Invalid);
    QCOMPARE(viewportPrimarySequence(item), sequenceBeforeRejected);
    QCOMPARE(item.state().request(), beforeRejected.request());
    QCOMPARE(item.state().display(), beforeRejected.display());
    QCOMPARE(item.state().presentation(), beforeRejected.presentation());
}

void ImageViewportPublicApiCommandsTest::unresolvedTargetAnchorResolvesWhenProviderGeometryArrives()
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

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    PresentationTargetTransitionPolicy policy;
    policy.setFitModeTransition(PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    policy.setFitMode(ImageViewportFitMode::Manual);
    policy.setContentPositionTransition(
        PresentationTargetTransitionPolicy::ContentPositionTransition::AnchorEnd);
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()), policy)
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QVERIFY(sessionFactory->lastSession());
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(contentPosition(item), QPointF());

    emitProviderMetadataReady(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(400.0, 100.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewportFitMode::Manual);
    QImage payload(400, 100, QImage::Format_ARGB32_Premultiplied);
    payload.fill(Qt::transparent);
    ImageFrame frame(payload);
    emitProviderFrameReady(
        sessionFactory->lastSession(), sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(maximumContentPosition(item), QPointF(300.0, 0.0));
    QCOMPARE(contentPosition(item), QPointF(300.0, 0.0));
}

QTEST_MAIN(ImageViewportPublicApiCommandsTest)

#include "tst_imageviewport_public_api_commands.moc"
