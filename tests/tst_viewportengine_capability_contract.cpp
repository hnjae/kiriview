#include "viewportengine_p.h"
#include "viewportengineassignmentoperations_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineplaybackoperations_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportengineprojection_p.h"
#include "viewportengineprovidereventcompletionoperations_p.h"
#include "viewportengineproviderfailureoperations_p.h"
#include "viewportengineproviderframeoperations_p.h"
#include "viewportengineprovidermetadataoperations_p.h"
#include "viewportengineproviderprojection_p.h"
#include "viewportengineproviderrequestoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"
#include "viewportengineproviderterminaloperations_p.h"
#include "viewportenginerenderoperations_p.h"
#include "viewportenginetargetspreadoperations_p.h"

#include <type_traits>

template <typename Access> using DisplayAccess = decltype(std::declval<Access&>().display());

template <typename Access>
using PresentationAccess = decltype(std::declval<Access&>().presentation());

template <typename Access, typename = void> struct HasPlaybackAccess : std::false_type
{
};
template <typename Access>
struct HasPlaybackAccess<Access, std::void_t<decltype(std::declval<Access&>().playback())>>
    : std::true_type
{
};

template <typename Access, typename = void> struct HasProviderSessionAccess : std::false_type
{
};
template <typename Access>
struct HasProviderSessionAccess<Access,
    std::void_t<decltype(std::declval<Access&>().providerSessions())>> : std::true_type
{
};

template <typename Access, typename = void> struct HasProviderRequestAccess : std::false_type
{
};

template <typename Access, typename = void> struct HasRequestAccess : std::false_type
{
};
template <typename Access>
struct HasRequestAccess<Access, std::void_t<decltype(std::declval<Access&>().request())>>
    : std::true_type
{
};

template <typename Access, typename = void> struct HasDisplayStateAccess : std::false_type
{
};

template <typename Access, typename = void> struct HasRolesAccess : std::false_type
{
};
template <typename Access>
struct HasRolesAccess<Access, std::void_t<decltype(std::declval<Access&>().roles())>>
    : std::true_type
{
};

template <typename Input, typename = void> struct HasDiagnosticMember : std::false_type
{
};
template <typename Input>
struct HasDiagnosticMember<Input, std::void_t<decltype(std::declval<Input&>().diagnostic)>>
    : std::true_type
{
};

template <typename Access, typename = void> struct HasPresentationStateAccess : std::false_type
{
};

template <typename Engine, typename = void> struct HasPlaybackScheduleEffect : std::false_type
{
};
template <typename Engine, typename = void> struct HasPublicRejectInvalidCommand : std::false_type
{
};
template <typename Engine>
struct HasPublicRejectInvalidCommand<Engine,
    std::void_t<decltype(std::declval<Engine&>().rejectInvalidCommand())>> : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicRevisionAllocator : std::false_type
{
};
template <typename Engine>
struct HasPublicRevisionAllocator<Engine,
    std::void_t<decltype(std::declval<Engine&>().allocateRevisionValue())>> : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicChangePublication : std::false_type
{
};
template <typename Engine>
struct HasPublicChangePublication<Engine,
    std::void_t<decltype(std::declval<Engine&>().publishChanges(
        ImageViewportInternal::ViewportChangeSet {}))>> : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicAcceptedGeometryInput : std::false_type
{
};
template <typename Engine>
struct HasPublicAcceptedGeometryInput<Engine,
    std::void_t<decltype(std::declval<const Engine&>().acceptedGeometryInput(QRectF {}, 1.0))>>
    : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicProjectedGeometryInput : std::false_type
{
};
template <typename Engine>
struct HasPublicProjectedGeometryInput<Engine,
    std::void_t<decltype(std::declval<const Engine&>().projectedGeometryInput(QRectF {}, 1.0))>>
    : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicGeometryState : std::false_type
{
};
template <typename Engine>
struct HasPublicGeometryState<Engine,
    std::void_t<decltype(std::declval<const Engine&>().geometryState())>> : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicProviderReducer : std::false_type
{
};
template <typename Engine>
struct HasPublicProviderReducer<Engine,
    std::void_t<decltype(std::declval<Engine&>().reduceProviderEvent(
        std::declval<const ViewportProviderEvent&>(), ViewportEngineViewportState {}))>>
    : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicProviderRestaging : std::false_type
{
};
template <typename Engine>
struct HasPublicProviderRestaging<Engine,
    std::void_t<decltype(std::declval<Engine&>().restageProviderDemands(
        ViewportEngineViewportState {}))>> : std::true_type
{
};
template <typename Engine, typename = void> struct HasPublicProviderSessionClose : std::false_type
{
};
template <typename Engine>
struct HasPublicProviderSessionClose<Engine,
    std::void_t<decltype(std::declval<Engine&>().closeProviderSession(
        ImageViewportPageRole::Primary))>> : std::true_type
{
};
template <typename Engine>
struct HasPlaybackScheduleEffect<Engine,
    std::void_t<decltype(std::declval<const Engine&>().playbackScheduleEffect())>> : std::true_type
{
};
template <typename Access>
struct HasPresentationStateAccess<Access,
    std::void_t<decltype(std::declval<Access&>().presentation())>> : std::true_type
{
};
template <typename Access>
struct HasDisplayStateAccess<Access, std::void_t<decltype(std::declval<Access&>().display())>>
    : std::true_type
{
};
template <typename Access>
struct HasProviderRequestAccess<Access,
    std::void_t<decltype(std::declval<Access&>().providerRequests())>> : std::true_type
{
};

template <typename Request, typename = void> struct HasViewportMember : std::false_type
{
};

template <typename Event, typename = void> struct HasBorrowedImageFrameMember : std::false_type
{
};
template <typename Event>
struct HasBorrowedImageFrameMember<Event, std::void_t<decltype(std::declval<Event&>().imageFrame)>>
    : std::true_type
{
};
template <typename Request>
struct HasViewportMember<Request, std::void_t<decltype(std::declval<Request&>().viewport)>>
    : std::true_type
{
};

static_assert(!std::is_copy_constructible_v<ViewportEngineSnapshotStateAccess>);
static_assert(!std::is_default_constructible_v<ViewportEnginePresentationTargetAssignmentAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePresentationTargetAssignmentAccess>);
static_assert(
    std::is_same_v<decltype(ViewportEnginePresentationTargetAssignmentReduction::mutation),
        ViewportEnginePresentationTargetAssignmentMutation>);
static_assert(
    !std::is_reference_v<decltype(ViewportEnginePresentationTargetAssignmentMutation::request)>);
static_assert(
    !std::is_reference_v<decltype(ViewportEnginePresentationTargetAssignmentMutation::playback)>);
static_assert(
    !std::is_reference_v<decltype(ViewportEnginePresentationTargetAssignmentMutation::display)>);
static_assert(
    !std::is_reference_v<decltype(ViewportEnginePresentationTargetAssignmentMutation::roles)>);
static_assert(!HasRequestAccess<ViewportEnginePresentationTargetAssignmentAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePresentationTargetAssignmentAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEnginePresentationTargetAssignmentAccess>::value);
static_assert(!HasRolesAccess<ViewportEnginePresentationTargetAssignmentAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePresentationTargetAssignment),
    ViewportEnginePresentationTargetAssignmentReduction (*)(
        ViewportEnginePresentationTargetAssignmentInput,
        ViewportEnginePresentationTargetAssignmentAccess)>);
static_assert(!std::is_reference_v<decltype(ViewportEnginePlaybackMutation::request)>);
static_assert(!std::is_reference_v<decltype(ViewportEnginePlaybackMutation::playback)>);
static_assert(!std::is_reference_v<decltype(ViewportEnginePlaybackMutation::display)>);
static_assert(!std::is_reference_v<decltype(ViewportEnginePlaybackMutation::roles)>);
static_assert(!std::is_reference_v<decltype(ViewportEnginePlaybackMutation::nextRevision)>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderSessionOpenAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderSessionOpenAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderSessionMutation::session)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderSessionMutation::requests)>);
static_assert(!HasRequestAccess<ViewportEngineProviderSessionOpenAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderSessionOpenAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderSessionOpenAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderSessionOpenAccess>::value);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderSessionCloseAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderSessionCloseAccess>);
static_assert(!HasRequestAccess<ViewportEngineProviderSessionCloseAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderSessionCloseAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderSessionCloseAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderSessionCloseAccess>::value);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderSessionAdmissionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderSessionAdmissionAccess>);
static_assert(!HasRequestAccess<ViewportEngineProviderSessionAdmissionAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderSessionAdmissionAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderSessionAdmissionAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderSessionAdmissionAccess>::value);
static_assert(std::is_same_v<decltype(&beginViewportEngineProviderSession),
    ViewportEngineProviderSessionOpenEffect (*)(
        ViewportEngineProviderSessionOpenInput, ViewportEngineProviderSessionOpenAccess&)>);
static_assert(std::is_same_v<decltype(&closeViewportEngineProviderSession),
    ViewportProviderFrameTransportEffect (*)(ViewportEngineProviderSessionCloseAccess&)>);
static_assert(std::is_same_v<decltype(&acceptsViewportEngineProviderSessionEvent),
    bool (*)(ViewportEngineProviderSessionAdmissionInput,
        ViewportEngineProviderSessionAdmissionAccess)>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderSessionOpenedAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderSessionOpenedAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderRequestMutation::request)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderRequestMutation::playback)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderRequestMutation::display)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderRequestMutation::roles)>);
static_assert(
    !std::is_reference_v<decltype(ViewportProviderRequestTokenAllocationMutation::roles)>);
static_assert(
    !std::is_reference_v<decltype(ViewportEngineProviderRoleMaterializationMutation::request)>);
static_assert(
    !std::is_reference_v<decltype(ViewportEngineProviderRoleMaterializationMutation::display)>);
static_assert(!HasRequestAccess<ViewportEngineProviderSessionOpenedAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderSessionOpenedAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderSessionOpenedAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderSessionOpenedAccess>::value);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderQueueFlushAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderQueueFlushAccess>);
static_assert(!HasRequestAccess<ViewportEngineProviderQueueFlushAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderQueueFlushAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderQueueFlushAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderQueueFlushAccess>::value);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderDemandRestageAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderDemandRestageAccess>);
static_assert(!HasRequestAccess<ViewportEngineProviderDemandRestageAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderDemandRestageAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderDemandRestageAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderDemandRestageAccess>::value);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderMetadataReadyAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderMetadataReadyAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderMetadataMutation::request)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderMetadataMutation::playback)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderMetadataMutation::display)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderMetadataMutation::roles)>);
static_assert(!HasRequestAccess<ViewportEngineProviderMetadataReadyAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderMetadataReadyAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderMetadataReadyAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderMetadataReadyAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderMetadataReady),
    ViewportEngineProviderMetadataReadyReduction (*)(
        ViewportEngineProviderMetadataReadyInput, ViewportEngineProviderMetadataReadyAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderFrameReadyAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderFrameReadyAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFrameReadyMutation::request)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFrameReadyMutation::playback)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFrameReadyMutation::display)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFrameReadyMutation::provider)>);
static_assert(!HasRequestAccess<ViewportEngineProviderFrameReadyAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderFrameReadyAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderFrameReadyAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderFrameReadyAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEngineProviderFrameReadyAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineProviderFrameReadyAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderFrameReady),
    ViewportEngineProviderFrameReadyReduction (*)(
        ViewportEngineProviderFrameReadyInput, ViewportEngineProviderFrameReadyAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderTerminalEventAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderTerminalEventAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFailureMutation::request)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFailureMutation::playback)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFailureMutation::session)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderFailureMutation::requests)>);
static_assert(!HasRequestAccess<ViewportEngineProviderTerminalEventAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderTerminalEventAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderTerminalEventAccess>::value);
static_assert(!HasDiagnosticMember<ViewportEngineProviderTerminalEventInput>::value);
static_assert(!HasDiagnosticMember<ViewportEngineProviderDispatchFailureInput>::value);
static_assert(!HasDiagnosticMember<ViewportEngineProviderSessionOpenFailureInput>::value);
static_assert(!HasDiagnosticMember<ViewportEngineProviderQueueFailureInput>::value);
static_assert(std::is_same_v<decltype(ViewportEngineTargetSpreadTerminalInput::diagnostic),
    ImageViewportInternal::PublicDiagnosticText>);
static_assert(std::is_same_v<decltype(ImageViewportInternal::RequestState::errorString),
    ImageViewportInternal::PublicDiagnosticText>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderTerminalEvent),
    ViewportEngineProviderTerminalEventReduction (*)(
        ViewportEngineProviderTerminalEventInput, ViewportEngineProviderTerminalEventAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderDispatchFailureAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderDispatchFailureAccess>);
static_assert(!HasRequestAccess<ViewportEngineProviderDispatchFailureAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderDispatchFailureAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderDispatchFailureAccess>::value);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderSessionOpenFailureAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderSessionOpenFailureAccess>);
static_assert(!HasRequestAccess<ViewportEngineProviderSessionOpenFailureAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderSessionOpenFailureAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderSessionOpenFailureAccess>::value);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderQueueFailureAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderQueueFailureAccess>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderWaitingAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderWaitingAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderWaitingMutation::request)>);
static_assert(!HasRequestAccess<ViewportEngineProviderWaitingAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderWaitingAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderWaiting),
    ViewportEngineProviderWaitingReduction (*)(
        ViewportEngineProviderWaitingInput, ViewportEngineProviderWaitingAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderEndOfSequenceAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderEndOfSequenceAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderEndOfSequenceMutation::request)>);
static_assert(
    !std::is_reference_v<decltype(ViewportEngineProviderEndOfSequenceMutation::playback)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderEndOfSequenceMutation::display)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineProviderEndOfSequenceMutation::roles)>);
static_assert(!HasRequestAccess<ViewportEngineProviderEndOfSequenceAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderEndOfSequenceAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderEndOfSequence),
    ViewportEngineProviderEndOfSequenceReduction (*)(
        ViewportEngineProviderEndOfSequenceInput, ViewportEngineProviderEndOfSequenceAccess&)>);
static_assert(!HasRequestAccess<ViewportEngineProviderQueueFailureAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderQueueFailureAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderQueueFailureAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderDispatchFailure),
    ViewportEngineProviderTerminalEventReduction (*)(
        ViewportEngineProviderDispatchFailureInput, ViewportEngineProviderDispatchFailureAccess&)>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderSessionOpenFailure),
    ViewportEngineProviderSessionOpenFailureReduction (*)(
        ViewportEngineProviderSessionOpenFailureInput,
        ViewportEngineProviderSessionOpenFailureAccess&)>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderQueueFailure),
    ViewportEngineProviderQueueFailureReduction (*)(
        ViewportEngineProviderQueueFailureInput, ViewportEngineProviderQueueFailureAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineProviderTerminalProjectionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderTerminalProjectionAccess>);
static_assert(
    !std::is_reference_v<decltype(ViewportEngineProviderTerminalProjectionMutation::request)>);
static_assert(!HasRequestAccess<ViewportEngineProviderTerminalProjectionAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineProviderTerminalProjectionAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineProviderTerminalProjectionAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineProviderTerminalProjectionAccess>::value);
static_assert(
    std::is_same_v<decltype(&reduceViewportEngineProviderDisplayRequestTerminalProjection),
        ImageViewportInternal::ViewportChangeSet (*)(ViewportEngineProviderTerminalProjectionInput,
            ViewportEngineProviderTerminalProjectionAccess&)>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineProviderGenerationTerminalProjection),
    ImageViewportInternal::ViewportChangeSet (*)(ViewportEngineProviderTerminalProjectionInput,
        ViewportEngineProviderTerminalProjectionAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineTransition>);
static_assert(!std::is_copy_constructible_v<ViewportEngineTransition>);
static_assert(std::is_move_constructible_v<ViewportEngineTransition>);
static_assert(!HasPublicChangePublication<ViewportEngine>::value);

static_assert(
    std::is_const_v<std::remove_reference_t<DisplayAccess<ViewportEngineSnapshotStateAccess>>>);
static_assert(std::is_const_v<
    std::remove_reference_t<PresentationAccess<ViewportEngineSnapshotStateAccess>>>);
static_assert(std::is_same_v<decltype(&projectViewportStateSnapshot),
    ImageViewportStateSnapshot (*)(
        ViewportEngineSnapshotInput, ViewportEngineSnapshotStateAccess)>);
static_assert(!std::is_copy_constructible_v<ViewportEngineCurrentGeometryProjectionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePendingGeometryProjectionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineAcceptedGeometryProjectionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderSnapshotProjectionAccess>);
static_assert(std::is_same_v<decltype(&projectViewportCurrentGeometry),
    ViewportEngineGeometryInput (*)(
        ViewportEngineGeometryQueryInput, ViewportEngineCurrentGeometryProjectionAccess)>);
static_assert(std::is_same_v<decltype(&projectViewportPendingGeometry),
    ViewportEngineGeometryInput (*)(
        ViewportEngineGeometryQueryInput, ViewportEnginePendingGeometryProjectionAccess)>);
static_assert(std::is_same_v<decltype(&projectViewportAcceptedGeometry),
    ViewportEngineGeometryInput (*)(
        ViewportEngineGeometryQueryInput, ViewportEngineAcceptedGeometryProjectionAccess)>);
static_assert(std::is_same_v<decltype(&projectViewportRenderSnapshot),
    ViewportRenderSnapshot (*)(
        ViewportRenderSnapshotInput, ViewportEngineRenderSnapshotProjectionAccess)>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderSynchronizationAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderCommitAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderFailureAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineRenderSynchronizationMutation::render)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineRenderMutation::request)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineRenderMutation::display)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineRenderMutation::playback)>);
static_assert(std::is_same_v<decltype(&synchronizeViewportEngineRender),
    ViewportEngineRenderCoordinationState::AttemptContext (*)(
        ViewportEngineRenderSynchronizationInput, ViewportEngineRenderSynchronizationAccess&)>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineRenderCommit),
    ViewportEngineRenderCommitReduction (*)(
        ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderCommitAccess&)>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineRenderFailure),
    ViewportEngineRenderFailureReduction (*)(
        ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderFailureAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineGeometryChangeAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineGeometryChangeAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineGeometryChangeMutation::request)>);
static_assert(!std::is_reference_v<decltype(ViewportEngineGeometryChangeMutation::display)>);
static_assert(!HasRequestAccess<ViewportEngineGeometryChangeAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineGeometryChangeAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineGeometryChangeAccess>::value);
static_assert(!HasPlaybackAccess<ViewportEngineGeometryChangeAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEngineGeometryChangeAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEngineGeometryChangeAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineGeometryChangeAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEngineGeometryChange),
    ViewportEngineGeometryChangeReduction (*)(
        ViewportEngineGeometryChangeInput, ViewportEngineGeometryChangeAccess&)>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderDemandProjectionAccess>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEngineProviderDemandProjectionAccess&>().request())>>);
static_assert(std::is_const_v<
    std::remove_reference_t<DisplayAccess<ViewportEngineProviderDemandProjectionAccess>>>);
static_assert(std::is_const_v<
    std::remove_reference_t<PresentationAccess<ViewportEngineProviderDemandProjectionAccess>>>);
static_assert(!HasPlaybackAccess<ViewportEngineProviderDemandProjectionAccess>::value);
static_assert(!std::is_same_v<ImageViewportInternal::ProviderSessionState,
    ImageViewportInternal::ProviderRequestLedger>);
static_assert(!std::is_same_v<ImageViewportInternal::ProviderSessionState,
    ImageViewportInternal::ProviderFactsState>);
static_assert(!std::is_same_v<ImageViewportInternal::ProviderRequestLedger,
    ImageViewportInternal::ProviderFactsState>);
static_assert(
    std::is_same_v<decltype(ViewportProviderEvent::kind), ImageSequenceProviderEventKind>);
static_assert(!HasBorrowedImageFrameMember<ViewportProviderEvent>::value);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEngineProviderDemandProjectionAccess&>().providerFacts())>>);
static_assert(!HasProviderSessionAccess<ViewportEngineProviderDemandProjectionAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineProviderDemandProjectionAccess>::value);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEngineSnapshotStateAccess&>().providerFacts())>>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEnginePendingGeometryProjectionAccess&>().providerFacts())>>);
static_assert(std::is_const_v<
    std::remove_reference_t<decltype(std::declval<ViewportEngineAcceptedGeometryProjectionAccess&>()
            .providerFacts())>>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEngineRenderCommitAccess&>().providerFacts())>>);
static_assert(!HasProviderSessionAccess<ViewportEngineSnapshotStateAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineSnapshotStateAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEnginePendingGeometryProjectionAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEnginePendingGeometryProjectionAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePendingGeometryProjectionAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEngineAcceptedGeometryProjectionAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineAcceptedGeometryProjectionAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEngineRenderCommitAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineRenderCommitAccess>::value);
static_assert(std::is_same_v<decltype(&projectViewportProviderDemand),
    ImageSequenceProviderDisplayDemand (*)(
        ViewportEngineProviderDemandInput, ViewportEngineProviderDemandProjectionAccess)>);
static_assert(!std::is_default_constructible_v<ViewportProviderRequestTokenAllocationAccess>);
static_assert(!std::is_copy_constructible_v<ViewportProviderRequestTokenAllocationAccess>);
static_assert(!std::is_copy_assignable_v<ViewportProviderRequestTokenAllocationAccess>);
static_assert(std::is_same_v<decltype(&allocateViewportProviderRequestToken),
    ViewportProviderRequestTokenAllocationResult (*)(ViewportProviderRequestTokenAllocationInput,
        ViewportProviderRequestTokenAllocationAccess&)>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePresentationCommandStateView>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePresentationTargetTransitionStateView>);
static_assert(!std::is_default_constructible_v<ViewportEnginePresentationCommandStateView>);
static_assert(
    !std::is_default_constructible_v<ViewportEnginePresentationTargetTransitionStateView>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEnginePresentationCommandStateView&>().presentation())>>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEnginePresentationTargetTransitionStateView&>()
                .presentation())>>);
static_assert(!HasRequestAccess<ViewportEnginePresentationCommandStateView>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePresentationCommandStateView>::value);
static_assert(!HasPlaybackAccess<ViewportEnginePresentationCommandStateView>::value);
static_assert(!HasProviderSessionAccess<ViewportEnginePresentationCommandStateView>::value);
static_assert(!HasProviderRequestAccess<ViewportEnginePresentationCommandStateView>::value);
static_assert(!HasRequestAccess<ViewportEnginePresentationTargetTransitionStateView>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePresentationTargetTransitionStateView>::value);
static_assert(!HasPlaybackAccess<ViewportEnginePresentationTargetTransitionStateView>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePresentationCommand),
    ViewportEnginePresentationCommandReduction (*)(
        ViewportEnginePresentationCommandInput, ViewportEnginePresentationCommandStateView)>);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePresentationTargetTransition),
    ViewportEnginePresentationTargetTransitionReduction (*)(
        ViewportEnginePresentationTargetTransitionInput,
        ViewportEnginePresentationTargetTransitionStateView)>);
static_assert(!std::is_default_constructible_v<ViewportEnginePlaybackScheduleAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackScheduleAccess>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEnginePlaybackScheduleAccess&>().request())>>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEnginePlaybackScheduleAccess&>().playback())>>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<ViewportEnginePlaybackScheduleAccess&>().providerFacts())>>);
static_assert(!HasDisplayStateAccess<ViewportEnginePlaybackScheduleAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEnginePlaybackScheduleAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEnginePlaybackScheduleAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEnginePlaybackScheduleAccess>::value);
static_assert(std::is_same_v<decltype(&projectViewportPlaybackSchedule),
    ViewportPlaybackScheduleEffect (*)(ViewportEnginePlaybackScheduleAccess)>);
static_assert(!HasPlaybackScheduleEffect<ViewportEngine>::value);
static_assert(!HasPublicRejectInvalidCommand<ViewportEngine>::value);
static_assert(!HasPublicRevisionAllocator<ViewportEngine>::value);
static_assert(!HasPublicAcceptedGeometryInput<ViewportEngine>::value);
static_assert(!HasPublicProjectedGeometryInput<ViewportEngine>::value);
static_assert(!HasPublicGeometryState<ViewportEngine>::value);
static_assert(!HasPublicProviderReducer<ViewportEngine>::value);
static_assert(!HasPublicProviderRestaging<ViewportEngine>::value);
static_assert(!HasPublicProviderSessionClose<ViewportEngine>::value);
static_assert(std::is_same_v<decltype(&ViewportEngine::handleProviderHostEvent),
    ViewportEngineTransition (ViewportEngine::*)(const ViewportEngineProviderHostEventRequest&)>);
static_assert(std::is_same_v<decltype(&ViewportEngine::handleViewportChanged),
    ViewportEngineTransition (ViewportEngine::*)(ViewportEngineViewportState)>);
static_assert(std::is_same_v<decltype(std::declval<const ViewportEngine&>().snapshot()),
    ImageViewportStateSnapshot>);
static_assert(std::is_same_v<decltype(&ViewportEngine::queryCoordinate),
    ViewportEngineCoordinateQueryResult (ViewportEngine::*)(
        const ViewportEngineCoordinateQueryRequest&) const>);
static_assert(std::is_same_v<decltype(std::declval<ViewportEngine&>().beginRenderSynchronization()),
    ViewportRenderAttempt>);
static_assert(!HasViewportMember<ViewportEnginePresentationTargetAssignmentRequest>::value);
static_assert(!HasViewportMember<ViewportEnginePlaybackCommandRequest>::value);
static_assert(!HasViewportMember<ViewportEnginePlaybackTickRequest>::value);
static_assert(!HasViewportMember<ViewportEnginePresentationCommandRequest>::value);
static_assert(!HasViewportMember<ViewportEngineProviderHostEventRequest>::value);
static_assert(!std::is_default_constructible_v<ViewportEnginePlaybackPauseAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackPauseAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEnginePlaybackPauseMutation::playback)>);
static_assert(!HasRequestAccess<ViewportEnginePlaybackPauseAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePlaybackPauseAccess>::value);
static_assert(!HasRolesAccess<ViewportEnginePlaybackPauseAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEnginePlaybackPauseAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEnginePlaybackPauseAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEnginePlaybackPauseAccess>::value);
static_assert(
    std::is_same_v<decltype(&validateViewportPlaybackCommand), bool (*)(ViewportPlaybackCommand)>);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePlaybackPause),
    ViewportEnginePlaybackPauseReduction (*)(
        ViewportEnginePlaybackPauseInput, ViewportEnginePlaybackPauseAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEnginePlaybackStopAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackStopAccess>);
static_assert(!HasRequestAccess<ViewportEnginePlaybackStopAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePlaybackStopAccess>::value);
static_assert(!HasRolesAccess<ViewportEnginePlaybackStopAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEnginePlaybackStopAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePlaybackStop),
    ViewportEnginePlaybackStopReduction (*)(
        ViewportEnginePlaybackStopInput, ViewportEnginePlaybackStopAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEnginePlaybackSeekAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackSeekAccess>);
static_assert(!HasRequestAccess<ViewportEnginePlaybackSeekAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePlaybackSeekAccess>::value);
static_assert(!HasRolesAccess<ViewportEnginePlaybackSeekAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEnginePlaybackSeekAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePlaybackSeek),
    ViewportEnginePlaybackSeekReduction (*)(
        ViewportEnginePlaybackSeekInput, ViewportEnginePlaybackSeekAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEnginePlaybackPlayAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackPlayAccess>);
static_assert(!HasRequestAccess<ViewportEnginePlaybackPlayAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePlaybackPlayAccess>::value);
static_assert(!HasRolesAccess<ViewportEnginePlaybackPlayAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEnginePlaybackPlayAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePlaybackPlay),
    ViewportEnginePlaybackPlayReduction (*)(
        ViewportEnginePlaybackPlayInput, ViewportEnginePlaybackPlayAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEnginePlaybackTickAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackTickAccess>);
static_assert(!HasRequestAccess<ViewportEnginePlaybackTickAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEnginePlaybackTickAccess>::value);
static_assert(!HasRolesAccess<ViewportEnginePlaybackTickAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEnginePlaybackTickAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEnginePlaybackTick),
    ViewportEnginePlaybackTickReduction (*)(
        ViewportEnginePlaybackTickInput, ViewportEnginePlaybackTickAccess&)>);
static_assert(!std::is_default_constructible_v<ViewportEngineAuthoredAutoplayAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineAuthoredAutoplayAccess>);
static_assert(!std::is_reference_v<decltype(ViewportEngineAuthoredAutoplayMutation::playback)>);
static_assert(std::is_const_v<
    std::remove_reference_t<decltype(std::declval<ViewportEngineAuthoredAutoplayAccess&>().source(
        ImageViewportPageRole::Primary))>>);
static_assert(std::is_const_v<
    std::remove_reference_t<decltype(std::declval<ViewportEngineAuthoredAutoplayAccess&>()
            .providerFacts(ImageViewportPageRole::Primary))>>);
static_assert(std::is_const_v<
    std::remove_reference_t<decltype(std::declval<ViewportEngineAuthoredAutoplayAccess&>()
            .activeRequest(ImageViewportPageRole::Primary))>>);
static_assert(!HasRequestAccess<ViewportEngineAuthoredAutoplayAccess>::value);
static_assert(!HasDisplayStateAccess<ViewportEngineAuthoredAutoplayAccess>::value);
static_assert(!HasRolesAccess<ViewportEngineAuthoredAutoplayAccess>::value);
static_assert(!HasPresentationStateAccess<ViewportEngineAuthoredAutoplayAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEngineAuthoredAutoplayAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineAuthoredAutoplayAccess>::value);
static_assert(std::is_same_v<decltype(&reduceViewportEngineAuthoredAutoplay),
    ViewportEngineAuthoredAutoplayReduction (*)(
        ViewportEngineAuthoredAutoplayInput, ViewportEngineAuthoredAutoplayAccess&)>);

int main() { }
