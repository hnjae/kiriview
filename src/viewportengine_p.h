#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollerprovidercontract_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportenginetransition_p.h"
#include "viewportengineassignmentoperations_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportengineplaybackoperations_p.h"
#include "viewportengineproviderfailureoperations_p.h"
#include "viewportengineprovidereventcompletionoperations_p.h"
#include "viewportengineproviderframeoperations_p.h"
#include "viewportengineprovidermetadataoperations_p.h"
#include "viewportengineproviderrequesttokenoperations_p.h"
#include "viewportengineproviderrequestoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"
#include "viewportenginerenderoperations_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportrendercontract_p.h"

#include <array>
#include <memory>

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
class ViewportEngineTestAccess;
class ViewportController;
#endif
struct ViewportEngineCanonicalState;
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
    using ViewportInput = ViewportEngineViewportInput;

    using PresentationTargetState = ViewportEnginePresentationTargetState;

    struct PresentationTargetAssignmentInput
    {
        ImageViewportPresentationTarget presentationTarget
            = ImageViewportPresentationTarget::clear();
        PresentationTargetTransitionPolicy transitionPolicy;
        ImageViewportInternal::ImageSequenceSource primarySource;
        ImageViewportInternal::ImageSequenceSource secondarySource;
        ViewportInput viewport;
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
        std::array<ViewportEngineProviderSessionOpenEffect, 2> providerSessionOpenEffects;
        ViewportPlaybackScheduleEffect schedule;
    };
    struct RenderSynchronizationInput
    {
        ViewportInput viewport;
    };

    struct GeometryChangeInput
    {
        ViewportInput viewport;
        QRectF oldContentRect;
        QRectF oldVisibleImageRect;
    };

    using RenderAcknowledgementInput = ViewportEngineRenderAcknowledgementInput;

    struct PlaybackCommandInput
    {
        ViewportPlaybackCommand command;
        ViewportInput viewport;
    };
    struct PlaybackTickInput
    {
        int elapsedMilliseconds = 0;
        ViewportInput viewport;
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
    struct PresentationCommandInput
    {
        ImageViewportPresentationCommand command;
        ViewportInput viewport;
        QPointF anchor;
        int quarterTurnDelta = 0;
    };
    struct ProviderHostEventInput
    {
        ViewportProviderHostEvent event;
        ViewportInput viewport;
    };
    ImageViewportStateSnapshot snapshot(ViewportInput input = {}) const;

private:
    using GeometryInput = ViewportEngineGeometryInput;
    CommandDiagnostics commandDiagnostics() const;
    PresentationTargetState presentationTargetState() const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    const ImageViewportInternal::PresentationState& presentationState() const;
#endif

public:
    PresentationGeometry::State geometryState(ViewportInput input) const;
private:
    ViewportRenderSnapshot renderSnapshot(const ViewportRenderSnapshotInput& input) const;

public:
    ViewportRenderSynchronization beginRenderSynchronization(
        const RenderSynchronizationInput& input);
    ViewportEngineRenderCommitTransition acknowledgeRenderCommit(
        const RenderAcknowledgementInput& input);
    ViewportEngineRenderFailureTransition acknowledgeRenderFailure(
        const RenderAcknowledgementInput& input);
    ViewportEngineGeometryChangeTransition handleGeometryChanged(
        const GeometryChangeInput& input);
    ViewportEngineTransition handleProviderHostEvent(const ProviderHostEventInput& input);
    ViewportEngineTransition handleDevicePixelRatioChanged(ViewportInput input);
    PlaybackCommandResult applyPlaybackCommand(const PlaybackCommandInput& input);
    PlaybackTickResult advancePlayback(const PlaybackTickInput& input);
    PresentationTargetAssignmentResult assignPresentationTarget(
        const PresentationTargetAssignmentInput& input);
    PresentationCommandResult applyPresentationCommand(
        const PresentationCommandInput& input);
private:
    CommandResult rejectInvalidCommand();
    quint64 allocateRevisionValue();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void setNextRevisionValueForTest(quint64 token);
#endif
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
    ViewportEngineSnapshotStateAccess snapshotAccess() const;
    ViewportEngineProviderFactsView providerFactsView() const;
    ViewportPlaybackScheduleEffect currentPlaybackSchedule() const;
    GeometryInput currentGeometry(ViewportInput input) const;
    GeometryInput pendingGeometry(ViewportInput input) const;
    GeometryInput acceptedGeometry(ViewportInput input) const;
    PresentationGeometry::State geometryState(const GeometryInput& input) const;
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        const GeometryInput& geometry);
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        ViewportInput input);
    ViewportProviderEventResult reduceProviderEvent(
        const ViewportProviderEvent& event, ViewportInput input);
    ViewportProviderTerminalEventResult reduceProviderDispatchFailure(
        ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event);
    ViewportProviderSessionOpenFailureResult reduceProviderSessionOpenFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    ViewportProviderSessionOpenResult reduceProviderSessionOpened(
        ImageViewport::PageRole role, ViewportInput input);
    ViewportProviderSchedulerFailureResult reduceProviderQueueSchedulingFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewport::PageRole role);
    ViewportProviderFrameQueueFlushResult reduceQueuedProviderFrameRequest(
        ImageViewport::PageRole role, ViewportInput input);

    static constexpr std::size_t roleIndex(ImageViewport::PageRole role)
    {
        return role == ImageViewport::PageRole::Secondary ? 1U : 0U;
    }
    CommandResult rejected(
        ImageViewport::CommandOutcome outcome, ImageViewport::CommandReason reason);
    CommandResult accepted();
    CommandResult acceptedPreservingCommandDiagnostics() const;
    RevisionToken nextCommandRevision();

    std::unique_ptr<ViewportEngineCanonicalState> m_state;
};
