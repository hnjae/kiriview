# Non-Retained Image Target Transitions

> [!NOTE]
> This decision is superseded by [ADR 0010](0010-commit-gated-image-target-transitions.md).

## Context

Retaining a previous image while another navigation target loads makes the visible image disagree with the user's current selection and complicates KiriView's selected-target, readiness, and error projections. KiriView already prepares adjacent images so common navigation can complete quickly, and transient loading phases do not by themselves justify immediate full-page loading feedback.

## Decision

Every different image target clears the prior presentation as soon as the target is accepted. The rule applies to same-scope navigation, direct-source replacement, and single-page versus Two-Page Spread changes. Target failures keep the requested target and presentation shape active and display that target's error instead of restoring a prior commit.

Same-image refinement is not a target transition. It may keep the current accepted payload visible while replacement detail is prepared.

Canonical loading state advances immediately. QML delays only the visible loading indicator so a target that becomes ready within 150 milliseconds appears without an intervening loading screen.

Clearing the prior presentation does not clear the accepted selected target. Target-identifying chrome follows the selected target during loading and target-specific errors, while controls and operations that require displayed media remain unavailable until a matching presentation is ready.

## Consequences

The selected target, displayed image, readiness projection, and presentation-shape policy do not require an application restoration path. KiriView clears its prior displayed-image projection when it accepts the new target and derives later presentation facts only from a matching public observation.

Selected-target identity and scope remain available independently of displayed-presentation readiness, so title and toolbar placement do not need QML-owned retained values.

The viewport can remain visually empty during a short load. Loads that outlast the feedback delay show the normal loading state, while terminal ready, error, or empty state cancels pending loading feedback immediately.

The `ImageViewport` integration must express this product transition through the dependency's supported interface without depending on or constraining its private implementation.
