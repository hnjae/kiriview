#include "imageviewporthelpers_p.h"

#include <utility>

using namespace ImageViewportInternal;

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
    QSizeF logicalSize, QVector<int> frameDurations, QVector<QImage> frameImages, QObject* parent)
    : QObject(parent)
    , m_timingModel(TimingModel::TimedList)
    , m_logicalSize(logicalSize)
    , m_frameDurations(std::move(frameDurations))
    , m_frameImages(std::move(frameImages))
{
}

ImageSequence::ImageSequence(
    std::shared_ptr<ImageSequenceProviderSessionFactory> providerSessionFactory,
    bool hasProviderKnownMetadata, bool hasCompleteProviderKnownMetadata,
    QSizeF providerKnownLogicalSize, QVector<int> providerKnownFrameDurations,
    ImageSequenceProviderCapabilitySupport timedPlaybackCapability,
    ImageSequenceProviderCapabilitySupport frameSeekCapability,
    ImageSequenceProviderCapabilitySupport positionSeekCapability, QObject* parent)
    : QObject(parent)
    , m_timingModel(TimingModel::Provider)
    , m_providerSessionFactory(std::move(providerSessionFactory))
    , m_hasProviderKnownMetadata(hasProviderKnownMetadata)
    , m_hasCompleteProviderKnownMetadata(hasCompleteProviderKnownMetadata)
    , m_providerKnownLogicalSize(providerKnownLogicalSize)
    , m_providerKnownFrameDurations(std::move(providerKnownFrameDurations))
    , m_providerTimedPlaybackCapability(timedPlaybackCapability)
    , m_providerFrameSeekCapability(frameSeekCapability)
    , m_providerPositionSeekCapability(positionSeekCapability)
{
}

bool ImageSequence::isValid() const
{
    if (isProvider()) {
        return m_providerSessionFactory != nullptr;
    }

    return m_timingModel != TimingModel::None && m_logicalSize.isValid()
        && m_logicalSize.width() > 0.0 && m_logicalSize.height() > 0.0
        && (isStill() || !m_frameDurations.isEmpty());
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
        return m_frameDurations.size();
    }

    return -1;
}

int ImageSequence::totalDuration() const
{
    if (!isTimedList()) {
        return -1;
    }

    int total = 0;
    for (int duration : m_frameDurations) {
        total += duration;
    }
    return total;
}

int ImageSequence::frameStartPosition(int frame) const
{
    if (!isTimedList() || frame < 0 || frame >= m_frameDurations.size()) {
        return -1;
    }

    int position = 0;
    for (int index = 0; index < frame; ++index) {
        position += m_frameDurations.at(index);
    }
    return position;
}

int ImageSequence::frameIndexForPosition(int position) const
{
    if (!isTimedList() || position < 0 || position > totalDuration()) {
        return -1;
    }
    if (position == totalDuration()) {
        return m_frameDurations.size() - 1;
    }

    int frameStart = 0;
    for (int index = 0; index < m_frameDurations.size(); ++index) {
        const int frameEnd = frameStart + m_frameDurations.at(index);
        if (position >= frameStart && position < frameEnd) {
            return index;
        }
        frameStart = frameEnd;
    }

    return -1;
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
    : QObject(parent)
{
    const QSizeF logicalSize = image.deviceIndependentSize();
    if (!image.isNull() && isPositiveFiniteInteger(logicalSize.width())
        && isPositiveFiniteInteger(logicalSize.height())) {
        m_logicalSize = logicalSize;
        m_payloadByteSize = image.sizeInBytes();
        m_hasAlphaChannel = image.hasAlphaChannel();
        if (m_payloadByteSize <= ImageSequenceLimits::maximumPayloadBytesPerFrame()) {
            m_image = image.copy();
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
    int total = 0;
    for (int duration : m_frameDurations) {
        total += duration;
    }
    return total;
}

void TimedImageFrameList::setErrorString(const QString& errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    emit diagnosticsChanged();
}
