// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEOPLAYBACKCONTROLRUNTIME_H
#define KIRIVIEW_VIDEOPLAYBACKCONTROLRUNTIME_H

#include "async/timerscheduler.h"

#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
enum class VideoPlaybackControlPresentationMode {
    Fixed,
    Floating,
};

enum class VideoPlaybackTimelineKind {
    Unavailable,
    NonSeekable,
    Seekable,
};

struct VideoPlaybackControlEnvironment
{
    qreal viewportWidth = 0.0;
    qreal viewportHeight = 0.0;
    qreal gridUnit = 0.0;
    bool mobile = false;
    bool transientTouchInput = false;
    int longAnimationDurationMsec = 0;
    int autoHideDelayMsec = 0;
};

struct VideoPlaybackControlMediaSnapshot
{
    bool ready = false;
    qint64 durationMsec = 0;
    qint64 positionMsec = 0;
    bool playing = false;
    bool seekable = false;
    bool muted = false;
    bool mediaEnded = false;
};

struct VideoPlaybackControlProjection
{
    quint64 revision = 0;
    quint64 sourceRevision = 0;
    bool ready = false;
    VideoPlaybackControlPresentationMode presentationMode
        = VideoPlaybackControlPresentationMode::Fixed;
    bool reserveSpace = false;
    bool shown = false;
    bool autoHideEligible = false;
    bool playing = false;
    bool muted = false;
    VideoPlaybackTimelineKind timelineKind = VideoPlaybackTimelineKind::Unavailable;
    bool timelineInteractive = false;
    qint64 sliderValueMsec = 0;
    qint64 sliderMaximumMsec = 1;
    QString currentTimeText = QStringLiteral("--:--");
    QString durationText = QStringLiteral("--:--");
    bool scrubbing = false;
};

using VideoPlaybackControlProjectionCallback
    = std::function<void(const VideoPlaybackControlProjection&)>;

class VideoPlaybackControlRuntime final
{
public:
    explicit VideoPlaybackControlRuntime(QObject* owner, TimerScheduler timerScheduler = {},
        VideoPlaybackControlProjectionCallback projectionCallback = {});

    [[nodiscard]] const VideoPlaybackControlProjection& projection() const;
    [[nodiscard]] const VideoPlaybackControlMediaSnapshot& mediaSnapshot() const;

    void replaceSource(quint64 sourceRevision);
    void acceptEnvironment(VideoPlaybackControlEnvironment environment);
    void acceptMediaSnapshot(VideoPlaybackControlMediaSnapshot snapshot);
    void setInteractionActive(bool active);
    void reveal();
    void beginScrub();
    void updateScrub(qint64 positionMsec);
    std::optional<qint64> commitScrub();
    void cancelScrub();
    std::optional<qint64> requestSeek(qint64 positionMsec);

private:
    [[nodiscard]] VideoPlaybackControlProjection projectedState() const;
    [[nodiscard]] VideoPlaybackControlPresentationMode presentationMode() const;
    [[nodiscard]] VideoPlaybackTimelineKind timelineKind() const;
    [[nodiscard]] qint64 normalizedPosition(qint64 positionMsec) const;
    void publishProjection();
    void synchronizeAutoHideTimer();
    void stopAutoHideTimer();
    void ensureAutoHideTimer();
    void handleAutoHideTimer();

    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    QObject* m_owner = nullptr;
    TimerScheduler m_timerScheduler;
    VideoPlaybackControlProjectionCallback m_projectionCallback;
    std::unique_ptr<RuntimeTimerHandle> m_autoHideTimer;
    VideoPlaybackControlEnvironment m_environment;
    VideoPlaybackControlMediaSnapshot m_media;
    VideoPlaybackControlProjection m_projection;
    quint64 m_sourceRevision = 0;
    quint64 m_nextProjectionRevision = 1;
    qint64 m_scrubPositionMsec = 0;
    int m_timerIntervalMsec = 0;
    bool m_environmentKnown = false;
    bool m_interactionActive = false;
    bool m_explicitlyRevealed = true;
    bool m_scrubbing = false;
};
}

#endif
