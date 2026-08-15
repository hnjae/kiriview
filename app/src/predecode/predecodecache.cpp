// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodecache.h"

#include "cache/imagebyteaccounting.h"
#include "diagnostics/diagnosticlogprojection.h"
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
                                  << "windowLocations" << m_windowKeys.size() << "queuedLoads"
                                  << m_queue.size() << "cachedImages" << m_images.size();
    m_windowKeys.clear();
    m_displayedHistory.clear();
    m_currentDisplayedKeys.reset();
    m_queue.clear();
    m_completedWork.clear();
    m_images.clear();
    m_lastUsedSequence = 0;
}

void PredecodeCache::clearQueuedLoads()
{
    qCDebug(kiriviewPredecodeLog) << "predecode queue clear"
                                  << "queuedLoads" << m_queue.size();
    m_queue.clear();
}

void PredecodeCache::retireQueuedLoads(const DisplayedImageLocation& location)
{
    const std::size_t retired = std::erase_if(m_queue,
        [&location](const PredecodeRequest& request) { return request.location == location; });
    qCDebug(kiriviewPredecodeLog) << "predecode queued location retired"
                                  << "url" << diagnosticSourceReference(location.imageUrl())
                                  << "retired" << retired;
}

void PredecodeCache::reclaimDisplayOutputAliases()
{
    qsizetype retiredBytes = 0;
    const std::size_t retired
        = std::erase_if(m_images, [this, &retiredBytes](const CachedImage& entry) {
              const bool exactCurrentKey = !m_currentDisplayedKeys.has_value()
                  || containsKey(*m_currentDisplayedKeys, entry.key);
              const bool currentDisplayed
                  = m_displayedHistory.currentContains(entry.key.location) && exactCurrentKey;
              const bool shouldRetire = entry.retainsDisplayOutputAdmission && !currentDisplayed;
              if (shouldRetire) {
                  retiredBytes = saturatedQtByteSum(retiredBytes, entry.byteCost);
              }
              return shouldRetire;
          });
    if (retired > 0) {
        qCDebug(kiriviewPredecodeLog) << "predecode display-output aliases reclaimed"
                                      << "retired" << retired << "retiredBytes" << retiredBytes
                                      << "cachedImages" << m_images.size();
    }
}

void PredecodeCache::setWindowLocations(const std::vector<DisplayedImageLocation>& locations)
{
    std::vector<PredecodeImageKey> keys;
    keys.reserve(locations.size());
    for (const DisplayedImageLocation& location : locations) {
        keys.push_back(PredecodeImageKey { location, {} });
    }
    setWindowKeys(keys);
}

void PredecodeCache::setWindowKeys(const std::vector<PredecodeImageKey>& keys)
{
    m_windowKeys.clear();
    m_queue.clear();

    for (const PredecodeImageKey& key : keys) {
        if (!normalizedValidImageUrl(key.location.imageUrl()).has_value()) {
            continue;
        }
        if (containsKey(m_windowKeys, key)) {
            continue;
        }

        m_windowKeys.push_back(key);
    }

    trimImagesToBudget();
    qCDebug(kiriviewPredecodeLog) << "predecode window locations set"
                                  << "requested" << keys.size() << "accepted"
                                  << m_windowKeys.size();
}

void PredecodeCache::setDisplayedLocations(const std::vector<DisplayedImageLocation>& locations)
{
    m_displayedHistory.setDisplayedLocations(locations);
    m_currentDisplayedKeys.reset();
    trimImagesToBudget();
}

void PredecodeCache::setDisplayedImages(const std::vector<DisplayedPredecodeImage>& images)
{
    std::vector<DisplayedImageLocation> locations;
    std::vector<PredecodeImageKey> keys;
    locations.reserve(images.size());
    keys.reserve(images.size());
    for (const DisplayedPredecodeImage& image : images) {
        if (!image.hasLocation()) {
            continue;
        }
        if (!std::ranges::contains(locations, image.location)) {
            locations.push_back(image.location);
        }
        if (!image.isCacheable()) {
            continue;
        }
        const PredecodeImageKey key { image.location, image.displayImage->sourceRevision };
        if (key.isValid() && !containsKey(keys, key)) {
            keys.push_back(key);
        }
    }

    m_displayedHistory.setDisplayedLocations(locations);
    m_currentDisplayedKeys = std::move(keys);
    trimImagesToBudget();
}

void PredecodeCache::completeWork(const PredecodeWorkKey& workKey)
{
    if (!normalizedValidImageUrl(workKey.image.location.imageUrl()).has_value()
        || (!workKey.image.sourceRevision.isValid() && !workKey.scope.isValid())) {
        return;
    }
    std::erase_if(m_queue, [&workKey](const PredecodeRequest& request) {
        return samePredecodeWork(request.workKey(), workKey);
    });
    if (!hasCompletedWork(workKey)) {
        m_completedWork.push_back(workKey);
    }
}

void PredecodeCache::enqueueMissingWindowLoads(
    const DisplayedImageLocation& foregroundOwnedLocation, const PredecodeActiveLoads& activeLoads,
    PredecodeWorkScope workScope)
{
    retainCompletedWorkForCurrentWindow(workScope);
    std::vector<PredecodeWindowLoadState> states;
    states.reserve(m_windowKeys.size());

    for (const PredecodeImageKey& key : m_windowKeys) {
        states.push_back(PredecodeWindowLoadState {
            m_displayedHistory.currentContains(key.location)
                || key.location == foregroundOwnedLocation,
            hasImage(key),
            isInFlight(PredecodeWorkKey { key, workScope }, activeLoads)
                || hasCompletedWork(PredecodeWorkKey { key, workScope }),
        });
    }

    const std::vector<std::size_t> missingIndices = predecodeMissingWindowLoadIndices(states);
    for (std::size_t index : missingIndices) {
        if (index < m_windowKeys.size()) {
            const PredecodeImageKey& key = m_windowKeys.at(index);
            qCDebug(kiriviewPredecodeLog)
                << "predecode enqueue"
                << "url" << diagnosticSourceReference(key.location.imageUrl())
                << "openedCollectionScope" << !key.location.openedCollectionScope().isEmpty();
            m_queue.push_back(PredecodeRequest { key.location, key.sourceRevision, workScope });
        }
    }
    qCDebug(kiriviewPredecodeLog) << "predecode enqueue missing complete"
                                  << "windowLocations" << m_windowKeys.size() << "enqueued"
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
            windowContains(request.key()),
            hasImage(request.key()),
            activeLoads.contains(request.workKey()) || hasCompletedWork(request.workKey()),
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
                                  << "url" << diagnosticSourceReference(request.location.imageUrl())
                                  << "index" << plan.index << "discardCount" << discardCount;
    m_queue.erase(m_queue.begin(), m_queue.begin() + static_cast<std::ptrdiff_t>(discardCount));
    return request;
}

bool PredecodeCache::windowContains(const PredecodeImageKey& key) const
{
    return normalizedValidImageUrl(key.location.imageUrl()).has_value()
        && containsKey(m_windowKeys, key);
}

bool PredecodeCache::hasImage(const PredecodeImageKey& key) const
{
    return key.isValid() && findCachedImage(key) != m_images.cend();
}

bool PredecodeCache::hasCompletedWork(const PredecodeWorkKey& workKey) const
{
    return std::ranges::any_of(m_completedWork, [&workKey](const PredecodeWorkKey& completed) {
        return samePredecodeWork(completed, workKey);
    });
}

void PredecodeCache::retainCompletedWorkForCurrentWindow(PredecodeWorkScope workScope)
{
    std::erase_if(m_completedWork, [this, workScope](const PredecodeWorkKey& completed) {
        const auto windowKey
            = std::ranges::find_if(m_windowKeys, [&completed](const PredecodeImageKey& key) {
                  return key.location == completed.image.location;
              });
        return windowKey == m_windowKeys.end()
            || !samePredecodeWork(completed, PredecodeWorkKey { *windowKey, workScope });
    });
}

bool PredecodeCache::isInFlight(
    const PredecodeWorkKey& key, const PredecodeActiveLoads& activeLoads) const
{
    if (!normalizedValidImageUrl(key.image.location.imageUrl()).has_value()) {
        return false;
    }

    return activeLoads.contains(key)
        || std::ranges::any_of(m_queue, [&key](const PredecodeRequest& request) {
               return samePredecodeWork(request.workKey(), key);
           });
}

std::optional<PredecodedImage> PredecodeCache::findImage(const PredecodeImageKey& key) const
{
    if (!key.isValid()) {
        return std::nullopt;
    }

    const auto cached = findCachedImage(key);
    if (cached == m_images.cend()) {
        return std::nullopt;
    }
    cached->lastUsedSequence = nextLastUsedSequence();

    return PredecodedImage { cached->displayImage, cached->key.location,
        cached->displayImage.embeddedMetadata };
}

std::optional<PredecodedImage> PredecodeCache::findCandidate(
    const DisplayedImageLocation& location) const
{
    if (!normalizedValidImageUrl(location.imageUrl()).has_value()) {
        return std::nullopt;
    }

    ConstCachedImageIterator cached = m_images.cend();
    if (m_currentDisplayedKeys.has_value()) {
        const auto currentKey = std::ranges::find_if(*m_currentDisplayedKeys,
            [&location](const PredecodeImageKey& key) { return key.location == location; });
        if (currentKey != m_currentDisplayedKeys->end()) {
            cached = findCachedImage(*currentKey);
        }
    }
    if (cached == m_images.cend()) {
        cached = findCachedImage(location);
    }
    if (cached == m_images.cend()) {
        return std::nullopt;
    }
    cached->lastUsedSequence = nextLastUsedSequence();

    return PredecodedImage { cached->displayImage, cached->key.location,
        cached->displayImage.embeddedMetadata };
}

void PredecodeCache::cacheImage(const DisplayedImageLocation& location,
    StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata)
{
    storeImage(location, std::move(displayImage), std::move(metadata), false);
}

void PredecodeCache::storeImage(const DisplayedImageLocation& location,
    StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata,
    bool retainsDisplayOutputAdmission)
{
    if (!displayImage.isAuthoritative()) {
        qCDebug(kiriviewPredecodeLog) << "predecode cache store skipped"
                                      << "reason"
                                      << "non-authoritative-payload"
                                      << "url" << diagnosticSourceReference(location.imageUrl());
        return;
    }

    if (!metadata.isEmpty()) {
        displayImage.embeddedMetadata = std::move(metadata);
    }

    const std::optional<qsizetype> byteCost = displayImage.byteCostWithinBudget(m_byteBudget);
    if (!byteCost.has_value()) {
        qCDebug(kiriviewPredecodeLog)
            << "predecode cache store skipped"
            << "reason"
            << "byte-budget"
            << "url" << diagnosticSourceReference(location.imageUrl()) << "byteCost"
            << displayImage.byteCost() << "budget" << m_byteBudget;
        return;
    }

    if (!normalizedValidImageUrl(location.imageUrl()).has_value()) {
        qCDebug(kiriviewPredecodeLog) << "predecode cache store skipped"
                                      << "reason"
                                      << "empty-location";
        return;
    }

    const PredecodeImageKey key { location, displayImage.sourceRevision };
    removeCachedImage(key);
    m_images.push_back(CachedImage { key, std::move(displayImage), *byteCost,
        nextLastUsedSequence(), retainsDisplayOutputAdmission });
    qCDebug(kiriviewPredecodeLog) << "predecode cache stored"
                                  << "url" << diagnosticSourceReference(location.imageUrl())
                                  << "byteCost" << *byteCost << "cachedImages" << m_images.size();

    trimImagesToBudget();
}

void PredecodeCache::cacheDisplayedImage(bool cacheable, const DisplayedImageLocation& location,
    StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata,
    bool retainsDisplayOutputAdmission)
{
    if (!cacheable || !normalizedValidImageUrl(location.imageUrl()).has_value()) {
        qCDebug(kiriviewPredecodeLog)
            << "displayed predecode cache skipped"
            << "reason" << (!cacheable ? "not-cacheable" : "empty-location") << "url"
            << diagnosticSourceReference(location.imageUrl());
        return;
    }

    storeImage(
        location, std::move(displayImage), std::move(metadata), retainsDisplayOutputAdmission);
}

bool PredecodeCache::containsKey(
    const std::vector<PredecodeImageKey>& keys, const PredecodeImageKey& key)
{
    return std::ranges::contains(keys, key);
}

PredecodeCache::ConstCachedImageIterator PredecodeCache::findCachedImage(
    const DisplayedImageLocation& location) const
{
    auto cached = m_images.cend();
    while (cached != m_images.cbegin()) {
        --cached;
        if (cached->key.location == location) {
            return cached;
        }
    }
    return m_images.cend();
}

PredecodeCache::CachedImageIterator PredecodeCache::findCachedImage(const PredecodeImageKey& key)
{
    return std::ranges::find_if(
        m_images, [&key](const CachedImage& entry) { return entry.key == key; });
}

PredecodeCache::ConstCachedImageIterator PredecodeCache::findCachedImage(
    const PredecodeImageKey& key) const
{
    return std::ranges::find_if(
        m_images, [&key](const CachedImage& entry) { return entry.key == key; });
}

void PredecodeCache::removeCachedImage(const PredecodeImageKey& key)
{
    const auto cached = findCachedImage(key);
    if (cached != m_images.end()) {
        m_images.erase(cached);
    }
}

std::size_t PredecodeCache::windowPriority(const DisplayedImageLocation& location) const
{
    const auto priorityEntry = std::ranges::find_if(m_windowKeys,
        [&location](const PredecodeImageKey& key) { return key.location == location; });
    if (priorityEntry == m_windowKeys.cend()) {
        return m_windowKeys.size();
    }

    return static_cast<std::size_t>(std::ranges::distance(m_windowKeys.cbegin(), priorityEntry));
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
        const bool exactCurrentKey = !m_currentDisplayedKeys.has_value()
            || containsKey(*m_currentDisplayedKeys, entry.key);
        const bool currentDisplayed
            = m_displayedHistory.currentContains(entry.key.location) && exactCurrentKey;
        states.push_back(PredecodeCachedImageState {
            currentDisplayed,
            m_displayedHistory.recentContains(entry.key.location),
            m_displayedHistory.currentPriority(entry.key.location),
            m_displayedHistory.recentPriority(entry.key.location),
            windowPriority(entry.key.location),
            entry.byteCost,
            entry.lastUsedSequence,
        });
    }

    const std::vector<std::size_t> retainedIndices
        = predecodeRetainedCachedImageIndices(states, m_windowKeys.size(), m_byteBudget);

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
