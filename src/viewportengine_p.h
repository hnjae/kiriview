#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollerprovidercontract_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportengineplaybackoperations_p.h"
#include "viewportengineproviderrequesttokenoperations_p.h"
#include "viewportenginerenderoperations_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportrendercontract_p.h"

#include <array>
#include <memory>

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
class ViewportEngineTestAccess;
class ViewportController;
#define VIEWPORT_ENGINE_TEST_VISIBILITY public
#else
#define VIEWPORT_ENGINE_TEST_VISIBILITY private
#endif
struct ViewportEngineCanonicalState;
struct ViewportEnginePlaybackStateAccess;
struct ViewportEngineProviderStateAccess;
struct ViewportEngineSnapshotStateAccess;

class ViewportEngine
{
public:
    class PendingPublication
    {
    public:
        PendingPublication() = delete;
        PendingPublication(const PendingPublication&) = delete;
        PendingPublication& operator=(const PendingPublication&) = delete;
        PendingPublication(PendingPublication&& other) noexcept;
        PendingPublication& operator=(PendingPublication&& other) noexcept;

    private:
        friend class ViewportEngine;
        PendingPublication(ViewportEngine* owner,
            ImageViewportInternal::ViewportChangeSet changes);

        ViewportEngine* m_owner = nullptr;
        ImageViewportInternal::ViewportChangeSet m_changes;
    };

    ViewportEngine();
    ~ViewportEngine();
    ViewportEngine(const ViewportEngine&) = delete;
    ViewportEngine& operator=(const ViewportEngine&) = delete;
    using GeometryInput = ViewportEngineGeometryInput;
    using GeometryProjectionTarget = ViewportEngineGeometryProjectionTarget;
    using SnapshotInput = ViewportEngineSnapshotInput;

    using PresentationTargetState = ViewportEnginePresentationTargetState;

    struct PresentationTargetAssignmentInput
    {
        ImageViewportPresentationTarget presentationTarget
            = ImageViewportPresentationTarget::clear();
        PresentationTargetTransitionPolicy transitionPolicy;
        ImageViewportInternal::ImageSequenceSource primarySource;
        ImageViewportInternal::ImageSequenceSource secondarySource;
        GeometryInput geometry;
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

    struct PresentationCommandResult
    {
        CommandResult command;
        ImageViewportInternal::ViewportChangeSet changes;
        std::array<ViewportProviderFrameTransportEffect, 2> providerEffects;
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
        ImageViewportInternal::ViewportChangeSet changes;
        std::array<ViewportProviderFrameTransportEffect, 2> providerEffects;
        bool openPrimaryProviderSession = false;
        bool openSecondaryProviderSession = false;
        ViewportPlaybackScheduleEffect schedule;
    };
    struct ProviderEventAdmissionInput
    {
        ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
        ImageSequenceProviderRequestToken token;
    };
    struct ProviderSessionBinding
    {
        std::shared_ptr<ImageSequenceProviderSessionFactory> factory;
        ImageSequenceProviderThreadingContract threadingContract
            = ImageSequenceProviderThreadingContract::AffinityBound;
        quint64 generation = 0;
        quint64 sessionSerial = 0;
        bool sessionActive = false;
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
    using RenderSynchronizationInput = ViewportEngineRenderSynchronizationInput;

    using RenderAcknowledgementInput = ViewportEngineRenderAcknowledgementInput;

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
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    CommandDiagnostics commandDiagnostics() const;
    PresentationTargetState presentationTargetState() const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    const ImageViewportInternal::PresentationState& presentationState() const;
#endif
public:
    PresentationGeometry::State geometryState(const GeometryInput& input) const;
    GeometryInput projectedGeometryInput(const QRectF& itemBounds, double devicePixelRatio = 1.0,
        GeometryProjectionTarget target = GeometryProjectionTarget::CurrentDisplay) const;
    GeometryInput acceptedGeometryInput(
        const QRectF& itemBounds, double devicePixelRatio = 1.0) const;
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    ViewportRenderSnapshot renderSnapshot(const ViewportRenderSnapshotInput& input) const;
public:
    ViewportRenderSynchronization beginRenderSynchronization(
        const RenderSynchronizationInput& input);
    ViewportEngineRenderCommitTransition acknowledgeRenderCommit(
        const RenderAcknowledgementInput& input);
    ViewportEngineRenderFailureTransition acknowledgeRenderFailure(
        const RenderAcknowledgementInput& input);
    ViewportEngineGeometryChangeTransition handleGeometryChanged(
        const ViewportEngineGeometryChangeInput& input);
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        const GeometryInput& geometry);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
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
    ViewportProviderMetadataReadyResult reduceProviderMetadataReady(ImageViewport::PageRole role,
        const ViewportProviderMetadataReadyEvent& event, const GeometryInput& geometry);
public:
    ViewportProviderEventResult reduceProviderEvent(
        const ViewportProviderEvent& event, const GeometryInput& geometry);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
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
    ProviderSessionBinding providerSessionBinding(ImageViewport::PageRole role) const;
    ViewportProviderRequestTokenAllocationAccess providerRequestTokenAllocationAccess();
    ViewportProviderTerminalEventResult reduceProviderTerminalEvent(
        ImageViewport::PageRole role, const ViewportProviderTerminalEvent& event);
public:
    ViewportProviderTerminalEventResult reduceProviderDispatchFailure(
        ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event);
    ViewportProviderSessionOpenFailureResult reduceProviderSessionOpenFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    ViewportProviderSessionOpenResult reduceProviderSessionOpened(
        ImageViewport::PageRole role, const GeometryInput& geometry);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest(
        ImageViewport::PageRole role);
    ViewportProviderFrameRequestStartResult startProviderFrameRequest(ImageViewport::PageRole role,
        ImageViewportInternal::DisplayRequestTarget target, const GeometryInput& geometry);
public:
    ViewportProviderSchedulerFailureResult reduceProviderQueueSchedulingFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    ImageViewportInternal::ViewportChangeSet reduceProviderWaitingEvent(
        ImageViewport::PageRole role, const ViewportProviderWaitingEvent& event);
    ViewportProviderEndOfSequenceResult reduceProviderEndOfSequenceProtocolViolation(
        ImageViewport::PageRole role, ViewportProviderEndOfSequenceProtocolViolation input);
    ViewportProviderEndOfSequenceResult reduceProviderEndOfSequence(ImageViewport::PageRole role,
        ViewportProviderEndOfSequenceEvent event, const GeometryInput& geometry);
public:
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewport::PageRole role);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    void clearQueuedProviderFrameRequest(ImageViewport::PageRole role);
    bool hasActiveProviderFrameToken(ImageViewport::PageRole role) const;
    ProviderFrameQueueResult queueProviderFrameRequest(ProviderFrameQueueInput input);
    ProviderFrameQueueFlushResult flushQueuedProviderFrameRequest(ImageViewport::PageRole role);
public:
    ViewportProviderFrameQueueFlushResult reduceQueuedProviderFrameRequest(
        ImageViewport::PageRole role, const GeometryInput& geometry);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    ImageSequenceProviderDisplayDemand providerDisplayDemand(
        ImageViewport::PageRole role, const GeometryInput& geometry);
public:
    PlaybackCommandResult applyPlaybackCommand(const PlaybackCommandInput& input);
    PlaybackTickResult advancePlayback(const PlaybackTickInput& input);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    void setPlaybackPhase(
        ImageViewport::PlaybackPhase phase, ImageViewportInternal::ViewportChangeSet& changes);
public:
    PresentationTargetAssignmentResult assignPresentationTarget(
        const PresentationTargetAssignmentInput& input);
    PresentationCommandResult applyPresentationCommand(
        const ViewportEnginePresentationCommandInput& input);
    VIEWPORT_ENGINE_TEST_VISIBILITY:
    CommandResult rejectInvalidCommand();
    CommandResult rejectMalformedEnumCommand();
    CommandResult clearFromEmpty();
    CommandResult validatePresentationNoop(ImageViewport::FitMode mode);
    quint64 allocateRevisionValue();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
public:
    void setNextRevisionValueForTest(quint64 token);
#endif

private:
    friend class ViewportController;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    friend class ViewportEngineTestAccess;
#endif
    PendingPublication preparePublication(ImageViewportInternal::ViewportChangeSet changes);
    ImageViewportInternal::ViewportChangeSet publish(PendingPublication publication);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    ImageViewportInternal::DisplayState& displayState();
    const ImageViewportInternal::DisplayState& displayState() const;
    ImageViewportInternal::RequestState& requestState();
    const ImageViewportInternal::RequestState& requestState() const;
    ImageViewportInternal::PlaybackState& playbackState();
    const ImageViewportInternal::PlaybackState& playbackState() const;
#endif
    ViewportEngineProviderStateAccess providerAccess();
    ViewportEnginePlaybackStateAccess playbackAccess();
    ViewportEngineSnapshotStateAccess snapshotAccess() const;
    ViewportEngineProviderFactsView providerFactsView() const;
    ViewportPlaybackScheduleEffect currentPlaybackSchedule() const;

    static constexpr std::size_t roleIndex(ImageViewport::PageRole role)
    {
        return role == ImageViewport::PageRole::Secondary ? 1U : 0U;
    }
    FramePreparation::ProviderFrameState providerFramePreparationState(
        ImageViewport::PageRole role) const;
    void recordProviderTerminal(ImageViewport::PageRole role, ImageViewport::RequestStatus status,
        ImageViewport::RequestReason reason, ImageViewportInternal::FailureScope scope,
        const QString& diagnostic, ImageViewportInternal::ViewportChangeSet& changes);
    CommandResult rejected(
        ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason);
    CommandResult accepted();
    CommandResult acceptedPreservingCommandDiagnostics() const;
    RevisionToken nextCommandRevision();
    quint64 nextPresentationTargetGeneration();
    PresentationTargetState presentationTargetStateFor(
        const ImageViewportPresentationTarget& presentationTarget, quint64 generation) const;

    std::unique_ptr<ViewportEngineCanonicalState> m_state;
};

#undef VIEWPORT_ENGINE_TEST_VISIBILITY
