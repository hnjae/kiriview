#include "imageviewport.h"
#include "viewportcontroller_p.h"

#include <QtTest/QTest>

#include <memory>

namespace {

class PresentationControllerContext final : public ViewportControllerContext
{
public:
    ImageSequence* sequence = nullptr;
    bool readyDisplay = false;
    QSizeF itemSize { 100.0, 100.0 };
    QSizeF logicalSize { 16.0, 8.0 };

    QRectF itemBounds() const override
    {
        if (itemSize.width() <= 0.0 || itemSize.height() <= 0.0) {
            return {};
        }
        return QRectF(0.0, 0.0, itemSize.width(), itemSize.height());
    }

    bool hasActiveRequest() const override { return sequence != nullptr; }
    bool hasReadyDisplay() const override { return readyDisplay; }
    bool hasDisplayableSequence() const override { return sequence != nullptr; }
    QSizeF sequenceLogicalSize() const override { return logicalSize; }
    double width() const override { return itemSize.width(); }
    double height() const override { return itemSize.height(); }
};

std::unique_ptr<ImageSequenceFactoryResult> makeStillSequence(
    ImageSequenceFactory& factory, PresentationControllerContext& context)
{
    QImage image(context.logicalSize.toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    auto result = std::unique_ptr<ImageSequenceFactoryResult>(factory.fromFrame(&frame));
    if (!result || !result->sequence()) {
        return {};
    }
    context.sequence = result->sequence();
    return result;
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
};

void ViewportControllerPresentationTest::standalonePresentationCommandsMutateControllerState()
{
    PresentationControllerContext context;
    ViewportController controller(context);

    const ViewportCommandResult zoom = controller.setZoomPercent(250.0, QPointF());
    QCOMPARE(zoom.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::Manual);
    QCOMPARE(controller.presentationState().zoom, 2.5);
    QCOMPARE(zoom.changes.presentation, true);
    QCOMPARE(zoom.changes.displayRevision, true);
    QCOMPARE(zoom.changes.geometryState, false);

    const ViewportCommandResult rotation = controller.rotateClockwise(QPointF());
    QCOMPARE(rotation.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().rotationDegrees, 90);
    QCOMPARE(rotation.changes.presentation, true);

    const ViewportCommandResult mirror = controller.setMirrorHorizontally(true, QPointF());
    QCOMPARE(mirror.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().mirrorHorizontally, true);
    QCOMPARE(mirror.changes.presentation, true);
}

void ViewportControllerPresentationTest::
    presentationCommandsReportGeometryChangesWhenDisplayIsReady()
{
    ImageSequenceFactory factory;
    PresentationControllerContext context;
    std::unique_ptr<ImageSequenceFactoryResult> sequence = makeStillSequence(factory, context);
    QVERIFY(sequence);
    context.readyDisplay = true;
    ViewportController controller(context);
    controller.assignSequence({ sequence->sequence() });

    const ViewportCommandResult zoom = controller.setZoomPercent(1000.0, QPointF(50.0, 50.0));
    QCOMPARE(zoom.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(zoom.changes.geometryState, true);

    const ViewportCommandResult pan = controller.panBy(QPointF(4.0, 0.0));
    QCOMPARE(pan.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(pan.changes.geometryState, true);
    QVERIFY(controller.presentationState().pan.x() != 0.0);

    const ViewportCommandResult reset = controller.resetView();
    QCOMPARE(reset.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::Contain);
    QCOMPARE(controller.presentationState().zoom, 1.0);
    QCOMPARE(controller.presentationState().pan, QPointF());
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
    ViewportController controller(context);

    QCOMPARE(controller.setZoomPercent(300.0, QPointF()).outcome,
        ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(
        controller.rotateClockwise(QPointF()).outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(controller.setMirrorHorizontally(true, QPointF()).outcome,
        ImageViewport::CommandOutcome::Accepted);

    PageSetTransitionPolicy policy;
    policy.setZoomTransition(PageSetTransitionPolicy::ZoomTransition::Preserve);
    policy.setRotationTransition(PageSetTransitionPolicy::RotationTransition::Reset);
    policy.setMirrorTransition(PageSetTransitionPolicy::MirrorTransition::Reset);
    policy.setFitModeTransition(PageSetTransitionPolicy::FitModeTransition::SetExplicit);
    policy.setFitMode(ImageViewport::FitMode::FitHeight);
    policy.setSpreadDirectionTransition(
        PageSetTransitionPolicy::SpreadDirectionTransition::SetExplicit);
    policy.setSpreadDirection(ImageViewport::SpreadDirection::RightToLeft);
    policy.setPageGapTransition(PageSetTransitionPolicy::PageGapTransition::SetExplicit);
    policy.setPageGap(4.0);

    ViewportSequenceAssignment assignment;
    assignment.sequence = sequence->sequence();
    assignment.transitionPolicy = policy;
    const ViewportSequenceAssignmentResult result = controller.assignSequence(assignment);

    QCOMPARE(result.outcome, ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(result.changes.presentation, true);
    QCOMPARE(controller.presentationState().fitMode, ImageViewport::FitMode::FitHeight);
    QCOMPARE(controller.presentationState().zoom, 3.0);
    QCOMPARE(controller.presentationState().rotationDegrees, 0);
    QCOMPARE(controller.presentationState().mirrorHorizontally, false);
    QCOMPARE(controller.presentationState().spreadDirection,
        ImageViewport::SpreadDirection::RightToLeft);
    QCOMPARE(controller.presentationState().pageGap, 4.0);
}

QTEST_MAIN(ViewportControllerPresentationTest)

#include "tst_viewportcontroller_presentation.moc"
