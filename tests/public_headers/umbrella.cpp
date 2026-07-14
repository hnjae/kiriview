#include <ImageViewport/ImageViewport>

#include <type_traits>

static_assert(std::is_base_of_v<QObject, ImageSequenceProviderSession>);
static_assert(std::is_base_of_v<QQuickItem, ImageViewport>);
static_assert(std::is_copy_constructible_v<ImageViewportStateSnapshot>);
