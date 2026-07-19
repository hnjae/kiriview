// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use super::{
    RustImageOpenBoolTarget, RustImageOpenDisplayedLocationTarget, RustImageOpenEffect,
    RustImageOpenErrorStringTarget, RustImageOpenLoadFailureRoute, RustImageOpenSourceKindTarget,
    RustImageOpenStateDelta, RustImageOpenStatusTarget, RustImageOpenTransition,
    RustImageOpenUrlTarget, RustImageOpenWorkflowEvent, RustImageOpenWorkflowEventKind,
};

#[cfg(test)]
use super::{RustImageOpenBeginSourceLoadInput, RustImageOpenSuccessfulImageLoadInput};

pub(super) fn transition(event: RustImageOpenWorkflowEvent) -> RustImageOpenTransition {
    match event.kind {
        RustImageOpenWorkflowEventKind::BeginSourceLoad => begin_source_load_transition(event),
        RustImageOpenWorkflowEventKind::FinishEmptySourceLoad => {
            finish_empty_source_load_transition()
        }
        RustImageOpenWorkflowEventKind::FinishSuccessfulImageLoad => {
            finish_successful_image_load_transition(event)
        }
        RustImageOpenWorkflowEventKind::FinishSourceLoadWithError => {
            source_load_error_transition(event)
        }
        RustImageOpenWorkflowEventKind::FinishContainerNavigationLoadWithError => {
            container_navigation_load_error_transition()
        }
        RustImageOpenWorkflowEventKind::ResolveSourceImage => resolve_source_image_transition(),
        RustImageOpenWorkflowEventKind::FinishUnsupportedOpenedCollectionVideoLoad => {
            opened_collection_video_transition(RustImageOpenBoolTarget::True)
        }
        RustImageOpenWorkflowEventKind::FinishPlayableOpenedCollectionVideoLoad => {
            opened_collection_video_transition(RustImageOpenBoolTarget::False)
        }
        _ => empty_transition(),
    }
}

fn begin_source_load_transition(event: RustImageOpenWorkflowEvent) -> RustImageOpenTransition {
    let mut transition = empty_transition();
    let input = event.begin_source_load;
    set_unsupported_opened_collection_video(&mut transition, RustImageOpenBoolTarget::False);
    set_embedded_metadata_cleared(&mut transition);
    set_error_string(&mut transition, RustImageOpenErrorStringTarget::Clear);

    if !input.has_image && !input.has_loading_container_navigation_target {
        set_container_navigation_url(&mut transition, RustImageOpenUrlTarget::Empty);
    }

    set_loading(&mut transition, RustImageOpenBoolTarget::True);
    if input.has_image {
        set_status(&mut transition, RustImageOpenStatusTarget::Loading);
    } else {
        push_effect(&mut transition, RustImageOpenEffect::ClearImage);
        set_status(&mut transition, RustImageOpenStatusTarget::Loading);
    }
    transition
}

fn finish_empty_source_load_transition() -> RustImageOpenTransition {
    let mut transition = empty_transition();
    push_effect(&mut transition, RustImageOpenEffect::ClearImage);
    set_unsupported_opened_collection_video(&mut transition, RustImageOpenBoolTarget::False);
    set_embedded_metadata_cleared(&mut transition);
    set_error_string(&mut transition, RustImageOpenErrorStringTarget::Clear);
    set_tracked_load_completed(&mut transition);
    set_container_navigation_url(&mut transition, RustImageOpenUrlTarget::Empty);
    set_status(&mut transition, RustImageOpenStatusTarget::Null);
    transition
}

fn resolve_source_image_transition() -> RustImageOpenTransition {
    let mut transition = empty_transition();
    set_source_url(&mut transition, RustImageOpenUrlTarget::SessionImage);
    set_source_kind(&mut transition, RustImageOpenSourceKindTarget::Session);
    transition
}

fn opened_collection_video_transition(
    unsupported_opened_collection_video: RustImageOpenBoolTarget,
) -> RustImageOpenTransition {
    let mut transition = empty_transition();
    set_source_url(&mut transition, RustImageOpenUrlTarget::SessionImage);
    set_source_kind(&mut transition, RustImageOpenSourceKindTarget::Session);
    set_displayed_location(
        &mut transition,
        RustImageOpenDisplayedLocationTarget::Session,
    );
    set_container_navigation_url(
        &mut transition,
        RustImageOpenUrlTarget::DerivedContainerNavigation,
    );
    set_error_string(&mut transition, RustImageOpenErrorStringTarget::Clear);
    set_embedded_metadata_cleared(&mut transition);
    set_tracked_load_completed(&mut transition);
    set_status(&mut transition, RustImageOpenStatusTarget::Ready);
    set_unsupported_opened_collection_video(&mut transition, unsupported_opened_collection_video);
    push_effect(&mut transition, RustImageOpenEffect::ClearSecondaryPage);
    push_effect(&mut transition, RustImageOpenEffect::UpdatePageNavigation);
    transition
}

fn finish_successful_image_load_transition(
    event: RustImageOpenWorkflowEvent,
) -> RustImageOpenTransition {
    let mut transition = empty_transition();
    let input = event.successful_image_load;
    set_source_url(&mut transition, RustImageOpenUrlTarget::SessionImage);
    set_displayed_location(
        &mut transition,
        RustImageOpenDisplayedLocationTarget::Session,
    );
    set_container_navigation_url(
        &mut transition,
        if input.has_request_container_navigation_target {
            RustImageOpenUrlTarget::SessionContainerNavigation
        } else {
            RustImageOpenUrlTarget::DerivedContainerNavigation
        },
    );
    set_error_string(&mut transition, RustImageOpenErrorStringTarget::Clear);
    set_unsupported_opened_collection_video(&mut transition, RustImageOpenBoolTarget::False);
    set_tracked_load_completed(&mut transition);
    set_status(&mut transition, RustImageOpenStatusTarget::Ready);
    push_effect(&mut transition, RustImageOpenEffect::UpdatePageNavigation);
    push_effect(
        &mut transition,
        RustImageOpenEffect::ScheduleAdjacentImagePredecode,
    );
    transition
}

fn container_navigation_load_error_transition() -> RustImageOpenTransition {
    let mut transition = empty_transition();
    push_effect(&mut transition, RustImageOpenEffect::ClearImage);
    set_tracked_load_completed(&mut transition);
    set_container_navigation_url(&mut transition, RustImageOpenUrlTarget::Container);
    set_source_url(&mut transition, RustImageOpenUrlTarget::Container);
    set_error_string(&mut transition, RustImageOpenErrorStringTarget::Provided);
    set_unsupported_opened_collection_video(&mut transition, RustImageOpenBoolTarget::False);
    set_status(&mut transition, RustImageOpenStatusTarget::Error);
    transition
}

fn source_target_load_error_transition() -> RustImageOpenTransition {
    let mut transition = empty_transition();
    set_tracked_load_completed(&mut transition);
    set_container_navigation_url(&mut transition, RustImageOpenUrlTarget::Empty);
    set_error_string(&mut transition, RustImageOpenErrorStringTarget::Provided);
    set_unsupported_opened_collection_video(&mut transition, RustImageOpenBoolTarget::False);
    set_embedded_metadata_cleared(&mut transition);
    set_status(&mut transition, RustImageOpenStatusTarget::Error);
    transition
}

fn initial_load_error_transition() -> RustImageOpenTransition {
    source_target_load_error_transition()
}

fn source_load_error_transition(event: RustImageOpenWorkflowEvent) -> RustImageOpenTransition {
    match event.load_failure_route {
        RustImageOpenLoadFailureRoute::ContainerNavigation => {
            container_navigation_load_error_transition()
        }
        RustImageOpenLoadFailureRoute::Source => initial_load_error_transition(),
        _ => initial_load_error_transition(),
    }
}

fn set_tracked_load_completed(transition: &mut RustImageOpenTransition) {
    transition
        .state_delta
        .clear_loading_container_navigation_url = true;
    set_loading(transition, RustImageOpenBoolTarget::False);
}

fn set_source_url(transition: &mut RustImageOpenTransition, target: RustImageOpenUrlTarget) {
    debug_assert_eq!(
        transition.state_delta.source_url,
        RustImageOpenUrlTarget::Unchanged
    );
    transition.state_delta.source_url = target;
}

fn set_source_kind(
    transition: &mut RustImageOpenTransition,
    target: RustImageOpenSourceKindTarget,
) {
    debug_assert_eq!(
        transition.state_delta.source_kind,
        RustImageOpenSourceKindTarget::Unchanged
    );
    transition.state_delta.source_kind = target;
}

fn set_displayed_location(
    transition: &mut RustImageOpenTransition,
    target: RustImageOpenDisplayedLocationTarget,
) {
    debug_assert_eq!(
        transition.state_delta.displayed_location,
        RustImageOpenDisplayedLocationTarget::Unchanged
    );
    transition.state_delta.displayed_location = target;
}

fn set_container_navigation_url(
    transition: &mut RustImageOpenTransition,
    target: RustImageOpenUrlTarget,
) {
    debug_assert_eq!(
        transition.state_delta.container_navigation_url,
        RustImageOpenUrlTarget::Unchanged
    );
    transition.state_delta.container_navigation_url = target;
}

fn set_loading(transition: &mut RustImageOpenTransition, target: RustImageOpenBoolTarget) {
    debug_assert_eq!(
        transition.state_delta.loading,
        RustImageOpenBoolTarget::Unchanged
    );
    transition.state_delta.loading = target;
}

fn set_status(transition: &mut RustImageOpenTransition, target: RustImageOpenStatusTarget) {
    debug_assert_eq!(
        transition.state_delta.status,
        RustImageOpenStatusTarget::Unchanged
    );
    transition.state_delta.status = target;
}

fn set_error_string(
    transition: &mut RustImageOpenTransition,
    target: RustImageOpenErrorStringTarget,
) {
    debug_assert_eq!(
        transition.state_delta.error_string,
        RustImageOpenErrorStringTarget::Unchanged
    );
    transition.state_delta.error_string = target;
}

fn set_unsupported_opened_collection_video(
    transition: &mut RustImageOpenTransition,
    target: RustImageOpenBoolTarget,
) {
    debug_assert_eq!(
        transition.state_delta.unsupported_opened_collection_video,
        RustImageOpenBoolTarget::Unchanged
    );
    transition.state_delta.unsupported_opened_collection_video = target;
}

fn set_embedded_metadata_cleared(transition: &mut RustImageOpenTransition) {
    debug_assert!(!transition.state_delta.clear_embedded_metadata);
    transition.state_delta.clear_embedded_metadata = true;
}

fn push_effect(transition: &mut RustImageOpenTransition, effect: RustImageOpenEffect) {
    transition.effects.push(effect);
}

fn empty_transition() -> RustImageOpenTransition {
    RustImageOpenTransition {
        state_delta: empty_state_delta(),
        effects: Vec::new(),
    }
}

fn empty_state_delta() -> RustImageOpenStateDelta {
    RustImageOpenStateDelta {
        source_url: RustImageOpenUrlTarget::Unchanged,
        source_kind: RustImageOpenSourceKindTarget::Unchanged,
        displayed_location: RustImageOpenDisplayedLocationTarget::Unchanged,
        container_navigation_url: RustImageOpenUrlTarget::Unchanged,
        loading: RustImageOpenBoolTarget::Unchanged,
        status: RustImageOpenStatusTarget::Unchanged,
        error_string: RustImageOpenErrorStringTarget::Unchanged,
        unsupported_opened_collection_video: RustImageOpenBoolTarget::Unchanged,
        clear_embedded_metadata: false,
        clear_loading_container_navigation_url: false,
    }
}

#[cfg(test)]
#[path = "transition/tests.rs"]
mod tests;
