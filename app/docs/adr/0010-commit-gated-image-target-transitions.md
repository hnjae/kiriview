# Commit-Gated Image Target Transitions

## Context

Clearing the committed image when a replacement target is accepted exposes the interval between provider readiness and render commit as an empty viewport. That interval remains observable when a predecoded payload is available because cache availability does not complete payload admission, upload, and scene-graph presentation.

Retaining pixels from the previous target does not require retaining its selected-target identity, operation readiness, or failure behavior. `ImageViewport` already distinguishes the accepted target from the displayed generation and can retain one complete committed display while a replacement is pending.

## Decision

This decision supersedes ADR 0009.

When KiriView accepts a different image target, selected-target identity and target-specific chrome advance immediately. Image operations become unavailable until a matching presentation is ready, and playback of the prior target stops.

KiriView asks `ImageViewport` to retain the last complete committed display while the replacement is pending and to keep a failed replacement as the accepted target. Retained pixels are a non-interactive visual fallback owned by their original displayed generation; they are never attributed to the new selected target.

A target-correlated 150-millisecond grace period controls loading feedback. A matching render commit before the deadline replaces the fallback directly. If the deadline expires first, a loading surface covers the viewport until a matching render commit or terminal state. Superseding, ready, error, and empty transitions retire or replace pending feedback for their target lifecycle.

Terminal replacement failure retires the fallback and displays the selected target's error. KiriView does not restore the prior selected target, presentation shape, or application state.

Cache and predecode availability remain preparation facts. The UI derives display readiness only from the matching supported viewport observation.

## Consequences

Fast image navigation changes directly between complete committed frames without an empty intermediate viewport. Slow navigation provides deliberate loading feedback without presenting fallback pixels as current media.

The transition may temporarily retain one previous complete display in addition to the accepted target's work. Display-store, provider, and viewport owners account for and release that fallback through their existing bounded ownership contracts.

Selected target, accepted viewport target, displayed generation, readiness, and feedback lifecycle remain independently correlated. No application restoration path or cache-hit UI projection is required.
