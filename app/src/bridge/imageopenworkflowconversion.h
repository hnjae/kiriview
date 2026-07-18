// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEOPENWORKFLOWCONVERSION_H
#define KIRIVIEW_IMAGEOPENWORKFLOWCONVERSION_H

#include "kiriview/src/policy/imageopenworkflow.cxx.h"

namespace kiriview {
namespace Bridge {
    enum class ImageDocumentSourceLoadKind {
        CurrentSource,
        SameScopeImageNavigation,
        ReplacementSource,
    };

    struct ImageDocumentSourceLoadPolicyInput
    {
        ImageDocumentSourceLoadKind loadKind = ImageDocumentSourceLoadKind::CurrentSource;
        bool preserveTwoPageSpreadTransition = false;
        bool rightToLeftReadingEnabled = false;
        bool sourceWithinDisplayedComicBookArchive = false;
        bool hasRequestedContainerNavigationUrl = false;
    };
}

RustImageDocumentSourceLoadPolicyInput rustImageDocumentSourceLoadPolicyInput(
    Bridge::ImageDocumentSourceLoadPolicyInput input);

RustImageOpenWorkflowEvent rustImageOpenWorkflowEvent(RustImageOpenWorkflowEventKind kind);
RustImageOpenWorkflowEvent rustBeginSourceLoadEvent(
    bool hasImage, bool hasLoadingContainerNavigationTarget);
RustImageOpenWorkflowEvent rustSuccessfulImageLoadEvent(bool hasRequestContainerNavigationTarget);
RustImageOpenWorkflowEvent rustSourceLoadErrorEvent(bool hasContainerNavigationTarget);
}

#endif
