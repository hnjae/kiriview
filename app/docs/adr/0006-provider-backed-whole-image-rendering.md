# Provider-Backed Whole-Image Rendering

Status: Partially superseded by [ADR 0007: Repository-Internal ImageViewport Ownership](0007-repository-internal-image-viewport-ownership.md).

## Context

KiriView previously planned static image presentation around visual tiles, custom scene-graph rendering, and SVG raster scale buckets. That design duplicated rendering policy below Qt Quick, coupled decode scheduling to viewport tile geometry, and conflicted with the provider-backed display architecture adopted for still images, accepted animation frames, and active-navigation thumbnails.

Rust ownership of Qt-independent SVG parsing and rasterization remains valuable. The superseded part of ADR 0003 is the decision to expose SVG tile rasterization and integrate it with tile scheduling, render-frame projection, custom scene-graph nodes, and scale-bucket compositing.

## Decision

Production image presentation uses provider-backed whole-image display entries. Decode and refinement work may use source-internal tiling, but it must assemble one accepted display `QImage` before publication. Qt Quick high-level image items own texture upload, filtering, and scene-graph rendering.

Rust continues to own static SVG parsing and rasterization through `usvg`, `resvg`, and `tiny-skia`. The language boundary accepts plain SVG bytes and whole-image target geometry and returns plain raster bytes or intrinsic size values. C++ owns `QImage` construction, display-entry lifetime, provider publication, async job lifetime, and stale-completion rejection.

SVG detail selection follows the same bounded whole-image refinement contract as other static images. It does not publish visual tiles, SVG scale-bucket draw layers, render-frame tile projections, or custom scene-graph resources.

## Consequences

ADR 0003 remains authoritative for Rust ownership of self-contained static SVG parsing and rasterization and for its supported-content restrictions. Its SVG tile source, tile planner, render-frame projection, scene-graph render node, tile cache key, and active-bucket compositing decisions are superseded. ADR 0007 supersedes this ADR's assignment of texture upload and scene-graph rendering to application-composed high-level Qt Quick image items; the whole-image provider contract remains current.

All production still-image and animation presentation shares one provider-entry lifecycle. Oversized demands resolve through bounded whole-image detail, unsupported state, or failure rather than rebuilding a visual tile fallback.
