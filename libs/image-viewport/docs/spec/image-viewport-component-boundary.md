# ImageViewport Component Boundary

This document defines component limits and the supported repository-internal integration boundary for `ImageViewport`.

## Component Limits

`ImageSequenceLimits` is a QML singleton and C++ type with `maximumSourceLogicalWidth: INT_MAX`, `maximumSourceLogicalHeight: INT_MAX`, `maximumSourceLogicalPixels: qint64(INT_MAX) * qint64(INT_MAX)`, `maximumPayloadRasterWidth: 16384`, `maximumPayloadRasterHeight: 16384`, `maximumPayloadBytes: 536870912`, `maximumTimedListPayloadBytes: 536870912`, `maximumFrameCount`, `maximumTotalDurationMilliseconds`, `maximumFrameDurationMilliseconds`, `maximumDiagnosticCharacters`, and `maximumFormatIdentifierCharacters`. Character limits count Unicode scalar values. Source logical components are finite positive integers. Source logical limits bound geometry identity and do not authorize or imply allocation proportional to logical pixel count. `maximumPayloadBytes` bounds each frame's declared payload byte size, while `maximumTimedListPayloadBytes` bounds the sum of declared payload byte sizes retained by an in-memory timed list; every list entry contributes its declared size even when entries share a frame or backing storage. Payload raster and byte limits independently bound retained CPU payload resources. Metadata, payload, timing, and format-identifier limits are unconditional admission bounds; `maximumDiagnosticCharacters` is the post-redaction maximum for any published diagnostic.

`ImageViewportDisplayLimits` is a QML singleton and C++ type with `minimumManualZoomPercent: 10`, `manualZoomStepFactor: 1.0905077326652577`, `maximumPageGap: 8192`, `minimumCheckerboardCellSize: 1`, and `maximumCheckerboardCellSize: 256`. The singleton has no static maximum-manual-zoom value; `state.presentation.maximumManualZoomPercent` is the authoritative accepted-target-dependent maximum and uses `0` when unavailable. A positive manual zoom step count increases manual zoom and a negative count decreases it. Page gap uses source-logical spread units; checkerboard cell size uses item-logical units. Per-request backend observations such as `maximumTextureSize`, `maximumPayloadBytes`, and `displayByteBudget` remain demand fields rather than singleton constants and may be invalid when the viewport cannot know them safely.

Every available published maximum, minimum, and step is finite and positive, and values are identical in QML and C++. The dynamic maximum-manual-zoom sentinel `0` is the sole unavailable maximum and is never a valid zoom demand. Validation uses overflow-safe calculations and rejects a value before any dependent allocation or display attempt when the applicable limit would be exceeded. Invalid or unavailable runtime backend caps are exposed as invalid demand fields rather than guessed values; they do not relax `ImageSequenceLimits`.

## Repository-Internal Integration Boundary

`ImageViewport` exposes a repository-internal interface solely to KiriView. It is not an independently consumable SDK or QML module and makes no standalone QML URI, import-version, package-target, ABI, semantic-versioning, or source-compatibility promise.

The supported C++ interface requires ISO C++23. KiriView and `ImageViewport` evolve together and do not provide C++17 or C++20 source-compatibility modes for this interface.

The component exposes one intentional C++ include surface to KiriView. `<ImageViewport/ImageViewport>` is the umbrella header for that surface; repository code may instead include only the canonical subject headers it uses:

- `<ImageViewport/imageviewporttypes.h>` provides shared public values, roles, ranges, and opaque tokens.
- `<ImageViewport/imagesequence.h>` provides image sequences, frames, typed factory results, factories, and sequence source limits.
- `<ImageViewport/imagesequenceprovider.h>` provides the application-owned provider extension contract and its protocol values.
- `<ImageViewport/imageviewport.h>` provides the viewport item, presentation command surface, display-demand limits, and application-shutdown provider-cleanup completion boundary.
- `<ImageViewport/imageviewportstate.h>` provides state snapshots, command results, and coordinate input and result values.

The umbrella and subject-specific include forms are the supported repository-internal component interface. KiriView and `ImageViewport` evolve atomically in the same repository, so an intentional contract change updates both sides without compatibility shims or parallel legacy APIs. The interface excludes private engine controllers, provider transport internals, render adapters, scene graph resources, native texture handles, and instrumentation types.
