# Milestones

This roadmap records implementation sequencing. It is intentionally lighter than a release gate: it describes goals, non-goals, and acceptance summaries without coverage IDs, manifests, or platform evidence policy.

## M0: Qt Quick Item Scaffold

Goal: establish a buildable Qt Quick module with an `ImageViewport` item that imports from QML, constructs safely, participates in scene graph updates, and has a minimal automated Qt test.

Acceptance summary: the module configures with CMake, the QML import works from the build tree, the item can be created in a test engine, and the item advertises content-bearing behavior without crashing under offscreen test execution.

Non-goals: sequence providers, playback, custom factories, provider adapters, real rendering validation, public packaging guarantees, and release-gate automation.

## M1: First Usable Viewport

Goal: make the viewport useful for caller-supplied in-memory image sequences and a narrow provider-backed path while preserving clean separation between source adaptation, request state, and render presentation.

Acceptance summary: callers can assign an explicit sequence, display at least one generated frame, observe request/display status, seek within supported content, use basic playback for timed frames, inspect content geometry, and clear or replace content without stale state leaks.

Non-goals: implicit file or URL loading, archive extraction, network policy, broad decoder integration, native texture import, tiled or region loading, built-in gestures, exhaustive backend pixel conformance, and automated coverage-manifest gating.

## Later Milestones

Later work can add file and URL factories, richer provider classes, decoder adapters, color-management policy, native texture paths, region loading, reusable gestures, stricter visual conformance, and release-gate automation once the implementation has enough surface area to justify that machinery.
