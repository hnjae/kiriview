# Non-Retained Image Target Transitions

## Context

Retaining a previous image while another navigation target loads makes the visible image disagree with the user's current selection. It also requires presentation-shape failures to pin and restore an entire prior component generation and the application policy associated with it. KiriView already prepares adjacent images so common navigation can commit quickly, and transient internal loading phases do not by themselves justify immediate full-page loading feedback.

## Decision

Every different image target clears the prior presentation as soon as the target is accepted. The rule applies to same-scope navigation, direct-source replacement, and single-page versus Two-Page Spread changes. Target failures keep the requested target and presentation shape active and display that target's error instead of restoring a prior commit.

Same-image refinement is not a target transition. It may keep the current accepted payload visible while replacement detail is prepared.

Canonical loading state advances immediately. QML delays only the visible loading indicator so a target that becomes ready within 150 milliseconds appears without an intervening loading screen.

## Consequences

The selected target, displayed image, readiness projection, and presentation-shape policy no longer require a restore correlation path. Accepting a target immediately stops any prior presentation and its animation resources.

The viewport can remain visually empty during a short load. Loads that outlast the feedback delay show the normal loading state, while terminal ready, error, or empty state cancels pending loading feedback immediately.

This decision supersedes ADR 0007's restore-on-failure decision for presentation-shape changes. ADR 0007 remains authoritative for repository-internal component ownership, provider failure handles, and the other component boundaries it defines.
