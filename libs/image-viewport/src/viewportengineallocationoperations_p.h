/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"

struct ViewportEnginePayloadAllocationRebuildResult
{
    bool retainedDisplayDiscarded = false;
    bool roleBudgetsIncreased = false;
};

qint64 viewportEngineDisplayPayloadByteBudget();
ViewportEnginePayloadAllocationRebuildResult rebuildViewportEnginePayloadAllocation(
    const ImageViewportInternal::RequestState& request,
    ImageViewportInternal::DisplayState& display, bool retainedDisplayPinned = false);
