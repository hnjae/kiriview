// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use super::*;

#[test]
fn current_source_load_plan_uses_reading_and_container_snapshots() {
    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::CurrentSource,
            true,
            false,
            false,
            false,
        )),
        RustImageDocumentSourceLoadPlan {
            operations: vec![
                RustImageDocumentSourceLoadOperation::CancelFileDeletion,
                RustImageDocumentSourceLoadOperation::ClearLoadingContainerNavigationUrl,
            ],
        }
    );

    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::CurrentSource,
            false,
            true,
            false,
            true,
        )),
        RustImageDocumentSourceLoadPlan {
            operations: vec![
                RustImageDocumentSourceLoadOperation::CancelFileDeletion,
                RustImageDocumentSourceLoadOperation::ClearLoadingContainerNavigationUrl,
                RustImageDocumentSourceLoadOperation::SetContainerNavigationUrlToRequested,
            ],
        }
    );
}

#[test]
fn same_scope_image_navigation_load_plan_begins_open_without_replacement_resets() {
    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::SameScopeImageNavigation,
            true,
            false,
            false,
            false,
        )),
        RustImageDocumentSourceLoadPlan {
            operations: vec![
                RustImageDocumentSourceLoadOperation::CancelFileDeletion,
                RustImageDocumentSourceLoadOperation::ClearLoadingContainerNavigationUrl,
                RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
                RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
                RustImageDocumentSourceLoadOperation::BeginOpen,
            ],
        }
    );

    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::SameScopeImageNavigation,
            false,
            true,
            false,
            false,
        ))
        .operations,
        vec![
            RustImageDocumentSourceLoadOperation::CancelFileDeletion,
            RustImageDocumentSourceLoadOperation::ResetRightToLeftReading,
            RustImageDocumentSourceLoadOperation::NotifyRightToLeftReadingChanged,
            RustImageDocumentSourceLoadOperation::ClearLoadingContainerNavigationUrl,
            RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
            RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
            RustImageDocumentSourceLoadOperation::BeginOpen,
        ]
    );
}

#[test]
fn replacement_source_load_plan_begins_open_and_clears_competing_state_in_order() {
    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::ReplacementSource,
            true,
            false,
            false,
            false,
        )),
        RustImageDocumentSourceLoadPlan {
            operations: vec![
                RustImageDocumentSourceLoadOperation::CancelFileDeletion,
                RustImageDocumentSourceLoadOperation::CancelAllNavigation,
                RustImageDocumentSourceLoadOperation::CancelPredecode,
                RustImageDocumentSourceLoadOperation::SetLoadingContainerNavigationUrlToRequested,
                RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
                RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
                RustImageDocumentSourceLoadOperation::BeginOpen,
            ],
        }
    );

    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::ReplacementSource,
            false,
            true,
            false,
            true,
        )),
        RustImageDocumentSourceLoadPlan {
            operations: vec![
                RustImageDocumentSourceLoadOperation::CancelFileDeletion,
                RustImageDocumentSourceLoadOperation::CancelAllNavigation,
                RustImageDocumentSourceLoadOperation::CancelPredecode,
                RustImageDocumentSourceLoadOperation::ClearSecondaryPage,
                RustImageDocumentSourceLoadOperation::SetLoadingContainerNavigationUrlToRequested,
                RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
                RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
                RustImageDocumentSourceLoadOperation::BeginOpen,
            ],
        }
    );
}

#[test]
fn right_to_left_reading_reset_is_decided_from_source_load_context() {
    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::CurrentSource,
            true,
            true,
            false,
            false,
        ))
        .operations,
        vec![
            RustImageDocumentSourceLoadOperation::CancelFileDeletion,
            RustImageDocumentSourceLoadOperation::ResetRightToLeftReading,
            RustImageDocumentSourceLoadOperation::NotifyRightToLeftReadingChanged,
            RustImageDocumentSourceLoadOperation::ClearLoadingContainerNavigationUrl,
        ]
    );

    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::ReplacementSource,
            true,
            true,
            false,
            false,
        ))
        .operations,
        vec![
            RustImageDocumentSourceLoadOperation::CancelFileDeletion,
            RustImageDocumentSourceLoadOperation::CancelAllNavigation,
            RustImageDocumentSourceLoadOperation::CancelPredecode,
            RustImageDocumentSourceLoadOperation::ResetRightToLeftReading,
            RustImageDocumentSourceLoadOperation::SetLoadingContainerNavigationUrlToRequested,
            RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
            RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
            RustImageDocumentSourceLoadOperation::BeginOpen,
            RustImageDocumentSourceLoadOperation::NotifyRightToLeftReadingChanged,
        ]
    );

    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::ReplacementSource,
            true,
            true,
            true,
            false,
        ))
        .operations,
        vec![
            RustImageDocumentSourceLoadOperation::CancelFileDeletion,
            RustImageDocumentSourceLoadOperation::CancelAllNavigation,
            RustImageDocumentSourceLoadOperation::CancelPredecode,
            RustImageDocumentSourceLoadOperation::SetLoadingContainerNavigationUrlToRequested,
            RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
            RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
            RustImageDocumentSourceLoadOperation::BeginOpen,
        ]
    );

    assert_eq!(
        plan(source_load_policy_input(
            RustImageDocumentSourceLoadKind::ReplacementSource,
            true,
            true,
            false,
            true,
        ))
        .operations,
        vec![
            RustImageDocumentSourceLoadOperation::CancelFileDeletion,
            RustImageDocumentSourceLoadOperation::CancelAllNavigation,
            RustImageDocumentSourceLoadOperation::CancelPredecode,
            RustImageDocumentSourceLoadOperation::SetLoadingContainerNavigationUrlToRequested,
            RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
            RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
            RustImageDocumentSourceLoadOperation::BeginOpen,
        ]
    );
}

fn source_load_policy_input(
    load_kind: RustImageDocumentSourceLoadKind,
    preserve_two_page_spread_transition: bool,
    right_to_left_reading_enabled: bool,
    source_within_displayed_comic_book_archive: bool,
    has_requested_container_navigation_url: bool,
) -> RustImageDocumentSourceLoadPolicyInput {
    RustImageDocumentSourceLoadPolicyInput {
        load_kind,
        preserve_two_page_spread_transition,
        right_to_left_reading_enabled,
        source_within_displayed_comic_book_archive,
        has_requested_container_navigation_url,
    }
}
