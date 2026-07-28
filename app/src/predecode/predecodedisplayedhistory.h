// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEDISPLAYEDHISTORY_H
#define KIRIVIEW_PREDECODEDISPLAYEDHISTORY_H

#include "location/imagelocation.h"

#include <cstddef>
#include <vector>

namespace kiriview {
class PredecodeDisplayedHistory
{
public:
    void clear();
    void setDisplayedLocations(const std::vector<DisplayedImageLocation>& locations);

    [[nodiscard]] bool currentContains(const DisplayedImageLocation& location) const;
    [[nodiscard]] bool recentContains(const DisplayedImageLocation& location) const;
    [[nodiscard]] std::size_t currentPriority(const DisplayedImageLocation& location) const;
    [[nodiscard]] std::size_t recentPriority(const DisplayedImageLocation& location) const;

private:
    static bool containsLocation(const std::vector<DisplayedImageLocation>& locations,
        const DisplayedImageLocation& location);
    static void removeLocation(
        std::vector<DisplayedImageLocation>& locations, const DisplayedImageLocation& location);
    static std::size_t priority(const std::vector<DisplayedImageLocation>& locations,
        const DisplayedImageLocation& location);

    std::vector<DisplayedImageLocation> m_currentLocations;
    std::vector<DisplayedImageLocation> m_recentLocations;
};
}

#endif
