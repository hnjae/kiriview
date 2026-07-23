// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESPREADSECONDARYPAGEREFRESH_H
#define KIRIVIEW_IMAGESPREADSECONDARYPAGEREFRESH_H

#include "navigation/imagedocumentpagenavigationtypes.h"
#include "presentation/imagespreadnavigation.h"
#include "presentation/imagespreadpagecache.h"

#include <QSize>
#include <QUrl>
#include <optional>

namespace kiriview {
enum class ImageSpreadSecondaryPageRefreshAction {
    PrimaryOnly,
    KeepCurrentSecondary,
    LoadTarget,
};

struct ImageSpreadSecondaryPageRefreshRequest
{
    bool twoPageModeActive = false;
    bool primaryPageIsWide = false;
    bool secondaryPageVisible = false;
    QUrl currentSecondaryUrl;
    ImageDocumentPageNavigationSnapshot navigation;
};

struct ImageSpreadSecondaryPageRefreshResult
{
    ImageSpreadSecondaryPageRefreshAction action
        = ImageSpreadSecondaryPageRefreshAction::PrimaryOnly;
    QUrl targetUrl;
};

struct ImageSpreadPageNavigationContext
{
    bool twoPageModeActive = false;
    bool secondaryPageVisible = false;
    ImageDocumentPageNavigationSnapshot navigation;
};

class ImageSpreadSecondaryPageRefresh final
{
public:
    void cachePageSize(const QUrl& url, QSize imageSize);
    [[nodiscard]] std::optional<bool> cachedPageIsWide(const QUrl& url) const;

    [[nodiscard]] ImageSpreadSecondaryPageRefreshResult planRefresh(
        const ImageSpreadSecondaryPageRefreshRequest& request) const;
    [[nodiscard]] int currentLastPageNumber(const ImageSpreadPageNavigationContext& context) const;
    [[nodiscard]] ImageDocumentPageActiveNavigationSnapshot activeNavigationSnapshot(
        const ImageSpreadPageNavigationContext& context) const;
    [[nodiscard]] ImageSpreadPageNavigationTarget pageNavigationTarget(
        NavigationDirection direction, const ImageSpreadPageNavigationContext& context) const;
    [[nodiscard]] int relativePageNavigationTarget(
        int offset, const ImageSpreadPageNavigationContext& context) const;
    [[nodiscard]] bool primarySelectionMatchesDisplayed(
        const ImageDocumentPageNavigationSnapshot& navigation, const QUrl& displayedUrl) const;

private:
    [[nodiscard]] ImageSpreadNavigationState navigationState(
        const ImageSpreadPageNavigationContext& context, bool previousPageIsWide = false) const;
    [[nodiscard]] bool previousPageIsWideForNavigation(
        const ImageSpreadPageNavigationContext& context) const;
    ImageSpreadPageCache m_pageCache;
};
}

#endif
