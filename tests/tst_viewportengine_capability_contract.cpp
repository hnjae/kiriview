#include "viewportenginecapabilities_p.h"

#include <type_traits>

template<typename Access>
using DisplayAccess = decltype(std::declval<Access&>().display());

template<typename Access>
using PresentationAccess = decltype(std::declval<Access&>().presentation());

static_assert(!std::is_copy_constructible_v<ViewportEngineProviderStateAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePlaybackStateAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineRenderStateAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEnginePresentationStateAccess>);
static_assert(!std::is_copy_constructible_v<ViewportEngineSnapshotStateAccess>);

static_assert(std::is_const_v<std::remove_reference_t<DisplayAccess<ViewportEnginePresentationStateAccess>>>);
static_assert(std::is_const_v<std::remove_reference_t<DisplayAccess<ViewportEngineSnapshotStateAccess>>>);
static_assert(std::is_const_v<std::remove_reference_t<PresentationAccess<ViewportEngineSnapshotStateAccess>>>);

int main() { }
