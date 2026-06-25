# ImageViewport API

This document records the intended public API shape for QML and C++ callers. Names may evolve before the first stable release, but the concepts below are the public surface the implementation should preserve.

## QML Item

`ImageViewport` should be a `QQuickItem` with a `sequence` property accepting only an `ImageSequence` object or `null`.

The item should expose request status, request status reason, display status, displayed frame, displayed position, displayed image size, content rectangle, visible image rectangle, and display revision observations.

The item should expose presentation properties for fill mode, horizontal and vertical alignment, smoothing, mipmap request, mirroring, background mode, background color, zoom, and pan.

The item should expose imperative commands for `clear()`, `play()`, `pause()`, `stop()`, `seek(frame)`, `seekToPosition(milliseconds)`, and `resetView()`. Commands that can fail should return an immediate outcome rather than requiring callers to infer command acceptance from retained display state.

Coordinate helpers should map item points to image points, image points to item points, and test whether an image point is inside the currently visible image area.

## Sequence Construction

`ImageSequence` is an opaque caller-facing handle for immutable or provider-backed image content. Callers should create sequences through explicit helpers instead of assigning raw inputs to the viewport.

The baseline construction concepts are a still image sequence, a timed frame list sequence, and a provider-backed sequence. QML-callable helpers should accept only QML-representable helper objects; C++ helpers may accept native image values when appropriate.

Invalid construction input should fail at construction time with synchronous factory diagnostics. A valid provider-backed sequence may still later report unsupported or error status after assignment if provider startup or content requests fail.

## Frames And Timed Lists

`ImageFrame` represents an immutable display-ready frame payload and its metadata. The public API should avoid exposing mutable raw pixel storage as viewport state.

`TimedImageFrameList` is a builder for explicit animated sequences. It should own typed frame-duration entries, expose a count, support append and clear operations, and report construction diagnostics when invalid entries are rejected.

Frame durations projected to QML should use milliseconds. Provider-origin timing may preserve fractional precision internally, but QML-facing duration and playback position values should use the same time unit consistently.

## Provider Adapter

`ImageSequenceFactory.fromProvider(...)` or the equivalent C++ helper should adapt an application-owned provider adapter into an `ImageSequence`. The provider object itself is not assigned directly to the viewport.

The public provider adapter surface should be sufficient for downstream applications to implement custom sources without including private headers or touching scene graph objects.

Provider integration is a trusted in-process extension point. The API should document bounded entry-point expectations and map detectable adapter failures into deterministic construction, unsupported, or error states.

## Status And Diagnostics

Public enum values should be scoped and stable enough for QML branching. Broad categories should distinguish no request, loading, ready, unsupported, and error states, plus request reasons that explain provider waiting, request queueing, render deferral, upload progress, unsupported requests, invalid requests, and provider failures.

`errorString`, `warningString`, and factory diagnostic strings should be user-presentable, redacted, bounded plain text. Structured internal diagnostics may exist for tests and debugging, but they are not a general public diagnostic history unless a concrete caller need promotes them.

## Deferred API

The initial public API should not include implicit file or URL loading properties on `ImageViewport`, built-in gesture controls, QML callback providers, scene graph resource injection, native texture payloads, tiled loading, region loading, or full color-management policy. These can be added later as explicit factories, adapters, or presentation policies with their own contracts.
