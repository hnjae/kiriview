#include "imagesequence_p.h"
#include "imageviewporthelpers_p.h"
#include "timingintervals_p.h"

#include <QtGui/QTransform>

#include <utility>

using namespace ImageViewportInternal;

namespace {
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
    facts.m_loopMode = LoopMode::Infinite;
    facts.m_loopCount = -1;
    return facts;
}

bool ImageSequenceAuthoredAnimationFacts::autoplay() const { return m_autoplay; }

void ImageSequenceAuthoredAnimationFacts::setAutoplay(bool autoplay) { m_autoplay = autoplay; }

bool ImageSequenceAuthoredAnimationFacts::progressiveAnimationReadiness() const
{
    return m_progressiveAnimationReadiness;
}

void ImageSequenceAuthoredAnimationFacts::setProgressiveAnimationReadiness(
    bool progressiveAnimationReadiness)
{
    m_progressiveAnimationReadiness = progressiveAnimationReadiness;
}

ImageSequenceAuthoredAnimationFacts::LoopMode ImageSequenceAuthoredAnimationFacts::loopMode() const
{
    return m_loopMode;
}

int ImageSequenceAuthoredAnimationFacts::loopCount() const { return m_loopCount; }

bool ImageSequenceAuthoredAnimationFacts::setFiniteLoopCount(int loopCount)
{
    if (loopCount < 1) {
        return false;
    }

    m_loopMode = LoopMode::Finite;
    m_loopCount = loopCount;
    return true;
}

std::unique_ptr<ImageSequenceData> ImageSequenceData::still(QSizeF logicalSize, QImage stillImage)
{
    auto data = std::make_unique<ImageSequenceData>();
    data->kind = Kind::Still;
    data->logicalSize = logicalSize;
    data->stillImage = std::move(stillImage);
    return data;
}

std::unique_ptr<ImageSequenceData> ImageSequenceData::timedList(QSizeF logicalSize,
    const QVector<int>& frameDurations, QVector<QImage> frameImages,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
{
    auto data = std::make_unique<ImageSequenceData>();
    data->kind = Kind::TimedList;
    data->logicalSize = logicalSize;
    data->timingIntervals
        = std::make_shared<TimingIntervals>(TimingIntervals::fromFrameDurations(frameDurations));
    data->frameImages = std::move(frameImages);
    data->authoredAnimationFacts = authoredAnimationFacts;
    return data;
}

std::unique_ptr<ImageSequenceData> ImageSequenceData::provider(
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
    ImageSequenceProviderKnownFacts providerKnownFacts,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
    ImageSequenceProviderThreadingContract providerThreadingContract)
{
    auto data = std::make_unique<ImageSequenceData>();
    data->kind = Kind::Provider;
    data->authoredAnimationFacts = authoredAnimationFacts;
    data->providerSessionFactory = std::move(providerSessionFactory);
    data->providerKnownFacts = std::move(providerKnownFacts);
    data->hasCompleteProviderKnownMetadata = data->providerKnownFacts.isComplete();
    data->providerKnownLogicalSize
        = data->providerKnownFacts.isSpecified() ? data->providerKnownFacts.logicalSize() : QSizeF();
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

ImageSequence::ImageSequence(std::unique_ptr<ImageSequenceData> data, QObject* parent)
    : QObject(parent)
    , d(std::move(data))
{
}

ImageSequence::~ImageSequence() = default;

std::shared_ptr<ImageSequence> ImageSequencePrivateAccess::createStill(
    QSizeF logicalSize, QImage stillImage)
{
    return std::shared_ptr<ImageSequence>(
        new ImageSequence(ImageSequenceData::still(logicalSize, std::move(stillImage))));
}

std::shared_ptr<ImageSequence> ImageSequencePrivateAccess::createTimedList(QSizeF logicalSize,
    const QVector<int>& frameDurations, QVector<QImage> frameImages,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
{
    return std::shared_ptr<ImageSequence>(new ImageSequence(ImageSequenceData::timedList(
        logicalSize, frameDurations, std::move(frameImages), authoredAnimationFacts)));
}

std::shared_ptr<ImageSequence> ImageSequencePrivateAccess::createProvider(
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
    ImageSequenceProviderKnownFacts providerKnownFacts,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability,
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts,
    ImageSequenceProviderThreadingContract providerThreadingContract)
{
    return std::shared_ptr<ImageSequence>(new ImageSequence(ImageSequenceData::provider(
        std::move(providerSessionFactory), std::move(providerKnownFacts), timedPlaybackCapability,
        frameSeekCapability, positionSeekCapability, authoredAnimationFacts,
        providerThreadingContract)));
}

bool ImageSequencePrivateAccess::isValid(const ImageSequence* sequence)
{
    if (!sequence || !sequence->d) {
        return false;
    }
    if (isProvider(sequence)) {
        return sequence->d->providerSessionFactory != nullptr;
    }

    return sequence->d->kind != ImageSequenceData::Kind::None && sequence->d->logicalSize.isValid()
        && sequence->d->logicalSize.width() > 0.0 && sequence->d->logicalSize.height() > 0.0
        && (isStill(sequence)
            || (sequence->d->timingIntervals && sequence->d->timingIntervals->isValid()));
}

bool ImageSequencePrivateAccess::isStill(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->kind == ImageSequenceData::Kind::Still;
}

bool ImageSequencePrivateAccess::isTimedList(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->kind == ImageSequenceData::Kind::TimedList;
}

bool ImageSequencePrivateAccess::isProvider(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->kind == ImageSequenceData::Kind::Provider;
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
    if (isStill(sequence) && frame == 0) {
        return sequence->d->stillImage;
    }
    if (isTimedList(sequence) && frame >= 0 && frame < sequence->d->frameImages.size()) {
        return sequence->d->frameImages.at(frame);
    }
    return {};
}

TimingIntervals ImageSequencePrivateAccess::timingIntervals(const ImageSequence* sequence)
{
    return sequence && sequence->d && sequence->d->timingIntervals
        ? *sequence->d->timingIntervals
        : TimingIntervals {};
}

ImageSequenceAuthoredAnimationFacts ImageSequencePrivateAccess::authoredAnimationFacts(
    const ImageSequence* sequence)
{
    return sequence && sequence->d ? sequence->d->authoredAnimationFacts
                                   : ImageSequenceAuthoredAnimationFacts {};
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
    const QImage normalizedImage = normalizedImageForOrientation(image, orientationPolicy);
    const QSizeF logicalSize = normalizedImage.deviceIndependentSize();
    if (!normalizedImage.isNull() && isPositiveFiniteInteger(logicalSize.width())
        && isPositiveFiniteInteger(logicalSize.height())) {
        m_logicalSize = logicalSize;
        m_payloadByteSize = normalizedImage.sizeInBytes();
        m_hasAlphaChannel = normalizedImage.hasAlphaChannel();
        m_orientationPolicy = orientationPolicy;
        if (m_payloadByteSize <= ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
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
        m_hasAlphaChannel = image.hasAlphaChannel();
        m_image = image.copy();
    }
}

bool ImageFrame::isValid() const
{
    return m_logicalSize.isValid() && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0;
}

QSizeF ImageFrame::logicalSize() const { return m_logicalSize; }

qint64 ImageFrame::payloadByteSize() const { return m_payloadByteSize; }

bool ImageFrame::hasAlphaChannel() const { return m_hasAlphaChannel; }

ImageFrame::OrientationPolicy ImageFrame::orientationPolicy() const { return m_orientationPolicy; }

const QImage& ImageFrame::imagePayload() const { return m_image; }

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

QString TimedImageFrameList::errorString() const { return m_errorString; }

QString TimedImageFrameList::warningString() const { return m_warningString; }

ImageSequenceAuthoredAnimationFacts TimedImageFrameList::authoredAnimationFacts() const
{
    return m_authoredAnimationFacts;
}

void TimedImageFrameList::setAuthoredAnimationFacts(
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
{
    m_authoredAnimationFacts = authoredAnimationFacts;
}

bool TimedImageFrameList::appendFrame(const QImage& image, int durationMilliseconds)
{
    ImageFrame frame(image);
    return appendFrame(&frame, durationMilliseconds);
}

bool TimedImageFrameList::appendFrame(ImageFrame* frame, int durationMilliseconds)
{
    if (!frame || !frame->isValid()) {
        setErrorString(QStringLiteral("ImageFrame is required"));
        return false;
    }
    if (durationMilliseconds <= 0) {
        setErrorString(QStringLiteral("frame duration must be positive"));
        return false;
    }
    if (durationMilliseconds > ImageSequenceLimits::maximumFrameDuration()) {
        setErrorString(QStringLiteral("frame duration exceeds maximumFrameDuration"));
        return false;
    }
    if (m_frameDurations.size() >= ImageSequenceLimits::maximumTimedListFrameCount()) {
        setErrorString(QStringLiteral("TimedImageFrameList exceeds maximumTimedListFrameCount"));
        return false;
    }

    const QString limitViolation = frameLimitViolation(*frame);
    if (!limitViolation.isEmpty()) {
        setErrorString(limitViolation);
        return false;
    }

    if (!m_logicalSize.isValid()) {
        m_logicalSize = frame->logicalSize();
    } else if (m_logicalSize != frame->logicalSize()) {
        setErrorString(QStringLiteral("frame logical size must match the timed list logical size"));
        return false;
    }

    if (static_cast<qint64>(totalDuration()) + durationMilliseconds
        > ImageSequenceLimits::maximumTotalSequenceDuration()) {
        setErrorString(QStringLiteral("TimedImageFrameList exceeds maximumTotalSequenceDuration"));
        return false;
    }

    m_frameDurations.append(durationMilliseconds);
    m_images.append(frame->imagePayload());
    if (!m_errorString.isEmpty()) {
        m_errorString.clear();
        emit diagnosticsChanged();
    }
    emit countChanged();
    return true;
}

void TimedImageFrameList::clear()
{
    if (m_frameDurations.isEmpty() && m_errorString.isEmpty() && m_warningString.isEmpty()) {
        return;
    }

    const bool shouldEmitCountChanged = !m_frameDurations.isEmpty();
    const bool shouldEmitDiagnosticsChanged
        = !m_errorString.isEmpty() || !m_warningString.isEmpty();
    m_logicalSize = {};
    m_frameDurations.clear();
    m_images.clear();
    m_errorString.clear();
    m_warningString.clear();
    if (shouldEmitCountChanged) {
        emit countChanged();
    }
    if (shouldEmitDiagnosticsChanged) {
        emit diagnosticsChanged();
    }
}

bool TimedImageFrameList::isValid() const
{
    return m_logicalSize.isValid() && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0
        && !m_frameDurations.isEmpty();
}

QSizeF TimedImageFrameList::logicalSize() const { return m_logicalSize; }

QVector<int> TimedImageFrameList::frameDurations() const { return m_frameDurations; }

QVector<QImage> TimedImageFrameList::frameImages() const { return m_images; }

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
    emit diagnosticsChanged();
}
