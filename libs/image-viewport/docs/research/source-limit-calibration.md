---
researched_on: 2026-07-17
---

# Source Limit Calibration

This note records the unresolved empirical work behind the current `ImageSequenceLimits` values. It is non-normative; the repository component API and [ImageViewport Component Boundary](../spec/image-viewport-component-boundary.md) define the active contract.

## Current Candidates

The implementation currently publishes logical and payload side limits of 8192, a logical-pixel limit of 67,108,864, a payload-byte limit of 268,435,456, a timed-list frame-count limit of 10,000, frame and total duration limits of 86,400,000 milliseconds, a diagnostic limit of 4096 Unicode scalar values, and a format-identifier limit of 256 Unicode scalar values.

These values are consistently consumed by sequence construction, provider metadata admission, frame preparation, QML, and the repository-internal component boundary. That consistency establishes one limit authority but does not demonstrate that the values are optimal for representative image-viewer workloads.

## Missing Evidence

Calibration requires representative still, comic, illustration, animation, and large-page workloads across the supported Qt Quick backends. Measurements should cover CPU payload retention, upload cost, texture constraints, concurrent spread ownership, retained fallback, provider refinement, and failure behavior near each boundary.

Until that evidence exists, implementation work should preserve one shared authority and overflow-safe admission without claiming empirical optimality. A future limit change must update the component contract and KiriView integration atomically before implementation.
