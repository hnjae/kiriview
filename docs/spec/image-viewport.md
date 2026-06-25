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

The viewport should own playback state for the supplied sequence. Playback state should include one mutually exclusive playback phase, such as stopped, playing, paused, or ended, plus a requested frame target, the display-committed frame when one exists, the total frame count when known, whether the frame count is known, and the playback speed.

The viewport should define loop behavior for animated sequences. Callers should be able to distinguish finite playback from infinite playback and observe when finite playback reaches the end of the sequence.

When a new sequence becomes displayable, playback should have a defined initial requested frame. Seeking to an available valid frame should update the display-committed frame immediately. Seeking to a valid but unavailable frame should update the requested frame and request state without pretending that the frame is already displayed. Empty sequences, unknown frame counts, and out-of-range frame selection should have defined behavior that does not produce misleading frame state.

When playback or seeking requests a frame that is not yet available, the viewport should retain the currently displayed frame and report a waiting or loading state for the latest request. The unavailable frame should not become the display-committed frame until the provider supplies content that can actually be presented for that frame.

The default timed playback policy should preserve sequence order rather than skipping unavailable frames. If the next timed frame is late, playback should hold the current displayable frame and resume from the requested frame when it becomes available. Frame skipping may be added later as an explicit policy, but it should not be the baseline behavior.

Timed playback should be display-driven by default. If the viewport cannot currently make progress toward display-committed content because it has no active rendering context, playback should hold its displayed frame and should not silently advance through the sequence offscreen. A temporarily hidden, offscreen, or not-yet-windowed item should not report an error solely because rendering is deferred.

Manual frame inspection should be expressed by pausing playback and selecting the requested frame. The viewport should not require a separate frame-sink mode where the caller repeatedly pushes the display frame outside the sequence playback model.

The viewport should make custom image formats viable when the application can decode those formats itself. A format does not need to be registered as a Qt image plugin before its decoded frames can be displayed.

Animated sequences should be able to preserve per-frame durations. The viewport should not require callers to collapse an animated image into a single fixed frame rate when the source format has variable frame timing.

## Presentation Controls

`ImageViewport` should provide familiar presentation controls from Qt Quick `Image` where they describe the rendered image rather than the loading mechanism.

The viewport should support image placement modes comparable to Qt Quick `Image` where they are useful for a viewport item. At minimum, callers should be able to request stretching to the item bounds, aspect-preserving fit, aspect-preserving crop, and unscaled padding.

The viewport should support horizontal and vertical alignment when the rendered image does not exactly match the item bounds.

By default, the viewport should use aspect-preserving fit, centered alignment, smooth filtering, disabled mipmap filtering, and no opaque background. These defaults favor image viewing over raw image stretching.

The viewport should expose the visible image content rectangle in local item coordinates. This rectangle describes the axis-aligned item-space footprint of displayable image content after placement, alignment, mirroring, zoom, pan, and the viewport's internal image clipping have been applied. It should not describe the unbounded transformed image outside the viewport, and it should not include child items or inherited scene transforms.

When the viewport has no displayable content, the rendered content rectangle should have a well-defined empty or invalid value. Coordinate conversion should report the absence of image content rather than returning misleading image coordinates.

The viewport should support horizontal mirroring through `mirror` and vertical mirroring through `mirrorVertically`. These controls should affect still images and every displayed frame of an animated image sequence.

The viewport should support filtering control through `smooth`. When `smooth` is true, scaled or transformed image content should prefer smooth sampling. When `smooth` is false, the viewport should use nearest-neighbor sampling so enlarged content remains pixelated and suitable for pixel inspection or pixel-art presentation.

The viewport should support mipmap filtering control through `mipmap`. This control should allow callers to request higher-quality minification when image content is scaled down or otherwise transformed, subject to the same kind of quality and performance tradeoff expected from Qt Quick image rendering.

The viewport should support an image orientation policy comparable to Qt Quick `Image` automatic transform behavior. The policy should let applications decide whether decoded image orientation metadata normalizes the supplied frame before viewport placement.

The viewport should allow applications to define how transparent pixels are visually backed. Supported presentation should include a solid background color and a checkerboard-style background for inspecting alpha content.

The viewport should define a color handling policy comparable in scope to Qt Quick image rendering rather than promising full viewer-grade color management by default. Applications should be able to provide or preserve source color space metadata, while initial display behavior may assume frames are already display-ready sRGB content. Full embedded-profile conversion, display-profile-aware rendering, wide-gamut handling, or HDR tone mapping are not baseline display guarantees unless they are added as explicit rendering capabilities.

Color and orientation policies should distinguish strict normalization from best-effort presentation and metadata preservation. When strict normalization is requested, the viewport should not silently display content that failed the requested conversion or orientation policy. Best-effort presentation may display the closest supported result with a warning, and metadata-preservation policy may carry source metadata forward without promising viewer-grade conversion.

Strict color handling should not imply support for viewer-grade color management, HDR, tone mapping, or gain-map processing. If strict color behavior is requested outside the viewport's supported conversion scope, the request should be reported as unsupported rather than approximated as successful display.

## Viewport Behavior

`ImageViewport` should expose the relationship between image coordinates and item coordinates as viewport state. The exact API surface is intentionally unspecified for now, but callers should be able to observe or control zoom and pan as required viewport concepts.

The viewport should clip rendered image content to the item bounds by default. Placement modes, zoom, pan, and crop behavior should not cause image pixels to paint outside the viewport unless a future API explicitly opts into that behavior. This is an internal image-content clip and should not imply Qt `Item::clip` behavior for child items. The inherited `clip` property remains the normal Qt Quick item clipping mechanism.

The viewport should provide coordinate conversion between item coordinates and image coordinates. This is required for user interactions such as pixel inspection, cropping, annotation, and hit testing against image content.

Coordinate conversion should make non-image areas distinguishable from valid image coordinates. Points in fit padding, outside the transformed image, outside the clipped viewport, or inside an empty viewport should not be reported as ordinary image pixels.

The viewport should define image coordinates in logical image space after source metadata normalization. Device-pixel-ratio handling should not leak into QML item coordinates or make coordinate conversion depend on physical render pixels.

When a metadata-preserving policy keeps orientation metadata without applying it to pixels, image coordinates should follow the displayed pixel geometry. The viewport should not report coordinates in a normalized orientation that was not actually used for display.

The viewport should expose viewport state but should not require built-in pointer, wheel, drag, or pinch gesture handling. Gesture policy remains separate so applications can integrate the viewport with their own input handlers.

## Local Mapping Order

`ImageViewport` should define its local image-to-item mapping in an order compatible with the mental model of Qt Quick `Image`.

The supplied frame should first be normalized according to source-level metadata policy, including EXIF-style orientation when that policy is enabled.

The viewport should then identify the source-space content represented by the supplied frame.

The viewport should compute the base image-to-item mapping from placement policy, aspect fit/crop/stretch/pad behavior, and alignment.

Horizontal and vertical mirroring should flip the selected content inside the chosen displayed rectangle. Mirroring should not change the displayed content rectangle, item geometry, or alignment result.

Viewport-specific state such as zoom and pan should then be composed into that image-to-item mapping.

Sampling policy, including `smooth` and mipmap filtering, should affect rendering quality after the local mapping has been chosen. Sampling policy should not alter image layout.

Inherited `QQuickItem` transforms such as item `rotation`, item `scale`, and the item `transform` list should remain outside the viewport's local image-to-item mapping unless an API explicitly asks for scene or window coordinates.

Coordinate conversion between image coordinates and local item coordinates should reflect the final local geometry mapping through mirroring and viewport-specific transforms. Sampling policy should not alter coordinate conversion, and local conversion should not silently include parent or scene transforms.

## State Behavior

`ImageViewport` should expose enough state for QML code to distinguish an empty viewport, a viewport with display-committed content, a viewport waiting for requested content, and a viewport that cannot display the requested content.

The state model should distinguish the content currently displayed from the most recent content request or replacement attempt. This allows QML code to reason about cases where a replacement is loading or has failed while the viewport still displays a previously valid frame.

Request status semantics should remain recognizable to Qt Quick users: empty content, ready content, loading or waiting content, unsupported content, and error content should map cleanly to the mental model of `Null`, `Ready`, `Loading`, and `Error` while making unsupported requests distinguishable when useful.

For public request status, ready content should mean the latest request is reflected in display-committed content or required no display change. Provider or CPU candidate readiness before the frame becomes display-committed should remain an internal implementation detail and should not be reported as public ready state.

Unsupported request status should be distinct from error status. Unsupported means the requested seek, access pattern, output policy, or normalization policy is outside the active sequence capabilities. Error means an operation that should have been possible failed.

Waiting or loading status should expose a reason sufficient to distinguish provider waiting from render-deferred waiting. Render-deferred waiting, such as a temporarily unavailable scene graph context, should not be presented as provider latency or decode failure.

Display status should be observable separately from request status. A failed replacement request should not force QML code to infer whether the viewport is visually empty or still rendering retained content.

The displayed frame value should be valid only when display status indicates retained or committed content. When no frame is displayable, the displayed frame should use a defined invalid value such as `-1`.

When retained content belongs to an older sequence generation, QML should be able to distinguish that fact. Displayed frame indexes and content geometry should not be assumed to describe the currently assigned sequence unless the viewport reports that displayed content belongs to the current sequence.

When an error occurs, the viewport should expose a human-readable error description suitable for diagnostics or user-facing fallback UI.

The display-committed frame should remain immutable from the QML user's perspective until the viewport advances to or commits another presentable frame. Callers should not observe transient clearing between frames during normal playback or content refresh.

When displayed content is replaced or refreshed asynchronously, the viewport should be able to retain the previous displayable frame until replacement content is ready. This avoids visible clearing or flicker during slow frame or image transitions.

If replacement content fails, the viewport should expose the failed request state and retain the previous displayable frame by default. While the error describes the failed replacement, displayed content geometry, coordinate conversion, frame metadata, and rendered output should continue to describe the retained display-committed frame until the caller clears or replaces it.

## Open Surface

The initial scaffold does not yet define the concrete image input API, provider protocol, buffering policy, or exact color-management pipeline.

Tiled presentation modes are not part of the required placement behavior yet. They remain open until a concrete viewport use case requires repeating image content horizontally, vertically, or in both directions.

Large-image region and level-of-detail supply remains open. The viewport should keep large-image scenarios in mind, but a region/LOD provider protocol is not part of the required behavior until the source model is designed.

Arbitrary transform control beyond the required zoom and pan concepts remains open.

Explicit content rotation remains open as a future presentation transform. Future behavior should define API naming, angle granularity, rotation center, transform composition order, and how rotated content affects reported bounds.

Implicit size behavior remains open. A future decision should define whether the item uses the normalized logical image size of the current sequence, a fixed empty size, or another QML layout policy when width and height are not explicitly set.
