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
    ImageViewportStateSnapshot snapshot(ViewportEngineViewportInput input = {}) const;
    PresentationGeometry::State geometryState(ViewportEngineViewportInput input) const;
    ViewportRenderSynchronization beginRenderSynchronization(
        const ViewportEngineRenderSynchronizationRequest& input);
    ViewportEngineRenderCommitTransition acknowledgeRenderCommit(
        const ViewportEngineRenderAcknowledgementRequest& input);
    ViewportEngineRenderFailureTransition acknowledgeRenderFailure(
        const ViewportEngineRenderAcknowledgementRequest& input);
    ViewportEngineRenderQualityFallbackTransition handleRenderQualityFallback(
        const ViewportRenderQualityFallbackFact& fact);
    ViewportEngineTransition handleResourcePressure(const ViewportEngineResourcePressureFact& fact);
    ViewportEngineGeometryChangeTransition handleGeometryChanged(
        const ViewportEngineGeometryChangeRequest& input);
    ViewportEngineTransition handleProviderHostEvent(
        const ViewportEngineProviderHostEventRequest& input);
    ViewportEngineTransition handleDevicePixelRatioChanged(ViewportEngineViewportInput input);
    ViewportEnginePlaybackCommandResult applyPlaybackCommand(
        const ViewportEnginePlaybackCommandRequest& input);
    ViewportEnginePlaybackTickResult advancePlayback(
        const ViewportEnginePlaybackTickRequest& input);
    ViewportEnginePresentationTargetAssignmentResult assignPresentationTarget(
        const ViewportEnginePresentationTargetAssignmentRequest& input);
    ViewportEnginePresentationCommandResult applyPresentationCommand(
        const ViewportEnginePresentationCommandRequest& input);
    ImageViewportInternal::ViewportChangeSet publishChanges(
        ImageViewportInternal::ViewportChangeSet changes);
    ViewportProviderTransportBatch shutdown();
    QSet<quint64> providerFrameLeaseIds() const;

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
    GeometryInput currentGeometry(ViewportEngineViewportInput input) const;
    GeometryInput pendingGeometry(ViewportEngineViewportInput input) const;
    GeometryInput acceptedGeometry(ViewportEngineViewportInput input) const;
    PresentationGeometry::State geometryState(const GeometryInput& input) const;
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        const GeometryInput& geometry);
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        ViewportEngineViewportInput input);
    ViewportProviderEventResult reduceProviderEvent(
        const ViewportProviderEvent& event, ViewportEngineViewportInput input);
    ViewportProviderTerminalEventResult reduceProviderDispatchFailure(
        ImageViewportPageRole role, const ViewportProviderDispatchFailureEvent& event);
    ViewportProviderSessionOpenFailureResult reduceProviderSessionOpenFailure(
        ImageViewportPageRole role, const QString& diagnostic);
    ViewportProviderSessionOpenResult reduceProviderSessionOpened(
        ImageViewportPageRole role, ViewportEngineViewportInput input);
    ViewportProviderSchedulerFailureResult reduceProviderQueueSchedulingFailure(
        ImageViewportPageRole role, const QString& diagnostic);
    ViewportProviderFrameQueueFlushResult reduceQueuedProviderFrameRequest(
        ImageViewportPageRole role, ViewportEngineViewportInput input);
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
