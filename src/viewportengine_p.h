#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "viewportrendercontract_p.h"

class ViewportEngine
{
public:
    struct GeometryInput
    {
        bool primaryPresent = false;
        QRectF itemBounds;
        QSizeF primarySize;
        QSizeF secondarySize;
        double devicePixelRatio = 1.0;
    };

    struct PresentationTargetState
    {
        ImageViewportPresentationTarget presentationTarget
            = ImageViewportPresentationTarget::clear();
        ImageViewportRoleSet acceptedRoleSet;
        ImageViewportRoleSet targetRoleSet;
        quint64 generation = 0;
        quint64 primaryRoleGeneration = 0;
        quint64 secondaryRoleGeneration = 0;
        ImageViewport::PageRole activeRole = ImageViewport::PageRole::Primary;
        bool activeRoleValid = false;
    };

    struct PresentationTargetAssignmentInput
    {
        ImageViewportPresentationTarget presentationTarget
            = ImageViewportPresentationTarget::clear();
        PresentationTargetTransitionPolicy transitionPolicy;
    };

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

    struct PresentationTargetAssignmentResult
    {
        CommandResult command;
        PresentationTargetState presentationTargetState;
        bool presentationTargetChanged = false;
        bool clear = true;
        bool retainPreviousDisplay = true;
        bool releaseDisplayedState = false;
        bool resetDisplayRequests = false;
        bool stopPlayback = false;
        bool closeProviderSessions = false;
    };

    ImageViewportStateSnapshot snapshot() const;
    CommandDiagnostics commandDiagnostics() const;
    PresentationTargetState presentationTargetState() const;
    ImageViewportInternal::DisplayState& displayState();
    const ImageViewportInternal::DisplayState& displayState() const;
    ImageViewportInternal::RequestState& requestState();
    const ImageViewportInternal::RequestState& requestState() const;
    ImageViewportInternal::ProviderGenerationState& providerState();
    const ImageViewportInternal::ProviderGenerationState& providerState() const;
    ImageViewportInternal::ProviderGenerationState& secondaryProviderState();
    const ImageViewportInternal::ProviderGenerationState& secondaryProviderState() const;
    const ImageViewportInternal::PresentationState& presentationState() const;
    ImageViewportInternal::PresentationState& presentationState();
    PresentationGeometry::State geometryState(const GeometryInput& input) const;
    PresentationGeometry::State geometryState(const GeometryInput& input,
        const ImageViewportInternal::PresentationState& presentation) const;
    ViewportRenderSnapshot renderSnapshot(const ViewportRenderSnapshotInput& input) const;

    PresentationTargetAssignmentResult assignPresentationTarget(
        PresentationTargetAssignmentInput input);
    CommandResult rejectInvalidCommand();
    CommandResult rejectMalformedEnumCommand();
    CommandResult clearFromEmpty();
    CommandResult validatePresentationNoop(ImageViewport::FitMode mode);
    quint64 allocateRevisionValue();
    void setNextRevisionValueForTest(quint64 token);

private:
    CommandResult rejected(
        ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason);
    CommandResult accepted();
    CommandResult acceptedPreservingCommandDiagnostics() const;
    RevisionToken nextCommandRevision();
    quint64 nextPresentationTargetGeneration();
    PresentationTargetState presentationTargetStateFor(
        ImageViewportPresentationTarget presentationTarget, quint64 generation) const;

    quint64 m_nextRevision = 0;
    quint64 m_nextPresentationTargetGeneration = 0;
    ImageViewport::CommandReason m_commandReason = ImageViewport::CommandReason::NoCommand;
    RevisionToken m_commandRevision;
    PresentationTargetState m_presentationTargetState;
    ImageViewportInternal::DisplayState m_displayState;
    ImageViewportInternal::RequestState m_requestState;
    ImageViewportInternal::ProviderGenerationState m_providerState;
    ImageViewportInternal::ProviderGenerationState m_secondaryProviderState;
    ImageViewportInternal::PresentationState m_presentationState;
};
