/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "framepreparation_p.h"

struct ViewportEngineBuiltInFrameStageResult
{
    bool accepted = true;
    ImageViewportPageRole failedRole = ImageViewportPageRole::Primary;
    ImageViewportRequestStatus status = ImageViewportRequestStatus::Ready;
    ImageViewportRequestReason reason = ImageViewportRequestReason::Ready;
    ImageViewportInternal::PublicDiagnosticText diagnostic;
    bool playbackStopped = false;
};

ViewportEngineBuiltInFrameStageResult stageViewportEngineBuiltInTargetSpread(
    ImageViewportInternal::RequestState& request, ImageViewportInternal::DisplayState& display,
    ImageViewportExactnessPreference exactnessPreference,
    ImageViewportInternal::PlaybackState* playback = nullptr);
