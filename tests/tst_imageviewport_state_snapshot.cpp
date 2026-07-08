#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

class ImageViewportStateSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultSnapshotValuesAndCopySemantics();
    void readyStillSnapshotMatchesFlatProperties();
    void loadingReplacementRetainsPreviousDisplaySeparately();
    void terminalProviderFailureProjectsDiagnostics();
    void presentationOnlyChangesUpdateSnapshot();
    void presentationCommandUpdatesSnapshotGeometry();
    void qmlReadsNestedSnapshotFields();
};

void ImageViewportStateSnapshotTest::defaultSnapshotValuesAndCopySemantics()
{
    ImageViewport item;
    const ImageViewportStateSnapshot snapshot = item.state();

    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::NoRequest);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::NoRequest);
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewport::PlaybackPhase::Stopped);
    QVERIFY(!snapshot.request().acceptedPageSetGeneration().isValid());
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.request().targetRoleSet(), ImageViewportRoleSet(false, false));
    QVERIFY(!snapshot.request().activeRole().isValid());
    QVERIFY(!snapshot.request().playbackRole().isValid());

    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Empty);
    QCOMPARE(snapshot.display().phase(), ImageViewport::DisplayPhase::NoPresentation);
    QVERIFY(!snapshot.display().displayedPageSetGeneration().isValid());
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.display().targetRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.display().belongsToAcceptedPageSet(), false);
    QCOMPARE(snapshot.display().retained(), false);
    QVERIFY(!snapshot.display().displayedPresentationRevision().isValid());
    QVERIFY(!snapshot.display().targetPresentationRevision().isValid());

    QCOMPARE(snapshot.presentation().fitMode(), item.fitMode());
    QCOMPARE(snapshot.presentation().zoomPercent(), item.zoomPercent());
    QCOMPARE(snapshot.presentation().minimumManualZoomPercent(), item.minimumManualZoomPercent());
    QCOMPARE(snapshot.presentation().maximumManualZoomPercent(), item.maximumManualZoomPercent());
    QCOMPARE(snapshot.presentation().manualZoomStepFactor(), item.manualZoomStepFactor());
    QCOMPARE(snapshot.presentation().rotationDegrees(), item.rotationDegrees());
    QCOMPARE(snapshot.presentation().spreadDirection(), item.spreadDirection());
    QCOMPARE(snapshot.presentation().pageGap(), item.pageGap());
    QCOMPARE(
        snapshot.presentation().qualityPreference(), ImageViewport::QualityPreference::Default);
    QCOMPARE(
        snapshot.presentation().exactnessPreference(), ImageViewport::ExactnessPreference::Default);

    QCOMPARE(snapshot.primary().present(), false);
    QCOMPARE(snapshot.primary().sequence(), nullptr);
    QCOMPARE(snapshot.primary().request().frame(), -1);
    QCOMPARE(snapshot.primary().display().frame(), -1);
    QCOMPARE(snapshot.primary().metadata().available(), false);
    QCOMPARE(snapshot.primary().metadata().frameSeekSupport(),
        ImageViewport::CapabilitySupport::Unavailable);
    QCOMPARE(snapshot.secondary().present(), false);

    QCOMPARE(snapshot.diagnostics().errorString(), QString());
    QCOMPARE(snapshot.diagnostics().warningString(), QString());
    QCOMPARE(snapshot.diagnostics().commandReason(), ImageViewport::CommandReason::NoCommand);
    QVERIFY(!snapshot.revisions().request().isValid());
    QVERIFY(!snapshot.revisions().display().isValid());
    QVERIFY(!snapshot.revisions().presentation().isValid());
    QVERIFY(!snapshot.revisions().command().isValid());
    QVERIFY(!snapshot.revisions().snapshot().isValid());

    const ImageViewportStateSnapshot copy = snapshot;
    QVERIFY(copy == snapshot);
    QVERIFY(!(copy != snapshot));
}

void ImageViewportStateSnapshotTest::readyStillSnapshotMatchesFlatProperties()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(item.setPageSet(result->sequence(), nullptr), ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QVERIFY(stateSpy.count() >= 1);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Ready);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::Ready);
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(snapshot.request().targetRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.request().acceptedPageSetGeneration().isValid());
    QCOMPARE(snapshot.request().activeRole().value<ImageViewport::PageRole>(),
        ImageViewport::PageRole::Primary);

    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Ready);
    QCOMPARE(snapshot.display().phase(), ImageViewport::DisplayPhase::CommittedActive);
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(snapshot.display().belongsToAcceptedPageSet(), true);
    QCOMPARE(snapshot.display().retained(), false);
    QCOMPARE(snapshot.display().spreadSize(), item.displayedSpreadSize());
    QCOMPARE(snapshot.display().contentRect(), item.contentRect());
    QCOMPARE(snapshot.display().contentSize(), item.contentSize());
    QCOMPARE(snapshot.display().contentPosition(), item.contentPosition());
    QCOMPARE(snapshot.display().maximumContentPosition(), item.maximumContentPosition());
    QCOMPARE(snapshot.display().visibleSpreadRect(), item.visibleSpreadRect());
    QCOMPARE(snapshot.display().horizontalPannable(), item.horizontalPannable());
    QCOMPARE(snapshot.display().verticalPannable(), item.verticalPannable());

    QCOMPARE(snapshot.primary().present(), true);
    QCOMPARE(snapshot.primary().sequence(), result->sequence());
    QCOMPARE(snapshot.primary().request().frame(), item.requestedFrame());
    QCOMPARE(snapshot.primary().request().position(), item.requestedPosition());
    QCOMPARE(snapshot.primary().request().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().belongsToAcceptedPageSet(), true);
    QCOMPARE(snapshot.primary().display().retained(), false);
    QCOMPARE(snapshot.primary().display().frame(), item.displayedFrame());
    QCOMPARE(snapshot.primary().display().position(), item.displayedPosition());
    QCOMPARE(snapshot.primary().display().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().payloadRasterSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().sourceToPayloadScale(), QSizeF(1.0, 1.0));
    QCOMPARE(snapshot.primary().display().quality(), ImageViewport::PayloadQuality::Exact);
    QCOMPARE(
        snapshot.primary().display().exactness(), ImageViewport::PayloadExactness::ExactForSource);
    QCOMPARE(snapshot.primary().metadata().available(), true);
    QCOMPARE(snapshot.primary().metadata().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().metadata().frameCount(), item.frameCount());
    QCOMPARE(snapshot.primary().metadata().totalDuration(), item.totalDuration());
    QCOMPARE(snapshot.primary().metadata().frameSeekBounds(), item.frameSeekBounds());
    QCOMPARE(snapshot.primary().metadata().positionSeekBounds(), item.positionSeekBounds());
    QCOMPARE(snapshot.primary().geometry().acceptedPageRect(), item.primaryPageRect());
    QCOMPARE(snapshot.primary().geometry().acceptedItemRect(), item.primaryItemRect());
    QCOMPARE(
        snapshot.primary().geometry().acceptedVisiblePageRect(), item.visiblePrimaryPageRect());
    QCOMPARE(snapshot.primary().geometry().displayedPageRect(), item.primaryPageRect());

    QVERIFY(snapshot.revisions().request().isValid());
    QVERIFY(snapshot.revisions().display().isValid());
    QVERIFY(snapshot.revisions().presentation().isValid());
    QVERIFY(snapshot.revisions().snapshot().isValid());
}

void ImageViewportStateSnapshotTest::loadingReplacementRetainsPreviousDisplaySeparately()
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
    const ImageViewportStateSnapshot readySnapshot = item.state();
    QCOMPARE(readySnapshot.display().status(), ImageViewport::DisplayStatus::Ready);

    QCOMPARE(item.setPageSet(loadingResult->sequence(), nullptr),
        ImageViewport::CommandOutcome::Accepted);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Loading);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::ProviderWaiting);
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.request().acceptedPageSetGeneration().isValid());
    QVERIFY(snapshot.request().acceptedPageSetGeneration()
        != readySnapshot.request().acceptedPageSetGeneration());
    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Retained);
    QCOMPARE(snapshot.display().phase(), ImageViewport::DisplayPhase::PreviousActive);
    QCOMPARE(snapshot.display().retained(), true);
    QCOMPARE(snapshot.display().belongsToAcceptedPageSet(), false);
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.display().displayedPageSetGeneration().isValid());
    QVERIFY(snapshot.display().displayedPageSetGeneration()
        != snapshot.request().acceptedPageSetGeneration());
    QCOMPARE(snapshot.primary().sequence(), loadingResult->sequence());
    QCOMPARE(snapshot.primary().request().frame(), -1);
    QCOMPARE(snapshot.primary().metadata().available(), false);
    QCOMPARE(snapshot.primary().display().retained(), true);
    QCOMPARE(snapshot.primary().display().belongsToAcceptedPageSet(), false);
    QCOMPARE(snapshot.primary().display().frame(), 0);
    QCOMPARE(snapshot.primary().display().sourceLogicalSize(), QSizeF(16.0, 8.0));
}

void ImageViewportStateSnapshotTest::terminalProviderFailureProjectsDiagnostics()
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
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    item.setSequence(result->sequence());
    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(
        sessionFactory->lastSession()->lastMetadataToken(), QStringLiteral("metadata unavailable"));
    drainQueuedProviderResults();

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Error);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::ProviderFailure);
    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Empty);
    QCOMPARE(snapshot.primary().request().frame(), -1);
    QCOMPARE(snapshot.diagnostics().errorString().contains(QStringLiteral("metadata unavailable")),
        true);
    QCOMPARE(snapshot.diagnostics().commandReason(), item.commandReason());
    QVERIFY(snapshot.revisions().request().isValid());
    QVERIFY(snapshot.revisions().snapshot().isValid());
    QVERIFY(stateSpy.count() >= 2);
}

void ImageViewportStateSnapshotTest::presentationOnlyChangesUpdateSnapshot()
{
    ImageViewport item;
    const ImageViewportStateSnapshot before = item.state();
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    item.setSmoothing(!item.smoothing());
    const ImageViewportStateSnapshot after = item.state();

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(after.request(), before.request());
    QCOMPARE(after.display().status(), before.display().status());
    QCOMPARE(after.presentation().smoothing(), item.smoothing());
    QVERIFY(after != before);

    item.setSmoothing(item.smoothing());
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportStateSnapshotTest::presentationCommandUpdatesSnapshotGeometry()
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
    command.setManualZoomPercent(200.0);
    command.setPanDelta(QPointF(4.0, 2.0));

    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Invalid);

    command = {};
    command.setManualZoomPercent(200.0);
    QCOMPARE(item.setPresentation(command), ImageViewport::CommandOutcome::Accepted);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(snapshot.presentation().zoomPercent(), item.zoomPercent());
    QCOMPARE(snapshot.display().contentRect(), item.contentRect());
    QCOMPARE(snapshot.display().visibleSpreadRect(), item.visibleSpreadRect());
    QCOMPARE(snapshot.primary().geometry().acceptedItemRect(), item.primaryItemRect());
    QCOMPARE(snapshot.primary().geometry().displayedItemRect(), item.primaryItemRect());
    QVERIFY(snapshot.revisions().presentation().isValid());
}

void ImageViewportStateSnapshotTest::qmlReadsNestedSnapshotFields()
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

    property int observedChanges: 0
    property bool defaultOk: state.request.status === ImageViewport.RequestStatus.NoRequest
        && state.display.status === ImageViewport.DisplayStatus.Empty
        && state.display.phase === ImageViewport.DisplayPhase.NoPresentation
        && state.primary.present === false
        && state.diagnostics.errorString === ""
        && !state.revisions.request.valid
    property bool readyOk: state.request.status === ImageViewport.RequestStatus.Ready
        && state.display.status === ImageViewport.DisplayStatus.Ready
        && state.display.phase === ImageViewport.DisplayPhase.CommittedActive
        && state.primary.present
        && state.primary.request.frame === 0
        && state.primary.display.frame === 0
        && state.primary.metadata.available
        && state.primary.metadata.frameCount === 1
        && state.revisions.request.valid
        && state.revisions.display.valid

    onStateChanged: observedChanges += 1
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    QCOMPARE(object->property("defaultOk").toBool(), true);
    auto* viewport = qobject_cast<ImageViewport*>(object.data());
    QVERIFY(viewport);

    viewport->setSequence(result->sequence());
    acknowledgePendingRenderCommitForTest(*viewport);

    QCOMPARE(object->property("readyOk").toBool(), true);
    QVERIFY(object->property("observedChanges").toInt() >= 1);
}

QTEST_MAIN(ImageViewportStateSnapshotTest)

#include "tst_imageviewport_state_snapshot.moc"
