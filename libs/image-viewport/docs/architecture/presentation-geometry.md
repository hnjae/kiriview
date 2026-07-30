# Presentation Geometry

Presentation geometry is an engine-owned architecture boundary. It defines how accepted source logical pages, viewport presentation preferences, item geometry, and retained display identity become public rectangles, coordinate conversion, render placement, demand projection, content anchoring, and pannability. Observable geometry and command behavior are canonical in [ImageViewport](../spec/image-viewport.md#display-geometry), [ImageViewport Presentation](../spec/image-viewport-presentation.md), and [ImageViewport State](../spec/image-viewport-state.md).

## Coordinate Authority

Logical page and spread geometry are the authority for public coordinate conversion. Payload raster size, texture placement, backend source rectangles, and upload rectangles are render details that must not change item-to-spread or item-to-page mapping. Device-pixel ratio participates only through the public physical-pixel-aware zoom contract: `zoomPercent: 100` maps one source logical page or spread unit to one physical display pixel, and the item logical transform is derived from the effective device pixel ratio.

Page and spread bounds are half-open logical rectangles. Coordinate sampling and hit testing reject the excluded right and bottom edges rather than clamping invalid coordinates to the last pixel.

## Pipeline

For each target or displayed identity, the geometry pipeline builds a virtual spread canvas from its primary role and optional secondary role. Spread direction chooses page placement, page gap participates in spread bounds, and unequal pages are centered on the unrotated spread's cross axis. The pipeline then applies normalized quarter-turn display rotation, derives fit or manual scale from the rotated spread and viewport frame, resolves and clamps viewport-owned content position, and applies horizontal and vertical mirroring in item axes. The same projection feeds render placement, content size and bounds, pannability, public rectangles, hit testing, content anchoring, coordinate conversion, and provider display demand.

The engine is the sole production authority for the geometry projection. Public snapshot projection and render synchronization consume the engine-authored projection for the current display or pending render target; item-private and render-local code may adapt that projection to API return values or render adapter fields, but must not reconstruct canonical presentation-geometry state from mutable item, sequence, provider, or render state.

## Canonical Presentation State

The geometry boundary consumes the canonical geometry-related presentation state: fit mode, preferred manual zoom, effective zoom, content position, spread direction, page gap, normalized quarter-turn rotation, and mirroring. Background, smoothing, mipmap, looping, quality, and exactness remain canonical presentation fields but do not become geometry authority. Placement primitives such as fill mode, alignment, raw item-scale zoom, and raw item-space pan are derived outputs rather than independent state.

The fit modes, manual-zoom fields, and their observable calculations follow the public presentation and state contracts. The geometry owner applies fit-mode selection, preferred-zoom updates, effective-zoom clamping, and content-position clamping as one coherent state change; a preferred percentage or step update cannot implicitly change fit mode.

Manual-range authority belongs to accepted-target presentation geometry and uses the formula and unavailable value defined by the public state contract. It consumes the complete accepted role set, page gap, normalized rotation, positive item size, and valid device-pixel ratio; retained displayed geometry is never a fallback input. Relevant geometry changes reconcile effective zoom and content position from the unchanged preference before public projection and provider demand, while an unavailable range preserves the preference.

Preferred-zoom, step, and anchor commands follow the public presentation contract and apply atomically against one accepted geometry projection. Anchoring preserves the specified unrotated spread-plane point through the final clamped geometry. The geometry boundary does not acquire nearest-point or snapping policy, and its internal arithmetic and transaction representation may vary while preserving the public finite, overflow-safe result.

Content bounds, pan clamping, anchors, zoom, snapshot bounds, and pannability consistently consume the effective maximum and tolerance defined by the public state contract. The geometry boundary provides no previous/next step or nearest-point policy; applications express those policies through direct position, pan, mapping, and containment primitives.

Display rotation is normalized quarter-turn presentation state, not source metadata. Rotation is applied to rendering, content rectangles, visible spread and page rectangles, hit testing, coordinate conversion, fit-derived zoom, pan bounds, scroll bounds, and content-anchor bounds. Source orientation policy remains payload metadata normalized during preparation; display rotation remains viewport presentation state, and the render path must not introduce arbitrary-angle viewport rotation semantics.

The engine maintains separate target and displayed presentation projections when retained pixels are visible. Accepted role geometry and provider demand use the target projection; displayed role geometry, coordinate helpers, and render placement for retained pixels use the displayed projection. Projection identities advance only for inputs that can affect their render or geometry result, independently of the broader canonical-presentation revision used for all preferences. The projections may share immutable inputs but must not mix target presentation state with retained displayed identities.

## Target Transition Reduction

Presentation-target policy and target replacement apply as one transaction under the validation and transition behavior defined by the public presentation contract. The geometry boundary preserves preferred-versus-effective zoom authority, resolves target-scoped content-position intent only from matching geometry, and admits same-target refinement only with the documented presentation-preserving combination. No host may impose follow-up mutation order.

When target source geometry is unavailable at replacement time, the engine owns an unresolved transition intent scoped to the new target generation. The first authoritative compatible geometry resolves that intent exactly once before accepted target geometry or render work is published. Metadata for a retired generation cannot resolve or overwrite a newer target's presentation state.

Presentation mutation commands apply atomically. The canonical owner validates the complete optional-field set, geometry requirements, final-geometry range constraints, and conflict domains before applying any field; no host may impose operation order or partially apply a command. Geometry-independent preferences remain canonical without a target, while geometry-dependent commands follow the availability rules in the public presentation contract. Rejection changes only command diagnostics, and an accepted change publishes geometry, projections, revisions, and provider demand from one coherent final state.

## Non-Positive Item Geometry

When item size is non-positive, render synchronization may keep the last committed payload identities but emits no presentable geometry for coordinate conversion. If that committed payload set already belongs to the accepted request, request and display status remain ready while presentable rectangles and coordinate conversions report no presentable image area; displayed page sizes remain the committed payloads' source logical page sizes. Prepared payloads for a new active request or target spread remain pending with render-waiting request state until positive item geometry allows a payload or spread commit; non-positive geometry is not a render failure.
