// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONROUTEPLAN_H
#define KIRIVIEW_DOCUMENTSESSIONROUTEPLAN_H

#include "session/documentsessiontypes.h"

#include <QUrl>
#include <optional>
#include <variant>
#include <vector>

namespace kiriview {
enum class DocumentSessionRouteKind {
    Empty,
    DirectVideo,
    DirectImage,
    ImageDocument,
};

struct ClearSessionErrorStringRouteOperation
{
};

struct CancelDirectMediaNavigationRouteOperation
{
};

struct CancelMediaDeletionRouteOperation
{
};

struct ClearDirectMediaNavigationRouteOperation
{
};

struct ClearDirectMediaCursorRouteOperation
{
};

struct SetDirectVideoCursorRouteOperation
{
};

struct RequestDirectImageCursorRouteOperation
{
};

struct ClearThenRequestDirectImageCursorRouteOperation
{
};

struct ClearImageDocumentRouteOperation
{
};

struct LeaveVideoModeRouteOperation
{
};

struct EnterEmptyDocumentRouteOperation
{
};

struct EnterImageDocumentRouteOperation
{
};

struct EnterImageDocumentSameScopeNavigationRouteOperation
{
};

struct EnterVideoDocumentRouteOperation
{
};

struct SyncDirectImageCursorFromDocumentRouteOperation
{
};

struct ClearSourceIdentityRouteOperation
{
};

struct UseOriginalSourceIdentityRouteOperation
{
};

struct UseImageDocumentSourceIdentityRouteOperation
{
};

using DocumentSessionRouteMutation
    = std::variant<ClearSessionErrorStringRouteOperation, CancelDirectMediaNavigationRouteOperation,
        CancelMediaDeletionRouteOperation, ClearDirectMediaNavigationRouteOperation,
        ClearDirectMediaCursorRouteOperation, SetDirectVideoCursorRouteOperation,
        RequestDirectImageCursorRouteOperation, ClearThenRequestDirectImageCursorRouteOperation,
        ClearImageDocumentRouteOperation, LeaveVideoModeRouteOperation,
        EnterEmptyDocumentRouteOperation, EnterImageDocumentRouteOperation,
        EnterImageDocumentSameScopeNavigationRouteOperation, EnterVideoDocumentRouteOperation,
        SyncDirectImageCursorFromDocumentRouteOperation, ClearSourceIdentityRouteOperation,
        UseOriginalSourceIdentityRouteOperation, UseImageDocumentSourceIdentityRouteOperation>;

struct RefreshDirectMediaNavigationAfterRoutingRouteEffect
{
};

struct ClearMediaPredecodeRouteEffect
{
};

using DocumentSessionRouteFollowUpEffect
    = std::variant<RefreshDirectMediaNavigationAfterRoutingRouteEffect,
        ClearMediaPredecodeRouteEffect>;

struct DocumentSessionRoutePlan
{
    DocumentSessionRouteKind kind = DocumentSessionRouteKind::Empty;
    QUrl sourceUrl;
    std::vector<DocumentSessionRouteMutation> mutations;
    bool publishPublicProjection = false;
    std::vector<DocumentSessionRouteFollowUpEffect> followUpEffects;
};

DocumentSessionRoutePlan documentSessionRoutePlanForSourceUrl(
    const QUrl& sourceUrl, DocumentSessionKind currentKind);
DocumentSessionRoutePlan documentSessionRoutePlanForMediaUrl(
    const QUrl& url, DocumentSessionKind currentKind);
DocumentSessionRoutePlan documentSessionRoutePlanAfterMediaDeletion(
    DocumentSessionKind deletedKind, std::optional<QUrl> fallbackUrl);
}

#endif
