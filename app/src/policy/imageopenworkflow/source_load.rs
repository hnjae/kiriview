// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

use super::{
    RustImageDocumentSourceLoadKind, RustImageDocumentSourceLoadOperation,
    RustImageDocumentSourceLoadPlan, RustImageDocumentSourceLoadPolicyInput,
};

pub(super) fn plan(
    input: RustImageDocumentSourceLoadPolicyInput,
) -> RustImageDocumentSourceLoadPlan {
    match input.load_kind {
        RustImageDocumentSourceLoadKind::CurrentSource => current_source_load_plan(input),
        RustImageDocumentSourceLoadKind::SameScopeImageNavigation => {
            same_scope_image_navigation_load_plan(input)
        }
        RustImageDocumentSourceLoadKind::ReplacementSource => replacement_source_load_plan(input),
        _ => current_source_load_plan(input),
    }
}

fn current_source_load_plan(
    input: RustImageDocumentSourceLoadPolicyInput,
) -> RustImageDocumentSourceLoadPlan {
    let mut plan = source_load_plan();
    apply_right_to_left_reading_transition(&mut plan, input, false);
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::ClearLoadingContainerNavigationUrl,
    );
    if input.has_requested_container_navigation_url {
        push_source_load_operation(
            &mut plan,
            RustImageDocumentSourceLoadOperation::SetContainerNavigationUrlToRequested,
        );
    }
    plan
}

fn replacement_source_load_plan(
    input: RustImageDocumentSourceLoadPolicyInput,
) -> RustImageDocumentSourceLoadPlan {
    let mut plan = source_load_plan();
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::CancelAllNavigation,
    );
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::CancelPredecode,
    );
    apply_right_to_left_reading_transition(&mut plan, input, true);
    push_source_load_if(
        &mut plan,
        !input.preserve_two_page_spread_transition,
        RustImageDocumentSourceLoadOperation::ClearSecondaryPage,
    );
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::SetLoadingContainerNavigationUrlToRequested,
    );
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
    );
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
    );
    push_source_load_operation(&mut plan, RustImageDocumentSourceLoadOperation::BeginOpen);
    if right_to_left_reading_reset_applies(input) {
        push_source_load_operation(
            &mut plan,
            RustImageDocumentSourceLoadOperation::NotifyRightToLeftReadingChanged,
        );
    }
    plan
}

fn same_scope_image_navigation_load_plan(
    input: RustImageDocumentSourceLoadPolicyInput,
) -> RustImageDocumentSourceLoadPlan {
    let mut plan = source_load_plan();
    apply_right_to_left_reading_transition(&mut plan, input, false);
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::ClearLoadingContainerNavigationUrl,
    );
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::PrepareSourceLoad,
    );
    push_source_load_operation(
        &mut plan,
        RustImageDocumentSourceLoadOperation::SetSourceUrlToRequested,
    );
    push_source_load_operation(&mut plan, RustImageDocumentSourceLoadOperation::BeginOpen);
    plan
}

fn apply_right_to_left_reading_transition(
    plan: &mut RustImageDocumentSourceLoadPlan,
    input: RustImageDocumentSourceLoadPolicyInput,
    replacement_source: bool,
) {
    if !right_to_left_reading_reset_applies(input) {
        return;
    }

    push_source_load_operation(
        plan,
        RustImageDocumentSourceLoadOperation::ResetRightToLeftReading,
    );
    if !replacement_source {
        push_source_load_operation(
            plan,
            RustImageDocumentSourceLoadOperation::NotifyRightToLeftReadingChanged,
        );
    }
}

fn right_to_left_reading_reset_applies(input: RustImageDocumentSourceLoadPolicyInput) -> bool {
    input.right_to_left_reading_enabled
        && !input.has_requested_container_navigation_url
        && !input.source_within_displayed_comic_book_archive
}

fn push_source_load_if(
    plan: &mut RustImageDocumentSourceLoadPlan,
    condition: bool,
    operation: RustImageDocumentSourceLoadOperation,
) {
    if condition {
        push_source_load_operation(plan, operation);
    }
}

fn push_source_load_operation(
    plan: &mut RustImageDocumentSourceLoadPlan,
    operation: RustImageDocumentSourceLoadOperation,
) {
    plan.operations.push(operation);
}

fn source_load_plan() -> RustImageDocumentSourceLoadPlan {
    RustImageDocumentSourceLoadPlan {
        operations: vec![RustImageDocumentSourceLoadOperation::CancelFileDeletion],
    }
}

#[cfg(test)]
#[path = "source_load/tests.rs"]
mod tests;
