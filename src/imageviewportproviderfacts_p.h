#pragma once

#include "imageviewport.h"

namespace ImageViewportInternal {

inline ImageViewport::CapabilitySupport providerCapabilitySupport(
    ImageSequenceProviderCapabilitySupport support)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::DeclaredFalse:
    case ImageSequenceProviderCapabilitySupport::KnownFalse:
        return ImageViewport::CapabilitySupport::False;
    case ImageSequenceProviderCapabilitySupport::DeclaredTrue:
    case ImageSequenceProviderCapabilitySupport::KnownTrue:
        return ImageViewport::CapabilitySupport::True;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return ImageViewport::CapabilitySupport::Unavailable;
    }

    return ImageViewport::CapabilitySupport::Unavailable;
}

inline bool providerCapabilityContradictsMetadata(
    ImageSequenceProviderCapabilitySupport support, bool metadataCapability)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::DeclaredFalse:
    case ImageSequenceProviderCapabilitySupport::KnownFalse:
        return metadataCapability;
    case ImageSequenceProviderCapabilitySupport::DeclaredTrue:
    case ImageSequenceProviderCapabilitySupport::KnownTrue:
        return !metadataCapability;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return false;
    }

    return false;
}

inline bool providerCapabilityKnownFalse(ImageSequenceProviderCapabilitySupport support)
{
    return support == ImageSequenceProviderCapabilitySupport::DeclaredFalse
        || support == ImageSequenceProviderCapabilitySupport::KnownFalse;
}

inline bool providerCapabilityKnownTrue(ImageSequenceProviderCapabilitySupport support)
{
    return support == ImageSequenceProviderCapabilitySupport::DeclaredTrue
        || support == ImageSequenceProviderCapabilitySupport::KnownTrue;
}

inline bool providerResolvedCapability(
    ImageSequenceProviderCapabilitySupport support, bool defaultSupport)
{
    if (providerCapabilityKnownFalse(support)) {
        return false;
    }
    if (providerCapabilityKnownTrue(support)) {
        return true;
    }
    return defaultSupport;
}

inline bool providerFactsContradictCapabilities(const ImageSequenceProviderKnownFacts& facts,
    ImageSequenceProviderCapabilitySupport timedPlaybackSupport,
    ImageSequenceProviderCapabilitySupport frameSeekSupport,
    ImageSequenceProviderCapabilitySupport positionSeekSupport)
{
    if (!facts.isSpecified() || facts.isLogicalSizeOnly()) {
        return false;
    }

    const bool timedFacts = facts.isTimedFrameCount() || facts.isTimedFrameList();
    if (facts.isStill() && providerCapabilityKnownFalse(frameSeekSupport)) {
        return true;
    }
    return providerCapabilityKnownTrue(timedPlaybackSupport) && !timedFacts
        || providerCapabilityKnownTrue(positionSeekSupport) && !timedFacts;
}

inline bool providerFactsContradictMetadata(
    const ImageSequenceProviderKnownFacts& facts, const ImageSequenceProviderMetadata& metadata)
{
    if (!facts.isSpecified()) {
        return false;
    }
    if (metadata.logicalSize() != facts.logicalSize()) {
        return true;
    }
    if (facts.isLogicalSizeOnly()) {
        return false;
    }
    if (facts.isStill()) {
        return !metadata.isStill();
    }
    if (facts.isTimedFrameCount()) {
        return !metadata.isTimedFrameList()
            || metadata.frameDurations().size() != facts.frameCount();
    }
    if (facts.isTimedFrameList()) {
        return !metadata.isTimedFrameList() || metadata.frameDurations() != facts.frameDurations();
    }
    return false;
}

}
