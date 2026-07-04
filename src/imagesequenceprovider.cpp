#include "imageviewportlimits_p.h"

#include <algorithm>
#include <utility>

using namespace ImageViewportInternal;

ImageSequenceProviderAdapter::ImageSequenceProviderAdapter(QObject* parent)
    : QObject(parent)
{
}

std::shared_ptr<ImageSequenceProviderSessionFactory>
ImageSequenceProviderAdapter::sessionFactory() const
{
    return {};
}

ImageSequenceProviderMetadata ImageSequenceProviderAdapter::knownMetadata() const { return {}; }

ImageSequenceProviderKnownFacts ImageSequenceProviderAdapter::knownFacts() const
{
    const ImageSequenceProviderMetadata metadata = knownMetadata();
    if (!metadata.isSpecified()) {
        return {};
    }
    if (metadata.isStill()) {
        return ImageSequenceProviderKnownFacts::still(metadata.logicalSize());
    }
    if (metadata.isTimedFrameList()) {
        return ImageSequenceProviderKnownFacts::timedFrameList(
            metadata.logicalSize(), metadata.frameDurations());
    }
    return {};
}

ImageSequenceProviderAdapter::CapabilitySupport
ImageSequenceProviderAdapter::timedPlaybackCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderAdapter::CapabilitySupport
ImageSequenceProviderAdapter::frameSeekCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceProviderAdapter::CapabilitySupport
ImageSequenceProviderAdapter::positionSeekCapability() const
{
    return CapabilitySupport::Unavailable;
}

ImageSequenceAuthoredAnimationFacts ImageSequenceProviderAdapter::authoredAnimationFacts() const
{
    return {};
}

ImageSequenceProviderThreadingContract ImageSequenceProviderAdapter::threadingContract() const
{
    return ImageSequenceProviderThreadingContract::AffinityBound;
}

ImageSequenceProviderRequestToken::ImageSequenceProviderRequestToken(quint64 id)
    : m_id(id)
{
}

quint64 ImageSequenceProviderRequestToken::id() const { return m_id; }

bool ImageSequenceProviderRequestToken::isValid() const { return m_id != 0; }

ImageSequenceProviderKnownFacts ImageSequenceProviderKnownFacts::logicalSize(QSizeF logicalSize)
{
    ImageSequenceProviderKnownFacts facts;
    facts.m_kind = Kind::LogicalSize;
    facts.m_logicalSize = logicalSize;
    return facts;
}

ImageSequenceProviderKnownFacts ImageSequenceProviderKnownFacts::still(QSizeF logicalSize)
{
    ImageSequenceProviderKnownFacts facts;
    facts.m_kind = Kind::Still;
    facts.m_logicalSize = logicalSize;
    facts.m_frameCount = 1;
    return facts;
}

ImageSequenceProviderKnownFacts ImageSequenceProviderKnownFacts::timedFrameCount(
    QSizeF logicalSize, int frameCount)
{
    ImageSequenceProviderKnownFacts facts;
    facts.m_kind = Kind::TimedFrameCount;
    facts.m_logicalSize = logicalSize;
    facts.m_frameCount = frameCount;
    return facts;
}

ImageSequenceProviderKnownFacts ImageSequenceProviderKnownFacts::fixedDurationFrames(
    QSizeF logicalSize, int frameCount, int frameDuration)
{
    ImageSequenceProviderKnownFacts facts;
    facts.m_kind = Kind::TimedFrameList;
    facts.m_logicalSize = logicalSize;
    facts.m_frameCount = frameCount;
    if (frameCount > 0) {
        const int retainedFrameCount
            = std::min(frameCount, ImageSequenceLimits::maximumTimedListFrameCount() + 1);
        facts.m_frameDurations = QVector<int>(retainedFrameCount, frameDuration);
    }
    return facts;
}

ImageSequenceProviderKnownFacts ImageSequenceProviderKnownFacts::timedFrameList(
    QSizeF logicalSize, QVector<int> frameDurations)
{
    ImageSequenceProviderKnownFacts facts;
    facts.m_kind = Kind::TimedFrameList;
    facts.m_logicalSize = logicalSize;
    facts.m_frameCount = frameDurations.size();
    facts.m_frameDurations = std::move(frameDurations);
    return facts;
}

bool ImageSequenceProviderKnownFacts::isSpecified() const { return m_kind != Kind::Unknown; }

bool ImageSequenceProviderKnownFacts::isValid() const
{
    if (!isSpecified()) {
        return false;
    }
    if (!isPositiveFiniteInteger(m_logicalSize.width())
        || !isPositiveFiniteInteger(m_logicalSize.height())) {
        return false;
    }
    if (isLogicalSizeOnly()) {
        return true;
    }
    if (isStill()) {
        return m_frameCount == 1 && m_frameDurations.isEmpty();
    }
    if (isTimedFrameCount()) {
        return m_frameCount > 0 && m_frameDurations.isEmpty();
    }
    if (isTimedFrameList()) {
        if (m_frameCount <= 0 || m_frameDurations.isEmpty()
            || m_frameDurations.size() != m_frameCount) {
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

bool ImageSequenceProviderKnownFacts::isComplete() const { return isStill() || isTimedFrameList(); }

bool ImageSequenceProviderKnownFacts::isLogicalSizeOnly() const
{
    return m_kind == Kind::LogicalSize;
}

bool ImageSequenceProviderKnownFacts::isStill() const { return m_kind == Kind::Still; }

bool ImageSequenceProviderKnownFacts::isTimedFrameCount() const
{
    return m_kind == Kind::TimedFrameCount;
}

bool ImageSequenceProviderKnownFacts::isTimedFrameList() const
{
    return m_kind == Kind::TimedFrameList;
}

QSizeF ImageSequenceProviderKnownFacts::logicalSize() const { return m_logicalSize; }

int ImageSequenceProviderKnownFacts::frameCount() const
{
    if (isStill()) {
        return 1;
    }
    if (isTimedFrameCount() || isTimedFrameList()) {
        return m_frameCount;
    }
    return -1;
}

QVector<int> ImageSequenceProviderKnownFacts::frameDurations() const { return m_frameDurations; }

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::still(QSizeF logicalSize)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_kind = Kind::Still;
    metadata.m_logicalSize = logicalSize;
    return metadata;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::fixedDurationFrames(
    QSizeF logicalSize, int frameCount, int frameDuration)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_kind = Kind::FixedDurationFrames;
    metadata.m_logicalSize = logicalSize;
    if (frameCount > 0) {
        const int retainedFrameCount
            = std::min(frameCount, ImageSequenceLimits::maximumTimedListFrameCount() + 1);
        metadata.m_frameDurations = QVector<int>(retainedFrameCount, frameDuration);
    }
    return metadata;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::timedFrameList(
    QSizeF logicalSize, QVector<int> frameDurations)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_kind = Kind::TimedFrameList;
    metadata.m_logicalSize = logicalSize;
    metadata.m_frameDurations = std::move(frameDurations);
    return metadata;
}

bool ImageSequenceProviderMetadata::isValid() const
{
    if (!isPositiveFiniteInteger(m_logicalSize.width())
        || !isPositiveFiniteInteger(m_logicalSize.height())) {
        return false;
    }
    if (isStill()) {
        return !timedPlaybackSupport() && !positionSeekSupport();
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
    return m_kind != Kind::Invalid;
}

bool ImageSequenceProviderMetadata::isStill() const { return m_kind == Kind::Still; }

bool ImageSequenceProviderMetadata::isTimedFrameList() const
{
    return m_kind == Kind::FixedDurationFrames
        || m_kind == Kind::TimedFrameList;
}

QSizeF ImageSequenceProviderMetadata::logicalSize() const { return m_logicalSize; }

QVector<int> ImageSequenceProviderMetadata::frameDurations() const { return m_frameDurations; }

bool ImageSequenceProviderMetadata::hasAuthoredAnimationFacts() const
{
    return m_hasAuthoredAnimationFacts;
}

ImageSequenceAuthoredAnimationFacts ImageSequenceProviderMetadata::authoredAnimationFacts() const
{
    return m_authoredAnimationFacts;
}

void ImageSequenceProviderMetadata::setAuthoredAnimationFacts(
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
{
    m_hasAuthoredAnimationFacts = true;
    m_authoredAnimationFacts = authoredAnimationFacts;
}

void ImageSequenceProviderMetadata::setTimedPlaybackSupport(bool supported)
{
    m_timedPlaybackSupport = supported;
}

void ImageSequenceProviderMetadata::setFrameSeekSupport(bool supported)
{
    m_frameSeekSupport = supported;
}

void ImageSequenceProviderMetadata::setPositionSeekSupport(bool supported)
{
    m_positionSeekSupport = supported;
}

bool ImageSequenceProviderMetadata::timedPlaybackSupport() const
{
    return m_timedPlaybackSupport.value_or(isTimedFrameList());
}

bool ImageSequenceProviderMetadata::frameSeekSupport() const
{
    return m_frameSeekSupport.value_or(isStill() || isTimedFrameList());
}

bool ImageSequenceProviderMetadata::positionSeekSupport() const
{
    return m_positionSeekSupport.value_or(isTimedFrameList());
}

ImageSequenceProviderFrameMetadata ImageSequenceProviderFrameMetadata::stillFrame()
{
    ImageSequenceProviderFrameMetadata metadata;
    metadata.m_kind = Kind::Still;
    metadata.m_frame = 0;
    return metadata;
}

ImageSequenceProviderFrameMetadata ImageSequenceProviderFrameMetadata::timedFrame(
    int frame, int frameStartPosition, int frameDuration)
{
    ImageSequenceProviderFrameMetadata metadata;
    metadata.m_kind = Kind::TimedFrame;
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
        return m_frame >= 0 && m_frameStartPosition >= 0
            && (m_frameDuration == -1 || m_frameDuration > 0);
    }

    return false;
}

bool ImageSequenceProviderFrameMetadata::isStillFrame() const
{
    return m_kind == Kind::Still;
}

bool ImageSequenceProviderFrameMetadata::isTimedFrame() const
{
    return m_kind == Kind::TimedFrame;
}

int ImageSequenceProviderFrameMetadata::frame() const { return m_frame; }

int ImageSequenceProviderFrameMetadata::frameStartPosition() const { return m_frameStartPosition; }

int ImageSequenceProviderFrameMetadata::frameDuration() const { return m_frameDuration; }

ImageSequenceProviderSession::ImageSequenceProviderSession(QObject* parent)
    : QObject(parent)
{
}

void ImageSequenceProviderSession::requestFrame(ImageSequenceProviderRequestToken, int) { }

void ImageSequenceProviderSession::requestPosition(
    ImageSequenceProviderRequestToken token, int resolvedFrame, int)
{
    requestFrame(token, resolvedFrame);
}

void ImageSequenceProviderSession::requestPlayback(
    ImageSequenceProviderRequestToken token, int frame, int)
{
    requestFrame(token, frame);
}

void ImageSequenceProviderSession::cancelRequest(ImageSequenceProviderRequestToken) { }

void ImageSequenceProviderSession::close() { }
