#pragma once

#include "imagesequence_p.h"
#include "timingintervals_p.h"
#include <ImageViewport/ImageViewport>

#include <QtGui/QImage>

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
    QSizeF logicalSize;
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

ImageSequenceSource makeImageSequenceSource(
    ImageSequence* sequence, std::shared_ptr<ImageSequence> owner = {});
ImageSequenceSource factorySequenceSource(ImageSequence* sequence);
bool sourceIsStill(const ImageSequenceSource& source);
int sourceFrameStartPosition(const ImageSequenceSource& source, int frame);
int sourceFrameIndexForPosition(const ImageSequenceSource& source, int position);
QSizeF sourceLogicalSize(const ImageSequenceSource& source);
QImage sourceFrameImage(const ImageSequenceSource& source, int frame);
FramePayloadFacts sourceFramePayloadFacts(const ImageSequenceSource& source, int frame);

}
