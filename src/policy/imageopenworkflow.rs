// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#[path = "imageopenworkflow/source_load.rs"]
mod source_load;
#[path = "imageopenworkflow/transition.rs"]
mod transition;

#[cxx::bridge(namespace = "kiriview")]
mod ffi {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenBoolTarget {
        Unchanged = 0,
        False = 1,
        True = 2,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenStatusTarget {
        Unchanged = 0,
        Null = 1,
        Loading = 2,
        Ready = 3,
        Error = 4,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenErrorStringTarget {
        Unchanged = 0,
        Clear = 1,
        Provided = 2,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenUrlTarget {
        Unchanged = 0,
        Empty = 1,
        SessionImage = 2,
        SessionContainerNavigation = 3,
        DerivedContainerNavigation = 4,
        Container = 5,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenSourceKindTarget {
        Unchanged = 0,
        Session = 1,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenDisplayedLocationTarget {
        Unchanged = 0,
        Session = 1,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenEffect {
        ClearImage = 0,
        ResetZoom = 1,
        UpdatePageNavigation = 2,
        ScheduleAdjacentImagePredecode = 3,
        PrepareFailedContainer = 4,
        ClearLoadingPresentation = 5,
        FinishSpreadTransition = 6,
        ClearSecondaryPage = 7,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenWorkflowEventKind {
        BeginSourceLoad = 0,
        FinishEmptySourceLoad = 1,
        FinishSuccessfulImageLoad = 2,
        FinishSourceLoadWithError = 3,
        FinishContainerNavigationLoadWithError = 4,
        FinishAnimationLoadWithError = 5,
        ResolveSourceImage = 6,
        FinishUnsupportedOpenedCollectionVideoLoad = 7,
        FinishPlayableOpenedCollectionVideoLoad = 8,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageOpenLoadFailureRoute {
        Source = 0,
        ContainerNavigation = 1,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageDocumentSourceLoadKind {
        CurrentSource = 0,
        SameScopeImageNavigation = 1,
        ReplacementSource = 2,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    enum RustImageDocumentSourceLoadOperation {
        CancelFileDeletion = 0,
        CancelAllNavigation = 1,
        CancelPredecode = 2,
        FinishSpreadTransition = 3,
        ResetRightToLeftReading = 4,
        NotifyRightToLeftReadingChanged = 5,
        ClearSecondaryPage = 6,
        BeginSameScopeImageNavigationPresentation = 7,
        ClearLoadingContainerNavigationUrl = 8,
        SetLoadingContainerNavigationUrlToRequested = 9,
        SetContainerNavigationUrlToRequested = 10,
        PrepareSourceLoad = 11,
        SetSourceUrlToRequested = 12,
        BeginOpen = 13,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    struct RustImageOpenBeginSourceLoadInput {
        has_image: bool,
        has_loading_container_navigation_target: bool,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    struct RustImageOpenSuccessfulImageLoadInput {
        has_request_container_navigation_target: bool,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    struct RustImageOpenWorkflowEvent {
        kind: RustImageOpenWorkflowEventKind,
        begin_source_load: RustImageOpenBeginSourceLoadInput,
        successful_image_load: RustImageOpenSuccessfulImageLoadInput,
        load_failure_route: RustImageOpenLoadFailureRoute,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    struct RustImageDocumentSourceLoadPolicyInput {
        load_kind: RustImageDocumentSourceLoadKind,
        preserve_two_page_spread_transition: bool,
        right_to_left_reading_enabled: bool,
        source_within_displayed_comic_book_archive: bool,
        has_requested_container_navigation_url: bool,
    }

    #[derive(Debug, PartialEq, Eq)]
    struct RustImageOpenTransition {
        state_delta: RustImageOpenStateDelta,
        effects: Vec<RustImageOpenEffect>,
    }

    #[derive(Clone, Debug, PartialEq, Eq)]
    struct RustImageDocumentSourceLoadPlan {
        operations: Vec<RustImageDocumentSourceLoadOperation>,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    struct RustImageOpenStateDelta {
        source_url: RustImageOpenUrlTarget,
        source_kind: RustImageOpenSourceKindTarget,
        displayed_location: RustImageOpenDisplayedLocationTarget,
        container_navigation_url: RustImageOpenUrlTarget,
        loading: RustImageOpenBoolTarget,
        status: RustImageOpenStatusTarget,
        error_string: RustImageOpenErrorStringTarget,
        unsupported_opened_collection_video: RustImageOpenBoolTarget,
        clear_embedded_metadata: bool,
        clear_loading_container_navigation_url: bool,
    }

    extern "Rust" {
        #[cxx_name = "rustImageOpenTransition"]
        fn rust_image_open_transition(event: RustImageOpenWorkflowEvent)
        -> RustImageOpenTransition;

        #[cxx_name = "rustImageDocumentSourceLoadPlan"]
        fn rust_image_document_source_load_plan(
            input: RustImageDocumentSourceLoadPolicyInput,
        ) -> RustImageDocumentSourceLoadPlan;
    }
}

pub(super) use ffi::{
    RustImageDocumentSourceLoadKind, RustImageDocumentSourceLoadOperation,
    RustImageDocumentSourceLoadPlan, RustImageDocumentSourceLoadPolicyInput,
    RustImageOpenBoolTarget, RustImageOpenDisplayedLocationTarget, RustImageOpenEffect,
    RustImageOpenErrorStringTarget, RustImageOpenLoadFailureRoute, RustImageOpenSourceKindTarget,
    RustImageOpenStateDelta, RustImageOpenStatusTarget, RustImageOpenTransition,
    RustImageOpenUrlTarget, RustImageOpenWorkflowEvent, RustImageOpenWorkflowEventKind,
};

#[cfg(test)]
pub(super) use ffi::{RustImageOpenBeginSourceLoadInput, RustImageOpenSuccessfulImageLoadInput};

fn rust_image_open_transition(event: RustImageOpenWorkflowEvent) -> RustImageOpenTransition {
    transition::transition(event)
}

fn rust_image_document_source_load_plan(
    input: RustImageDocumentSourceLoadPolicyInput,
) -> RustImageDocumentSourceLoadPlan {
    source_load::plan(input)
}
