# Provider Rendering Architecture

This document defines the intended image-display boundary. User-visible image behavior remains in `../spec/image-display.md`.

## Rendering Contract

Still images and accepted animation frames are displayed through high-level Qt Quick image items backed by immutable, provider-published whole-image entries. Production display must not use custom render nodes, visual tile surfaces, shaders, or application-owned low-level rendering resources.

Decoders and refinement jobs may use source-internal tiling only to assemble one bounded display image before publication. If one safe display image cannot represent the requested detail, the owner publishes bounded-detail, unsupported, or failed state rather than introducing a visual tile fallback.

## Ownership

- The image-presentation runtime owns presentation mode, page roles, visible geometry, source projection, quality, freshness revision, retention eligibility, and display-load acknowledgment state.
- Page-resource owners own the immutable entries, animation-frame handoff, pending-load retention, reusable-entry leases, and release associated with each page role. They do not own zoom, rotation, viewport, spread, or navigation state.
- The display image store owns immutable entry identity, byte accounting, priority, pinning, reuse lookup, eviction, and release. Entry ids are never reused during the runtime lifetime.
- Decode and refinement owners perform source access, decode, safe SVG rasterization, preview validation, metadata extraction, animation composition, and whole-image refinement behind async lifecycle boundaries.
- Qt Quick owns texture upload, filtering, item lifetime, and scene rendering. QML binds accepted projections and reports load outcomes; it does not choose display buckets, provider ids, cache retention, animation timing, or stale acceptance.

## Provider And Display-Source Lifecycle

Display providers are cache-only, cheap, and reentrant. A request may return an already published immutable entry and its stored size, but it must not decode, rasterize, perform I/O, resample, schedule work, install cache entries, decide freshness, or mutate public state.

Every accepted entry with different image bytes receives fresh provider identity. Reusing the same immutable bytes is allowed only through a store-owned reuse identity that proves compatible source, scope, transform, size, quality, role, and rendering constraints. QML engine caching is never the freshness authority.

A display-source projection may expose provider identity only after its page-resource owner has accepted and published the corresponding entry. Visible, pending-load, retained, and reusable leases are distinct so acknowledging one projection cannot release another entry.

Load outcomes carry page role, provider identity, projection revision, and source identity back to the presentation owner. Matching success commits visual acceptance and releases the matching pending lease. Matching failure either publishes display error or triggers explicit reconciliation for the still-current payload. Stale, wrong-role, or superseded outcomes are ignored.

Top-level source replacement, mode change, clear, and selected-target failure may clear the previous projection. Same-scope image navigation and same-source refinement may retain the last committed presentation until the matching replacement is ready. Retention must not make the retained media appear to be the new selected target.

## Preview, Refinement, And Reuse

An initial display may use a validated lower-detail preview or bounded first display. Preview acceptance must establish the correct source, orientation-aware intrinsic dimensions, aspect, freshness, and safe resource bounds. Preview output is never marked exact and is superseded by accepted sharper output for the same demand.

Refinement demand is derived from the accepted source, visible geometry, zoom, rotation, device pixel ratio, and resolved resource limits. Its freshness identity includes every fact that could make a completion unsuitable for the active presentation. A separate reuse identity contains only facts that prove two results have equivalent display bytes.

Refinement work is best-effort cancelable. Results may populate a bounded cache by reuse identity, but they may replace the visible entry only when the full active demand identity still matches. Duplicate in-flight work for the same reuse identity may be shared without sharing presentation authority.

Predecoded still images enter the same immutable-entry, identity, quality, resource-limit, and publication path as foreground decodes. Video rows may guide adjacent-image preparation but never create still-image display payloads.

## Animation And SVG

The animation owner controls reader lifetime, authored timing, loop progress, composition, and frame acceptance. Each accepted frame is published as an immutable display entry through the normal display-source path. Source replacement or animation stop invalidates later frame completions, and bounded previous-frame retention prevents a blank transition while the next accepted frame attaches.

Supported animated containers must be classified through animation-aware decoding before static fallback so a multi-frame file is not silently reduced to its first frame.

SVG parsing and rasterization must disable scripts, animation, and external network or file resources before publication. SVG output uses bounded whole-image buckets keyed by source and active display demand. Failed refinement retains the last accepted image; failed initial display follows the normal image-error path.

## Viewport Boundary

A non-rendering context boundary supplies attachment, scene, device-pixel-ratio, and safe display-limit facts to the presentation owner. It must not draw, own textures, use private rendering interfaces, or schedule decode work.

QML owns physical scrolling items, gesture samples, and high-level image item instances. C++ owns the logical viewport frame and revisioned commands. QML applies only the latest applicable command and reports physical observations or load outcomes using owner-issued opaque revisions; it must not calculate canonical zoom, pan, scan, display quality, or freshness state.
