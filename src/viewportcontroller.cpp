#include "viewportcontroller_p.h"

#include "viewportcommandoutcome_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerhelpers_p.h"
#include "viewportcontrollerprovidercontract_p.h"

#include <limits>
#include <optional>
#include <utility>


ViewportController::ViewportController(std::function<QRectF()> captureItemBounds)
    : captureItemBounds(std::move(captureItemBounds))
{
}

QRectF ViewportController::itemBounds() const
{
    return captureItemBounds ? captureItemBounds() : QRectF {};
}

ImageViewportStateSnapshot ViewportController::stateSnapshot(double devicePixelRatio) const
{
    return engine.snapshot({ engine.acceptedGeometryInput(itemBounds(), devicePixelRatio),
        engine.projectedGeometryInput(itemBounds(), devicePixelRatio) });
}

ImageViewportInternal::ViewportChangeSet ViewportController::publishChanges(
    ImageViewportInternal::ViewportChangeSet changes)
{
    return engine.publishChanges(changes);
}

const ImageViewportInternal::PresentationState& ViewportController::presentationState() const
{
    return engine.presentationState();
}

const ImageViewportInternal::DisplayState& ViewportController::displayState() const
{
    return ViewportEngineStateAccess::display(engine);
}

const ImageViewportInternal::RequestState& ViewportController::requestState() const
{
    return ViewportEngineStateAccess::request(engine);
}

bool ViewportController::hasProviderSession() const
{
    return ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Primary).sessionActive;
}


bool ViewportController::providerMetadataReady() const
{
    return ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Primary).metadataReady;
}

bool ViewportController::secondaryProviderMetadataReady() const
{
    return ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Secondary).metadataReady;
}

ViewportSequenceAssignmentResult ViewportController::assignSequence(
    ViewportSequenceAssignment assignment)
{
    if (assignment.presentationTarget.isClear()) {
        ImageSequence* const primary = assignment.source.sequence
            ? assignment.source.sequence
            : assignment.sequence;
        ImageSequence* const secondary = assignment.secondarySourceHandle.sequence
            ? assignment.secondarySourceHandle.sequence
            : assignment.secondarySequence;
        if (primary) {
            assignment.presentationTarget = ImageViewportPresentationTarget(primary, secondary);
        }
    }
    if (assignment.source.sequence && !assignment.source.facts.present) {
        assignment.source = ImageViewportInternal::makeImageSequenceSource(
            assignment.source.sequence, std::move(assignment.source.owner));
    }
    if (assignment.secondarySourceHandle.sequence
        && !assignment.secondarySourceHandle.facts.present) {
        assignment.secondarySourceHandle = ImageViewportInternal::makeImageSequenceSource(
            assignment.secondarySourceHandle.sequence,
            std::move(assignment.secondarySourceHandle.owner));
    }
    if (!assignment.source.sequence && assignment.sequence) {
        assignment.source = ImageViewportInternal::makeImageSequenceSource(assignment.sequence);
    }
    if (!assignment.secondarySourceHandle.sequence && assignment.secondarySequence) {
        assignment.secondarySourceHandle
            = ImageViewportInternal::makeImageSequenceSource(assignment.secondarySequence);
    }

    const auto engineResult = engine.assignPresentationTarget(
        { assignment.presentationTarget, assignment.transitionPolicy,
            std::move(assignment.source), std::move(assignment.secondarySourceHandle),
            engine.acceptedGeometryInput(itemBounds()) });
    const ViewportCommandResult command
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(engineResult.command);
    ViewportSequenceAssignmentResult result;
    result.outcome = command.outcome;
    result.changes = command.changes;
    mergeChanges(result.changes, engineResult.changes);
    result.providerFrameTransport = engineResult.providerEffects[0];
    result.secondaryProviderFrameTransport = engineResult.providerEffects[1];
    result.openProviderSession = engineResult.openPrimaryProviderSession;
    result.openSecondaryProviderSession = engineResult.openSecondaryProviderSession;
    return result;
}

ViewportCommandResult ViewportController::clear()
{
    ViewportSequenceAssignmentResult assignment = assignSequence({});
    ViewportCommandResult result;
    result.outcome = assignment.outcome;
    result.changes = assignment.changes;
    result.providerFrameTransport = assignment.providerFrameTransport;
    result.secondaryProviderFrameTransport = assignment.secondaryProviderFrameTransport;
    result.playbackSchedule = engine.playbackScheduleEffect();
    return result;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::setNextProviderRequestTokenForTest(quint64 token)
{
    ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Primary).nextRequestToken = token;
}

void ViewportController::setNextProviderRequestTokenForTest(
    ImageViewport::PageRole role, quint64 token)
{
    if (role == ImageViewport::PageRole::Secondary) {
        ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Secondary).nextRequestToken = token;
        return;
    }

    setNextProviderRequestTokenForTest(token);
}

void ViewportController::setNextRevisionTokenForTest(quint64 token)
{
    engine.setNextRevisionValueForTest(token);
    ViewportEngineStateAccess::display(engine).revision = 0;
    ViewportEngineStateAccess::request(engine).requestRevision = 0;
    ViewportEngineStateAccess::request(engine).commandRevision = 0;
}

bool ViewportController::hasPendingRenderCommitForTest() const
{
    return ViewportEngineStateAccess::display(engine).roles[0].pendingRenderPayload.commitPending;
}

quint64 ViewportController::activeRequestIdForTest() const
{
    return ViewportEngineStateAccess::request(engine).roles[0].activeRequest.identity.id;
}

quint64 ViewportController::displayedRequestIdForTest() const
{
    return ViewportEngineStateAccess::display(engine).roles[0].displayedRequest.request.identity.id;
}

quint64 ViewportController::pendingRenderGenerationForTest() const
{
    return ViewportEngineStateAccess::display(engine).roles[0].pendingRenderPayload.generation;
}

quint64 ViewportController::pendingRenderPayloadIdForTest() const
{
    return ViewportEngineStateAccess::display(engine).roles[0].pendingRenderPayload.payloadId;
}

quint64 ViewportController::secondaryPendingRenderPayloadIdForTest() const
{
    return ViewportEngineStateAccess::display(engine).roles[1].pendingRenderPayload.payloadId;
}

ImageViewportInternal::RenderFailureDiagnostic
ViewportController::lastAcceptedRenderFailureDiagnosticForTest() const
{
    return ViewportEngineStateAccess::request(engine).lastAcceptedRenderFailure;
}
#endif
