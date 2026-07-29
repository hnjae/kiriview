// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessiondirectimagecursorsync.h"

#include "location/sourcekey.h"

namespace {
bool sameRequestedSource(const QUrl& left, const QUrl& right)
{
    return kiriview::sameSourceKey(
        kiriview::sourceKeyForUrl(left), kiriview::sourceKeyForUrl(right));
}
}

namespace kiriview {
DocumentSessionDirectImageCursorSyncPlan documentSessionDirectImageCursorSyncPlan(
    const DocumentSessionDirectImageCursorSyncInput& input)
{
    if (input.documentKind != DocumentSessionKind::Image) {
        return {};
    }

    const QUrl pendingUrl = input.cursor.pendingSource.requestedUrl();
    const QUrl displayedUrl = input.image.displayedUrl;
    if (!pendingUrl.isEmpty()) {
        if (input.image.ordinaryDirectMediaScopeActive
            && sameRequestedSource(displayedUrl, pendingUrl)) {
            return DocumentSessionDirectImageCursorSyncPlan {
                DocumentSessionDirectImageCursorSyncOperation::ConfirmDirectImageCursor,
                displayedUrl,
            };
        }

        if (input.image.error) {
            if (sameRequestedSource(input.image.sourceUrl, pendingUrl)) {
                return DocumentSessionDirectImageCursorSyncPlan {
                    DocumentSessionDirectImageCursorSyncOperation::ConfirmDirectImageCursor,
                    pendingUrl,
                };
            }
            return DocumentSessionDirectImageCursorSyncPlan {
                DocumentSessionDirectImageCursorSyncOperation::RestoreDirectImageCursorAfterFailure,
                QUrl(),
            };
        }

        if (!input.image.sourceUrl.isEmpty()
            && !sameRequestedSource(input.image.sourceUrl, pendingUrl)) {
            return DocumentSessionDirectImageCursorSyncPlan {
                DocumentSessionDirectImageCursorSyncOperation::RestoreDirectImageCursorAfterFailure,
                QUrl(),
            };
        }
        return {};
    }

    if (input.image.ordinaryDirectMediaScopeActive && !displayedUrl.isEmpty()) {
        return DocumentSessionDirectImageCursorSyncPlan {
            DocumentSessionDirectImageCursorSyncOperation::ConfirmDirectImageCursor,
            displayedUrl,
        };
    }

    if (input.image.error) {
        return DocumentSessionDirectImageCursorSyncPlan {
            DocumentSessionDirectImageCursorSyncOperation::RestoreDirectImageCursorAfterFailure,
            QUrl(),
        };
    }

    return {};
}
}
