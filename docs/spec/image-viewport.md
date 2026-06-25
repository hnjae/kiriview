# ImageViewport Behavior

`ImageViewport` is a Qt Quick item for displaying caller-supplied image sequences. It consumes an `ImageSequence` object, presents the selected frame inside its item bounds, exposes display and request state to QML, and leaves source lookup or decoding policy to sequence factories and provider adapters.

## Sequence Assignment

The viewport should behave as a sequence consumer, not as a URL loader. It should not interpret strings, file paths, URLs, archives, byte buffers, JavaScript objects, or raw provider objects as image sources through the `sequence` property.

Assigning a valid non-null sequence starts a new display generation. Assigning `null` or calling `clear()` clears the active generation, stops playback, and removes display content unless retained content is explicitly part of a failed replacement state.

Replacing a sequence should keep the previous display visible until the replacement can display content or reaches a terminal unsupported/error state. Retention is a visual fallback; it must not make the new request appear ready or inherit the previous sequence's capabilities.

## Display Geometry

The item should support common image placement modes such as contain, cover, stretch, and center. Placement is computed from the item bounds, image logical size, alignment, mirroring, zoom, and pan.

The viewport should expose geometry observations that let callers position overlays and inspect pixels without duplicating placement logic. At minimum, callers should be able to observe the displayed content rectangle, the visible image rectangle, the displayed image size, and conversions between item coordinates and image coordinates.

If the item has non-positive width or height, no new frame can be presented. Existing committed content may remain the current display identity, but coordinate conversion and visible-image containment should report no presentable image area.

## Requests And Status

The viewport should distinguish the latest accepted display request from the currently committed display content. Loading, ready, unsupported, and error states describe the accepted request rather than the mere presence of retained pixels.

Invalid or unsupported commands should not erase the previous accepted request. A failed seek should report the command outcome and diagnostic state while preserving the last accepted requested target and any pending request that is still meaningful.

Public status values should be stable enough for QML controls to branch on. Diagnostic strings are presentation text and should be redacted, bounded, and plain text.

## Playback

Timed sequences should support play, pause, stop, frame seeking, and position seeking when the active sequence reports the required capability. Playback should advance from sequence timing metadata rather than wall-clock sleeps in tests or application-visible semantics.

Looping and end-of-sequence behavior should be explicit. Reaching the end should not silently turn into an error, and unsupported seeks should not change playback phase or loop counters.

## Presentation Controls

Presentation properties such as smoothing, mipmap request, mirror flags, background mode, background color, zoom, pan, fill mode, and alignment are viewport state. Sequence assignment and clearing should not reset viewport transforms unless the caller explicitly requests a reset.

Transparent content should be inspectable through a deterministic background mode. A checkerboard background should affect only presentation behind the image and must not change image geometry, request state, or coordinate conversion.

## Non-Goals

The viewport does not own file discovery, network access, archive extraction, decoder selection, temporary files, permissions, or sandbox policy. Those concerns belong to explicit sequence factories, application code, or provider adapters that return `ImageSequence` objects.
