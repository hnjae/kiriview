#pragma once

#include "imageviewportproviderfacts_p.h"
#include "timingintervals_p.h"
#include <ImageViewport/imagesequenceprovider.h>

#include <memory>

namespace ImageViewportInternal {

struct FramePayloadFacts
{
    QSizeF sourceLogicalSize;
    QSizeF payloadRasterSize;
    QSizeF sourceToPayloadScale;
    qint64 payloadByteSize = 0;
    ImageViewportPayloadQuality quality = ImageViewportPayloadQuality::Unknown;
    ImageViewportPayloadExactness exactness = ImageViewportPayloadExactness::Unknown;
    bool hasAlpha = false;
    ImageFrame::OrientationPolicy orientationPolicy = ImageFrame::OrientationPolicy::Identity;
    QString formatIdentifier;
};

struct FramePayload
{
    QImage image;
    FramePayloadFacts facts;
};

} // namespace ImageViewportInternal

class ImageSequence::Data
{
public:
    enum class Kind {
        None,
        Still,
        TimedList,
        Provider,
    };

    static std::unique_ptr<Data> still(
        QSizeF logicalSize, ImageViewportInternal::FramePayload payload);
    static std::unique_ptr<Data> timedList(QSizeF logicalSize, const QVector<int>& frameDurations,
        QVector<ImageViewportInternal::FramePayload> framePayloads,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    static std::unique_ptr<Data> provider(
        std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
        ImageViewportInternal::ImageSequenceProviderKnownFacts providerKnownFacts,
        ImageViewportInternal::ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
        ImageViewportInternal::ImageSequenceProviderCapabilitySupport frameSeekCapability,
        ImageViewportInternal::ImageSequenceProviderCapabilitySupport positionSeekCapability,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
        bool authoredAnimationFactsAvailable,
        ImageSequenceProviderThreadingContract providerThreadingContract);

    Kind kind = Kind::None;
    QSizeF logicalSize;
    ImageViewportInternal::FramePayload stillPayload;
    std::shared_ptr<const TimingIntervals> timingIntervals;
    QVector<ImageViewportInternal::FramePayload> framePayloads;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
    bool authoredAnimationFactsAvailable = false;
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory;
    std::weak_ptr<ImageSequence> owner;
    ImageViewportInternal::ImageSequenceProviderKnownFacts providerKnownFacts;
    bool hasCompleteProviderKnownMetadata = false;
    QSizeF providerKnownLogicalSize;
    std::shared_ptr<const TimingIntervals> providerKnownTimingIntervals;
    ImageViewportInternal::ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability
        = ImageViewportInternal::ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageViewportInternal::ImageSequenceProviderCapabilitySupport providerFrameSeekCapability
        = ImageViewportInternal::ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageViewportInternal::ImageSequenceProviderCapabilitySupport providerPositionSeekCapability
        = ImageViewportInternal::ImageSequenceProviderCapabilitySupport::Unavailable;
    ImageSequenceProviderThreadingContract providerThreadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
};

namespace ImageViewportInternal {

class ImageSequencePrivateAccess
{
public:
    static std::shared_ptr<ImageSequence> createStill(QSizeF logicalSize, FramePayload payload);
    static std::shared_ptr<ImageSequence> createTimedList(QSizeF logicalSize,
        const QVector<int>& frameDurations, QVector<FramePayload> framePayloads,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts);
    static std::shared_ptr<ImageSequence> createProvider(
        std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
        ImageSequenceProviderKnownFacts providerKnownFacts,
        ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
        ImageSequenceProviderCapabilitySupport frameSeekCapability,
        ImageSequenceProviderCapabilitySupport positionSeekCapability,
        ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
        bool authoredAnimationFactsAvailable,
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
    static FramePayload framePayload(const ImageSequence* sequence, int frame);
    static FramePayloadFacts framePayloadFacts(const ImageSequence* sequence, int frame);
    static TimingIntervals timingIntervals(const ImageSequence* sequence);
    static ImageSequenceAuthoredAnimationFacts authoredAnimationFacts(
        const ImageSequence* sequence);
    static bool authoredAnimationFactsAvailable(const ImageSequence* sequence);
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
