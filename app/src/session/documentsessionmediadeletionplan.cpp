// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionmediadeletionplan.h"

#include "location/imageurl.h"

#include <optional>
#include <utility>

namespace {
void appendDeletedDirectMediaNavigationCandidate(
    std::vector<kiriview::DirectMediaNavigationCandidate>* candidates, const QUrl& currentUrl)
{
    candidates->push_back(kiriview::DirectMediaNavigationCandidate {
        currentUrl, kiriview::userVisibleFileNameForUrl(currentUrl) });
}

std::optional<QUrl> preferredMediaDeletionFallback(
    const kiriview::DocumentSessionMediaDeletionFallbackPlan& fallbackPlan)
{
    if (fallbackPlan.preferredFallbackUrl.has_value()) {
        return fallbackPlan.preferredFallbackUrl;
    }

    return fallbackPlan.fallbackUrl;
}
}

namespace kiriview {
DocumentSessionMediaDeletionStartPlan documentSessionMediaDeletionStartPlan(FileDeletionMode mode,
    std::vector<DirectMediaNavigationCandidate> candidates, const QUrl& actualTargetUrl,
    const QUrl& navigationIdentityUrl)
{
    const DocumentSessionMediaDeletionFallbackPlan fallbackPlan
        = documentSessionMediaDeletionFallbackPlan(
            std::move(candidates), actualTargetUrl, navigationIdentityUrl);
    if (!fallbackPlan.hasTarget()) {
        return {};
    }

    return DocumentSessionMediaDeletionStartPlan {
        true,
        FileDeletionRequest { fallbackPlan.actualTargetUrl, mode },
        fallbackPlan,
    };
}

DocumentSessionMediaDeletionFallbackPlan documentSessionMediaDeletionFallbackPlan(
    std::vector<DirectMediaNavigationCandidate> candidates, const QUrl& actualTargetUrl,
    const QUrl& navigationIdentityUrl)
{
    const QUrl& identityUrl = navigationIdentityUrl;
    if (identityUrl.isEmpty()) {
        return {};
    }

    if (!directMediaNavigationCandidateIndex(candidates, identityUrl).has_value()) {
        appendDeletedDirectMediaNavigationCandidate(&candidates, identityUrl);
        sortDirectMediaNavigationCandidates(&candidates);
    }

    return DocumentSessionMediaDeletionFallbackPlan {
        actualTargetUrl,
        adjacentDirectMediaNavigationUrl(candidates, identityUrl, NavigationDirection::Next),
        adjacentDirectMediaNavigationUrl(candidates, identityUrl, NavigationDirection::Previous),
    };
}

DocumentSessionMediaDeletionCompletionPlan documentSessionMediaDeletionCompletionPlan(
    DocumentSessionKind currentKind, const DocumentSessionMediaDeletionFallbackPlan& fallbackPlan,
    FileDeletionResult result)
{
    switch (fileDeletionCompletionAction(result)) {
    case FileDeletionCompletionAction::ClearDeletedTargetAndOpenFallback: {
        return DocumentSessionMediaDeletionCompletionPlan {
            documentSessionRoutePlanAfterMediaDeletion(
                currentKind, preferredMediaDeletionFallback(fallbackPlan)),
            false,
        };
    }
    case FileDeletionCompletionAction::Ignore:
        return {};
    case FileDeletionCompletionAction::ReportFailure:
        return DocumentSessionMediaDeletionCompletionPlan { {}, true };
    }

    return DocumentSessionMediaDeletionCompletionPlan { {}, true };
}
}
