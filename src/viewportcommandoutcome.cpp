#include "viewportcommandoutcome_p.h"

#include "imageviewporttoken_p.h"
namespace ImageViewportInternal::CommandOutcome {

ViewportCommandResult fromEngineCommand(const ViewportEngine::CommandResult& command)
{
    ViewportCommandResult result;
    result.outcome = command.outcome;
    if (command.commandRevisionChanged) {
        result.transition.changes.commandRevision = true;
        result.transition.changes.commandRevisionValue
            = ImageViewportInternal::RevisionTokenPrivateAccess::value(command.commandRevision);
    }
    return result;
}

}
