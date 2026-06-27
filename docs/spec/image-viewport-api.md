# ImageViewport API

This document records the public API shape for QML and C++ callers. The concepts below are the public surface the implementation preserves.

## QML Item

`ImageViewport` should be a `QQuickItem` with a `sequence` property accepting only an `ImageSequence` object or `null`. Values that are not `ImageSequence` or `null` are not viewport requests: if the binding or C++ call attempts to assign a string, URL, archive, byte buffer, JavaScript object, raw provider object, or other unsupported value, the assignment is rejected before changing the property or ignored by the typed setter, existing request and display state are preserved, and request and display revisions do not change. Diagnostics for such type-conversion failures belong to the caller language or factory that rejected the input, not to viewport request status.

The item exposes request status, request status reason, command diagnostic reason, display status, playback phase, displayed frame, requested frame, displayed position, requested position, frame count, total duration, frame seek bounds, position seek bounds, timed playback support, frame seek support, position seek support, displayed image size, content rectangle, visible image rectangle, display revision, request revision, command revision, error string, and warning string observations.

The item exposes presentation properties for fill mode, horizontal and vertical alignment, smoothing, mipmap request, mirroring, background mode, background color, zoom, pan, and playback looping.

The item exposes imperative commands for `clear()`, `play()`, `pause()`, `stop()`, `seek(frame)`, `seekToPosition(milliseconds)`, and `resetView()`. Commands that can fail return an immediate outcome rather than requiring callers to infer command acceptance from retained display state.

Coordinate helpers map item points to image points, image points to item points, and test whether an image point is inside the currently visible image area. The stable QML signatures are `itemToImage(x: real, y: real): CoordinateResult`, `imageToItem(x: real, y: real): CoordinateResult`, and `containsVisibleImagePoint(x: real, y: real): bool`; C++ exposes equivalent methods with real-valued point inputs and the same result fields. Point conversions return a public coordinate-result value with `valid`, `x`, and `y` fields; failed conversions return `valid: false`, `x: 0`, and `y: 0` so QML and C++ callers can test validity without relying on clamping, NaN handling, or exception behavior.

The QML item surface uses the following stable property names and default or unavailable values.

| Property | Type | Default or unavailable value | Notes |
| --- | --- | --- | --- |
| `sequence` | `ImageSequence` or `null` | `null` | Assigning any other value is not a viewport request and preserves existing viewport state. |
| `requestStatus` | `RequestStatus` | `NoRequest` | Describes the current accepted display request within the accepted sequence generation. |
| `requestReason` | `RequestReason` | `NoRequest` | Refines `requestStatus` for the accepted request; callers branch first on `requestStatus` and command outcome. |
| `commandReason` | `CommandReason` | `NoCommand` | Describes the latest command that was invalid, unsupported, or ignored without changing the accepted request. |
| `displayStatus` | `DisplayStatus` | `Empty` | Describes whether visible display content is empty, accepted-request content, or retained earlier-request content. |
| `playbackPhase` | `PlaybackPhase` | `Stopped` | Current observable playback phase for the accepted request. |
| `displayedFrame` / `requestedFrame` | `int` | `-1` | Zero-based when known. |
| `displayedPosition` / `requestedPosition` | `int` milliseconds | `-1` | Milliseconds at the QML boundary. |
| `frameCount` | `int` | `-1` | Unknown until construction facts or validated metadata provide it. |
| `totalDuration` | `int` milliseconds | `-1` | Unknown until construction facts or validated metadata provide it. |
| `frameSeekBounds` | integer range value | minimum `-1`, maximum `-1` | Invalid range means unknown or unavailable. |
| `positionSeekBounds` | millisecond range value | minimum `-1`, maximum `-1` | Invalid range means unknown or unavailable. |
| `timedPlaybackSupport` / `frameSeekSupport` / `positionSeekSupport` | `TriState` | `Unavailable` | `Unavailable` means metadata or declaration has not resolved the capability; it is not equivalent to `False`. |
| `displayedImageSize` | size value | width `0`, height `0` | Public logical image size of committed display content; unavailable only when no display content is committed. |
| `contentRect` / `visibleImageRect` | rectangle value | width `0`, height `0` | Non-positive dimensions mean unavailable or no presentable image area. |
| `displayRevision` / `requestRevision` / `commandRevision` | unsigned integer | `0` | Monotonically increase within an item lifetime and never reset except by item destruction. |
| `errorString` / `warningString` | string | empty string | Redacted, bounded, plain text. |
| `fillMode` | `FillMode` | `Contain` | Other values are `Cover`, `Stretch`, and `Center`. |
| `horizontalAlignment` / `verticalAlignment` | `HorizontalAlignment` / `VerticalAlignment` | `AlignHCenter` / `AlignVCenter` | `HorizontalAlignment` values are `AlignLeft`, `AlignHCenter`, and `AlignRight`; `VerticalAlignment` values are `AlignTop`, `AlignVCenter`, and `AlignBottom`. Alignment positions the placed image or cover crop focus. |
| `smoothing` / `mipmap` / `mirrorHorizontally` / `mirrorVertically` / `looping` | bool | `false` except `smoothing` defaults to `true` | Quality requests are preferences; looping changes playback end behavior only for timed sequences. |
| `backgroundMode` | `BackgroundMode` | `Transparent` | Other values are `SolidColor` and `Checkerboard`. |
| `backgroundColor` | color | transparent black | Used only when background mode needs a color. |
| `zoom` | real | `1.0` | Values must be positive and finite. |
| `pan` | point value | `0, 0` | Item logical pixels applied after zoom. |

The stable command signatures are `clear(): CommandOutcome`, `play(): CommandOutcome`, `pause(): CommandOutcome`, `stop(): CommandOutcome`, `seek(frame: int): CommandOutcome`, `seekToPosition(milliseconds: int): CommandOutcome`, and `resetView(): CommandOutcome`. `CommandOutcome` values are `Accepted`, `Invalid`, `Unsupported`, and `IgnoredNoRequest`.

## Public Status

`RequestStatus` has the values `NoRequest`, `Loading`, `Ready`, `Unsupported`, and `Error`. `NoRequest` means no accepted sequence is active. `Loading` means an accepted request is waiting on metadata, frame payload, preparation, or render commit. `Ready` means the accepted request has committed display content. `Unsupported` means the accepted request cannot be satisfied because the requested operation or content is unsupported. `Error` means the accepted request failed despite being supported in principle.

`DisplayStatus` has the values `Empty`, `Ready`, and `Retained`. `Empty` means no display content is committed. `Ready` means the committed display identity belongs to the accepted request; a non-positive item size may still expose no presentable image area. `Retained` means committed display content belongs to an earlier request while the accepted request is still loading or has failed.

`PlaybackPhase` has the values `Stopped`, `Playing`, `Waiting`, and `Paused`. `Stopped` means playback is not consuming sequence time. `Playing` means playback may advance when the current frame and metadata allow it. `Waiting` means playback has been accepted and is waiting for metadata to select a target, or has selected a target frame or position and is waiting for provider, preparation, or render completion before consuming additional sequence time. `Paused` means playback preserves the current target without consuming sequence time.

`RequestReason` has the values `NoRequest`, `ProviderWaiting`, `RequestQueued`, `UploadPending`, `RenderWaiting`, `Ready`, `UnsupportedRequest`, `InvalidRequest`, `ProviderFailure`, `PayloadRejection`, and `RenderFailure`. `ProviderWaiting` means the accepted request is waiting for provider metadata or frame production, `RequestQueued` means the accepted display request has not entered its next provider, preparation, or render stage because a previously accepted operation in the same serialized pipeline must finish or release ownership first, `UploadPending` means a validated payload is waiting for preparation or upload ownership, and `RenderWaiting` means a prepared payload is waiting for positive item geometry or render commit acknowledgement. Once an older operation cannot affect public state, it is stale and must not keep the accepted request in `RequestQueued`; retained visible pixels alone do not imply `RequestQueued`. Request reasons describe the current accepted request only; command failures that do not replace or change the accepted request are reported through `commandReason`. Supersession is an ordering event, not the current reason of the newly accepted request.

The valid status and reason pairs are: `NoRequest` with `NoRequest`; `Loading` with `ProviderWaiting`, `RequestQueued`, `UploadPending`, or `RenderWaiting`; `Ready` with `Ready`; `Unsupported` with `UnsupportedRequest`, `InvalidRequest`, or `PayloadRejection`; and `Error` with `ProviderFailure`, `PayloadRejection`, or `RenderFailure`.

`CommandReason` has the values `NoCommand`, `IgnoredNoRequest`, `InvalidRequest`, and `UnsupportedRequest`. Accepted commands clear `commandReason` to `NoCommand` unless they synchronously return a more specific non-accepted outcome. Invalid, unsupported, and ignored commands update `commandReason` and `commandRevision` without changing `requestStatus`, `requestReason`, accepted target, display status, retained display content, playback phase, or request revision.

Request, display, and command revisions are monotonically increasing observations. The request revision changes when the accepted request identity, request status, request reason, requested target, metadata-derived public observations for the accepted generation, or public diagnostic for that request changes. The display revision changes when visible display content, display ownership, display status, presentation-affecting geometry, background presentation, mirroring, zoom, pan, fill mode, alignment, or an observable quality fallback changes, including presentation-only changes while `displayStatus` is `Empty`. The command revision changes when the command diagnostic generation changes: every invalid, unsupported, or ignored command increments it even when the resulting `commandReason` value repeats, and an accepted command increments it only when it clears a previous non-`NoCommand` diagnostic to `NoCommand`. Property-specific notifications remain the stable observation path for callers that only need one field.

## Public Value Types

Public range values expose `minimum` and `maximum` integer fields in the units of the property that owns the range. An invalid range has `minimum: -1` and `maximum: -1`; otherwise both fields are inclusive bounds.

Public rectangle values expose real-valued `x`, `y`, `width`, and `height` fields in the coordinate space named by the property. Empty or unavailable rectangles use `width <= 0` or `height <= 0`; callers must not infer an image-space origin from an unavailable rectangle.

Public size values expose real-valued `width` and `height` fields. Public point values expose real-valued `x` and `y` fields. Public coordinate-result values expose `valid`, `x`, and `y` fields; invalid coordinate results use `valid: false`, `x: 0`, and `y: 0`.

QML notifications are emitted when any field of a public value changes according to exact numeric equality for integer fields and exact equality of the normalized public value for real fields. C++ callers observe the same field names and units through the equivalent value types.

## Position Observations

The viewport's public timing model is integer milliseconds. Sequence factories and provider adapters supply or normalize timing metadata to integer millisecond frame boundaries before the viewport validates seek bounds, total duration, requested position, or displayed position. Providers may preserve higher-precision timing internally, but public viewport seeking and observation use the normalized millisecond intervals only.

`requestedPosition` is the accepted public target position in milliseconds when the accepted request was created from a position seek or playback advancement. For a timed frame-identity request, including frame seek, initial display, and metadata-bound selection, it is the start position of the selected frame when timing metadata is known and `-1` while no position identity is available. Still-image requests always expose `requestedPosition: -1`.

`requestedFrame` is the accepted public target frame index when a frame identity is known. A syntactically valid frame seek publishes the requested zero-based frame index immediately, even while metadata bounds are unknown and the target still requires later revalidation. Initial display, metadata-bound selection, position seek, and playback advancement expose the selected or resolved zero-based frame index once metadata resolves the target. It remains `-1` while metadata has not resolved a frame identity, including playback accepted before metadata selects a target and unknown-target initial display before metadata-bound selection.

The public requested target is distinct from the resolved frame identity used for provider frame validation. A position seek preserves the caller's accepted public position, including positions inside a frame interval and `totalDuration`; the resolved frame identity is the selected frame index and that frame's start position after public timing normalization. For `seekToPosition(totalDuration)`, the valid resolved frame identity is the final frame index and final frame start position even though public `requestedPosition` remains `totalDuration`.

`displayedPosition` is the start position in milliseconds of the displayed frame's half-open interval. If a caller seeks exactly to `totalDuration`, `requestedPosition` remains `totalDuration`, the selected displayed frame is the final frame, and `displayedPosition` is the final frame's start position after commit. In play-once mode, the terminal displayed frame keeps its frame-start `displayedPosition`; playback stopping at the end does not expose an out-of-range displayed position. In looping mode, the first accepted request after wrap has `requestedPosition: 0` and later commits with `displayedPosition: 0`; no transient position greater than `totalDuration` is public.

Still-image sequences have one public frame and no public timing identity. They expose `frameCount: 1`, `frameSeekSupport: True`, `frameSeekBounds: 0..0`, `timedPlaybackSupport: False`, `positionSeekSupport: False`, `totalDuration: -1`, invalid position seek bounds, `requestedPosition: -1`, and `displayedPosition: -1`. `seek(0)` is accepted as an explicit display request for the still frame; other frame indices are invalid, and `seekToPosition(...)` and `play()` are unsupported.

## Unavailable Values

Unknown integer counts and frame indices are exposed as `-1`. Unknown durations and positions are exposed as `-1` milliseconds. Unknown seek bounds are exposed as an invalid range whose minimum and maximum are both `-1` milliseconds or frame indices, matching the unit of the seek mode.

Unknown booleans, including timed playback support, frame seek support, and position seek support before provider metadata is validated, are exposed through a tri-state value with `Unavailable`, `False`, and `True` states rather than by guessing.

Unavailable image sizes and rectangles are exposed as empty values with non-positive width or height. Non-positive item geometry makes presentable rectangles and coordinate conversions unavailable, but does not clear `displayedImageSize` while display content remains committed. Invalid coordinate conversions return `valid: false` coordinate-result values and must not be represented by clamping to an edge pixel.

## Command Outcomes

Command outcomes distinguish accepted, invalid, unsupported, and ignored because there is no active request. A command outcome reports immediate command acceptance; asynchronous provider or render failure is reported through request status.

`clear()` always succeeds, stops playback, removes retained content, and moves request and display status to their empty states. `stop()` exits playback without seeking and supersedes only pending requests created by playback advancement; it does not select a target other than the latest non-playback target for the generation and does not change display content by itself. If a non-playback request is still the active accepted request, `stop()` leaves it eligible to commit. If playback had superseded the latest non-playback target, `stop()` creates a fresh accepted non-playback display request identity for that same target, or promotes already committed same-generation content for that target to the accepted display identity; old provider, preparation, and render results for the previously superseded request identity remain stale. If playback was waiting on metadata after superseding an unknown-target initial request, `stop()` creates a fresh unknown-target non-playback initial request identity with unknown requested frame and position, and later validated metadata may create the metadata-bound initial frame `0` request from that fresh identity. If the generation has no latest non-playback target and no initial target pending metadata, requested frame and position remain unknown until metadata or an explicit command selects one. `pause()` preserves the current requested target; it changes playback phase from `Playing` or `Waiting` to `Paused`, and otherwise succeeds for an active request without changing playback phase. `resetView()` restores only zoom and pan to their neutral values without changing the active sequence, fill mode, alignment, mirroring, background, quality preferences, or looping.

Command behavior after an unsupported or error request depends on the failure scope. Generation-terminal unsupported or error requests cannot accept new display or playback requests until `clear()` or sequence replacement. Display-request-terminal unsupported or error requests keep the accepted generation usable; later valid explicit seeks, and playback-selected targets when playback is supported, are evaluated against the accepted generation's metadata and capabilities.

| Command | No active request | Loading metadata or frame | Ready display | Unsupported or error request by failure scope | Retained display |
| --- | --- | --- | --- | --- | --- |
| `clear()` | Accepted; stays empty | Accepted; cancels request and clears display | Accepted; clears request and display | Accepted; clears request and display | Accepted; clears request and retained display |
| `play()` | Ignored because there is no active request | Accepted when `timedPlaybackSupport` is `True`; accepted when `timedPlaybackSupport` is `Unavailable` and the sequence has not declared timed playback impossible; unsupported when `timedPlaybackSupport` is `False` or the sequence declares no timed playback | Accepted for playable timed sequences; unsupported for still or unplayable sequences | Unsupported for generation-terminal failures; otherwise accepted or unsupported according to accepted generation capability | Uses the accepted request's capability, not the retained display's capability |
| `pause()` | Ignored because there is no active request | Accepted and preserves the requested target | Accepted and preserves the requested target | Accepted but does not change terminal request status | Accepted for the accepted request only |
| `stop()` | Ignored because there is no active request | Accepted and stops playback without seeking | Accepted and stops playback without seeking | Accepted but does not change terminal request status | Accepted for the accepted request only |
| `seek(frame)` | Ignored because there is no active request | Accepted when the target is syntactically valid and bounds are unknown; invalid when known bounds reject it; unsupported when frame seeking is unavailable | Accepted for in-range frames; invalid out of range; unsupported when frame seeking is unavailable | Invalid for syntactically invalid input; unsupported for generation-terminal failures; otherwise accepted, invalid, or unsupported according to accepted generation metadata and capabilities | Uses the accepted request's metadata and capabilities, not retained display metadata |
| `seekToPosition(milliseconds)` | Ignored because there is no active request | Accepted when the target is syntactically valid and bounds are unknown; invalid when known bounds reject it; unsupported when position seeking is unavailable | Accepted for valid positions; invalid out of range; unsupported when position seeking is unavailable | Invalid for syntactically invalid input; unsupported for generation-terminal failures; otherwise accepted, invalid, or unsupported according to accepted generation metadata and capabilities | Uses the accepted request's metadata and capabilities, not retained display metadata |
| `resetView()` | Accepted; restores neutral transform state | Accepted; restores neutral transform state | Accepted; restores neutral transform state | Accepted; restores neutral transform state | Accepted; restores neutral transform state |

Invalid commands set `commandReason` to `InvalidRequest` without replacing the accepted request, changing request status, changing request reason, changing the accepted target, changing playback phase, or clearing retained display content. Unsupported commands set `commandReason` to `UnsupportedRequest` under the same preservation rules. Commands ignored because no request is active set `commandReason` to `IgnoredNoRequest`. Command diagnostic reasons remain until the next accepted command, newer command diagnostic, or `clear()`.

A current active target accepted while metadata bounds were unknown is revalidated when authoritative metadata arrives. If the target is then out of range or its seek mode proves unavailable, the accepted display request reports `Unsupported` with `InvalidRequest` or `UnsupportedRequest` according to the resolved failure, playback stops if the target was playback-generated, and display content remains unchanged. The accepted generation remains usable when generation metadata is valid, so later valid requests may supersede the failed display request. If a newer target superseded it before metadata arrived, the stale target rejection is ignored and cannot change request status, diagnostics, playback phase, or display content.

## Public Limits

The installed API exposes one limits object named `ImageSequenceLimits` as both a QML singleton and a C++ type with static read-only accessors using the same field names. It reports `maximumLogicalWidth`, `maximumLogicalHeight`, `maximumPixelsPerFrame`, `maximumPayloadBytesPerFrame`, `maximumTimedListFrameCount`, `maximumFrameDuration`, `maximumTotalSequenceDuration`, and `maximumDiagnosticStringLength`; duration fields use integer milliseconds, byte fields use bytes, pixel fields use logical pixels or pixel counts, and diagnostic length uses Unicode scalar values.

Concrete builds may publish higher values, but they must support at least 8192 logical pixels per frame side, 67108864 pixels per frame, 268435456 payload bytes per frame, 10000 timed-list frames, 86400000 milliseconds per frame, 86400000 milliseconds total duration, and 4096 Unicode scalar values per public diagnostic string. Published limits are end-to-end admission limits for the installed build; a backend-specific failure below the published limit is reported as render failure, not silently treated as unsupported input.

Factory helpers reject construction input outside the published limits with synchronous diagnostics. Provider-backed content outside the published limits reports a request status and reason for the affected request according to the failure source. Public admission failures are distinct from backend capacity or backend-specific failures: malformed metadata or payload envelopes, missing payload byte-size metadata when the viewport cannot derive a conservative byte size before retention or upload, public-limit violations, and publicly unsupported payload shapes report payload rejection, while otherwise supported payloads at or below the published limits that fail because the installed rendering backend cannot prepare or commit them report `Error` with `RenderFailure`. Payload-rejection diagnostics are redacted and bounded, but may identify the broad admission category that failed.

| Condition | Request status | Request reason |
| --- | --- | --- |
| Metadata accurately describes unsupported sequence content | `Unsupported` | `UnsupportedRequest` |
| Metadata accurately describes an unsupported operation or capability for otherwise valid content | `Unsupported` | `UnsupportedRequest` for the active operation or display request only |
| Metadata is malformed, internally inconsistent, contradicts construction-time facts, violates timing or dimension rules, or exceeds published metadata limits | `Error` | `PayloadRejection` |
| Provider fails while producing metadata for otherwise supported content | `Error` | `ProviderFailure` |
| Provider reports unsupported operation for an active display request or playback advancement | `Unsupported` | `UnsupportedRequest` |
| Provider reports unsupported payload for an active display request or playback advancement | `Unsupported` | `PayloadRejection` |
| Frame payload exceeds published size or byte limits | `Unsupported` | `PayloadRejection` |
| Frame payload metadata does not match validated sequence metadata, including sequence-wide public logical image size | `Error` | `PayloadRejection` |
| Frame payload omits required byte-size metadata when the viewport cannot derive a conservative byte size before retention or upload | `Error` | `PayloadRejection` |
| Provider fails while producing otherwise supported frame content | `Error` | `ProviderFailure` |
| Public admission validation rejects a display-critical payload before render ownership begins | `Unsupported` or `Error` according to whether the rejected shape is unsupported or malformed | `PayloadRejection` |
| Backend preparation or capacity fails for an otherwise admitted display-critical payload before render ownership begins | `Error` | `RenderFailure` |
| Render commit fails for the active accepted display request and prepared-payload identity | `Error` | `RenderFailure` |

## Sequence Construction

`ImageSequence` is an opaque caller-facing handle for immutable or provider-backed image content. Callers create sequences through explicit helpers instead of assigning raw inputs to the viewport.

The construction concepts are a still image sequence, a timed frame list sequence, and a provider-backed sequence. QML-callable helpers accept only QML-representable helper objects; C++ helpers may accept native image values when appropriate. The stable QML construction helper signatures are `ImageSequenceFactory.fromFrame(frame: ImageFrame): ImageSequenceFactoryResult`, `ImageSequenceFactory.fromTimedFrameList(list: TimedImageFrameList): ImageSequenceFactoryResult`, and `ImageSequenceFactory.fromProvider(adapter: ImageSequenceProviderAdapter): ImageSequenceFactoryResult`; C++ exposes equivalent helpers with native overloads where appropriate. `ImageSequenceFactoryResult` exposes `sequence`, `outcome`, `errorString`, and `warningString`; failed construction returns `sequence: null` with a bounded diagnostic. `FactoryOutcome` values are `Created`, `Invalid`, `Unsupported`, and `Error`.

Invalid construction input fails at construction time with synchronous factory diagnostics. A valid provider-backed sequence may subsequently report unsupported or error status after assignment if provider startup or content requests fail.

Still-image and timed-list factories derive construction-time capability facts from their validated construction input. Provider-backed sequences derive construction-time capability facts from adapter declarations and cheap known metadata.

Provider-backed sequence construction establishes sequence identity, provider ownership, declared provider capabilities, construction-time facts, and a provider threading contract. Destroying the original adapter QObject after successful sequence construction does not invalidate already-created sequences or active sessions. If provider-owned external resources become unavailable after assignment, the active generation reports provider failure through the session; if the adapter cannot create a sequence that survives adapter QObject destruction under that contract, construction fails with factory diagnostics.

Provider-backed sequence construction starts each capability as one of `Unavailable`, `DeclaredFalse`, `DeclaredTrue`, or `KnownTrue`/`KnownFalse` from construction-time facts. These construction states are capability facts, not the public QML observation enum: `Unavailable` projects to public `Unavailable`, `DeclaredFalse` and `KnownFalse` project to public `False`, and `DeclaredTrue` and `KnownTrue` project to public `True`. Runtime metadata may resolve `Unavailable` to true or false. Runtime metadata that contradicts `DeclaredFalse`, `DeclaredTrue`, or construction-time facts is malformed metadata and maps to `Error` with `PayloadRejection`; it is not treated as a normal capability resolution.

Provider construction facts are represented by `ImageSequenceProviderKnownFacts`. The value can be unknown, logical-size-only, still, timed-frame-count, fixed-duration, or timed-frame-list. Unknown facts provide no metadata observations before runtime metadata. Logical-size-only facts constrain runtime metadata logical size but do not provide frame count, duration, seek bounds, or an initial display target. Timed-frame-count facts constrain runtime metadata frame count and may expose frame count and frame seek bounds when frame seeking is declared or known true, but they do not provide total duration or position bounds. Fixed-duration and timed-frame-list facts are complete timed facts. Still facts and complete timed facts are complete construction metadata and may select initial frame `0` at assignment time. The legacy `knownMetadata()` adapter method remains a compatibility convenience; the base `knownFacts()` implementation maps valid complete `knownMetadata()` to equivalent known facts.

Frame count, total duration, frame durations, public logical image size, and seek bounds are unknown until provider metadata is validated unless the provider declares them as construction-time facts through the public adapter API. Construction-time facts are authoritative constraints for the accepted generation; runtime metadata that contradicts those facts is malformed metadata and maps to `Error` with `PayloadRejection`. Partial facts only project observations that are safe without complete runtime metadata: logical size constrains validation but does not select a target, frame count can project frame bounds only when frame seek support is known true, and total duration or position bounds project only when complete durations are known.

`ImageSequenceProviderThreadingContract` has the values `AffinityBound` and `ThreadSafe`. `ImageSequenceProviderAdapter::threadingContract()` defaults to `AffinityBound`. Affinity-bound sessions receive entry-point calls on the session QObject affinity. Thread-safe sessions may receive session entry-point calls directly from the viewport controller affinity, while session QObject destruction remains on the session affinity.

## Frames And Timed Lists

`ImageFrame` represents an immutable display-ready frame payload and its metadata. The public API does not expose mutable raw pixel storage as viewport state.

Frame logical width and height must be positive finite integers. Public logical image size is the orientation-normalized image size that callers observe through geometry, coordinate conversion, and displayed image size; physical payload dimensions may differ when the declared orientation rotates or otherwise transforms the payload into that public logical image space. A sequence has one public logical image size for every frame in that sequence; timed-list construction rejects entries whose public logical image size differs from the first accepted frame, and provider-backed frame payloads whose envelope public logical image size differs from validated sequence metadata report `Error` with `PayloadRejection`. Frame payload metadata must describe the public logical image size, transparency, and orientation that callers can observe through viewport geometry and presentation. Invalid frame metadata is rejected during factory construction or reported as payload rejection for provider-backed content.

`ImageFrame::OrientationPolicy` follows the eight Qt image I/O transformation states: `Identity`, `MirrorHorizontally`, `MirrorVertically`, `Rotate180`, `Rotate90`, `MirrorHorizontallyAndRotate90`, `MirrorVerticallyAndRotate90`, and `Rotate270`. `Rotate90`, `MirrorHorizontallyAndRotate90`, `MirrorVerticallyAndRotate90`, and `Rotate270` swap public logical width and height relative to the physical payload. A C++ `ImageFrame(QImage, OrientationPolicy, QObject*)` constructor creates a frame whose stored payload is orientation-normalized before factory construction, provider validation, render upload, and test pixel inspection.

`TimedImageFrameList` is a builder for explicit animated sequences. It owns typed frame-duration entries, exposes a count, supports append and clear operations, and reports construction diagnostics when invalid entries are rejected.

Timed lists use zero-based frame indices. Durations projected to QML use milliseconds and must be positive. Empty lists, zero-duration frames, negative durations, non-finite durations, and total durations that exceed published range limits are invalid construction input. Provider-origin timing may preserve fractional precision internally, but adapter metadata accepted by the viewport provides integer millisecond frame boundaries or durations; conversion to that public timing model is the provider adapter's responsibility.

## Provider Adapter

`ImageSequenceFactory.fromProvider(...)` or the equivalent C++ helper should adapt an application-owned provider adapter into an `ImageSequence`. The provider object itself is not assigned directly to the viewport.

The public provider adapter surface is sufficient for downstream applications to implement custom sources without including private headers or touching scene graph objects.

Provider integration is a trusted in-process extension point. The API documents bounded entry-point expectations and maps detectable adapter failures into deterministic construction, unsupported, or error states.

Provider sessions use public request tokens supplied by the adapter API. Tokens are opaque to provider implementations except for equality and lifetime; providers echo the token on every result so the viewport can associate metadata, progress, frames, cancellation, and failures with the request that caused them.

Tokens are unique within a provider session for as long as late results for that session can arrive. A closed generation's tokens are never accepted for a later generation, even if the public token values compare equal outside their session identity.

## Status And Diagnostics

Public enum values are scoped and stable enough for QML branching.

`errorString`, `warningString`, and factory diagnostic strings are user-presentable, redacted, bounded plain text. Public diagnostics do not expose provider private data, file-system paths, URLs, credentials, raw exception text, or unbounded payload excerpts unless the application explicitly supplied that same text as user-facing content.

`errorString` describes the current accepted display request within the accepted sequence generation. `clear()`, a new accepted sequence assignment, and any accepted display-request identity change clear it before the new request reports its own terminal diagnostic, and transition to `Ready` clears any previous error for that request. Retained display content does not retain the previous generation's error string. Command-only failures are reported through `commandReason` and do not change `errorString`.

`warningString` is advisory. Callers may display or log it, but branchable behavior comes from status, reason, command outcome, revisions, and capability observations. Optional quality degradation may set a warning, but the absence of a warning does not guarantee that every presentation preference was satisfied exactly. Warnings belong to the accepted request or the current presentation fallback that caused them; request-owned warning changes increment `requestRevision`, and presentation-fallback warning changes increment `displayRevision`. `clear()`, a new accepted sequence assignment, any accepted display-request identity change for request-owned warnings, successful replacement of the warned request, or resolution of the presentation fallback clears the warning. Retained display content does not carry an earlier generation's warning into the accepted replacement.

## Out-Of-Scope API

The public API does not include implicit file or URL loading properties on `ImageViewport`, built-in gesture controls, QML callback providers, numeric provider progress, scene graph resource injection, native texture payloads, tiled loading, region loading, or full color-management policy. Those concerns require explicit factories, adapters, or presentation policies with their own contracts.
