// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadsecondarypagerefreshpolicy.h"

#include <limits>

namespace kiriview {
ImageSpreadSecondaryPageRefreshPlan imageSpreadSecondaryPageRefreshPlan(
    ImageSpreadSecondaryPageRefreshState state)
{
    if (state.currentPageNumber == std::numeric_limits<int>::max()) {
        return {};
    }
    const int nextPageNumber = state.currentPageNumber + 1;
    if (!state.twoPageModeActive || state.currentPageNumber == 1 || state.primaryPageIsWide
        || nextPageNumber <= 1 || nextPageNumber > state.pageCount || !state.nextPageAvailable
        || state.nextPageIsWide) {
        return {};
    }
    return { state.currentSecondaryMatchesNext
            ? ImageSpreadSecondaryPageDecision::KeepCurrentSecondary
            : ImageSpreadSecondaryPageDecision::LoadNext,
        nextPageNumber };
}
}
