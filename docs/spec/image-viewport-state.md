# ImageViewport State

This document defines the canonical public snapshot schema, state enums, and revision tokens for `ImageViewport`. The item and command API is defined in [ImageViewport API](image-viewport-api.md), and provider demand/payload values referenced by role snapshots are defined in [ImageSequence Provider Protocol](image-sequence-provider-protocol.md).

## Snapshot Values

`ImageViewportStateSnapshot` contains only value types and object references that are safe for QML binding and C++ copying. It does not expose private controller, render, provider transport, scene graph, native texture, or instrumentation types.

`state.request: ImageViewportRequestSnapshot` contains `status`, `reason`, `playbackPhase`, `acceptedPresentationTargetGeneration`, `acceptedRoleSet`, `targetRoleSet`, `activeRole`, and `playbackRole`. It describes the accepted request rather than retained visible pixels. `acceptedPresentationTargetGeneration` uses `ImageViewportPresentationTargetGenerationToken`; `activeRole` and `playbackRole` are nullable `ImageViewportPageRole` values and are null when no role is active for command admission or playback.

`state.display: ImageViewportDisplaySnapshot` contains `status`, `phase`, `displayedPresentationTargetGeneration`, `displayedRoleSet`, `targetRoleSet`, `belongsToAcceptedPresentationTarget`, `retained`, `displayedPresentationRevision`, `targetPresentationRevision`, `spreadSize`, `contentRect`, `contentSize`, `contentPosition`, `maximumContentPosition`, `visibleSpreadRect`, `horizontalPannable`, and `verticalPannable`. `displayedPresentationTargetGeneration` uses `ImageViewportPresentationTargetGenerationToken`; `displayedPresentationRevision` and `targetPresentationRevision` use `ImageViewportRevisionToken`. `display.status` distinguishes empty display, ready display for the accepted request, and retained display from an earlier request. `display.phase` distinguishes `NoPresentation`, `PreviousActive`, `TransitioningPlaceholder`, and `CommittedActive` so callers can keep controls and render output from mixing old and new presentation facts. Display geometry and coordinate helpers use the displayed presentation identified by `displayedPresentationRevision`; when retained display is visible, that revision may differ from `state.revisions.presentation`, which identifies the accepted target presentation.

`state.presentation` contains `fitMode`, `zoomPercent`, `minimumManualZoomPercent`, `maximumManualZoomPercent`, `manualZoomStepFactor`, `rotationDegrees`, `mirrorHorizontally`, `mirrorVertically`, `spreadDirection`, `pageGap`, `backgroundMode`, `backgroundColor`, `smoothing`, `mipmap`, `looping`, `qualityPreference`, and `exactnessPreference`.

`state.primary` and `state.secondary` are `ImageViewportRoleSnapshot` values. Each role snapshot contains `present`, `sequence`, `request`, `display`, `metadata`, and `geometry`. `present` is true only when the role belongs to the accepted presentation target, and `sequence` is the accepted `ImageSequence` handle or null when absent.

`ImageViewportRoleRequestSnapshot` contains `belongsToAcceptedPresentationTarget`, `presentationTargetGeneration`, `role`, `frame`, `position`, `sourceLogicalSize`, and `demandRevision`. It describes the accepted target for the role, not retained visible pixels. `presentationTargetGeneration` uses `ImageViewportPresentationTargetGenerationToken`; `demandRevision` uses `ImageViewportDemandRevisionToken`. `frame` is `0` for still sequences, the resolved zero-based frame index for timed sequences, and `-1` when the target is not yet metadata-resolved. `position` is the requested millisecond position for position-based requests, the resolved frame-start position for timed frame and playback requests, and `-1` when unavailable.

`ImageViewportRoleDisplaySnapshot` contains `belongsToAcceptedPresentationTarget`, `retained`, `frame`, `position`, `sourceLogicalSize`, `payloadRasterSize`, `sourceToPayloadScale`, `quality`, `exactness`, `currentForDemand`, and `demandRevision`. It describes currently visible pixels for the role. `quality` uses `ImageViewportPayloadQuality`; `exactness` uses `ImageViewportPayloadExactness`; `demandRevision` uses `ImageViewportDemandRevisionToken`. `currentForDemand` is true only when the displayed payload was admitted for the active demand revision of the accepted role request.

`ImageViewportRoleMetadataSnapshot` contains `available`, `sourceLogicalSize`, `frameCount`, `totalDuration`, `frameSeekBounds`, `positionSeekBounds`, `frameSeekSupport`, `positionSeekSupport`, `timedPlaybackSupport`, `autoplay`, `progressiveAnimationReadiness`, `loopMode`, and `loopCount`. `available` is false until validated construction-time or runtime metadata is authoritative for the accepted generation. `loopMode` uses `ImageSequenceAuthoredAnimationLoopMode`; `loopCount` is positive for finite loops and `-1` when loop count is not applicable.

`ImageViewportRoleGeometrySnapshot` contains `acceptedPageRect`, `acceptedItemRect`, `acceptedVisiblePageRect`, `displayedPageRect`, `displayedItemRect`, and `displayedVisiblePageRect`. Accepted rectangles describe the accepted target role; displayed rectangles describe visible pixels and may point at retained content from an earlier generation.

Unavailable fields use explicit invalid values: frame and position values are `-1`, sizes and rectangles have non-positive dimensions, ranges are invalid, tokens are invalid, booleans are false, nullable object references are null, and capability support is `Unavailable`.

`state.diagnostics` contains bounded public `errorString`, `warningString`, and latest command diagnostic reason. Public diagnostics are redacted, plain text, and suitable for user presentation.

`state.revisions` contains opaque monotonic tokens for `request`, `display`, `presentation`, `command`, and `snapshot`. QML observes revision tokens as equality-only opaque values and must not receive a numeric representation that can be narrowed or compared arithmetically. C++ callers may copy and compare tokens for equality or change; numeric ordering is not a public contract.

## State Enums And Tokens

`ImageViewportRequestStatus` values are `NoRequest`, `Loading`, `Ready`, `Unsupported`, and `Error`.

`ImageViewportRequestReason` values are `NoRequest`, `ProviderWaiting`, `RequestQueued`, `UploadPending`, `RenderWaiting`, `Ready`, `UnsupportedRequest`, `InvalidRequest`, `ProviderFailure`, `PayloadRejection`, and `RenderFailure`.

`ImageViewportDisplayStatus` values are `Empty`, `Ready`, and `Retained`.

`ImageViewportDisplayPhase` values are `NoPresentation`, `PreviousActive`, `TransitioningPlaceholder`, and `CommittedActive`.

`ImageViewportPlaybackPhase` values are `Stopped`, `Playing`, `Waiting`, and `Paused`.

`ImageViewportCapabilitySupport` values are `Unavailable`, `False`, and `True`. `Unavailable` means capability information is not valid for the current role or request; it is not equivalent to `False`.

`ImageViewportRoleSet` is an equality-comparable value with `primary` and `secondary` booleans. A non-empty accepted role set must have `primary == true`.

`ImageViewportPresentationTargetGenerationToken` is an equality-only opaque value. Its default value is invalid; invalid generation tokens compare equal only to other invalid generation tokens. It identifies accepted or displayed presentation-target ownership and is not ordered by callers.

`ImageViewportRevisionToken` is an equality-only opaque value. Its default value is invalid; invalid tokens compare equal only to other invalid tokens. QML receives tokens as non-numeric values and compares them through token equality helpers, not JavaScript arithmetic or numeric ordering. C++ callers may copy tokens and compare equality, but must not rely on ordering, density, or the absence of gaps.

`ImageViewportDemandRevisionToken` is the equality-only token shape used by role request and display snapshots for provider display demand. It advances under the demand rules defined in [ImageSequence Provider Protocol](image-sequence-provider-protocol.md#provider-enums-and-tokens).
