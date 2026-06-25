---
title: Sequence image library models
researched_on: 2026-06-21
---

# Sequence Image Library Models

This note compares how `libwebp`, `libavif`, `libheif`, `libjxl`, and `giflib` expose animated or sequence image data, with emphasis on the provider abstraction needed by `ImageViewport`.

## Sources

- libwebp documentation: [Container API](https://developers.google.com/speed/webp/docs/container-api)
- libwebp source: [`src/webp/demux.h` at `3757b8afeb54e305eaef18502812a9a88b7ed662`](https://github.com/webmproject/libwebp/blob/3757b8afeb54e305eaef18502812a9a88b7ed662/src/webp/demux.h)
- libavif source: [`include/avif/avif.h` at `3f4212ffcfaf0eba9897781672d8e65bbb98cec3`](https://github.com/AOMediaCodec/libavif/blob/3f4212ffcfaf0eba9897781672d8e65bbb98cec3/include/avif/avif.h)
- libavif example documentation: [`README.md` advanced decoding example at `7d36984b2994210bde08c6f9f990c4f72bdcdcbf`](https://aomedia.googlesource.com/libavif/+/7d36984b2994210bde08c6f9f990c4f72bdcdcbf/)
- libheif documentation: [Reading and Writing Sequences](https://github.com/strukturag/libheif/wiki/Reading-and-Writing-Sequences)
- libheif source: [`libheif/api/libheif/heif_sequences.h` at `438ecc34cfee08b6f26fae74afb59b7c21acc191`](https://github.com/strukturag/libheif/blob/438ecc34cfee08b6f26fae74afb59b7c21acc191/libheif/api/libheif/heif_sequences.h)
- libjxl documentation: [Decoder API](https://libjxl.readthedocs.io/en/latest/api_decoder.html)
- libjxl documentation: [Image and frame metadata](https://libjxl.readthedocs.io/en/latest/api_metadata.html)
- libjxl source: [`lib/include/jxl/codestream_header.h` at `7a208214e1c084fcd73adf6957dafaf69612e025`](https://github.com/libjxl/libjxl/blob/7a208214e1c084fcd73adf6957dafaf69612e025/lib/include/jxl/codestream_header.h)
- giflib documentation: [The GIFLIB Library](https://giflib.sourceforge.net/gif_lib.html)
- giflib source: [`gif_lib.h` at `a8e3114a81f0987a61d06a41c99fd7cc2d58232c`](https://sourceforge.net/p/giflib/code/ci/a8e3114a81f0987a61d06a41c99fd7cc2d58232c/tree/gif_lib.h)

## Comparison

| Library | Sequence discovery | Frame access | Timing model | Output model | Notes for abstraction |
| --- | --- | --- | --- | --- | --- |
| libwebp | Demuxer exposes canvas metadata, loop count, background color, and frame count. | Demux API can access frame fragments by number and move next/previous; AnimDecoder decodes sequentially and can reset. | Demux frames carry per-frame duration in milliseconds; AnimDecoder reports timestamps. | Demux exposes compressed fragments with offsets, blend, disposal, alpha, and duration; AnimDecoder yields decoder-owned rendered buffers. | A provider can either expose coalesced display frames from AnimDecoder or implement its own composition from demux fragments. |
| libavif | Decoder parse exposes image count, sequence duration, timescale, repetition count, progressive state, and alpha presence. | Advanced decoder supports next-image and nth-image decoding for sequences or progressive layers. | Current image timing has presentation timestamp and duration; sequence duration and repetition count are separate. | Decoded `avifImage` is YUV/A plus metadata; RGB conversion is an explicit post-decode step. | Provider should distinguish animated sequence frames from progressive image layers and should own conversion into the viewport's display-ready frame format. |
| libheif | Context can report whether a sequence exists, list sequence tracks, and expose visual, video, auxiliary, or metadata tracks. | Visual sequence API decodes the next image in temporal order from a track. | Sequence and track timescales may differ; decoded image duration is expressed in the track timescale. | Decoded `heif_image` can be requested in a chosen colorspace/chroma or left for libheif to decide. | Provider should treat HEIF sequence input as a timed track, not as a simple array of independent still images. |
| libjxl | Basic info reports animation presence, canvas size, orientation, color metadata, and animation header data. | Decoder is event-driven; frame header is available at frame-start events and pixels at full-image events. | Animation header defines ticks-per-second and loop count; each displayed frame header carries duration in ticks and optional timecode. | Decoder normally coalesces internal layers into displayed frames; disabling coalescing exposes non-coalesced regular layers. | Provider should consume displayed frames by default and hide zero-duration layers, reference frames, and blending unless a specialized inspection mode is added. |
| giflib | High-level `DGifSlurp()` loads the complete multi-image GIF into `ImageCount` and `SavedImages`. | After slurp, frames are array entries; lower-level sequential APIs also exist but are legacy-oriented. | GIF89 graphics control block stores delay in 1/100 second units per saved image. | Saved images are palette-indexed subimages with extension blocks; graphics control block provides disposal and transparency. | Provider must composite frames against the logical screen using disposal, transparency, local/global palettes, and loop extension parsing before handing frames to ImageViewport. |

## Library Findings

libwebp has two useful views of animated content. The demux layer exposes frame metadata and compressed frame fragments, including canvas offsets, dimensions, duration, disposal, blending, alpha, completeness, and total frame count. The animation decoder layer instead provides sequential rendered buffers plus timestamps, with reset support for replay. This split is useful: demux is metadata-rich and seek-friendly, while AnimDecoder is closer to display-frame playback.

libavif treats AVIF image sequences through its decoder state. After parsing, the decoder exposes image count, sequence duration, timescale, repetition count, progressive state, alpha presence, current image index, and current image timing. Its decoded image model is not immediately a Qt-friendly RGB bitmap; applications often need an explicit YUV/A-to-RGB conversion and must preserve color metadata, transforms, alpha, and gain-map or HDR-related data according to policy.

libheif sequence support is track-oriented. The sequence API starts by checking for a sequence, enumerating tracks, selecting a track, checking the handler type, then decoding the next visual image in temporal order. Timing is expressed in track timescale units, and track timescale can differ from sequence timescale. This model looks closer to a media track reader than to an indexed still-image container.

libjxl exposes animation through a streaming event decoder. The basic info and animation header define global properties such as orientation, color information, ticks-per-second, and loop count. Frame events expose displayed frame headers, including duration, timecode, last-frame marker, and layer information. By default the decoder coalesces internal layers and produces displayed frames, which is the behavior most useful for a viewport.

giflib exposes GIF as a logical screen plus saved images and extension blocks. `DGifSlurp()` loads the full multi-image file into memory so the application can inspect `SavedImages`, `ImageCount`, raster bits, palettes, and per-frame extension blocks. The graphics control block gives delay, disposal mode, user-input flag, and transparent color, but giflib does not turn those pieces into final RGBA display frames by itself.

## Cross-Library Design Implications

The common provider abstraction should be based on display frames, not codec-native frame fragments. Several formats can encode a frame as a subrectangle, delta, layer, or reference-dependent update. `ImageViewport` should not need to implement WebP blend/dispose rules, GIF disposal rules, JPEG XL reference frames, or HEIF track internals.

Codec-native composition concepts should belong to the provider or decoder adapter. Blending, disposal, reference frames, palettes, delta frames, and zero-duration layers decide how a display frame is produced; the viewport should consume the resulting display frame. This keeps `ImageViewport` from becoming a format-specific animation compositor.

`ImageViewport` can still benefit from derived provider metadata. A provider may expose dirty regions, dependency information, independent-decode capability, key-frame-like boundaries, or whether a frame was already coalesced. The viewport can use those facts for cache, prefetch, upload, and seek strategy without owning the underlying format semantics.

The provider should expose sequence-level metadata separately from frame data: logical canvas size, frame count when known, duration/timescale basis, loop count or unknown/infinite loop, alpha presence, orientation metadata, color metadata, background color when meaningful, and whether the stream is animated, still, progressive, or track-like.

Frame timing should support both duration and presentation timestamp. Some libraries expose durations directly, some expose timestamps, and some expose durations in arbitrary timescale units. A normalized provider result should carry enough information for the playback controller to compute stable scheduling without assuming fixed FPS.

Frame access capability should be explicit. A source may support random access by index, sequential next-frame only, rewind/reset, or partial metadata discovery while parsing. Prefetch requests from `ImageViewport` must therefore be hints that providers may satisfy, degrade, or ignore.

Frame count should not be assumed to be cheaply known at construction time. It is known after full demux in some libraries, known after parse in others, and may be incomplete during streaming or partial demux. QML-visible `frameCount` should allow an unknown state.

Decode results need an ownership and lifetime boundary. Some APIs return decoder-owned buffers, some return library-owned images, and some expose application-managed output buffers. The provider interface should hand ImageViewport an owned or ref-counted frame representation whose lifetime is independent of the next decoder call.

Color and orientation should be provider-normalization policy, not a hidden side effect. AVIF, HEIF, and JPEG XL can carry explicit color and transform metadata; GIF and WebP have simpler but still relevant alpha, palette, background, ICC, EXIF, and XMP data. The provider should state whether delivered frames are already normalized to ImageViewport's requested display frame policy.

Progressive image layers should not be treated as animation frames by default. libavif and libjxl both have concepts that can yield multiple image-like outputs before the final still image, but those are not necessarily playback frames. `ImageViewport` can support progressive loading later, but sequence playback should remain distinct from progressive refinement.

## Suggested Provider Shape

The provider contract should be able to describe a sequence before decoding every frame.

```text
SequenceInfo
- logicalSize
- frameCount: known(count) | unknown
- duration: known(milliseconds) | unknown
- loopBehavior: finite(totalPlaythroughs) | infinite | none | unknown
- kind: still | animation | progressive | track
- capabilities: stableDisplayIndexes | randomAccess | sequential | rewindable | streaming | prefetchable
- metadata: orientation, color, alpha, background, format-specific diagnostics
- derivedHints: dependency model, key-frame-like boundaries, composition/coalescing state
```

Each decoded display frame should be self-contained from the viewport's perspective.

```text
DisplayFrame
- index
- logicalRect or canvasSize
- presentationTime: optional milliseconds
- duration: optional milliseconds
- pixels or texture-compatible payload
- normalization: orientation/color/alpha state
- dirtyRegion: optional
- decodeDependencyHint: independent | providerManaged | keyFrameLike | deltaLike
- sourceGeneration or sequence token
```

The viewport should request frames by intent rather than by codec mechanism.

```text
FrameRequest
- index or playback-position request
- desired normalization policy
- output format preference
- cancellation token
```

Preparation should remain advisory.

```text
PrepareHint
- current index
- playback direction
- nearby frame window
- memory budget class
```

This shape keeps the QML-facing playback model stable while allowing adapters for simple indexed formats, sequential animated decoders, media-track-like HEIF sequences, streaming JPEG XL decoders, and GIF/WebP compositors.
