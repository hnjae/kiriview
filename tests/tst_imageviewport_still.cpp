#include "imageviewport_provider_test_support.h"

class ImageViewportStillTest : public QObject
{
    Q_OBJECT

public:
    explicit ImageViewportStillTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void resetViewWithoutRequestClearsTransformAndCommandDiagnostic();
    void resetViewWithoutTransformChangePreservesCommandDiagnostic();
    void resetViewResetsTransformsAndPreservesNonTransformPresentationState();
    void stillImageSequenceAssignmentPublishesReadyState();
    void nullSequenceAssignmentClearsDisplayObservations();
    void nullSequenceAssignmentClearsCommandDiagnostic();
    void clearActiveRequestClearsCommandDiagnostic();
    void clearPreservesPresentationState();
    void clearReadyDisplayClearsGeometryObservations();
    void clearNonPresentableDisplayKeepsEmptyGeometryObservations();
    void stillImageReadyReplacementIncrementsDisplayRevision();
    void stillImageReplacementPreservesPresentationState();
    void stillImageCommandsPreserveOrReplaceDocumentedState();
    void secondaryStillImagePlayReportsUnsupported();
    void secondaryStillImagePositionSeekReportsUnsupported();
    void coordinateHelpersRejectNonFiniteInputs();
    void stillImageMirroredCoverUsesMirroredVisibleImageRect();
    void stillImageCoverUsesBottomAlignmentAsCropFocus();
    void stillImageAssignmentWaitsForPositiveGeometry();
    void stillImageFactoryRejectsPublishedLimitViolations();
    void timedFrameListLoopingPlaybackWrapsToFirstFrame();
    void replacementRetainsPreviousDisplayWhileWaitingForGeometry();
};

static ImageViewport::CommandOutcome setPanDelta(ImageViewport& item, QPointF delta)
{
    ImageViewportPresentationCommand command;
    command.setPanDelta(delta);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setFitModeCommand(
    ImageViewport& item, ImageViewport::FitMode mode)
{
    ImageViewportPresentationCommand command;
    command.setFitMode(mode);
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

static ImageViewport::CommandOutcome setMirrorCommand(
    ImageViewport& item, bool mirrorHorizontally, bool mirrorVertically)
{
    ImageViewportPresentationCommand command;
    command.setMirrorHorizontally(mirrorHorizontally);
    command.setMirrorVertically(mirrorVertically);
    return item.setPresentation(command).outcome();
}

static ImageViewport::CommandOutcome setRotationCommand(ImageViewport& item, int degrees)
{
    ImageViewportPresentationCommand command;
    command.setRotationDegrees(degrees);
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

void ImageViewportStillTest::resetViewWithoutRequestClearsTransformAndCommandDiagnostic()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    const ImageViewportRevisionToken displayRevisionBeforeReset
        = revisionTokenProperty(item, "displayRevision");

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(item.resetView().outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 100.0);
    QCOMPARE(contentPosition(item), QPointF());
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "requestRevision").isValid());
    verifyRevisionChanged(item, "displayRevision", displayRevisionBeforeReset);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportStillTest::resetViewWithoutTransformChangePreservesCommandDiagnostic()
{
    ImageViewport item;
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");
    QVERIFY(!revisionTokenProperty(item, "displayRevision").isValid());

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);

    QCOMPARE(item.resetView().outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 100.0);
    QCOMPARE(contentPosition(item), QPointF());
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(revisionTokenProperty(item, "commandRevision"), commandRevision);
    QVERIFY(!revisionTokenProperty(item, "requestRevision").isValid());
    QVERIFY(!revisionTokenProperty(item, "displayRevision").isValid());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(stateSpy.count(), 0);
}

void ImageViewportStillTest::resetViewResetsTransformsAndPreservesNonTransformPresentationState()
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

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setRotationCommand(item, 90), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 250.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");

    QCOMPARE(item.resetView().outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(viewportPrimarySequence(item), result->sequence());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().rotationDegrees(), 0);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), false);
    QCOMPARE(item.state().presentation().mirrorVertically(), false);
    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 625.0);
    QCOMPARE(contentPosition(item), QPointF());
    QCOMPARE(item.state().presentation().looping(), true);
}

void ImageViewportStillTest::stillImageSequenceAssignmentPublishesReadyState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "UploadPending"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);

    acknowledgePendingRenderCommitForTest(item);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(primaryFrameCount(item), 1);
    QCOMPARE(primaryTotalDuration(item), -1);
    QCOMPARE(primaryFrameSeekBounds(item).minimum(), 0);
    QCOMPARE(primaryFrameSeekBounds(item).maximum(), 0);
    QCOMPARE(primaryPositionSeekBounds(item).minimum(), -1);
    QCOMPARE(primaryPositionSeekBounds(item).maximum(), -1);
    QCOMPARE(primaryTimedPlaybackSupport(item), ImageViewport::CapabilitySupport::False);
    QCOMPARE(primaryFrameSeekSupport(item), ImageViewport::CapabilitySupport::True);
    QCOMPARE(primaryPositionSeekSupport(item), ImageViewport::CapabilitySupport::False);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));

    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    const double changedWidth = 100.0 + 5.0e-13;
    QVERIFY(changedWidth != 100.0);
    item.setSize(QSizeF(changedWidth, 100.0));
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QCOMPARE(stateSpy.count(), 1);
    item.setSize(QSizeF(100.0, 100.0));

    const ImageViewportCoordinateResult centerImage = mapItemToPrimaryPage(item, 50.0, 50.0);
    QCOMPARE(centerImage.isValid(), true);
    QCOMPARE(centerImage.point().x(), 8.0);
    QCOMPARE(centerImage.point().y(), 4.0);

    const ImageViewportCoordinateResult rightEdgeImage = mapItemToPrimaryPage(item, 100.0, 50.0);
    verifyInvalidCoordinateResult(rightEdgeImage);
    const ImageViewportCoordinateResult bottomEdgeImage = mapItemToPrimaryPage(item, 50.0, 75.0);
    verifyInvalidCoordinateResult(bottomEdgeImage);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 4.0), true);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 16.0, 4.0), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 8.0), false);

    const ImageViewportCoordinateResult centerItem = mapPrimaryPageToItem(item, 8.0, 4.0);
    QCOMPARE(centerItem.isValid(), true);
    QCOMPARE(centerItem.point().x(), 50.0);
    QCOMPARE(centerItem.point().y(), 50.0);
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 8.0, 8.0));
}

void ImageViewportStillTest::nullSequenceAssignmentClearsDisplayObservations()
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
    const ImageViewportRevisionToken readyRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), -1);
    QCOMPARE(primaryRequestedPosition(item), -1);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
    verifyRevisionChanged(item, "requestRevision", readyRequestRevision);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);

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

    item.setPresentationTarget(ImageViewportPresentationTarget(providerResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    verifyRequestStatusReasonPair(item);
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(primaryDisplayedPosition(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
}

void ImageViewportStillTest::nullSequenceAssignmentClearsCommandDiagnostic()
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    item.setPresentationTarget(
        ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});

    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportStillTest::clearActiveRequestClearsCommandDiagnostic()
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    const ImageViewportRevisionToken commandRevision
        = revisionTokenProperty(item, "commandRevision");

    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    verifyRevisionChanged(item, "commandRevision", commandRevision);
    QCOMPARE(stateSpy.count(), 1);
}

void ImageViewportStillTest::clearPreservesPresentationState()
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
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::SolidColor, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 250.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.clear().outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(viewportPrimarySequence(item), nullptr);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), true);
    QCOMPARE(item.state().presentation().mirrorVertically(), true);
    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.state().presentation().zoomPercent(), 250.0);
    QCOMPARE(contentPosition(item), QPointF());
    QCOMPARE(item.state().presentation().looping(), true);
}

void ImageViewportStillTest::clearReadyDisplayClearsGeometryObservations()
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
    QCOMPARE(contentRect(item), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 16.0, 8.0));

    item.setPresentationTarget(
        ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});

    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleImageRect(item), QRectF());
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
}

void ImageViewportStillTest::clearNonPresentableDisplayKeepsEmptyGeometryObservations()
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
    item.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleImageRect(item), QRectF());

    item.setPresentationTarget(
        ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});

    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(visibleImageRect(item), QRectF());
}

void ImageViewportStillTest::stillImageReadyReplacementIncrementsDisplayRevision()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QVERIFY(firstResult->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    const ImageViewportRevisionToken readyRequestRevision
        = revisionTokenProperty(item, "requestRevision");

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    verifyRevisionChanged(item, "requestRevision", readyRequestRevision);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
}

void ImageViewportStillTest::stillImageReplacementPreservesPresentationState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QVERIFY(firstResult->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(
        factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(ImageViewportPresentationTarget(firstResult->sequence()),
        PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(setQualityTogglesCommand(item, false, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, true), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setBackgroundCommand(
                 item, ImageViewport::BackgroundMode::Checkerboard, QColor(20, 40, 60, 255)),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setManualZoomPercentCommand(item, 150.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);

    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(item.state().presentation().smoothing(), false);
    QCOMPARE(item.state().presentation().mipmap(), true);
    QCOMPARE(item.state().presentation().mirrorHorizontally(), true);
    QCOMPARE(item.state().presentation().mirrorVertically(), true);
    QCOMPARE(
        item.state().presentation().backgroundMode(), ImageViewport::BackgroundMode::Checkerboard);
    QCOMPARE(item.state().presentation().backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Manual);
    QCOMPARE(item.state().presentation().zoomPercent(), 150.0);
    QCOMPARE(contentPosition(item), QPointF());
    QCOMPARE(item.state().presentation().looping(), true);
}

void ImageViewportStillTest::stillImageCommandsPreserveOrReplaceDocumentedState()
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

    const ImageViewportRevisionToken readyRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "requestRevision", readyRequestRevision);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.pause(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.stop(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QVERIFY(!revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    const ImageViewportRevisionToken afterAcceptedSeekRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), afterAcceptedSeekRequestRevision);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 2).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), afterAcceptedSeekRequestRevision);
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), afterAcceptedSeekRequestRevision);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());

    QCOMPARE(setManualZoomPercentCommand(item, 200.0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPanDelta(item, QPointF(4.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.resetView().outcome(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.state().presentation().fitMode(), ImageViewport::FitMode::Contain);
    QCOMPARE(item.state().presentation().zoomPercent(), 625.0);
    QCOMPARE(contentPosition(item), QPointF());
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "NoCommand"));
    QVERIFY(revisionTokenProperty(item, "commandRevision").isValid());
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportStillTest::secondaryStillImagePlayReportsUnsupported()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(12, 8, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::black);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.play(ImageViewport::PageRole::Secondary).outcome(),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportStillTest::secondaryStillImagePositionSeekReportsUnsupported()
{
    ImageSequenceFactory factory;
    QImage primaryImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    primaryImage.fill(Qt::transparent);
    ImageFrame primaryFrame(primaryImage);
    QScopedPointer<ImageSequenceFactoryResult> primaryResult(factory.fromFrame(&primaryFrame));
    QVERIFY(primaryResult->sequence());

    QImage secondaryImage(12, 8, QImage::Format_ARGB32_Premultiplied);
    secondaryImage.fill(Qt::black);
    ImageFrame secondaryFrame(secondaryImage);
    QScopedPointer<ImageSequenceFactoryResult> secondaryResult(factory.fromFrame(&secondaryFrame));
    QVERIFY(secondaryResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.setPresentationTarget(ImageViewportPresentationTarget(
                                            primaryResult->sequence(), secondaryResult->sequence()),
                     PresentationTargetTransitionPolicy {})
                 .outcome(),
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommitForTest(item);
    const QMetaObject* metaObject = item.metaObject();
    const ImageViewportRevisionToken requestRevision
        = revisionTokenProperty(item, "requestRevision");
    const ImageViewportRevisionToken displayRevision
        = revisionTokenProperty(item, "displayRevision");

    QCOMPARE(item.seekToPosition(ImageViewport::PageRole::Secondary, 0).outcome(),
        ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(
        commandReasonValue(item), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(revisionTokenProperty(item, "requestRevision"), requestRevision);
    QCOMPARE(revisionTokenProperty(item, "displayRevision"), displayRevision);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportStillTest::coordinateHelpersRejectNonFiniteInputs()
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

    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    verifyInvalidCoordinateResult(mapItemToPrimaryPage(item, infinity, 50.0));
    verifyInvalidCoordinateResult(mapItemToPrimaryPage(item, 50.0, nan));
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, infinity, 4.0));
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 8.0, nan));
    QCOMPARE(containsVisiblePrimaryPagePoint(item, infinity, 4.0), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, nan), false);
}

void ImageViewportStillTest::stillImageMirroredCoverUsesMirroredVisibleImageRect()
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
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitHeight),
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setPanDelta(item, QPointF(-50.0, 0.0)), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(setMirrorCommand(item, true, false), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(contentRect(item), QRectF(-100.0, 0.0, 200.0, 100.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 0.0, 8.0, 8.0));
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 7.999, 4.0), true);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 4.0), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 15.999, 4.0), false);

    const ImageViewportCoordinateResult leftItem = mapItemToPrimaryPage(item, 0.001, 50.0);
    QCOMPARE(leftItem.isValid(), true);
    QCOMPARE(leftItem.point().x(), 7.99992);

    const ImageViewportCoordinateResult leftHalfImage = mapPrimaryPageToItem(item, 4.0, 4.0);
    QCOMPARE(leftHalfImage.isValid(), true);
    QCOMPARE(leftHalfImage.point().x(), 50.0);
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 12.0, 4.0));
}

void ImageViewportStillTest::stillImageCoverUsesBottomAlignmentAsCropFocus()
{
    ImageSequenceFactory factory;
    QImage image(8, 16, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(setFitModeCommand(item, ImageViewport::FitMode::FitWidth),
        ImageViewport::CommandOutcome::Accepted);
    ImageViewportPresentationCommand scanCommand;
    scanCommand.setScanDirection(ImageViewport::ScanDirection::End);
    QCOMPARE(item.setPresentation(scanCommand).outcome(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(contentRect(item), QRectF(0.0, -100.0, 100.0, 200.0));
    QCOMPARE(visibleImageRect(item), QRectF(0.0, 8.0, 8.0, 8.0));
    QCOMPARE(mapItemToPrimaryPage(item, 50.0, 0.0).point().y(), 8.0);
    QVERIFY(mapItemToPrimaryPage(item, 50.0, 99.0).point().y() > 15.9);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 4.0, 7.999), false);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 4.0, 8.0), true);
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 4.0, 15.999), true);
}

void ImageViewportStillTest::stillImageAssignmentWaitsForPositiveGeometry()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(0.0, 100.0));
    item.setPresentationTarget(
        ImageViewportPresentationTarget(result->sequence()), PresentationTargetTransitionPolicy {});
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), -1);
    QCOMPARE(displayedImageSize(item), QSizeF(0.0, 0.0));
    QCOMPARE(contentRect(item), QRectF());
    verifyInvalidCoordinateResult(mapItemToPrimaryPage(item, 0.0, 0.0));

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Empty"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));

    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    item.setSize(QSizeF(0.0, 100.0));
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF());
    verifyInvalidCoordinateResult(mapPrimaryPageToItem(item, 8.0, 4.0));
    QCOMPARE(containsVisiblePrimaryPagePoint(item, 8.0, 4.0), false);
}

void ImageViewportStillTest::stillImageFactoryRejectsPublishedLimitViolations()
{
    ImageSequenceFactory factory;
    QImage oversized(
        ImageSequenceLimits::maximumLogicalWidth() + 1, 1, QImage::Format_ARGB32_Premultiplied);
    oversized.fill(Qt::transparent);
    ImageFrame frame(oversized);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("maximumLogicalWidth")));
}

void ImageViewportStillTest::timedFrameListLoopingPlaybackWrapsToFirstFrame()
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
    QCOMPARE(setLoopingCommand(item, true), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject* metaObject = item.metaObject();

    QCOMPARE(item.play(ImageViewport::PageRole::Primary).outcome(), ImageViewport::CommandOutcome::Accepted);
    advancePlaybackForTest(item, 350);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, -1).outcome(), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(commandReasonValue(item), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(primaryRequestedPosition(item), 0);
    QCOMPARE(primaryDisplayedPosition(item), 0);

    advancePlaybackForTest(item, 100);
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(playbackPhaseValue(item), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(primaryRequestedFrame(item), 1);
    QCOMPARE(primaryDisplayedFrame(item), 1);
    QCOMPARE(primaryRequestedPosition(item), 100);
    QCOMPARE(primaryDisplayedPosition(item), 100);
}

void ImageViewportStillTest::replacementRetainsPreviousDisplayWhileWaitingForGeometry()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage replacementImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame replacementFrame(replacementImage);
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
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));

    item.setSize(QSizeF(0.0, 100.0));
    const ImageViewportRevisionToken readyDisplayRevision
        = revisionTokenProperty(item, "displayRevision");
    QSignalSpy stateSpy(&item, &ImageViewport::stateChanged);
    item.setPresentationTarget(ImageViewportPresentationTarget(replacementResult->sequence()),
        PresentationTargetTransitionPolicy {});
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(primaryRequestedFrame(item), 0);
    QCOMPARE(primaryDisplayedFrame(item), 0);
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    QCOMPARE(contentRect(item), QRectF());
    QCOMPARE(mapItemToPrimaryPage(item, 1.0, 1.0).isValid(), false);
    verifyRevisionChanged(item, "displayRevision", readyDisplayRevision);
    QCOMPARE(stateSpy.count(), 1);

    const ImageViewportRevisionToken retainedRequestRevision
        = revisionTokenProperty(item, "requestRevision");
    QCOMPARE(item.seek(ImageViewport::PageRole::Primary, 0).outcome(), ImageViewport::CommandOutcome::Accepted);
    verifyRevisionChanged(item, "requestRevision", retainedRequestRevision);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(displayedImageSize(item), QSizeF(16.0, 8.0));
    const ImageViewportRevisionToken retainedDisplayRevision
        = revisionTokenProperty(item, "displayRevision");

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Retained"));
    acknowledgePendingRenderCommitForTest(item);
    QCOMPARE(requestStatusValue(item), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(requestReasonValue(item), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(displayStatusValue(item), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(displayedImageSize(item), QSizeF(8.0, 8.0));
    QCOMPARE(contentRect(item), QRectF(0.0, 0.0, 100.0, 100.0));
    verifyRevisionChanged(item, "displayRevision", retainedDisplayRevision);
}

QTEST_MAIN(ImageViewportStillTest)

#include "tst_imageviewport_still.moc"
