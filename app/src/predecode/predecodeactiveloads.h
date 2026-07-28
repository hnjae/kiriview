// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEACTIVELOADS_H
#define KIRIVIEW_PREDECODEACTIVELOADS_H

#include "location/imagelocation.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace kiriview {
class PredecodeActiveLoads final
{
public:
    PredecodeActiveLoads() = default;

    static PredecodeActiveLoads fromLocations(const std::vector<DisplayedImageLocation>& locations)
    {
        PredecodeActiveLoads loads;
        loads.m_locations.reserve(locations.size());
        for (const DisplayedImageLocation& location : locations) {
            if (!normalizedValidImageUrl(location.imageUrl()).has_value()
                || loads.contains(location)) {
                continue;
            }

            loads.m_locations.push_back(location);
        }
        return loads;
    }

    [[nodiscard]] std::size_t size() const { return m_locations.size(); }

    [[nodiscard]] bool contains(const DisplayedImageLocation& location) const
    {
        return normalizedValidImageUrl(location.imageUrl()).has_value()
            && std::ranges::contains(m_locations, location);
    }

private:
    std::vector<DisplayedImageLocation> m_locations;
};
}

#endif
