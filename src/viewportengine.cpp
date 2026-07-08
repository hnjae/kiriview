#include "viewportengine_p.h"

namespace {
bool fitModeValid(ImageViewport::FitMode mode)
{
    switch (mode) {
    case ImageViewport::FitMode::Contain:
    case ImageViewport::FitMode::FitWidth:
    case ImageViewport::FitMode::FitHeight:
    case ImageViewport::FitMode::Manual:
        return true;
    }
    return false;
}
}

ImageViewportStateSnapshot ViewportEngine::snapshot() const { return {}; }

ViewportEngine::CommandDiagnostics ViewportEngine::commandDiagnostics() const
{
    return { m_commandReason, m_commandRevision };
}

ViewportEngine::CommandResult ViewportEngine::rejectInvalidCommand()
{
    return rejected(
        ImageViewport::CommandOutcome::Invalid, ImageViewport::CommandReason::InvalidRequest);
}

ViewportEngine::CommandResult ViewportEngine::rejectMalformedEnumCommand()
{
    return rejectInvalidCommand();
}

ViewportEngine::CommandResult ViewportEngine::clearFromEmpty() { return accepted(); }

ViewportEngine::CommandResult ViewportEngine::validatePresentationNoop(ImageViewport::FitMode mode)
{
    if (!fitModeValid(mode)) {
        return rejectMalformedEnumCommand();
    }
    return accepted();
}

ViewportEngine::CommandResult ViewportEngine::rejected(
    ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason)
{
    m_commandReason = reason;
    m_commandRevision = nextCommandRevision();
    return { outcome, reason, m_commandRevision, true };
}

ViewportEngine::CommandResult ViewportEngine::accepted()
{
    const bool hadDiagnostic = m_commandReason != ImageViewport::CommandReason::NoCommand;
    m_commandReason = ImageViewport::CommandReason::NoCommand;
    if (hadDiagnostic) {
        m_commandRevision = nextCommandRevision();
        return { ImageViewport::CommandOutcome::Accepted, m_commandReason, m_commandRevision,
            true };
    }
    return { ImageViewport::CommandOutcome::Accepted, m_commandReason, m_commandRevision, false };
}

RevisionToken ViewportEngine::nextCommandRevision()
{
    ++m_nextRevision;
    return RevisionToken(m_nextRevision);
}
