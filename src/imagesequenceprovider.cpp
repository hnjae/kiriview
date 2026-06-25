#include "imageviewporthelpers_p.h"

#include <algorithm>
#include <utility>

using namespace ImageViewportInternal;

ImageSequenceProviderAdapter::ImageSequenceProviderAdapter(QObject *parent)
    : QObject(parent)
{
}

std::shared_ptr<ImageSequenceProviderSessionFactory> ImageSequenceProviderAdapter::sessionFactory() const
{
    return {};
}

ImageSequenceProviderMetadata ImageSequenceProviderAdapter::knownMetadata() const
{
    return {};
}

ImageSequenceProviderAdapter::CapabilitySupport ImageSequenceProviderAdapter::timedPlaybackCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderAdapter::CapabilitySupport ImageSequenceProviderAdapter::frameSeekCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderAdapter::CapabilitySupport ImageSequenceProviderAdapter::positionSeekCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderRequestToken::ImageSequenceProviderRequestToken(quint64 id)
    : m_id(id)
{
}

quint64 ImageSequenceProviderRequestToken::id() const
{
    return m_id;
}

bool ImageSequenceProviderRequestToken::isValid() const
{
    return m_id != 0;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::still(const QSizeF &logicalSize)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_timingModel = TimingModel::Still;
    metadata.m_logicalSize = logicalSize;
    return metadata;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::fixedDurationFrames(const QSizeF &logicalSize, int frameCount, int frameDuration)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_timingModel = TimingModel::FixedDurationFrames;
    metadata.m_logicalSize = logicalSize;
    if (frameCount > 0) {
        const int retainedFrameCount = std::min(frameCount, ImageSequenceLimits::maximumTimedListFrameCount() + 1);
        metadata.m_frameDurations = QVector<int>(retainedFrameCount, frameDuration);
    }
    return metadata;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::timedFrameList(const QSizeF &logicalSize, QVector<int> frameDurations)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_timingModel = TimingModel::TimedFrameList;
    metadata.m_logicalSize = logicalSize;
    metadata.m_frameDurations = std::move(frameDurations);
    return metadata;
}

bool ImageSequenceProviderMetadata::isValid() const
{
    if (!isPositiveFiniteInteger(m_logicalSize.width()) || !isPositiveFiniteInteger(m_logicalSize.height())) {
        return false;
    }
    if (isStill()) {
        return true;
    }
    if (isTimedFrameList()) {
        if (m_frameDurations.isEmpty()) {
            return false;
        }
        for (int duration : m_frameDurations) {
            if (duration <= 0) {
                return false;
            }
        }
        return true;
    }

    return false;
}

bool ImageSequenceProviderMetadata::isSpecified() const
{
    return m_timingModel != TimingModel::Invalid;
}

bool ImageSequenceProviderMetadata::isStill() const
{
    return m_timingModel == TimingModel::Still;
}

bool ImageSequenceProviderMetadata::isTimedFrameList() const
{
    return m_timingModel == TimingModel::FixedDurationFrames || m_timingModel == TimingModel::TimedFrameList;
}

QSizeF ImageSequenceProviderMetadata::logicalSize() const
{
    return m_logicalSize;
}

QVector<int> ImageSequenceProviderMetadata::frameDurations() const
{
    return m_frameDurations;
}

ImageSequenceProviderFrameMetadata ImageSequenceProviderFrameMetadata::stillFrame()
{
    ImageSequenceProviderFrameMetadata metadata;
    metadata.m_timingModel = TimingModel::Still;
    metadata.m_frame = 0;
    return metadata;
}

ImageSequenceProviderFrameMetadata ImageSequenceProviderFrameMetadata::timedFrame(int frame, int frameStartPosition, int frameDuration)
{
    ImageSequenceProviderFrameMetadata metadata;
    metadata.m_timingModel = TimingModel::TimedFrame;
    metadata.m_frame = frame;
    metadata.m_frameStartPosition = frameStartPosition;
    metadata.m_frameDuration = frameDuration;
    return metadata;
}

bool ImageSequenceProviderFrameMetadata::isValid() const
{
    if (isStillFrame()) {
        return m_frame == 0;
    }
    if (isTimedFrame()) {
        return m_frame >= 0 && m_frameStartPosition >= 0 && (m_frameDuration == -1 || m_frameDuration > 0);
    }

    return false;
}

bool ImageSequenceProviderFrameMetadata::isStillFrame() const
{
    return m_timingModel == TimingModel::Still;
}

bool ImageSequenceProviderFrameMetadata::isTimedFrame() const
{
    return m_timingModel == TimingModel::TimedFrame;
}

int ImageSequenceProviderFrameMetadata::frame() const
{
    return m_frame;
}

int ImageSequenceProviderFrameMetadata::frameStartPosition() const
{
    return m_frameStartPosition;
}

int ImageSequenceProviderFrameMetadata::frameDuration() const
{
    return m_frameDuration;
}

ImageSequenceProviderSession::ImageSequenceProviderSession(QObject *parent)
    : QObject(parent)
{
}

void ImageSequenceProviderSession::requestFrame(const ImageSequenceProviderRequestToken &, int)
{
}

void ImageSequenceProviderSession::requestPlayback(const ImageSequenceProviderRequestToken &token, int frame, int)
{
    requestFrame(token, frame);
}

void ImageSequenceProviderSession::cancelRequest(const ImageSequenceProviderRequestToken &)
{
}

void ImageSequenceProviderSession::close()
{
}
