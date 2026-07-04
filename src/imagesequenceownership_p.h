#pragma once

#include "imageviewport.h"
#include "timingintervals_p.h"

#include <memory>

namespace ImageViewportInternal {

struct ImageSequenceSourceFacts
{
    bool present = false;
    bool provider = false;
    bool timed = false;
    int frameCount = -1;
    int totalDuration = -1;
    int firstFramePosition = -1;
    TimingIntervals timingIntervals;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
    bool hasCompleteProviderKnownMetadata = false;
    ImageSequenceProviderKnownFacts providerKnownFacts;
    QSizeF providerKnownLogicalSize;
    TimingIntervals providerKnownTimingIntervals;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderThreadingContract providerThreadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
};

struct ImageSequenceSource
{
    ImageSequence* sequence = nullptr;
    std::shared_ptr<ImageSequence> owner;
    ImageSequenceSourceFacts facts;
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory;
};

void registerFactorySequenceOwner(const std::shared_ptr<ImageSequence>& sequence);
ImageSequenceSource makeImageSequenceSource(
    ImageSequence* sequence, std::shared_ptr<ImageSequence> owner = {});
ImageSequenceSource factorySequenceSource(ImageSequence* sequence);
// Transitional wrapper for legacy owner-only callers until assignments store ImageSequenceSource.
std::shared_ptr<ImageSequence> factorySequenceOwner(ImageSequence* sequence);

}
