// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONMEDIADELETIONPLAN_H
#define KIRIVIEW_DOCUMENTSESSIONMEDIADELETIONPLAN_H

#include "navigation/directmedianavigationmodel.h"
#include "session/documentsessionrouteplan.h"
#include "session/documentsessiontypes.h"
#include "system/filedeletion.h"

#include <QUrl>
#include <optional>
#include <vector>

namespace kiriview {
struct DocumentSessionMediaDeletionFallbackPlan
{
    QUrl actualTargetUrl;
    std::optional<QUrl> preferredFallbackUrl;
    std::optional<QUrl> fallbackUrl;

    [[nodiscard]] bool hasTarget() const { return !actualTargetUrl.isEmpty(); }
};

struct DocumentSessionMediaDeletionStartPlan
{
    bool shouldStartDeletion = false;
    FileDeletionRequest request;
    DocumentSessionMediaDeletionFallbackPlan fallbackPlan;
};

struct DocumentSessionMediaDeletionCompletionPlan
{
    DocumentSessionRoutePlan routePlan;
    bool reportFailure = false;

    [[nodiscard]] bool hasRoutePlan() const
    {
        return !routePlan.mutations.empty() || routePlan.publishPublicProjection
            || !routePlan.followUpEffects.empty();
    }
};

DocumentSessionMediaDeletionStartPlan documentSessionMediaDeletionStartPlan(FileDeletionMode mode,
    std::vector<DirectMediaNavigationCandidate> candidates, const QUrl& actualTargetUrl,
    const QUrl& navigationIdentityUrl);
DocumentSessionMediaDeletionFallbackPlan documentSessionMediaDeletionFallbackPlan(
    std::vector<DirectMediaNavigationCandidate> candidates, const QUrl& actualTargetUrl,
    const QUrl& navigationIdentityUrl);
DocumentSessionMediaDeletionCompletionPlan documentSessionMediaDeletionCompletionPlan(
    DocumentSessionKind currentKind, const DocumentSessionMediaDeletionFallbackPlan& fallbackPlan,
    FileDeletionResult result);
}

#endif
