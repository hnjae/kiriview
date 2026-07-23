# Video Thumbnail Extraction

`VideoThumbnailExtraction` is KiriView's repository-internal asynchronous capability for deriving one bounded representative thumbnail image from one caller-supplied video source. It returns an image or a typed failure and does not own source discovery, source authorization, navigation identity, cache identity, persistent cache access, thumbnail scheduling, publication, playback state, or user-interface policy.

## Request

`VideoThumbnailExtractionRequest` contains `sourceUrl` and `maximumLongEdge`. `sourceUrl` must be non-empty and valid. `maximumLongEdge` must be positive and no greater than `VideoThumbnailExtractionLimits::maximumOutputLongEdge`.

`VideoThumbnailExtractionLimits` exposes these immutable admission limits:

- `maximumOutputLongEdge` is `4096`.
- `maximumOutputBytes` is `67108864`.
- `maximumDiagnosticCharacters` is `1024`, counted as Unicode scalar values.

The caller authorizes the source and decides whether its source class is eligible for thumbnail work before making a request. Accepting a `QUrl` does not promise support for any URL scheme, container, codec, network location, credential form, or protected media. The component must not expand caller eligibility or use the source URL as navigation, cache, session, or freshness identity.

An invalid request completes with `InvalidRequest` without constructing multimedia or timer resources.

## Result

`VideoThumbnailExtractionResult` has status `Ready` or `Failed`.

A `Ready` result contains one non-null `QImage`, contains no failure, has positive dimensions, preserves the selected source image's aspect ratio subject to integer raster dimensions, and has a long edge no greater than the request. Extraction does not upscale a selected image whose long edge is already within the request. The returned image satisfies both public output limits, including its actual retained byte size.

A `Failed` result contains no image and contains one `VideoThumbnailExtractionFailure`. The failure has a typed `cause` and an optional bounded `diagnostic`. `VideoThumbnailExtractionFailureCause` contains:

- `InvalidRequest` for malformed or out-of-range request values.
- `SourceUnavailable` when the source cannot be opened or accessed.
- `UnsupportedMedia` when the multimedia backend cannot handle the source format or required extraction operations.
- `BackendFailure` for another multimedia or conversion failure.
- `TimedOut` when the extraction deadline expires without an admissible image.
- `NoRepresentativeImage` when the source reaches a terminal state without a usable embedded image or decoded frame.
- `ResourceLimit` when an input-derived or output resource would exceed a component limit.

Failure cause is the stable branching contract. Diagnostic text is normalized plain text for development diagnostics, may be empty, is not stable wording, is not localization-ready user-facing text, and must not be used for branching. It must not disclose credentials, URL user information, raw backend object identity, or unbounded backend-authored text. KiriView maps a typed failure to its own user-facing error projection.

Cancellation is not a result status and does not invoke the completion callback.

## Representative Image

The component may use an embedded cover image, an embedded thumbnail, or a decoded video frame. Within one metadata observation, a usable cover image takes precedence over an embedded thumbnail. The component may sample a bounded set of positions for seekable media and may use the first usable frame for non-seekable media.

The result makes no promise about an exact frame, timestamp, seek sequence, frame-interest heuristic, or repeatability across multimedia backends and platform codec versions. Those choices may change without changing the public contract. A source image is not usable when it is null, cannot be converted to a bounded output, or violates component resource admission.

## Asynchronous Operation

`startVideoThumbnailExtraction` accepts a live `QObject` receiver, one request, and one completion callback and returns a move-only `VideoThumbnailExtractionJob`. Start and job control occur on the receiver's affinity thread, and completion is delivered on that thread while the receiver is alive.

Completion is never invoked before `startVideoThumbnailExtraction` returns. Every admitted operation that is not canceled delivers exactly one terminal `Ready` or `Failed` result. The job becomes inactive before invoking completion, so completion may safely release the job or receiver.

`VideoThumbnailExtractionJob::cancel()` is idempotent. Destroying or replacing an active job cancels it. Once cancellation returns, completion is permanently suppressed even if backend, timer, queued, or reentrant events arrive later. Destroying the receiver has the same suppression guarantee.

The extraction deadline is bounded and measured with monotonic time. Its exact duration is component resource policy rather than a caller scheduling guarantee; callers must not infer progress or failure timing from wall-clock delay.

## Repository-Internal Interface

The component exposes one intentional C++23 include surface to KiriView:

- `<VideoThumbnailExtraction/VideoThumbnailExtraction>` is a declaration-free umbrella header.
- `<VideoThumbnailExtraction/videothumbnailextraction.h>` declares `VideoThumbnailExtractionLimits`, request, result, failure, job, callback, and start operation types in the `kiriview` namespace.

No workflow state, candidate-position helper, frame-interest helper, operation plan, backend factory, multimedia fact, timer port, player, video sink, or test instrumentation is part of the supported interface.

`VideoThumbnailExtraction` is not an independently consumable SDK and makes no installed-header, exported-package, stable ABI, semantic-versioning, QML URI, plugin, or source-compatibility promise. KiriView and the component evolve atomically in this repository without compatibility shims.
