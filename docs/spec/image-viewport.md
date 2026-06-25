# ImageViewport

`ImageViewport` is a Qt Quick QML module intended to provide a custom item for displaying image content inside a QML scene.

The module is imported from QML as:

```qml
import ImageViewport
```

The primary QML type is `ImageViewport`. It behaves as a visual Qt Quick item and is designed to expose image viewing behavior through QML properties as the library evolves.

## Purpose

`ImageViewport` displays image content in a Qt Quick scene when the application needs more control than Qt's built-in image items expose.

The item is intended to support still images and animated image sequences. Animated content must not require image files addressed by direct filesystem paths. An application should be able to display animated image data that originates from an archive, container, custom storage layer, or another source that does not naturally exist as an unpacked temporary file.

Animated image playback must not depend exclusively on Qt image plugins. Applications should be able to integrate image decoding libraries directly, including libraries for formats such as HEIF, and present the resulting image sequence through `ImageViewport`.

## Animated Image Behavior

`ImageViewport` should use a sequence playback model. Still images should be treated as single-frame sequences, and animated images should be treated as multi-frame sequences.

Applications should be able to provide image sequence content and frame timing information without requiring direct filesystem paths. The observable result is that callers can present animated content without unpacking it to a temporary path solely to satisfy the viewport.

The viewport should behave as a caller-supplied sequence consumer rather than as a URL loader. A `source: url` loading property is not required behavior for `ImageViewport`; simple construction paths for still images, explicit frame lists, or provider-backed sequences should live on sequence or provider helper types instead of on the viewport item.

The viewport should accept an `ImageSequence` facade as its content input. Decoder providers, archive readers, byte buffers, URLs, files, and custom storage handles should be adapted into an `ImageSequence` before assignment rather than being assigned directly to the viewport item.

The module should make `ImageSequence` creation a documented caller-facing path. Applications should be able to create a sequence from an in-memory still image, a small explicit list of timed frames, or an application-owned provider adapter without relying on `ImageViewport` to perform source lookup. The first construction surface should be QML-callable when its inputs are QML-representable and C++-callable when the input is not naturally a QML value; raw images, raw frame lists, and raw provider objects should be adapted into an `ImageSequence` before assignment to the viewport. QML callers should not need to construct raw pixel buffers directly.

The construction boundary should remain explicit. C++ callers may create sequences from `QImage`-backed still images, timed frame lists, or provider adapters. QML callers should work through module-defined frame references, timed frame entries, sequence builders, provider adapter objects, or factories that return `ImageSequence`. A plain JavaScript object or list should not be treated as a raw image payload or provider by the viewport.

Explicit timed frame list construction should freeze or copy the list into the new sequence generation. Mutating the original QML frame list or helper objects after construction should not implicitly change the displayed sequence; applications should create and assign a new `ImageSequence` when they want changed content.

An assigned `ImageSequence` should be immutable from the viewport's perspective. If the caller changes archive entries, decoder inputs, provider-affecting policy, or frame lists, that change should appear as a replacement sequence assignment rather than an in-place refresh of the active sequence.

The viewport should own playback state for the supplied sequence. Playback state should include one readonly mutually exclusive playback phase, such as stopped, playing, paused, or ended, plus a requested frame target, the display-committed frame when one exists, the total frame count when known, whether the frame count is known, and the playback speed. Callers should control the phase through playback commands rather than assigning the phase directly.

Assigning a non-null sequence should automatically request the initial display frame. Timed playback should not start merely because a sequence was assigned; callers should call `play()` when they want animation time to advance.

The viewport should define loop behavior for animated sequences. The default loop behavior should preserve source metadata when the provider reports it. If source loop metadata is absent, explicitly none, or unknown, source-defined looping should resolve to one complete play-through and then ended. Callers should be able to choose source-defined looping, finite looping with a positive total play-through count, or infinite playback. A finite count of `1` should mean one complete play-through and should be the default finite count; `0` should not mean infinite playback.

Writable numeric viewport properties should reject invalid values by leaving the previous accepted value observable rather than silently clamping. Playback speed should be a finite positive multiplier. `0` should not mean pause, negative speed should not mean reverse playback, and non-finite values should not become valid playback state. Pausing and stopping should be expressed through playback commands.

When a new sequence becomes displayable, playback should have a defined initial requested frame target. Stopping playback should reset automatic playback to that initial target without clearing retained content by itself. Seeking to an available valid stable-index frame should update the display-committed frame after render commit. Seeking to a valid but unavailable frame should update the requested frame target and request state without pretending that the frame is already displayed. Empty sequences, unknown frame counts, missing stable frame indexes, and out-of-range frame selection should have defined behavior that does not produce misleading frame state.

Frame-number properties and `seek(frame)` should be valid only when the active sequence exposes stable display indexes. For index-unstable, position-target, or streaming sequences, frame-number properties should use the defined invalid value `-1` and index-based seek should report unsupported rather than inventing frame numbers. QML callers should be able to inspect capability state before showing frame sliders or frame-number controls.

Playback-position properties and `seekToPosition(milliseconds)` should be valid only when the active sequence exposes playback-position seeking. Position values should use milliseconds and use `-1` when no meaningful position exists. QML callers should be able to build timeline scrubbers for position-seekable sequences without requiring stable display indexes.

Imperative frame seek should have an immediate observable outcome, such as accepted, unsupported, or invalid input, while the longer-lived request and display state remains authoritative through status properties. Accepted means the request entered the viewport request model; it may still wait on provider work, render commit, or request queueing. Invalid or unsupported frame selection should not be reported as successful just because retained content remains visible. Invalid selection should map to public unsupported request status with an invalid-request reason rather than adding a separate invalid request-status category.

Declarative assignment to `requestedFrame` should not have stronger success semantics than `seek(frame)`. If a requested frame assignment is invalid or unsupported, the previous accepted requested target should remain observable and request status should describe the invalid or unsupported request. Position targeting should use `seekToPosition(milliseconds)` rather than a writable requested-position property so command outcomes remain explicit.

When playback or seeking requests a frame that is not yet available, the viewport should retain the currently displayed frame and report a waiting or loading state for the latest request. The unavailable frame should not become the display-committed frame until the provider supplies content that can actually be presented for that frame.

Within the currently assigned sequence generation, playback advances and seek requests should not clear the displayable frame while waiting. Replacement retention policy applies to sequence replacement, not to ordinary in-generation playback or seeking.

The default timed playback policy should preserve sequence order rather than skipping unavailable frames. If the next timed frame is late, playback should hold the current displayable frame and resume from the requested frame when it becomes available. Frame skipping may be added later as an explicit policy, but it should not be the baseline behavior.

Timed playback should be display-driven by default. If the viewport cannot currently make progress toward display-committed content because it has no active rendering context or rendering is suspended by observable Qt Quick lifecycle state, playback should hold its displayed frame and should not silently advance through the sequence. A not-yet-windowed item, invalidated scene graph, released render resources, backend/window replacement, non-exposed suspended window, or `visible: false` item that Qt Quick does not render should not report an error solely because rendering is deferred. Pure geometric offscreen position, ancestor clipping, or opacity should not be a baseline playback-freeze condition unless a future API deliberately adds visibility-aware playback policy. Provider results that match the active generation and request may still be accepted as pending candidates while rendering is deferred, but displayed state and playback duration consumption should advance only after a render commit.

Manual frame inspection should be expressed by pausing playback and selecting the requested frame. The viewport should not require a separate frame-sink mode where the caller repeatedly pushes the display frame outside the sequence playback model.

The viewport should provide an explicit clearing action. Clearing, or assigning a null sequence, should remove retained display content, reset request and display status to the empty/no-request state, clear diagnostics, reset requested frame and position observations to invalid values, reset loop counters, leave playback stopped, and make coordinate conversion report no image content. Clearing previously committed content should advance the display revision so overlays can discard stale annotations.

Calling `clear()` should make the observable `sequence` property null and should have the same external meaning as an imperative `sequence = null` plus retained-display clearing. If application code bound the `sequence` property, `clear()` follows normal QML imperative-assignment behavior; declarative clearing should instead make the binding evaluate to null. A later non-null assignment starts a new sequence generation.

The viewport should make custom image formats viable when the application can decode those formats itself. A format does not need to be registered as a Qt image plugin before its decoded frames can be displayed.

Animated sequences should be able to preserve per-frame durations. The viewport should not require callers to collapse an animated image into a single fixed frame rate when the source format has variable frame timing. QML-visible sequence duration, frame duration, and presentation timestamp values should be normalized to milliseconds.

## Presentation Controls

`ImageViewport` should provide familiar presentation controls from Qt Quick `Image` where they describe the rendered image rather than the loading mechanism.

The viewport should support image placement modes comparable to Qt Quick `Image` where they are useful for a viewport item. At minimum, callers should be able to request stretching to the item bounds, aspect-preserving fit, aspect-preserving crop, and unscaled padding.

The viewport should support horizontal and vertical alignment when the rendered image does not exactly match the item bounds. Horizontal alignment should expose left, center, and right values. Vertical alignment should expose top, center, and bottom values. The default should be centered alignment.

By default, the viewport should use aspect-preserving fit, centered alignment, smooth filtering, disabled mipmap filtering, and no opaque background. These defaults favor image viewing over raw image stretching.

The viewport should expose the visible clipped image content rectangle in local item coordinates. This rectangle describes the axis-aligned item-space footprint of displayable image content after placement, alignment, mirroring, zoom, pan, and the viewport's internal image clipping have been applied. It should not describe the unbounded transformed image outside the viewport, and it should not include child items or inherited scene transforms. Convenience painted-size properties, if exposed, should be derived from this visible clipped rectangle rather than from an unclipped transformed image footprint.

The viewport should also expose the visible image-space rectangle when a meaningful image coordinate rectangle exists. This rectangle describes which part of the displayed image is visible through the viewport after placement, zoom, pan, mirroring, and clipping. It should use the canonical empty value `Qt.rect(0, 0, 0, 0)` when no displayable content exists or when the active sequence cannot expose a stable image-space rectangle.

When the viewport has no displayable content, the rendered content rectangle should use the canonical empty value `Qt.rect(0, 0, 0, 0)`. Coordinate conversion should report the absence of image content rather than returning misleading image coordinates. Callers should use display-status properties and coordinate-conversion validity rather than inferring image presence from rect dimensions alone.

The viewport should support horizontal mirroring through `mirror` and vertical mirroring through `mirrorVertically`. These controls should affect still images and every displayed frame of an animated image sequence.

The viewport should support filtering control through `smooth`. When `smooth` is true, scaled or transformed image content should prefer smooth sampling. When `smooth` is false, the viewport should use nearest-neighbor sampling so enlarged content remains pixelated and suitable for pixel inspection or pixel-art presentation.

The viewport should support mipmap filtering control through `mipmap`. This control should allow callers to request higher-quality minification when image content is scaled down or otherwise transformed, subject to the same kind of quality and performance tradeoff expected from Qt Quick image rendering.

The viewport should support an image orientation policy comparable to Qt Quick `Image` automatic transform behavior. The policy should let applications decide whether decoded image orientation metadata normalizes the supplied frame before viewport placement.

The viewport should allow applications to define how transparent pixels are visually backed. Supported presentation should include a solid background color and a checkerboard-style background for inspecting alpha content.

Solid and checkerboard backing should fill the viewport item's local bounds, not only the image content rectangle. It should remain visible when no image frame is displayable, while `contentRect`, `visibleImageRect`, and coordinate conversion continue to report no image content. Transparent backing should draw nothing.

Checkerboard presentation may start with a built-in default pattern. Custom checkerboard colors and cell size are useful but do not need to be part of the first API unless a concrete caller need appears.

The viewport should define a color handling policy comparable in scope to Qt Quick image rendering rather than promising full viewer-grade color management by default. Applications should be able to preserve source color space metadata, while initial display behavior may assume frames are already display-ready sRGB content. Full embedded-profile conversion, display-profile-aware rendering, wide-gamut handling, HDR tone mapping, and gain-map processing are not baseline display guarantees unless they are added as explicit rendering capabilities.

Orientation policy should distinguish strict normalization from best-effort presentation and metadata preservation. When strict orientation normalization is requested, the viewport should not silently display content that failed the requested orientation policy. Best-effort orientation presentation may display the closest supported result with a warning, and metadata-preservation policy may carry source metadata forward while image coordinates continue to describe the displayed pixel geometry.

The first color policy should stay narrower than orientation policy. It should distinguish assuming display-ready sRGB-like pixels from preserving source color metadata for diagnostics or future rendering paths. Strict or best-effort color conversion should remain outside baseline behavior until an actual conversion path exists; if a future strict color behavior is requested outside the viewport's supported conversion scope, the request should be reported as unsupported rather than approximated as successful display.

## Viewport Behavior

`ImageViewport` should expose the relationship between image coordinates and item coordinates as viewport state. Callers should be able to observe or control zoom and pan as required viewport concepts.

Zoom should be a finite positive multiplier applied after fill-mode placement and alignment. Invalid zoom values should leave the previous accepted zoom unchanged. The default zoom anchor should be the center of the placed content rectangle. Pan should be measured in local item logical pixels and applied after zoom; positive pan moves the rendered image content right and down. Non-finite pan coordinates should leave the previous accepted pan unchanged. Applications that implement cursor-anchored zoom should do so by adjusting pan around the chosen anchor point.

The viewport should clip rendered image content to the item bounds by default. Placement modes, zoom, pan, and crop behavior should not cause image pixels to paint outside the viewport unless a future API explicitly opts into that behavior. This is an internal image-content clip and should not imply Qt `Item::clip` behavior for child items. The inherited `clip` property remains the normal Qt Quick item clipping mechanism.

The viewport should provide coordinate conversion between item coordinates and image coordinates. This is required for user interactions such as pixel inspection, cropping, annotation, and hit testing against image content.

Item-to-image coordinate conversion should make non-image areas distinguishable from valid image coordinates. Points in fit padding, outside the transformed image, outside the clipped viewport, or inside an empty viewport should not be reported as ordinary image pixels.

Image-to-item coordinate conversion should be able to map valid image-space points even when the resulting item-space point is outside the currently visible viewport. Visibility should be queried separately. This makes annotation, crop, and overlay code able to position handles or guides for image points that are currently panned or cropped out of view.

Coordinate conversion should expose invalid mapping explicitly through a QML-friendly result object with `valid` and `point` fields. The v0 C++ surface should return a `QVariantMap` with those keys so QML callers receive an ordinary object-like value. Callers should not need to infer invalidity from sentinel coordinates.

Invalid coordinate-conversion results should use `Qt.point(0, 0)` as the canonical `point` value. The `valid` field remains the only validity signal; the canonical point only gives bindings stable fallback data.

The viewport should define image coordinates in logical image space after source metadata normalization. Device-pixel-ratio handling should not leak into QML item coordinates or make coordinate conversion depend on physical render pixels.

When a metadata-preserving policy keeps orientation metadata without applying it to pixels, image coordinates should follow the displayed pixel geometry. The viewport should not report coordinates in a normalized orientation that was not actually used for display.

The viewport should expose viewport state but should not require built-in pointer, wheel, drag, or pinch gesture handling. Gesture policy remains separate so applications can integrate the viewport with their own input handlers.

## Local Mapping Order

`ImageViewport` should define its local image-to-item mapping in an order compatible with the mental model of Qt Quick `Image`.

The supplied frame should first be normalized according to source-level metadata policy, including EXIF-style orientation when that policy is enabled.

The viewport should then identify the source-space content represented by the supplied frame.

The viewport should compute the base image-to-item mapping from placement policy, aspect fit/crop/stretch/pad behavior, and alignment.

Horizontal and vertical mirroring should flip the selected content inside the chosen displayed rectangle. Mirroring should not change the displayed content rectangle, item geometry, or alignment result.

Viewport-specific state such as zoom and pan should then be composed into that image-to-item mapping. Zoom should scale around the placed content center by default, and pan should translate in local item logical pixels after zoom.

Sampling policy, including `smooth` and mipmap filtering, should affect rendering quality after the local mapping has been chosen. Sampling policy should not alter image layout.

Inherited `QQuickItem` transforms such as item `rotation`, item `scale`, and the item `transform` list should remain outside the viewport's local image-to-item mapping unless an API explicitly asks for scene or window coordinates.

Coordinate conversion between image coordinates and local item coordinates should reflect the final local geometry mapping through mirroring and viewport-specific transforms. Sampling policy should not alter coordinate conversion, and local conversion should not silently include parent or scene transforms.

## State Behavior

`ImageViewport` should expose enough state for QML code to distinguish an empty viewport, a viewport with display-committed content, a viewport waiting for requested content, and a viewport that cannot display the requested content.

The state model should distinguish the content currently displayed from the most recent content request or replacement attempt. This allows QML code to reason about cases where a replacement is loading or has failed while the viewport still displays a previously valid frame.

Request status semantics should remain recognizable to Qt Quick users: empty content, ready content, loading or waiting content, unsupported content, and error content should map cleanly to the API's `NoRequest`, `RequestReady`, `RequestLoading`, `RequestUnsupported`, and `RequestError` states while making unsupported requests distinguishable when useful.

For public request status, ready content should mean the latest request is reflected in display-committed content or required no display change. Provider or CPU candidate readiness before the frame becomes display-committed should remain an internal implementation detail and should not be reported as public ready state.

Unsupported request status should be distinct from error status. Unsupported means the requested seek, access pattern, output policy, or normalization policy is outside the active sequence capabilities. Error means an operation that should have been possible failed.

Waiting or loading status should expose a reason sufficient to distinguish provider waiting from render-deferred waiting. Render-deferred waiting, such as a temporarily unavailable scene graph context, should not be presented as provider latency or decode failure.

Display status should be observable separately from request status. A failed replacement request should not force QML code to infer whether the viewport is visually empty, rendering current content, or rendering retained content from an older sequence generation.

Public state should make common combinations explicit: no sequence is request-null and display-empty; initial provider waiting is loading with no displayable frame; render-deferred waiting is loading for the request while retained display state is preserved; active committed content is ready and current; replacement loading, unsupported, or error may report the new request state while display status remains retained-previous.

The displayed frame value should be valid only when display status indicates retained or committed content. When no frame is displayable, the displayed frame should use the defined invalid value `-1`.

When retained content belongs to an older sequence generation, QML should be able to distinguish that fact. Displayed frame indexes and content geometry should not be assumed to describe the currently assigned sequence unless the viewport reports that displayed content belongs to the current sequence.

QML should be able to detect display changes even when the active sequence has no stable display indexes. The viewport should expose a monotonic display revision and an opaque comparable displayed snapshot token for the currently committed content when available. Ordinary QML bindings should prefer the revision; the token is for comparing committed snapshot identity across observations.

Display revision should describe display-committed content changes, not every geometry-only presentation change. Resize, zoom, pan, alignment, mirroring, and clip changes should notify geometry and mapping state directly without implying that a new snapshot was committed. The displayed snapshot token should be process-local and generation-scoped; applications should not persist it or use it as a decoder, file, or cross-session cache key.

Geometry and coordinate state for an already displayable snapshot should update from item-side mapping state as soon as viewport geometry or presentation properties change. Scene graph commit determines when a new frame snapshot becomes display-committed, but it should not delay `contentRect`, `visibleImageRect`, or coordinate conversion updates for unchanged displayed content.

When an error occurs, the viewport should expose a human-readable error description suitable for diagnostics or user-facing fallback UI.

The display-committed frame should remain immutable from the QML user's perspective until the viewport advances to or commits another presentable frame. Callers should not observe transient clearing between frames during normal playback or sequence replacement.

When displayed content is replaced asynchronously, the viewport should expose a retention policy for the previous displayable frame. The required policies are immediate clearing on replacement, retaining only until replacement reaches a ready/unsupported/error terminal state, and retaining through replacement failure until the caller explicitly clears or assigns another displayable sequence.

The default retention policy should retain through failure. If replacement content fails under that policy, the viewport should expose the failed request state and retain the previous displayable frame. While the error describes the failed replacement, displayed content geometry, coordinate conversion, frame metadata, and rendered output should continue to describe the retained display-committed frame until the caller clears or replaces it. Applications that prefer empty-state transitions should opt into clearing on replacement or retaining only until a terminal replacement failure.

Playback commands and frame-selection commands should always target the active assigned sequence. Retained previous content is a visual fallback, not the command target; applications that want to interact with a previous sequence again should assign that sequence again.

The initial item should keep Qt Quick's default implicit size behavior of `0 x 0` instead of deriving implicit size from the current sequence. A later API may choose image-derived implicit sizing, but the baseline should avoid asynchronous relayout caused by delayed sequence metadata.

An item with non-positive width or height should not commit a new display frame because it has no presentable viewport area. The latest request may remain loading with the public `RenderDeferred` reason until positive item bounds and a scene graph update allow commit. If a previously committed frame already exists and the item is later resized to zero, the frame remains retained for display-state identity, but visible geometry and item-to-image conversion should report no visible image area and timed playback should not advance while presentation is suspended internally.

## Open Surface

The sequence-based input model is decided: `ImageViewport` consumes an `ImageSequence` and does not expose `source: url`. The v0 sequence construction recipes are decided at the caller-facing shape level: still image, explicit timed frames, and provider adapter. Provider protocol signatures, buffering policy, file or archive helper factories, and color-management pipeline remain open.

Tiled presentation modes are not part of the required placement behavior yet. They remain open until a concrete viewport use case requires repeating image content horizontally, vertically, or in both directions.

Large-image region and level-of-detail supply remains open. The viewport should keep large-image scenarios in mind, but a region/LOD provider protocol is not part of the required behavior until the source model is designed.

Arbitrary transform control beyond the required zoom and pan concepts remains open.

Explicit content rotation remains open as a future presentation transform. Future behavior should define API naming, angle granularity, rotation center, transform composition order, and how rotated content affects reported bounds.

Image-derived implicit sizing remains open as a future layout policy. The v0 behavior is to keep the item at Qt Quick's default implicit size unless the caller or layout assigns dimensions.

Low-level provider capability inspection remains open. The baseline QML item should expose only the capability properties needed for common controls, while detailed provider traits such as single-flight behavior, rewind cost, frame addressing mode, and decode dependency hints remain diagnostic or architecture-level concerns until a concrete caller need appears.

Advisory seek interaction reporting remains open. The baseline QML item should expose `canSeek`, `canSeekByFrame`, and `canSeekByPosition`; a later API may add a compact value such as `seekBehavior` if caller UI needs to distinguish immediate, waiting, serialized, or expensive seek interactions.
