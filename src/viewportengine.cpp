#include "viewportengine_p.h"

#include "imageviewporttoken_p.h"

#include <limits>

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

ViewportEngine::PageSetState ViewportEngine::pageSetState() const { return m_pageSetState; }

ImageViewportInternal::DisplayState& ViewportEngine::displayState() { return m_displayState; }

const ImageViewportInternal::DisplayState& ViewportEngine::displayState() const
{
    return m_displayState;
}

ImageViewportInternal::RequestState& ViewportEngine::requestState() { return m_requestState; }

const ImageViewportInternal::RequestState& ViewportEngine::requestState() const
{
    return m_requestState;
}

ImageViewportInternal::ProviderGenerationState& ViewportEngine::providerState()
{
    return m_providerState;
}

const ImageViewportInternal::ProviderGenerationState& ViewportEngine::providerState() const
{
    return m_providerState;
}

ImageViewportInternal::ProviderGenerationState& ViewportEngine::secondaryProviderState()
{
    return m_secondaryProviderState;
}

const ImageViewportInternal::ProviderGenerationState& ViewportEngine::secondaryProviderState() const
{
    return m_secondaryProviderState;
}

const ImageViewportInternal::PresentationState& ViewportEngine::presentationState() const
{
    return m_presentationState;
}

ImageViewportInternal::PresentationState& ViewportEngine::presentationState()
{
    return m_presentationState;
}

PresentationGeometry::State ViewportEngine::geometryState(const GeometryInput& input) const
{
    return geometryState(input, m_presentationState);
}

PresentationGeometry::State ViewportEngine::geometryState(
    const GeometryInput& input, const ImageViewportInternal::PresentationState& presentation) const
{
    return {
        input.primaryPresent,
        input.itemBounds,
        input.primarySize,
        input.secondarySize,
        presentation.pageGap,
        presentation.spreadDirection,
        presentation.fitMode,
        presentation.rotationDegrees,
        presentation.mirrorHorizontally,
        presentation.mirrorVertically,
        presentation.manualZoom,
        input.devicePixelRatio > 0.0 ? input.devicePixelRatio : 1.0,
        presentation.contentPosition,
    };
}

ViewportEngine::PageSetAssignmentResult ViewportEngine::assignPageSet(PageSetAssignmentInput input)
{
    if (!input.pageSet.isValid() || !input.transitionPolicy.isValid()) {
        return { rejectInvalidCommand(), m_pageSetState };
    }

    const bool clear = input.pageSet.isClear();
    const bool clearNoop = clear && m_pageSetState.acceptedRoleSet == ImageViewportRoleSet();
    const bool pageSetChanged = !clearNoop;
    PageSetAssignmentResult result;
    result.command = acceptedPreservingCommandDiagnostics();
    result.pageSetChanged = pageSetChanged;
    result.clear = clear;
    result.retainPreviousDisplay = input.transitionPolicy.displayTransition()
        == PageSetTransitionPolicy::DisplayTransition::RetainPrevious;
    result.releaseDisplayedState = clear || !result.retainPreviousDisplay;
    result.resetDisplayRequests = pageSetChanged;
    result.stopPlayback = pageSetChanged;
    result.closeProviderSessions = pageSetChanged;

    if (pageSetChanged) {
        const quint64 generation = nextPageSetGeneration();
        m_pageSetState = pageSetStateFor(input.pageSet, generation);
    }

    result.pageSetState = m_pageSetState;
    return result;
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

ViewportEngine::CommandResult ViewportEngine::acceptedPreservingCommandDiagnostics() const
{
    return { ImageViewport::CommandOutcome::Accepted, m_commandReason, m_commandRevision, false };
}

RevisionToken ViewportEngine::nextCommandRevision()
{
    return ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(allocateRevisionValue());
}

quint64 ViewportEngine::allocateRevisionValue()
{
    if (m_nextRevision == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport revision token allocator exhausted");
    }
    return ++m_nextRevision;
}

void ViewportEngine::setNextRevisionValueForTest(quint64 token)
{
    m_nextRevision = token == 0 ? 0 : token - 1;
    m_commandRevision = {};
}

quint64 ViewportEngine::nextPageSetGeneration()
{
    if (m_nextPageSetGeneration == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport page-set generation allocator exhausted");
    }
    return ++m_nextPageSetGeneration;
}

ViewportEngine::PageSetState ViewportEngine::pageSetStateFor(
    ImageViewportPageSet pageSet, quint64 generation) const
{
    PageSetState state;
    if (pageSet.isClear()) {
        state.pageSet = ImageViewportPageSet::clear();
        state.generation = generation;
        return state;
    }

    state.pageSet = pageSet;
    state.acceptedRoleSet = ImageViewportRoleSet(true, pageSet.secondary() != nullptr);
    state.targetRoleSet = state.acceptedRoleSet;
    state.generation = generation;
    state.primaryRoleGeneration = generation;
    state.secondaryRoleGeneration = pageSet.secondary() ? generation : 0;
    state.activeRole = ImageViewport::PageRole::Primary;
    state.activeRoleValid = true;
    return state;
}
