// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportlimits_p.h"
#include "imageviewportproviderfacts_p.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>

using namespace ImageViewportInternal;

namespace {
bool isPositiveFiniteValue(double value) { return std::isfinite(value) && value > 0.0; }

bool isPositiveFiniteSize(QSizeF size)
{
    return isPositiveFiniteValue(size.width()) && isPositiveFiniteValue(size.height());
}

bool isValidRole(ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary || role == ImageViewportPageRole::Secondary;
}

bool isValidUnsupportedCause(ImageSequenceProviderUnsupportedCause cause)
{
    return cause == ImageSequenceProviderUnsupportedCause::UnsupportedRequest
        || cause == ImageSequenceProviderUnsupportedCause::PayloadRejection;
}

bool isValidFailureCause(ImageSequenceProviderFailureCause cause)
{
    switch (cause) {
    case ImageSequenceProviderFailureCause::SourceAccess:
    case ImageSequenceProviderFailureCause::Decode:
    case ImageSequenceProviderFailureCause::ResourceExhausted:
    case ImageSequenceProviderFailureCause::ProviderInternal:
        return true;
    case ImageSequenceProviderFailureCause::Unavailable:
        return false;
    }
    return false;
}

quint64 allocateFailureReferenceValue()
{
    static std::atomic<quint64> nextReference { 1 };
    quint64 value = nextReference.fetch_add(1, std::memory_order_relaxed);
    if (value == 0) {
        value = nextReference.fetch_add(1, std::memory_order_relaxed);
    }
    return value;
}
}

ImageSequenceProviderAdapter::ImageSequenceProviderAdapter(QObject* parent)
    : QObject(parent)
{
}

ImageSequenceProviderFailureHandle::ImageSequenceProviderFailureHandle(
    ReleaseCallback releaseFailure, QObject* parent)
    : QObject(parent)
    , m_reference(allocateFailureReferenceValue())
    , m_releaseFailure(std::move(releaseFailure))
{
}

ImageSequenceProviderFailureHandle::~ImageSequenceProviderFailureHandle() { release(); }

bool ImageSequenceProviderFailureHandle::isValid() const
{
    return m_reference.isValid() && bool(m_releaseFailure);
}

void ImageSequenceProviderFailureHandle::release()
{
    if (m_released) {
        return;
    }
    m_released = true;
    if (m_releaseFailure) {
        m_releaseFailure();
    }
}

bool ImageSequenceProviderFailure::isValid() const
{
    return isValidFailureCause(m_cause)
        && (!m_applicationFailureHandle || m_applicationFailureHandle->isValid());
}

ImageSequenceProviderSessionFactoryResult ImageSequenceProviderSessionFactoryResult::created(
    ImageSequenceProviderSession* session)
{
    ImageSequenceProviderSessionFactoryResult result;
    result.m_outcome = ImageSequenceProviderSessionFactoryOutcome::Created;
    result.m_session = session;
    return result;
}

ImageSequenceProviderSessionFactoryResult ImageSequenceProviderSessionFactoryResult::failed(
    ImageSequenceProviderFailure failure)
{
    ImageSequenceProviderSessionFactoryResult result;
    result.m_outcome = ImageSequenceProviderSessionFactoryOutcome::Failed;
    result.m_failure = std::move(failure);
    return result;
}

ImageSequenceProviderSessionFactoryOutcome
ImageSequenceProviderSessionFactoryResult::outcome() const
{
    return m_outcome;
}

ImageSequenceProviderSession* ImageSequenceProviderSessionFactoryResult::session() const
{
    return m_session;
}

ImageSequenceProviderDescriptor::ImageSequenceProviderDescriptor(
    ImageSequenceProviderMetadata constructionMetadata,
    ImageSequenceProviderThreadingContract threadingContract, SessionFactory sessionFactory)
    : m_constructionMetadata(std::move(constructionMetadata))
    , m_threadingContract(threadingContract)
    , m_sessionFactory(std::move(sessionFactory))
{
}

bool ImageSequenceProviderDescriptor::isValid() const
{
    if (!m_sessionFactory) {
        return false;
    }
    switch (m_threadingContract) {
    case ImageSequenceProviderThreadingContract::AffinityBound:
    case ImageSequenceProviderThreadingContract::ThreadSafe:
        return true;
    }
    return false;
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
            = std::min(frameCount, ImageSequenceLimits::maximumFrameCount() + 1);
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
            = std::min(frameCount, ImageSequenceLimits::maximumFrameCount() + 1);
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

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::withSourceLogicalSize(
    QSizeF sourceLogicalSize)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_logicalSize = sourceLogicalSize;
    return metadata;
}

ImageSequenceProviderMetadata ImageSequenceProviderMetadata::timedFrameCount(
    QSizeF logicalSize, int frameCount)
{
    ImageSequenceProviderMetadata metadata;
    metadata.m_logicalSize = logicalSize;
    metadata.m_constructionFrameCount = frameCount;
    return metadata;
}

bool ImageSequenceProviderMetadata::isValid() const
{
    if (!isSpecified()) {
        return false;
    }
    if (m_hasAuthoredAnimationFacts && !m_authoredAnimationFacts.isValid()) {
        return false;
    }
    if (!hasCompleteModel()) {
        if (m_logicalSize.isEmpty()) {
            return m_constructionFrameCount < 0;
        }
        return isPositiveFiniteInteger(m_logicalSize.width())
            && isPositiveFiniteInteger(m_logicalSize.height())
            && (m_constructionFrameCount == -1 || m_constructionFrameCount > 0);
    }
    if (!isPositiveFiniteInteger(m_logicalSize.width())
        || !isPositiveFiniteInteger(m_logicalSize.height())) {
        return false;
    }
    if (isStill()) {
        return timedPlaybackSupport() != ImageViewportCapabilitySupport::True
            && positionSeekSupport() != ImageViewportCapabilitySupport::True;
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
    return hasCompleteModel() || !m_logicalSize.isEmpty() || m_constructionFrameCount >= 0
        || m_timedPlaybackSupport.has_value() || m_frameSeekSupport.has_value()
        || m_positionSeekSupport.has_value() || m_hasAuthoredAnimationFacts;
}

bool ImageSequenceProviderMetadata::hasCompleteModel() const { return m_kind != Kind::Invalid; }

bool ImageSequenceProviderMetadata::isStill() const { return m_kind == Kind::Still; }

bool ImageSequenceProviderMetadata::isTimedFrameList() const
{
    return m_kind == Kind::FixedDurationFrames || m_kind == Kind::TimedFrameList;
}

QSizeF ImageSequenceProviderMetadata::sourceLogicalSize() const { return m_logicalSize; }

int ImageSequenceProviderMetadata::frameCount() const
{
    if (hasCompleteModel()) {
        return isStill() ? 1 : m_frameDurations.size();
    }
    return m_constructionFrameCount;
}

int ImageSequenceProviderMetadata::totalDuration() const
{
    if (!hasCompleteModel() || !isTimedFrameList()) {
        return -1;
    }
    qint64 total = 0;
    for (int duration : m_frameDurations) {
        total += duration;
    }
    return total <= std::numeric_limits<int>::max() ? static_cast<int>(total) : -1;
}

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

void ImageSequenceProviderMetadata::setTimedPlaybackSupport(ImageViewportCapabilitySupport support)
{
    if (support == ImageViewportCapabilitySupport::Unavailable) {
        m_timedPlaybackSupport.reset();
    } else {
        m_timedPlaybackSupport = support == ImageViewportCapabilitySupport::True;
    }
}

void ImageSequenceProviderMetadata::setFrameSeekSupport(ImageViewportCapabilitySupport support)
{
    if (support == ImageViewportCapabilitySupport::Unavailable) {
        m_frameSeekSupport.reset();
    } else {
        m_frameSeekSupport = support == ImageViewportCapabilitySupport::True;
    }
}

void ImageSequenceProviderMetadata::setPositionSeekSupport(ImageViewportCapabilitySupport support)
{
    if (support == ImageViewportCapabilitySupport::Unavailable) {
        m_positionSeekSupport.reset();
    } else {
        m_positionSeekSupport = support == ImageViewportCapabilitySupport::True;
    }
}

ImageViewportCapabilitySupport ImageSequenceProviderMetadata::timedPlaybackSupport() const
{
    if (m_timedPlaybackSupport.has_value()) {
        return *m_timedPlaybackSupport ? ImageViewportCapabilitySupport::True
                                       : ImageViewportCapabilitySupport::False;
    }
    return hasCompleteModel() ? (isTimedFrameList() ? ImageViewportCapabilitySupport::True
                                                    : ImageViewportCapabilitySupport::False)
                              : ImageViewportCapabilitySupport::Unavailable;
}

ImageViewportCapabilitySupport ImageSequenceProviderMetadata::frameSeekSupport() const
{
    if (m_frameSeekSupport.has_value()) {
        return *m_frameSeekSupport ? ImageViewportCapabilitySupport::True
                                   : ImageViewportCapabilitySupport::False;
    }
    return hasCompleteModel() ? ImageViewportCapabilitySupport::True
                              : ImageViewportCapabilitySupport::Unavailable;
}

ImageViewportCapabilitySupport ImageSequenceProviderMetadata::positionSeekSupport() const
{
    if (m_positionSeekSupport.has_value()) {
        return *m_positionSeekSupport ? ImageViewportCapabilitySupport::True
                                      : ImageViewportCapabilitySupport::False;
    }
    return hasCompleteModel() ? (isTimedFrameList() ? ImageViewportCapabilitySupport::True
                                                    : ImageViewportCapabilitySupport::False)
                              : ImageViewportCapabilitySupport::Unavailable;
}

ImageViewportCapabilitySupport ImageSequenceProviderMetadata::autoplay() const
{
    if (!m_hasAuthoredAnimationFacts) {
        return ImageViewportCapabilitySupport::Unavailable;
    }
    return m_authoredAnimationFacts.autoplay() ? ImageViewportCapabilitySupport::True
                                               : ImageViewportCapabilitySupport::False;
}

ImageSequenceAuthoredAnimationLoopMode ImageSequenceProviderMetadata::authoredLoopMode() const
{
    return m_hasAuthoredAnimationFacts ? m_authoredAnimationFacts.loopMode()
                                       : ImageSequenceAuthoredAnimationLoopMode::Unavailable;
}

int ImageSequenceProviderMetadata::authoredLoopCount() const
{
    return m_hasAuthoredAnimationFacts ? m_authoredAnimationFacts.loopCount() : -1;
}

ImageViewportRange ImageSequenceProviderMetadata::frameSeekBounds() const
{
    if (frameSeekSupport() != ImageViewportCapabilitySupport::True || frameCount() <= 0) {
        return {};
    }
    return { 0, frameCount() - 1 };
}

ImageViewportRange ImageSequenceProviderMetadata::positionSeekBounds() const
{
    if (positionSeekSupport() != ImageViewportCapabilitySupport::True || totalDuration() < 0) {
        return {};
    }
    return { 0, totalDuration() };
}

ImageSequenceProviderFrameEnvelope ImageSequenceProviderFrameEnvelope::stillFrame()
{
    ImageSequenceProviderFrameEnvelope envelope;
    envelope.m_frame = 0;
    return envelope;
}

ImageSequenceProviderFrameEnvelope ImageSequenceProviderFrameEnvelope::timedFrame(
    int frame, int frameStartPosition, int frameDuration)
{
    ImageSequenceProviderFrameEnvelope envelope;
    envelope.m_frame = frame;
    envelope.m_frameStartPosition = frameStartPosition;
    envelope.m_frameDuration = frameDuration;
    return envelope;
}

ImageSequenceProviderSession::ImageSequenceProviderSession(QObject* parent)
    : QObject(parent)
{
}

bool ImageSequenceProviderFrameEnvelope::isValid() const
{
    if (m_frame < 0) {
        return false;
    }
    if (m_frameStartPosition < 0 || m_frameDuration < 0) {
        return m_frame == 0 && m_frameStartPosition == -1 && m_frameDuration == -1;
    }
    return m_frameDuration > 0;
}

bool ImageSequenceProviderFrameEnvelope::isStillFrame() const
{
    return m_frame == 0 && m_frameStartPosition == -1 && m_frameDuration == -1;
}

bool ImageSequenceProviderFrameEnvelope::isTimedFrame() const
{
    return m_frame >= 0 && m_frameStartPosition >= 0;
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
    ImageSequenceProviderRequestToken token, ImageViewportPageRole role, int frame,
    ImageSequenceProviderDisplayDemand demand)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Frame;
    request.m_token = token;
    request.m_role = role;
    request.m_frame = frame;
    request.m_resolvedFrame = frame;
    request.m_requestedPosition = -1;
    request.m_demand = demand;
    return request;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::position(
    ImageSequenceProviderRequestToken token, ImageViewportPageRole role, int requestedPosition,
    int resolvedFrame, ImageSequenceProviderDisplayDemand demand)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Position;
    request.m_token = token;
    request.m_role = role;
    request.m_frame = resolvedFrame;
    request.m_resolvedFrame = resolvedFrame;
    request.m_requestedPosition = requestedPosition;
    request.m_demand = demand;
    return request;
}

ImageSequenceProviderRequest ImageSequenceProviderRequest::playback(
    ImageSequenceProviderRequestToken token, ImageViewportPageRole role, int frame, int position,
    ImageSequenceProviderDisplayDemand demand)
{
    ImageSequenceProviderRequest request;
    request.m_kind = ImageSequenceProviderRequestKind::Playback;
    request.m_token = token;
    request.m_role = role;
    request.m_frame = frame;
    request.m_resolvedFrame = frame;
    request.m_requestedPosition = position;
    request.m_demand = demand;
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
    event.m_frameEnvelope = frameEnvelope;
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
    ImageSequenceProviderRequestToken token, ImageSequenceProviderUnsupportedCause cause)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Unsupported;
    event.m_token = token;
    event.m_unsupportedCause = cause;
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::cancelled(
    ImageSequenceProviderRequestToken token)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Cancelled;
    event.m_token = token;
    return event;
}

ImageSequenceProviderEvent ImageSequenceProviderEvent::failed(
    ImageSequenceProviderRequestToken token, ImageSequenceProviderFailure failure)
{
    ImageSequenceProviderEvent event;
    event.m_kind = ImageSequenceProviderEventKind::Failed;
    event.m_token = token;
    event.m_failure = std::move(failure);
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
        return m_token.isValid();
    case ImageSequenceProviderEventKind::Failed:
        return m_token.isValid() && m_failure.isValid();
    case ImageSequenceProviderEventKind::Progress:
        return m_token.isValid() && std::isfinite(m_progress) && m_progress >= 0.0
            && m_progress <= 1.0;
    case ImageSequenceProviderEventKind::Unsupported:
        return m_token.isValid() && isValidUnsupportedCause(m_unsupportedCause);
    }
    return false;
}
