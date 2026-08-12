# Video Thumbnail Extraction

`VideoThumbnailExtraction` is KiriView's repository-internal asynchronous capability for deriving one bounded representative thumbnail image from one caller-supplied video source. It returns an image or a typed failure and does not own source discovery, source authorization, navigation identity, cache identity, persistent cache access, thumbnail scheduling, publication, playback state, or user-interface policy.

## Request

`VideoThumbnailExtractionRequest` contains `sourceUrl` and `maximumLongEdge`. `sourceUrl` must be non-empty and valid. `maximumLongEdge` must be positive and no greater than `VideoThumbnailExtractionLimits::maximumOutputLongEdge`.

`VideoThumbnailExtractionLimits` exposes these immutable admission limits:

- `maximumOutputLongEdge` is `4096`.
- `maximumOutputBytes` is `67108864`.
- `maximumWorkingBytes` is `536870912` and bounds the caller-accounted peak of component-controlled materialized source, transformation, and result pixel storage for one operation.
- `maximumDiagnosticCharacters` is `1024`, counted as Unicode scalar values.

These limits apply to one extraction operation and its result. Before starting an operation, the caller must admit `maximumWorkingBytes` against its aggregate decoded-memory policy and keep that admission until completion or cancellation; after a ready completion it may replace the working admission with a charge for the returned image that lasts through the final pixel alias. Multimedia-backend-private buffers remain outside this host-accounted bound and under the backend's own policy. These limits are not an aggregate concurrency or memory budget; the caller bounds the number and priority of operations it admits.

The caller authorizes the source, keeps any caller-owned access grant, mount, lease, or equivalent prerequisite valid while the job is active, and decides whether the source class is eligible for thumbnail work. Accepting a `QUrl` does not promise support for any URL scheme, container, codec, network location, credential form, or protected media. The component must not expand caller eligibility or use the source URL as navigation, cache, session, or freshness identity.

The component requests only read access to the source. It must not modify source content or caller-managed metadata, replace or delete the source, or create source-adjacent artifacts.

An invalid request completes with `InvalidRequest` without touching the source or starting multimedia or deadline work.

## Result

`VideoThumbnailExtractionResult` has status `Ready` or `Failed`.

A `Ready` result contains one non-null `QImage`, contains no failure, has positive dimensions, preserves the selected source image's aspect ratio subject to integer raster dimensions, and has a long edge no greater than the request. Extraction does not upscale a selected image whose long edge is already within the request. The returned image has `QImage::sizeInBytes()` no greater than `VideoThumbnailExtractionLimits::maximumOutputBytes` and owns or shares self-contained pixel storage whose lifetime does not depend on a multimedia backend, decoded frame, or other source object.

A `Failed` result contains no image and contains one `VideoThumbnailExtractionFailure`. The failure has a typed `cause` and an optional bounded `diagnostic`. `VideoThumbnailExtractionFailureCause` contains:

- `InvalidRequest` for malformed or out-of-range request values.
- `SourceUnavailable` when the component identifies that the source cannot be opened or accessed.
- `UnsupportedMedia` when the component identifies that the multimedia backend cannot handle the source format or required extraction operations.
- `BackendFailure` for a multimedia or conversion failure that the component cannot classify more specifically.
- `TimedOut` when the extraction deadline expires without an admissible image.
- `NoRepresentativeImage` when, without a usable embedded image or decoded frame and without another terminal failure, the source reaches a terminal state or the bounded candidate search is exhausted.
- `ResourceLimit` when an input-derived or output resource for a present candidate would exceed a component limit. Resource rejection follows the embedded-candidate fallback rule below and is otherwise terminal.

Failure cause is the stable branching contract. Diagnostic text is normalized plain text for development diagnostics, may be empty, is not stable wording, is not localization-ready user-facing text, and must not be used for branching. It must not disclose credentials, URL user information, raw backend object identity, or unbounded backend-authored text. KiriView maps a typed failure to its own user-facing error projection.

Cancellation is not a result status and does not invoke the completion callback.

## Representative Image

The component may use an embedded cover image, an embedded thumbnail, or a decoded video frame. Within one metadata observation, a usable cover image takes precedence over an embedded thumbnail. A usable embedded thumbnail is selected when the cover is absent or unusable, including because the cover violates resource admission. If no usable embedded image is available from that observation and any present embedded image violates resource admission, extraction fails with `ResourceLimit`. Outside that same-observation fallback, a candidate that violates resource admission terminates extraction with `ResourceLimit`; the component does not skip it in favor of a candidate that might become available later. The component may sample a bounded set of positions for seekable media and may use the first usable frame for non-seekable media.

The result makes no promise about an exact frame, timestamp, seek sequence, frame-interest heuristic, or repeatability across multimedia backends and platform codec versions. Those choices may change without changing the public contract. A source image is not usable when it is null, cannot be converted to a bounded output, or violates component resource admission.

## Asynchronous Operation

`startVideoThumbnailExtraction` accepts a live non-null `QObject` receiver, one request, and one non-empty completion callback and returns a move-only `VideoThumbnailExtractionJob`. The receiver's affinity thread must have an event dispatcher capable of queued delivery for the operation lifetime. Start, moving or replacing an active job, cancellation, and destruction of an active job occur on the receiver's affinity thread; completion is delivered on that thread while the receiver is alive. These receiver, callback, dispatcher, and thread requirements are caller preconditions rather than request values, and violating them has no supported result or callback behavior.

Completion is never invoked before `startVideoThumbnailExtraction` returns. Every admitted operation that is neither canceled nor suppressed by receiver destruction delivers exactly one terminal `Ready` or `Failed` result. The job remains active while terminal delivery is pending and becomes inactive immediately before invoking completion, so completion may safely release the job, destroy the receiver, or start another extraction.

`VideoThumbnailExtractionJob::cancel()` is idempotent. Destroying or replacing an active job cancels it. Cancellation while terminal delivery is pending suppresses that delivery. Once cancellation returns, completion is permanently suppressed even if backend, timer, queued, or reentrant events arrive later. Destroying the receiver has the same suppression guarantee.

The extraction deadline is bounded and measured with monotonic time. Its exact duration is component resource policy rather than a caller scheduling guarantee; callers must not infer progress or failure timing from wall-clock delay.

## Repository-Internal Interface

The component exposes one intentional C++23 include surface to KiriView:

- `<VideoThumbnailExtraction/VideoThumbnailExtraction>` is a declaration-free umbrella header.
- `<VideoThumbnailExtraction/videothumbnailextraction.h>` declares `VideoThumbnailExtractionLimits`, request, result, failure, job, callback, and start operation types in the `kiriview` namespace.

Only the declarations listed above are part of the supported interface. No other declarations are supported.

`VideoThumbnailExtraction` is not an independently consumable SDK and makes no installed-header, exported-package, stable ABI, semantic-versioning, QML URI, plugin, or source-compatibility promise. KiriView and the component evolve atomically in this repository without compatibility shims.
