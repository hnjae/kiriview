// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodewindowplan.h"

#include "navigation/imagedocumentpagenavigationpolicy.h"

#include <optional>
#include <type_traits>
#include <utility>

namespace {
std::optional<kiriview::DisplayedImageLocation> predecodeWindowImageLocation(
    const QUrl& url, const kiriview::ImageDocumentPageCandidateListContext& context)
{
    return context.visit(
        [&url](const auto& source) -> std::optional<kiriview::DisplayedImageLocation> {
            using Source = std::decay_t<decltype(source)>;
            if constexpr (std::is_same_v<Source,
                              kiriview::ImageDocumentPageCandidateListSource::Directory>) {
                const kiriview::SourceKey parentKey
                    = kiriview::sourceKeyForUrl(source.directoryUrl);
                const std::optional<kiriview::DirectMediaPageScopeIdentity> identity
                    = kiriview::directMediaPageScopeIdentityForOwnerCandidate(url, parentKey);
                if (!identity.has_value()) {
                    return std::nullopt;
                }
                return kiriview::DisplayedImageLocation::fromDirectMediaPageScope(url, *identity);
            } else {
                return kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                    url, source.openedCollectionScope);
            }
        });
}

std::vector<kiriview::DisplayedImageLocation> predecodeWindowImageLocations(
    const std::vector<kiriview::ImageDocumentPageCandidate>& candidates,
    const std::vector<std::size_t>& indices,
    const kiriview::ImageDocumentPageCandidateListContext& context)
{
    std::vector<kiriview::DisplayedImageLocation> locations;
    locations.reserve(indices.size());
    for (std::size_t index : indices) {
        if (index < candidates.size() && imageDocumentPageCandidateIsImage(candidates.at(index))) {
            if (std::optional<kiriview::DisplayedImageLocation> location
                = predecodeWindowImageLocation(candidates.at(index).url, context)) {
                locations.push_back(std::move(*location));
            }
        }
    }
    return locations;
}

}

namespace kiriview {
bool PredecodeWindowStartPlan::shouldLoadCandidates() const { return candidateList.has_value(); }

PredecodeWindowStartPlan predecodeWindowStartPlan(const PredecodeWindowPlanRequest& request)
{
    const PredecodeSchedulePlan initialSchedule
        = predecodeSchedulePlan(0, std::nullopt, request.policyInput);
    const std::optional<ImageDocumentPageCandidateListContext> candidateContext
        = imageDocumentPageCandidateListContextForDisplayedImage(request.displayedLocation);

    PredecodeWindowStartPlan plan {
        PredecodeWindowPlan {
            {},
            initialSchedule.parallelLimit,
        },
        std::nullopt,
    };
    if (initialSchedule.parallelLimit > 0 && candidateContext.has_value()) {
        plan.candidateList
            = PredecodeCandidateListLoadPlan { *candidateContext, request.policyInput };
    }

    return plan;
}

PredecodeWindowPlan predecodeWindowPlanForCandidates(
    const PredecodeWindowStartPlan& plan, const std::vector<ImageDocumentPageCandidate>& candidates)
{
    if (!plan.candidateList.has_value()) {
        return plan.fallbackWindow;
    }

    const PredecodeSchedulePlan schedule = predecodeSchedulePlan(candidates.size(),
        imageDocumentPageCandidateIndex(candidates, plan.candidateList->context.currentUrl()),
        plan.candidateList->policyInput);
    return PredecodeWindowPlan {
        predecodeWindowImageLocations(
            candidates, schedule.targetIndices, plan.candidateList->context),
        schedule.parallelLimit,
    };
}

}
