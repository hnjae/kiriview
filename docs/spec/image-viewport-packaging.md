# ImageViewport Packaging

This document defines public limits and the supported installed package boundary for downstream consumers of `ImageViewport`.

## Public Limits

`ImageSequenceLimits` is a QML singleton and C++ type with `maximumSourceLogicalWidth`, `maximumSourceLogicalHeight`, `maximumSourceLogicalPixels`, `maximumPayloadRasterWidth`, `maximumPayloadRasterHeight`, `maximumPayloadBytes`, `maximumFrameCount`, `maximumTotalDurationMilliseconds`, `maximumFrameDurationMilliseconds`, `maximumDiagnosticCharacters`, and `maximumFormatIdentifierCharacters`. Character limits count Unicode scalar values. Source, metadata, payload, timing, and format-identifier values are unconditional admission bounds; `maximumDiagnosticCharacters` is the post-redaction maximum for any published diagnostic.

`ImageViewportDisplayLimits` is a QML singleton and C++ type with `minimumManualZoomPercent: 1`, `maximumManualZoomPercent: 10000`, `manualZoomStepFactor: 1.25`, `maximumPageGap: 8192`, `minimumCheckerboardCellSize: 1`, and `maximumCheckerboardCellSize: 256`. A positive manual zoom step count increases manual zoom and a negative count decreases it. Page gap uses source-logical spread units; checkerboard cell size uses item-logical units. Per-request backend observations such as `maximumTextureSize`, `maximumPayloadBytes`, and `displayByteBudget` remain demand fields rather than singleton constants and may be invalid when the viewport cannot know them safely.

Every published maximum, minimum, and step is finite and positive, each minimum is less than or equal to its corresponding maximum, and values are identical in QML and C++. Validation uses overflow-safe calculations and rejects a value before any dependent allocation or display attempt when the applicable limit would be exceeded. Invalid or unavailable runtime backend caps are exposed as invalid demand fields rather than guessed values; they do not relax `ImageSequenceLimits`.

## Installed Package Boundary

The supported install target is Linux with a static Qt QML module plugin. The QML module URI is `ImageViewport`, the supported import version is `1.0`, and the package compatibility version is `1.0.0`. Installed consumers import `ImageViewport 1.0`, use public headers, and link the `ImageViewport::ImageViewport` package target only. Public API signatures and supported extension points must not expose private controller types, internal provider transport, render adapters, scene graph resources, native texture handles, or instrumentation types.

`<ImageViewport/ImageViewport>` is the umbrella header for the complete public C++ API. Consumers may instead include only the canonical subject headers they use:

- `<ImageViewport/imageviewporttypes.h>` provides shared public values, roles, ranges, and opaque tokens.
- `<ImageViewport/imagesequence.h>` provides image sequences, frames, typed factory results, factories, and sequence source limits.
- `<ImageViewport/imagesequenceprovider.h>` provides the application-owned provider extension contract and its protocol values.
- `<ImageViewport/imageviewport.h>` provides the viewport item, presentation command surface, and display-demand limits.
- `<ImageViewport/imageviewportstate.h>` provides state snapshots, command results, and coordinate input and result values.

The umbrella and subject-specific include forms are both supported package interfaces. All public headers belong to one C++ library, one QML module, and one package compatibility and release boundary; header separation does not create independently versioned APIs. Within major version `1`, minor releases may add backward-compatible API and patch releases may correct behavior within existing guarantees. Removing or renaming public API, rejecting previously valid input, changing value semantics incompatibly, or lowering a published hard limit requires a new major version. A downstream application or provider must be implementable without private headers, source-tree include paths, or build-tree-only QML imports.

## Open Question

- The concrete numeric values for the named source limits require representative image workloads and Qt/backend measurements before they can be fixed responsibly.
