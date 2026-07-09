# Architecture Index

This directory defines the durable end-state architecture contract for `ImageViewport`. It focuses on boundaries, ownership, invariants, and data flow rather than transient plans, implementation status, or exhaustive verification inventories.

- [Subsystem Boundaries](subsystem-boundaries.md)
- [Provider Protocol](provider-protocol.md)
- [Rendering](rendering.md)
- [Presentation Geometry](presentation-geometry.md)
- [Build And Package Boundary](build-and-package.md)
- [Playback State Machine](playback-state-machine.md)

## Traceability

Major public guarantees in `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, `docs/spec/image-viewport-state.md`, `docs/spec/image-sequence-provider-protocol.md`, `docs/spec/image-sequence-provider-adapter.md`, and `docs/spec/image-viewport-packaging.md` trace to the subsystem contracts here: presentation-target ownership, retained display, request/display status, command admission, diagnostics, limits, sequence construction, factory validation, provider construction facts, and presentation state are enforced by [Subsystem Boundaries](subsystem-boundaries.md); geometry, coordinate conversion, display rotation, mirroring, and non-positive item geometry are enforced by [Presentation Geometry](presentation-geometry.md); render commit, render ownership, background, and quality fallback are enforced by [Rendering](rendering.md); provider-backed session lifetime, token matching, metadata readiness, payload admission, cancellation, and provider failure classification are enforced by [Provider Protocol](provider-protocol.md); installed Linux/static package boundaries are enforced by [Build And Package Boundary](build-and-package.md); playback phases, single-driver ownership, stop/pause/seek ordering, authored autoplay, and loop handling are enforced by [Playback State Machine](playback-state-machine.md).
