# ImageViewport API Draft

This document records the initial API surface direction for `ImageViewport`. It is a draft design surface, not a final ABI or implementation commitment.

The API should make `ImageViewport` feel like a Qt Quick image item while keeping its input model centered on caller-supplied image sequences rather than URLs. It is a sequence consumer, not a source URL loader.

## Input Model

The primary content input should be a sequence object or provider, exposed conceptually as `sequence`.

```qml
ImageViewport {
    sequence: mySequence
}
```

`ImageViewport` should not expose `source: url`. URL resolution, filesystem access, archive lookup, network loading, and direct integration with storage systems should remain caller or provider responsibilities.

The core model should remain suitable for archive entries, container-backed images, custom storage, application-owned decoders, and formats that are not registered as Qt image plugins.

Still images should be represented as single-frame sequences. Animated images should be represented as timed multi-frame sequences.

The API should still provide a practical way to create sequence objects. `source: url` should remain absent from `ImageViewport`, but the module or companion types should offer simple construction paths for at least an in-memory still image, an explicit frame list, and an application-provided sequence provider. File, URL, archive, or byte-buffer helpers may exist outside the viewport item as provider factories rather than as `ImageViewport` loading properties.

## QML Item Surface

The QML-facing item should expose playback, presentation, viewport mapping, status, and coordinate conversion as first-class concepts.

```qml
ImageViewport {
    sequence: mySequence

    playbackState: ImageViewport.Playing
    requestedFrame: 0
    speed: 1.0
    loopCount: ImageViewport.Infinite

    fillMode: ImageViewport.PreserveAspectFit
    horizontalAlignment: ImageViewport.AlignHCenter
    verticalAlignment: ImageViewport.AlignVCenter

    zoom: 1.0
    pan: Qt.point(0, 0)

    smooth: true
    mipmap: false
    mirror: false
    mirrorVertically: false
}
```

The initial QML item surface should include the following concepts.

```qml
property ImageSequence sequence

readonly property bool frameCountKnown
readonly property int frameCount
property int requestedFrame
readonly property int displayedFrame
property PlaybackState playbackState
property real speed
property int loopCount
readonly property int completedLoops

readonly property RequestStatus requestStatus
readonly property RequestStatusReason requestStatusReason
readonly property DisplayStatus displayStatus
readonly property bool hasDisplayableFrame
readonly property bool displayedBelongsToCurrentSequence
readonly property string errorString
readonly property string warningString
property bool retainWhileLoading

property FillMode fillMode
property Alignment horizontalAlignment
property Alignment verticalAlignment
readonly property rect contentRect
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
function seek(frame)

function itemToImage(point)
function imageToItem(point)
function containsImagePoint(point)
```

Concrete type names may change, but the observable concepts should remain stable unless later research invalidates them.

## Playback State

`ImageViewport` should be a sequence consumer with its own playback state. Callers provide sequence content and timing; the viewport selects requested frame targets according to `playbackState`, `requestedFrame`, `speed`, and `loopCount`.

The playback phase should be mutually exclusive. The expected phases are stopped, playing, paused, and ended.

`requestedFrame` should represent the selected or requested frame target from the caller's perspective. `displayedFrame` should represent the frame that has become display-committed or is retained as previously committed content. During pending seek, waiting, replacement, or upload failure, these values may differ.

Assigning `requestedFrame` and calling `seek(frame)` should have the same frame-selection semantics. `seek(frame)` is the command-style convenience form for QML code that prefers an imperative action.

Manual frame inspection should use paused playback plus `requestedFrame`; the API should not require a separate frame-sink mode where QML repeatedly pushes the currently displayed frame.

`frameCountKnown` should make unknown frame counts explicit. `frameCount` should only be interpreted as an exact total when `frameCountKnown` is true, so QML callers do not need to rely on sentinel values such as `-1`.

`displayedFrame` should be interpreted only when `hasDisplayableFrame` is true. When `hasDisplayableFrame` is false, `displayedFrame` should have a defined invalid value such as `-1` and should not be used for image-coordinate decisions.

`displayedBelongsToCurrentSequence` should indicate whether `displayedFrame` and display geometry describe the currently assigned sequence generation. It may be false when a replacement sequence is loading or failed while retained content from a previous sequence remains visible.

`loopCount` should support finite loop counts and an infinite value. `completedLoops` should expose progress through finite or repeated playback without forcing applications to infer it from `requestedFrame`.

## Status State

The status model should distinguish the latest content request from the content currently display-committed.

`requestStatus` should describe the most recent content request or replacement attempt in terms recognizable to Qt Quick users: null or empty, loading or waiting, ready, unsupported, and error. Public `Ready` should mean the requested content has become display-committed or that the latest request did not need to change the already display-committed content. Internal candidate-frame readiness before the frame becomes display-committed should not be exposed as public `Ready`.

`requestStatusReason` should distinguish why a non-ready request is not yet display-committed. Expected reasons include provider waiting, render deferred because no scene graph context is available, upload pending, unsupported request, and failure. `Unsupported` should remain distinguishable from `Error`; it means the requested operation or policy is not supported, not that an expected operation failed.

`displayStatus` should describe whether the viewport currently has no display-committed content, has renderable retained content, or cannot present any content.

`hasDisplayableFrame` should describe whether the viewport currently has retained display-committed content. This is intentionally separate from `requestStatus` because replacement failure may produce `requestStatus == Error` while the viewport still renders a previous valid frame.

`errorString` should describe the failed request or display problem without implying that retained content has been cleared.

`warningString` or an equivalent diagnostics surface should describe recoverable best-effort presentation differences, such as a color or orientation policy that could only be approximated.

`retainWhileLoading` should allow asynchronous replacement to keep the previously displayed frame until the replacement becomes displayable. Replacement failure should retain the previous displayable frame by default unless the caller clears content explicitly.

## Presentation State

`fillMode` should support stretch, aspect-preserving fit, aspect-preserving crop, and unscaled padding. Tiling remains outside the initial API.

Alignment should apply when the displayed content does not exactly match the item bounds.

`contentRect` should expose the visible image content rectangle in local item coordinates. It should reflect placement, alignment, mirroring, zoom, pan, and internal viewport clipping, but not inherited scene transforms or child item bounds.

`paintedWidth` and `paintedHeight` may remain available for familiarity, but `contentRect` is the more useful viewport concept.

`smooth == true` should request smooth sampling. `smooth == false` should request nearest-neighbor sampling.

`mipmap`, `mirror`, and `mirrorVertically` should map to the presentation behavior already defined in the main `ImageViewport` behavior specification.

## Viewport Mapping

`zoom` and `pan` should be part of the QML-visible viewport state. The initial API should not include built-in pointer, wheel, drag, or pinch gesture policy.

Coordinate conversion should include item-to-image and image-to-item mapping. Non-image areas should be distinguishable from valid image coordinates.

The coordinate API should be explicitly local-item based. Inherited `QQuickItem` transforms should not be silently included unless a later API explicitly asks for scene or window coordinates.

Coordinate conversion should follow the actual normalized frame geometry used for display. If orientation metadata is preserved but not applied, image coordinates should describe the displayed pixel geometry rather than pretending that metadata normalization occurred.

## Provider Surface

The provider surface should supply display frames rather than codec-native frame fragments.

The provider should expose sequence-level information before every frame is decoded when possible.

```text
SequenceInfo
- logicalSize
- frameCount: known(count) | unknown
- duration: known(duration) | unknown
- loopCount: finite(count) | infinite | unknown
- kind: still | animation | progressive | track
- capabilities: randomAccess | sequential | statefulSequential | singleFlight | rewindable | streaming | prefetchable
- metadata: orientation, color, alpha, background, format-specific diagnostics
- derivedHints: dependency model, key-frame-like boundaries, composition/coalescing state
```

Random access should be an optional capability, not a baseline provider requirement. Sequential providers should be valid when they can deliver immutable full-frame display snapshots in playback order, even if arbitrary frame-index requests are unsupported or require a rewind.

Sequence capability information should be observable enough for QML to choose interaction affordances. Callers should be able to determine whether random seek is expected to work, whether seek may wait or rewind, and whether display-critical requests are single-flight.

Each delivered frame should be self-contained from the viewport's perspective. A display frame should be a canvas-equivalent snapshot that can be presented without the viewport applying codec disposal, blending, reference-frame, or dependency rules.

```text
DisplayFrame
- index
- canvasSize
- logicalRect: optional descriptive or dirty/content region
- presentationTime: optional
- duration: optional
- pixels or texture-compatible payload
- normalization: orientation/color/alpha state
- dirtyRegion: optional
- dependency: independent | providerManagedHint
- sourceGeneration or sequence token
```

The provider should accept frame requests by display intent, not by codec mechanism.

```text
FrameRequest
- index or playback-position request
- desired normalization policy
- output format preference
- cancellation token
```

For stateful sequential providers, display-critical frame requests may be constrained to one active request at a time. QML-visible behavior should still expose a clear waiting, unsupported, or error outcome rather than pretending that an unsupported seek target is already displayed.

Preparation should be advisory.

```text
PrepareHint
- current index
- playback direction
- nearby frame window
- memory budget class
```

## Provider Responsibility

Codec-native composition concepts belong to the provider or decoder adapter. Blending, disposal, reference frames, palettes, delta frames, and zero-duration layers decide how a display frame is produced; they should not become QML-facing viewport rules.

`ImageViewport` may use provider-derived metadata such as dirty regions, dependency information, independent-decode capability, key-frame-like boundaries, and coalescing state for cache, prefetch, upload, and seek strategy.

The provider should define whether delivered frames are already normalized according to the requested orientation and color policy.

The requested orientation and color policy should carry a compliance level. A strict policy requires the provider or viewport preparation path to satisfy the requested normalization or report `Unsupported` or `Error` before display. A best-effort policy may return the closest supported display-ready result with diagnostics. A metadata-preserving policy may keep source metadata available while treating the pixel payload as already suitable for baseline display.

## Deferred API

The following surfaces should remain outside the initial API until there is a concrete need and implementation strategy.

- Native texture import or caller-owned `QSGTexture` exchange.
- Region or level-of-detail provider protocol for very large images.
- Full embedded-profile conversion, display-profile-aware rendering, wide-gamut handling, or HDR tone mapping.
- Arbitrary content rotation beyond metadata orientation policy.
- Detailed cache policy controls beyond advisory preparation hints.
