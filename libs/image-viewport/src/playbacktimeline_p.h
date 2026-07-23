/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"

namespace ImageViewportInternal {

struct PlaybackAdvanceTarget
{
    DisplayRequestTarget displayTarget;
    int playbackPosition = -1;
    bool reachedEnd = false;
    bool looped = false;
    bool valid = false;
};

template <typename FrameStartFor, typename FrameIndexFor>
PlaybackAdvanceTarget playbackAdvanceTarget(int elapsedMilliseconds, int currentFrame,
    int currentPlaybackPosition, bool looping, int totalDuration, int frameCount,
    FrameStartFor frameStartFor, FrameIndexFor frameIndexFor)
{
    PlaybackAdvanceTarget target;
    const qint64 initialPlaybackPosition
        = currentPlaybackPosition < 0 ? frameStartFor(currentFrame) : currentPlaybackPosition;
    const qint64 nextPlaybackPosition = initialPlaybackPosition + elapsedMilliseconds;

    if (nextPlaybackPosition >= totalDuration) {
        if (looping) {
            const int wrappedPosition
                = totalDuration > 0 ? int(nextPlaybackPosition % totalDuration) : 0;
            const int wrappedFrame = frameIndexFor(wrappedPosition);
            if (wrappedFrame < 0) {
                return target;
            }
            target.displayTarget.frame = wrappedFrame;
            target.playbackPosition = wrappedPosition;
            target.displayTarget.position = frameStartFor(wrappedFrame);
            target.looped = true;
            target.valid = true;
            return target;
        }

        const int finalFrame = frameCount - 1;
        target.displayTarget.frame = finalFrame;
        target.displayTarget.position = frameStartFor(finalFrame);
        target.playbackPosition = totalDuration;
        target.reachedEnd = true;
        target.valid = true;
        return target;
    }

    const int admittedPlaybackPosition = int(nextPlaybackPosition);
    const int nextFrame = frameIndexFor(admittedPlaybackPosition);
    if (nextFrame < 0) {
        return target;
    }
    target.displayTarget.frame = nextFrame;
    target.displayTarget.position = frameStartFor(nextFrame);
    target.playbackPosition = admittedPlaybackPosition;
    target.valid = true;
    return target;
}

} // namespace ImageViewportInternal
