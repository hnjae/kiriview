// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videoplaybackcontrolruntime.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
QString formattedTimestamp(qint64 milliseconds)
{
    if (milliseconds < 0) {
        return QStringLiteral("--:--");
    }

    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

bool sameProjection(const kiriview::VideoPlaybackControlProjection& left,
    const kiriview::VideoPlaybackControlProjection& right)
{
    return left.sourceRevision == right.sourceRevision && left.ready == right.ready
        && left.presentationMode == right.presentationMode
        && left.reserveSpace == right.reserveSpace && left.shown == right.shown
        && left.autoHideEligible == right.autoHideEligible && left.playing == right.playing
        && left.muted == right.muted && left.timelineKind == right.timelineKind
        && left.timelineInteractive == right.timelineInteractive
        && left.sliderValueMsec == right.sliderValueMsec
        && left.sliderMaximumMsec == right.sliderMaximumMsec
        && left.currentTimeText == right.currentTimeText && left.durationText == right.durationText
        && left.scrubbing == right.scrubbing;
}
}

namespace kiriview {
VideoPlaybackControlRuntime::VideoPlaybackControlRuntime(QObject* owner,
    TimerScheduler timerScheduler, VideoPlaybackControlProjectionCallback projectionCallback)
    : m_owner(owner)
    , m_timerScheduler(timerSchedulerWithDefaults(std::move(timerScheduler)))
    , m_projectionCallback(std::move(projectionCallback))
{
}

const VideoPlaybackControlProjection& VideoPlaybackControlRuntime::projection() const
{
    return m_projection;
}

const VideoPlaybackControlMediaSnapshot& VideoPlaybackControlRuntime::mediaSnapshot() const
{
    return m_media;
}

void VideoPlaybackControlRuntime::replaceSource(quint64 sourceRevision)
{
    stopAutoHideTimer();
    const bool muted = m_media.muted;
    m_sourceRevision = sourceRevision;
    m_media = {};
    m_media.muted = muted;
    m_interactionActive = false;
    m_explicitlyRevealed = true;
    m_scrubbing = false;
    m_scrubPositionMsec = 0;
    publishProjection();
}

void VideoPlaybackControlRuntime::acceptEnvironment(VideoPlaybackControlEnvironment environment)
{
    const VideoPlaybackControlPresentationMode previousMode = presentationMode();
    const int previousDelay = m_environment.autoHideDelayMsec;
    m_environment = environment;
    m_environmentKnown = true;
    if (previousMode != presentationMode()) {
        m_explicitlyRevealed = true;
    }
    if (previousDelay != m_environment.autoHideDelayMsec) {
        stopAutoHideTimer();
        m_autoHideTimer.reset();
        m_timerIntervalMsec = 0;
    }
    publishProjection();
    synchronizeAutoHideTimer();
}

void VideoPlaybackControlRuntime::acceptMediaSnapshot(VideoPlaybackControlMediaSnapshot snapshot)
{
    snapshot.durationMsec = std::max<qint64>(0, snapshot.durationMsec);
    snapshot.positionMsec = std::max<qint64>(0, snapshot.positionMsec);
    if (snapshot.durationMsec > 0) {
        snapshot.positionMsec = std::min(snapshot.positionMsec, snapshot.durationMsec);
    }

    const bool readinessChanged = m_media.ready != snapshot.ready;
    const bool playingChanged = m_media.playing != snapshot.playing;
    m_media = snapshot;
    if (!m_media.ready || timelineKind() != VideoPlaybackTimelineKind::Seekable) {
        m_scrubbing = false;
    }
    if (readinessChanged || playingChanged) {
        m_explicitlyRevealed = true;
    }
    publishProjection();
    synchronizeAutoHideTimer();
}

void VideoPlaybackControlRuntime::setInteractionActive(bool active)
{
    if (m_interactionActive == active) {
        return;
    }
    m_interactionActive = active;
    if (active) {
        m_explicitlyRevealed = true;
    }
    publishProjection();
    synchronizeAutoHideTimer();
}

void VideoPlaybackControlRuntime::reveal()
{
    if (!m_media.ready) {
        return;
    }
    m_explicitlyRevealed = true;
    publishProjection();
    synchronizeAutoHideTimer();
}

void VideoPlaybackControlRuntime::beginScrub()
{
    if (timelineKind() != VideoPlaybackTimelineKind::Seekable) {
        return;
    }
    m_scrubbing = true;
    m_scrubPositionMsec = normalizedPosition(m_media.positionMsec);
    m_explicitlyRevealed = true;
    publishProjection();
    synchronizeAutoHideTimer();
}

void VideoPlaybackControlRuntime::updateScrub(qint64 positionMsec)
{
    if (!m_scrubbing) {
        return;
    }
    m_scrubPositionMsec = normalizedPosition(positionMsec);
    publishProjection();
}

std::optional<qint64> VideoPlaybackControlRuntime::commitScrub()
{
    if (!m_scrubbing || timelineKind() != VideoPlaybackTimelineKind::Seekable) {
        cancelScrub();
        return std::nullopt;
    }
    const qint64 positionMsec = normalizedPosition(m_scrubPositionMsec);
    m_media.positionMsec = positionMsec;
    m_scrubbing = false;
    publishProjection();
    synchronizeAutoHideTimer();
    return positionMsec;
}

void VideoPlaybackControlRuntime::cancelScrub()
{
    if (!m_scrubbing) {
        return;
    }
    m_scrubbing = false;
    publishProjection();
    synchronizeAutoHideTimer();
}

std::optional<qint64> VideoPlaybackControlRuntime::requestSeek(qint64 positionMsec)
{
    if (timelineKind() != VideoPlaybackTimelineKind::Seekable) {
        return std::nullopt;
    }
    m_media.positionMsec = normalizedPosition(positionMsec);
    m_explicitlyRevealed = true;
    publishProjection();
    synchronizeAutoHideTimer();
    return m_media.positionMsec;
}

VideoPlaybackControlProjection VideoPlaybackControlRuntime::projectedState() const
{
    VideoPlaybackControlProjection projection;
    projection.sourceRevision = m_sourceRevision;
    projection.ready = m_media.ready;
    projection.presentationMode = presentationMode();
    projection.reserveSpace = projection.ready
        && projection.presentationMode == VideoPlaybackControlPresentationMode::Fixed;
    projection.autoHideEligible = projection.ready && m_media.playing
        && projection.presentationMode == VideoPlaybackControlPresentationMode::Floating;
    projection.shown = projection.ready
        && (projection.presentationMode == VideoPlaybackControlPresentationMode::Fixed
            || !m_media.playing || m_interactionActive || m_scrubbing || m_explicitlyRevealed);
    projection.playing = m_media.playing;
    projection.muted = m_media.muted;
    projection.timelineKind = timelineKind();
    projection.timelineInteractive = projection.timelineKind == VideoPlaybackTimelineKind::Seekable;
    projection.sliderMaximumMsec
        = projection.timelineInteractive ? m_media.durationMsec : qint64(1);
    projection.sliderValueMsec = projection.timelineInteractive
        ? normalizedPosition(m_scrubbing ? m_scrubPositionMsec : m_media.positionMsec)
        : qint64(0);
    projection.currentTimeText = formattedTimestamp(
        normalizedPosition(m_scrubbing ? m_scrubPositionMsec : m_media.positionMsec));
    projection.durationText = m_media.durationMsec > 0 ? formattedTimestamp(m_media.durationMsec)
                                                       : QStringLiteral("--:--");
    projection.scrubbing = m_scrubbing;
    return projection;
}

VideoPlaybackControlPresentationMode VideoPlaybackControlRuntime::presentationMode() const
{
    const bool validGeometry = m_environmentKnown && std::isfinite(m_environment.viewportWidth)
        && std::isfinite(m_environment.viewportHeight) && std::isfinite(m_environment.gridUnit)
        && m_environment.viewportWidth > 0.0 && m_environment.viewportHeight > 0.0
        && m_environment.gridUnit > 0.0 && m_environment.autoHideDelayMsec > 0;
    if (!validGeometry || m_environment.mobile || m_environment.transientTouchInput
        || m_environment.longAnimationDurationMsec <= 0
        || m_environment.viewportWidth < m_environment.gridUnit * 32.0
        || m_environment.viewportHeight < m_environment.gridUnit * 16.0) {
        return VideoPlaybackControlPresentationMode::Fixed;
    }
    return VideoPlaybackControlPresentationMode::Floating;
}

VideoPlaybackTimelineKind VideoPlaybackControlRuntime::timelineKind() const
{
    if (!m_media.ready || m_media.durationMsec <= 0) {
        return VideoPlaybackTimelineKind::Unavailable;
    }
    return m_media.seekable ? VideoPlaybackTimelineKind::Seekable
                            : VideoPlaybackTimelineKind::NonSeekable;
}

qint64 VideoPlaybackControlRuntime::normalizedPosition(qint64 positionMsec) const
{
    const qint64 nonNegativePosition = std::max<qint64>(0, positionMsec);
    return m_media.durationMsec > 0 ? std::min(nonNegativePosition, m_media.durationMsec)
                                    : nonNegativePosition;
}

void VideoPlaybackControlRuntime::publishProjection()
{
    VideoPlaybackControlProjection projection = projectedState();
    if (sameProjection(m_projection, projection)) {
        return;
    }
    projection.revision = m_nextProjectionRevision++;
    m_projection = std::move(projection);
    if (m_projectionCallback) {
        m_projectionCallback(m_projection);
    }
}

void VideoPlaybackControlRuntime::synchronizeAutoHideTimer()
{
    if (!m_projection.autoHideEligible || m_interactionActive || m_scrubbing
        || !m_explicitlyRevealed) {
        stopAutoHideTimer();
        return;
    }
    ensureAutoHideTimer();
    if (m_autoHideTimer != nullptr) {
        m_autoHideTimer->start(TimerDuration(m_environment.autoHideDelayMsec));
    }
}

void VideoPlaybackControlRuntime::stopAutoHideTimer()
{
    if (m_autoHideTimer != nullptr) {
        m_autoHideTimer->stop();
    }
}

void VideoPlaybackControlRuntime::ensureAutoHideTimer()
{
    if (m_autoHideTimer != nullptr && m_timerIntervalMsec == m_environment.autoHideDelayMsec) {
        return;
    }
    stopAutoHideTimer();
    m_timerIntervalMsec = m_environment.autoHideDelayMsec;
    m_autoHideTimer = m_timerScheduler.singleShotTimer(
        m_owner, TimerDuration(m_timerIntervalMsec), [this]() { handleAutoHideTimer(); });
}

void VideoPlaybackControlRuntime::handleAutoHideTimer()
{
    if (!m_projection.autoHideEligible || m_interactionActive || m_scrubbing) {
        return;
    }
    m_explicitlyRevealed = false;
    publishProjection();
}
}
