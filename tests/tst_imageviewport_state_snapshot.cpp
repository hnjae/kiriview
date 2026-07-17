#include "imageviewport_provider_test_support.h"
#include "imageviewport_qml_test_support.h"

#include <QtCore/QMetaProperty>

namespace {

QVariant metadataProperty(
    const ImageViewportRoleMetadataSnapshot& metadata, const char* propertyName)
{
    const QMetaObject& metaObject = ImageViewportRoleMetadataSnapshot::staticMetaObject;
    const int propertyIndex = metaObject.indexOfProperty(propertyName);
    return propertyIndex >= 0 ? metaObject.property(propertyIndex).readOnGadget(&metadata)
                              : QVariant {};
}

}

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
    void authoredAnimationMetadataUsesExplicitAvailability();
    void readyStillSnapshotMatchesFlatProperties();
    void loadingReplacementRetainsPreviousDisplaySeparately();
    void terminalProviderFailureProjectsDiagnostics();
    void timedPlaybackSnapshotTracksRequestState();
    void presentationOnlyChangesUpdateSnapshot();
    void retainedDisplayKeepsCommittedPresentationIdentity();
    void displayedPayloadFactsComeFromCommittedFrame();
    void requireExactRejectsNewInMemoryPayloadButPreservesCommittedPixels();
    void presentationCommandUpdatesSnapshotGeometry();
    void qmlReadsNestedSnapshotFields();
};

void ImageViewportStateSnapshotTest::authoredAnimationMetadataUsesExplicitAvailability()
{
    ImageViewport item;
    const ImageViewportRoleMetadataSnapshot unavailable = item.state().primary().metadata();
    QCOMPARE(unavailable.available(), false);
    QCOMPARE(metadataProperty(unavailable, "autoplay"),
        QVariant::fromValue(ImageViewportCapabilitySupport::Unavailable));
    QCOMPARE(unavailable.loopMode(), ImageSequenceAuthoredAnimationLoopMode::Unavailable);
    QCOMPARE(unavailable.loopCount(), -1);

    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    const ImageViewportRoleMetadataSnapshot still = item.state().primary().metadata();
    QCOMPARE(still.available(), true);
    QCOMPARE(metadataProperty(still, "autoplay"),
        QVariant::fromValue(ImageViewportCapabilitySupport::False));
    QCOMPARE(still.loopMode(), ImageSequenceAuthoredAnimationLoopMode::PlayOnce);
    QCOMPARE(still.loopCount(), 1);
}

void ImageViewportStateSnapshotTest::defaultSnapshotValuesAndCopySemantics()
{
    ImageViewport item;
    const ImageViewportStateSnapshot snapshot = item.state();

    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::NoRequest);
    QCOMPARE(snapshot.request().reason(), ImageViewportRequestReason::NoRequest);
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewportPlaybackPhase::Stopped);
    QVERIFY(!snapshot.request().acceptedPresentationTargetGeneration().isValid());
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(false, false));
    QVERIFY(!snapshot.request().playbackRole().isValid());

    QCOMPARE(snapshot.display().status(), ImageViewportDisplayStatus::Empty);
    QCOMPARE(snapshot.display().phase(), ImageViewportDisplayPhase::NoPresentation);
    QVERIFY(!snapshot.display().displayedPresentationTargetGeneration().isValid());
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.display().targetRoleSet(), ImageViewportRoleSet(false, false));
    QCOMPARE(snapshot.display().belongsToAcceptedPresentationTarget(), false);
    QCOMPARE(snapshot.display().retained(), false);
    QVERIFY(!snapshot.display().displayedPresentationRevision().isValid());
    QVERIFY(!snapshot.display().targetPresentationRevision().isValid());

    QCOMPARE(snapshot.presentation().fitMode(), ImageViewportFitMode::Contain);
    QCOMPARE(snapshot.presentation().zoomPercent(), 0.0);
    QCOMPARE(snapshot.presentation().manualZoomPercent(), 100.0);
    QCOMPARE(snapshot.presentation().minimumManualZoomPercent(), 1.0);
    QCOMPARE(snapshot.presentation().maximumManualZoomPercent(),
        ImageViewportDisplayLimits::maximumManualZoomPercent());
    QCOMPARE(snapshot.presentation().manualZoomStepFactor(), 1.25);
    QCOMPARE(snapshot.presentation().rotationDegrees(), 0);
    QCOMPARE(snapshot.presentation().spreadDirection(), ImageViewportSpreadDirection::LeftToRight);
    QCOMPARE(snapshot.presentation().pageGap(), 0.0);
    QCOMPARE(snapshot.presentation().backgroundMode(), ImageViewportBackgroundMode::Transparent);
    QCOMPARE(snapshot.presentation().backgroundColor(), QColor(QStringLiteral("#ffffff")));
    QCOMPARE(snapshot.presentation().checkerboardLightColor(), QColor(QStringLiteral("#ffffff")));
    QCOMPARE(snapshot.presentation().checkerboardDarkColor(), QColor(QStringLiteral("#dcdcdc")));
    QCOMPARE(snapshot.presentation().checkerboardCellSize(), 8.0);
    QCOMPARE(snapshot.presentation().qualityPreference(), ImageViewportQualityPreference::Default);
    QCOMPARE(
        snapshot.presentation().exactnessPreference(), ImageViewportExactnessPreference::Default);

    QCOMPARE(snapshot.primary().present(), false);
    QCOMPARE(snapshot.primary().sequence(), nullptr);
    QCOMPARE(snapshot.primary().request().frame(), -1);
    QCOMPARE(snapshot.primary().display().frame(), -1);
    QCOMPARE(snapshot.primary().metadata().available(), false);
    QCOMPARE(snapshot.primary().metadata().frameSeekSupport(),
        ImageViewportCapabilitySupport::Unavailable);
    QCOMPARE(snapshot.secondary().present(), false);

    QCOMPARE(snapshot.diagnostics().errorString(), QString());
    QCOMPARE(snapshot.diagnostics().warningString(), QString());
    QCOMPARE(snapshot.diagnostics().commandReason(), ImageViewportCommandReason::NoCommand);
    QVERIFY(snapshot.revisions().request().isValid());
    QVERIFY(snapshot.revisions().display().isValid());
    QVERIFY(snapshot.revisions().presentation().isValid());
    QVERIFY(snapshot.revisions().command().isValid());
    QVERIFY(snapshot.revisions().snapshot().isValid());

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
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    QVERIFY(stateSpy.count() >= 1);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Ready);
    QCOMPARE(snapshot.request().reason(), ImageViewportRequestReason::Ready);
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.request().acceptedPresentationTargetGeneration().isValid());

    QCOMPARE(snapshot.display().status(), ImageViewportDisplayStatus::Ready);
    QCOMPARE(snapshot.display().phase(), ImageViewportDisplayPhase::CommittedActive);
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(true, false));
    QCOMPARE(snapshot.display().belongsToAcceptedPresentationTarget(), true);
    QCOMPARE(snapshot.display().retained(), false);
    QVERIFY(snapshot.display().displayedPresentationRevision().isValid());
    QCOMPARE(snapshot.display().displayedPresentationRevision(),
        snapshot.display().targetPresentationRevision());
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
    QCOMPARE(snapshot.primary().display().currentForDemand(), true);
    QCOMPARE(snapshot.primary().display().frame(), 0);
    QCOMPARE(snapshot.primary().display().position(), -1);
    QCOMPARE(snapshot.primary().display().sourceLogicalSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().payloadRasterSize(), QSizeF(16.0, 8.0));
    QCOMPARE(snapshot.primary().display().sourceToPayloadScale(), QSizeF(1.0, 1.0));
    QCOMPARE(snapshot.primary().display().quality(), ImageViewportPayloadQuality::Exact);
    QCOMPARE(
        snapshot.primary().display().exactness(), ImageViewportPayloadExactness::ExactForSource);
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
    QCOMPARE(readySnapshot.display().status(), ImageViewportDisplayStatus::Ready);

    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(loadingResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Loading);
    QCOMPARE(snapshot.request().reason(), ImageViewportRequestReason::ProviderWaiting);
    QCOMPARE(snapshot.request().acceptedRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.request().acceptedPresentationTargetGeneration().isValid());
    QVERIFY(snapshot.request().acceptedPresentationTargetGeneration()
        != readySnapshot.request().acceptedPresentationTargetGeneration());
    QCOMPARE(snapshot.display().status(), ImageViewportDisplayStatus::Retained);
    QCOMPARE(snapshot.display().phase(), ImageViewportDisplayPhase::PreviousActive);
    QCOMPARE(snapshot.display().retained(), true);
    QCOMPARE(snapshot.display().belongsToAcceptedPresentationTarget(), false);
    QVERIFY(snapshot.display().displayedPresentationRevision().isValid());
    QVERIFY(snapshot.display().targetPresentationRevision().isValid());
    QVERIFY(snapshot.display().displayedPresentationRevision()
        != snapshot.display().targetPresentationRevision());
    QCOMPARE(snapshot.display().displayedRoleSet(), ImageViewportRoleSet(true, false));
    QVERIFY(snapshot.display().displayedPresentationTargetGeneration().isValid());
    QVERIFY(snapshot.display().displayedPresentationTargetGeneration()
        != snapshot.request().acceptedPresentationTargetGeneration());
    QCOMPARE(snapshot.primary().sequence(), loadingResult->sequence());
    QCOMPARE(snapshot.primary().request().frame(), -1);
    QCOMPARE(snapshot.primary().metadata().available(), false);
    QCOMPARE(snapshot.primary().display().retained(), true);
    QCOMPARE(snapshot.primary().display().belongsToAcceptedPresentationTarget(), false);
    QCOMPARE(snapshot.primary().display().currentForDemand(), false);
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
    QCOMPARE(item.setPresentation(rotation).outcome(), ImageViewportCommandOutcome::Accepted);
    const ImageViewportStateSnapshot committed = item.state();

    PresentationTargetTransitionPolicy policy;
    policy.setRotationTransition(PresentationTargetTransitionPolicy::RotationTransition::Reset);
    item.setPresentationTarget(ImageViewportPresentationTarget(second->sequence()), policy);
    const ImageViewportStateSnapshot retained = item.state();

    QCOMPARE(retained.display().status(), ImageViewportDisplayStatus::Retained);
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
    ImageFrame frame(payload, QSizeF(16.0, 8.0), QSizeF(8.0, 4.0), QSizeF(0.5, 0.5),
        payload.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QStringLiteral("argb32"));
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
    QCOMPARE(display.quality(), ImageViewportPayloadQuality::Preview);
    QCOMPARE(display.exactness(), ImageViewportPayloadExactness::NotExact);
}

void ImageViewportStateSnapshotTest::
    requireExactRejectsNewInMemoryPayloadButPreservesCommittedPixels()
{
    QImage payload(8, 4, QImage::Format_ARGB32_Premultiplied);
    payload.fill(Qt::transparent);
    ImageFrame frame(payload, QSizeF(16.0, 8.0), QSizeF(8.0, 4.0), QSizeF(0.5, 0.5),
        payload.sizeInBytes(), ImageViewportPayloadQuality::Preview,
        ImageViewportPayloadExactness::NotExact, true, ImageFrame::OrientationPolicy::Identity,
        QStringLiteral("preview/argb32"));
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport rejectingItem;
    rejectingItem.setSize(QSizeF(100.0, 100.0));
    ImageViewportPresentationCommand requireExact;
    requireExact.setExactnessPreference(ImageViewportExactnessPreference::RequireExact);
    QCOMPARE(rejectingItem.setPresentation(requireExact).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(rejectingItem
                 .setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(rejectingItem.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(
        rejectingItem.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(rejectingItem.state().display().status(), ImageViewportDisplayStatus::Empty);

    QImage exactPayload(16, 8, QImage::Format_ARGB32_Premultiplied);
    exactPayload.fill(Qt::black);
    ImageFrame exactFrame(exactPayload);
    QScopedPointer<ImageSequenceFactoryResult> exactResult(factory.fromFrame(&exactFrame));
    QVERIFY(exactResult->sequence());
    ImageViewport spreadItem;
    spreadItem.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(
        spreadItem.setPresentation(requireExact).outcome(), ImageViewportCommandOutcome::Accepted);
    QCOMPARE(spreadItem
                 .setPresentationTarget(
                     ImageViewportPresentationTarget(exactResult->sequence(), result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(spreadItem.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(spreadItem.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(spreadItem.state().request().acceptedRoleSet(), ImageViewportRoleSet(true, true));
    QCOMPARE(spreadItem.state().display().status(), ImageViewportDisplayStatus::Empty);
    QCOMPARE(spreadItem.state().display().displayedRoleSet(), ImageViewportRoleSet(false, false));

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(
        sessionCount, metadataRequestCount, frameRequestCount, lastRequestedFrame, closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());
    ImageViewport mixedSpreadItem;
    mixedSpreadItem.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(mixedSpreadItem.setPresentation(requireExact).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(mixedSpreadItem
                 .setPresentationTarget(ImageViewportPresentationTarget(
                                            result->sequence(), providerResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(mixedSpreadItem.state().request().status(), ImageViewportRequestStatus::Unsupported);
    QCOMPARE(
        mixedSpreadItem.state().request().reason(), ImageViewportRequestReason::PayloadRejection);
    QCOMPARE(*sessionCount, 0);

    ImageViewport committedItem;
    committedItem.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(committedItem
                 .setPresentationTarget(ImageViewportPresentationTarget(result->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewportCommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(committedItem);
    const auto committedDisplay = committedItem.state().primary().display();
    QCOMPARE(committedDisplay.exactness(), ImageViewportPayloadExactness::NotExact);

    QCOMPARE(committedItem.setPresentation(requireExact).outcome(),
        ImageViewportCommandOutcome::Accepted);
    QCOMPARE(committedItem.state().request().status(), ImageViewportRequestStatus::Ready);
    QCOMPARE(committedItem.state().request().reason(), ImageViewportRequestReason::Ready);
    QCOMPARE(committedItem.state().display().status(), ImageViewportDisplayStatus::Ready);
    QCOMPARE(committedItem.state().primary().display(), committedDisplay);
    QCOMPARE(committedItem.state().primary().display().currentForDemand(), true);
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
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Error);
    QCOMPARE(snapshot.request().reason(), ImageViewportRequestReason::ProviderFailure);
    QCOMPARE(snapshot.display().status(), ImageViewportDisplayStatus::Empty);
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
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Ready);
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewportPlaybackPhase::Stopped);
    QVERIFY(!snapshot.request().playbackRole().isValid());
    QCOMPARE(snapshot.primary().request().frame(), 0);
    QCOMPARE(snapshot.primary().request().position(), 0);

    QCOMPARE(
        item.play(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    snapshot = item.state();
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewportPlaybackPhase::Playing);
    QCOMPARE(snapshot.request().playbackRole().value<ImageViewportPageRole>(),
        ImageViewportPageRole::Primary);
    QCOMPARE(snapshot.primary().request().frame(), 0);
    QCOMPARE(snapshot.primary().request().position(), 0);

    QCOMPARE(item.pause(ImageViewportPageRole::Primary).outcome(),
        ImageViewportCommandOutcome::Accepted);
    snapshot = item.state();
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewportPlaybackPhase::Paused);
    QCOMPARE(snapshot.request().playbackRole().value<ImageViewportPageRole>(),
        ImageViewportPageRole::Primary);

    QCOMPARE(
        item.stop(ImageViewportPageRole::Primary).outcome(), ImageViewportCommandOutcome::Accepted);
    snapshot = item.state();
    QCOMPARE(snapshot.request().playbackPhase(), ImageViewportPlaybackPhase::Stopped);
    QVERIFY(!snapshot.request().playbackRole().isValid());
    QCOMPARE(snapshot.request().status(), ImageViewportRequestStatus::Ready);
}

void ImageViewportStateSnapshotTest::presentationOnlyChangesUpdateSnapshot()
{
    ImageViewport item;
    const ImageViewportStateSnapshot before = item.state();
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    ImageViewportPresentationCommand smoothingCommand;
    smoothingCommand.setSmoothing(!before.presentation().smoothing());
    QCOMPARE(
        item.setPresentation(smoothingCommand).outcome(), ImageViewportCommandOutcome::Accepted);
    const ImageViewportStateSnapshot after = item.state();

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(after.request(), before.request());
    QCOMPARE(after.display().status(), before.display().status());
    QCOMPARE(after.presentation().smoothing(), !before.presentation().smoothing());
    QVERIFY(after != before);

    smoothingCommand = {};
    smoothingCommand.setSmoothing(after.presentation().smoothing());
    QCOMPARE(
        item.setPresentation(smoothingCommand).outcome(), ImageViewportCommandOutcome::Accepted);
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

    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Invalid);

    command = {};
    command.setFitMode(ImageViewportFitMode::Manual);
    command.setManualZoomPercent(200.0);
    command.setQualityPreference(ImageViewportQualityPreference::BalancedDetail);
    command.setExactnessPreference(ImageViewportExactnessPreference::PreferExact);
    QCOMPARE(item.setPresentation(command).outcome(), ImageViewportCommandOutcome::Accepted);

    const ImageViewportStateSnapshot snapshot = item.state();
    QCOMPARE(snapshot.presentation().fitMode(), ImageViewportFitMode::Manual);
    QCOMPARE(snapshot.presentation().zoomPercent(), 200.0);
    QCOMPARE(snapshot.presentation().qualityPreference(),
        ImageViewportQualityPreference::BalancedDetail);
    QCOMPARE(snapshot.presentation().exactnessPreference(),
        ImageViewportExactnessPreference::PreferExact);
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
        && state.revisions.request.valid
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
