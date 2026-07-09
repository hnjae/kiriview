# ImageViewport API

This document defines the public Qt/QML item surface, source-handle entry point, mutating command contract, presentation-target replacement contract, and rendering ownership boundary for `ImageViewport`. The coherent snapshot schema is defined in [ImageViewport State](image-viewport-state.md), provider request/event/demand/payload values are defined in [ImageSequence Provider Protocol](image-sequence-provider-protocol.md), provider adapter behavior is defined in [ImageSequence Provider Adapter](image-sequence-provider-adapter.md), and installed package limits are defined in [ImageViewport Packaging](image-viewport-packaging.md).

## QML Item

`ImageViewport` is a `QQuickItem` that accepts presentation-target replacement through `setPresentationTarget(target, policy)`. `target` is an `ImageViewportPresentationTarget` value with a required `primary` `ImageSequence` for non-empty presentation and an optional `secondary` `ImageSequence`; a null primary represents clear. Raw strings, URLs, byte arrays, JavaScript objects, raw provider objects, provider URLs, image-provider ids, load acknowledgements, and arbitrary variants are not viewport sources.

The item exposes one primary observation property, `state: ImageViewportStateSnapshot`. The snapshot is immutable from QML and C++ caller perspective and contains `request`, `display`, `presentation`, `primary`, `secondary`, `diagnostics`, and `revisions` value objects as defined in [ImageViewport State](image-viewport-state.md). The item emits `stateChanged` whenever any snapshot field changes. Callers bind to fields inside `state`; independently projected flat item observations are not public API.

The item exposes imperative commands described in this document. Commands that can fail return `ImageViewportCommandResult`, which contains `outcome`, `reason`, `commandRevision`, and `snapshotRevision`. Presentation mutations are expressed through `ImageViewportPresentationCommand` instead of separate setter methods, so fit, zoom, pan, scan, rotation, mirror, spread direction, page gap, background, smoothing, mipmap, looping, quality, and exactness changes enter one validation and outcome path.

The item exposes coordinate helpers as snapshot-relative operations: `mapPoint(input): ImageViewportCoordinateResult`, `containsPoint(input): bool`, and `nearestVisiblePoint(input): ImageViewportCoordinateResult`. `ImageViewportCoordinateInput` names the source space, target space, optional page role, and point. Coordinate helpers read the current `state.display` and `state.presentation`; they do not mutate state or revisions.

## Source Handles

`ImageSequenceFactory` creates opaque `ImageSequence` handles from `ImageFrame`, `TimedImageFrameList`, or `ImageSequenceProviderAdapter`. The viewport accepts only `ImageSequence` handles; it does not discover files, URLs, archives, decoders, caches, predecode work, display-image stores, or QML image-provider entries.

Provider-backed construction, request values, event values, display demand, frame envelopes, and frame-handle ownership are defined in [ImageSequence Provider Protocol](image-sequence-provider-protocol.md). The provider adapter extension contract is defined in [ImageSequence Provider Adapter](image-sequence-provider-adapter.md).

## Command Values

`ImageViewportCommandResult` contains `outcome`, `reason`, `commandRevision`, and `snapshotRevision`. `commandRevision` identifies the command observation domain after the command is handled; `snapshotRevision` identifies the resulting snapshot. Rejected commands do not mutate request, display, or presentation state except for command diagnostics.

`clear()`, `play(role)`, `pause(role)`, `stop(role)`, `seek(role, frame)`, `seekToPosition(role, milliseconds)`, `setPresentationTarget(target, policy)`, `setPresentation(command)`, and `resetView()` return `ImageViewportCommandResult`. Coordinate helpers return coordinate results or booleans and do not allocate command revisions.

`ImageViewportCommandOutcome` values are `Accepted`, `Invalid`, `Unsupported`, and `IgnoredNoRequest`.

`ImageViewportCommandReason` values are `NoCommand`, `IgnoredNoRequest`, `InvalidRequest`, and `UnsupportedRequest`.

`ImageViewportPageRole` values are `Primary` and `Secondary`.

## Presentation-Target Replacement

`setPresentationTarget(target, policy)` is the only public mutating API that creates or replaces an accepted presentation target. A two-page spread is accepted only when primary and secondary roles are supplied in the same command. The viewport must not expose a transient primary-only accepted presentation target for one accepted two-role replacement.

`clear()` is equivalent to `setPresentationTarget(ImageViewportPresentationTarget.clear(), PresentationTargetTransitionPolicy.defaultClear())`. Clearing stops playback, removes retained display content, closes accepted generation interests, and preserves presentation preferences unless a separate accepted presentation command changes them.

`PresentationTargetTransitionPolicy` expresses caller intent for retain versus clear-before-load, fit and zoom transition, content-position transition, rotation transition, mirror transition, spread direction, page gap, and replacement intent. The viewport computes the resulting presentation state from canonical state; callers do not repair zoom, pan, spread geometry, or visible rectangles through ordered follow-up commands.

Invalid presentation target values or invalid transition policy fields reject the whole command before any state changes. The previous presentation target, presentation, retained or empty display, playback phase, provider work, diagnostics, and revision tokens remain unchanged except for the command diagnostic revision documented by `ImageViewportCommandResult`.

`PresentationTargetTransitionPolicy.DisplayTransition` values are `RetainPrevious` and `ClearBeforeLoad`.

`PresentationTargetTransitionPolicy.ZoomTransition` values are `Preserve`, `ResetToContain`, and `PreserveManualPercent`.

`PresentationTargetTransitionPolicy.ContentPositionTransition` values are `Preserve`, `Clamp`, `ScanStart`, and `ScanEnd`.

`PresentationTargetTransitionPolicy.RotationTransition` values are `Preserve` and `Reset`.

`PresentationTargetTransitionPolicy.MirrorTransition` values are `Preserve` and `Reset`.

`PresentationTargetTransitionPolicy.FitModeTransition`, `PresentationTargetTransitionPolicy.SpreadDirectionTransition`, and `PresentationTargetTransitionPolicy.PageGapTransition` values are `Preserve` and `SetExplicit`. `SetExplicit` uses the corresponding `fitMode`, `spreadDirection`, or `pageGap` value in the same policy object.

`PresentationTargetTransitionPolicy.ReplacementIntent` values are `NewTarget` and `SameTargetRefinement`.

`PresentationTargetTransitionPolicy` contains `displayTransition`, `zoomTransition`, `contentPositionTransition`, `rotationTransition`, `mirrorTransition`, `fitModeTransition`, `fitMode`, `spreadDirectionTransition`, `spreadDirection`, `pageGapTransition`, `pageGap`, and `replacementIntent`. `fitMode` uses `ImageViewportFitMode`, `spreadDirection` uses `ImageViewportSpreadDirection`, and `pageGap` is a finite non-negative source-logical distance in spread space. When `fitModeTransition`, `spreadDirectionTransition`, or `pageGapTransition` is `SetExplicit`, the corresponding value field is required and validated; when the transition is `Preserve`, the corresponding value field is ignored for state mutation but still must be well-formed if present. `SameTargetRefinement` is the public carrier for caller-supplied presentation-target replacements that keep the same application-owned source logical identity and source logical size but replace payload detail for the same target; the viewport trusts the caller's identity intent and validates only public presentation-target shape, metadata consistency, and logical-size equality. `SameTargetRefinement` rejects presentation-transition mutations other than `displayTransition`: `zoomTransition` must be `Preserve`, `contentPositionTransition` must be `Clamp` or `Preserve`, `rotationTransition` and `mirrorTransition` must be `Preserve`, and `fitModeTransition`, `spreadDirectionTransition`, and `pageGapTransition` must be `Preserve`.

Default clear policy uses `displayTransition: ClearBeforeLoad`, `zoomTransition: Preserve`, `contentPositionTransition: Clamp`, `rotationTransition: Preserve`, `mirrorTransition: Preserve`, `fitModeTransition: Preserve`, `spreadDirectionTransition: Preserve`, `pageGapTransition: Preserve`, and `replacementIntent: NewTarget`; it resets playback to stopped, preserves presentation preferences, clears retained display content, and its ignored explicit `fitMode`, `spreadDirection`, and `pageGap` values default to `Contain`, `LeftToRight`, and `0`. Default non-clear replacement policy uses `displayTransition: RetainPrevious`, `zoomTransition: Preserve`, `contentPositionTransition: Clamp`, `rotationTransition: Preserve`, `mirrorTransition: Preserve`, `fitModeTransition: Preserve`, `spreadDirectionTransition: Preserve`, `pageGapTransition: Preserve`, and `replacementIntent: NewTarget`; its ignored explicit `fitMode`, `spreadDirection`, and `pageGap` values default to `Contain`, `LeftToRight`, and `0`.

## Presentation Commands

`ImageViewportPresentationCommand` is the only public value used for presentation mutations. It is a structured value whose operation fields are optional and whose field presence is explicit; omitted fields do not mutate their domains. It contains one or more explicit operation fields selected from `fitMode`, `manualZoomPercent`, `zoomStepDelta`, `contentPosition`, `panDelta`, `scanDirection`, `rotationDegrees`, `mirrorHorizontally`, `mirrorVertically`, `spreadDirection`, `pageGap`, `backgroundMode`, `backgroundColor`, `smoothing`, `mipmap`, `looping`, `qualityPreference`, `exactnessPreference`, and `resetView`. The command validates as one transaction before applying any operation; if any supplied operation is malformed, unsupported for the current accepted request, duplicates another operation for the same domain, or conflicts with another operation in the same command, the whole presentation command is rejected.

Presentation command numeric fields must be finite. `manualZoomPercent` is valid only within the current manual zoom range; `zoomStepDelta` is a signed integer step count; `contentPosition` and `panDelta` are item content-space points; `scanDirection` uses `ImageViewportScanDirection`; `rotationDegrees` is one of `0`, `90`, `180`, or `270`; `pageGap` is a finite non-negative source-logical distance in spread space. The geometry-positioning conflict domain contains `manualZoomPercent`, `zoomStepDelta`, `contentPosition`, `panDelta`, and `scanDirection`; at most one operation from that domain may be present in one command. `resetView` is mutually exclusive with `fitMode`, every geometry-positioning operation, `rotationDegrees`, `mirrorHorizontally`, and `mirrorVertically`, but may be combined with background, smoothing, mipmap, looping, quality, and exactness preference changes.

`resetView()` is equivalent to `setPresentation(ImageViewportPresentationCommand.resetView())`. It resets fit mode to `Contain`, clears manual pan to the scan start for the current spread direction, clears display rotation and mirror flags, preserves background, quality, and exactness preferences, and leaves the accepted presentation target and playback state unchanged unless another accepted command changes them.

`ImageViewportFitMode` values are `Contain`, `FitWidth`, `FitHeight`, and `Manual`.

`ImageViewportSpreadDirection` values are `LeftToRight` and `RightToLeft`.

`ImageViewportScanDirection` values are `Start`, `Previous`, `Next`, and `End`.

`ImageViewportBackgroundMode` values are `Transparent`, `SolidColor`, and `Checkerboard`.

`ImageViewportQualityPreference` values are `Default`, `FastFirstDisplay`, `BalancedDetail`, and `ExactDetail`. The preference is advisory unless paired with an exactness preference that rejects inexact payloads.

`ImageViewportExactnessPreference` values are `Default`, `AllowInexact`, `PreferExact`, and `RequireExact`. `RequireExact` makes inexact provider payloads inadmissible for the request; other values allow providers to return earlier complete-frame payloads when their envelopes remain valid.

## Rendering Ownership

`ImageViewport` owns rendering for its accepted presentation target. The public API exposes source handles, commands, snapshot state, coordinate helpers, provider requests, provider events, diagnostics, limits, and revision tokens; it does not expose scene graph nodes, native textures, QML image-provider URLs, Qt Quick `Image` load status, load acknowledgement callbacks, render-frame projection APIs, or caller-managed render resources.

The render backend may be implemented with Qt Quick scene graph resources, but backend selection and resource lifetime are private to the item. Applications that need source lookup, cache reuse, display-image store lifetime, predecode, or refinement policy implement those concerns behind `ImageSequenceProviderAdapter`; they do not drive viewport rendering through external provider URLs.
