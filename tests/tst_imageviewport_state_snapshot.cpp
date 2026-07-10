#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

class ImageViewportStateSnapshotTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportStateSnapshotTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void defaultSnapshotValuesAndCopySemantics();
    void readyStillSnapshotMatchesFlatProperties();
    void loadingReplacementRetainsPreviousDisplaySeparately();
    void terminalProviderFailureProjectsDiagnostics();
    void timedPlaybackSnapshotTracksRequestState();
    void presentationOnlyChangesUpdateSnapshot();
    void retainedDisplayKeepsCommittedPresentationIdentity();
    void displayedPayloadFactsComeFromCommittedFrame();
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
    QVERIFY(!snapshot.request().acceptedPresentationTargetGeneration().isValid());
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.request().targetRoleSet(), ImageViewportRoleSet(false, false));
    QVERIFY(!snapshot.request().activeRole().isValid());
    QVERIFY(!snapshot.request().playbackRole().isValid());

    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Empty);
    QCOMPARE(snapshot.display().phase(), ImageViewport::DisplayPhase::NoPresentation);
    QVERIFY(!snapshot.display().displayedPresentationTargetGeneration().isValid());
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.display().targetRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.display().belongsToAcceptedPresentationTarget(), false);
    QCOMPARE(snapshot.display().retained(), false);
    QVERIFY(!snapshot.display().displayedPresentationRevision().isValid());
    QVERIFY(!snapshot.display().targetPresentationRevision().isValid());

    QCOMPARE(snapshot.presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(snapshot.presentation().zoomPercent(), 100.0);
    QVERIFY(snapshot.presentation().minimumManualZoomPercent() > 0.0);
    QCOMPARE(snapshot.presentation().maximumManualZoomPercent(),
        ImageViewportDisplayLimits::maximumManualZoomPercent());
    QCOMPARE(snapshot.presentation().manualZoomStepFactor(), 1.25);
    QCOMPARE(snapshot.presentation().rotationDegrees(), 0);
    QCOMPARE(
        snapshot.presentation().spreadDirection(), ImageViewport::SpreadDirection::LeftToRight);
    QCOMPARE(snapshot.presentation().pageGap(), 0.0);
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

    const ImageViewportStateSnapshot copy // NOLINT(performance-unnecessary-copy-initialization)
        = snapshot;
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

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QVERIFY(stateSpy.count() >= 1);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Ready);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::Ready);
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(snapshot.request().targetRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.request().acceptedPresentationTargetGeneration().isValid());
    QCOMPARE(snapshot.request().activeRole().value<ImageViewport::PageRole>(),
        ImageViewport::PageRole::Primary);

    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Ready);
    QCOMPARE(snapshot.display().phase(), ImageViewport::DisplayPhase::CommittedActive);
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(snapshot.display().belongsToAcceptedPresentationTarget(), true);
    QCOMPARE(snapshot.display().retained(), false);
    QCOMPARE(snapshot.display().spreadSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.display().contentRect(), contentRect(item));
    QCOMPARE(snapshot.display().contentSize(), QSizeF(100.0, 50.0));
    QCOMPARE(snapshot.display().contentPosition(), QPointF());
    QCOMPARE(snapshot.display().maximumContentPosition(), QPointF());
    QCOMPARE(snapshot.display().visibleSpreadRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(snapshot.display().horizontalPannable(), false);
    QCOMPARE(snapshot.display().verticalPannable(), false);

    QCOMPARE(snapshot.primary().present(), true);
    QCOMPARE(snapshot.primary().sequence(), result->sequence());
    QCOMPARE(snapshot.primary().request().frame(), 0);
    QCOMPARE(snapshot.primary().request().position(), -1);
    QCOMPARE(snapshot.primary().request().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().belongsToAcceptedPresentationTarget(), true);
    QCOMPARE(snapshot.primary().display().retained(), false);
    QCOMPARE(snapshot.primary().display().frame(), 0);
    QCOMPARE(snapshot.primary().display().position(), -1);
    QCOMPARE(snapshot.primary().display().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().payloadRasterSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().sourceToPayloadScale(), QSizeF(1.0, 1.0));
    QCOMPARE(snapshot.primary().display().quality(), ImageViewport::PayloadQuality::Exact);
    QCOMPARE(
        snapshot.primary().display().exactness(), ImageViewport::PayloadExactness::ExactForSource);
    QCOMPARE(snapshot.primary().metadata().available(), true);
    QCOMPARE(snapshot.primary().metadata().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().metadata().frameCount(), 1);
    QCOMPARE(snapshot.primary().metadata().totalDuration(), -1);
    QCOMPARE(snapshot.primary().metadata().frameSeekBounds().minimum(), 0);
    QCOMPARE(snapshot.primary().metadata().frameSeekBounds().maximum(), 0);
    QCOMPARE(snapshot.primary().metadata().positionSeekBounds().minimum(), -1);
    QCOMPARE(snapshot.primary().metadata().positionSeekBounds().maximum(), -1);
    QCOMPARE(snapshot.primary().geometry().acceptedPageRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(snapshot.primary().geometry().acceptedItemRect(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(snapshot.primary().geometry().acceptedVisiblePageRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(snapshot.primary().geometry().displayedPageRect(),
        snapshot.primary().geometry().acceptedPageRect());

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
    item.setPresentationTarget(ImageViewportPresentationTarget(readyResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const ImageViewportStateSnapshot readySnapshot = item.state();
    QCOMPARE(readySnapshot.display().status(), ImageViewport::DisplayStatus::Ready);

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Loading);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::ProviderWaiting);
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.request().acceptedPresentationTargetGeneration().isValid());
    QVERIFY(snapshot.request().acceptedPresentationTargetGeneration()
        != readySnapshot.request().acceptedPresentationTargetGeneration());
    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Retained);
    QCOMPARE(snapshot.display().phase(), ImageViewport::DisplayPhase::PreviousActive);
    QCOMPARE(snapshot.display().retained(), true);
    QCOMPARE(snapshot.display().belongsToAcceptedPresentationTarget(), false);
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.display().displayedPresentationTargetGeneration().isValid());
    QVERIFY(snapshot.display().displayedPresentationTargetGeneration()
        != snapshot.request().acceptedPresentationTargetGeneration());
    QCOMPARE(snapshot.primary().sequence(), loadingResult->sequence());
    QCOMPARE(snapshot.primary().request().frame(), -1);
    QCOMPARE(snapshot.primary().metadata().available(), false);
    QCOMPARE(snapshot.primary().display().retained(), true);
    QCOMPARE(snapshot.primary().display().belongsToAcceptedPresentationTarget(), false);
    QCOMPARE(snapshot.primary().display().frame(), 0);
    QCOMPARE(snapshot.primary().display().sourceLogicalSize(), QSizeF(16.0, 8.0));
}

void ImageViewportStateSnapshotTest::retainedDisplayKeepsCommittedPresentationIdentity()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::red);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> first(factory.fromFrame(&firstFrame));
    QImage secondImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::blue);
    ImageFrame secondFrame(secondImage);
    QScopedPointer<ImageSequenceFactoryResult> second(factory.fromFrame(&secondFrame));
    QVERIFY(first->sequence());
    QVERIFY(second->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(first->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    ImageViewportPresentationCommand rotation;
    rotation.setRotationDegrees(90);
    QCOMPARE(item.setPresentation(rotation).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot committed = item.state();

    PresentationTargetTransitionPolicy policy;
    policy.setRotationTransition(PresentationTargetTransitionPolicy::RotationTransition::Reset);
    item.setPresentationTarget(ImageViewportPresentationTarget(second->sequence()), policy);
    const ImageViewportStateSnapshot retained = item.state();

    QCOMPARE(retained.display().status(), ImageViewport::DisplayStatus::Retained);
    QVERIFY(
        retained.display().displayedPresentationRevision() != retained.revisions().presentation());
    QCOMPARE(retained.primary().geometry().displayedItemRect(),
        committed.primary().geometry().displayedItemRect());
    QVERIFY(retained.primary().geometry().acceptedItemRect()
        != retained.primary().geometry().displayedItemRect());
}

void ImageViewportStateSnapshotTest::displayedPayloadFactsComeFromCommittedFrame()
{
    QImage payload(8, 4, QImage::Format_ARGB32_Premultiplied);
    payload.fill(Qt::transparent);
    ImageSequenceProviderFrameEnvelope envelope;
    envelope.setSourceLogicalSize(QSizeF(16.0, 8.0));
    envelope.setPayloadRasterSize(QSizeF(8.0, 4.0));
    envelope.setSourceToPayloadScale(QSizeF(0.5, 0.5));
    envelope.setPayloadByteSize(payload.sizeInBytes());
    envelope.setQuality(ImageViewport::PayloadQuality::Preview);
    envelope.setExactness(ImageViewport::PayloadExactness::NotExact);
    envelope.setFrame(0);
    envelope.setFrameStartPosition(-1);
    envelope.setFrameDuration(-1);
    envelope.setHasAlpha(true);
    ImageFrame frame(payload, envelope);
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const ImageViewportRoleDisplaySnapshot display = item.state().primary().display();
    QCOMPARE(display.sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(display.payloadRasterSize(), QSizeF(8.0, 4.0));
    QCOMPARE(display.sourceToPayloadScale(), QSizeF(0.5, 0.5));
    QCOMPARE(display.quality(), ImageViewport::PayloadQuality::Preview);
    QCOMPARE(display.exactness(), ImageViewport::PayloadExactness::NotExact);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    QVERIFY(sessionFactory->lastSession());
    emitProviderFailed(sessionFactory->lastSession(),
        sessionFactory->lastSession()->lastMetadataToken(), QStringLiteral("metadata unavailable"));
    drainQueuedProviderResults();

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Error);
    QCOMPARE(snapshot.request().reason(), ImageViewport::RequestReason::ProviderFailure);
    QCOMPARE(snapshot.display().status(), ImageViewport::DisplayStatus::Empty);
    QCOMPARE(snapshot.primary().request().frame(), -1);
    QCOMPARE(snapshot.diagnostics().errorString().contains(QStringLiteral("metadata unavailable")),
        true);
    QCOMPARE(snapshot.diagnostics().commandReason(), viewportCommandReason(item));
    QVERIFY(snapshot.revisions().request().isValid());
    QVERIFY(snapshot.revisions().snapshot().isValid());
    QVERIFY(stateSpy.count() >= 2);
}

void ImageViewportStateSnapshotTest::timedPlaybackSnapshotTracksRequestState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Ready);
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewport::PlaybackPhase::Stopped);
    QVERIFY(!snapshot.request().playbackRole().isValid());
    QCOMPARE(snapshot.primary().request().frame(), 0);
    QCOMPARE(snapshot.primary().request().position(), 0);

    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    snapshot = item.state();
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewport::PlaybackPhase::Playing);
    QCOMPARE(snapshot.request().playbackRole().value<ImageViewport::PageRole>(),
        ImageViewport::PageRole::Primary);
    QCOMPARE(snapshot.primary().request().frame(), 0);
    QCOMPARE(snapshot.primary().request().position(), 0);

    QCOMPARE(item.pause(ImageViewport::PageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    snapshot = item.state();
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewport::PlaybackPhase::Paused);
    QCOMPARE(snapshot.request().playbackRole().value<ImageViewport::PageRole>(),
        ImageViewport::PageRole::Primary);

    QCOMPARE(item.stop(ImageViewport::PageRole::Primary).outcome(),
        ImageViewport::CommandOutcome::Accepted);
    snapshot = item.state();
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewport::PlaybackPhase::Stopped);
    QVERIFY(!snapshot.request().playbackRole().isValid());
    QCOMPARE(snapshot.request().status(), ImageViewport::RequestStatus::Ready);
}

void ImageViewportStateSnapshotTest::presentationOnlyChangesUpdateSnapshot()
{
    ImageViewport item;
    const ImageViewportStateSnapshot before = item.state();
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    ImageViewportPresentationCommand smoothingCommand;
    smoothingCommand.setSmoothing(!before.presentation().smoothing());
    QCOMPARE(
        item.setPresentation(smoothingCommand).outcome(), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportStateSnapshot after = item.state();

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(after.request(), before.request());
    QCOMPARE(after.display().status(), before.display().status());
    QCOMPARE(after.presentation().smoothing(), !before.presentation().smoothing());
    QVERIFY(after != before);

    smoothingCommand = {};
    smoothingCommand.setSmoothing(after.presentation().smoothing());
    QCOMPARE(
        item.setPresentation(smoothingCommand).outcome(), ImageViewport::CommandOutcome::Accepted);
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
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);

    ImageViewportPresentationCommand command;
    command.setManualZoomPercent(200.0);
    command.setPanDelta(QPointF(4.0, 2.0));

    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Invalid);

    command = {};
    command.setManualZoomPercent(200.0);
    command.setQualityPreference(ImageViewport::QualityPreference::BalancedDetail);
    command.setExactnessPreference(ImageViewport::ExactnessPreference::PreferExact);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewport::CommandOutcome::Accepted);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(snapshot.presentation().zoomPercent(), 200.0);
    QCOMPARE(snapshot.presentation().qualityPreference(),
        ImageViewport::QualityPreference::BalancedDetail);
    QCOMPARE(snapshot.presentation().exactnessPreference(),
        ImageViewport::ExactnessPreference::PreferExact);
    QCOMPARE(snapshot.display().contentRect(), contentRect(item));
    QCOMPARE(snapshot.display().visibleSpreadRect(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(snapshot.primary().geometry().acceptedItemRect(), snapshot.display().contentRect());
    QCOMPARE(snapshot.primary().geometry().displayedItemRect(),
        snapshot.primary().geometry().acceptedItemRect());
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
        && state.primary.geometry.acceptedPageRect.width === 0
        && state.primary.geometry.displayedItemRect.height === 0
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
        && state.primary.geometry.acceptedPageRect.width === 16
        && state.primary.geometry.acceptedPageRect.height === 8
        && state.primary.geometry.acceptedItemRect.y === 25
        && state.primary.geometry.acceptedItemRect.height === 50
        && state.primary.geometry.displayedVisiblePageRect.width === 16
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

    viewport->setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(*viewport);

    QCOMPARE(object->property("readyOk").toBool(), true);
    QVERIFY(object->property("observedChanges").toInt() >= 1);
}

QTEST_MAIN(ImageViewportStateSnapshotTest)

#include "tst_imageviewport_state_snapshot.moc"
