#include "viewportengine_p.h"

#include <QtCore/QScopedPointer>
#include <QtGui/QImage>
#include <QtTest/QTest>

class ViewportEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultSnapshotMatchesPublicDefaultProjection();
    void invalidCommandUpdatesOnlyCommandDiagnostics();
    void malformedEnumRejectionMatchesInvalidCommand();
    void clearFromEmptyIsAcceptedNoop();
    void presentationNoopValidatesEnumShape();
    void validPageSetAssignmentAllocatesGenerationAndRoleSet();
    void twoRoleAssignmentIsAcceptedAtomically();
    void invalidPageSetAssignmentMutatesOnlyCommandDiagnostics();
    void invalidTransitionPolicyMutatesOnlyCommandDiagnostics();
    void clearPageSetAllocatesTransactionAndThenNoops();
    void pageSetAssignmentPreservesPreviousCommandDiagnostic();
    void assignmentEffectFlagsFollowTransitionPolicy();
};

void ViewportEngineTest::defaultSnapshotMatchesPublicDefaultProjection()
{
    ViewportEngine engine;
    ImageViewport item;
    const ImageViewportStateSnapshot engineSnapshot = engine.snapshot();
    const ImageViewportStateSnapshot itemSnapshot = item.state();

    QCOMPARE(engineSnapshot.request(), itemSnapshot.request());
    QCOMPARE(engineSnapshot.display().status(), itemSnapshot.display().status());
    QCOMPARE(engineSnapshot.display().phase(), itemSnapshot.display().phase());
    QCOMPARE(
        engineSnapshot.display().displayedRoleSet(), itemSnapshot.display().displayedRoleSet());
    QCOMPARE(engineSnapshot.display().targetRoleSet(), itemSnapshot.display().targetRoleSet());
    QCOMPARE(engineSnapshot.display().belongsToAcceptedPageSet(),
        itemSnapshot.display().belongsToAcceptedPageSet());
    QCOMPARE(engineSnapshot.display().retained(), itemSnapshot.display().retained());
    QCOMPARE(engineSnapshot.display().displayedPresentationRevision().isValid(), false);
    QCOMPARE(engineSnapshot.display().targetPresentationRevision().isValid(), false);
    QCOMPARE(engineSnapshot.primary().present(), itemSnapshot.primary().present());
    QCOMPARE(engineSnapshot.primary().sequence(), itemSnapshot.primary().sequence());
    QCOMPARE(engineSnapshot.secondary().present(), itemSnapshot.secondary().present());
    QCOMPARE(engineSnapshot.secondary().sequence(), itemSnapshot.secondary().sequence());
    QCOMPARE(engineSnapshot.diagnostics(), itemSnapshot.diagnostics());
    QCOMPARE(engineSnapshot.revisions(), itemSnapshot.revisions());
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(engine.commandDiagnostics().revision.isValid(), false);
}

void ViewportEngineTest::invalidCommandUpdatesOnlyCommandDiagnostics()
{
    ViewportEngine engine;
    const ImageViewportStateSnapshot snapshot = engine.snapshot();

    const ViewportEngine::CommandResult result = engine.rejectInvalidCommand();

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(result.commandRevisionChanged, true);
    QVERIFY(result.commandRevision.isValid());
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(engine.commandDiagnostics().revision, result.commandRevision);
    QCOMPARE(engine.snapshot(), snapshot);
}

void ViewportEngineTest::malformedEnumRejectionMatchesInvalidCommand()
{
    ViewportEngine engine;

    const ViewportEngine::CommandResult result = engine.rejectMalformedEnumCommand();

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(result.commandRevisionChanged, true);
    QVERIFY(result.commandRevision.isValid());
}

void ViewportEngineTest::clearFromEmptyIsAcceptedNoop()
{
    ViewportEngine engine;
    const ImageViewportStateSnapshot snapshot = engine.snapshot();

    const ViewportEngine::CommandResult result = engine.clearFromEmpty();

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(result.commandRevisionChanged, false);
    QCOMPARE(result.commandRevision.isValid(), false);
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(engine.snapshot(), snapshot);
}

void ViewportEngineTest::presentationNoopValidatesEnumShape()
{
    ViewportEngine engine;

    const ViewportEngine::CommandResult accepted
        = engine.validatePresentationNoop(ImageViewport::FitMode::Contain);
    QCOMPARE(accepted.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(accepted.commandRevisionChanged, false);

    const ViewportEngine::CommandResult rejected
        = engine.validatePresentationNoop(static_cast<ImageViewport::FitMode>(-1));
    QCOMPARE(rejected.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(rejected.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(rejected.commandRevisionChanged, true);
    QVERIFY(rejected.commandRevision.isValid());
}

void ViewportEngineTest::validPageSetAssignmentAllocatesGenerationAndRoleSet()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    const ViewportEngine::PageSetAssignmentResult result
        = engine.assignPageSet({ ImageViewportPageSet(sequence->sequence()), {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.command.reason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(result.command.commandRevisionChanged, false);
    QCOMPARE(result.pageSetChanged, true);
    QCOMPARE(result.clear, false);
    QCOMPARE(result.pageSetState.pageSet.primary(), sequence->sequence());
    QCOMPARE(result.pageSetState.pageSet.secondary(), nullptr);
    QCOMPARE(result.pageSetState.acceptedRoleSet, ImageViewportRoleSet(true, false));
    QCOMPARE(result.pageSetState.targetRoleSet, ImageViewportRoleSet(true, false));
    QCOMPARE(result.pageSetState.generation, 1);
    QCOMPARE(result.pageSetState.primaryRoleGeneration, 1);
    QCOMPARE(result.pageSetState.secondaryRoleGeneration, 0);
    QCOMPARE(result.pageSetState.activeRoleValid, true);
    QCOMPARE(result.pageSetState.activeRole, ImageViewport::PageRole::Primary);
    QCOMPARE(engine.pageSetState().generation, result.pageSetState.generation);
}

void ViewportEngineTest::twoRoleAssignmentIsAcceptedAtomically()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromFrame(&primaryFrame));
    QVERIFY(primary->sequence());
    QImage secondaryImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::transparent);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondary(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondary->sequence());

    ViewportEngine engine;
    const ViewportEngine::PageSetAssignmentResult result = engine.assignPageSet(
        { ImageViewportPageSet(primary->sequence(), secondary->sequence()), {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.pageSetState.pageSet.primary(), primary->sequence());
    QCOMPARE(result.pageSetState.pageSet.secondary(), secondary->sequence());
    QCOMPARE(result.pageSetState.acceptedRoleSet, ImageViewportRoleSet(true, true));
    QCOMPARE(result.pageSetState.targetRoleSet, ImageViewportRoleSet(true, true));
    QCOMPARE(result.pageSetState.primaryRoleGeneration, result.pageSetState.generation);
    QCOMPARE(result.pageSetState.secondaryRoleGeneration, result.pageSetState.generation);
}

void ViewportEngineTest::invalidPageSetAssignmentMutatesOnlyCommandDiagnostics()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    ImageFrame secondaryFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> secondary(factory.fromFrame(&secondaryFrame));
    QVERIFY(primary->sequence());
    QVERIFY(secondary->sequence());

    ViewportEngine engine;
    QVERIFY(engine.assignPageSet({ ImageViewportPageSet(primary->sequence()), {} }).pageSetChanged);
    const ViewportEngine::PageSetState previousState = engine.pageSetState();
    ImageViewportPageSet secondaryOnly;
    secondaryOnly.setSecondary(secondary->sequence());

    const ViewportEngine::PageSetAssignmentResult result
        = engine.assignPageSet({ secondaryOnly, {} });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.command.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(result.command.commandRevisionChanged, true);
    QVERIFY(result.command.commandRevision.isValid());
    QCOMPARE(result.pageSetChanged, false);
    QCOMPARE(engine.pageSetState().pageSet, previousState.pageSet);
    QCOMPARE(engine.pageSetState().acceptedRoleSet, previousState.acceptedRoleSet);
    QCOMPARE(engine.pageSetState().generation, previousState.generation);
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::InvalidRequest);
}

void ViewportEngineTest::invalidTransitionPolicyMutatesOnlyCommandDiagnostics()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame primaryFrame(image);
    ImageFrame replacementFrame(image);
    QScopedPointer<ImageSequenceFactoryResult> primary(factory.fromFrame(&primaryFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacement(factory.fromFrame(&replacementFrame));
    QVERIFY(primary->sequence());
    QVERIFY(replacement->sequence());

    ViewportEngine engine;
    QVERIFY(engine.assignPageSet({ ImageViewportPageSet(primary->sequence()), {} }).pageSetChanged);
    const ViewportEngine::PageSetState previousState = engine.pageSetState();
    PageSetTransitionPolicy invalidPolicy;
    invalidPolicy.setPageGapTransition(PageSetTransitionPolicy::PageGapTransition::SetExplicit);
    invalidPolicy.setPageGap(-1.0);

    const ViewportEngine::PageSetAssignmentResult result
        = engine.assignPageSet({ ImageViewportPageSet(replacement->sequence()), invalidPolicy });

    QCOMPARE(result.command.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(result.pageSetChanged, false);
    QCOMPARE(engine.pageSetState().pageSet, previousState.pageSet);
    QCOMPARE(engine.pageSetState().acceptedRoleSet, previousState.acceptedRoleSet);
    QCOMPARE(engine.pageSetState().generation, previousState.generation);
}

void ViewportEngineTest::clearPageSetAllocatesTransactionAndThenNoops()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    QVERIFY(
        engine.assignPageSet({ ImageViewportPageSet(sequence->sequence()), {} }).pageSetChanged);

    const ViewportEngine::PageSetAssignmentResult clearResult
        = engine.assignPageSet({ ImageViewportPageSet::clear(), {} });

    QCOMPARE(clearResult.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(clearResult.clear, true);
    QCOMPARE(clearResult.pageSetChanged, true);
    QCOMPARE(clearResult.pageSetState.pageSet, ImageViewportPageSet::clear());
    QCOMPARE(clearResult.pageSetState.acceptedRoleSet, ImageViewportRoleSet(false, false));
    QCOMPARE(clearResult.pageSetState.targetRoleSet, ImageViewportRoleSet(false, false));
    QCOMPARE(clearResult.pageSetState.generation, 2);
    QCOMPARE(clearResult.releaseDisplayedState, true);
    QCOMPARE(clearResult.resetDisplayRequests, true);
    QCOMPARE(clearResult.closeProviderSessions, true);

    const ViewportEngine::PageSetAssignmentResult noopClear
        = engine.assignPageSet({ ImageViewportPageSet::clear(), {} });

    QCOMPARE(noopClear.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(noopClear.pageSetChanged, false);
    QCOMPARE(noopClear.pageSetState.generation, clearResult.pageSetState.generation);
    QCOMPARE(noopClear.resetDisplayRequests, false);
    QCOMPARE(noopClear.closeProviderSessions, false);
}

void ViewportEngineTest::pageSetAssignmentPreservesPreviousCommandDiagnostic()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> sequence(factory.fromFrame(&frame));
    QVERIFY(sequence->sequence());

    ViewportEngine engine;
    QVERIFY(engine.rejectInvalidCommand().commandRevisionChanged);
    const RevisionToken rejectedRevision = engine.commandDiagnostics().revision;

    const ViewportEngine::PageSetAssignmentResult accepted
        = engine.assignPageSet({ ImageViewportPageSet(sequence->sequence()), {} });

    QCOMPARE(accepted.command.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(accepted.command.reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(accepted.command.commandRevisionChanged, false);
    QCOMPARE(accepted.command.commandRevision, rejectedRevision);
    QCOMPARE(engine.commandDiagnostics().reason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(engine.commandDiagnostics().revision, rejectedRevision);
}

void ViewportEngineTest::assignmentEffectFlagsFollowTransitionPolicy()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> first(factory.fromFrame(&firstFrame));
    QVERIFY(first->sequence());
    QImage secondImage(8, 16, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame secondFrame(secondImage);
    QScopedPointer<ImageSequenceFactoryResult> second(factory.fromFrame(&secondFrame));
    QVERIFY(second->sequence());

    ViewportEngine engine;
    const ViewportEngine::PageSetAssignmentResult retained
        = engine.assignPageSet({ ImageViewportPageSet(first->sequence()), {} });
    QCOMPARE(retained.retainPreviousDisplay, true);
    QCOMPARE(retained.releaseDisplayedState, false);
    QCOMPARE(retained.resetDisplayRequests, true);
    QCOMPARE(retained.stopPlayback, true);

    PageSetTransitionPolicy policy;
    policy.setDisplayTransition(PageSetTransitionPolicy::DisplayTransition::ClearBeforeLoad);
    const ViewportEngine::PageSetAssignmentResult cleared
        = engine.assignPageSet({ ImageViewportPageSet(second->sequence()), policy });
    QCOMPARE(cleared.retainPreviousDisplay, false);
    QCOMPARE(cleared.releaseDisplayedState, true);
    QCOMPARE(cleared.resetDisplayRequests, true);
    QCOMPARE(cleared.stopPlayback, true);
}

QTEST_MAIN(ViewportEngineTest)

#include "tst_viewportengine.moc"
