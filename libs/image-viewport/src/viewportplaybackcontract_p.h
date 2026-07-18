/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <ImageViewport/imageviewporttypes.h>

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
};
