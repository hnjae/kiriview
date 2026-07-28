// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodecache.h"

#include "location/imageurl.h"
#include "predecodelogging.h"
#include "predecodepolicy.h"

#include <QDebug>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>

namespace kiriview {
PredecodeCache::PredecodeCache(qsizetype byteBudget)
    : m_byteBudget(byteBudget)
{
}

void PredecodeCache::clear()
{
    qCDebug(kiriviewPredecodeLog) << "predecode cache clear"
                                  << "windowLocations" << m_windowLocations.size() << "queuedLoads"
                                  << m_queue.size() << "cachedImages" << m_images.size();
    m_windowLocations.clear();
    m_displayedHistory.clear();
    m_queue.clear();
    m_images.clear();
    m_lastUsedSequence = 0;
}

void PredecodeCache::clearQueuedLoads()
{
    qCDebug(kiriviewPredecodeLog) << "predecode queue clear"
                                  << "queuedLoads" << m_queue.size();
    m_queue.clear();
}

void PredecodeCache::setWindowLocations(const std::vector<DisplayedImageLocation>& locations)
{
    m_windowLocations.clear();
    m_queue.clear();

    for (const DisplayedImageLocation& location : locations) {
        if (!normalizedValidImageUrl(location.imageUrl()).has_value()) {
            continue;
        }
        if (containsLocation(m_windowLocations, location)) {
            continue;
        }

        m_windowLocations.push_back(location);
    }

    trimImagesToBudget();
    qCDebug(kiriviewPredecodeLog) << "predecode window locations set"
                                  << "requested" << locations.size() << "accepted"
                                  << m_windowLocations.size();
}

void PredecodeCache::setDisplayedLocations(const std::vector<DisplayedImageLocation>& locations)
{
    m_displayedHistory.setDisplayedLocations(locations);
    trimImagesToBudget();
}

void PredecodeCache::enqueueMissingWindowLoads(
    const DisplayedImageLocation& displayedLocation, const PredecodeActiveLoads& activeLoads)
{
    std::vector<PredecodeWindowLoadState> states;
    states.reserve(m_windowLocations.size());

    for (const DisplayedImageLocation& location : m_windowLocations) {
        states.push_back(PredecodeWindowLoadState {
            m_displayedHistory.currentContains(location) || location == displayedLocation,
            hasImage(location),
            isInFlight(location, activeLoads),
        });
    }

    const std::vector<std::size_t> missingIndices = predecodeMissingWindowLoadIndices(states);
    for (std::size_t index : missingIndices) {
        if (index < m_windowLocations.size()) {
            qCDebug(kiriviewPredecodeLog)
                << "predecode enqueue"
                << "url" << m_windowLocations.at(index).imageUrl() << "openedCollectionScope"
                << !m_windowLocations.at(index).openedCollectionScope().isEmpty();
            m_queue.push_back(PredecodeRequest { m_windowLocations.at(index) });
        }
    }
    qCDebug(kiriviewPredecodeLog) << "predecode enqueue missing complete"
                                  << "windowLocations" << m_windowLocations.size() << "enqueued"
                                  << missingIndices.size() << "queueSize" << m_queue.size();
}

std::optional<PredecodeRequest> PredecodeCache::takeNextRequest(
    const PredecodeActiveLoads& activeLoads)
{
    std::vector<PredecodeQueuedLoadState> states;
    states.reserve(m_queue.size());

    for (const PredecodeRequest& request : m_queue) {
        states.push_back(PredecodeQueuedLoadState {
            !request.location.isEmpty(),
            windowContains(request.location),
            hasImage(request.location),
            activeLoads.contains(request.location),
        });
    }

    const PredecodeQueuedLoadPlan plan = predecodeNextQueuedLoadPlan(states);
    const std::size_t discardCount
        = std::min(plan.discardCount, static_cast<std::size_t>(m_queue.size()));
    if (!plan.found || plan.index >= m_queue.size()) {
        qCDebug(kiriviewPredecodeLog)
            << "predecode queue exhausted"
            << "discardCount" << discardCount << "queueSize" << m_queue.size();
        m_queue.erase(m_queue.begin(), m_queue.begin() + static_cast<std::ptrdiff_t>(discardCount));
        return std::nullopt;
    }

    auto requestEntry = m_queue.begin() + static_cast<std::ptrdiff_t>(plan.index);
    PredecodeRequest request = std::move(*requestEntry);
    qCDebug(kiriviewPredecodeLog) << "predecode dequeue"
                                  << "url" << request.location.imageUrl() << "index" << plan.index
                                  << "discardCount" << discardCount;
    m_queue.erase(m_queue.begin(), m_queue.begin() + static_cast<std::ptrdiff_t>(discardCount));
    return request;
}

bool PredecodeCache::windowContains(const DisplayedImageLocation& location) const
{
    return normalizedValidImageUrl(location.imageUrl()).has_value()
        && containsLocation(m_windowLocations, location);
}

bool PredecodeCache::hasImage(const DisplayedImageLocation& location) const
{
    return normalizedValidImageUrl(location.imageUrl()).has_value()
        && findCachedImage(location) != m_images.cend();
}

bool PredecodeCache::isInFlight(
    const DisplayedImageLocation& location, const PredecodeActiveLoads& activeLoads) const
{
    if (!normalizedValidImageUrl(location.imageUrl()).has_value()) {
        return false;
    }

    return activeLoads.contains(location)
        || std::ranges::any_of(m_queue,
            [&location](const PredecodeRequest& request) { return request.location == location; });
}

std::optional<PredecodedImage> PredecodeCache::findImage(
    const DisplayedImageLocation& location) const
{
    if (!normalizedValidImageUrl(location.imageUrl()).has_value()) {
        return std::nullopt;
    }

    const auto cached = findCachedImage(location);
    if (cached == m_images.cend()) {
        return std::nullopt;
    }
    cached->lastUsedSequence = nextLastUsedSequence();

    return PredecodedImage { cached->displayImage, cached->location,
        cached->displayImage.embeddedMetadata };
}

void PredecodeCache::cacheImage(const DisplayedImageLocation& location,
    StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata)
{
    if (!displayImage.isAuthoritative()) {
        qCDebug(kiriviewPredecodeLog) << "predecode cache store skipped"
                                      << "reason"
                                      << "non-authoritative-payload"
                                      << "url" << location.imageUrl();
        return;
    }

    if (!metadata.isEmpty()) {
        displayImage.embeddedMetadata = std::move(metadata);
    }

    const std::optional<qsizetype> byteCost = displayImage.byteCostWithinBudget(m_byteBudget);
    if (!byteCost.has_value()) {
        qCDebug(kiriviewPredecodeLog) << "predecode cache store skipped"
                                      << "reason"
                                      << "byte-budget"
                                      << "url" << location.imageUrl() << "byteCost"
                                      << displayImage.byteCost() << "budget" << m_byteBudget;
        return;
    }

    if (!normalizedValidImageUrl(location.imageUrl()).has_value()) {
        qCDebug(kiriviewPredecodeLog) << "predecode cache store skipped"
                                      << "reason"
                                      << "empty-location";
        return;
    }

    removeCachedImage(location);
    m_images.push_back(
        CachedImage { location, std::move(displayImage), *byteCost, nextLastUsedSequence() });
    qCDebug(kiriviewPredecodeLog) << "predecode cache stored"
                                  << "url" << location.imageUrl() << "byteCost" << *byteCost
                                  << "cachedImages" << m_images.size();

    trimImagesToBudget();
}

void PredecodeCache::cacheDisplayedImage(bool cacheable, const DisplayedImageLocation& location,
    StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata)
{
    if (!cacheable || !normalizedValidImageUrl(location.imageUrl()).has_value()) {
        qCDebug(kiriviewPredecodeLog)
            << "displayed predecode cache skipped"
            << "reason" << (!cacheable ? "not-cacheable" : "empty-location") << "url"
            << location.imageUrl();
        return;
    }

    cacheImage(location, std::move(displayImage), std::move(metadata));
}

bool PredecodeCache::containsLocation(
    const std::vector<DisplayedImageLocation>& locations, const DisplayedImageLocation& location)
{
    return std::ranges::contains(locations, location);
}

PredecodeCache::CachedImageIterator PredecodeCache::findCachedImage(
    const DisplayedImageLocation& location)
{
    return std::ranges::find_if(
        m_images, [&location](const CachedImage& entry) { return entry.location == location; });
}

PredecodeCache::ConstCachedImageIterator PredecodeCache::findCachedImage(
    const DisplayedImageLocation& location) const
{
    return std::ranges::find_if(
        m_images, [&location](const CachedImage& entry) { return entry.location == location; });
}

void PredecodeCache::removeCachedImage(const DisplayedImageLocation& location)
{
    const auto cached = findCachedImage(location);
    if (cached != m_images.end()) {
        m_images.erase(cached);
    }
}

std::size_t PredecodeCache::windowPriority(const DisplayedImageLocation& location) const
{
    const auto priorityEntry = std::ranges::find(m_windowLocations, location);
    if (priorityEntry == m_windowLocations.cend()) {
        return m_windowLocations.size();
    }

    return static_cast<std::size_t>(
        std::ranges::distance(m_windowLocations.cbegin(), priorityEntry));
}

quint64 PredecodeCache::nextLastUsedSequence() const
{
    ++m_lastUsedSequence;
    if (m_lastUsedSequence == 0) {
        ++m_lastUsedSequence;
    }
    return m_lastUsedSequence;
}

void PredecodeCache::trimImagesToBudget()
{
    std::vector<PredecodeCachedImageState> states;
    states.reserve(m_images.size());

    for (const CachedImage& entry : m_images) {
        states.push_back(PredecodeCachedImageState {
            m_displayedHistory.currentContains(entry.location),
            m_displayedHistory.recentContains(entry.location),
            m_displayedHistory.currentPriority(entry.location),
            m_displayedHistory.recentPriority(entry.location),
            windowPriority(entry.location),
            entry.byteCost,
            entry.lastUsedSequence,
        });
    }

    const std::vector<std::size_t> retainedIndices
        = predecodeRetainedCachedImageIndices(states, m_windowLocations.size(), m_byteBudget);

    std::vector<CachedImage> retainedImages;
    retainedImages.reserve(retainedIndices.size());
    for (std::size_t index : retainedIndices) {
        if (index < m_images.size()) {
            retainedImages.push_back(std::move(m_images[index]));
        }
    }

    m_images = std::move(retainedImages);
}
}
