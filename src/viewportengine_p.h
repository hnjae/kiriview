#pragma once

#include "viewportenginefacadecontract_p.h"

#include <QtCore/QSet>

#include <array>
#include <memory>

class ViewportEngineTestAccess;
struct ViewportEngineCanonicalState;
struct ViewportEngineSnapshotStateAccess;
struct ViewportEngineProviderFactsView;
struct ViewportProviderEventResult;
struct ViewportProviderFrameQueueFlushResult;
struct ViewportProviderSchedulerFailureResult;
struct ViewportProviderSessionOpenFailureResult;
struct ViewportProviderSessionOpenResult;
struct ViewportProviderTerminalEventResult;

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
        PendingPublication(ViewportEngine* owner, ImageViewportInternal::ViewportChangeSet changes);

        ViewportEngine* m_owner = nullptr;
        ImageViewportInternal::ViewportChangeSet m_changes;
    };

    ViewportEngine();
    ~ViewportEngine();
    ViewportEngine(const ViewportEngine&) = delete;
    ViewportEngine& operator=(const ViewportEngine&) = delete;
    ImageViewportStateSnapshot snapshot() const;
    PresentationGeometry::State geometryState() const;
    ViewportRenderAttempt beginRenderSynchronization();
    ViewportEngineRenderHostTransition handleRenderHostFact(
        const ViewportEngineRenderHostFactRequest& input);
    ViewportEngineTransition handleResourcePressure();
    ViewportEngineTransition handleViewportChanged(ViewportEngineViewportState viewport);
    ViewportEngineTransition handleProviderHostEvent(
        const ViewportEngineProviderHostEventRequest& input);
    ViewportEnginePlaybackCommandResult applyPlaybackCommand(
        ViewportEnginePlaybackCommandRequest input);
    ViewportEnginePlaybackTickResult advancePlayback(ViewportEnginePlaybackTickRequest input);
    ViewportEnginePresentationTargetAssignmentResult assignPresentationTarget(
        const ViewportEnginePresentationTargetAssignmentRequest& input);
    ViewportEnginePresentationCommandResult applyPresentationCommand(
        const ViewportEnginePresentationCommandRequest& input);
    ImageViewportInternal::ViewportChangeSet publishChanges(
        ImageViewportInternal::ViewportChangeSet changes);
    ViewportProviderTransportBatch shutdown();
    QSet<quint64> providerFrameLeaseIds() const;
    bool acceptsProviderTransportCommand(const ViewportProviderTransportCommand& command) const;

private:
    using GeometryInput = ViewportEngineGeometryInput;
    ViewportEngineCommandDiagnostics commandDiagnostics() const;
    ViewportEnginePresentationTargetState presentationTargetState() const;
    ViewportRenderSnapshot renderSnapshot(const ViewportRenderSnapshotInput& input) const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    const ImageViewportInternal::PresentationState& presentationState() const;
#endif
    ViewportEngineCommandResult rejectInvalidCommand();
    quint64 allocateRevisionValue();
    quint64 advanceTargetPresentationRevision();
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void setNextRevisionValueForTest(quint64 token);
#endif
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
    GeometryInput currentGeometry() const;
    GeometryInput pendingGeometry() const;
    GeometryInput rawAcceptedGeometry() const;
    GeometryInput acceptedGeometry() const;
    PresentationGeometry::State geometryState(const GeometryInput& input) const;
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        const GeometryInput& geometry);
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands();
    ViewportProviderEventResult reduceProviderEvent(const ViewportProviderEvent& event);
    ViewportProviderTerminalEventResult reduceProviderProtocolViolation(
        ImageViewportPageRole role, ImageSequenceProviderRequestToken token);
    ViewportProviderTerminalEventResult reduceProviderDispatchFailure(
        ImageViewportPageRole role, const ViewportProviderDispatchFailureEvent& event);
    ViewportProviderSessionOpenFailureResult reduceProviderSessionOpenFailure(
        ImageViewportPageRole role, const QString& diagnostic);
    ViewportProviderSessionOpenResult reduceProviderSessionOpened(ImageViewportPageRole role);
    ViewportProviderSchedulerFailureResult reduceProviderQueueSchedulingFailure(
        ImageViewportPageRole role, const QString& diagnostic);
    ViewportProviderFrameQueueFlushResult reduceQueuedProviderFrameRequest(
        ImageViewportPageRole role);
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewportPageRole role);

    static constexpr std::size_t roleIndex(ImageViewportPageRole role)
    {
        return role == ImageViewportPageRole::Secondary ? 1U : 0U;
    }
    ViewportEngineCommandResult rejected(
        ImageViewportCommandOutcome outcome, ImageViewportCommandReason reason);
    ViewportEngineCommandResult accepted();
    ViewportEngineCommandResult acceptedPreservingCommandDiagnostics() const;
    RevisionToken nextCommandRevision();

    std::unique_ptr<ViewportEngineCanonicalState> m_state;
};
