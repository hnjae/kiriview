# ImageViewport API

This document defines the repository-internal Qt/QML item surface, common command-result contract, coordinate operations, and rendering ownership boundary for `ImageViewport`. Presentation-target replacement and presentation mutation are defined in [ImageViewport Presentation](image-viewport-presentation.md), source-handle construction is defined in [ImageSequence Construction](image-sequence.md), the coherent snapshot schema is defined in [ImageViewport State](image-viewport-state.md), and component limits are defined in [ImageViewport Component Boundary](image-viewport-component-boundary.md).

## QML Item

`ImageViewport` is a `QQuickItem` that accepts presentation-target replacement through `setPresentationTarget(target, policy)`. `target` is an `ImageViewportPresentationTarget` value with a required `primary` `ImageSequence` for non-empty presentation and an optional `secondary` `ImageSequence`; both roles null represents clear. Raw strings, URLs, byte arrays, JavaScript objects, raw provider objects, provider URLs, image-provider ids, load acknowledgements, and arbitrary variants are not viewport sources.

The item exposes one primary observation property, `state: ImageViewportStateSnapshot`. The snapshot is immutable from QML and C++ caller perspective and contains `request`, `display`, `presentation`, `primary`, `secondary`, `diagnostics`, and `revisions` value objects as defined in [ImageViewport State](image-viewport-state.md). The item emits `stateChanged` whenever any snapshot field changes. Callers bind to fields inside `state`; independently projected flat item observations are not public API.

The item exposes imperative commands under the common result contract in this document. Presentation mutations use the single transactional command surface defined in [ImageViewport Presentation](image-viewport-presentation.md#presentation-commands).

QML visibility is a transport property of the component API, not an application-layer ownership grant. KiriView integration policy decides which application owner may invoke mutating operations and consume component observations for shared product state. The component does not distinguish application owners and applies the same validation and state-transition contract at every declared C++ and QML entry point.

The item exposes coordinate helpers as displayed-snapshot-relative operations: `mapPoint(input): ImageViewportCoordinateResult` and `containsPoint(input): bool`. `ImageViewportCoordinateInput` contains `sourceSpace`, `targetSpace`, nullable `role`, and `point`; `ImageViewportCoordinateResult` contains `valid`, `space`, nullable `role`, and `point`. `ImageViewportCoordinateSpace` values are `Item`, `DisplayedSpread`, and `DisplayedPage`. Item space starts at the item's top-left; displayed spread space starts at the unrotated virtual spread's top-left; displayed page space starts at the orientation-normalized page's top-left. Every space has positive x to the right and positive y downward. Spread and page domains are half-open: their left and top edges are included, and their right and bottom edges are excluded. A role is required and must belong to `state.display.displayedRoleSet` when either space is `DisplayedPage`; otherwise it must be null. `mapPoint` converts through the geometry identified by `state.display.displayedPresentationRevision`, and `containsPoint` reports whether the same input has a valid mapping into the requested target domain. Invalid inputs, excluded-edge points, and gap points for page-scoped conversion return `valid == false` or `false` without clamping and without changing diagnostics or revisions. Nearest-point selection and snapping policy belong to application code and are not viewport API.

## Command Values

`ImageViewportCommandResult` contains `outcome`, `reason`, `commandRevision`, and `snapshotRevision`. `commandRevision` identifies the command observation domain after the command is handled; `snapshotRevision` identifies the resulting snapshot. Rejected commands do not mutate request, display, or presentation state except for command diagnostics.

`clear()`, `play(role)`, `pause(role)`, `stop(role)`, `seek(role, frame)`, `seekToPosition(role, milliseconds)`, `setPresentationTarget(target, policy)`, `setPresentation(command)`, and `resetView()` return `ImageViewportCommandResult`. Coordinate helpers return coordinate results or booleans and do not allocate command revisions.

Commands that address source content require an explicit `ImageViewportPageRole`; the API has no primary-default `play`, `pause`, `stop`, `seek`, or `seekToPosition` aliases.

`ImageViewportCommandOutcome` values are `Accepted`, `Invalid`, `Unsupported`, and `IgnoredNoRequest`.

`ImageViewportCommandReason` values are `NoCommand`, `IgnoredNoRequest`, `InvalidRequest`, and `UnsupportedRequest`.

Command outcomes and reasons have a fixed pairing: `Accepted` uses `NoCommand`, `Invalid` uses `InvalidRequest`, `Unsupported` uses `UnsupportedRequest`, and `IgnoredNoRequest` uses `IgnoredNoRequest`. An accepted command may still be a defined no-op.

Except where a command explicitly creates a new identity, an accepted command whose complete effect would leave state unchanged is a no-op: it preserves diagnostics and revisions, emits no `stateChanged`, and returns the current revision tokens. This applies to clearing an already clear viewport, replaying an already-playing role without another target change, and pausing or stopping a present role already in that phase. A non-empty `setPresentationTarget` with `replacementIntent: NewTarget` always creates a new generation even when its handles equal the current target.

Seek targets are signed values but must be non-negative. When authoritative bounds are available, `seek(role, frame)` accepts only a frame in the inclusive frame bounds and `seekToPosition(role, milliseconds)` accepts only a position in the inclusive position bounds; `totalDuration` resolves to the final frame start rather than beyond the sequence. A structurally valid target accepted before bounds are known becomes display-request-terminal `InvalidRequest` if later metadata proves it out of range. A known out-of-range target is `Invalid` before capability checks; a valid target for an unsupported seek capability is `Unsupported`.

`ImageViewportPageRole` values are `Primary` and `Secondary`.

## Rendering Ownership

`ImageViewport` owns rendering for its accepted presentation target. The public API exposes source handles, commands, snapshot state, coordinate helpers, provider requests, provider events, diagnostics, limits, and revision tokens; it does not expose scene graph nodes, native textures, QML image-provider URLs, Qt Quick `Image` load status, load acknowledgement callbacks, render-frame projection APIs, or caller-managed render resources.

Backend selection and resource lifetime are private to the item. Applications that need source lookup, cache reuse, display-image store lifetime, predecode, or refinement payload production policy implement those concerns behind `ImageSequenceProviderAdapter`; they do not drive viewport rendering through external provider URLs.
