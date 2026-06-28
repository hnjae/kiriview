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

ImageSequence::ImageSequence(QObject* parent)
    : QObject(parent)
{
}

ImageSequence::ImageSequence(QSizeF logicalSize, QImage stillImage, QObject* parent)
    : QObject(parent)
    , m_timingModel(TimingModel::Still)
    , m_logicalSize(logicalSize)
    , m_stillImage(std::move(stillImage))
{
}

ImageSequence::ImageSequence(
    QSizeF logicalSize, const QVector<int>& frameDurations, QVector<QImage> frameImages,
    QObject* parent)
    : QObject(parent)
    , m_timingModel(TimingModel::TimedList)
    , m_logicalSize(logicalSize)
    , m_timingIntervals(
          std::make_shared<TimingIntervals>(TimingIntervals::fromFrameDurations(frameDurations)))
    , m_frameImages(std::move(frameImages))
{
}

ImageSequence::ImageSequence(
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
    ImageSequenceProviderKnownFacts providerKnownFacts,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability,
    ImageSequenceProviderThreadingContract providerThreadingContract, QObject* parent)
    : QObject(parent)
    , m_timingModel(TimingModel::Provider)
    , m_providerSessionFactory(std::move(providerSessionFactory))
    , m_providerKnownFacts(std::move(providerKnownFacts))
    , m_hasProviderKnownMetadata(m_providerKnownFacts.isSpecified())
    , m_hasCompleteProviderKnownMetadata(m_providerKnownFacts.isComplete())
    , m_providerKnownLogicalSize(
          m_providerKnownFacts.isSpecified() ? m_providerKnownFacts.logicalSize() : QSizeF())
    , m_providerKnownFrameCount(m_providerKnownFacts.frameCount())
    , m_providerKnownTimingIntervals(m_providerKnownFacts.isTimedFrameList()
              ? std::make_shared<TimingIntervals>(
                    TimingIntervals::fromFrameDurations(m_providerKnownFacts.frameDurations()))
              : nullptr)
    , m_providerTimedPlaybackCapability(timedPlaybackCapability)
    , m_providerFrameSeekCapability(frameSeekCapability)
    , m_providerPositionSeekCapability(positionSeekCapability)
    , m_providerThreadingContract(providerThreadingContract)
{
}

bool ImageSequence::isValid() const
{
    if (isProvider()) {
        return m_providerSessionFactory != nullptr;
    }

    return m_timingModel != TimingModel::None && m_logicalSize.isValid()
        && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0
        && (isStill() || (m_timingIntervals && m_timingIntervals->isValid()));
}

bool ImageSequence::isStill() const { return m_timingModel == TimingModel::Still; }

bool ImageSequence::isTimedList() const { return m_timingModel == TimingModel::TimedList; }

bool ImageSequence::isProvider() const { return m_timingModel == TimingModel::Provider; }

QSizeF ImageSequence::logicalSize() const { return m_logicalSize; }

int ImageSequence::frameCount() const
{
    if (isStill()) {
        return 1;
    }
    if (isTimedList()) {
        return m_timingIntervals ? m_timingIntervals->frameCount() : -1;
    }

    return -1;
}

int ImageSequence::totalDuration() const
{
    if (!isTimedList()) {
        return -1;
    }

    return m_timingIntervals ? m_timingIntervals->totalDuration() : -1;
}

int ImageSequence::frameStartPosition(int frame) const
{
    if (!isTimedList()) {
        return -1;
    }

    return m_timingIntervals ? m_timingIntervals->frameStartPosition(frame) : -1;
}

int ImageSequence::frameIndexForPosition(int position) const
{
    if (!isTimedList()) {
        return -1;
    }

    return m_timingIntervals ? m_timingIntervals->frameIndexForPosition(position) : -1;
}

QImage ImageSequence::frameImage(int frame) const
{
    if (isStill() && frame == 0) {
        return m_stillImage;
    }
    if (isTimedList() && frame >= 0 && frame < m_frameImages.size()) {
        return m_frameImages.at(frame);
    }
    return {};
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
QImage ImageSequence::frameImageForTest(int frame) const { return frameImage(frame); }
#endif

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

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ImageFrame::ImageFrame(const QImage& image, qsizetype payloadByteSizeForTest, QObject* parent)
    : QObject(parent)
{
    const QSizeF logicalSize = image.deviceIndependentSize();
    if (!image.isNull() && isPositiveFiniteInteger(logicalSize.width())
        && isPositiveFiniteInteger(logicalSize.height())) {
        m_logicalSize = logicalSize;
        m_payloadByteSize = payloadByteSizeForTest;
        m_hasAlphaChannel = image.hasAlphaChannel();
        m_image = image.copy();
    }
}
#endif

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

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
QImage ImageFrame::imageForTest() const { return m_image; }
#endif

TimedImageFrameList::TimedImageFrameList(QObject* parent)
    : QObject(parent)
{
}

int TimedImageFrameList::count() const { return m_frameDurations.size(); }

QString TimedImageFrameList::errorString() const { return m_errorString; }

QString TimedImageFrameList::warningString() const { return m_warningString; }

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
    m_frameImages.append(frame->imagePayload());
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
    m_frameImages.clear();
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

QVector<QImage> TimedImageFrameList::frameImages() const { return m_frameImages; }

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
