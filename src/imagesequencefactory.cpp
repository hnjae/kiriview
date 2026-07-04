#include "framepreparation_p.h"
#include "imagesequence_p.h"
#include "imageviewportlimits_p.h"
#include "imageviewportproviderfacts_p.h"

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

    std::shared_ptr<ImageSequence> sequence
        = ImageSequencePrivateAccess::createStill(frame->logicalSize(), frame->imagePayload());
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

    std::shared_ptr<ImageSequence> sequence = ImageSequencePrivateAccess::createTimedList(
        list->logicalSize(), list->frameDurations(), list->frameImages(),
        list->authoredAnimationFacts());
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
    const ImageSequenceAuthoredAnimationFacts authoredAnimationFacts
        = adapter->authoredAnimationFacts();
    const ImageSequenceProviderThreadingContract threadingContract = adapter->threadingContract();
    if (knownMetadata.isSpecified()) {
        const auto metadataAdmission = FramePreparation::admitProviderMetadata(knownMetadata);
        if (!metadataAdmission.accepted()) {
            return new ImageSequenceFactoryResult(nullptr,
                ImageSequenceFactoryResult::FactoryOutcome::Invalid, metadataAdmission.diagnostic);
        }
    }

    const auto factsAdmission = FramePreparation::admitProviderKnownFacts(knownFacts);
    if (!factsAdmission.accepted()) {
        return new ImageSequenceFactoryResult(
            nullptr, factsAdmission.outcome, factsAdmission.diagnostic);
    }

    const bool hasKnownMetadata = knownMetadata.isSpecified();
    ImageSequenceProviderKnownFacts effectiveKnownFacts = knownFacts;
    if (hasKnownMetadata) {
        if (providerFactsContradictMetadata(knownFacts, knownMetadata)) {
            return new ImageSequenceFactoryResult(nullptr,
                ImageSequenceFactoryResult::FactoryOutcome::Invalid,
                QStringLiteral("provider construction facts contradict known metadata"));
        }
        effectiveKnownFacts = knownMetadata.isStill()
            ? ImageSequenceProviderKnownFacts::still(knownMetadata.logicalSize())
            : ImageSequenceProviderKnownFacts::timedFrameList(
                  knownMetadata.logicalSize(), knownMetadata.frameDurations());
    }
    if (hasKnownMetadata
        && (providerCapabilityContradictsMetadata(
                timedPlaybackCapability, knownMetadata.timedPlaybackSupport())
            || providerCapabilityContradictsMetadata(
                frameSeekCapability, knownMetadata.frameSeekSupport())
            || providerCapabilityContradictsMetadata(
                positionSeekCapability, knownMetadata.positionSeekSupport()))) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("provider metadata contradicts declared capabilities"));
    }
    const ImageSequenceProviderCapabilitySupport effectiveTimedPlaybackCapability = hasKnownMetadata
        ? (knownMetadata.timedPlaybackSupport()
                  ? ImageSequenceProviderCapabilitySupport::KnownTrue
                  : ImageSequenceProviderCapabilitySupport::KnownFalse)
        : timedPlaybackCapability;
    const ImageSequenceProviderCapabilitySupport effectiveFrameSeekCapability = hasKnownMetadata
        ? (knownMetadata.frameSeekSupport() ? ImageSequenceProviderCapabilitySupport::KnownTrue
                                            : ImageSequenceProviderCapabilitySupport::KnownFalse)
        : frameSeekCapability;
    const ImageSequenceProviderCapabilitySupport effectivePositionSeekCapability = hasKnownMetadata
        ? (knownMetadata.positionSeekSupport() ? ImageSequenceProviderCapabilitySupport::KnownTrue
                                               : ImageSequenceProviderCapabilitySupport::KnownFalse)
        : positionSeekCapability;
    if (providerFactsContradictCapabilities(effectiveKnownFacts, effectiveTimedPlaybackCapability,
            effectiveFrameSeekCapability, effectivePositionSeekCapability)) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("provider construction facts contradict declared capabilities"));
    }

    std::shared_ptr<ImageSequence> sequence = ImageSequencePrivateAccess::createProvider(
        std::move(sessionFactory), effectiveKnownFacts, effectiveTimedPlaybackCapability,
        effectiveFrameSeekCapability, effectivePositionSeekCapability, authoredAnimationFacts,
        threadingContract);
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

ImageViewportDisplayLimits::ImageViewportDisplayLimits(QObject* parent)
    : QObject(parent)
{
}

double ImageViewportDisplayLimits::getMaximumManualZoomPercent() const
{
    return maximumManualZoomPercent();
}

double ImageViewportDisplayLimits::maximumManualZoomPercent() { return 10000.0; }
