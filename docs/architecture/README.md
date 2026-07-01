# Architecture Index

This directory defines the durable end-state architecture contract for `ImageViewport`. It focuses on boundaries, ownership, invariants, and data flow rather than transient plans, implementation status, or exhaustive verification inventories.

- [Subsystem Boundaries](subsystem-boundaries.md)
- [Provider Protocol](provider-protocol.md)
- [Rendering](rendering.md)
- [Playback State Machine](playback-state-machine.md)

## Traceability

Major public guarantees in `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, and `docs/spec/image-sequence-provider-adapter.md` trace to the subsystem contracts here: page-set ownership, retained display, request/display status, command admission, diagnostics, limits, and presentation state are enforced by [Subsystem Boundaries](subsystem-boundaries.md); geometry, coordinate conversion, display rotation, mirroring, non-positive item geometry, render commit, background, and quality fallback are enforced by [Rendering](rendering.md); provider-backed construction, session lifetime, token matching, metadata readiness, payload admission, cancellation, and provider failure classification are enforced by [Provider Protocol](provider-protocol.md); playback phases, single-driver ownership, stop/pause/seek ordering, autoplay, progressive animation readiness, and loop handling are enforced by [Playback State Machine](playback-state-machine.md).
