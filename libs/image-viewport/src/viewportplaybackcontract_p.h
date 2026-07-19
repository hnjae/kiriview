/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imageviewporttypes.h>

#include <array>
#include <cstddef>

struct ViewportPlaybackCommand
{
    enum class Kind {
        Play,
        Pause,
        Stop,
        SeekFrame,
        SeekPosition,
    };

    Kind kind = Kind::Play;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    int value = 0;
};

struct ViewportPlaybackScheduleEffect
{
    enum class Action {
        NoChange,
        Stop,
        ArmAfter,
    };

    Action action = Action::NoChange;
    int delayMilliseconds = -1;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 generation = 0;
    quint64 scheduleIdentity = 0;
};

struct ViewportPlaybackScheduleBatch
{
    ViewportPlaybackScheduleEffect& forRole(ImageViewportPageRole role)
    {
        return effects[role == ImageViewportPageRole::Secondary ? 1U : 0U];
    }

    const ViewportPlaybackScheduleEffect& forRole(ImageViewportPageRole role) const
    {
        return effects[role == ImageViewportPageRole::Secondary ? 1U : 0U];
    }

    std::array<ViewportPlaybackScheduleEffect, 2> effects {
        ViewportPlaybackScheduleEffect {
            ViewportPlaybackScheduleEffect::Action::NoChange, -1, ImageViewportPageRole::Primary },
        ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange, -1,
            ImageViewportPageRole::Secondary },
    };
};

struct ViewportPlaybackTimeoutFact
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 generation = 0;
    quint64 scheduleIdentity = 0;
    int elapsedMilliseconds = 0;
};
