// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "directmedianavigationmodel.h"

#include "location/imageurl.h"
#include "location/sourcekey.h"
#include "navigationcandidateordering.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace {
std::optional<std::size_t> clampedMediaNumberIndex(int mediaNumber, std::size_t candidateCount)
{
    if (candidateCount == 0) {
        return std::nullopt;
    }

    if (mediaNumber <= 1) {
        return 0;
    }

    return std::min(static_cast<std::size_t>(mediaNumber - 1), candidateCount - 1);
}

std::optional<QUrl> directMediaNavigationTargetUrlForRequest(
    const std::vector<kiriview::DirectMediaNavigationCandidate>& candidates, const QUrl& currentUrl,
    kiriview::DirectMediaNavigationOpenRequest request)
{
    switch (request.kind) {
    case kiriview::DirectMediaNavigationOpenKind::Previous:
        return kiriview::adjacentDirectMediaNavigationUrl(
            candidates, currentUrl, kiriview::NavigationDirection::Previous);
    case kiriview::DirectMediaNavigationOpenKind::Next:
        return kiriview::adjacentDirectMediaNavigationUrl(
            candidates, currentUrl, kiriview::NavigationDirection::Next);
    case kiriview::DirectMediaNavigationOpenKind::Number: {
        const std::optional<std::size_t> targetIndex
            = clampedMediaNumberIndex(request.mediaNumber, candidates.size());
        if (!targetIndex.has_value()) {
            return std::nullopt;
        }

        return candidates.at(*targetIndex).url;
    }
    }

    return std::nullopt;
}
}

namespace kiriview {
DirectMediaNavigationOpenRequest previousDirectMediaNavigationOpenRequest()
{
    return DirectMediaNavigationOpenRequest { DirectMediaNavigationOpenKind::Previous, 0 };
}

DirectMediaNavigationOpenRequest nextDirectMediaNavigationOpenRequest()
{
    return DirectMediaNavigationOpenRequest { DirectMediaNavigationOpenKind::Next, 0 };
}

DirectMediaNavigationOpenRequest numberedDirectMediaNavigationOpenRequest(int mediaNumber)
{
    return DirectMediaNavigationOpenRequest { DirectMediaNavigationOpenKind::Number, mediaNumber };
}

std::optional<std::size_t> directMediaNavigationCandidateIndex(
    const std::vector<DirectMediaNavigationCandidate>& candidates, const QUrl& currentUrl)
{
    const SourceKey currentKey = sourceKeyForUrl(currentUrl);
    if (!currentKey.valid) {
        return std::nullopt;
    }

    const auto currentCandidate = std::ranges::find_if(
        candidates, [&currentKey](const DirectMediaNavigationCandidate& row) {
            return sameSourceKey(sourceKeyForUrl(row.url), currentKey);
        });
    if (currentCandidate == candidates.cend()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::ranges::distance(candidates.cbegin(), currentCandidate));
}

std::optional<QUrl> adjacentDirectMediaNavigationUrl(
    const std::vector<DirectMediaNavigationCandidate>& candidates, const QUrl& currentUrl,
    NavigationDirection direction)
{
    const std::optional<std::size_t> targetIndex = adjacentNavigationIndex(
        candidates.size(), directMediaNavigationCandidateIndex(candidates, currentUrl), direction);
    if (!targetIndex.has_value()) {
        return std::nullopt;
    }

    return candidates.at(*targetIndex).url;
}

std::optional<QUrl> directMediaNavigationRemovalFallbackUrl(
    const std::vector<DirectMediaNavigationCandidate>& candidates, const QUrl& removedCurrentUrl)
{
    std::vector<DirectMediaNavigationCandidate> orderedCandidates = candidates;
    if (!directMediaNavigationCandidateIndex(orderedCandidates, removedCurrentUrl).has_value()) {
        orderedCandidates.push_back(DirectMediaNavigationCandidate {
            removedCurrentUrl, userVisibleFileNameForUrl(removedCurrentUrl) });
        sortDirectMediaNavigationCandidates(&orderedCandidates);
    }

    std::optional<QUrl> nextUrl = adjacentDirectMediaNavigationUrl(
        orderedCandidates, removedCurrentUrl, NavigationDirection::Next);
    if (nextUrl.has_value()) {
        return nextUrl;
    }

    return adjacentDirectMediaNavigationUrl(
        orderedCandidates, removedCurrentUrl, NavigationDirection::Previous);
}

DirectMediaNavigationBoundaryState directMediaNavigationBoundaryState(
    const std::vector<DirectMediaNavigationCandidate>& candidates, const QUrl& currentUrl)
{
    const std::optional<std::size_t> currentIndex
        = directMediaNavigationCandidateIndex(candidates, currentUrl);
    if (!currentIndex.has_value() || candidates.empty()) {
        return {};
    }

    return DirectMediaNavigationBoundaryState {
        *currentIndex > 0,
        *currentIndex + 1 < candidates.size(),
        *currentIndex == 0,
        *currentIndex + 1 == candidates.size(),
        static_cast<int>(*currentIndex + 1),
        static_cast<int>(candidates.size()),
    };
}

DirectMediaNavigationOpenPlan directMediaNavigationOpenPlan(
    const std::vector<DirectMediaNavigationCandidate>& candidates, const QUrl& currentUrl,
    DirectMediaNavigationOpenRequest request)
{
    return DirectMediaNavigationOpenPlan {
        directMediaNavigationBoundaryState(candidates, currentUrl),
        directMediaNavigationTargetUrlForRequest(candidates, currentUrl, request),
    };
}

void sortDirectMediaNavigationCandidates(std::vector<DirectMediaNavigationCandidate>* candidates)
{
    sortNavigationCandidatesByNameAndUrl(candidates);
}
}
