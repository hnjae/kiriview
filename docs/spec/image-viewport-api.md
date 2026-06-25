# ImageViewport API

This document records the v0 caller-facing API surface for `ImageViewport`. Names may still be adjusted before a stable release, but concepts in the committed surface describe intended external behavior. Deferred or exploratory ideas belong in the Deferred API section rather than in the v0 item contract.

The API should make `ImageViewport` feel like a Qt Quick image item while keeping its input model centered on caller-supplied image sequences rather than URLs. It is a sequence consumer, not a source URL loader.

## Input Model

The primary content input should be an `ImageSequence` facade object, exposed conceptually as `sequence`.

```qml
ImageViewport {
    sequence: mySequence
}
```

Provider objects should not be assigned directly to `ImageViewport`. A decoder, archive reader, file resolver, byte-buffer reader, or application-owned provider should be wrapped by an `ImageSequence` factory or adapter before it reaches the viewport item. This keeps the viewport's QML contract centered on displayable sequences rather than on every possible loading or decoding backend.

`ImageViewport` should not expose `source: url`. URL resolution, filesystem access, archive lookup, network loading, and direct integration with storage systems should remain caller or provider responsibilities.

The core model should remain suitable for archive entries, container-backed images, custom storage, application-owned decoders, and formats that are not registered as Qt image plugins.

Still images should be represented as single-frame sequences. Animated images should be represented as timed multi-frame sequences.

The API should still provide a practical way to create sequence objects. `source: url` should remain absent from `ImageViewport`, but the module or companion types should offer simple construction paths for at least an in-memory still image, an explicit frame list, and an application-provided sequence provider. File, URL, archive, or byte-buffer helpers may exist in a separate adapter or factory layer that returns `ImageSequence` objects rather than as `ImageViewport` loading properties.

The minimum caller recipe should be explicit: QML assigns an `ImageSequence`; C++ or companion helper code creates that sequence from an image, a list of timed frames, or a provider adapter; the viewport never owns storage lookup or decoder selection.

The v0 module should include a first-class sequence construction surface with the three required recipes `ImageSequence.fromImage(...)`, `ImageSequence.fromFrames(...)`, and `ImageSequence.fromProvider(...)` or their direct C++ equivalents. The exact C++ spelling may adapt to Qt binding constraints, but these recipes are required caller-facing behavior rather than optional scaffolding. QML callers normally receive `ImageFrame` objects from C++ code, module helpers, or provider adapters rather than manufacturing pixel buffers in JavaScript. URL, archive, and file helpers should be additional provider factories that return `ImageSequence` objects, not alternate loading properties on `ImageViewport`.

The first public construction surface should be QML-callable when its inputs are QML-representable, and C++-callable when the input is not naturally a QML value. In-memory still images and explicit timed frame lists may be exposed through module-defined helper value/object types such as image frame references, timed frame entries, or a sequence builder. Application-owned decoders should enter through a QObject provider adapter or a C++ factory that returns an `ImageSequence`. QML code should not be expected to manufacture pixel buffers directly, and `ImageViewport` should not accept raw `QImage`, raw frame lists, or raw provider objects directly as its `sequence` value.

The minimum construction boundary should be explicit. C++ callers should be able to construct a sequence from `QImage`-backed still frames, timed frame lists, or provider adapters. QML callers should construct only module-defined objects such as opaque frame references, timed frame entries, sequence builders, or provider adapter objects, then pass those objects to `ImageSequence` helpers. `ImageFrame` owns or shares an immutable display-ready frame payload, `TimedImageFrame` owns a frame reference plus duration metadata, and provider adapters own their provider bridge for the constructed sequence generation. A raw JavaScript object or list should not become an implicit pixel buffer or provider just because it has similarly named fields.

An explicit timed frame list should contain display-ready full-frame entries plus per-frame duration metadata. The frame entry type may be a module-defined image frame object rather than a bare QML value, because QML has no native `QImage` value type.

Explicit timed frame list construction should freeze or copy the frame list at sequence creation time. Later mutation of the original QML list or helper frame objects should not silently alter an active sequence generation; callers who want changed content should create and assign a new `ImageSequence`.

An assigned `ImageSequence` should be immutable from the viewport's perspective. Content replacement, archive entry changes, decoder reset, or provider-affecting policy changes should enter the viewport as a new `ImageSequence` assignment that creates a new generation. The initial API should avoid in-place `ImageSequence.refresh()` semantics because they blur the boundary between retained display, provider lifetime, and generation-scoped request filtering.

The intended caller recipes are:

```qml
ImageViewport {
    sequence: stillSequence
}

// Created by a module helper, C++ factory, or application adapter.
property ImageSequence stillSequence: ImageSequence.fromImage(stillFrame)
property ImageSequence animatedSequence: ImageSequence.fromFrames(timedFrames)
property ImageSequence providerSequence: ImageSequence.fromProvider(appProvider)
```

The example names are descriptive placeholders, but the shape is intentional: QML assigns only an `ImageSequence`; helper objects or C++ code adapt images, timed frame lists, byte sources, archive entries, URLs, or application decoders into that sequence.

## Companion Sequence Surface

The first caller-facing sequence helpers should be concrete enough that applications can display content without subclassing the viewport or relying on undocumented C++ scaffolding.

`ImageSequence` should be the object assigned to `ImageViewport.sequence`. It should be a handle for an immutable sequence generation source, not a mutable image model owned by the viewport.

`ImageFrame` should be an opaque module-defined frame reference for QML-visible construction paths. It may wrap a C++ `QImage` payload, a module-owned decoded frame, or another display-ready CPU frame representation. QML code should be able to pass an `ImageFrame` around but should not be expected to construct raw pixels directly.

`TimedImageFrame` should pair an `ImageFrame` with a millisecond duration for explicit small animations. A timed frame list should be copied or frozen when `ImageSequence.fromFrames(...)` creates a sequence.

`ImageSequenceProviderAdapter` should be the QML/C++ bridge for application-owned decoders and storage systems. It should adapt provider callbacks or request methods into an `ImageSequence` rather than becoming a value accepted directly by `ImageViewport.sequence`.

The minimum public construction recipes should therefore be:

```qml
property ImageSequence stillSequence: ImageSequence.fromImage(imageFrame)
property ImageSequence animatedSequence: ImageSequence.fromFrames(timedImageFrames)
property ImageSequence providerSequence: ImageSequence.fromProvider(providerAdapter)
```

File, URL, archive, byte-buffer, and codec-specific helpers may exist as separate factories, but their output should still be an `ImageSequence`. The viewport should not add parallel loading properties for those sources.

## QML Item Surface

The QML-facing item should expose playback, presentation, viewport mapping, status, and coordinate conversion as first-class concepts.

```qml
ImageViewport {
    sequence: mySequence

    requestedFrame: 0
    speed: 1.0
    loopMode: ImageViewport.SourceLoop

    fillMode: ImageViewport.PreserveAspectFit
    horizontalAlignment: ImageViewport.AlignHCenter
    verticalAlignment: ImageViewport.AlignVCenter

    zoom: 1.0
    pan: Qt.point(0, 0)

    smooth: true
    mipmap: false
    mirror: false
    mirrorVertically: false

    Component.onCompleted: play()
}
```

The initial QML item surface should include the following concepts.

```qml
property ImageSequence sequence

readonly property bool frameCountKnown
readonly property int frameCount
readonly property bool durationKnown
readonly property real duration
readonly property bool canSeek
readonly property bool canSeekByFrame
readonly property bool canSeekByPosition
readonly property bool streaming
property int requestedFrame
readonly property int displayedFrame
readonly property real requestedPosition
readonly property real displayedPosition
readonly property PlaybackState playbackState
property real speed
property LoopMode loopMode
property int loopCount
readonly property int completedLoops

readonly property RequestStatus requestStatus
readonly property RequestStatusReason requestStatusReason
readonly property DisplayStatus displayStatus
readonly property bool hasDisplayableFrame
readonly property bool displayedBelongsToCurrentSequence
readonly property int displayRevision
readonly property string displayedSnapshotToken
readonly property string errorString
readonly property string warningString
property RetentionPolicy retentionPolicy

property FillMode fillMode
property HorizontalAlignment horizontalAlignment
property VerticalAlignment verticalAlignment
readonly property rect contentRect
readonly property rect visibleImageRect
readonly property real paintedWidth
readonly property real paintedHeight

property real zoom
property point pan

property bool smooth
property bool mipmap
property bool mirror
property bool mirrorVertically

property OrientationPolicy orientationPolicy
property BackgroundMode backgroundMode
property color backgroundColor
property ColorPolicy colorPolicy

function play()
function pause()
function stop()
function seek(frame): RequestOutcome
function seekToPosition(milliseconds): RequestOutcome
function clear()

function mapItemToImage(point)
function mapImageToItem(point)
function containsVisibleImagePoint(point)
```

Concrete type names may change, but the observable concepts should remain stable unless later research invalidates them.

The minimal public header should expose these concepts as Qt properties, enums, and invokable commands even before every backend behavior is implemented. Placeholder implementations may return empty or default state while the provider, playback, and render layers are built, but the QML surface should not remain an unrelated empty `QQuickItem` once this API is treated as v0 intent.

## Playback State

`ImageViewport` should be a sequence consumer with its own playback state. Callers provide sequence content and frame timing metadata through `ImageSequence`; the viewport selects requested frame targets according to play, pause, stop, seek, `requestedFrame`, `speed`, `loopMode`, and `loopCount`.

The playback phase should be mutually exclusive. The expected phases are stopped, playing, paused, and ended.

`playbackState` should be a readonly observation property. Callers should control playback through `play()`, `pause()`, `stop()`, `seek(frame)`, `requestedFrame`, `speed`, `loopMode`, and `loopCount` rather than assigning playback phases directly. This avoids ambiguous assignments such as setting `Ended` by hand.

Assigning a non-null `sequence` should request the initial display frame automatically. Timed playback should not begin solely because a sequence was assigned; callers should call `play()` when they want animation time to advance. This keeps still-image display immediate while preserving explicit playback control for animated sequences.

`requestedFrame` should represent the selected or requested frame target from the caller's perspective when the active sequence exposes stable display indexes. `displayedFrame` should represent the stable frame index that has become display-committed or is retained as previously committed content when such an index exists. During pending seek, waiting, replacement, or upload failure, these values may differ.

Assigning `requestedFrame` and calling `seek(frame)` should target the same frame-selection behavior when the active sequence supports stable display indexes. `seek(frame)` is the preferred command-style surface when QML code needs the immediate `RequestOutcome`; `requestedFrame` remains useful for declarative bindings and inspector controls.

If assigning `requestedFrame` is invalid or unsupported, the assignment should not be reported as a successful target change merely because retained content remains visible. The setter should leave the last accepted requested frame observable, set `requestStatus` to `Unsupported` with `requestStatusReason` set to `InvalidRequest` or `UnsupportedRequest`, and keep display state unchanged unless a later accepted request commits a frame.

`seek(frame)` should return a lightweight request outcome so QML can distinguish an admitted request from an immediately unsupported or invalid request. `Accepted` means the request entered the viewport request model; it may still be queued, waiting on the provider, waiting on render commit, or complete immediately. Waiting and queueing are long-lived request-state facts reported through `requestStatus` and `requestStatusReason`, not separate successful command outcomes. The authoritative long-lived state remains `requestStatus`, `requestStatusReason`, `requestedFrame`, and `displayedFrame`; the return value is only the immediate command outcome.

Index-based frame APIs should be capability-gated. `canSeekByFrame` is the primary QML condition for frame-number controls. When `canSeekByFrame` is false, `requestedFrame` and `displayedFrame` should use the defined invalid value `-1`, assigning `requestedFrame` should not pretend success, and `seek(frame)` should report an unsupported request outcome. Display can still be valid without a stable frame index; callers should use request and display status rather than frame numbers for those sequences.

Position-based APIs should be capability-gated separately from frame-index APIs. `seekToPosition(milliseconds)` should request the frame appropriate for a non-negative playback position in milliseconds when `canSeekByPosition` is true. `requestedPosition` and `displayedPosition` should use milliseconds and should use `-1` when no meaningful playback position exists. Position seeking may be accepted, may later wait on provider or render work, or may be unsupported according to sequence capabilities, and it should return the same immediate `RequestOutcome` categories as frame seeking.

`requestedPosition` should be readonly in the initial API. Declarative assignment semantics apply to `requestedFrame` only; position targeting should use `seekToPosition(milliseconds)` so invalid, unsupported, or accepted command outcomes are explicit. This keeps timeline scrubbers command-driven instead of implying that every position-binding update is a successful display target.

`canSeek` should be a broad convenience value for any supported user seek operation. `canSeekByFrame` should describe indexed frame seeking specifically, and `canSeekByPosition` should describe time or playback-position seeking. Applications that build a frame slider should bind to `canSeekByFrame` and `frameCountKnown`; applications that build a timeline scrubber should bind to `canSeekByPosition` and `durationKnown`. If a frame-addressing mode is exposed later, it should be a secondary descriptive value for diagnostics or presentation choices, not the primary branching surface when boolean capabilities are available.

The initial public capability surface should stay small. `frameCountKnown`, `durationKnown`, `canSeek`, `canSeekByFrame`, `canSeekByPosition`, and `streaming` are the canonical QML branching properties. Lower-level provider traits such as single-flight request serialization, seek-may-wait cost, random-access mode, rewind cost, and frame-addressing mode should remain diagnostics or deferred API. A later small advisory value such as `seekBehavior` may be added if caller UI needs to distinguish immediate, waiting, serialized, or expensive seek interactions, but it should not be required for ordinary QML control flow in v0.

Manual frame inspection should use paused playback plus `requestedFrame`; the API should not require a separate frame-sink mode where QML repeatedly pushes the currently displayed frame.

`frameCountKnown` should make unknown frame counts explicit. `frameCount` should only be interpreted as an exact total when `frameCountKnown` is true, so QML callers do not need to rely on sentinel values such as `-1`.

`displayedFrame` should be interpreted only when `hasDisplayableFrame` is true. When `hasDisplayableFrame` is false, `displayedFrame` should have the defined invalid value `-1` and should not be used for image-coordinate decisions.

`displayedBelongsToCurrentSequence` should indicate whether `displayedFrame` and display geometry describe the currently assigned sequence generation. It may be false when a replacement sequence is loading or failed while retained content from a previous sequence remains visible.

`displayRevision` should increment whenever display-committed content changes or is cleared. `displayedSnapshotToken` should be an opaque comparable string derived from the committed snapshot identity when one exists, and an empty string when no committed snapshot exists. QML overlays should prefer `displayRevision` for ordinary change detection and use the token only when they need to compare whether two observations refer to the same committed snapshot without relying on stable frame indexes.

Geometry-only presentation changes such as item resize, fill-mode recomputation, zoom, pan, alignment, mirroring, or viewport clipping should notify their affected geometry properties, but should not increment `displayRevision` by themselves when the committed snapshot is unchanged. This keeps `displayRevision` useful as a content commit signal, while overlay layout can follow `contentRect`, `visibleImageRect`, and coordinate-conversion changes through their own notifications.

`contentRect`, `visibleImageRect`, and coordinate conversion should follow the current item-side mapping state for the current displayable frame immediately after geometry-affecting property changes. They should not wait for a new scene graph commit when the committed snapshot is unchanged. Scene graph commit controls when a new snapshot becomes display-committed; it does not delay ordinary QML geometry observations for an already displayable snapshot.

`displayedSnapshotToken` should be process-local and generation-scoped. It is suitable for comparing current QML observations inside the running viewport, but applications should not persist it across sessions, treat it as a file or decoder cache key, or assume it is meaningful after the sequence generation has been replaced.

Playback commands and frame-selection commands should always target the active assigned sequence. Retained previous content is a visual fallback, not a resurrected playback target. To control a previous sequence again, the caller should assign that sequence again.

`stop()` should reset automatic playback to the initial frame target for the current sequence and make the playback clock inactive. It should not clear retained display content by itself. If the initial frame is not already display-committed, the current displayable frame may remain visible while the initial-frame request is pending. A later `play()` after `stop()` should start from the initial frame target rather than from the frame where playback was stopped.

Writable numeric viewport properties should prefer predictable rejection over silent coercion. If `speed`, `zoom`, `loopCount`, or another constrained writable property receives an invalid value, the setter should leave the previous accepted value observable, update `warningString` or equivalent diagnostics when useful, and avoid changing request or display state unless the property itself defines a narrower rule. This keeps QML bindings from observing surprising clamp behavior while still making invalid input non-fatal.

`speed` should be a finite positive playback multiplier. `1.0` means source timing, values above `1.0` play faster, and values below `1.0` but greater than zero play slower. `0`, negative values, NaN, and infinity should be rejected without changing the previous accepted speed. Pausing and stopping should use `pause()` and `stop()`. Reverse playback remains outside the initial API because it conflicts with sequential providers.

Loop control should separate the override mode from the finite count. `loopMode` should support `SourceLoop`, `FiniteLoop`, and `InfiniteLoop`. The default should be `SourceLoop` so animated formats can preserve their metadata when the provider reports it. If source loop metadata is absent, `none`, or `unknown`, `SourceLoop` should resolve to one complete play-through and then `Ended`; callers who want indefinite playback should set `InfiniteLoop` explicitly. `loopCount` should default to `1`, should be meaningful only with `FiniteLoop`, and should be a positive total number of complete sequence play-throughs; `1` means play once and enter ended after the final frame duration completes. `0` and negative values are not valid finite playback counts and should be rejected without changing the previous accepted `loopCount`. `completedLoops` should expose the number of complete play-throughs that have elapsed without forcing applications to infer it from `requestedFrame`.

`duration` and frame-duration values surfaced to QML should use milliseconds. A real-valued millisecond duration allows providers to normalize formats with arbitrary timescales while still matching Qt Quick timer and animation conventions. `durationKnown == false` should make `duration` informational only.

`clear()` should explicitly remove the assigned sequence request and clear retained display content, status diagnostics, requested-frame state, requested-position state, loop counters, and display-committed geometry. It should leave playback phase `Stopped`, set frame and position observations to their invalid values, make `displayStatus` empty, make `requestStatus` null, and increment `displayRevision` when it clears previously committed content.

`clear()` should be externally equivalent to an imperative `sequence = null` followed by clearing retained display state. The observable `sequence` property should become null. If application code had established a QML binding on `sequence`, calling `clear()` should follow normal QML imperative-assignment semantics; applications that want declarative clearing should make the binding expression evaluate to null instead. If a binding or later assignment provides a non-null `ImageSequence` again, that assignment creates a new sequence generation.

The initial item should not derive `implicitWidth` or `implicitHeight` from the current sequence. Unless the caller or layout assigns a size, the item keeps the default Qt Quick implicit size behavior of `0 x 0`. A future layout policy may derive implicit size from normalized image dimensions, but v0 should avoid relayout surprises while asynchronous sequence metadata is still loading.

An item with `width <= 0` or `height <= 0` has no presentable viewport area. A new candidate frame should not become display-committed while the item has no positive local bounds; the latest request should remain loading with the public `RenderDeferred` reason until positive bounds and a scene graph update allow commit. If the item already has retained display-committed content and is later resized to zero, that content remains the retained displayed snapshot for state purposes, but `contentRect`, `visibleImageRect`, and item-to-image conversion should report no visible image area and timed playback should not consume frame duration while presentation is suspended internally.

## Status State

The status model should distinguish the latest content request from the content currently display-committed.

`requestStatus` should describe the most recent content request or replacement attempt in terms recognizable to Qt Quick users: null or empty, loading or waiting, ready, unsupported, and error. Public `Ready` should mean the requested content has become display-committed or that the latest request did not need to change the already display-committed content. Internal candidate-frame readiness before the frame becomes display-committed should not be exposed as public `Ready`.

`requestStatusReason` should distinguish why a non-ready request is not yet display-committed. Expected reasons include provider waiting, render deferred because no scene graph context is available, upload pending, unsupported request, and failure. `Unsupported` should remain distinguishable from `Error`; it means the requested operation or policy is not supported, not that an expected operation failed.

`displayStatus` should describe whether the viewport currently has no display-committed content, has current renderable content for the active sequence, or is retaining renderable content from a previous sequence generation.

`hasDisplayableFrame` should describe whether the viewport currently has retained display-committed content. This is intentionally separate from `requestStatus` because replacement failure may produce `requestStatus == Error` while the viewport still renders a previous valid frame.

`errorString` should describe the failed request or display problem without implying that retained content has been cleared.

`warningString` or an equivalent diagnostics surface should describe recoverable best-effort presentation differences, such as an orientation policy that could only be approximated or preserved source color metadata that the baseline renderer did not convert.

`retentionPolicy` should define what happens to already display-committed content while a replacement sequence request is pending or fails. The canonical policies should be `ClearOnReplacement`, `RetainUntilReady`, and `RetainThroughFailure`.

The default value should be `RetainThroughFailure` because image-viewer behavior benefits from avoiding flicker and from preserving a known-good frame when replacement content fails. `RetainUntilReady` should retain during loading but clear if the replacement reaches unsupported or error state. `ClearOnReplacement` should clear immediately when a new replacement request is accepted. A Qt-compatible `retainWhileLoading` convenience may be added later, but it should map to `retentionPolicy` rather than becoming an independent state source.

Within the currently assigned sequence generation, timed playback advances and explicit seek requests should always retain the current display-committed frame until another frame is committed, the viewport is cleared, or the sequence is replaced. `retentionPolicy` should not make normal playback or in-generation seeking flicker to empty content.

The public status enums should be the canonical branching surface for QML code. Derived convenience booleans may be added later if they remove real QML boilerplate, but they should not become an independent state source.

| Enum | Values |
| --- | --- |
| `PlaybackState` | `Stopped`, `Playing`, `Paused`, `Ended` |
| `RequestStatus` | `Null`, `Loading`, `Ready`, `Unsupported`, `Error` |
| `RequestStatusReason` | `None`, `ProviderWaiting`, `RequestQueued`, `RenderDeferred`, `UploadPending`, `UnsupportedRequest`, `UnsupportedPolicy`, `InvalidRequest`, `CpuPreparationFailed`, `TextureUploadFailed`, `ProviderFailure` |
| `DisplayStatus` | `Empty`, `Current`, `RetainedPrevious` |
| `LoopMode` | `SourceLoop`, `FiniteLoop`, `InfiniteLoop` |
| `RequestOutcome` | `Accepted`, `Unsupported`, `Invalid` |
| `FillMode` | `Stretch`, `PreserveAspectFit`, `PreserveAspectCrop`, `Pad` |
| `HorizontalAlignment` | `AlignLeft`, `AlignHCenter`, `AlignRight` |
| `VerticalAlignment` | `AlignTop`, `AlignVCenter`, `AlignBottom` |
| `OrientationPolicy` | `BestEffortApply`, `PreserveMetadata`, `StrictApply` |
| `ColorPolicy` | `AssumeSrgb`, `PreserveMetadata` |
| `BackgroundMode` | `Transparent`, `SolidColor`, `Checkerboard` |
| `RetentionPolicy` | `ClearOnReplacement`, `RetainUntilReady`, `RetainThroughFailure` |

Typical status combinations should be defined as observable behavior.

| Scenario | `requestStatus` | `requestStatusReason` | `displayStatus` | `hasDisplayableFrame` | `displayedBelongsToCurrentSequence` |
| --- | --- | --- | --- | --- | --- |
| No sequence assigned | `Null` | `None` | `Empty` | false | false |
| Initial frame waiting on provider | `Loading` | `ProviderWaiting` | `Empty` | false | false |
| Candidate waiting for scene graph context | `Loading` | `RenderDeferred` | previous state or `Empty` | previous value | previous value |
| Active sequence committed | `Ready` | `None` | `Current` | true | true |
| Replacement loading while retained frame is visible | `Loading` | provider or render reason | `RetainedPrevious` | true | false |
| Replacement unsupported while retained frame is visible | `Unsupported` | `UnsupportedRequest` or `UnsupportedPolicy` | `RetainedPrevious` | true | false |
| Replacement failed while retained frame is visible | `Error` | failure reason | `RetainedPrevious` | true | false |
| Active frame upload failed with no retained frame | `Error` | `TextureUploadFailed` | `Empty` | false | false |

## Presentation State

`fillMode` should support stretch, aspect-preserving fit, aspect-preserving crop, and unscaled padding. Tiling remains outside the initial API. The default should be `PreserveAspectFit`.

Alignment should apply when the displayed content does not exactly match the item bounds. The default should be centered alignment: `AlignHCenter` horizontally and `AlignVCenter` vertically. Horizontal alignment accepts only `AlignLeft`, `AlignHCenter`, and `AlignRight`; vertical alignment accepts only `AlignTop`, `AlignVCenter`, and `AlignBottom`. Invalid enum assignments should leave the previous accepted value unchanged.

`contentRect` should expose the visible clipped image content rectangle in local item coordinates. It should reflect placement, alignment, mirroring, zoom, pan, and internal viewport clipping, but not inherited scene transforms or child item bounds. This is intentionally a viewport-oriented rectangle, not the unbounded transformed image footprint; applications that need hit testing, overlay layout, or visible-area UI should prefer it over Qt `Image` mental models for painted size.

`visibleImageRect` should expose the image-space rectangle that is currently visible inside the viewport after placement, mirroring, zoom, pan, and internal clipping. It should be `Qt.rect(0, 0, 0, 0)` when no displayable content exists or when the current sequence cannot expose a meaningful image-space rectangle. Callers should use `hasDisplayableFrame` and the coordinate conversion result's `valid` field instead of treating a zero-size rect as a separate validity channel. This is a convenience for crop UI, pixel inspection, and annotation overlays; it should be consistent with the coordinate conversion functions.

`paintedWidth` and `paintedHeight` may remain available for familiarity, but they should be convenience observations derived from the visible clipped `contentRect`: `paintedWidth == contentRect.width` and `paintedHeight == contentRect.height`. They are therefore viewport-visible painted dimensions, not the width or height of an unclipped transformed image. Callers that need placement, hit testing, or overlay layout should prefer `contentRect`.

`smooth == true` should request smooth sampling. `smooth == false` should request nearest-neighbor sampling.

`mipmap`, `mirror`, and `mirrorVertically` should map to the presentation behavior already defined in the main `ImageViewport` behavior specification.

`backgroundMode` should describe the viewport backing behind image content, not the image content rectangle itself. `Transparent` draws no backing. `SolidColor` and `Checkerboard` fill the entire local item bounds, are clipped to the item bounds, and remain visible even when no image frame is displayable. The backing is not included in `contentRect`, does not make coordinate conversion valid, and exists only to make transparent image pixels and empty viewport space visually inspectable.

## Viewport Mapping

`zoom` and `pan` should be part of the QML-visible viewport state. `zoom` should be a finite positive multiplier applied after fill-mode placement and alignment. Invalid zoom values should leave the previous accepted zoom unchanged. The default zoom anchor should be the center of the placed content rectangle; applications that need cursor-anchored zoom can preserve the cursor point by adjusting `pan`. `pan` should be measured in local item logical pixels and applied after zoom; positive `pan.x` moves the rendered image content to the right, and positive `pan.y` moves it downward. Non-finite pan coordinates should be rejected without changing the previous accepted pan. The initial API should not include built-in pointer, wheel, drag, pinch gesture policy, or high-level view manipulation helpers such as `setZoomAround(...)`; those helpers may be added later if repeated QML code proves the need.

Coordinate conversion should include item-to-image and image-to-item mapping. Non-image areas should be distinguishable from valid image coordinates.

`mapItemToImage(point)` and `mapImageToItem(point)` should return a QML-friendly object with `valid: bool` and `point: point` fields. The v0 C++ return type should be `QVariantMap` with keys `valid` and `point`, because it is simple for QML callers and does not require a separate result object lifetime. `mapItemToImage(point)` should report `valid == false` for padding, clipped-out item regions, empty content, unsupported mappings, and points outside the image. `mapImageToItem(point)` should report `valid == true` for points inside the current image coordinate bounds even when the mapped item point lies outside the visible viewport; this keeps annotation and crop overlays able to position offscreen handles. Points outside the image bounds, empty content, and unsupported mappings should report `valid == false`. Invalid mapping results should use the canonical point value `Qt.point(0, 0)` so QML bindings have stable fallback data while still branching on `valid`.

Common QML branching should follow the high-level state model. Frame sliders should require `canSeekByFrame && frameCountKnown`; timeline scrubbers should require `canSeekByPosition && durationKnown`; overlays should require `hasDisplayableFrame`, a valid coordinate conversion result, and, when they refer to the active sequence specifically, `displayedBelongsToCurrentSequence`. Error UI should inspect `requestStatus` and `errorString` without assuming that `hasDisplayableFrame` is false, because replacement failure may retain a valid previous frame.

`containsVisibleImagePoint(point)` should accept an image-space point and report whether that point belongs to the currently displayable image content and is visible inside the viewport's local item bounds after placement, mirroring, zoom, pan, and internal clipping. Item-space hit testing should use `mapItemToImage(point).valid`.

The coordinate API should be explicitly local-item based. Inherited `QQuickItem` transforms should not be silently included unless a later API explicitly asks for scene or window coordinates.

Coordinate conversion should follow the actual normalized frame geometry used for display. If orientation metadata is preserved but not applied, image coordinates should describe the displayed pixel geometry rather than pretending that metadata normalization occurred.

## Provider Boundary

Provider details are intentionally not part of the `ImageViewport` item API. The user-facing rule is that providers, decoder adapters, and factories must produce an `ImageSequence`; the viewport consumes that sequence through documented playback, status, presentation, and mapping properties.

Each frame that reaches the viewport should already be a display-ready full-frame snapshot from the caller's perspective. Codec-native disposal, blending, reference-frame, palette, delta-frame, and composition rules belong to the provider or decoder adapter, not to QML-facing viewport behavior.

Provider capability detail may influence the derived QML properties listed above, but QML applications should not need to understand the full provider protocol for ordinary UI decisions. The requested orientation policy carries the public compliance level for delivered frames. The v0 color policy remains narrower: it can assume display-ready sRGB-like pixels or preserve source color metadata for diagnostics and future rendering paths. The internal provider request/result model is documented in architecture documents.

## Deferred API

The following surfaces should remain outside the initial API until there is a concrete need and implementation strategy.

- Native texture import or caller-owned `QSGTexture` exchange.
- Region or level-of-detail provider protocol for very large images.
- Best-effort or strict embedded-profile conversion, display-profile-aware rendering, wide-gamut handling, HDR tone mapping, gain-map processing, or any color behavior beyond assuming display-ready sRGB-like content and preserving metadata.
- Arbitrary content rotation beyond metadata orientation policy.
- Detailed cache policy controls beyond advisory preparation hints.
- Low-level provider capability inspection such as single-flight request detail, random-access class, rewind cost, frame-addressing mode, and decode dependency hints.
- Advisory seek interaction reporting, such as a future `seekBehavior` value that distinguishes immediate, waiting, serialized, or expensive seek interactions after concrete caller UI needs appear.
