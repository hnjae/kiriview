// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videoplaybackcontrolplan.h"

#include <algorithm>
#include <limits>

namespace {
qint64 clampedPosition(qint64 position, qint64 duration)
{
    return duration > 0 ? std::clamp(position, qint64 { 0 }, duration)
                        : std::max(position, qint64 { 0 });
}

qint64 saturatedAdd(qint64 left, qint64 right)
{
    if (right > 0 && left > std::numeric_limits<qint64>::max() - right) {
        return std::numeric_limits<qint64>::max();
    }
    if (right < 0 && left < std::numeric_limits<qint64>::min() - right) {
        return std::numeric_limits<qint64>::min();
    }
    return left + right;
}
}

namespace kiriview {
bool VideoPlaybackControlPlan::isEmpty() const
{
    return !stateDelta.mediaEnded.has_value() && !stateDelta.playing.has_value()
        && !stateDelta.position.has_value() && backendOperations.empty();
}

VideoPlaybackControlPlan videoPlaybackPlayPlan(VideoPlaybackControlSnapshot snapshot)
{
    VideoPlaybackControlPlan plan;
    if (snapshot.sourceUrlEmpty) {
        return plan;
    }
    plan.backendOperations.emplace_back(EnsureVideoPlaybackBackendOperation {});
    if (snapshot.mediaEnded && snapshot.seekable) {
        plan.backendOperations.emplace_back(SetVideoPlaybackPositionOperation { 0 });
        plan.stateDelta.position = 0;
    }
    plan.stateDelta.mediaEnded = false;
    plan.backendOperations.emplace_back(PlayVideoPlaybackOperation {});
    return plan;
}

VideoPlaybackControlPlan videoPlaybackPausePlan(VideoPlaybackControlSnapshot snapshot)
{
    VideoPlaybackControlPlan plan;
    if (snapshot.mediaBackendAvailable) {
        plan.backendOperations.emplace_back(PauseVideoPlaybackOperation {});
    }
    return plan;
}

VideoPlaybackControlPlan videoPlaybackStopPlan(VideoPlaybackControlSnapshot snapshot)
{
    VideoPlaybackControlPlan plan;
    plan.stateDelta.mediaEnded = false;
    if (snapshot.mediaBackendAvailable) {
        plan.backendOperations.emplace_back(StopVideoPlaybackOperation {});
    }
    plan.stateDelta.playing = false;
    if (snapshot.seekable) {
        if (snapshot.mediaBackendAvailable) {
            plan.backendOperations.emplace_back(SetVideoPlaybackPositionOperation { 0 });
        }
        plan.stateDelta.position = 0;
    }
    return plan;
}

VideoPlaybackControlPlan videoPlaybackTogglePlan(VideoPlaybackControlSnapshot snapshot)
{
    return snapshot.playing ? videoPlaybackPausePlan(snapshot) : videoPlaybackPlayPlan(snapshot);
}

VideoPlaybackControlPlan videoPlaybackSetPositionPlan(
    VideoPlaybackControlSnapshot snapshot, qint64 position)
{
    VideoPlaybackControlPlan plan;
    if (!snapshot.seekable) {
        return plan;
    }
    const qint64 target = clampedPosition(position, snapshot.duration);
    plan.stateDelta.mediaEnded = false;
    plan.backendOperations.emplace_back(EnsureVideoPlaybackBackendOperation {});
    plan.backendOperations.emplace_back(SetVideoPlaybackPositionOperation { target });
    plan.stateDelta.position = target;
    return plan;
}

VideoPlaybackControlPlan videoPlaybackSeekByPlan(
    VideoPlaybackControlSnapshot snapshot, qint64 deltaMilliseconds)
{
    if (!snapshot.seekable) {
        return {};
    }
    const qint64 target
        = clampedPosition(saturatedAdd(snapshot.position, deltaMilliseconds), snapshot.duration);
    if (target == snapshot.position) {
        return {};
    }
    return videoPlaybackSetPositionPlan(snapshot, target);
}

}
