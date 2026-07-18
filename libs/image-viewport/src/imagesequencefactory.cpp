// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "framepreparation_p.h"
#include "imagesequence_p.h"
#include "imageviewportlimits_p.h"
#include "imageviewportproviderfacts_p.h"
#include "publicdiagnostic_p.h"

#include <ImageViewport/imageviewport.h>

#include <utility>

using namespace ImageViewportInternal;

namespace {
FramePayload framePayload(const ImageFrame& frame)
{
    return { ImageFramePrivateAccess::image(frame),
        { frame.sourceLogicalSize(), frame.payloadRasterSize(), frame.sourceToPayloadScale(),
            frame.payloadByteSize(), frame.quality(), frame.exactness(), frame.hasAlpha(),
            frame.orientationPolicy(), frame.formatIdentifier() } };
}

QString factoryReasonDiagnostic(
    ImageSequenceFactoryOutcome outcome, ImageSequenceFactoryReason reason)
{
    if (outcome != ImageSequenceFactoryOutcome::Rejected) {
        return {};
    }
    switch (reason) {
    case ImageSequenceFactoryReason::InvalidFrame:
        return QStringLiteral("invalid image frame");
    case ImageSequenceFactoryReason::InvalidTiming:
        return QStringLiteral("invalid image sequence timing");
    case ImageSequenceFactoryReason::InvalidAnimationMetadata:
        return QStringLiteral("invalid animation metadata");
    case ImageSequenceFactoryReason::InvalidProviderDescriptor:
        return QStringLiteral("invalid provider descriptor");
    case ImageSequenceFactoryReason::LimitExceeded:
        return QStringLiteral("image sequence limit exceeded");
    case ImageSequenceFactoryReason::NoError:
        return {};
    }
    return {};
}
}

ImageSequenceFactoryResult::ImageSequenceFactoryResult(ImageSequence* sequence,
    ImageSequenceFactoryOutcome outcome, ImageSequenceFactoryReason reason, QObject* parent)
    : QObject(parent)
    , m_sequence(sequence)
    , m_outcome(outcome)
    , m_reason(reason)
    , m_errorString(ImageViewportInternal::PublicDiagnosticText::fromTrusted(
          factoryReasonDiagnostic(outcome, reason))
              .text())
{
}

ImageSequenceFactoryResult::ImageSequenceFactoryResult(std::shared_ptr<ImageSequence> sequence,
    ImageSequenceFactoryOutcome outcome, ImageSequenceFactoryReason reason, QString errorString,
    QObject* parent)
    : QObject(parent)
    , m_sequence(sequence.get())
    , m_outcome(outcome)
    , m_reason(reason)
    , m_errorString(
          ImageViewportInternal::PublicDiagnosticText::fromTrusted(std::move(errorString)).text())
{
    m_sequenceOwner = std::move(sequence);
}

ImageSequence* ImageSequenceFactoryResult::sequence() const { return m_sequence; }

ImageSequenceFactoryOutcome ImageSequenceFactoryResult::outcome() const { return m_outcome; }

ImageSequenceFactoryReason ImageSequenceFactoryResult::reason() const { return m_reason; }

QString ImageSequenceFactoryResult::errorString() const { return m_errorString; }

ImageSequenceFactory::ImageSequenceFactory(QObject* parent)
    : QObject(parent)
{
}

ImageSequenceFactoryResult* ImageSequenceFactory::rejected(
    ImageSequenceFactoryReason reason, QString errorString)
{
    return new ImageSequenceFactoryResult(std::shared_ptr<ImageSequence> {},
        ImageSequenceFactoryOutcome::Rejected, reason, std::move(errorString));
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromFrame(const QImage& image)
{
    ImageFrame frame(image);
    return fromFrame(&frame);
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromTimedFrameList(
    const QVector<QImage>& images, const QVector<int>& durationsMilliseconds)
{
    QSizeF sourceLogicalSize;
    for (const QImage& image : images) {
        const ImageFrame frame(image);
        if (!frame.isValid()
            || (!sourceLogicalSize.isEmpty() && sourceLogicalSize != frame.sourceLogicalSize())) {
            return rejected(ImageSequenceFactoryReason::InvalidFrame,
                QStringLiteral("timed frames must be valid and use one source logical size"));
        }
        sourceLogicalSize = frame.sourceLogicalSize();
    }

    if (images.size() != durationsMilliseconds.size()) {
        return rejected(ImageSequenceFactoryReason::InvalidTiming,
            QStringLiteral("timed frame images and durations must have the same count"));
    }

    for (int duration : durationsMilliseconds) {
        if (duration <= 0) {
            return rejected(ImageSequenceFactoryReason::InvalidTiming,
                QStringLiteral("timed frame durations must be positive"));
        }
    }

    TimedImageFrameList list;
    for (qsizetype index = 0; index < images.size(); ++index) {
        ImageFrame frame(images.at(index));
        if (!list.appendFrame(&frame, durationsMilliseconds.at(index))) {
            return rejected(ImageSequenceFactoryReason::LimitExceeded, list.errorString());
        }
    }

    return fromTimedFrameList(&list);
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromFrame(ImageFrame* frame)
{
    if (!frame) {
        return rejected(
            ImageSequenceFactoryReason::InvalidFrame, QStringLiteral("ImageFrame is required"));
    }

    if (!frame->isValid()) {
        return rejected(ImageSequenceFactoryReason::InvalidFrame,
            QStringLiteral("ImageFrame must have a positive logical size"));
    }

    const QString limitViolation = frameLimitViolation(*frame);
    if (!limitViolation.isEmpty()) {
        return rejected(ImageSequenceFactoryReason::LimitExceeded, limitViolation);
    }

    std::shared_ptr<ImageSequence> sequence
        = ImageSequencePrivateAccess::createStill(frame->sourceLogicalSize(), framePayload(*frame));
    return new ImageSequenceFactoryResult(std::move(sequence), ImageSequenceFactoryOutcome::Created,
        ImageSequenceFactoryReason::NoError);
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromTimedFrameList(TimedImageFrameList* list)
{
    if (!list || !list->isValid()) {
        return rejected(ImageSequenceFactoryReason::InvalidTiming,
            QStringLiteral("TimedImageFrameList must contain at least one frame"));
    }

    if (!list->authoredAnimationFacts().isValid()
        || list->authoredAnimationFacts().loopMode()
            == ImageSequenceAuthoredAnimationLoopMode::Unavailable) {
        return rejected(ImageSequenceFactoryReason::InvalidAnimationMetadata,
            QStringLiteral("TimedImageFrameList authored animation metadata is invalid"));
    }

    QVector<FramePayload> payloads;
    const QList<TimedImageFrame> frames = list->frames();
    payloads.reserve(frames.size());
    for (const TimedImageFrame& timedFrame : frames) {
        payloads.append(framePayload(*timedFrame.frame()));
    }
    std::shared_ptr<ImageSequence> sequence
        = ImageSequencePrivateAccess::createTimedList(list->logicalSize(), list->frameDurations(),
            std::move(payloads), list->authoredAnimationFacts());
    return new ImageSequenceFactoryResult(std::move(sequence), ImageSequenceFactoryOutcome::Created,
        ImageSequenceFactoryReason::NoError);
}

ImageSequenceFactoryResult* ImageSequenceFactory::fromProvider(
    ImageSequenceProviderAdapter* adapter)
{
    if (!adapter) {
        return rejected(ImageSequenceFactoryReason::InvalidProviderDescriptor,
            QStringLiteral("ImageSequenceProviderAdapter is required"));
    }

    const ImageSequenceProviderDescriptor descriptor = adapter->descriptor();
    ImageSequenceProviderSessionFactory sessionFactory = descriptor.sessionFactory();
    if (!descriptor.isValid()) {
        return rejected(ImageSequenceFactoryReason::InvalidProviderDescriptor,
            QStringLiteral("ImageSequenceProviderAdapter must provide a bounded session factory"));
    }

    const ImageSequenceProviderMetadata knownMetadata = descriptor.constructionMetadata();
    ImageSequenceProviderKnownFacts knownFacts;
    if (knownMetadata.hasCompleteModel()) {
        knownFacts = knownMetadata.isStill()
            ? ImageSequenceProviderKnownFacts::still(knownMetadata.sourceLogicalSize())
            : ImageSequenceProviderKnownFacts::timedFrameList(
                  knownMetadata.sourceLogicalSize(), knownMetadata.frameDurations());
    } else if (!knownMetadata.sourceLogicalSize().isEmpty()) {
        knownFacts = knownMetadata.frameCount() >= 0
            ? ImageSequenceProviderKnownFacts::timedFrameCount(
                  knownMetadata.sourceLogicalSize(), knownMetadata.frameCount())
            : ImageSequenceProviderKnownFacts::logicalSize(knownMetadata.sourceLogicalSize());
    }
    const auto internalCapability = [](ImageViewportCapabilitySupport support) {
        switch (support) {
        case ImageViewportCapabilitySupport::False:
            return ImageSequenceProviderCapabilitySupport::KnownFalse;
        case ImageViewportCapabilitySupport::True:
            return ImageSequenceProviderCapabilitySupport::KnownTrue;
        case ImageViewportCapabilitySupport::Unavailable:
            return ImageSequenceProviderCapabilitySupport::Unavailable;
        }
        return ImageSequenceProviderCapabilitySupport::Unavailable;
    };
    const ImageSequenceProviderCapabilitySupport timedPlaybackCapability
        = internalCapability(knownMetadata.timedPlaybackSupport());
    const ImageSequenceProviderCapabilitySupport frameSeekCapability
        = internalCapability(knownMetadata.frameSeekSupport());
    const ImageSequenceProviderCapabilitySupport positionSeekCapability
        = internalCapability(knownMetadata.positionSeekSupport());
    const ImageSequenceAuthoredAnimationFacts authoredAnimationFacts
        = knownMetadata.hasAuthoredAnimationFacts() ? knownMetadata.authoredAnimationFacts()
                                                    : ImageSequenceAuthoredAnimationFacts {};
    const ImageSequenceProviderThreadingContract threadingContract = descriptor.threadingContract();
    if (knownMetadata.isSpecified() && !knownMetadata.hasCompleteModel()
        && !knownMetadata.isValid()) {
        return rejected(ImageSequenceFactoryReason::InvalidProviderDescriptor,
            QStringLiteral("provider construction metadata is invalid"));
    }
    if (knownMetadata.hasCompleteModel()) {
        const auto metadataAdmission = FramePreparation::admitProviderMetadata(knownMetadata);
        if (!metadataAdmission.accepted()) {
            return rejected(ImageSequenceFactoryReason::InvalidProviderDescriptor,
                metadataAdmission.diagnostic);
        }
    }

    const auto factsAdmission = FramePreparation::admitProviderKnownFacts(knownFacts);
    if (!factsAdmission.accepted()) {
        return rejected(factsAdmission.cause
                    == FramePreparation::ProviderKnownFactsAdmissionResult::Cause::InvalidFacts
                ? ImageSequenceFactoryReason::InvalidProviderDescriptor
                : ImageSequenceFactoryReason::LimitExceeded,
            factsAdmission.diagnostic);
    }

    const bool hasKnownMetadata = knownMetadata.hasCompleteModel();
    ImageSequenceProviderKnownFacts effectiveKnownFacts = knownFacts;
    if (hasKnownMetadata) {
        if (providerFactsContradictMetadata(knownFacts, knownMetadata)) {
            return rejected(ImageSequenceFactoryReason::InvalidProviderDescriptor,
                QStringLiteral("provider construction facts contradict known metadata"));
        }
        effectiveKnownFacts = knownMetadata.isStill()
            ? ImageSequenceProviderKnownFacts::still(knownMetadata.sourceLogicalSize())
            : ImageSequenceProviderKnownFacts::timedFrameList(
                  knownMetadata.sourceLogicalSize(), knownMetadata.frameDurations());
    }
    if (hasKnownMetadata
        && (providerCapabilityContradictsMetadata(timedPlaybackCapability,
                knownMetadata.timedPlaybackSupport() == ImageViewportCapabilitySupport::True)
            || providerCapabilityContradictsMetadata(frameSeekCapability,
                knownMetadata.frameSeekSupport() == ImageViewportCapabilitySupport::True)
            || providerCapabilityContradictsMetadata(positionSeekCapability,
                knownMetadata.positionSeekSupport() == ImageViewportCapabilitySupport::True))) {
        return rejected(ImageSequenceFactoryReason::InvalidProviderDescriptor,
            QStringLiteral("provider metadata contradicts declared capabilities"));
    }
    const ImageSequenceProviderCapabilitySupport effectiveTimedPlaybackCapability = hasKnownMetadata
        ? (knownMetadata.timedPlaybackSupport() == ImageViewportCapabilitySupport::True
                  ? ImageSequenceProviderCapabilitySupport::KnownTrue
                  : ImageSequenceProviderCapabilitySupport::KnownFalse)
        : timedPlaybackCapability;
    const ImageSequenceProviderCapabilitySupport effectiveFrameSeekCapability = hasKnownMetadata
        ? (knownMetadata.frameSeekSupport() == ImageViewportCapabilitySupport::True
                  ? ImageSequenceProviderCapabilitySupport::KnownTrue
                  : ImageSequenceProviderCapabilitySupport::KnownFalse)
        : frameSeekCapability;
    const ImageSequenceProviderCapabilitySupport effectivePositionSeekCapability = hasKnownMetadata
        ? (knownMetadata.positionSeekSupport() == ImageViewportCapabilitySupport::True
                  ? ImageSequenceProviderCapabilitySupport::KnownTrue
                  : ImageSequenceProviderCapabilitySupport::KnownFalse)
        : positionSeekCapability;
    if (providerFactsContradictCapabilities(effectiveKnownFacts, effectiveTimedPlaybackCapability,
            effectiveFrameSeekCapability, effectivePositionSeekCapability)) {
        return rejected(ImageSequenceFactoryReason::InvalidProviderDescriptor,
            QStringLiteral("provider construction facts contradict declared capabilities"));
    }

    std::shared_ptr<ImageSequence> sequence = ImageSequencePrivateAccess::createProvider(
        std::make_shared<ImageSequenceProviderSessionFactory>(std::move(sessionFactory)),
        effectiveKnownFacts, effectiveTimedPlaybackCapability, effectiveFrameSeekCapability,
        effectivePositionSeekCapability, authoredAnimationFacts,
        knownMetadata.hasAuthoredAnimationFacts(), threadingContract);
    return new ImageSequenceFactoryResult(std::move(sequence), ImageSequenceFactoryOutcome::Created,
        ImageSequenceFactoryReason::NoError);
}

ImageSequenceLimits::ImageSequenceLimits(QObject* parent)
    : QObject(parent)
{
}

int ImageSequenceLimits::getMaximumSourceLogicalWidth() const
{
    return maximumSourceLogicalWidth();
}

int ImageSequenceLimits::getMaximumSourceLogicalHeight() const
{
    return maximumSourceLogicalHeight();
}

qint64 ImageSequenceLimits::getMaximumSourceLogicalPixels() const
{
    return maximumSourceLogicalPixels();
}

int ImageSequenceLimits::getMaximumPayloadRasterWidth() const
{
    return maximumPayloadRasterWidth();
}

int ImageSequenceLimits::getMaximumPayloadRasterHeight() const
{
    return maximumPayloadRasterHeight();
}

qint64 ImageSequenceLimits::getMaximumPayloadBytes() const { return maximumPayloadBytes(); }

int ImageSequenceLimits::getMaximumFrameCount() const { return maximumFrameCount(); }

int ImageSequenceLimits::getMaximumFrameDurationMilliseconds() const
{
    return maximumFrameDurationMilliseconds();
}

int ImageSequenceLimits::getMaximumTotalDurationMilliseconds() const
{
    return maximumTotalDurationMilliseconds();
}

int ImageSequenceLimits::getMaximumDiagnosticCharacters() const
{
    return maximumDiagnosticCharacters();
}

int ImageSequenceLimits::getMaximumFormatIdentifierCharacters() const
{
    return maximumFormatIdentifierCharacters();
}

int ImageSequenceLimits::maximumSourceLogicalWidth() { return minimumMaximumLogicalSide; }

int ImageSequenceLimits::maximumSourceLogicalHeight() { return minimumMaximumLogicalSide; }

qint64 ImageSequenceLimits::maximumSourceLogicalPixels() { return minimumMaximumPixelsPerFrame; }

int ImageSequenceLimits::maximumPayloadRasterWidth() { return minimumMaximumPayloadRasterSide; }

int ImageSequenceLimits::maximumPayloadRasterHeight() { return minimumMaximumPayloadRasterSide; }

qint64 ImageSequenceLimits::maximumPayloadBytes() { return minimumMaximumPayloadBytesPerFrame; }

int ImageSequenceLimits::maximumFrameCount() { return minimumMaximumTimedListFrameCount; }

int ImageSequenceLimits::maximumFrameDurationMilliseconds() { return minimumMaximumDuration; }

int ImageSequenceLimits::maximumTotalDurationMilliseconds() { return minimumMaximumDuration; }

int ImageSequenceLimits::maximumDiagnosticCharacters()
{
    return minimumMaximumDiagnosticStringLength;
}

int ImageSequenceLimits::maximumFormatIdentifierCharacters()
{
    return minimumMaximumFormatIdentifierLength;
}

ImageViewportDisplayLimits::ImageViewportDisplayLimits(QObject* parent)
    : QObject(parent)
{
}

double ImageViewportDisplayLimits::getMinimumManualZoomPercent() const
{
    return minimumManualZoomPercent();
}

double ImageViewportDisplayLimits::getMaximumManualZoomPercent() const
{
    return maximumManualZoomPercent();
}

double ImageViewportDisplayLimits::getManualZoomStepFactor() const
{
    return manualZoomStepFactor();
}

double ImageViewportDisplayLimits::getMaximumPageGap() const { return maximumPageGap(); }

double ImageViewportDisplayLimits::getMinimumCheckerboardCellSize() const
{
    return minimumCheckerboardCellSize();
}

double ImageViewportDisplayLimits::getMaximumCheckerboardCellSize() const
{
    return maximumCheckerboardCellSize();
}

double ImageViewportDisplayLimits::minimumManualZoomPercent()
{
    return ViewportDisplayLimits::minimumManualZoomPercent();
}

double ImageViewportDisplayLimits::maximumManualZoomPercent()
{
    return ViewportDisplayLimits::maximumManualZoomPercent();
}

double ImageViewportDisplayLimits::manualZoomStepFactor()
{
    return ViewportDisplayLimits::manualZoomStepFactor();
}

double ImageViewportDisplayLimits::maximumPageGap()
{
    return ViewportDisplayLimits::maximumPageGap();
}

double ImageViewportDisplayLimits::minimumCheckerboardCellSize()
{
    return ViewportDisplayLimits::minimumCheckerboardCellSize();
}

double ImageViewportDisplayLimits::maximumCheckerboardCellSize()
{
    return ViewportDisplayLimits::maximumCheckerboardCellSize();
}
