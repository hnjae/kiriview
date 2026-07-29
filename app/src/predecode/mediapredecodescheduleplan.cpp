// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediapredecodescheduleplan.h"

#include "location/imageurl.h"

#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
namespace {
    struct MediaPredecodeCurrent
    {
        DisplayedImageLocation location;
        DirectMediaPageScopeIdentity identity;
    };

    std::optional<MediaPredecodeCurrent> mediaPredecodeCurrent(
        const MediaPredecodeScheduleRequest& request, const DirectMediaScope& candidateOwnerScope)
    {
        if (!request.activeScope.has_value()) {
            return std::nullopt;
        }
        if (!request.immediate) {
            return MediaPredecodeCurrent {
                DisplayedImageLocation::fromResolvedSource(request.activeScope->source()),
                candidateOwnerScope.pageScopeIdentity(),
            };
        }

        const std::optional<QUrl> normalizedCurrentUrl
            = normalizedValidUrlForIdentity(request.currentUrl);
        if (!normalizedCurrentUrl.has_value()) {
            return std::nullopt;
        }

        const SourceKey selectedTargetKey = sourceKeyForUrl(*normalizedCurrentUrl);
        if (sameNormalizedUrl(*normalizedCurrentUrl, request.activeScope->currentUrl())
            || sameSourceKey(selectedTargetKey, request.activeScope->currentKey())) {
            return MediaPredecodeCurrent {
                DisplayedImageLocation::fromResolvedSource(request.activeScope->source()),
                candidateOwnerScope.pageScopeIdentity(),
            };
        }

        const std::optional<DirectMediaPageScopeIdentity> targetIdentity
            = directMediaPageScopeIdentityForOwnerCandidate(
                *normalizedCurrentUrl, candidateOwnerScope.parentKey());
        if (!targetIdentity.has_value()) {
            return std::nullopt;
        }
        return MediaPredecodeCurrent {
            DisplayedImageLocation::fromDirectMediaPageScope(
                *normalizedCurrentUrl, *targetIdentity),
            *targetIdentity,
        };
    }
}

bool MediaPredecodeSchedulePlan::shouldSchedule() const
{
    return !context.currentLocation.isEmpty();
}

MediaPredecodeSchedulePlan mediaPredecodeSchedulePlan(MediaPredecodeScheduleRequest request)
{
    if (!request.activeScope.has_value() || !request.candidateSnapshot.known
        || !request.candidateSnapshot.source.has_value()
        || *request.activeScope != *request.candidateSnapshot.source) {
        return {};
    }
    const std::optional<MediaPredecodeCurrent> current
        = mediaPredecodeCurrent(request, *request.candidateSnapshot.source);
    if (!current.has_value()) {
        return {};
    }

    MediaPredecodeEligibilitySnapshot eligibility = mediaPredecodeEligibilitySnapshot(
        directMediaNavigationCandidateRows(request.candidateSnapshot), current->identity);
    if (request.immediate && !eligibility.currentMediaIndex.has_value()) {
        return {};
    }
    auto payload = std::make_shared<MediaPredecodeSchedulePayload>();
    payload->directMediaNavigationCandidateSnapshot = std::move(request.candidateSnapshot);
    payload->eligibleImages = std::move(eligibility);
    PredecodeScheduleContext context {
        current->location,
        std::move(request.displayedImages),
        request.firstDisplayContext,
        payload->eligibleImages.currentMediaIndex.has_value()
            ? static_cast<int>(*payload->eligibleImages.currentMediaIndex)
            : -1,
        std::move(payload),
        request.immediate,
        ImageDocumentPageCandidateListSnapshot {},
    };
    return MediaPredecodeSchedulePlan { std::move(context) };
}

const DirectMediaNavigationCandidateSnapshot* mediaPredecodeScheduleCandidateSnapshot(
    const PredecodePendingSchedule& schedule)
{
    const auto* payload = predecodeSchedulePayload<MediaPredecodeSchedulePayload>(schedule);
    return payload != nullptr ? &payload->directMediaNavigationCandidateSnapshot : nullptr;
}

const MediaPredecodeEligibilitySnapshot* mediaPredecodeScheduleEligibility(
    const PredecodePendingSchedule& schedule)
{
    const auto* payload = predecodeSchedulePayload<MediaPredecodeSchedulePayload>(schedule);
    return payload != nullptr ? &payload->eligibleImages : nullptr;
}
}
