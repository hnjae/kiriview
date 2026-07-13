// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageloadplan.h"

#include "location/imagedocumentlocation.h"

#include <utility>

namespace kiriview {
OpenedCollectionScopeLoadPlan openedCollectionScopeLoadPlan(const ImageLoadRequest& request)
{
    if (const ResolvedNavigationSource* source = request.externalSource()) {
        const std::optional<OpenedCollectionScopeLocation> openedCollectionScope
            = openedCollectionScopeLocationForDirectlyOpenedLocalSource(*source);
        if (openedCollectionScope.has_value()) {
            return { *openedCollectionScope,
                OpenedCollectionScopeLoadEffect::LoadImageDocumentPageCandidates };
        }
        return { OpenedCollectionScopeLocation::none(),
            OpenedCollectionScopeLoadEffect::ReadImage };
    }

    const OpenedCollectionScopeLocation scope = request.openedCollectionScope();
    if (request.sameScopePageNavigation() && !scope.isEmpty()
        && sameNormalizedUrl(request.sourceUrl(), scope.fileUrl())) {
        return { scope, OpenedCollectionScopeLoadEffect::LoadImageDocumentPageCandidates };
    }

    return { scope, OpenedCollectionScopeLoadEffect::ReadImage };
}

ImageLoadPlan imageLoadPlan(
    quint64 id, ImageLoadRequest request, ImageFirstDisplayDecodeContext firstDisplayContext)
{
    OpenedCollectionScopeLoadPlan scopePlan = openedCollectionScopeLoadPlan(request);
    const ImageLoadStartEffect startEffect
        = scopePlan.effect == OpenedCollectionScopeLoadEffect::LoadImageDocumentPageCandidates
        ? ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates
        : ImageLoadStartEffect::DecodeImage;
    DisplayedImageLocation location;
    if (const ResolvedNavigationSource* source = request.externalSource()) {
        location
            = DisplayedImageLocation::fromResolvedSource(*source, scopePlan.openedCollectionScope);
    } else {
        location = DisplayedImageLocation::fromOpenedCollectionScope(
            request.sourceUrl(), scopePlan.openedCollectionScope);
    }

    return ImageLoadPlan {
        ImageLoadSession(id, std::move(request), std::move(location), firstDisplayContext),
        startEffect,
    };
}
}
