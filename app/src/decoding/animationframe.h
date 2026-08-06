// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ANIMATIONFRAME_H
#define KIRIVIEW_ANIMATIONFRAME_H

#include "imagedecodeworkspace.h"

#include <QImage>
#include <QString>
#include <expected>
#include <optional>
#include <utility>

namespace kiriview {
struct AnimationFrame
{
    AnimationFrame() = default;
    AnimationFrame(
        QImage frameImage, int frameDelay, ImageDecodeWorkspaceHold retainedWorkspace = {})
        : workspaceHold(std::move(retainedWorkspace))
        , image(std::move(frameImage))
        , delay(frameDelay)
    {
    }
    AnimationFrame(const AnimationFrame&) = default;
    AnimationFrame(AnimationFrame&&) noexcept = default;
    ~AnimationFrame() = default;
    AnimationFrame& operator=(const AnimationFrame& other)
    {
        if (this == &other) {
            return *this;
        }
        QImage nextImage = other.image;
        ImageDecodeWorkspaceHold nextWorkspaceHold = other.workspaceHold;
        image = {};
        workspaceHold = {};
        workspaceHold = std::move(nextWorkspaceHold);
        image = std::move(nextImage);
        delay = other.delay;
        return *this;
    }
    AnimationFrame& operator=(AnimationFrame&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        QImage nextImage = std::move(other.image);
        ImageDecodeWorkspaceHold nextWorkspaceHold = std::move(other.workspaceHold);
        image = {};
        workspaceHold = {};
        workspaceHold = std::move(nextWorkspaceHold);
        image = std::move(nextImage);
        delay = other.delay;
        return *this;
    }

    ImageDecodeWorkspaceHold workspaceHold;
    QImage image;
    int delay = 0;
};

using AnimationFrameReadResult = std::expected<std::optional<AnimationFrame>, QString>;
}

#endif
