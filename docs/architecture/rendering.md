# Rendering

Rendering should translate prepared image payloads and viewport presentation state into Qt Quick scene graph updates. The first durable boundary is a CPU-backed image path, with native texture or tiled paths deferred until they have explicit contracts.

## Render Data Flow

Prepared frame payloads carry logical image size, physical payload size, alpha information, timing identity, and presentation-relevant policy identity. The render adapter receives that prepared payload plus item-space placement, source rectangle, mirroring, smoothing, mipmap request, background state, zoom, and pan.

The adapter should create or update scene graph resources only on the appropriate Qt Quick synchronization/render path. GUI-side cleanup should invalidate logical render state without dereferencing raw scene graph pointers after ownership has moved to the render side.

## Geometry

Logical image geometry is the authority for public coordinate conversion. Device-pixel ratio, texture atlas placement, backend source rectangles, and upload rectangles are render details that must not change QML-visible item-to-image mapping.

The render path should treat image bounds as half-open logical rectangles. Sampling fixtures and hit testing should reject the excluded right and bottom edges rather than clamping invalid coordinates to the last pixel.

## Quality Requests

Smoothing and mipmap settings are caller presentation preferences. If a backend cannot satisfy an optional quality request, the viewport may continue with the closest supported rendering mode and expose a warning when useful.

## Background

Transparent, solid color, and checkerboard backgrounds are item backing presentation. They fill the local item bounds behind image content and do not contribute to content geometry, image coordinate validity, or request readiness.
