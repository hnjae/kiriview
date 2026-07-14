#pragma once

#include <ImageViewport/ImageViewport>

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
