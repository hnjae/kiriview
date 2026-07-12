#include "viewportenginefacadecontract_p.h"

#include <type_traits>

static_assert(std::is_default_constructible_v<ViewportEngineViewportInput>);
static_assert(std::is_default_constructible_v<ViewportEnginePresentationTargetAssignmentRequest>);
static_assert(std::is_default_constructible_v<ViewportEnginePresentationCommandRequest>);
static_assert(std::is_default_constructible_v<ViewportEnginePlaybackCommandRequest>);
static_assert(std::is_default_constructible_v<ViewportEngineRenderSynchronizationRequest>);
static_assert(std::is_default_constructible_v<ViewportEngineProviderHostEventRequest>);

int main() { }
