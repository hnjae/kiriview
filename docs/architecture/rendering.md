# Rendering

Rendering should translate prepared image payloads and viewport presentation state into Qt Quick scene graph updates. The durable baseline is a CPU-backed image path; native texture and tiled paths require explicit contracts before they become part of this architecture.

## Render Data Flow

Prepared frame payloads carry public logical image size, physical payload size, alpha information, timing identity, orientation identity, and any internal payload policy identity required by the installed backend. Public logical image size is the orientation-normalized image size used for geometry, coordinate conversion, and displayed image size; physical payload size and backend source rectangles remain render details. Full color-management policy is not part of the public viewport contract; internal color tags must not change public geometry, status, coordinate conversion, or request ordering. The render adapter receives that prepared payload plus item-space placement, source rectangle, mirroring, smoothing, mipmap request, background state, zoom, and pan.

The adapter should create or update scene graph resources only on the appropriate Qt Quick synchronization/render path. GUI-side cleanup should invalidate logical render state without dereferencing raw scene graph pointers after ownership has moved to the render side.

Render data is immutable for the duration of one synchronization attempt. Payload data and presentation data have separate identities. Payload changes require a new prepared payload identity and payload commit acknowledgement; presentation-only changes reuse the latest committed payload identity and advance presentation/display revision without resetting request readiness.

## Geometry

Logical image geometry is the authority for public coordinate conversion. Device-pixel ratio, texture atlas placement, backend source rectangles, and upload rectangles are render details that must not change QML-visible item-to-image mapping.

The render path should treat image bounds as half-open logical rectangles. Coordinate sampling and hit testing should reject the excluded right and bottom edges rather than clamping invalid coordinates to the last pixel.

The geometry pipeline applies fill mode from item bounds and public logical image size, alignment within the resulting placement, zoom around the placement center, pan in item logical pixels, and mirroring around the placement center. The same pipeline feeds scene graph mapping, public rectangles, hit testing, and coordinate conversion.

Contain preserves the full public logical image inside the item. Cover fills the item and uses alignment as the crop focus. Stretch maps the full public logical image to the item bounds without preserving aspect ratio. Center preserves public logical image size before zoom and uses alignment only when the item and image extents differ.

When item size is non-positive, render synchronization may retain the last committed payload identity but emits no presentable geometry for coordinate conversion. If the retained payload already belongs to the accepted request, request and display status remain ready while presentable rectangles and coordinate conversions report no presentable image area; `displayedImageSize` remains the committed payload's public logical image size. A prepared payload for a new active request remains pending with render-waiting request state until positive item geometry allows a payload commit; non-positive geometry is not a render failure.

## Quality Requests

Smoothing and mipmap settings are caller presentation preferences. If a backend cannot satisfy an optional quality request, the viewport may continue with the closest supported rendering mode. Warning diagnostics are advisory presentation text; correctness and caller branching depend on status, reason, capability, and geometry observations.

## Background

Transparent, solid color, and checkerboard backgrounds are item backing presentation. They fill the local item bounds behind image content and do not contribute to content geometry, image coordinate validity, or request readiness.

Background rendering is independent of payload admission. A background-only update must not advance request status, display frame, displayed position, or playback phase.

Background mode, background color, mirroring, zoom, pan, fill mode, alignment, and observable quality fallback are presentation-affecting display changes. They update display revision even when the committed payload identity is unchanged.
