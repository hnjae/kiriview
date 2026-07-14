#include <ImageViewport/imageviewportstate.h>

#include <type_traits>

static_assert(std::is_copy_constructible_v<ImageViewportStateSnapshot>);
static_assert(std::is_copy_constructible_v<ImageViewportCommandResult>);
static_assert(std::is_copy_constructible_v<ImageViewportCoordinateInput>);
