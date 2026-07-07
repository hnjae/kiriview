#include "imageviewport_p.h"

using namespace ImageViewportInternal;

bool ImageViewportPrivate::providerHasCompleteKnownMetadata() const
{
    return controller.requestState().sequenceSource.facts.hasCompleteProviderKnownMetadata;
}

ImageSequenceProviderKnownFacts ImageViewportPrivate::providerKnownFacts() const
{
    return controller.requestState().sequenceSource.facts.providerKnownFacts;
}

QSizeF ImageViewportPrivate::providerKnownLogicalSize() const
{
    return controller.requestState().sequenceSource.facts.providerKnownLogicalSize;
}

TimingIntervals ImageViewportPrivate::providerKnownTimingIntervals() const
{
    return controller.requestState().sequenceSource.facts.providerKnownTimingIntervals;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerTimedPlaybackCapability() const
{
    return controller.requestState().sequenceSource.facts.providerTimedPlaybackCapability;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerFrameSeekCapability() const
{
    return controller.requestState().sequenceSource.facts.providerFrameSeekCapability;
}

ImageSequenceProviderCapabilitySupport ImageViewportPrivate::providerPositionSeekCapability() const
{
    return controller.requestState().sequenceSource.facts.providerPositionSeekCapability;
}

ImageSequenceProviderKnownFacts ImageViewportPrivate::secondaryProviderKnownFacts() const
{
    return controller.requestState().secondarySequenceSource.facts.providerKnownFacts;
}

QSizeF ImageViewportPrivate::secondaryProviderKnownLogicalSize() const
{
    return controller.requestState().secondarySequenceSource.facts.providerKnownLogicalSize;
}

TimingIntervals ImageViewportPrivate::secondaryProviderKnownTimingIntervals() const
{
    return controller.requestState().secondarySequenceSource.facts.providerKnownTimingIntervals;
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderTimedPlaybackCapability() const
{
    return controller.requestState().secondarySequenceSource.facts.providerTimedPlaybackCapability;
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderFrameSeekCapability() const
{
    return controller.requestState().secondarySequenceSource.facts.providerFrameSeekCapability;
}

ImageSequenceProviderCapabilitySupport
ImageViewportPrivate::secondaryProviderPositionSeekCapability() const
{
    return controller.requestState()
        .secondarySequenceSource.facts.providerPositionSeekCapability;
}

int ImageViewportPrivate::providerFrameStartPosition(int frame) const
{
    return controller.providerFrameStartPosition(frame);
}

int ImageViewportPrivate::providerFrameIndexForPosition(int position) const
{
    return controller.providerFrameIndexForPosition(position);
}

ImageSequenceAuthoredAnimationFacts ImageViewportPrivate::providerAuthoredAnimationFacts() const
{
    const ImageSequenceSource& source = controller.requestState().sequenceSource;
    if (!source.sequence) {
        return {};
    }
    if (controller.providerMetadataReady()) {
        return controller.providerAuthoredAnimationFacts();
    }
    return source.facts.authoredAnimationFacts;
}
