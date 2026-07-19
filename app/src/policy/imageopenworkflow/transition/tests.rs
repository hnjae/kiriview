// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use super::*;

fn has_effect(transition: &RustImageOpenTransition, effect: RustImageOpenEffect) -> bool {
    transition.effects.contains(&effect)
}

fn state_delta(
    source_url: RustImageOpenUrlTarget,
    displayed_location: RustImageOpenDisplayedLocationTarget,
    container_navigation_url: RustImageOpenUrlTarget,
    loading: RustImageOpenBoolTarget,
    status: RustImageOpenStatusTarget,
    error_string: RustImageOpenErrorStringTarget,
    clear_loading_container_navigation_url: bool,
) -> RustImageOpenStateDelta {
    RustImageOpenStateDelta {
        source_url,
        source_kind: RustImageOpenSourceKindTarget::Unchanged,
        displayed_location,
        container_navigation_url,
        loading,
        status,
        error_string,
        unsupported_opened_collection_video: RustImageOpenBoolTarget::Unchanged,
        clear_embedded_metadata: false,
        clear_loading_container_navigation_url,
    }
}

fn completed_load_delta(
    source_url: RustImageOpenUrlTarget,
    displayed_location: RustImageOpenDisplayedLocationTarget,
    container_navigation_url: RustImageOpenUrlTarget,
    status: RustImageOpenStatusTarget,
    error_string: RustImageOpenErrorStringTarget,
) -> RustImageOpenStateDelta {
    state_delta(
        source_url,
        displayed_location,
        container_navigation_url,
        RustImageOpenBoolTarget::False,
        status,
        error_string,
        true,
    )
}

fn with_unsupported_opened_collection_video_cleared(
    mut delta: RustImageOpenStateDelta,
) -> RustImageOpenStateDelta {
    delta.unsupported_opened_collection_video = RustImageOpenBoolTarget::False;
    delta
}

fn with_embedded_metadata_cleared(mut delta: RustImageOpenStateDelta) -> RustImageOpenStateDelta {
    delta.clear_embedded_metadata = true;
    delta
}

fn image_open_event(kind: RustImageOpenWorkflowEventKind) -> RustImageOpenWorkflowEvent {
    RustImageOpenWorkflowEvent {
        kind,
        begin_source_load: RustImageOpenBeginSourceLoadInput {
            has_image: false,
            has_loading_container_navigation_target: false,
        },
        successful_image_load: RustImageOpenSuccessfulImageLoadInput {
            has_request_container_navigation_target: false,
        },
        load_failure_route: RustImageOpenLoadFailureRoute::Source,
    }
}

fn begin_source_load_event(
    has_image: bool,
    has_loading_container_navigation_target: bool,
) -> RustImageOpenWorkflowEvent {
    RustImageOpenWorkflowEvent {
        kind: RustImageOpenWorkflowEventKind::BeginSourceLoad,
        begin_source_load: RustImageOpenBeginSourceLoadInput {
            has_image,
            has_loading_container_navigation_target,
        },
        ..image_open_event(RustImageOpenWorkflowEventKind::BeginSourceLoad)
    }
}

fn successful_image_load_event(
    has_request_container_navigation_target: bool,
) -> RustImageOpenWorkflowEvent {
    RustImageOpenWorkflowEvent {
        kind: RustImageOpenWorkflowEventKind::FinishSuccessfulImageLoad,
        successful_image_load: RustImageOpenSuccessfulImageLoadInput {
            has_request_container_navigation_target,
        },
        ..image_open_event(RustImageOpenWorkflowEventKind::FinishSuccessfulImageLoad)
    }
}

fn source_load_error_event(
    load_failure_route: RustImageOpenLoadFailureRoute,
) -> RustImageOpenWorkflowEvent {
    RustImageOpenWorkflowEvent {
        kind: RustImageOpenWorkflowEventKind::FinishSourceLoadWithError,
        load_failure_route,
        ..image_open_event(RustImageOpenWorkflowEventKind::FinishSourceLoadWithError)
    }
}

#[test]
fn first_source_load_clears_image_and_enters_loading() {
    let transition = transition(begin_source_load_event(false, false));

    assert_eq!(
        transition.state_delta,
        with_embedded_metadata_cleared(with_unsupported_opened_collection_video_cleared(
            state_delta(
                RustImageOpenUrlTarget::Unchanged,
                RustImageOpenDisplayedLocationTarget::Unchanged,
                RustImageOpenUrlTarget::Empty,
                RustImageOpenBoolTarget::True,
                RustImageOpenStatusTarget::Loading,
                RustImageOpenErrorStringTarget::Clear,
                false,
            ),
        ))
    );
    assert_eq!(transition.effects, vec![RustImageOpenEffect::ClearImage]);
}

#[test]
fn replacement_source_load_clears_presentation_and_enters_loading() {
    let transition = transition(begin_source_load_event(true, true));

    assert_eq!(
        transition.state_delta,
        with_embedded_metadata_cleared(with_unsupported_opened_collection_video_cleared(
            state_delta(
                RustImageOpenUrlTarget::Unchanged,
                RustImageOpenDisplayedLocationTarget::Unchanged,
                RustImageOpenUrlTarget::Unchanged,
                RustImageOpenBoolTarget::True,
                RustImageOpenStatusTarget::Loading,
                RustImageOpenErrorStringTarget::Clear,
                false,
            ),
        ))
    );
    assert!(transition.effects.is_empty());
}

#[test]
fn first_source_load_preserves_pending_container_navigation_target() {
    let transition = transition(begin_source_load_event(false, true));

    assert_eq!(
        transition.state_delta.container_navigation_url,
        RustImageOpenUrlTarget::Unchanged
    );
    assert_eq!(
        transition.state_delta.status,
        RustImageOpenStatusTarget::Loading
    );
}

#[test]
fn successful_load_uses_session_targets_and_clears_error() {
    let transition = transition(successful_image_load_event(true));

    assert_eq!(
        transition.state_delta,
        with_unsupported_opened_collection_video_cleared(completed_load_delta(
            RustImageOpenUrlTarget::SessionImage,
            RustImageOpenDisplayedLocationTarget::Session,
            RustImageOpenUrlTarget::SessionContainerNavigation,
            RustImageOpenStatusTarget::Ready,
            RustImageOpenErrorStringTarget::Clear,
        ))
    );
    assert_eq!(
        transition.effects,
        vec![
            RustImageOpenEffect::UpdatePageNavigation,
            RustImageOpenEffect::ScheduleAdjacentImagePredecode,
        ]
    );
}

#[test]
fn source_resolution_uses_session_image_without_completing_load() {
    let transition = transition(image_open_event(
        RustImageOpenWorkflowEventKind::ResolveSourceImage,
    ));
    let mut expected = state_delta(
        RustImageOpenUrlTarget::SessionImage,
        RustImageOpenDisplayedLocationTarget::Unchanged,
        RustImageOpenUrlTarget::Unchanged,
        RustImageOpenBoolTarget::Unchanged,
        RustImageOpenStatusTarget::Unchanged,
        RustImageOpenErrorStringTarget::Unchanged,
        false,
    );
    expected.source_kind = RustImageOpenSourceKindTarget::Session;

    assert_eq!(transition.state_delta, expected);
    assert!(transition.effects.is_empty());
}

#[test]
fn successful_load_derives_missing_container_navigation_target() {
    let transition = transition(successful_image_load_event(false));

    assert_eq!(
        transition.state_delta.container_navigation_url,
        RustImageOpenUrlTarget::DerivedContainerNavigation
    );
}

#[test]
fn source_load_failure_keeps_target_error_without_recovery_effects() {
    let transition = source_target_load_error_transition();

    assert_eq!(
        transition.state_delta,
        with_embedded_metadata_cleared(with_unsupported_opened_collection_video_cleared(
            completed_load_delta(
                RustImageOpenUrlTarget::Unchanged,
                RustImageOpenDisplayedLocationTarget::Unchanged,
                RustImageOpenUrlTarget::Empty,
                RustImageOpenStatusTarget::Error,
                RustImageOpenErrorStringTarget::Provided,
            ),
        ))
    );
    assert!(transition.effects.is_empty());
}

#[test]
fn source_errors_keep_the_failed_target() {
    let initial = transition(source_load_error_event(
        RustImageOpenLoadFailureRoute::Source,
    ));

    assert!(!has_effect(&initial, RustImageOpenEffect::ClearImage));
    assert_eq!(
        initial.state_delta.container_navigation_url,
        RustImageOpenUrlTarget::Empty
    );
    assert_eq!(initial.state_delta.status, RustImageOpenStatusTarget::Error);
}

#[test]
fn routed_load_failure_returns_the_selected_error_transition() {
    let container = transition(source_load_error_event(
        RustImageOpenLoadFailureRoute::ContainerNavigation,
    ));
    assert_eq!(
        container.state_delta.source_url,
        RustImageOpenUrlTarget::Container
    );
    assert!(has_effect(&container, RustImageOpenEffect::ClearImage));
    assert_eq!(
        container.state_delta.status,
        RustImageOpenStatusTarget::Error
    );

    let replacement = transition(source_load_error_event(
        RustImageOpenLoadFailureRoute::Source,
    ));
    assert_eq!(
        replacement.state_delta.source_url,
        RustImageOpenUrlTarget::Unchanged
    );
    assert!(replacement.effects.is_empty());
    assert_eq!(
        replacement.state_delta.status,
        RustImageOpenStatusTarget::Error
    );

    let initial = transition(source_load_error_event(
        RustImageOpenLoadFailureRoute::Source,
    ));
    assert_eq!(
        initial.state_delta.source_url,
        RustImageOpenUrlTarget::Unchanged
    );
    assert!(!has_effect(&initial, RustImageOpenEffect::ClearImage));
    assert_eq!(
        initial.state_delta.container_navigation_url,
        RustImageOpenUrlTarget::Empty
    );
    assert_eq!(initial.state_delta.status, RustImageOpenStatusTarget::Error);

    let explicit_container = transition(image_open_event(
        RustImageOpenWorkflowEventKind::FinishContainerNavigationLoadWithError,
    ));
    assert_eq!(
        explicit_container.state_delta.source_url,
        RustImageOpenUrlTarget::Container
    );
    assert!(has_effect(
        &explicit_container,
        RustImageOpenEffect::ClearImage
    ));
}
