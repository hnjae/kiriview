// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imageanimationpolicy.h"

#include <algorithm>

namespace {
constexpr int defaultAnimationFrameDelayMs = 100;
constexpr int minimumAnimationFrameDelayMs = 10;
}

namespace kiriview {
int normalizedAnimationFrameDelay(int delayMs)
{
    if (delayMs < 0) {
        return defaultAnimationFrameDelayMs;
    }

    return std::max(delayMs, minimumAnimationFrameDelayMs);
}
}
