#include <ImageViewport/imagesequenceprovider.h>

#include <type_traits>

static_assert(std::is_base_of_v<QObject, ImageSequenceProviderSession>);
static_assert(std::is_enum_v<ImageSequenceProviderRequestKind>);
static_assert(std::is_copy_constructible_v<ImageSequenceProviderDescriptor>);
