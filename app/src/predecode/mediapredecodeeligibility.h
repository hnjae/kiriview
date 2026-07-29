// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAPREDECODEELIGIBILITY_H
#define KIRIVIEW_MEDIAPREDECODEELIGIBILITY_H

#include "location/imagelocation.h"
#include "navigation/directmedianavigationmodel.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace kiriview {
struct MediaPredecodeEligibleImage
{
    DisplayedImageLocation location;
    std::size_t mediaIndex = 0;
};

struct MediaPredecodeEligibilitySnapshot
{
    std::size_t directMediaNavigationCandidateCount = 0;
    std::optional<std::size_t> currentMediaIndex;
    std::vector<MediaPredecodeEligibleImage> images;
};

MediaPredecodeEligibilitySnapshot mediaPredecodeEligibilitySnapshot(
    const std::vector<DirectMediaNavigationCandidate>& candidates,
    const DirectMediaPageScopeIdentity& currentIdentity);
std::vector<DisplayedImageLocation> mediaPredecodeEligibleLocationsForTargetIndices(
    const MediaPredecodeEligibilitySnapshot& snapshot, const std::vector<std::size_t>& indices);
}

#endif
