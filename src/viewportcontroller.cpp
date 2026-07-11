#include "viewportcontroller_p.h"

#include "viewportcommandoutcome_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerhelpers_p.h"
#include "viewportcontrollerprovidercontract_p.h"
#include "viewportprovidertransporteffects_p.h"

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

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
const ImageViewportInternal::PresentationState& ViewportController::presentationState() const
{
    return engine.presentationState();
}

const ImageViewportInternal::DisplayState& ViewportController::displayState() const
{
    return ViewportEngineTestAccess::display(engine);
}

const ImageViewportInternal::RequestState& ViewportController::requestState() const
{
    return ViewportEngineTestAccess::request(engine);
}

bool ViewportController::hasProviderSession() const
{
    return ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).sessionActive;
}


bool ViewportController::providerMetadataReady() const
{
    return ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).metadataReady;
}

bool ViewportController::secondaryProviderMetadataReady() const
{
    return ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary).metadataReady;
}
#endif

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
    appendProviderTransport(
        result.afterChanges, engineResult.providerEffects[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(
        result.afterChanges, engineResult.providerEffects[1], ImageViewport::PageRole::Secondary);
    const auto appendOpen = [&](ImageViewport::PageRole role, bool open) {
        if (!open) {
            return;
        }
        const auto binding = engine.providerSessionBinding(role);
        ViewportProviderTransportCommand effect;
        effect.kind = ViewportProviderTransportCommand::Kind::OpenSession;
        effect.role = role;
        effect.sessionFactory = binding.factory;
        effect.threadingContract = binding.threadingContract;
        effect.generation = binding.generation;
        effect.sessionSerial = binding.sessionSerial;
        result.afterChanges.append(std::move(effect));
    };
    appendOpen(ImageViewport::PageRole::Primary, engineResult.openPrimaryProviderSession);
    appendOpen(ImageViewport::PageRole::Secondary, engineResult.openSecondaryProviderSession);
    return result;
}

ViewportCommandResult ViewportController::clear()
{
    ViewportSequenceAssignmentResult assignment = assignSequence({});
    ViewportCommandResult result;
    result.outcome = assignment.outcome;
    result.changes = assignment.changes;
    result.beforeChanges = assignment.beforeChanges;
    result.afterChanges = assignment.afterChanges;
    result.playbackSchedule = engine.playbackScheduleEffect();
    return result;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::setNextProviderRequestTokenForTest(quint64 token)
{
    ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Primary).nextRequestToken = token;
}

void ViewportController::setNextProviderRequestTokenForTest(
    ImageViewport::PageRole role, quint64 token)
{
    if (role == ImageViewport::PageRole::Secondary) {
        ViewportEngineTestAccess::provider(engine, ImageViewport::PageRole::Secondary).nextRequestToken = token;
        return;
    }

    setNextProviderRequestTokenForTest(token);
}

void ViewportController::setNextRevisionTokenForTest(quint64 token)
{
    engine.setNextRevisionValueForTest(token);
    ViewportEngineTestAccess::display(engine).revision = 0;
    ViewportEngineTestAccess::request(engine).requestRevision = 0;
    ViewportEngineTestAccess::request(engine).commandRevision = 0;
}

bool ViewportController::hasPendingRenderCommitForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.commitPending;
}

quint64 ViewportController::activeRequestIdForTest() const
{
    return ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.id;
}

quint64 ViewportController::displayedRequestIdForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[0].displayedRequest.request.identity.id;
}

quint64 ViewportController::pendingRenderGenerationForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.generation;
}

quint64 ViewportController::pendingRenderPayloadIdForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.payloadId;
}

quint64 ViewportController::secondaryPendingRenderPayloadIdForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.payloadId;
}

ImageViewportInternal::RenderFailureDiagnostic
ViewportController::lastAcceptedRenderFailureDiagnosticForTest() const
{
    return ViewportEngineTestAccess::request(engine).lastAcceptedRenderFailure;
}
#endif
