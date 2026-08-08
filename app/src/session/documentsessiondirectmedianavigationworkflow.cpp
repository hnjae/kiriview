// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessiondirectmedianavigationworkflow.h"

#include "location/imageurl.h"

#include <utility>

namespace {
bool confirmedBoundaryState(kiriview::DirectMediaNavigationBoundaryState boundaryState)
{
    return boundaryState.currentNumber > 0 && boundaryState.count > 0
        && boundaryState.currentNumber <= boundaryState.count;
}

bool refreshSelectionChanged(kiriview::ActiveNavigationSourceKind sourceKind,
    kiriview::ActiveNavigationSnapshot previousSnapshot,
    kiriview::DirectMediaNavigationBoundaryState boundaryState)
{
    return sourceKind != kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia
        || !previousSnapshot.known || previousSnapshot.currentNumber != boundaryState.currentNumber
        || previousSnapshot.count != boundaryState.count;
}
}

namespace kiriview {
DocumentSessionDirectMediaNavigationRefreshApplication
documentSessionDirectMediaNavigationRefreshApplication(ActiveNavigationSourceKind sourceKind,
    ActiveNavigationSnapshot previousSnapshot,
    DocumentSessionDirectMediaNavigationRefreshResult result)
{
    if (!result.succeeded || !confirmedBoundaryState(result.boundaryState)) {
        return DocumentSessionDirectMediaNavigationRefreshApplication {
            {},
            false,
            {},
            DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync,
            false,
        };
    }

    const bool selectionChanged
        = refreshSelectionChanged(sourceKind, previousSnapshot, result.boundaryState);
    return DocumentSessionDirectMediaNavigationRefreshApplication {
        result.boundaryState,
        true,
        std::move(result.candidates),
        selectionChanged
            ? DocumentSessionDirectMediaNavigationRevealAction::UsePendingOrProgrammaticSync
            : DocumentSessionDirectMediaNavigationRevealAction::None,
        true,
    };
}

DocumentSessionDirectMediaNavigationOpenApplication
documentSessionDirectMediaNavigationOpenApplication(
    const QUrl& activeDirectMediaCursorUrl, DocumentSessionDirectMediaNavigationOpenResult result)
{
    if (!result.succeeded || !confirmedBoundaryState(result.plan.boundaryState)) {
        return DocumentSessionDirectMediaNavigationOpenApplication {
            {},
            false,
            {},
            DocumentSessionDirectMediaNavigationRevealAction::ProgrammaticSync,
            false,
            std::nullopt,
        };
    }

    const bool targetChangesMedia = result.plan.targetUrl.has_value()
        && !sameNormalizedUrl(*result.plan.targetUrl, activeDirectMediaCursorUrl);
    return DocumentSessionDirectMediaNavigationOpenApplication {
        result.plan.boundaryState,
        true,
        std::move(result.candidates),
        targetChangesMedia ? DocumentSessionDirectMediaNavigationRevealAction::
                                 UsePendingOrProgrammaticSyncAndKeepPending
                           : DocumentSessionDirectMediaNavigationRevealAction::Clear,
        true,
        std::move(result.plan.targetUrl),
    };
}
}
