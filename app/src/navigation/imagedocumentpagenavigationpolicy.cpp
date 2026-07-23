// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagenavigationpolicy.h"

#include "location/imageurl.h"
#include "navigationcandidateordering.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace {
std::optional<std::size_t> currentUrlIndex(const std::vector<QUrl>& urls, const QUrl& currentUrl)
{
    const auto current = std::ranges::find_if(urls,
        [&currentUrl](const QUrl& url) { return kiriview::sameNormalizedUrl(url, currentUrl); });
    if (current == urls.cend()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(std::ranges::distance(urls.cbegin(), current));
}

int boundedIndex(std::size_t index)
{
    return std::in_range<int>(index) ? static_cast<int>(index) : std::numeric_limits<int>::max();
}
}

namespace kiriview {
int ImageDocumentPageNavigationSnapshot::currentPageNumber() const
{
    return pageNavigationCurrentPageNumber(state);
}

int ImageDocumentPageNavigationSnapshot::pageCount() const
{
    return pageNavigationPageCount(state);
}

std::optional<QUrl> ImageDocumentPageNavigationSnapshot::urlAtPage(int pageNumber) const
{
    return pageNavigationUrlAtPage(state, pageNumber);
}

std::vector<ImageDocumentPageTarget> imageDocumentPageCandidateTargets(
    const std::vector<ImageDocumentPageCandidate>& candidates)
{
    std::vector<ImageDocumentPageTarget> targets;
    targets.reserve(candidates.size());
    for (const ImageDocumentPageCandidate& candidate : candidates) {
        targets.emplace_back(candidate.url, candidate.kind, candidate.name);
    }

    return targets;
}

std::vector<QUrl> imageDocumentPageTargetUrls(const std::vector<ImageDocumentPageTarget>& targets)
{
    std::vector<QUrl> urls;
    urls.reserve(targets.size());
    for (const ImageDocumentPageTarget& target : targets) {
        urls.push_back(target.url);
    }

    return urls;
}

bool imageDocumentPageCandidateIsImage(const ImageDocumentPageCandidate& candidate)
{
    return candidate.kind == ImageDocumentPageKind::Image;
}

std::optional<std::size_t> imageDocumentPageCandidateIndex(
    const std::vector<ImageDocumentPageCandidate>& candidates, const QUrl& currentUrl)
{
    return navigationCandidateIndex(candidates, currentUrl);
}

bool imageDocumentPageCandidatesContainUrl(
    const std::vector<ImageDocumentPageCandidate>& candidates, const QUrl& url)
{
    return imageDocumentPageCandidateIndex(candidates, url).has_value();
}

std::optional<ImageDocumentPageCandidate> adjacentImageDocumentPageCandidate(
    const std::vector<ImageDocumentPageCandidate>& candidates, const QUrl& currentUrl,
    NavigationDirection direction)
{
    return adjacentNavigationCandidate(candidates, currentUrl, direction);
}

std::optional<ContainerNavigationCandidate> adjacentContainerNavigationCandidate(
    const std::vector<ContainerNavigationCandidate>& candidates, const QUrl& currentContainerUrl,
    NavigationDirection direction)
{
    return adjacentNavigationCandidate(candidates, currentContainerUrl, direction);
}

std::optional<std::size_t> containerNavigationCandidateIndex(
    const std::vector<ContainerNavigationCandidate>& candidates, const QUrl& currentContainerUrl)
{
    return navigationCandidateIndex(candidates, currentContainerUrl);
}

int pageNavigationCurrentPageNumber(const PageNavigationState& state)
{
    return state.currentIndex < 0 ? 0 : state.currentIndex + 1;
}

int pageNavigationPageCount(const PageNavigationState& state)
{
    return static_cast<int>(state.targets.size());
}

bool pageNavigationHasKnownSelection(const PageNavigationState& state)
{
    if (state.currentIndex < 0) {
        return false;
    }

    return static_cast<std::size_t>(state.currentIndex) < state.targets.size();
}

std::optional<QUrl> pageNavigationUrlAtPage(const PageNavigationState& state, int pageNumber)
{
    const std::optional<ImageDocumentPageTarget> target
        = pageNavigationTargetAtPage(state, pageNumber);
    if (!target.has_value()) {
        return std::nullopt;
    }

    return target->url;
}

std::optional<ImageDocumentPageTarget> pageNavigationTargetAtPage(
    const PageNavigationState& state, int pageNumber)
{
    if (pageNumber < 1) {
        return std::nullopt;
    }

    const std::size_t pageIndex = static_cast<std::size_t>(pageNumber - 1);
    if (pageIndex >= state.targets.size()) {
        return std::nullopt;
    }

    return state.targets.at(pageIndex);
}

std::optional<std::size_t> pageNavigationTargetIndex(
    const PageNavigationState& state, int pageNumber)
{
    if (pageNumber < 1) {
        return std::nullopt;
    }
    const std::size_t index = static_cast<std::size_t>(pageNumber - 1);
    if (index >= state.targets.size()
        || (state.currentIndex >= 0 && index == static_cast<std::size_t>(state.currentIndex))) {
        return std::nullopt;
    }
    return index;
}

std::optional<std::size_t> pageNavigationAdjacentTargetIndex(
    const PageNavigationState& state, NavigationDirection direction)
{
    if (!pageNavigationHasKnownSelection(state)) {
        return std::nullopt;
    }
    const std::size_t current = static_cast<std::size_t>(state.currentIndex);
    if (direction == NavigationDirection::Previous) {
        return current == 0 ? std::nullopt : std::optional<std::size_t>(current - 1);
    }
    return current + 1 < state.targets.size() ? std::optional<std::size_t>(current + 1)
                                              : std::nullopt;
}

PageNavigationState pageNavigationStateForCurrentUrl(
    const PageNavigationState& knownState, const QUrl& currentUrl)
{
    const std::vector<QUrl> knownUrls = imageDocumentPageTargetUrls(knownState.targets);
    const std::optional<std::size_t> current = currentUrlIndex(knownUrls, currentUrl);
    if (current.has_value()) {
        return PageNavigationState { knownState.targets, boundedIndex(*current) };
    }
    if (!currentUrl.isValid() || currentUrl.isEmpty() || knownState.targets.empty()) {
        return {};
    }
    return PageNavigationState { knownState.targets, -1 };
}

PageNavigationState pageNavigationStateForTargets(
    std::vector<ImageDocumentPageTarget> targets, const QUrl& currentUrl)
{
    PageNavigationState state { std::move(targets), -1 };
    std::vector<QUrl> urls = imageDocumentPageTargetUrls(state.targets);
    const std::optional<std::size_t> current = currentUrlIndex(urls, currentUrl);
    if (current.has_value()) {
        state.currentIndex = boundedIndex(*current);
        return state;
    }
    if (currentUrl.isValid() && !currentUrl.isEmpty() && state.targets.empty()) {
        const QUrl normalizedUrl = normalizedImageUrl(currentUrl);
        state.targets.insert(state.targets.begin(),
            ImageDocumentPageTarget { normalizedUrl, ImageDocumentPageKind::Image,
                normalizedUrl.fileName(QUrl::PrettyDecoded) });
        state.currentIndex = 0;
    }

    return state;
}

bool samePageNavigationState(const PageNavigationState& left, const PageNavigationState& right)
{
    return left.targets == right.targets && left.currentIndex == right.currentIndex;
}

void sortImageDocumentPageCandidates(std::vector<ImageDocumentPageCandidate>* candidates)
{
    sortNavigationCandidatesByNameAndUrl(candidates);
}

void sortContainerNavigationCandidates(std::vector<ContainerNavigationCandidate>* candidates)
{
    sortNavigationCandidatesByNameAndUrl(candidates);
}

}
