# Presentation Geometry

Presentation geometry is an engine-owned architecture boundary. It defines how accepted source logical pages, viewport presentation preferences, item geometry, and retained display identity become public rectangles, coordinate conversion, render placement, demand projection, content anchoring, and pannability.

## Coordinate Authority

Logical page and spread geometry are the authority for public coordinate conversion. Payload raster size, texture placement, backend source rectangles, and upload rectangles are render details that must not change item-to-spread or item-to-page mapping. Device-pixel ratio participates only through the public physical-pixel-aware zoom contract: `zoomPercent: 100` maps one source logical page or spread unit to one physical display pixel, and the item logical transform is derived from the effective device pixel ratio.

Page and spread bounds are half-open logical rectangles. Coordinate sampling and hit testing reject the excluded right and bottom edges rather than clamping invalid coordinates to the last pixel.

## Pipeline

For each target or displayed identity, the geometry pipeline builds a virtual spread canvas from its primary role and optional secondary role. Spread direction chooses page placement, page gap participates in spread bounds, and unequal pages are centered on the unrotated spread's cross axis. The pipeline then applies normalized quarter-turn display rotation, derives fit or manual scale from the rotated spread and viewport frame, resolves and clamps viewport-owned content position, and applies horizontal and vertical mirroring in item axes. The same projection feeds render placement, content size and bounds, pannability, public rectangles, hit testing, content anchoring, coordinate conversion, and provider display demand.

The engine is the sole production authority for the geometry projection. Public snapshot projection and render synchronization consume the engine-authored projection for the current display or pending render target; item-private and render-local code may adapt that projection to API return values or render adapter fields, but must not reconstruct canonical presentation-geometry state from mutable item, sequence, provider, or render state.

## Canonical Presentation State

The geometry boundary consumes the canonical geometry-related presentation state: fit mode, manual zoom demand, content position, spread direction, page gap, normalized quarter-turn rotation, and mirroring. Background, smoothing, mipmap, looping, quality, and exactness remain canonical presentation fields but do not become geometry authority. Placement primitives such as fill mode, alignment, raw item-scale zoom, and raw item-space pan are derived outputs rather than independent state.

`Contain` preserves the full rotated spread inside the item. `FitWidth` fits the rotated spread width. `FitHeight` fits the rotated spread height. `Manual` uses the persistent public manual zoom percent and clamps content position to viewport-owned bounds. Fit-mode selection and manual-demand updates are independent fields reduced in one engine transaction; a manual percentage or step update must not implicitly change fit mode. Step calculation uses overflow-safe arithmetic and clamps to the public manual range. A change to source logical size, item geometry, device-pixel ratio, or display limits recomputes the manual range and clamps both the stored manual demand and content position before one coherent projection is published.

On an axis where transformed content does not exceed the viewport frame, placement is centered, maximum content position is zero, and panning is disabled. On a pannable axis, the engine resolves `Start` and `End` from maximum content position and spread direction using the public formulas before publication. The geometry boundary provides no previous/next step or nearest-point policy; applications express those policies through direct position, pan, mapping, and containment primitives.

Display rotation is normalized quarter-turn presentation state, not source metadata. Rotation is applied to rendering, content rectangles, visible spread and page rectangles, hit testing, coordinate conversion, fit-derived zoom, pan bounds, scroll bounds, and content-anchor bounds. Source orientation policy remains payload metadata normalized during preparation; display rotation remains viewport presentation state, and the render path must not introduce arbitrary-angle viewport rotation semantics.

The engine maintains separate target and displayed presentation projections when retained pixels are visible. Accepted role geometry and provider demand use the target projection; displayed role geometry, coordinate helpers, and render placement for retained pixels use the displayed projection. Projection identities advance only for inputs that can affect their render or geometry result, independently of the broader canonical-presentation revision used for all preferences. The projections may share immutable inputs but must not mix target presentation state with retained displayed identities.

## Target Transition Reduction

The engine reduces presentation-target policy together with target replacement in one transaction. It validates every enum, required explicit value, same-target compatibility fact, and cross-field conflict before allocating effects. `ZoomTransition.Preserve` retains manual zoom demand while fit-mode transition independently preserves or explicitly selects the active mode. `ResetToContain` atomically selects `Contain` and resets manual demand to clamped `100`, and therefore cannot be combined with an explicit fit-mode transition. Content-position `Clamp` retains the prior numeric offset and clamps it to the new bounds; `AnchorStart` and `AnchorEnd` resolve from the new bounds and spread direction. Same-target refinement admits only the presentation-preserving combination defined by the public presentation contract.

When target source geometry is unavailable at replacement time, the engine owns an unresolved transition intent scoped to the new target generation. The first authoritative compatible geometry resolves that intent exactly once before accepted target geometry or render effects are published. Metadata for a retired generation cannot resolve or overwrite a newer target's presentation state.

Presentation mutation commands use the same serialized reducer. The engine validates the complete optional-field set, geometry requirements, range constraints, and conflict domains before applying any field; no host may impose operation order or partially apply a command. Geometry-independent preferences remain canonical without a target, while geometry-dependent commands are rejected until accepted target geometry exists.

## Non-Positive Item Geometry

When item size is non-positive, render synchronization may keep the last committed payload identities but emits no presentable geometry for coordinate conversion. If that committed payload set already belongs to the accepted request, request and display status remain ready while presentable rectangles and coordinate conversions report no presentable image area; displayed page sizes remain the committed payloads' source logical page sizes. Prepared payloads for a new active request or target spread remain pending with render-waiting request state until positive item geometry allows a payload or spread commit; non-positive geometry is not a render failure.
