#pragma once

#include "imageviewport.h"

#include <memory>

namespace ImageViewportInternal {

void registerFactorySequenceOwner(const std::shared_ptr<ImageSequence>& sequence);
std::shared_ptr<ImageSequence> factorySequenceOwner(ImageSequence* sequence);

}
