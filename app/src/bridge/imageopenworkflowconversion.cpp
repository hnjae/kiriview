// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageopenworkflowconversion.h"

namespace {
kiriview::RustImageDocumentSourceLoadKind rustSourceLoadKind(
    kiriview::Bridge::ImageDocumentSourceLoadKind loadKind)
{
    switch (loadKind) {
    case kiriview::Bridge::ImageDocumentSourceLoadKind::CurrentSource:
        return kiriview::RustImageDocumentSourceLoadKind::CurrentSource;
    case kiriview::Bridge::ImageDocumentSourceLoadKind::SameScopeImageNavigation:
        return kiriview::RustImageDocumentSourceLoadKind::SameScopeImageNavigation;
    case kiriview::Bridge::ImageDocumentSourceLoadKind::ReplacementSource:
        return kiriview::RustImageDocumentSourceLoadKind::ReplacementSource;
    }

    return kiriview::RustImageDocumentSourceLoadKind::CurrentSource;
}

}

namespace kiriview {
RustImageDocumentSourceLoadPolicyInput rustImageDocumentSourceLoadPolicyInput(
    Bridge::ImageDocumentSourceLoadPolicyInput input)
{
    return RustImageDocumentSourceLoadPolicyInput {
        rustSourceLoadKind(input.loadKind),
        input.preserveTwoPageSpreadTransition,
        input.rightToLeftReadingEnabled,
        input.sourceWithinDisplayedComicBookArchive,
        input.hasRequestedContainerNavigationUrl,
    };
}

RustImageOpenWorkflowEvent rustImageOpenWorkflowEvent(RustImageOpenWorkflowEventKind kind)
{
    RustImageOpenWorkflowEvent event {};
    event.kind = kind;
    return event;
}

RustImageOpenWorkflowEvent rustBeginSourceLoadEvent(
    bool hasImage, bool hasLoadingContainerNavigationTarget)
{
    RustImageOpenWorkflowEvent event
        = rustImageOpenWorkflowEvent(RustImageOpenWorkflowEventKind::BeginSourceLoad);
    event.begin_source_load.has_image = hasImage;
    event.begin_source_load.has_loading_container_navigation_target
        = hasLoadingContainerNavigationTarget;
    return event;
}

RustImageOpenWorkflowEvent rustSuccessfulImageLoadEvent(bool hasRequestContainerNavigationTarget)
{
    RustImageOpenWorkflowEvent event
        = rustImageOpenWorkflowEvent(RustImageOpenWorkflowEventKind::FinishSuccessfulImageLoad);
    event.successful_image_load.has_request_container_navigation_target
        = hasRequestContainerNavigationTarget;
    return event;
}

RustImageOpenWorkflowEvent rustSourceLoadErrorEvent(bool hasContainerNavigationTarget)
{
    RustImageOpenWorkflowEvent event
        = rustImageOpenWorkflowEvent(RustImageOpenWorkflowEventKind::FinishSourceLoadWithError);
    event.load_failure_route = hasContainerNavigationTarget
        ? RustImageOpenLoadFailureRoute::ContainerNavigation
        : RustImageOpenLoadFailureRoute::Source;
    return event;
}
}
