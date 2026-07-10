#pragma once

#include "imageviewport.h"
#include "timingintervals_p.h"

#include <memory>

namespace ImageViewportInternal {

struct FramePayloadFacts
{
    QSizeF sourceLogicalSize;
    QSizeF payloadRasterSize;
    QSizeF sourceToPayloadScale;
    ImageViewport::PayloadQuality quality = ImageViewport::PayloadQuality::Unknown;
    ImageViewport::PayloadExactness exactness = ImageViewport::PayloadExactness::Unknown;
    ImageViewportDemandRevisionToken demandRevision;
};

class ImageSequenceData
{
public:
    enum class Kind {
        None,
        Still,
        TimedList,
        Provider,
    };

    static std::unique_ptr<ImageSequenceData> still(
        QSizeF logicalSize, QImage stillImage, FramePayloadFacts payloadFacts = {});
    static std::unique_ptr<ImageSequenceData> timedList(QSizeF logicalSize,
        const QVector<int>& frameDurations, QVector<QImage> frameImages,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    static std::unique_ptr<ImageSequenceData> provider(
        std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
        ImageSequenceProviderKnownFacts providerKnownFacts,
        ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
        ImageSequenceProviderCapabilitySupport frameSeekCapability,
        ImageSequenceProviderCapabilitySupport positionSeekCapability,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
        ImageSequenceProviderThreadingContract providerThreadingContract);

    Kind kind = Kind::None;
    QSizeF logicalSize;
    QImage stillImage;
    FramePayloadFacts stillPayloadFacts;
    std::shared_ptr<const TimingIntervals> timingIntervals;
    QVector<QImage> frameImages;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory;
    std::weak_ptr<ImageSequence> owner;
    ImageSequenceProviderKnownFacts providerKnownFacts;
    bool hasCompleteProviderKnownMetadata = false;
    QSizeF providerKnownLogicalSize;
    std::shared_ptr<const TimingIntervals> providerKnownTimingIntervals;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability
        = ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderThreadingContract providerThreadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
};

class ImageSequencePrivateAccess
{
public:
    static std::shared_ptr<ImageSequence> createStill(
        QSizeF logicalSize, QImage stillImage, FramePayloadFacts payloadFacts = {});
    static std::shared_ptr<ImageSequence> createTimedList(QSizeF logicalSize,
        const QVector<int>& frameDurations, QVector<QImage> frameImages,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    static std::shared_ptr<ImageSequence> createProvider(
        std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
        ImageSequenceProviderKnownFacts providerKnownFacts,
        ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
        ImageSequenceProviderCapabilitySupport frameSeekCapability,
        ImageSequenceProviderCapabilitySupport positionSeekCapability,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
        ImageSequenceProviderThreadingContract providerThreadingContract);

    static bool isValid(const ImageSequence* sequence);
    static bool isStill(const ImageSequence* sequence);
    static bool isTimedList(const ImageSequence* sequence);
    static bool isProvider(const ImageSequence* sequence);
    static QSizeF logicalSize(const ImageSequence* sequence);
    static int frameCount(const ImageSequence* sequence);
    static int totalDuration(const ImageSequence* sequence);
    static int frameStartPosition(const ImageSequence* sequence, int frame);
    static int frameIndexForPosition(const ImageSequence* sequence, int position);
    static QImage frameImage(const ImageSequence* sequence, int frame);
    static FramePayloadFacts framePayloadFacts(const ImageSequence* sequence, int frame);
    static TimingIntervals timingIntervals(const ImageSequence* sequence);
    static ImageSequenceAuthoredAnimationFacts authoredAnimationFacts(
        const ImageSequence* sequence);
    static bool hasCompleteProviderKnownMetadata(const ImageSequence* sequence);
    static ImageSequenceProviderKnownFacts providerKnownFacts(const ImageSequence* sequence);
    static QSizeF providerKnownLogicalSize(const ImageSequence* sequence);
    static TimingIntervals providerKnownTimingIntervals(const ImageSequence* sequence);
    static ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability(
        const ImageSequence* sequence);
    static ImageSequenceProviderCapabilitySupport providerFrameSeekCapability(
        const ImageSequence* sequence);
    static ImageSequenceProviderCapabilitySupport providerPositionSeekCapability(
        const ImageSequence* sequence);
    static std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory(
        const ImageSequence* sequence);
    static ImageSequenceProviderThreadingContract providerThreadingContract(
        const ImageSequence* sequence);
    static std::shared_ptr<ImageSequence> owner(const ImageSequence* sequence);
};

class ImageFramePrivateAccess
{
public:
    static std::unique_ptr<ImageFrame> createWithPayloadByteSize(
        const QImage& image, qsizetype payloadByteSize);
    static QImage image(const ImageFrame& frame);
};

} // namespace ImageViewportInternal
