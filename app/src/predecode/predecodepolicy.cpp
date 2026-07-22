// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodepolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace {
constexpr qint64 biasedNavigationMsec = 450;
constexpr qint64 scrubbingNavigationMsec = 120;
constexpr qint64 neutralNavigationMsec = 800;
constexpr int scrubbingPageJump = 3;

qsizetype saturatedAdd(qsizetype left, qsizetype right)
{
    return right > 0 && left > std::numeric_limits<qsizetype>::max() - right
        ? std::numeric_limits<qsizetype>::max()
        : left + right;
}

qint64 saturatedSubtract(qint64 left, qint64 right)
{
    if (right < 0 && left > std::numeric_limits<qint64>::max() + right) {
        return std::numeric_limits<qint64>::max();
    }
    if (right > 0 && left < std::numeric_limits<qint64>::min() + right) {
        return std::numeric_limits<qint64>::min();
    }
    return left - right;
}

int saturatedIncrement(int value)
{
    return value == std::numeric_limits<int>::max() ? value : value + 1;
}

struct RetainedImage
{
    std::size_t index = 0;
    kiriview::PredecodeCachedImageState state;
};

std::size_t retentionGroup(const RetainedImage& image, std::size_t windowCount)
{
    if (image.state.recentDisplayed) {
        return 0;
    }
    return image.state.windowPriority < windowCount ? 1 : 2;
}
}

namespace kiriview {
int predecodeDebounceMsec() { return static_cast<int>(scrubbingNavigationMsec); }

int predecodeNeutralRefreshMsec() { return static_cast<int>(neutralNavigationMsec); }

PredecodeSourceProfile predecodeSourceProfileForOpenedCollectionScope(
    const OpenedCollectionScopeLocation& openedCollectionScope, int idealThreadCount)
{
    if (openedCollectionScope.isEmpty()) {
        return directMediaPredecodeSourceProfile();
    }
    if (openedCollectionScope.isDirectory()) {
        return { 3, 4, 6, 2 };
    }
    return { 3, 4, 6, static_cast<std::size_t>(std::clamp(idealThreadCount / 2, 1, 4)) };
}

std::vector<std::size_t> predecodeRetainedCachedImageIndices(
    const std::vector<PredecodeCachedImageState>& states, std::size_t windowCount,
    qsizetype byteBudget)
{
    if (byteBudget <= 0) {
        return {};
    }
    std::vector<RetainedImage> current;
    std::vector<RetainedImage> other;
    for (std::size_t index = 0; index < states.size(); ++index) {
        if (states[index].byteCost <= 0) {
            continue;
        }
        (states[index].currentDisplayed ? current : other).push_back({ index, states[index] });
    }
    std::ranges::sort(current, [](const RetainedImage& left, const RetainedImage& right) {
        return std::tie(left.state.currentDisplayedPriority, left.index)
            < std::tie(right.state.currentDisplayedPriority, right.index);
    });
    qsizetype currentCost = 0;
    for (const RetainedImage& image : current) {
        currentCost = saturatedAdd(currentCost, image.state.byteCost);
    }
    const qsizetype adjacentBudget
        = std::max<qsizetype>(byteBudget - std::min(byteBudget, currentCost), 0);
    std::ranges::sort(other, [windowCount](const RetainedImage& left, const RetainedImage& right) {
        const std::size_t leftGroup = retentionGroup(left, windowCount);
        const std::size_t rightGroup = retentionGroup(right, windowCount);
        if (leftGroup != rightGroup) {
            return leftGroup < rightGroup;
        }
        if (leftGroup == 0
            && left.state.recentDisplayedPriority != right.state.recentDisplayedPriority) {
            return left.state.recentDisplayedPriority < right.state.recentDisplayedPriority;
        }
        if (leftGroup == 1 && left.state.windowPriority != right.state.windowPriority) {
            return left.state.windowPriority < right.state.windowPriority;
        }
        if (leftGroup == 2 && left.state.lastUsedSequence != right.state.lastUsedSequence) {
            return left.state.lastUsedSequence > right.state.lastUsedSequence;
        }
        return left.index < right.index;
    });
    qsizetype retainedCost = 0;
    for (const RetainedImage& image : other) {
        if (saturatedAdd(retainedCost, image.state.byteCost) > adjacentBudget) {
            break;
        }
        retainedCost = saturatedAdd(retainedCost, image.state.byteCost);
        current.push_back(image);
    }
    std::vector<std::size_t> indices;
    indices.reserve(current.size());
    for (const RetainedImage& image : current) {
        indices.push_back(image.index);
    }
    return indices;
}

std::vector<std::size_t> predecodeMissingWindowLoadIndices(
    const std::vector<PredecodeWindowLoadState>& states)
{
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < states.size(); ++index) {
        if (!states[index].displayed && !states[index].cached && !states[index].inFlight) {
            indices.push_back(index);
        }
    }
    return indices;
}

PredecodeSchedulePlan predecodeSchedulePlan(
    std::size_t candidateCount, std::optional<std::size_t> currentIndex, PredecodePolicyInput input)
{
    if (input.powerSaverEnabled || input.momentumMode == PredecodeMomentumMode::ScrubbingNext
        || input.momentumMode == PredecodeMomentumMode::ScrubbingPrev) {
        return {};
    }
    PredecodeSchedulePlan plan { input.sourceProfile.parallelLimit, {} };
    if (plan.parallelLimit == 0 || !currentIndex.has_value() || *currentIndex >= candidateCount) {
        return plan;
    }
    plan.targetIndices.push_back(*currentIndex);
    std::size_t nextCount = input.sourceProfile.neutralNextPageCount;
    std::size_t previousCount = input.sourceProfile.neutralPreviousPageCount;
    if (input.momentumMode == PredecodeMomentumMode::NextBiased) {
        nextCount = input.sourceProfile.biasedDirectionPageCount;
        previousCount = 1;
    } else if (input.momentumMode == PredecodeMomentumMode::PrevBiased) {
        nextCount = 1;
        previousCount = input.sourceProfile.biasedDirectionPageCount;
    }
    for (std::size_t offset = 1; offset <= std::max(nextCount, previousCount); ++offset) {
        if (offset <= nextCount && offset <= std::numeric_limits<std::size_t>::max() - *currentIndex
            && *currentIndex + offset < candidateCount) {
            plan.targetIndices.push_back(*currentIndex + offset);
        }
        if (offset <= previousCount && offset <= *currentIndex) {
            plan.targetIndices.push_back(*currentIndex - offset);
        }
    }
    return plan;
}

PredecodeMomentumState predecodeUpdatedMomentumState(
    PredecodeMomentumState state, int pageIndex, qint64 monotonicMsec)
{
    state.mode = PredecodeMomentumMode::Neutral;
    if (pageIndex < 0) {
        return state;
    }
    if (state.lastPageIndex < 0 || state.lastNavigationMsec < 0) {
        state.lastPageIndex = pageIndex;
        state.lastNavigationMsec = monotonicMsec;
        return state;
    }
    if (pageIndex == state.lastPageIndex) {
        return state;
    }
    const qint64 elapsed = saturatedSubtract(monotonicMsec, state.lastNavigationMsec);
    const PredecodeMomentumDirection direction = pageIndex > state.lastPageIndex
        ? PredecodeMomentumDirection::Next
        : PredecodeMomentumDirection::Previous;
    state.sameDirectionMoveCount
        = direction == state.lastDirection && elapsed <= biasedNavigationMsec
        ? saturatedIncrement(state.sameDirectionMoveCount)
        : 1;
    const qint64 pageDelta = static_cast<qint64>(pageIndex) - state.lastPageIndex;
    if (elapsed >= 0 && elapsed < neutralNavigationMsec) {
        const bool next = pageDelta > 0;
        const bool scrubbing
            = (state.sameDirectionMoveCount >= 2 && elapsed <= scrubbingNavigationMsec)
            || (std::abs(pageDelta) >= scrubbingPageJump && elapsed <= biasedNavigationMsec);
        if (scrubbing) {
            state.mode = next ? PredecodeMomentumMode::ScrubbingNext
                              : PredecodeMomentumMode::ScrubbingPrev;
        } else if (state.sameDirectionMoveCount >= 2 && elapsed <= biasedNavigationMsec) {
            state.mode
                = next ? PredecodeMomentumMode::NextBiased : PredecodeMomentumMode::PrevBiased;
        }
    }
    state.lastDirection = direction;
    state.lastPageIndex = pageIndex;
    state.lastNavigationMsec = monotonicMsec;
    return state;
}

PredecodeQueuedLoadPlan predecodeNextQueuedLoadPlan(
    const std::vector<PredecodeQueuedLoadState>& states)
{
    for (std::size_t index = 0; index < states.size(); ++index) {
        const PredecodeQueuedLoadState& state = states[index];
        if (state.valid && state.inWindow && !state.cached && !state.inFlight) {
            return { true, index,
                index == std::numeric_limits<std::size_t>::max() ? index : index + 1 };
        }
    }
    return { false, 0, states.size() };
}
}
