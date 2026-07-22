// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadnavigation.h"

#include <QtGlobal>
#include <limits>

namespace kiriview {
int imageSpreadNavigationCurrentLastPageNumber(ImageSpreadNavigationState state)
{
    if (state.currentPageNumber <= 0) {
        return 0;
    }
    return state.secondaryPageVisible && state.currentPageNumber < std::numeric_limits<int>::max()
        ? state.currentPageNumber + 1
        : state.currentPageNumber;
}

ImageSpreadPageNavigationTarget imageSpreadPageNavigationTarget(
    NavigationDirection direction, ImageSpreadNavigationState state)
{
    if (!state.twoPageModeActive || state.currentPageNumber <= 0) {
        return {};
    }
    if (direction == NavigationDirection::Next) {
        const int last = imageSpreadNavigationCurrentLastPageNumber(state);
        return { true, last < state.pageCount ? last + 1 : 0 };
    }
    if (state.currentPageNumber <= 2) {
        return { true, 1 };
    }
    const int offset = state.secondaryPageVisible && !state.previousPageIsWide ? -2 : -1;
    return { true, state.currentPageNumber + offset };
}

int imageSpreadRelativePageNavigationTarget(ImageSpreadNavigationState state, int offset)
{
    const qint64 target = static_cast<qint64>(state.currentPageNumber) + offset;
    return target >= 1 && target <= state.pageCount ? static_cast<int>(target) : 0;
}

bool imageSpreadShouldBeginNavigationTransition(
    ImageSpreadNavigationState state, int targetPageNumber)
{
    return state.twoPageModeActive && state.currentPageNumber > 0 && targetPageNumber > 0
        && targetPageNumber <= state.pageCount && targetPageNumber != state.currentPageNumber;
}
}
