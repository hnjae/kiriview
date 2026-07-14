#include <ImageViewport/imagesequence.h>

#include <type_traits>

static_assert(std::is_base_of_v<QObject, ImageSequence>);
static_assert(std::is_base_of_v<QObject, ImageFrame>);
static_assert(std::is_enum_v<ImageSequenceFactoryOutcome>);
