// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGESHORTCUTNAVIGATIONPOLICY_H
#define KIRIVIEW_IMAGESHORTCUTNAVIGATIONPOLICY_H

namespace kiriview {
class ImageShortcutNavigationPolicy final
{
public:
    enum class HorizontalArrowAction {
        PanLeft = 0,
        PanRight,
        RequestPreviousActiveNavigation,
        RequestNextActiveNavigation,
    };

    enum class SinglePageArrowAction {
        OpenPreviousSinglePage = 0,
        OpenNextSinglePage,
    };

    enum class ScanAction {
        NoScanAction = 0,
        RequestPreviousActiveNavigationFromScan,
        RequestNextActiveNavigationFromScan,
        OpenPreviousPageFromFinalScanStart,
        ShowFirstImageBoundary,
    };

    [[nodiscard]] ImageShortcutNavigationPolicy::HorizontalArrowAction horizontalArrowAction(
        bool leftArrow, bool horizontallyPannable, bool rightToLeftReadingActive) const;
    [[nodiscard]] ImageShortcutNavigationPolicy::SinglePageArrowAction singlePageArrowAction(
        bool leftArrow, bool rightToLeftReadingActive) const;
    [[nodiscard]] ImageShortcutNavigationPolicy::ScanAction scanForwardAction(
        bool imagePannable, bool viewportMoved) const;
    [[nodiscard]] ImageShortcutNavigationPolicy::ScanAction scanBackwardAction(bool imagePannable,
        bool viewportMoved, bool imageDocumentPageNavigationActive,
        bool atKnownFirstActiveNavigation, bool canOpenPreviousActiveNavigation) const;
};
}

#endif
