#include "imageviewport.h"
#include "viewportcontroller_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerrendercontract_p.h"

#include <QtTest/QTest>

#include <cmath>
#include <limits>
#include <memory>

namespace {

class PresentationControllerContext final
{
public:
    ImageSequence* sequence = nullptr;
    bool readyDisplay = false;
    QSizeF itemSize { 100.0, 100.0 };
    QSizeF logicalSize { 16.0, 8.0 };
    QSizeF secondaryLogicalSize { 8.0, 8.0 };

    QRectF itemBounds() const
    {
        if (itemSize.width() <= 0.0 || itemSize.height() <= 0.0) {
            return {};
        }
        return QRectF(0.0, 0.0, itemSize.width(), itemSize.height());
    }

    bool hasActiveRequest() const { return sequence != nullptr; }
    bool hasReadyDisplay() const { return readyDisplay; }
    bool hasDisplayableSequence() const { return sequence != nullptr; }
    QSizeF sequenceLogicalSize() const { return logicalSize; }
    QSizeF secondarySequenceLogicalSize() const { return secondaryLogicalSize; }
    double width() const { return itemSize.width(); }
    double height() const { return itemSize.height(); }
};

std::unique_ptr<ImageSequenceFactoryResult> makeStillSequence(
    ImageSequenceFactory& factory, QSizeF logicalSize)
{
    QImage image(logicalSize.toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    return std::unique_ptr<ImageSequenceFactoryResult>(factory.fromFrame(&frame));
}

std::unique_ptr<ImageSequenceFactoryResult> makeStillSequence(
    ImageSequenceFactory& factory, PresentationControllerContext& context)
{
    auto result = makeStillSequence(factory, context.logicalSize);
    if (!result || !result->sequence()) {
        return {};
    }
    context.sequence = result->sequence();
    return result;
}

PresentationTargetTransitionPolicy replacementSpreadPolicy(
    PresentationTargetTransitionPolicy::ContentPositionTransition contentPositionTransition)
{
    PresentationTargetTransitionPolicy policy;
    policy.setContentPositionTransition(contentPositionTransition);
    policy.setSpreadDirectionTransition(
        PresentationTargetTransitionPolicy::SpreadDirectionTransition::SetExplicit);
    policy.setSpreadDirection(ImageViewport::SpreadDirection::RightToLeft);
    policy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(50.0);
    return policy;
}

ViewportSequenceAssignment replacementSpreadAssignment(ImageSequence* primary,
    ImageSequence* secondary, const PresentationTargetTransitionPolicy& policy)
{
    ViewportSequenceAssignment assignment;
    assignment.sequence = primary;
    assignment.secondarySequence = secondary;
    assignment.secondarySource.present = secondary != nullptr;
    assignment.secondarySource.frameCount = secondary ? 1 : -1;
    assignment.secondarySource.firstFramePosition = -1;
    assignment.transitionPolicy = policy;
    return assignment;
}

void acknowledgePendingRenderCommit(ViewportController& controller)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    if (!synchronization.pendingTargetCommit) {
        return;
    }
    const ImageViewportInternal::PreparedPayloadIdentity primaryPayload
        = controller.displayState().roles[0].pendingRenderPayload.identity().isValid()
        ? controller.displayState().roles[0].pendingRenderPayload.identity()
        : synchronization.preparedPayload.identity();
    QVector<ViewportRenderRolePayload> rolePayloads { { ImageViewport::PageRole::Primary,
        primaryPayload } };
    if (controller.requestState().roles[1].sequence
        && controller.requestState().roles[1].activeRequest.target.frame >= 0) {
        const ImageViewportInternal::PreparedPayloadIdentity secondaryPayload
            = controller.displayState().roles[1].pendingRenderPayload.identity().isValid()
            ? controller.displayState().roles[1].pendingRenderPayload.identity()
            : primaryPayload;
        rolePayloads.append({ ImageViewport::PageRole::Secondary, secondaryPayload });
    }
    controller.acknowledgeRenderCommit({ primaryPayload, rolePayloads }, true, synchronization);
}

} // namespace

class ViewportControllerPresentationTest : public QObject
{
    Q_OBJECT

public:
    explicit ViewportControllerPresentationTest(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

private slots:
    void standalonePresentationCommandsMutateControllerState();
    void presentationCommandsReportGeometryChangesWhenDisplayIsReady();
    void assignmentAppliesPresentationTransitionInControllerTransaction();
    void assignmentDerivesDisplayTransitionFromPolicy();
    void spreadDirectionAndPageGapPreserveCommandDiagnosticsForInvalidAndNoop();
    void presentationTargetTransitionScanStartUsesReplacementSpreadGeometry();
    void presentationTargetTransitionScanEndUsesReplacementSpreadGeometry();
    void presentationTargetTransitionClampUsesReplacementBounds();
    void manualZoomUsesDevicePixelRatioForTwoPageSpreadGeometry();
    void nearestVisibleHelpersUseDevicePixelRatioAdjustedManualGeometry();
    void manualZoomHelpersUseControllerPresentationGeometry();
    void zoomByStepUsesControllerStepMathAndValidation();
};

void ViewportControllerPresentationTest::standalonePresentationCommandsMutateControllerState()
{
    PresentationControllerContext context;
    ViewportController controller([&context] { return context.itemBounds(); });

    const ViewportCommandResult zoom = controller.setZoomPercent(250.0, QPointF());
    QCOMPARE(zoom.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::Manual);
    QCOMPARE(controller.presentationState().manualZoom, 2.5);
    QCOMPARE(zoom.changes.displayRevision, true);
    QCOMPARE(zoom.changes.geometryState, false);
    QCOMPARE(zoom.changes.scheduleUpdate, true);

    const ViewportCommandResult rotation = controller.rotateClockwise(QPointF());
    QCOMPARE(rotation.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().rotationDegrees, 90);
    QCOMPARE(rotation.changes.displayRevision, true);
    QCOMPARE(rotation.changes.scheduleUpdate, true);

    const ViewportCommandResult mirror = controller.setMirrorHorizontally(true, QPointF());
    QCOMPARE(mirror.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().mirrorHorizontally, true);
    QCOMPARE(mirror.changes.displayRevision, true);
    QCOMPARE(mirror.changes.scheduleUpdate, true);
}

void ViewportControllerPresentationTest::
    presentationCommandsReportGeometryChangesWhenDisplayIsReady()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeStillSequence(factory, context);
    QVERIFY(sequence);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });
    controller.assignSequence({ sequence->sequence() });
    acknowledgePendingRenderCommit(controller);

    const ViewportCommandResult zoom = controller.setZoomPercent(1000.0, QPointF(50.0, 50.0));
    QCOMPARE(zoom.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(zoom.changes.geometryState, true);

    const ViewportCommandResult pan = controller.panBy(QPointF(4.0, 0.0));
    QCOMPARE(pan.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(pan.changes.geometryState, true);
    QVERIFY(controller.presentationState().contentPosition.x() != 0.0);

    QCOMPARE(controller.rotateClockwise(QPointF(50.0, 50.0)).outcome,
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.setMirrorHorizontally(true, QPointF(50.0, 50.0)).outcome,
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.setMirrorVertically(true, QPointF(50.0, 50.0)).outcome,
        ImageViewport::CommandOutcome::Accepted);

    const ViewportCommandResult reset = controller.resetView();
    QCOMPARE(reset.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::Contain);
    QCOMPARE(controller.presentationState().manualZoom, 1.0);
    QCOMPARE(controller.presentationState().contentPosition, QPointF());
    QCOMPARE(controller.presentationState().rotationDegrees, 0);
    QCOMPARE(controller.presentationState().mirrorHorizontally, false);
    QCOMPARE(controller.presentationState().mirrorVertically, false);
    QCOMPARE(reset.changes.geometryState, true);
}

void ViewportControllerPresentationTest::
    assignmentAppliesPresentationTransitionInControllerTransaction()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeStillSequence(factory, context);
    QVERIFY(sequence);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });

    QCOMPARE(controller.setZoomPercent(300.0, QPointF()).outcome,
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        controller.rotateClockwise(QPointF()).outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.setMirrorHorizontally(true, QPointF()).outcome,
        ImageViewport::CommandOutcome::Accepted);

    PresentationTargetTransitionPolicy policy;
    policy.setZoomTransition(PresentationTargetTransitionPolicy::ZoomTransition::Preserve);
    policy.setRotationTransition(PresentationTargetTransitionPolicy::RotationTransition::Reset);
    policy.setMirrorTransition(PresentationTargetTransitionPolicy::MirrorTransition::Reset);
    policy.setFitModeTransition(PresentationTargetTransitionPolicy::FitModeTransition::SetExplicit);
    policy.setFitMode(ImageViewport::FitMode::FitHeight);
    policy.setSpreadDirectionTransition(
        PresentationTargetTransitionPolicy::SpreadDirectionTransition::SetExplicit);
    policy.setSpreadDirection(ImageViewport::SpreadDirection::RightToLeft);
    policy.setPageGapTransition(PresentationTargetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(4.0);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    assignment.transitionPolicy = policy;
    const ViewportSequenceAssignmentResult result = controller.assignSequence(assignment);

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.changes.displayRevision, true);
    QCOMPARE(result.changes.scheduleUpdate, true);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::FitHeight);
    QCOMPARE(controller.presentationState().manualZoom, 3.0);
    QCOMPARE(controller.presentationState().rotationDegrees, 0);
    QCOMPARE(controller.presentationState().mirrorHorizontally, false);
    QCOMPARE(controller.presentationState().spreadDirection,
        ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(controller.presentationState().pageGap, 4.0);
}

void ViewportControllerPresentationTest::assignmentDerivesDisplayTransitionFromPolicy()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> initial = makeStillSequence(factory, context);
    QVERIFY(initial);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });
    QCOMPARE(controller.assignSequence({ initial->sequence() }).outcome,
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.displayState().status, ImageViewport::DisplayStatus::Ready);

    context.logicalSize = QSizeF(32.0, 16.0);
    std::unique_ptr<ImageSequenceFactoryResult> replacement = makeStillSequence(factory, context);
    QVERIFY(replacement);
    context.sequence = replacement->sequence();

    PresentationTargetTransitionPolicy policy;
    policy.setDisplayTransition(
        PresentationTargetTransitionPolicy::DisplayTransition::ClearBeforeLoad);

    ViewportSequenceAssignment assignment;
    assignment.sequence = replacement->sequence();
    assignment.transitionPolicy = policy;
    const ViewportSequenceAssignmentResult result = controller.assignSequence(assignment);

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.displayState().status, ImageViewport::DisplayStatus::Empty);
    QCOMPARE(controller.displayState().roles[0].displayedImageSize, QSizeF());
    QCOMPARE(result.changes.displayRevision, true);
}

void ViewportControllerPresentationTest::
    spreadDirectionAndPageGapPreserveCommandDiagnosticsForInvalidAndNoop()
{
    PresentationControllerContext context;
    ViewportController controller([&context] { return context.itemBounds(); });

    const ViewportCommandResult invalidZoom
        = controller.setZoomPercent(std::numeric_limits<double>::infinity(), QPointF());
    QCOMPARE(invalidZoom.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(controller.requestState().commandReason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(invalidZoom.changes.commandRevision, true);
    const uint unchangedRevision = controller.requestState().commandRevision;

    const ViewportCommandResult invalidDirection
        = controller.setSpreadDirection(static_cast<ImageViewport::SpreadDirection>(-1));
    QCOMPARE(invalidDirection.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(controller.requestState().commandReason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(controller.requestState().commandRevision, unchangedRevision);
    QCOMPARE(invalidDirection.changes.commandRevision, true);
    QVERIFY(invalidDirection.changes.commandRevisionValue != 0);

    const ViewportCommandResult sameDirection
        = controller.setSpreadDirection(controller.presentationState().spreadDirection);
    QCOMPARE(sameDirection.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().commandReason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(controller.requestState().commandRevision, unchangedRevision);
    QCOMPARE(sameDirection.changes.commandRevision, false);

    const ViewportCommandResult invalidGap
        = controller.setPageGap(std::numeric_limits<double>::infinity());
    QCOMPARE(invalidGap.outcome, ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(controller.requestState().commandReason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(controller.requestState().commandRevision, unchangedRevision);
    QCOMPARE(invalidGap.changes.commandRevision, true);
    QVERIFY(invalidGap.changes.commandRevisionValue != 0);

    const ViewportCommandResult sameGap
        = controller.setPageGap(controller.presentationState().pageGap);
    QCOMPARE(sameGap.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().commandReason, ImageViewport::CommandReason::InvalidRequest);
    QCOMPARE(controller.requestState().commandRevision, unchangedRevision);
    QCOMPARE(sameGap.changes.commandRevision, false);

    const ViewportCommandResult changedDirection
        = controller.setSpreadDirection(ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(changedDirection.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.requestState().commandReason, ImageViewport::CommandReason::NoCommand);
    QCOMPARE(controller.requestState().commandRevision, unchangedRevision);
    QCOMPARE(changedDirection.changes.commandRevision, true);
}

void ViewportControllerPresentationTest::
    presentationTargetTransitionScanStartUsesReplacementSpreadGeometry()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    context.logicalSize = QSizeF(100.0, 100.0);
    std::unique_ptr<ImageSequenceFactoryResult> initial = makeStillSequence(factory, context);
    QVERIFY(initial);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });
    QCOMPARE(controller.assignSequence({ initial->sequence() }).outcome,
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.setZoomPercent(200.0, QPointF(50.0, 50.0)).outcome,
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.panToEnd().outcome, ImageViewport::CommandOutcome::Accepted);

    context.logicalSize = QSizeF(200.0, 100.0);
    context.secondaryLogicalSize = QSizeF(50.0, 100.0);
    std::unique_ptr<ImageSequenceFactoryResult> replacementPrimary
        = makeStillSequence(factory, context.logicalSize);
    std::unique_ptr<ImageSequenceFactoryResult> replacementSecondary
        = makeStillSequence(factory, context.secondaryLogicalSize);
    QVERIFY(replacementPrimary);
    QVERIFY(replacementSecondary);
    context.sequence = replacementPrimary->sequence();

    const PresentationTargetTransitionPolicy policy = replacementSpreadPolicy(
        PresentationTargetTransitionPolicy::ContentPositionTransition::ScanStart);
    const ViewportSequenceAssignmentResult result
        = controller.assignSequence(replacementSpreadAssignment(
            replacementPrimary->sequence(), replacementSecondary->sequence(), policy));

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    const PresentationGeometry::State geometry = controller.geometryState();
    QCOMPARE(PresentationGeometry::spreadSize(geometry), QSizeF(300.0, 100.0));
    QCOMPARE(PresentationGeometry::primaryPageRect(geometry), QRectF(100.0, 0.0, 200.0, 100.0));
    QCOMPARE(PresentationGeometry::secondaryPageRect(geometry), QRectF(0.0, 0.0, 50.0, 100.0));
    QCOMPARE(PresentationGeometry::contentPosition(geometry), QPointF(0.0, 0.0));
}

void ViewportControllerPresentationTest::
    presentationTargetTransitionScanEndUsesReplacementSpreadGeometry()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    context.logicalSize = QSizeF(100.0, 100.0);
    std::unique_ptr<ImageSequenceFactoryResult> initial = makeStillSequence(factory, context);
    QVERIFY(initial);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });
    QCOMPARE(controller.assignSequence({ initial->sequence() }).outcome,
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.setZoomPercent(200.0, QPointF(50.0, 50.0)).outcome,
        ImageViewport::CommandOutcome::Accepted);

    context.logicalSize = QSizeF(200.0, 100.0);
    context.secondaryLogicalSize = QSizeF(50.0, 100.0);
    std::unique_ptr<ImageSequenceFactoryResult> replacementPrimary
        = makeStillSequence(factory, context.logicalSize);
    std::unique_ptr<ImageSequenceFactoryResult> replacementSecondary
        = makeStillSequence(factory, context.secondaryLogicalSize);
    QVERIFY(replacementPrimary);
    QVERIFY(replacementSecondary);
    context.sequence = replacementPrimary->sequence();

    const PresentationTargetTransitionPolicy policy = replacementSpreadPolicy(
        PresentationTargetTransitionPolicy::ContentPositionTransition::ScanEnd);
    const ViewportSequenceAssignmentResult result
        = controller.assignSequence(replacementSpreadAssignment(
            replacementPrimary->sequence(), replacementSecondary->sequence(), policy));

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    const PresentationGeometry::State geometry = controller.geometryState();
    const QPointF maximum = PresentationGeometry::maximumContentPosition(geometry);
    QCOMPARE(maximum, QPointF(500.0, 100.0));
    QCOMPARE(PresentationGeometry::contentPosition(geometry), maximum);
}

void ViewportControllerPresentationTest::presentationTargetTransitionClampUsesReplacementBounds()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    context.logicalSize = QSizeF(100.0, 100.0);
    std::unique_ptr<ImageSequenceFactoryResult> initial = makeStillSequence(factory, context);
    QVERIFY(initial);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });
    QCOMPARE(controller.assignSequence({ initial->sequence() }).outcome,
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.setZoomPercent(200.0, QPointF(50.0, 50.0)).outcome,
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.panToEnd().outcome, ImageViewport::CommandOutcome::Accepted);

    context.logicalSize = QSizeF(200.0, 100.0);
    context.secondaryLogicalSize = QSizeF(50.0, 100.0);
    std::unique_ptr<ImageSequenceFactoryResult> replacementPrimary
        = makeStillSequence(factory, context.logicalSize);
    std::unique_ptr<ImageSequenceFactoryResult> replacementSecondary
        = makeStillSequence(factory, context.secondaryLogicalSize);
    QVERIFY(replacementPrimary);
    QVERIFY(replacementSecondary);
    context.sequence = replacementPrimary->sequence();

    const PresentationTargetTransitionPolicy policy = replacementSpreadPolicy(
        PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp);
    const ViewportSequenceAssignmentResult result
        = controller.assignSequence(replacementSpreadAssignment(
            replacementPrimary->sequence(), replacementSecondary->sequence(), policy));

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    const PresentationGeometry::State geometry = controller.geometryState();
    QCOMPARE(PresentationGeometry::maximumContentPosition(geometry), QPointF(500.0, 100.0));
    QCOMPARE(PresentationGeometry::contentPosition(geometry), QPointF(100.0, 100.0));
    QCOMPARE(PresentationGeometry::contentRect(geometry).topLeft(), QPointF(-100.0, -100.0));
}

void ViewportControllerPresentationTest::manualZoomUsesDevicePixelRatioForTwoPageSpreadGeometry()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    context.logicalSize = QSizeF(16.0, 8.0);
    context.secondaryLogicalSize = QSizeF(8.0, 8.0);
    std::unique_ptr<ImageSequenceFactoryResult> primary = makeStillSequence(factory, context);
    std::unique_ptr<ImageSequenceFactoryResult> secondary
        = makeStillSequence(factory, context.secondaryLogicalSize);
    QVERIFY(primary);
    QVERIFY(secondary);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });

    ViewportSequenceAssignment assignment;
    assignment.sequence = primary->sequence();
    assignment.secondarySequence = secondary->sequence();
    assignment.secondarySource.present = true;
    assignment.secondarySource.frameCount = 1;
    assignment.secondarySource.firstFramePosition = -1;
    QCOMPARE(
        controller.assignSequence(assignment).outcome, ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.setPageGap(4.0).outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.setZoomPercent(100.0, QPointF()).outcome,
        ImageViewport::CommandOutcome::Accepted);

    const PresentationGeometry::State geometry = controller.geometryState(2.0);
    QCOMPARE(geometry.devicePixelRatio, 2.0);
    QCOMPARE(PresentationGeometry::contentRect(geometry).size(), QSizeF(14.0, 4.0));
    QCOMPARE(PresentationGeometry::pageItemRect(geometry, ImageViewport::PageRole::Primary).size(),
        QSizeF(8.0, 4.0));
    QCOMPARE(
        PresentationGeometry::pageItemRect(geometry, ImageViewport::PageRole::Secondary).size(),
        QSizeF(4.0, 4.0));

    const ViewportRenderSynchronization synchronization
        = controller.beginRenderSynchronization(2.0);
    QCOMPARE(synchronization.geometryState.devicePixelRatio, 2.0);
    QCOMPARE(synchronization.renderSnapshot.imageLayers.size(), 2);
    QCOMPARE(synchronization.renderSnapshot.imageLayers.at(0).targetRect.size(), QSizeF(8.0, 4.0));
    QCOMPARE(synchronization.renderSnapshot.imageLayers.at(1).targetRect.size(), QSizeF(4.0, 4.0));
}

void ViewportControllerPresentationTest::
    nearestVisibleHelpersUseDevicePixelRatioAdjustedManualGeometry()
{
    PresentationGeometry::State geometry;
    geometry.hasReadyDisplay = true;
    geometry.itemBounds = QRectF(0.0, 0.0, 100.0, 100.0);
    geometry.primaryImageSize = QSizeF(100.0, 100.0);
    geometry.fitMode = ImageViewport::FitMode::Manual;
    geometry.manualZoom = 2.0;
    geometry.devicePixelRatio = 1.0;

    QCOMPARE(PresentationGeometry::visibleSpreadRect(geometry), QRectF(0.0, 0.0, 50.0, 50.0));
    const CoordinateResult dprOneNearest
        = PresentationGeometry::nearestVisibleSpreadPoint(geometry, 75.0, 75.0);
    QCOMPARE(dprOneNearest.isValid(), true);
    QCOMPARE(dprOneNearest.x(), std::nextafter(50.0, 0.0));
    QCOMPARE(dprOneNearest.y(), std::nextafter(50.0, 0.0));

    geometry.devicePixelRatio = 2.0;

    QCOMPARE(PresentationGeometry::visibleSpreadRect(geometry), QRectF(0.0, 0.0, 100.0, 100.0));
    const CoordinateResult dprTwoNearest
        = PresentationGeometry::nearestVisibleSpreadPoint(geometry, 75.0, 75.0);
    QCOMPARE(dprTwoNearest.isValid(), true);
    QCOMPARE(dprTwoNearest.x(), 75.0);
    QCOMPARE(dprTwoNearest.y(), 75.0);
}

void ViewportControllerPresentationTest::manualZoomHelpersUseControllerPresentationGeometry()
{
    const double displayDemandCeiling = ImageViewportDisplayLimits::maximumManualZoomPercent();
    const auto verifyClose = [](double actual, double expected) {
        QVERIFY2(qAbs(actual - expected) < 0.000001,
            qPrintable(QStringLiteral("actual %1 expected %2").arg(actual).arg(expected)));
    };
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeStillSequence(factory, context);
    QVERIFY(sequence);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });

    QCOMPARE(controller.maximumManualZoomPercent(2.0), displayDemandCeiling);
    QVERIFY(controller.minimumManualZoomPercent() > 0.0);
    QCOMPARE(controller.manualZoomStepFactor(), 1.25);
    QCOMPARE(controller.clampedManualZoomPercent(-1.0, 2.0), controller.minimumManualZoomPercent());

    QCOMPARE(controller.assignSequence({ sequence->sequence() }).outcome,
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.setFitMode(ImageViewport::FitMode::FitHeight, QPointF()).outcome,
        ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(controller.maximumManualZoomPercent(2.0), displayDemandCeiling);
    verifyClose(controller.steppedManualZoomPercent(0, 2.0), 2500.0);
    verifyClose(controller.steppedManualZoomPercent(1, 2.0), 3125.0);
    QCOMPARE(controller.steppedManualZoomPercent(std::numeric_limits<int>::max(), 2.0),
        displayDemandCeiling);

    context.itemSize = QSizeF(0.0, 100.0);
    QCOMPARE(controller.maximumManualZoomPercent(2.0), displayDemandCeiling);
}

void ViewportControllerPresentationTest::zoomByStepUsesControllerStepMathAndValidation()
{
    const double displayDemandCeiling = ImageViewportDisplayLimits::maximumManualZoomPercent();
    const auto verifyClose = [](double actual, double expected) {
        QVERIFY2(qAbs(actual - expected) < 0.000001,
            qPrintable(QStringLiteral("actual %1 expected %2").arg(actual).arg(expected)));
    };
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeStillSequence(factory, context);
    QVERIFY(sequence);
    context.readyDisplay = true;
    ViewportController controller([&context] { return context.itemBounds(); });

    QCOMPARE(controller.assignSequence({ sequence->sequence() }).outcome,
        ImageViewport::CommandOutcome::Accepted);
    acknowledgePendingRenderCommit(controller);
    QCOMPARE(controller.setFitMode(ImageViewport::FitMode::FitHeight, QPointF(50.0, 50.0)).outcome,
        ImageViewport::CommandOutcome::Accepted);

    const ViewportCommandResult stepResult = controller.zoomByStep(1, QPointF(50.0, 50.0), 2.0);
    QCOMPARE(stepResult.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::Manual);
    verifyClose(controller.presentationState().manualZoom, 31.25);
    verifyClose(controller.steppedManualZoomPercent(0, 2.0), 3125.0);

    const double manualBeforeInvalid = controller.presentationState().manualZoom;
    const ViewportCommandResult invalidResult
        = controller.zoomByStep(1, QPointF(std::numeric_limits<double>::infinity(), 50.0), 2.0);
    QCOMPARE(invalidResult.outcome, ImageViewport::CommandOutcome::Invalid);
    verifyClose(controller.presentationState().manualZoom, manualBeforeInvalid);

    const ViewportCommandResult largeResult
        = controller.zoomByStep(std::numeric_limits<int>::max(), QPointF(50.0, 50.0), 2.0);
    QCOMPARE(largeResult.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::Manual);
    QCOMPARE(controller.presentationState().manualZoom, displayDemandCeiling / 100.0);
}

QTEST_MAIN(ViewportControllerPresentationTest)

#include "tst_viewportcontroller_presentation.moc"
