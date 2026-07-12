// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTSOURCELOADREQUEST_H
#define KIRIVIEW_IMAGEDOCUMENTSOURCELOADREQUEST_H

#include "location/imageurl.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

#include <QUrl>
#include <optional>

namespace kiriview {
struct ImageDocumentSourceLoadRequest
{
    QUrl sourceUrl;
    ImageDocumentPageKind sourceKind = ImageDocumentPageKind::Image;
    QUrl containerNavigationUrl;
    bool preserveTwoPageSpreadTransition = false;
    bool sameScopeImageNavigation = false;
    std::optional<ResolvedNavigationSource> resolvedSource;

    static ImageDocumentSourceLoadRequest fromUrl(const QUrl& sourceUrl)
    {
        return fromTarget(ImageDocumentPageTarget { sourceUrl, ImageDocumentPageKind::Image });
    }

    static ImageDocumentSourceLoadRequest fromSource(const ResolvedNavigationSource& source)
    {
        ImageDocumentSourceLoadRequest request = fromUrl(source.requestedUrl());
        request.resolvedSource = source;
        return request;
    }

    static ImageDocumentSourceLoadRequest fromTarget(const ImageDocumentPageTarget& target)
    {
        return ImageDocumentSourceLoadRequest { target.url, target.kind, QUrl(), false, false,
            std::nullopt };
    }

    static ImageDocumentSourceLoadRequest fromContainerImage(
        const QUrl& imageUrl, const QUrl& containerUrl)
    {
        return fromContainerTarget(
            ImageDocumentPageTarget { imageUrl, ImageDocumentPageKind::Image }, containerUrl);
    }

    static ImageDocumentSourceLoadRequest fromContainerTarget(
        const ImageDocumentPageTarget& target, const QUrl& containerUrl)
    {
        return ImageDocumentSourceLoadRequest { target.url, target.kind, containerUrl, false, false,
            std::nullopt };
    }

    static ImageDocumentSourceLoadRequest fromPageNavigation(
        const QUrl& sourceUrl, bool preserveTwoPageSpreadTransition)
    {
        return fromPageNavigationTarget(
            ImageDocumentPageTarget { sourceUrl, ImageDocumentPageKind::Image },
            preserveTwoPageSpreadTransition);
    }

    static ImageDocumentSourceLoadRequest fromPageNavigationTarget(
        const ImageDocumentPageTarget& target, bool preserveTwoPageSpreadTransition)
    {
        return ImageDocumentSourceLoadRequest {
            target.url,
            target.kind,
            QUrl(),
            preserveTwoPageSpreadTransition,
            true,
            std::nullopt,
        };
    }

    static ImageDocumentSourceLoadRequest fromSameScopeImageNavigationUrl(const QUrl& sourceUrl)
    {
        return fromPageNavigation(sourceUrl, true);
    }

    static ImageDocumentSourceLoadRequest fromSameScopeImageNavigationSource(
        const ResolvedNavigationSource& source)
    {
        ImageDocumentSourceLoadRequest request
            = fromSameScopeImageNavigationUrl(source.requestedUrl());
        request.resolvedSource = source;
        return request;
    }
};
}

#endif
