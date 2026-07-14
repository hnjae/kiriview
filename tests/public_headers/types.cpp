#include <ImageViewport/imageviewporttypes.h>

#include <type_traits>

static_assert(std::is_enum_v<ImageViewportFitMode>);
static_assert(std::is_enum_v<ImageViewportContentAnchor>);
static_assert(std::is_copy_constructible_v<ImageViewportRevisionToken>);
static_assert(std::is_copy_constructible_v<ImageViewportRoleSet>);
