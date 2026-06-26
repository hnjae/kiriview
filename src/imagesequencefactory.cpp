#include "framepreparation_p.h"
#include "imagesequenceownership_p.h"
#include "imageviewporthelpers_p.h"

#include <utility>

using namespace ImageViewportInternal;

ImageSequenceFactoryResult::ImageSequenceFactoryResult(ImageSequence* sequence,
    FactoryOutcome outcome, QString errorString, QString warningString, QObject* parent)
    : QObject(parent)
    , m_sequence(sequence)
    , m_outcome(outcome)
    , m_errorString(FramePreparation::boundedDiagnostic(std::move(errorString), {}))
    , m_warningString(FramePreparation::boundedDiagnostic(std::move(warningString), {}))
{
}

ImageSequenceFactoryResult::ImageSequenceFactoryResult(std::shared_ptr<ImageSequence> sequence,
    FactoryOutcome outcome, QString errorString, QString warningString, QObject* parent)
    : ImageSequenceFactoryResult(
          sequence.get(), outcome, std::move(errorString), std::move(warningString), parent)
{
    m_sequenceOwner = std::move(sequence);
}

ImageSequence* ImageSequenceFactoryResult::sequence() const { return m_sequence; }

ImageSequenceFactoryResult::FactoryOutcome ImageSequenceFactoryResult::outcome() const
{
    return m_outcome;
}

QString ImageSequenceFactoryResult::errorString() const { return m_errorString; }

QString ImageSequenceFactoryResult::warningString() const { return m_warningString; }

ImageSequenceFactory::ImageSequenceFactory(QObject* parent)
    : QObject(parent)
{
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromFrame(const QImage& image)
{
    ImageFrame frame(image);
    return fromFrame(&frame);
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromTimedFrameList(
    const QVector<QImage>& images, const QVector<int>& durationsMilliseconds)
{
    if (images.size() != durationsMilliseconds.size()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("timed frame images and durations must have the same count"));
    }

    TimedImageFrameList list;
    for (qsizetype index = 0; index < images.size(); ++index) {
        ImageFrame frame(images.at(index));
        if (!list.appendFrame(&frame, durationsMilliseconds.at(index))) {
            return new ImageSequenceFactoryResult(nullptr,
                ImageSequenceFactoryResult::FactoryOutcome::Invalid, list.errorString(),
                list.warningString());
        }
    }

    return fromTimedFrameList(&list);
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromFrame(ImageFrame* frame)
{
    if (!frame) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageFrame is required"));
    }

    if (!frame->isValid()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageFrame must have a positive logical size"));
    }

    const QString limitViolation = frameLimitViolation(*frame);
    if (!limitViolation.isEmpty()) {
        return new ImageSequenceFactoryResult(
            nullptr, ImageSequenceFactoryResult::FactoryOutcome::Invalid, limitViolation);
    }

    std::shared_ptr<ImageSequence> sequence(
        new ImageSequence(frame->logicalSize(), frame->imagePayload()));
    registerFactorySequenceOwner(sequence);
    return new ImageSequenceFactoryResult(
        std::move(sequence), ImageSequenceFactoryResult::FactoryOutcome::Created, {}, {});
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromTimedFrameList(TimedImageFrameList* list)
{
    if (!list || !list->isValid()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("TimedImageFrameList must contain at least one frame"));
    }

    std::shared_ptr<ImageSequence> sequence(
        new ImageSequence(list->logicalSize(), list->frameDurations(), list->frameImages()));
    registerFactorySequenceOwner(sequence);
    return new ImageSequenceFactoryResult(
        std::move(sequence), ImageSequenceFactoryResult::FactoryOutcome::Created, {}, {});
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromProvider(
    ImageSequenceProviderAdapter* adapter)
{
    if (!adapter) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageSequenceProviderAdapter is required"));
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory = adapter->sessionFactory();
    if (!sessionFactory) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageSequenceProviderAdapter must provide a bounded session factory"));
    }

    const ImageSequenceProviderMetadata knownMetadata = adapter->knownMetadata();
    const ImageSequenceProviderKnownFacts knownFacts = adapter->knownFacts();
    const ImageSequenceProviderCapabilitySupport timedPlaybackCapability
        = adapter->timedPlaybackCapability();
    const ImageSequenceProviderCapabilitySupport frameSeekCapability
        = adapter->frameSeekCapability();
    const ImageSequenceProviderCapabilitySupport positionSeekCapability
        = adapter->positionSeekCapability();
    const ImageSequenceProviderThreadingContract threadingContract = adapter->threadingContract();
    const QString metadataViolation = providerMetadataLimitViolation(knownMetadata);
    if (!metadataViolation.isEmpty()) {
        return new ImageSequenceFactoryResult(
            nullptr, ImageSequenceFactoryResult::FactoryOutcome::Invalid, metadataViolation);
    }

    const QString factsViolation = providerKnownFactsLimitViolation(knownFacts);
    if (!factsViolation.isEmpty()) {
        return new ImageSequenceFactoryResult(
            nullptr, ImageSequenceFactoryResult::FactoryOutcome::Invalid, factsViolation);
    }

    const bool hasKnownMetadata = knownMetadata.isSpecified();
    if (hasKnownMetadata
        && (providerCapabilityContradictsMetadata(
                timedPlaybackCapability, knownMetadata.isTimedFrameList())
            || providerCapabilityContradictsMetadata(frameSeekCapability, true)
            || providerCapabilityContradictsMetadata(
                positionSeekCapability, knownMetadata.isTimedFrameList()))) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("provider metadata contradicts declared capabilities"));
    }
    if (providerFactsContradictCapabilities(
            knownFacts, timedPlaybackCapability, frameSeekCapability, positionSeekCapability)) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("provider construction facts contradict declared capabilities"));
    }

    std::shared_ptr<ImageSequence> sequence(
        new ImageSequence(std::move(sessionFactory), knownFacts, timedPlaybackCapability,
            frameSeekCapability, positionSeekCapability, threadingContract));
    registerFactorySequenceOwner(sequence);
    return new ImageSequenceFactoryResult(
        std::move(sequence), ImageSequenceFactoryResult::FactoryOutcome::Created, {}, {});
}

ImageSequenceLimits::ImageSequenceLimits(QObject* parent)
    : QObject(parent)
{
}

int ImageSequenceLimits::getMaximumLogicalWidth() const { return maximumLogicalWidth(); }

int ImageSequenceLimits::getMaximumLogicalHeight() const { return maximumLogicalHeight(); }

qint64 ImageSequenceLimits::getMaximumPixelsPerFrame() const { return maximumPixelsPerFrame(); }

qint64 ImageSequenceLimits::getMaximumPayloadBytesPerFrame() const
{
    return maximumPayloadBytesPerFrame();
}

int ImageSequenceLimits::getMaximumTimedListFrameCount() const
{
    return maximumTimedListFrameCount();
}

int ImageSequenceLimits::getMaximumFrameDuration() const { return maximumFrameDuration(); }

int ImageSequenceLimits::getMaximumTotalSequenceDuration() const
{
    return maximumTotalSequenceDuration();
}

int ImageSequenceLimits::getMaximumDiagnosticStringLength() const
{
    return maximumDiagnosticStringLength();
}

int ImageSequenceLimits::maximumLogicalWidth() { return minimumMaximumLogicalSide; }

int ImageSequenceLimits::maximumLogicalHeight() { return minimumMaximumLogicalSide; }

qint64 ImageSequenceLimits::maximumPixelsPerFrame() { return minimumMaximumPixelsPerFrame; }

qint64 ImageSequenceLimits::maximumPayloadBytesPerFrame()
{
    return minimumMaximumPayloadBytesPerFrame;
}

int ImageSequenceLimits::maximumTimedListFrameCount() { return minimumMaximumTimedListFrameCount; }

int ImageSequenceLimits::maximumFrameDuration() { return minimumMaximumDuration; }

int ImageSequenceLimits::maximumTotalSequenceDuration() { return minimumMaximumDuration; }

int ImageSequenceLimits::maximumDiagnosticStringLength()
{
    return minimumMaximumDiagnosticStringLength;
}
