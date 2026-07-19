// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTNAVIGATIONCONTROLLER_H
#define KIRIVIEW_IMAGEDOCUMENTNAVIGATIONCONTROLLER_H

#include "imagedocumentruntimeplan.h"
#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

#include <functional>

namespace kiriview {
class ImageDocumentState;
class ImageDocumentPageNavigationService;
class ImageSpreadPresentationController;

class ImageDocumentNavigationController final
{
public:
    using RuntimeTransactionCallback = std::function<void(ImageDocumentRuntimeTransaction)>;

    ImageDocumentNavigationController(ImageDocumentState& state,
        ImageDocumentPageNavigationService& navigationService,
        ImageSpreadPresentationController& spreadController,
        RuntimeTransactionCallback runtimeTransactionCallback);

    int currentPageNumber() const;
    int pageCount() const;
    ImageDocumentPageNavigationSnapshot pageNavigationSnapshot() const;
    const ImageDocumentPageCandidateListSnapshot& confirmedPageCandidateSnapshot() const;

    void openAdjacentPage(NavigationDirection direction);
    void openAdjacentContainer(NavigationDirection direction);
    void openImageAtPage(int pageNumber);
    void openImageAtRelativePageOffset(int offset);

    void updatePageNavigation();
    void cancelNavigation();
    void cancelContainerNavigation();
    void cancelPageNavigationUpdate();
    void cancelAllNavigation();
    void clearPageNavigation();

private:
    ImageDocumentState& m_state;
    ImageDocumentPageNavigationService& m_navigationService;
    ImageSpreadPresentationController& m_spreadController;
    RuntimeTransactionCallback m_runtimeTransactionCallback;
};
}

#endif
