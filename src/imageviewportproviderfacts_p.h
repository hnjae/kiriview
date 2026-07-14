#pragma once

#include <ImageViewport/ImageViewport>

namespace ImageViewportInternal {

enum class ImageSequenceProviderCapabilitySupport {
    Unavailable,
    KnownFalse,
    KnownTrue,
};

class ImageSequenceProviderKnownFacts
{
public:
    enum class Kind {
        Unknown,
        LogicalSize,
        Still,
        TimedFrameCount,
        TimedFrameList,
    };

    ImageSequenceProviderKnownFacts() = default;
    static ImageSequenceProviderKnownFacts logicalSize(QSizeF logicalSize);
    static ImageSequenceProviderKnownFacts still(QSizeF logicalSize);
    static ImageSequenceProviderKnownFacts timedFrameCount(QSizeF logicalSize, int frameCount);
    static ImageSequenceProviderKnownFacts fixedDurationFrames(
        QSizeF logicalSize, int frameCount, int frameDuration);
    static ImageSequenceProviderKnownFacts timedFrameList(
        QSizeF logicalSize, QVector<int> frameDurations);

    bool isSpecified() const;
    bool isValid() const;
    bool isComplete() const;
    bool isLogicalSizeOnly() const;
    bool isStill() const;
    bool isTimedFrameCount() const;
    bool isTimedFrameList() const;
    QSizeF logicalSize() const;
    int frameCount() const;
    QVector<int> frameDurations() const;

private:
    Kind m_kind = Kind::Unknown;
    QSizeF m_logicalSize;
    int m_frameCount = -1;
    QVector<int> m_frameDurations;
};

inline ImageViewportCapabilitySupport providerCapabilitySupport(
    ImageSequenceProviderCapabilitySupport support)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::KnownFalse:
        return ImageViewportCapabilitySupport::False;
    case ImageSequenceProviderCapabilitySupport::KnownTrue:
        return ImageViewportCapabilitySupport::True;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return ImageViewportCapabilitySupport::Unavailable;
    }

    return ImageViewportCapabilitySupport::Unavailable;
}

inline bool providerCapabilityContradictsMetadata(
    ImageSequenceProviderCapabilitySupport support, bool metadataCapability)
{
    switch (support) {
    case ImageSequenceProviderCapabilitySupport::KnownFalse:
        return metadataCapability;
    case ImageSequenceProviderCapabilitySupport::KnownTrue:
        return !metadataCapability;
    case ImageSequenceProviderCapabilitySupport::Unavailable:
        return false;
    }

    return false;
}

inline bool providerCapabilityKnownFalse(ImageSequenceProviderCapabilitySupport support)
{
    return support == ImageSequenceProviderCapabilitySupport::KnownFalse;
}

inline bool providerCapabilityKnownTrue(ImageSequenceProviderCapabilitySupport support)
{
    return support == ImageSequenceProviderCapabilitySupport::KnownTrue;
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
    if (metadata.sourceLogicalSize() != facts.logicalSize()) {
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
