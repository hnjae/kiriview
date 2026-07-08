#include "viewportengine_p.h"

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

QTEST_MAIN(ViewportEngineTest)

#include "tst_viewportengine.moc"
