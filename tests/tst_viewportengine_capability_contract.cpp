#include "viewportenginecapabilities_p.h"
#include "viewportengine_p.h"
#include "viewportengineprojection_p.h"
#include "viewportengineproviderprojection_p.h"

#include <type_traits>

template<typename Access>
using DisplayAccess = decltype(std::declval<Access&>().display());

template<typename Access>
using PresentationAccess = decltype(std::declval<Access&>().presentation());

template<typename Access, typename = void>
struct HasPlaybackAccess : std::false_type { };
template<typename Access>
struct HasPlaybackAccess<Access,
    std::void_t<decltype(std::declval<Access&>().playback())>> : std::true_type { };

template<typename Access, typename = void>
struct HasProviderSessionAccess : std::false_type { };
template<typename Access>
struct HasProviderSessionAccess<Access,
    std::void_t<decltype(std::declval<Access&>().providerSessions())>> : std::true_type { };

template<typename Access, typename = void>
struct HasProviderRequestAccess : std::false_type { };
template<typename Access>
struct HasProviderRequestAccess<Access,
    std::void_t<decltype(std::declval<Access&>().providerRequests())>> : std::true_type { };

static_assert(!std::is_copy_constructible_v<ViewportEngineProviderStateAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackStateAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineGeometryTransitionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePresentationStateAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineSnapshotStateAccess>);
static_assert(!std::is_default_constructible_v<ViewportEngine::PendingPublication>);
static_assert(!std::is_copy_constructible_v<ViewportEngine::PendingPublication>);
static_assert(std::is_move_constructible_v<ViewportEngine::PendingPublication>);

static_assert(std::is_const_v<std::remove_reference_t<DisplayAccess<ViewportEnginePresentationStateAccess>>>);
static_assert(std::is_const_v<std::remove_reference_t<DisplayAccess<ViewportEngineSnapshotStateAccess>>>);
static_assert(std::is_const_v<std::remove_reference_t<PresentationAccess<ViewportEngineSnapshotStateAccess>>>);
static_assert(std::is_same_v<decltype(&projectViewportStateSnapshot),
    ImageViewportStateSnapshot (*)(ViewportEngineSnapshotInput,
        ViewportEngineSnapshotStateAccess)>);
static_assert(!std::is_copy_constructible_v<ViewportEngineCurrentGeometryProjectionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePendingGeometryProjectionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineAcceptedGeometryProjectionAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderSnapshotProjectionAccess>);
static_assert(std::is_same_v<decltype(&projectViewportCurrentGeometry),
    ViewportEngineGeometryInput (*)(ViewportEngineGeometryQueryInput,
        ViewportEngineCurrentGeometryProjectionAccess)>);
static_assert(std::is_same_v<decltype(&projectViewportPendingGeometry),
    ViewportEngineGeometryInput (*)(ViewportEngineGeometryQueryInput,
        ViewportEnginePendingGeometryProjectionAccess)>);
static_assert(std::is_same_v<decltype(&projectViewportAcceptedGeometry),
    ViewportEngineGeometryInput (*)(ViewportEngineGeometryQueryInput,
        ViewportEngineAcceptedGeometryProjectionAccess)>);
static_assert(std::is_same_v<decltype(&projectViewportRenderSnapshot),
    ViewportRenderSnapshot (*)(ViewportRenderSnapshotInput,
        ViewportEngineRenderSnapshotProjectionAccess)>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderSynchronizationAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderCommitAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderFailureAccess>);
static_assert(std::is_same_v<decltype(&synchronizeViewportEngineRender),
    ViewportRenderSynchronization (*)(ViewportEngineRenderSynchronizationInput,
        ViewportEngineRenderSynchronizationAccess)>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineRenderCommit),
    ViewportEngineRenderCommitReduction (*)(ViewportEngineRenderAcknowledgementInput,
        ViewportEngineRenderCommitAccess)>);
static_assert(std::is_same_v<decltype(&reduceViewportEngineRenderFailure),
    ViewportEngineRenderFailureReduction (*)(ViewportEngineRenderAcknowledgementInput,
        ViewportEngineRenderFailureAccess)>);
static_assert(!std::is_copy_constructible_v<ViewportEngineProviderDemandProjectionAccess>);
static_assert(std::is_const_v<std::remove_reference_t<
    decltype(std::declval<ViewportEngineProviderDemandProjectionAccess&>().request())>>);
static_assert(std::is_const_v<std::remove_reference_t<
    DisplayAccess<ViewportEngineProviderDemandProjectionAccess>>>);
static_assert(std::is_const_v<std::remove_reference_t<
    PresentationAccess<ViewportEngineProviderDemandProjectionAccess>>>);
static_assert(!HasPlaybackAccess<ViewportEngineProviderDemandProjectionAccess>::value);
static_assert(!std::is_same_v<ImageViewportInternal::ProviderSessionState,
    ImageViewportInternal::ProviderRequestState>);
static_assert(!std::is_same_v<ImageViewportInternal::ProviderSessionState,
    ImageViewportInternal::ProviderFactsState>);
static_assert(!std::is_same_v<ImageViewportInternal::ProviderRequestState,
    ImageViewportInternal::ProviderFactsState>);
static_assert(std::is_const_v<std::remove_reference_t<decltype(
    std::declval<ViewportEngineProviderDemandProjectionAccess&>().providerFacts())>>);
static_assert(!HasProviderSessionAccess<ViewportEngineProviderDemandProjectionAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineProviderDemandProjectionAccess>::value);
static_assert(std::is_const_v<std::remove_reference_t<decltype(
    std::declval<ViewportEngineSnapshotStateAccess&>().providerFacts())>>);
static_assert(std::is_const_v<std::remove_reference_t<decltype(
    std::declval<ViewportEnginePendingGeometryProjectionAccess&>().providerFacts())>>);
static_assert(std::is_const_v<std::remove_reference_t<decltype(
    std::declval<ViewportEngineAcceptedGeometryProjectionAccess&>().providerFacts())>>);
static_assert(std::is_const_v<std::remove_reference_t<decltype(
    std::declval<ViewportEngineRenderCommitAccess&>().providerFacts())>>);
static_assert(!HasProviderSessionAccess<ViewportEngineSnapshotStateAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineSnapshotStateAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEnginePendingGeometryProjectionAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEnginePendingGeometryProjectionAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEngineAcceptedGeometryProjectionAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineAcceptedGeometryProjectionAccess>::value);
static_assert(!HasProviderSessionAccess<ViewportEngineRenderCommitAccess>::value);
static_assert(!HasProviderRequestAccess<ViewportEngineRenderCommitAccess>::value);
static_assert(std::is_same_v<decltype(&projectViewportProviderDemand),
    ImageSequenceProviderDisplayDemand (*)(ViewportEngineProviderDemandInput,
        ViewportEngineProviderDemandProjectionAccess)>);

int main() { }
