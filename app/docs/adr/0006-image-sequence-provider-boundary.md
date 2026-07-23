# ImageSequence Provider Boundary

## Context

KiriView previously coupled source decoding and application cache policy to an application-owned image-rendering path. That duplicated responsibilities now delegated through the `ImageViewport` boundary and made source work depend on presentation implementation details.

Rust ownership of Qt-independent SVG parsing and rasterization remains valuable, but its output must enter the same supported provider boundary as other image sources.

## Decision

KiriView supplies image data only through the supported `ImageSequence` provider interface. Application decoding, caching, predecode, and refinement may use source-private techniques, but their results must satisfy that public payload contract and must not create an alternate application-owned presentation path.

Rust continues to own static SVG parsing and rasterization through `usvg`, `resvg`, and `tiny-skia`. The language boundary accepts plain SVG bytes and target geometry and returns plain raster bytes or intrinsic size values. C++ owns Qt payload construction, application cache and lease state, provider adaptation, async job lifetime, and stale-completion rejection.

## Consequences

ADR 0003 remains authoritative for Rust ownership of self-contained static SVG parsing and rasterization and for its supported-content restrictions.

Still-image and animation source adapters share the public provider boundary. This ADR defines KiriView's source and provider obligations only; payload shapes, limits, ownership rules, and presentation behavior come from the dependency's supported contract.
