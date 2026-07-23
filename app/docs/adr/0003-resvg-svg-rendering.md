# Resvg SVG Rendering

## Context

KiriView requires static SVG rendering behavior that QtSvg does not provide completely, including correct handling of supported clipped content. SVG parsing and rasterization operate on source bytes and target geometry and do not need to own application workflow state or Qt runtime objects.

The image-presentation dependency is responsible for its own private rendering strategy. KiriView needs only a source-side rasterization capability that can answer its supported provider requests without exposing dependency internals across the Rust/C++ boundary.

## Decision

Rust owns Qt-independent static SVG parsing and rasterization through `usvg`, `resvg`, and `tiny-skia`. The Rust bridge exposes plain byte-oriented functions for intrinsic size and bounded rasterization. C++ owns invocation, scheduling, stale-result rejection, Qt payload construction, and adaptation to the supported `ImageViewport` provider interface.

The support boundary accepts self-contained static SVG content, including clip paths, and disables script execution, animation playback, external file references, and network resource loading.

## Consequences

SVG behavior no longer depends on QtSvg renderer feature coverage for the supported static subset.

The language boundary remains value-based: Rust receives SVG bytes and integer target geometry and returns byte buffers or size values, while C++ keeps all Qt object ownership.

KiriView may cache or refine valid SVG payloads according to application policy and public provider demand, but this ADR does not select or constrain the image-presentation dependency's rendering implementation.
