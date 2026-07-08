#include "viewportcommandoutcome_p.h"

#include "imageviewporttoken_p.h"
#include "viewportcontroller_p.h"

namespace ImageViewportInternal::CommandOutcome {

void markAccepted(ViewportControllerPort& viewport, ViewportCommandResult& result)
{
    result.changes.commandRevision
        = viewport.requestState().clearCommandDiagnosticForAcceptedCommand()
        || result.changes.commandRevision;
}

void markRejected(ViewportControllerPort& viewport, ViewportCommandResult& result,
    ImageViewport::CommandReason reason)
{
    viewport.requestState().setCommandDiagnostic(reason);
    result.changes.commandRevision = true;
}

ViewportCommandResult accepted(ViewportControllerPort& viewport)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    markAccepted(viewport, result);
    return result;
}

ViewportCommandResult accepted(
    ViewportControllerPort& viewport, ImageViewportInternal::ViewportChangeSet changes)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    result.changes = changes;
    markAccepted(viewport, result);
    return result;
}

ViewportCommandResult rejected(ViewportControllerPort& viewport,
    ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason)
{
    ViewportCommandResult result;
    result.outcome = outcome;
    markRejected(viewport, result, reason);
    return result;
}

ViewportCommandResult invalid(ViewportControllerPort& viewport)
{
    return rejected(viewport, ImageViewport::CommandOutcome::Invalid,
        ImageViewport::CommandReason::InvalidRequest);
}

ViewportCommandResult unsupported(ViewportControllerPort& viewport)
{
    return rejected(viewport, ImageViewport::CommandOutcome::Unsupported,
        ImageViewport::CommandReason::UnsupportedRequest);
}

ViewportCommandResult ignoredNoRequest(ViewportControllerPort& viewport)
{
    return rejected(viewport, ImageViewport::CommandOutcome::IgnoredNoRequest,
        ImageViewport::CommandReason::IgnoredNoRequest);
}

ViewportCommandResult fromEngineCommand(
    ViewportControllerPort& viewport, const ViewportEngine::CommandResult& command)
{
    ViewportCommandResult result;
    result.outcome = command.outcome;
    if (command.commandRevisionChanged) {
        viewport.requestState().setCommandDiagnostic(command.reason);
        result.changes.commandRevision = true;
        result.changes.commandRevisionValue
            = ImageViewportInternal::RevisionTokenPrivateAccess::value(command.commandRevision);
    }
    return result;
}

}
