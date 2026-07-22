// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagesequence_p.h"
#include "imageviewportlimits_p.h"
#include "timingintervals_p.h"

#include <QtGui/QTransform>

#include <cmath>
#include <utility>

using namespace ImageViewportInternal;

namespace {
bool isPositiveFiniteValue(double value) { return std::isfinite(value) && value > 0.0; }

bool isPositiveFiniteSize(QSizeF size)
{
    return isPositiveFiniteValue(size.width()) && isPositiveFiniteValue(size.height());
}

QImage normalizedImageForOrientation(
    const QImage& image, ImageFrame::OrientationPolicy orientationPolicy)
{
    switch (orientationPolicy) {
    case ImageFrame::OrientationPolicy::Identity:
        return image;
    case ImageFrame::OrientationPolicy::MirrorHorizontally:
        return image.flipped(Qt::Horizontal);
    case ImageFrame::OrientationPolicy::MirrorVertically:
        return image.flipped(Qt::Vertical);
    case ImageFrame::OrientationPolicy::Rotate180:
        return image.transformed(QTransform().rotate(180));
    case ImageFrame::OrientationPolicy::Rotate90:
        return image.transformed(QTransform().rotate(90));
    case ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90:
        return image.flipped(Qt::Horizontal).transformed(QTransform().rotate(90));
    case ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90:
        return image.flipped(Qt::Vertical).transformed(QTransform().rotate(90));
    case ImageFrame::OrientationPolicy::Rotate270:
        return image.transformed(QTransform().rotate(270));
    }
    return image;
}

QSizeF scaleFor(QSizeF sourceLogicalSize, QSize payloadRasterSize)
{
    if (!isPositiveFiniteSize(sourceLogicalSize) || payloadRasterSize.isEmpty()) {
        return {};
    }
    return QSizeF(payloadRasterSize.width() / sourceLogicalSize.width(),
        payloadRasterSize.height() / sourceLogicalSize.height());
}

bool orientationSwapsAxes(ImageFrame::OrientationPolicy orientationPolicy)
{
    switch (orientationPolicy) {
    case ImageFrame::OrientationPolicy::Rotate90:
    case ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90:
    case ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90:
    case ImageFrame::OrientationPolicy::Rotate270:
        return true;
    case ImageFrame::OrientationPolicy::Identity:
    case ImageFrame::OrientationPolicy::MirrorHorizontally:
    case ImageFrame::OrientationPolicy::MirrorVertically:
    case ImageFrame::OrientationPolicy::Rotate180:
        return false;
    }
    return false;
}

QSize normalizedRasterSize(QSize size, ImageFrame::OrientationPolicy orientationPolicy)
{
    if (orientationSwapsAxes(orientationPolicy)) {
        size.transpose();
    }
    return size;
}

QSizeF normalizedLogicalSize(const QImage& image, ImageFrame::OrientationPolicy orientationPolicy)
{
    QSizeF size = image.deviceIndependentSize();
    if (orientationSwapsAxes(orientationPolicy)) {
        size.transpose();
    }
    return size;
}

bool payloadFactsMatchRaster(QSize imageSize, qsizetype retainedImageBytes,
    QSizeF sourceLogicalSize, QSizeF payloadRasterSize, QSizeF sourceToPayloadScale,
    qint64 payloadByteSize)
{
    if (imageSize.isEmpty() || retainedImageBytes <= 0
        || !isPositiveFiniteInteger(sourceLogicalSize.width())
        || !isPositiveFiniteInteger(sourceLogicalSize.height())
        || payloadRasterSize != QSizeF(imageSize) || !isPositiveFiniteSize(sourceToPayloadScale)
        || payloadByteSize < retainedImageBytes) {
        return false;
    }
    const QSizeF mapped(sourceLogicalSize.width() * sourceToPayloadScale.width(),
        sourceLogicalSize.height() * sourceToPayloadScale.height());
    return qAbs(mapped.width() - payloadRasterSize.width()) < 0.0001
        && qAbs(mapped.height() - payloadRasterSize.height()) < 0.0001;
}

bool payloadFactsMatchImage(const QImage& image, QSizeF sourceLogicalSize, QSizeF payloadRasterSize,
    QSizeF sourceToPayloadScale, qint64 payloadByteSize)
{
    return !image.isNull()
        && payloadFactsMatchRaster(image.size(), image.sizeInBytes(), sourceLogicalSize,
            payloadRasterSize, sourceToPayloadScale, payloadByteSize);
}

bool isValidOrientationPolicy(ImageFrame::OrientationPolicy policy)
{
    switch (policy) {
    case ImageFrame::OrientationPolicy::Identity:
    case ImageFrame::OrientationPolicy::MirrorHorizontally:
    case ImageFrame::OrientationPolicy::MirrorVertically:
    case ImageFrame::OrientationPolicy::Rotate180:
    case ImageFrame::OrientationPolicy::Rotate90:
    case ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90:
    case ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90:
    case ImageFrame::OrientationPolicy::Rotate270:
        return true;
    }
    return false;
}

bool payloadWithinPublicEnvelope(const QImage& image)
{
    return !image.isNull() && image.width() > 0 && image.height() > 0
        && image.width() <= ImageSequenceLimits::maximumPayloadRasterWidth()
        && image.height() <= ImageSequenceLimits::maximumPayloadRasterHeight()
        && image.sizeInBytes() > 0
        && image.sizeInBytes() <= ImageSequenceLimits::maximumPayloadBytes();
}

}

ImageSequenceAuthoredAnimationFacts ImageSequenceAuthoredAnimationFacts::finiteLoop(int loopCount)
{
    ImageSequenceAuthoredAnimationFacts facts;
    facts.setFiniteLoopCount(loopCount);
    return facts;
}

ImageSequenceAuthoredAnimationFacts ImageSequenceAuthoredAnimationFacts::infiniteLoop()
{
    ImageSequenceAuthoredAnimationFacts facts;
    facts.m_loopMode = ImageSequenceAuthoredAnimationLoopMode::Infinite;
    facts.m_loopCount = -1;
    return facts;
}

bool ImageSequenceAuthoredAnimationFacts::autoplay() const { return m_autoplay; }

void ImageSequenceAuthoredAnimationFacts::setAutoplay(bool autoplay) { m_autoplay = autoplay; }

ImageSequenceAuthoredAnimationLoopMode ImageSequenceAuthoredAnimationFacts::loopMode() const
{
    return m_loopMode;
}

int ImageSequenceAuthoredAnimationFacts::loopCount() const { return m_loopCount; }

bool ImageSequenceAuthoredAnimationFacts::setFiniteLoopCount(int loopCount)
{
    if (loopCount < 2) {
        return false;
    }

    m_loopMode = ImageSequenceAuthoredAnimationLoopMode::Finite;
    m_loopCount = loopCount;
    return true;
}

bool ImageSequenceAuthoredAnimationFacts::isValid() const
{
    switch (m_loopMode) {
    case ImageSequenceAuthoredAnimationLoopMode::Unavailable:
        return m_loopCount == -1;
    case ImageSequenceAuthoredAnimationLoopMode::PlayOnce:
        return m_loopCount == 1;
    case ImageSequenceAuthoredAnimationLoopMode::Finite:
        return m_loopCount >= 2;
    case ImageSequenceAuthoredAnimationLoopMode::Infinite:
        return m_loopCount == -1;
    }
    return false;
}

std::unique_ptr<ImageSequence::Data> ImageSequence::Data::still(
    QSizeF logicalSize, FramePayload payload)
{
    auto data = std::make_unique<ImageSequence::Data>();
    data->kind = Kind::Still;
    data->logicalSize = logicalSize;
    data->stillPayload = std::move(payload);
    data->authoredAnimationFactsAvailable = true;
    return data;
}

std::unique_ptr<ImageSequence::Data> ImageSequence::Data::timedList(QSizeF logicalSize,
    const QVector<int>& frameDurations, QVector<FramePayload> framePayloads,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
{
    auto data = std::make_unique<ImageSequence::Data>();
    data->kind = Kind::TimedList;
    data->logicalSize = logicalSize;
    data->timingIntervals
        = std::make_shared<TimingIntervals>(TimingIntervals::fromFrameDurations(frameDurations));
    data->framePayloads = std::move(framePayloads);
    data->authoredAnimationFacts = authoredAnimationFacts;
    data->authoredAnimationFactsAvailable = true;
    return data;
}

std::unique_ptr<ImageSequence::Data> ImageSequence::Data::provider(
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
    ImageSequenceProviderKnownFacts providerKnownFacts,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
    bool authoredAnimationFactsAvailable,
    ImageSequenceProviderThreadingContract providerThreadingContract)
{
    auto data = std::make_unique<ImageSequence::Data>();
    data->kind = Kind::Provider;
    data->authoredAnimationFacts = authoredAnimationFacts;
    data->authoredAnimationFactsAvailable = authoredAnimationFactsAvailable;
    data->providerSessionFactory = std::move(providerSessionFactory);
    data->providerKnownFacts = std::move(providerKnownFacts);
    data->hasCompleteProviderKnownMetadata = data->providerKnownFacts.isComplete();
    data->providerKnownLogicalSize = data->providerKnownFacts.isSpecified()
        ? data->providerKnownFacts.logicalSize()
        : QSizeF();
    data->providerKnownTimingIntervals = data->providerKnownFacts.isTimedFrameList()
        ? std::make_shared<TimingIntervals>(
              TimingIntervals::fromFrameDurations(data->providerKnownFacts.frameDurations()))
        : nullptr;
    data->providerTimedPlaybackCapability = timedPlaybackCapability;
    data->providerFrameSeekCapability = frameSeekCapability;
    data->providerPositionSeekCapability = positionSeekCapability;
    data->providerThreadingContract = providerThreadingContract;
    return data;
}

ImageSequence::ImageSequence(std::unique_ptr<ImageSequence::Data> data, QObject* parent)
    : QObject(parent)
    , d(std::move(data))
{
}

ImageSequence::~ImageSequence() = default;

std::shared_ptr<ImageSequence> ImageSequencePrivateAccess::createStill(
    QSizeF logicalSize, FramePayload payload)
{
    std::shared_ptr<ImageSequence> sequence(
        new ImageSequence(ImageSequence::Data::still(logicalSize, std::move(payload))));
    sequence->d->owner = sequence;
    return sequence;
}

std::shared_ptr<ImageSequence> ImageSequencePrivateAccess::createTimedList(QSizeF logicalSize,
    const QVector<int>& frameDurations, QVector<FramePayload> framePayloads,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
{
    std::shared_ptr<ImageSequence> sequence(new ImageSequence(ImageSequence::Data::timedList(
        logicalSize, frameDurations, std::move(framePayloads), authoredAnimationFacts)));
    sequence->d->owner = sequence;
    return sequence;
}

std::shared_ptr<ImageSequence> ImageSequencePrivateAccess::createProvider(
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
    ImageSequenceProviderKnownFacts providerKnownFacts,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
    bool authoredAnimationFactsAvailable,
    ImageSequenceProviderThreadingContract providerThreadingContract)
{
    std::shared_ptr<ImageSequence> sequence(new ImageSequence(ImageSequence::Data::provider(
        std::move(providerSessionFactory), std::move(providerKnownFacts), timedPlaybackCapability,
        frameSeekCapability, positionSeekCapability, authoredAnimationFacts,
        authoredAnimationFactsAvailable, providerThreadingContract)));
    sequence->d->owner = sequence;
    return sequence;
}

bool ImageSequencePrivateAccess::isValid(const ImageSequence* sequence)
{
    if (!sequence || !sequence->d) {
        return false;
    }
    if (isProvider(sequence)) {
        return sequence->d->providerSessionFactory != nullptr;
    }

    return sequence->d->kind != ImageSequence::Data::Kind::None
        && sequence->d->logicalSize.isValid() && sequence->d->logicalSize.width() > 0.0
        && sequence->d->logicalSize.height() > 0.0
        && (isStill(sequence)
            || (sequence->d->timingIntervals && sequence->d->timingIntervals->isValid()));
}

bool ImageSequencePrivateAccess::isStill(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->kind == ImageSequence::Data::Kind::Still;
}

bool ImageSequencePrivateAccess::isTimedList(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->kind == ImageSequence::Data::Kind::TimedList;
}

bool ImageSequencePrivateAccess::isProvider(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->kind == ImageSequence::Data::Kind::Provider;
}

QSizeF ImageSequencePrivateAccess::logicalSize(const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->logicalSize : QSizeF {};
}

int ImageSequencePrivateAccess::frameCount(const ImageSequence* sequence)
{
    if (isStill(sequence)) {
        return 1;
    }
    if (isTimedList(sequence)) {
        return sequence->d->timingIntervals ? sequence->d->timingIntervals->frameCount() : -1;
    }

    return -1;
}

int ImageSequencePrivateAccess::totalDuration(const ImageSequence* sequence)
{
    if (!isTimedList(sequence)) {
        return -1;
    }

    return sequence->d->timingIntervals ? sequence->d->timingIntervals->totalDuration() : -1;
}

int ImageSequencePrivateAccess::frameStartPosition(const ImageSequence* sequence, int frame)
{
    if (!isTimedList(sequence)) {
        return -1;
    }

    return sequence->d->timingIntervals ? sequence->d->timingIntervals->frameStartPosition(frame)
                                        : -1;
}

int ImageSequencePrivateAccess::frameIndexForPosition(const ImageSequence* sequence, int position)
{
    if (!isTimedList(sequence)) {
        return -1;
    }

    return sequence->d->timingIntervals
        ? sequence->d->timingIntervals->frameIndexForPosition(position)
        : -1;
}

QImage ImageSequencePrivateAccess::frameImage(const ImageSequence* sequence, int frame)
{
    return framePayload(sequence, frame).image;
}

FramePayload ImageSequencePrivateAccess::framePayload(const ImageSequence* sequence, int frame)
{
    if (isStill(sequence) && frame == 0) {
        return sequence->d->stillPayload;
    }
    if (isTimedList(sequence) && frame >= 0 && frame < sequence->d->framePayloads.size()) {
        return sequence->d->framePayloads.at(frame);
    }
    return {};
}

FramePayloadFacts ImageSequencePrivateAccess::framePayloadFacts(
    const ImageSequence* sequence, int frame)
{
    return framePayload(sequence, frame).facts;
}

TimingIntervals ImageSequencePrivateAccess::timingIntervals(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->timingIntervals ? *sequence->d->timingIntervals
                                                                   : TimingIntervals {};
}

ImageSequenceAuthoredAnimationFacts ImageSequencePrivateAccess::authoredAnimationFacts(
    const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->authoredAnimationFacts
                                   : ImageSequenceAuthoredAnimationFacts {};
}

bool ImageSequencePrivateAccess::authoredAnimationFactsAvailable(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->authoredAnimationFactsAvailable;
}

bool ImageSequencePrivateAccess::hasCompleteProviderKnownMetadata(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->hasCompleteProviderKnownMetadata;
}

ImageSequenceProviderKnownFacts ImageSequencePrivateAccess::providerKnownFacts(
    const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->providerKnownFacts
                                   : ImageSequenceProviderKnownFacts {};
}

QSizeF ImageSequencePrivateAccess::providerKnownLogicalSize(const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->providerKnownLogicalSize : QSizeF {};
}

TimingIntervals ImageSequencePrivateAccess::providerKnownTimingIntervals(
    const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->providerKnownTimingIntervals
        ? *sequence->d->providerKnownTimingIntervals
        : TimingIntervals {};
}

ImageSequenceProviderCapabilitySupport ImageSequencePrivateAccess::providerTimedPlaybackCapability(
    const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->providerTimedPlaybackCapability
                                   : ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport ImageSequencePrivateAccess::providerFrameSeekCapability(
    const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->providerFrameSeekCapability
                                   : ImageSequenceProviderCapabilitySupport::Unavailable;
}

ImageSequenceProviderCapabilitySupport ImageSequencePrivateAccess::providerPositionSeekCapability(
    const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->providerPositionSeekCapability
                                   : ImageSequenceProviderCapabilitySupport::Unavailable;
}

std::shared_ptr<ImageSequenceProviderSessionFactory>
ImageSequencePrivateAccess::providerSessionFactory(const ImageSequence* sequence)
{
    return sequence && sequence->d && isProvider(sequence) ? sequence->d->providerSessionFactory
                                                           : nullptr;
}

ImageSequenceProviderThreadingContract ImageSequencePrivateAccess::providerThreadingContract(
    const ImageSequence* sequence)
{
    return sequence && sequence->d && isProvider(sequence)
        ? sequence->d->providerThreadingContract
        : ImageSequenceProviderThreadingContract::AffinityBound;
}

std::shared_ptr<ImageSequence> ImageSequencePrivateAccess::owner(const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->owner.lock() : nullptr;
}

ImageFrame::ImageFrame(QObject* parent)
    : QObject(parent)
{
}

ImageFrame::ImageFrame(const QImage& image, QObject* parent)
    : ImageFrame(image, OrientationPolicy::Identity, parent)
{
}

ImageFrame::ImageFrame(const QImage& image, OrientationPolicy orientationPolicy, QObject* parent)
    : QObject(parent)
{
    if (!payloadWithinPublicEnvelope(image)) {
        const QSizeF logicalSize = normalizedLogicalSize(image, orientationPolicy);
        const QSize rasterSize = normalizedRasterSize(image.size(), orientationPolicy);
        if (!image.isNull() && isValidOrientationPolicy(orientationPolicy)
            && isPositiveFiniteInteger(logicalSize.width())
            && isPositiveFiniteInteger(logicalSize.height())) {
            m_logicalSize = logicalSize;
            m_payloadByteSize = image.sizeInBytes();
            m_payloadRasterSize = QSizeF(rasterSize);
            m_sourceToPayloadScale = scaleFor(logicalSize, rasterSize);
            m_quality = ImageViewportPayloadQuality::Exact;
            m_exactness = ImageViewportPayloadExactness::ExactForSource;
            m_hasAlpha = image.hasAlphaChannel();
            m_orientationPolicy = orientationPolicy;
        }
        return;
    }
    const QImage normalizedImage = normalizedImageForOrientation(image, orientationPolicy);
    const QSizeF logicalSize = normalizedImage.deviceIndependentSize();
    if (!normalizedImage.isNull() && isPositiveFiniteInteger(logicalSize.width())
        && isPositiveFiniteInteger(logicalSize.height())) {
        m_logicalSize = logicalSize;
        m_payloadByteSize = normalizedImage.sizeInBytes();
        m_payloadRasterSize = QSizeF(normalizedImage.size());
        m_sourceToPayloadScale = scaleFor(logicalSize, normalizedImage.size());
        m_quality = ImageViewportPayloadQuality::Exact;
        m_exactness = ImageViewportPayloadExactness::ExactForSource;
        m_hasAlpha = normalizedImage.hasAlphaChannel();
        m_orientationPolicy = orientationPolicy;
        if (m_payloadRasterSize.width() <= ImageSequenceLimits::maximumPayloadRasterWidth()
            && m_payloadRasterSize.height() <= ImageSequenceLimits::maximumPayloadRasterHeight()
            && m_payloadByteSize <= ImageSequenceLimits::maximumPayloadBytes()) {
            m_image = normalizedImage.copy();
        }
    }
}

ImageFrame::ImageFrame(const QImage& image, QSizeF sourceLogicalSize, QSizeF payloadRasterSize,
    QSizeF sourceToPayloadScale, qint64 payloadByteSize, ImageViewportPayloadQuality quality,
    ImageViewportPayloadExactness exactness, bool hasAlpha, OrientationPolicy orientationPolicy,
    QString formatIdentifier, QObject* parent)
    : QObject(parent)
{
    if (!payloadWithinPublicEnvelope(image)
        || payloadRasterSize.width() > ImageSequenceLimits::maximumPayloadRasterWidth()
        || payloadRasterSize.height() > ImageSequenceLimits::maximumPayloadRasterHeight()
        || payloadByteSize > ImageSequenceLimits::maximumPayloadBytes()) {
        const QSize rasterSize = normalizedRasterSize(image.size(), orientationPolicy);
        const bool exactPair = (quality == ImageViewportPayloadQuality::Exact)
            == (exactness == ImageViewportPayloadExactness::ExactForSource);
        if (!image.isNull() && isValidOrientationPolicy(orientationPolicy)
            && payloadFactsMatchRaster(rasterSize, image.sizeInBytes(), sourceLogicalSize,
                payloadRasterSize, sourceToPayloadScale, payloadByteSize)
            && exactPair && hasAlpha == image.hasAlphaChannel()) {
            m_logicalSize = sourceLogicalSize;
            m_payloadByteSize = payloadByteSize;
            m_payloadRasterSize = payloadRasterSize;
            m_sourceToPayloadScale = sourceToPayloadScale;
            m_quality = quality;
            m_exactness = exactness;
            m_hasAlpha = hasAlpha;
            m_orientationPolicy = orientationPolicy;
            m_formatIdentifier = std::move(formatIdentifier);
        }
        return;
    }
    const QImage normalizedImage = normalizedImageForOrientation(image, orientationPolicy);
    const bool exactPair = (quality == ImageViewportPayloadQuality::Exact)
        == (exactness == ImageViewportPayloadExactness::ExactForSource);
    if (payloadFactsMatchImage(normalizedImage, sourceLogicalSize, payloadRasterSize,
            sourceToPayloadScale, payloadByteSize)
        && exactPair && hasAlpha == normalizedImage.hasAlphaChannel()
        && isValidOrientationPolicy(orientationPolicy)) {
        m_logicalSize = sourceLogicalSize;
        m_payloadByteSize = payloadByteSize;
        m_payloadRasterSize = payloadRasterSize;
        m_sourceToPayloadScale = sourceToPayloadScale;
        m_quality = quality;
        m_exactness = exactness;
        m_hasAlpha = hasAlpha;
        m_orientationPolicy = orientationPolicy;
        m_formatIdentifier = std::move(formatIdentifier);
        if (m_payloadRasterSize.width() <= ImageSequenceLimits::maximumPayloadRasterWidth()
            && m_payloadRasterSize.height() <= ImageSequenceLimits::maximumPayloadRasterHeight()
            && m_payloadByteSize <= ImageSequenceLimits::maximumPayloadBytes()) {
            m_image = normalizedImage.copy();
        }
    }
}

ImageFrame::ImageFrame(const QImage& image, qsizetype payloadByteSizeOverride, QObject* parent)
    : QObject(parent)
{
    const QSizeF logicalSize = image.deviceIndependentSize();
    if (!image.isNull() && isPositiveFiniteInteger(logicalSize.width())
        && isPositiveFiniteInteger(logicalSize.height())) {
        m_logicalSize = logicalSize;
        m_payloadByteSize = payloadByteSizeOverride;
        m_payloadRasterSize = QSizeF(image.size());
        m_sourceToPayloadScale = scaleFor(logicalSize, image.size());
        m_quality = ImageViewportPayloadQuality::Exact;
        m_exactness = ImageViewportPayloadExactness::ExactForSource;
        m_hasAlpha = image.hasAlphaChannel();
        m_image = image.copy();
    }
}

bool ImageFrame::isValid() const
{
    return m_logicalSize.isValid() && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0;
}

QSizeF ImageFrame::sourceLogicalSize() const { return m_logicalSize; }

qint64 ImageFrame::payloadByteSize() const { return m_payloadByteSize; }

QSizeF ImageFrame::payloadRasterSize() const { return m_payloadRasterSize; }

QSizeF ImageFrame::sourceToPayloadScale() const { return m_sourceToPayloadScale; }

ImageViewportPayloadQuality ImageFrame::quality() const { return m_quality; }

ImageViewportPayloadExactness ImageFrame::exactness() const { return m_exactness; }

bool ImageFrame::hasAlpha() const { return m_hasAlpha; }

ImageFrame::OrientationPolicy ImageFrame::orientationPolicy() const { return m_orientationPolicy; }

QString ImageFrame::formatIdentifier() const { return m_formatIdentifier; }

TimedImageFrame::TimedImageFrame(ImageFrame* frame, int startPosition, int duration)
    : m_startPosition(startPosition)
    , m_duration(duration)
{
    if (!frame) {
        return;
    }
    m_frame = std::make_shared<ImageFrame>();
    m_frame->m_image = frame->m_image;
    m_frame->m_logicalSize = frame->m_logicalSize;
    m_frame->m_payloadByteSize = frame->m_payloadByteSize;
    m_frame->m_payloadRasterSize = frame->m_payloadRasterSize;
    m_frame->m_sourceToPayloadScale = frame->m_sourceToPayloadScale;
    m_frame->m_quality = frame->m_quality;
    m_frame->m_exactness = frame->m_exactness;
    m_frame->m_hasAlpha = frame->m_hasAlpha;
    m_frame->m_orientationPolicy = frame->m_orientationPolicy;
    m_frame->m_formatIdentifier = frame->m_formatIdentifier;
}

ImageFrame* TimedImageFrame::frame() const { return m_frame.get(); }

int TimedImageFrame::startPosition() const { return m_startPosition; }

int TimedImageFrame::duration() const { return m_duration; }

bool TimedImageFrame::isValid() const
{
    return m_frame && m_frame->isValid() && m_startPosition >= 0 && m_duration > 0;
}

ImageSequenceProviderFrameHandle::ImageSequenceProviderFrameHandle(
    std::unique_ptr<ImageFrame> frame, QObject* parent)
    : QObject(parent)
    , m_frame(frame.release())
    , m_releaseFrame([](ImageFrame* releasedFrame) { delete releasedFrame; })
{
}

ImageSequenceProviderFrameHandle::ImageSequenceProviderFrameHandle(
    ImageFrame* frame, ReleaseCallback releaseFrame, QObject* parent)
    : QObject(parent)
    , m_frame(frame)
    , m_releaseFrame(std::move(releaseFrame))
{
}

ImageSequenceProviderFrameHandle::~ImageSequenceProviderFrameHandle() { release(); }

ImageFrame* ImageSequenceProviderFrameHandle::frame() const { return m_frame; }

void ImageSequenceProviderFrameHandle::release()
{
    if (m_released) {
        return;
    }

    m_released = true;
    ImageFrame* releasedFrame = m_frame;
    m_frame = nullptr;
    if (m_releaseFrame) {
        m_releaseFrame(releasedFrame);
    }
}

std::unique_ptr<ImageFrame> ImageFramePrivateAccess::createWithPayloadByteSize(
    const QImage& image, qsizetype payloadByteSize)
{
    return std::unique_ptr<ImageFrame>(new ImageFrame(image, payloadByteSize));
}

QImage ImageFramePrivateAccess::image(const ImageFrame& frame) { return frame.m_image; }

TimedImageFrameList::TimedImageFrameList(QObject* parent)
    : QObject(parent)
{
}

int TimedImageFrameList::count() const { return m_frameDurations.size(); }

QList<TimedImageFrame> TimedImageFrameList::frames() const { return m_frames; }

QString TimedImageFrameList::errorString() const { return m_errorString; }

bool TimedImageFrameList::autoplay() const { return m_authoredAnimationFacts.autoplay(); }

void TimedImageFrameList::setAutoplay(bool autoplay)
{
    if (m_authoredAnimationFacts.autoplay() == autoplay) {
        return;
    }
    m_authoredAnimationFacts.setAutoplay(autoplay);
    Q_EMIT animationFactsChanged();
}

ImageSequenceAuthoredAnimationLoopMode TimedImageFrameList::loopMode() const
{
    return m_authoredAnimationFacts.loopMode();
}

int TimedImageFrameList::loopCount() const { return m_authoredAnimationFacts.loopCount(); }

ImageSequenceAuthoredAnimationFacts TimedImageFrameList::authoredAnimationFacts() const
{
    return m_authoredAnimationFacts;
}

void TimedImageFrameList::setAuthoredAnimationFacts(
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
{
    if (m_authoredAnimationFacts.loopMode() == authoredAnimationFacts.loopMode()
        && m_authoredAnimationFacts.loopCount() == authoredAnimationFacts.loopCount()
        && m_authoredAnimationFacts.autoplay() == authoredAnimationFacts.autoplay()) {
        return;
    }
    m_authoredAnimationFacts = authoredAnimationFacts;
    Q_EMIT animationFactsChanged();
}

bool TimedImageFrameList::appendFrame(const QImage& image, int durationMilliseconds)
{
    ImageFrame frame(image);
    return appendFrame(&frame, durationMilliseconds);
}

bool TimedImageFrameList::appendFrame(ImageFrame* frame, int durationMilliseconds)
{
    return appendFrame(TimedImageFrame(frame, totalDuration(), durationMilliseconds));
}

bool TimedImageFrameList::appendFrame(const TimedImageFrame& timedFrame)
{
    ImageFrame* frame = timedFrame.frame();
    const int durationMilliseconds = timedFrame.duration();
    if (!frame || !frame->isValid()) {
        setErrorString(QStringLiteral("ImageFrame is required"));
        return false;
    }
    if (durationMilliseconds <= 0) {
        setErrorString(QStringLiteral("frame duration must be positive"));
        return false;
    }
    if (timedFrame.startPosition() != totalDuration()) {
        setErrorString(QStringLiteral("timed frames must form contiguous intervals"));
        return false;
    }
    if (durationMilliseconds > ImageSequenceLimits::maximumFrameDurationMilliseconds()) {
        setErrorString(QStringLiteral("frame duration exceeds maximumFrameDurationMilliseconds"));
        return false;
    }
    if (m_frameDurations.size() >= ImageSequenceLimits::maximumFrameCount()) {
        setErrorString(QStringLiteral("TimedImageFrameList exceeds maximumFrameCount"));
        return false;
    }

    const QString limitViolation = frameLimitViolation(*frame);
    if (!limitViolation.isEmpty()) {
        setErrorString(limitViolation);
        return false;
    }

    if (!m_logicalSize.isValid()) {
        m_logicalSize = frame->sourceLogicalSize();
    } else if (m_logicalSize != frame->sourceLogicalSize()) {
        setErrorString(QStringLiteral("frame logical size must match the timed list logical size"));
        return false;
    }

    if (static_cast<qint64>(totalDuration()) + durationMilliseconds
        > ImageSequenceLimits::maximumTotalDurationMilliseconds()) {
        setErrorString(
            QStringLiteral("TimedImageFrameList exceeds maximumTotalDurationMilliseconds"));
        return false;
    }

    m_frameDurations.append(durationMilliseconds);
    m_frames.append(timedFrame);
    if (!m_errorString.isEmpty()) {
        m_errorString.clear();
        Q_EMIT diagnosticsChanged();
    }
    Q_EMIT countChanged();
    return true;
}

void TimedImageFrameList::clear()
{
    if (m_frameDurations.isEmpty() && m_errorString.isEmpty()) {
        return;
    }

    const bool shouldEmitCountChanged = !m_frameDurations.isEmpty();
    const bool shouldEmitDiagnosticsChanged = !m_errorString.isEmpty();
    m_logicalSize = {};
    m_frameDurations.clear();
    m_frames.clear();
    m_errorString.clear();
    if (shouldEmitCountChanged) {
        Q_EMIT countChanged();
    }
    if (shouldEmitDiagnosticsChanged) {
        Q_EMIT diagnosticsChanged();
    }
}

bool TimedImageFrameList::isValid() const
{
    return m_logicalSize.isValid() && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0
        && !m_frameDurations.isEmpty();
}

QSizeF TimedImageFrameList::logicalSize() const { return m_logicalSize; }

QVector<int> TimedImageFrameList::frameDurations() const { return m_frameDurations; }

int TimedImageFrameList::totalDuration() const
{
    const TimingIntervals timing = TimingIntervals::fromFrameDurations(m_frameDurations);
    return timing.isValid() ? timing.totalDuration() : 0;
}

void TimedImageFrameList::setErrorString(const QString& errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    Q_EMIT diagnosticsChanged();
}
