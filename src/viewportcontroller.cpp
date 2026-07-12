#include "viewportcontroller_p.h"
#include "viewportenginetestaccess_p.h"

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
    return engine.publish(engine.preparePublication(std::move(changes)));
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

const ImageViewportInternal::PlaybackState& ViewportController::playbackState() const
{
    return ViewportEngineTestAccess::playback(engine);
}

ViewportEngine::CommandDiagnostics ViewportController::commandDiagnostics() const
{
    return engine.commandDiagnostics();
}

quint64 ViewportController::publishedCommandRevision() const
{
    return ViewportEngineTestAccess::publishedCommandRevision(engine);
}

#endif

ViewportCommandResult ViewportController::assignSequence(ViewportSequenceAssignment assignment)
{
    if (assignment.presentationTarget.isClear()) {
        ImageSequence* const primary
            = assignment.source.sequence ? assignment.source.sequence : assignment.sequence;
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

    const auto engineResult = engine.assignPresentationTarget({ assignment.presentationTarget,
        assignment.transitionPolicy, std::move(assignment.source),
        std::move(assignment.secondarySourceHandle), engine.acceptedGeometryInput(itemBounds()) });
    ViewportCommandResult result
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(engineResult.command);
    mergeChanges(result.transition.changes, engineResult.changes);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[0], ImageViewport::PageRole::Primary);
    appendProviderTransport(result.transition.providerAfterPublication,
        engineResult.providerEffects[1], ImageViewport::PageRole::Secondary);
    for (auto& effect : engineResult.providerSessionOpenEffects) {
        if (effect.openSession) {
            result.transition.providerAfterPublication.append(std::move(effect.command));
        }
    }
    result.transition.playbackSchedule = engineResult.schedule;
    return result;
}

ViewportCommandResult ViewportController::clear()
{
    return assignSequence({});
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::setNextProviderRequestTokenForTest(quint64 token)
{
    ViewportEngineTestAccess::providerRequests(
        engine, ImageViewport::PageRole::Primary).nextRequestToken
        = token;
}

void ViewportController::setNextProviderRequestTokenForTest(
    ImageViewport::PageRole role, quint64 token)
{
    if (role == ImageViewport::PageRole::Secondary) {
        ViewportEngineTestAccess::providerRequests(engine, ImageViewport::PageRole::Secondary)
            .nextRequestToken
            = token;
        return;
    }

    setNextProviderRequestTokenForTest(token);
}

void ViewportController::setNextRevisionTokenForTest(quint64 token)
{
    engine.setNextRevisionValueForTest(token);
    ViewportEngineTestAccess::display(engine).revision = 0;
    ViewportEngineTestAccess::request(engine).requestRevision = 0;
    ViewportEngineTestAccess::publishedCommandRevision(engine) = 0;
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
