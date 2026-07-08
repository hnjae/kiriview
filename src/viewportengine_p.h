#pragma once

#include "imageviewport.h"

class ViewportEngine
{
public:
    struct CommandDiagnostics
    {
        ImageViewport::CommandReason reason = ImageViewport::CommandReason::NoCommand;
        RevisionToken revision;
    };

    struct CommandResult
    {
        ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
        ImageViewport::CommandReason reason = ImageViewport::CommandReason::NoCommand;
        RevisionToken commandRevision;
        bool commandRevisionChanged = false;
    };

    ImageViewportStateSnapshot snapshot() const;
    CommandDiagnostics commandDiagnostics() const;

    CommandResult rejectInvalidCommand();
    CommandResult rejectMalformedEnumCommand();
    CommandResult clearFromEmpty();
    CommandResult validatePresentationNoop(ImageViewport::FitMode mode);

private:
    CommandResult rejected(
        ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason);
    CommandResult accepted();
    RevisionToken nextCommandRevision();

    quint64 m_nextRevision = 0;
    ImageViewport::CommandReason m_commandReason = ImageViewport::CommandReason::NoCommand;
    RevisionToken m_commandRevision;
};
