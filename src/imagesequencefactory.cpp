#include "imageviewporthelpers_p.h"

#include <utility>

using namespace ImageViewportInternal;

ImageSequenceFactoryResult::ImageSequenceFactoryResult(ImageSequence *sequence,
    FactoryOutcome outcome,
    QString errorString,
    QString warningString,
    QObject *parent)
    : QObject(parent)
    , m_sequence(sequence)
    , m_outcome(outcome)
    , m_errorString(std::move(errorString))
    , m_warningString(std::move(warningString))
{
}

ImageSequence *ImageSequenceFactoryResult::sequence() const
{
    return m_sequence;
}

ImageSequenceFactoryResult::FactoryOutcome ImageSequenceFactoryResult::outcome() const
{
    return m_outcome;
}

QString ImageSequenceFactoryResult::errorString() const
{
    return m_errorString;
}

QString ImageSequenceFactoryResult::warningString() const
{
    return m_warningString;
}

ImageSequenceFactory::ImageSequenceFactory(QObject *parent)
    : QObject(parent)
{
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromFrame(const QImage &image)
{
    ImageFrame frame(image);
    return fromFrame(&frame);
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromTimedFrameList(const QVector<QImage> &images, const QVector<int> &durationsMilliseconds)
{
    if (images.size() != durationsMilliseconds.size()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("timed frame images and durations must have the same count"),
            {},
            this);
    }

    TimedImageFrameList list;
    for (qsizetype index = 0; index < images.size(); ++index) {
        ImageFrame frame(images.at(index));
        if (!list.appendFrame(&frame, durationsMilliseconds.at(index))) {
            return new ImageSequenceFactoryResult(nullptr,
                ImageSequenceFactoryResult::FactoryOutcome::Invalid,
                list.errorString(),
                list.warningString(),
                this);
        }
    }

    return fromTimedFrameList(&list);
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromFrame(ImageFrame *frame)
{
    if (!frame) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageFrame is required"),
            {},
            this);
    }

    if (!frame->isValid()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageFrame must have a positive logical size"),
            {},
            this);
    }

    const QString limitViolation = frameLimitViolation(*frame);
    if (!limitViolation.isEmpty()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            limitViolation,
            {},
            this);
    }

    return new ImageSequenceFactoryResult(new ImageSequence(frame->logicalSize(), frame->imagePayload(), this),
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        {},
        this);
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromTimedFrameList(TimedImageFrameList *list)
{
    if (!list || !list->isValid()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("TimedImageFrameList must contain at least one frame"),
            {},
            this);
    }

    return new ImageSequenceFactoryResult(new ImageSequence(list->logicalSize(), list->frameDurations(), list->frameImages(), this),
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        {},
        this);
}

ImageSequenceFactoryResult *ImageSequenceFactory::fromProvider(ImageSequenceProviderAdapter *adapter)
{
    if (!adapter) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageSequenceProviderAdapter is required"),
            {},
            this);
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory = adapter->sessionFactory();
    if (!sessionFactory) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("ImageSequenceProviderAdapter must provide a bounded session factory"),
            {},
            this);
    }

    const ImageSequenceProviderMetadata knownMetadata = adapter->knownMetadata();
    const QString metadataViolation = providerMetadataLimitViolation(knownMetadata);
    if (!metadataViolation.isEmpty()) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            metadataViolation,
            {},
            this);
    }

    const bool hasKnownMetadata = knownMetadata.isSpecified();
    if (hasKnownMetadata
        && (providerCapabilityContradictsMetadata(adapter->timedPlaybackCapability(), knownMetadata.isTimedFrameList())
            || providerCapabilityContradictsMetadata(adapter->frameSeekCapability(), true)
            || providerCapabilityContradictsMetadata(adapter->positionSeekCapability(), knownMetadata.isTimedFrameList()))) {
        return new ImageSequenceFactoryResult(nullptr,
            ImageSequenceFactoryResult::FactoryOutcome::Invalid,
            QStringLiteral("provider metadata contradicts declared capabilities"),
            {},
            this);
    }

    return new ImageSequenceFactoryResult(new ImageSequence(std::move(sessionFactory),
                                           hasKnownMetadata,
                                           hasKnownMetadata ? knownMetadata.logicalSize() : QSizeF(),
                                           hasKnownMetadata && knownMetadata.isTimedFrameList() ? knownMetadata.frameDurations() : QVector<int>(),
                                           adapter->timedPlaybackCapability(),
                                           adapter->frameSeekCapability(),
                                           adapter->positionSeekCapability(),
                                           this),
        ImageSequenceFactoryResult::FactoryOutcome::Created,
        {},
        {},
        this);
}

ImageSequenceLimits::ImageSequenceLimits(QObject *parent)
    : QObject(parent)
{
}

int ImageSequenceLimits::getMaximumLogicalWidth() const
{
    return maximumLogicalWidth();
}

int ImageSequenceLimits::getMaximumLogicalHeight() const
{
    return maximumLogicalHeight();
}

qint64 ImageSequenceLimits::getMaximumPixelsPerFrame() const
{
    return maximumPixelsPerFrame();
}

qint64 ImageSequenceLimits::getMaximumPayloadBytesPerFrame() const
{
    return maximumPayloadBytesPerFrame();
}

int ImageSequenceLimits::getMaximumTimedListFrameCount() const
{
    return maximumTimedListFrameCount();
}

int ImageSequenceLimits::getMaximumFrameDuration() const
{
    return maximumFrameDuration();
}

int ImageSequenceLimits::getMaximumTotalSequenceDuration() const
{
    return maximumTotalSequenceDuration();
}

int ImageSequenceLimits::getMaximumDiagnosticStringLength() const
{
    return maximumDiagnosticStringLength();
}

int ImageSequenceLimits::maximumLogicalWidth()
{
    return minimumMaximumLogicalSide;
}

int ImageSequenceLimits::maximumLogicalHeight()
{
    return minimumMaximumLogicalSide;
}

qint64 ImageSequenceLimits::maximumPixelsPerFrame()
{
    return minimumMaximumPixelsPerFrame;
}

qint64 ImageSequenceLimits::maximumPayloadBytesPerFrame()
{
    return minimumMaximumPayloadBytesPerFrame;
}

int ImageSequenceLimits::maximumTimedListFrameCount()
{
    return minimumMaximumTimedListFrameCount;
}

int ImageSequenceLimits::maximumFrameDuration()
{
    return minimumMaximumDuration;
}

int ImageSequenceLimits::maximumTotalSequenceDuration()
{
    return minimumMaximumDuration;
}

int ImageSequenceLimits::maximumDiagnosticStringLength()
{
    return minimumMaximumDiagnosticStringLength;
}
