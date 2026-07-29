// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediapredecodeeligibility.h"

#include "navigation/mediaformatregistry.h"

#include <algorithm>

namespace kiriview {
MediaPredecodeEligibilitySnapshot mediaPredecodeEligibilitySnapshot(
    const std::vector<DirectMediaNavigationCandidate>& candidates,
    const DirectMediaPageScopeIdentity& currentIdentity)
{
    MediaPredecodeEligibilitySnapshot snapshot {
        candidates.size(),
        std::nullopt,
        {},
    };
    snapshot.images.reserve(candidates.size());

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const DirectMediaNavigationCandidate& candidate = candidates.at(index);
        const std::optional<DirectMediaPageScopeIdentity> candidateIdentity
            = directMediaPageScopeIdentityForOwnerCandidate(
                candidate.url, currentIdentity.parentKey());
        if (!candidateIdentity.has_value()) {
            continue;
        }
        if (sameSourceKey(candidateIdentity->currentKey(), currentIdentity.currentKey())) {
            snapshot.currentMediaIndex = index;
        }
        if (isSupportedStillImageDirectMediaNavigationCandidate(candidate)) {
            snapshot.images.push_back(MediaPredecodeEligibleImage {
                DisplayedImageLocation::fromDirectMediaPageScope(candidate.url, *candidateIdentity),
                index,
            });
        }
    }

    return snapshot;
}

std::vector<DisplayedImageLocation> mediaPredecodeEligibleLocationsForTargetIndices(
    const MediaPredecodeEligibilitySnapshot& snapshot, const std::vector<std::size_t>& indices)
{
    std::vector<DisplayedImageLocation> locations;
    locations.reserve(indices.size());

    for (std::size_t index : indices) {
        const auto eligible = std::ranges::find_if(
            snapshot.images, [index](const MediaPredecodeEligibleImage& image) {
                return image.mediaIndex == index;
            });
        if (eligible != snapshot.images.cend()) {
            locations.push_back(eligible->location);
        }
    }

    return locations;
}
}
