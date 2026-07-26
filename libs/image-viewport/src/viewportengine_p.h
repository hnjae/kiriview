/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

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
struct ViewportEngineProviderSessionOpenFailureInput;

class ViewportEngine
{
public:
    ViewportEngine();
    ~ViewportEngine();
    ViewportEngine(const ViewportEngine&) = delete;
    ViewportEngine& operator=(const ViewportEngine&) = delete;
    ViewportEngine(ViewportEngine&&) = delete;
    ViewportEngine& operator=(ViewportEngine&&) = delete;
    [[nodiscard]] ImageViewportStateSnapshot snapshot() const;
    [[nodiscard]] ViewportEngineCoordinateQueryResult queryCoordinate(
        const ViewportEngineCoordinateQueryRequest& input) const;
    ViewportRenderAttempt beginRenderSynchronization();
    ViewportEngineTransition handleRenderHostFact(const ViewportEngineRenderHostFactRequest& input);
    ViewportEngineTransition handleResourcePressure();
    ViewportEngineTransition handleViewportChanged(ViewportEngineViewportState viewport);
    ViewportEngineTransition handleProviderHostEvent(
        const ViewportEngineProviderHostEventRequest& input);
    ViewportEngineCommandTransition applyPlaybackCommand(
        ViewportEnginePlaybackCommandRequest input);
    ViewportEngineTransition advancePlayback(ViewportEnginePlaybackTickRequest input);
    [[nodiscard]] bool canAssignPresentationTarget(
        const ViewportEnginePresentationTargetAssignmentRequest& input) const;
    ViewportEngineCommandTransition assignPresentationTarget(
        const ViewportEnginePresentationTargetAssignmentRequest& input);
    ViewportEngineCommandTransition applyPresentationCommand(
        const ViewportEnginePresentationCommandRequest& input);
    ViewportProviderTransportBatch shutdown();
    [[nodiscard]] QSet<quint64> providerFrameLeaseIds() const;
    [[nodiscard]] QSet<quint64> providerFailureLeaseIds() const;
    [[nodiscard]] bool acceptsProviderTransportCommand(
        const ViewportProviderTransportCommand& command) const;

private:
    using GeometryInput = ViewportEngineGeometryInput;
    [[nodiscard]] ViewportEngineCommandDiagnostics commandDiagnostics() const;
    [[nodiscard]] ViewportEnginePresentationTargetState presentationTargetState() const;
    [[nodiscard]] ViewportRenderSnapshot renderSnapshot(
        const ViewportRenderSnapshotInput& input) const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    [[nodiscard]] const ImageViewportInternal::PresentationState& presentationState() const;
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
    ImageViewportInternal::ViewportChangeSet publishChanges(
        ImageViewportInternal::ViewportChangeSet changes);
    ViewportEngineTransition finalizeTransition(ViewportEngineTransitionDraft draft);
    ViewportEngineCommandTransition finalizeCommandTransition(
        const ViewportEngineCommandResult& command, ViewportEngineTransitionDraft draft);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    ImageViewportInternal::DisplayState& displayState();
    [[nodiscard]] const ImageViewportInternal::DisplayState& displayState() const;
    ImageViewportInternal::RequestState& requestState();
    [[nodiscard]] const ImageViewportInternal::RequestState& requestState() const;
    ImageViewportInternal::PlaybackState& playbackState();
    [[nodiscard]] const ImageViewportInternal::PlaybackState& playbackState() const;
#endif
    [[nodiscard]] ViewportEngineSnapshotStateAccess snapshotAccess() const;
    [[nodiscard]] ViewportEngineProviderFactsView providerFactsView() const;
    ViewportPlaybackScheduleBatch currentPlaybackSchedules();
    [[nodiscard]] PresentationGeometry::State geometryState() const;
    [[nodiscard]] GeometryInput currentGeometry() const;
    [[nodiscard]] GeometryInput pendingGeometry() const;
    [[nodiscard]] GeometryInput rawAcceptedGeometry() const;
    [[nodiscard]] GeometryInput acceptedGeometry() const;
    [[nodiscard]] PresentationGeometry::State geometryState(const GeometryInput& input) const;
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands(
        const GeometryInput& geometry,
        ImageViewportRoleSet forcedRefinementRoles = ImageViewportRoleSet {},
        bool restageUnforcedRoles = true);
    std::array<ViewportProviderFrameTransportEffect, 2> restageProviderDemands();
    ViewportProviderEventResult reduceProviderEvent(const ViewportProviderEvent& event);
    ViewportProviderTerminalEventResult reduceProviderProtocolViolation(ImageViewportPageRole role,
        ImageSequenceProviderRequestToken token,
        ImageViewportInternal::InternalObservationCause cause,
        ImageSequenceProviderEventKind eventKind);
    ViewportProviderTerminalEventResult reduceProviderDispatchFailure(
        ImageViewportPageRole role, ViewportProviderDispatchFailureEvent event);
    ViewportProviderSessionOpenFailureResult reduceProviderSessionOpenFailure(
        ViewportEngineProviderSessionOpenFailureInput input);
    ViewportProviderSessionOpenResult reduceProviderSessionOpened(ImageViewportPageRole role);
    ViewportProviderSchedulerFailureResult reduceProviderQueueSchedulingFailure(
        ImageViewportPageRole role);
    ViewportProviderFrameQueueFlushResult reduceQueuedProviderFrameRequest(
        ImageViewportPageRole role);
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewportPageRole role);
    [[nodiscard]] bool hasCompleteCommittedPresentation() const;
    ViewportProviderTransportBatch pinCurrentPresentationForRestoration();
    void retireRestoration(ViewportEngineTransitionDraft& transition);
    bool restorePreviousIfTerminal(ViewportEngineTransitionDraft& transition);
    void commitReplacementIfReady(ViewportEngineTransitionDraft& transition);
    void discardProvisionalPresentationIfTerminal(
        ImageViewportInternal::ViewportChangeSet& changes);

    static constexpr std::size_t roleIndex(ImageViewportPageRole role)
    {
        return role == ImageViewportPageRole::Secondary ? 1U : 0U;
    }
    ViewportEngineCommandResult rejected(
        ImageViewportCommandOutcome outcome, ImageViewportCommandReason reason);
    ViewportEngineCommandResult accepted();
    [[nodiscard]] ViewportEngineCommandResult acceptedPreservingCommandDiagnostics() const;
    RevisionToken nextCommandRevision();

    std::unique_ptr<ViewportEngineCanonicalState> m_state;
};
