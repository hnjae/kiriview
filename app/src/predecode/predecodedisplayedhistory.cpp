// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodedisplayedhistory.h"

#include "location/imageurl.h"

#include <algorithm>
#include <utility>

namespace {
constexpr std::size_t recentDisplayedCacheLimit = 4;
}

namespace kiriview {
void PredecodeDisplayedHistory::clear()
{
    m_currentLocations.clear();
    m_recentLocations.clear();
}

void PredecodeDisplayedHistory::setDisplayedLocations(
    const std::vector<DisplayedImageLocation>& locations)
{
    std::vector<DisplayedImageLocation> displayedLocations;

    for (const DisplayedImageLocation& location : locations) {
        if (!normalizedValidImageUrl(location.imageUrl()).has_value()
            || containsLocation(displayedLocations, location)) {
            continue;
        }

        displayedLocations.push_back(location);
    }

    for (const DisplayedImageLocation& location : m_currentLocations) {
        if (containsLocation(displayedLocations, location)) {
            continue;
        }

        removeLocation(m_recentLocations, location);
        m_recentLocations.insert(m_recentLocations.begin(), location);
    }

    for (const DisplayedImageLocation& location : displayedLocations) {
        removeLocation(m_recentLocations, location);
    }
    if (m_recentLocations.size() > recentDisplayedCacheLimit) {
        m_recentLocations.resize(recentDisplayedCacheLimit);
    }

    m_currentLocations = std::move(displayedLocations);
}

bool PredecodeDisplayedHistory::currentContains(const DisplayedImageLocation& location) const
{
    return normalizedValidImageUrl(location.imageUrl()).has_value()
        && containsLocation(m_currentLocations, location);
}

bool PredecodeDisplayedHistory::recentContains(const DisplayedImageLocation& location) const
{
    return normalizedValidImageUrl(location.imageUrl()).has_value()
        && containsLocation(m_recentLocations, location);
}

std::size_t PredecodeDisplayedHistory::currentPriority(const DisplayedImageLocation& location) const
{
    return normalizedValidImageUrl(location.imageUrl()).has_value()
        ? priority(m_currentLocations, location)
        : m_currentLocations.size();
}

std::size_t PredecodeDisplayedHistory::recentPriority(const DisplayedImageLocation& location) const
{
    return normalizedValidImageUrl(location.imageUrl()).has_value()
        ? priority(m_recentLocations, location)
        : m_recentLocations.size();
}

bool PredecodeDisplayedHistory::containsLocation(
    const std::vector<DisplayedImageLocation>& locations, const DisplayedImageLocation& location)
{
    return std::ranges::contains(locations, location);
}

void PredecodeDisplayedHistory::removeLocation(
    std::vector<DisplayedImageLocation>& locations, const DisplayedImageLocation& location)
{
    std::erase(locations, location);
}

std::size_t PredecodeDisplayedHistory::priority(
    const std::vector<DisplayedImageLocation>& locations, const DisplayedImageLocation& location)
{
    const auto priorityEntry = std::ranges::find(locations, location);
    if (priorityEntry == locations.cend()) {
        return locations.size();
    }

    return static_cast<std::size_t>(std::ranges::distance(locations.cbegin(), priorityEntry));
}
}
