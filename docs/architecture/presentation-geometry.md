# Presentation Geometry

Presentation geometry is an engine-owned architecture boundary. It defines how accepted source logical pages, viewport presentation preferences, item geometry, and retained display identity become public rectangles, coordinate conversion, render placement, demand projection, scan movement, and pannability.

## Coordinate Authority

Logical page and spread geometry are the authority for public coordinate conversion. Payload raster size, texture atlas placement, backend source rectangles, and upload rectangles are render details that must not change QML-visible item-to-spread or item-to-page mapping. Device-pixel ratio participates only through the public physical-pixel-aware zoom contract: `zoomPercent: 100` maps one source logical page or spread pixel to one physical display pixel, and the item logical transform is derived from the effective device pixel ratio.

Page and spread bounds are half-open logical rectangles. Coordinate sampling and hit testing reject the excluded right and bottom edges rather than clamping invalid coordinates to the last pixel.

## Pipeline

The geometry pipeline first builds a virtual spread canvas from the accepted primary role and optional secondary role. Reading direction chooses left-to-right or right-to-left page placement, the default gap is zero, and any explicit gap is part of spread bounds. The pipeline then applies normalized quarter-turn display rotation to the spread presentation, derives fit-mode placement from the rotated spread bounds and viewport frame, applies physical-pixel-aware zoom percent, resolves viewport-owned content position and scroll bounds, and applies independent horizontal and vertical mirroring consistently with the chosen coordinate mapping. The same pipeline feeds scene graph mapping, content size, content position, maximum content position, pannability, public rectangles, hit testing, scan movement, coordinate conversion, and provider display-demand projection.

The engine is the sole production authority for the geometry projection. Public snapshot projection and render synchronization consume the engine-authored projection for the current display or pending render target; item-private and render-local code may adapt that projection to API return values or render adapter fields, but must not reconstruct canonical presentation-geometry state from mutable item, sequence, provider, or render state.

## Canonical Presentation State

Persistent presentation state is canonical: fit mode, manual zoom demand, content position, spread direction, page gap, background, normalized quarter-turn rotation, mirroring, quality preference, and exactness preference. Placement primitives such as fill mode, alignment, raw item-scale zoom, and raw item-space pan are not engine state; compatible centering, clamping, and fit behavior is derived inside the presentation geometry pipeline from the canonical values.

`Contain` preserves the full rotated spread inside the item. `FitWidth` fits the rotated spread width. `FitHeight` fits the rotated spread height. `Manual` preserves the public manual zoom percent across device-pixel-ratio, item-size, image-size, spread-size, page-gap, and quarter-turn rotation changes, then clamps or anchor-preserves content position according to viewport-owned scroll rules.

Display rotation is normalized quarter-turn presentation state, not source metadata. Rotation is applied to rendering, content rectangles, visible spread and page rectangles, hit testing, coordinate conversion, fit-derived zoom, pan bounds, scroll bounds, and scan movement. Source orientation policy remains payload metadata normalized during preparation; display rotation remains viewport presentation state, and the render path must not introduce arbitrary-angle viewport rotation semantics.

## Non-Positive Item Geometry

When item size is non-positive, render synchronization may retain the last committed payload identities but emits no presentable geometry for coordinate conversion. If the retained payload set already belongs to the accepted request, request and display status remain ready while presentable rectangles and coordinate conversions report no presentable image area; displayed page sizes remain the committed payloads' source logical page sizes. Prepared payloads for a new active request or target spread remain pending with render-waiting request state until positive item geometry allows a payload or spread commit; non-positive geometry is not a render failure.
