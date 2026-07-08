#include "imageviewportlimits_p.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace ImageViewportInternal;

namespace {
bool isPositiveFiniteValue(double value) { return std::isfinite(value) && value > 0.0; }

bool isPositiveFiniteSize(QSizeF size)
{
    return isPositiveFiniteValue(size.width()) && isPositiveFiniteValue(size.height());
}

bool isValidRole(ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary || role == ImageViewport::PageRole::Secondary;
}

bool isValidUnsupportedCause(ImageSequenceProviderUnsupportedCause cause)
{
    return cause == ImageSequenceProviderUnsupportedCause::UnsupportedRequest
        || cause == ImageSequenceProviderUnsupportedCause::PayloadRejection;
}
}

ImageSequenceProviderAdapter::ImageSequenceProviderAdapter(QObject* parent)
    : QObject(parent)
{
}

ImageSequenceProviderRequestToken::ImageSequenceProviderRequestToken(quint64 id)
    : m_id(id)
{
}

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

bool ImageSequenceProviderMetadata::isSpecified() const { return m_kind != Kind::Invalid; }

bool ImageSequenceProviderMetadata::isStill() const { return m_kind == Kind::Still; }

bool ImageSequenceProviderMetadata::isTimedFrameList() const
{
    return m_kind == Kind::FixedDurationFrames || m_kind == Kind::TimedFrameList;
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

bool ImageSequenceProviderFrameMetadata::isStillFrame() const { return m_kind == Kind::Still; }

bool ImageSequenceProviderFrameMetadata::isTimedFrame() const { return m_kind == Kind::TimedFrame; }

int ImageSequenceProviderFrameMetadata::frame() const { return m_frame; }

int ImageSequenceProviderFrameMetadata::frameStartPosition() const { return m_frameStartPosition; }

int ImageSequenceProviderFrameMetadata::frameDuration() const { return m_frameDuration; }

ImageSequenceProviderSession::ImageSequenceProviderSession(QObject* parent)
    : QObject(parent)
{
}

bool ImageSequenceProviderFrameEnvelope::isValid() const
{
    if (!isPositiveFiniteSize(m_sourceLogicalSize) || !isPositiveFiniteSize(m_payloadRasterSize)
        || !isPositiveFiniteSize(m_sourceToPayloadScale) || m_payloadByteSize <= 0) {
        return false;
    }
    if (m_quality == ImageViewport::PayloadQuality::Unknown
        || m_exactness == ImageViewport::PayloadExactness::Unknown) {
        return false;
    }
    const QSizeF mapped(m_sourceLogicalSize.width() * m_sourceToPayloadScale.width(),
        m_sourceLogicalSize.height() * m_sourceToPayloadScale.height());
    if (std::abs(mapped.width() - m_payloadRasterSize.width()) > 0.01
        || std::abs(mapped.height() - m_payloadRasterSize.height()) > 0.01) {
        return false;
    }
    if (m_frame < 0) {
        return false;
    }
    if (m_frameStartPosition < 0 || m_frameDuration < 0) {
        return m_frame == 0 && m_frameStartPosition == -1 && m_frameDuration == -1;
    }
    return m_frameDuration > 0;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::metadata(
    ImageSequenceProviderRequestToken token)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Metadata;
    request.m_token = token;
    return request;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::frame(
    ImageSequenceProviderRequestToken token, ImageViewport::PageRole role, int frame,
    ImageSequenceProviderDisplayDemand demand)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Frame;
    request.m_token = token;
    request.m_role = role;
    request.m_frame = frame;
    request.m_resolvedFrame = frame;
    request.m_requestedPosition = -1;
    request.m_demand = std::move(demand);
    return request;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::position(
    ImageSequenceProviderRequestToken token, ImageViewport::PageRole role, int requestedPosition,
    int resolvedFrame, ImageSequenceProviderDisplayDemand demand)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Position;
    request.m_token = token;
    request.m_role = role;
    request.m_frame = resolvedFrame;
    request.m_resolvedFrame = resolvedFrame;
    request.m_requestedPosition = requestedPosition;
    request.m_demand = std::move(demand);
    return request;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::playback(
    ImageSequenceProviderRequestToken token, ImageViewport::PageRole role, int frame, int position,
    ImageSequenceProviderDisplayDemand demand)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Playback;
    request.m_token = token;
    request.m_role = role;
    request.m_frame = frame;
    request.m_resolvedFrame = frame;
    request.m_requestedPosition = position;
    request.m_demand = std::move(demand);
    return request;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::cancel(
    QVector<ImageSequenceProviderRequestToken> tokens)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Cancel;
    request.m_tokens = std::move(tokens);
    return request;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::close()
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Close;
    return request;
}

bool ImageSequenceProviderRequest::isValid() const
{
    switch (m_kind) {
    case ImageSequenceProviderRequestKind::Metadata:
        return m_token.isValid();
    case ImageSequenceProviderRequestKind::Frame:
        return m_token.isValid() && isValidRole(m_role) && m_frame >= 0;
    case ImageSequenceProviderRequestKind::Position:
        return m_token.isValid() && isValidRole(m_role) && m_resolvedFrame >= 0
            && m_requestedPosition >= 0;
    case ImageSequenceProviderRequestKind::Playback:
        return m_token.isValid() && isValidRole(m_role) && m_frame >= 0 && m_requestedPosition >= 0;
    case ImageSequenceProviderRequestKind::Cancel:
        if (m_tokens.isEmpty()) {
            return false;
        }
        return std::all_of(m_tokens.cbegin(), m_tokens.cend(),
            [](ImageSequenceProviderRequestToken token) { return token.isValid(); });
    case ImageSequenceProviderRequestKind::Close:
        return true;
    }
    return false;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::metadataReady(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderMetadata metadata)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::MetadataReady;
    event.m_token = token;
    event.m_metadata = std::move(metadata);
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::frameReady(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frameHandle,
    ImageSequenceProviderFrameEnvelope frameEnvelope)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::FrameReady;
    event.m_token = token;
    event.m_frameHandle = frameHandle;
    event.m_frameEnvelope = std::move(frameEnvelope);
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::waiting(
    ImageSequenceProviderRequestToken token)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Waiting;
    event.m_token = token;
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::progress(
    ImageSequenceProviderRequestToken token, double progress)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Progress;
    event.m_token = token;
    event.m_progress = progress;
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::endOfSequence(
    ImageSequenceProviderRequestToken token)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::EndOfSequence;
    event.m_token = token;
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::unsupported(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderUnsupportedCause cause,
    QString diagnostic)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Unsupported;
    event.m_token = token;
    event.m_unsupportedCause = cause;
    event.m_diagnostic = std::move(diagnostic);
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::cancelled(
    ImageSequenceProviderRequestToken token, QString diagnostic)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Cancelled;
    event.m_token = token;
    event.m_diagnostic = std::move(diagnostic);
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::failed(
    ImageSequenceProviderRequestToken token, QString diagnostic)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Failed;
    event.m_token = token;
    event.m_diagnostic = std::move(diagnostic);
    return event;
}

bool ImageSequenceProviderEvent::isValid() const
{
    switch (m_kind) {
    case ImageSequenceProviderEventKind::MetadataReady:
        return m_token.isValid() && m_metadata.isValid();
    case ImageSequenceProviderEventKind::FrameReady:
        return m_token.isValid() && m_frameHandle && m_frameEnvelope.isValid();
    case ImageSequenceProviderEventKind::Waiting:
    case ImageSequenceProviderEventKind::EndOfSequence:
    case ImageSequenceProviderEventKind::Cancelled:
    case ImageSequenceProviderEventKind::Failed:
        return m_token.isValid();
    case ImageSequenceProviderEventKind::Progress:
        return m_token.isValid() && std::isfinite(m_progress) && m_progress >= 0.0
            && m_progress <= 1.0;
    case ImageSequenceProviderEventKind::Unsupported:
        return m_token.isValid() && isValidUnsupportedCause(m_unsupportedCause);
    }
    return false;
}
