#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "viewportrendercontract_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportcontrollerprovidercontract_p.h"

#include <array>
#include <optional>


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
    struct SnapshotInput
    {
        GeometryInput acceptedGeometry;
        GeometryInput displayedGeometry;
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

    struct PresentationCommandInput
    {
        ImageViewportPresentationCommand command;
        GeometryInput geometry;
        QPointF anchor;
        bool readyDisplay = false;
    };

    struct PresentationCommandResult
    {
        CommandResult command;
        ImageViewportInternal::ViewportChangeSet changes;
    };

    struct PresentationTargetTransitionInput
    {
        PresentationTargetTransitionPolicy::ZoomTransition zoomTransition
            = PresentationTargetTransitionPolicy::ZoomTransition::Preserve;
        PresentationTargetTransitionPolicy::ContentPositionTransition contentPositionTransition
            = PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp;
        PresentationTargetTransitionPolicy::RotationTransition rotationTransition
            = PresentationTargetTransitionPolicy::RotationTransition::Preserve;
        PresentationTargetTransitionPolicy::MirrorTransition mirrorTransition
            = PresentationTargetTransitionPolicy::MirrorTransition::Preserve;
        std::optional<ImageViewport::FitMode> explicitFitMode;
        std::optional<ImageViewport::SpreadDirection> explicitSpreadDirection;
        std::optional<double> explicitPageGap;
        GeometryInput acceptedGeometry;
        QPointF previousContentPosition;
        double previousZoomPercent = 100.0;
        bool readyDisplay = false;
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
    struct ProviderEventAdmissionInput
    {
        ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
        ImageSequenceProviderRequestToken token;
    };

    struct ProviderFrameEventAdmission
    {
        bool accepted = false;
        FramePreparation::ProviderFrameState preparationState;
    };

    struct ProviderMetadataEventAdmission
    {
        bool accepted = false;
    };

    struct ProviderFrameQueueInput
    {
        ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
        int frame = -1;
        ImageViewportInternal::ProviderRequestTargetKind targetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    };

    struct ProviderFrameQueueResult
    {
        ImageSequenceProviderRequestToken cancelToken;
        bool deferredFlush = false;
    };

    struct ProviderFrameQueueFlushResult
    {
        bool startRequest = false;
        int frame = -1;
        ImageViewportInternal::ProviderRequestTargetKind targetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    };
    struct RenderSynchronizationInput
    {
        QSizeF itemSize;
        QRectF itemBounds;
        QRectF oldContentRect;
        QRectF oldVisibleImageRect;
        GeometryInput currentGeometry;
        GeometryInput pendingGeometry;
    };

    struct RenderAcknowledgementInput
    {
        ViewportRenderAcknowledgement acknowledgement;
        bool renderedImagePresent = false;
        quint64 synchronizationAttempt = 0;
        bool pendingTargetCommit = false;
        bool pendingSecondaryProviderCommit = false;
        ImageViewportInternal::PreparedPayload preparedPayload;
        ImageViewport::DisplayStatus oldDisplayStatus = ImageViewport::DisplayStatus::Empty;
        QRectF oldContentRect;
        QRectF oldVisibleImageRect;
        PresentationGeometry::State geometryState;
    };

    struct GeometryChangeInput
    {
        QRectF itemBounds;
        QRectF oldContentRect;
        QRectF oldVisibleImageRect;
        PresentationGeometry::State geometryState;
    };
    struct PlaybackCommandInput
    {
        ViewportPlaybackCommand command;
        GeometryInput geometry;
    };
    struct PlaybackTickInput
    {
        int elapsedMilliseconds = 0;
        GeometryInput geometry;
    };
    struct PlaybackProviderEffects
    {
        std::array<ViewportProviderFrameTransportEffect, 2> providerFrameTransport;
    };
    struct PlaybackCommandResult
    {
        CommandResult command;
        ImageViewportInternal::ViewportChangeSet changes;
        PlaybackProviderEffects effects;
        ViewportPlaybackScheduleEffect schedule;
    };
    struct PlaybackTickResult
    {
        ImageViewportInternal::ViewportChangeSet changes;
        PlaybackProviderEffects effects;
        ViewportPlaybackScheduleEffect schedule;
    };
    ImageViewportStateSnapshot snapshot() const;
    ImageViewportStateSnapshot snapshot(const GeometryInput& input) const;
    ImageViewportStateSnapshot snapshot(const SnapshotInput& input) const;
    ImageViewportInternal::ViewportChangeSet publishChanges(
        ImageViewportInternal::ViewportChangeSet changes);
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
    PresentationGeometry::State geometryState(const GeometryInput& input) const;
    PresentationGeometry::State geometryState(const GeometryInput& input,
        const ImageViewportInternal::PresentationState& presentation) const;
    ViewportRenderSnapshot renderSnapshot(const ViewportRenderSnapshotInput& input) const;
    ViewportRenderSynchronization beginRenderSynchronization(
        const RenderSynchronizationInput& input);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderCommit(
        const RenderAcknowledgementInput& input);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderFailure(
        const RenderAcknowledgementInput& input);
    ImageViewportInternal::ViewportChangeSet handleGeometryChanged(
        const GeometryChangeInput& input);
    ProviderFrameEventAdmission admitProviderFrameEvent(ProviderEventAdmissionInput input);
    ImageViewportInternal::ViewportChangeSet reduceProviderFrameAdmission(
        ImageViewport::PageRole role,
        const FramePreparation::ProviderFrameAdmissionResult& admission,
        const GeometryInput& geometry);
    ImageViewportInternal::ViewportChangeSet reduceProviderFrameEvent(ImageViewport::PageRole role,
        ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata, const GeometryInput& geometry);
    ProviderMetadataEventAdmission admitProviderMetadataEvent(ProviderEventAdmissionInput input);
    ViewportProviderMetadataAdmissionResult reduceProviderMetadataAdmission(
        ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata);
    ImageViewportInternal::ViewportChangeSet acceptProviderMetadataFacts(
        ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts);
    ImageViewportInternal::ViewportChangeSet rejectProviderMetadataTarget(
        ImageViewport::PageRole role, ViewportProviderMetadataTargetRejection rejection);
    ViewportProviderMetadataTargetPolicyResult applyProviderMetadataTargetPolicy(
        ImageViewport::PageRole role, const ViewportProviderAcceptedMetadataFacts& facts,
        const GeometryInput& geometry);
    quint64 activateProviderSession(ImageViewport::PageRole role);
    void retireProviderSession(ImageViewport::PageRole role);
    bool acceptsProviderSessionEvent(
        ImageViewport::PageRole role, quint64 sessionSerial, quint64 generation) const;
    ViewportProviderRequestTokenAllocation allocateProviderRequestToken(
        ImageViewport::PageRole role);
    ViewportProviderTerminalEventResult reduceProviderTerminalEvent(
        ImageViewport::PageRole role, const ViewportProviderTerminalEvent& event);
    ViewportProviderTerminalEventResult reduceProviderDispatchFailure(
        ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event);
    ImageViewportInternal::ViewportChangeSet reduceProviderSessionOpenFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    ViewportProviderSessionOpenResult reduceProviderSessionOpened(
        ImageViewport::PageRole role, const GeometryInput& geometry);
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest(
        ImageViewport::PageRole role);
    ViewportProviderFrameRequestStartResult startProviderFrameRequest(ImageViewport::PageRole role,
        ImageViewportInternal::DisplayRequestTarget target, const GeometryInput& geometry);
    ViewportProviderSchedulerFailureResult reduceProviderQueueSchedulingFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    ImageViewportInternal::ViewportChangeSet reduceProviderWaitingEvent(
        ImageViewport::PageRole role, const ViewportProviderWaitingEvent& event);
    ViewportProviderEndOfSequenceResult reduceProviderEndOfSequenceProtocolViolation(
        ImageViewport::PageRole role, ViewportProviderEndOfSequenceProtocolViolation input);
    ViewportProviderEndOfSequenceResult reduceProviderEndOfSequence(
        ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event,
        const GeometryInput& geometry);
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewport::PageRole role);
    void clearQueuedProviderFrameRequest(ImageViewport::PageRole role);
    bool hasActiveProviderFrameToken(ImageViewport::PageRole role) const;
    ProviderFrameQueueResult queueProviderFrameRequest(ProviderFrameQueueInput input);
    ProviderFrameQueueFlushResult flushQueuedProviderFrameRequest(ImageViewport::PageRole role);
    ImageSequenceProviderDisplayDemand providerDisplayDemand(
        ImageViewport::PageRole role, const GeometryInput& geometry);
    PlaybackCommandResult applyPlaybackCommand(const PlaybackCommandInput& input);
    PlaybackTickResult advancePlayback(const PlaybackTickInput& input);
    void setPlaybackPhase(ImageViewport::PlaybackPhase phase,
        ImageViewportInternal::ViewportChangeSet& changes);
    void armAuthoredAutoplayIfEligible();
    ViewportPlaybackScheduleEffect playbackScheduleEffect() const;

    PresentationTargetAssignmentResult assignPresentationTarget(
        const PresentationTargetAssignmentInput& input);
    PresentationCommandResult applyPresentationCommand(const PresentationCommandInput& input);
    ImageViewportInternal::ViewportChangeSet applyPresentationTargetTransition(
        const PresentationTargetTransitionInput& input);
    CommandResult rejectInvalidCommand();
    CommandResult rejectMalformedEnumCommand();
    CommandResult clearFromEmpty();
    CommandResult validatePresentationNoop(ImageViewport::FitMode mode);
    quint64 allocateRevisionValue();
    void setNextRevisionValueForTest(quint64 token);

private:
    struct RoleState
    {
        ImageViewportInternal::ProviderGenerationState provider;
    };

    static constexpr std::size_t roleIndex(ImageViewport::PageRole role)
    {
        return role == ImageViewport::PageRole::Secondary ? 1U : 0U;
    }
    FramePreparation::ProviderFrameState providerFramePreparationState(
        ImageViewport::PageRole role) const;
    void recordProviderTerminal(ImageViewport::PageRole role,
        ImageViewport::RequestStatus status, ImageViewport::RequestReason reason,
        ImageViewportInternal::FailureScope scope, const QString& diagnostic,
        ImageViewportInternal::ViewportChangeSet& changes);
    CommandResult rejected(
        ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason);
    CommandResult accepted();
    CommandResult acceptedPreservingCommandDiagnostics() const;
    RevisionToken nextCommandRevision();
    quint64 nextPresentationTargetGeneration();
    PresentationTargetState presentationTargetStateFor(
        const ImageViewportPresentationTarget& presentationTarget, quint64 generation) const;

    quint64 m_nextRevision = 0;
    quint64 m_nextPresentationTargetGeneration = 0;
    quint64 m_nextRenderSynchronizationAttempt = 0;
    quint64 m_presentationRevision = 0;
    quint64 m_snapshotRevision = 0;
    ViewportRenderSynchronization m_lastRenderSynchronization;
    RevisionToken m_commandRevision;
    PresentationTargetState m_presentationTargetState;
    ImageViewportInternal::DisplayState m_displayState;
    ImageViewportInternal::RequestState m_requestState;
    std::array<RoleState, 2> m_roles;
    ImageViewportInternal::PresentationState m_presentationState;
};
