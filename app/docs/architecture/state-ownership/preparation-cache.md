# Preparation And Cache

## Thumbnail Preparation

The document session owns the QML-facing thumbnail strip derived from the same confirmed candidate snapshot as active navigation. Thumbnail activation is a session navigation command using projected row identity; QML must not reconstruct or open source URLs from model data.

The thumbnail runtime owns source adaptation, demand, scheduling, async lifecycle, stale rejection, result publication, and image-store lifetime. QML reports visible demand and renders projected results. Detailed source and cache boundaries are defined in [Thumbnail Source Adapters](../thumbnail-source-adapters.md).

## Still-Image Preparation

The preparation runtimes own prepared image objects, decode jobs, reusable provider payload entries, byte pressure, recency, lifetime, preparation-window policy, priority, and eviction decisions. Preparation policy consumes snapshots and must not acquire a second live image or async-job state.

Preparation windows govern new work and priority, not immediate cache destruction. Current and recently displayed images, the active preparation window, and warm same-scope entries are retained in that order while the byte budget permits. Scope replacement and explicit clear invalidate scope-owned warm entries; selection changes inside the same scope reprioritize rather than clear compatible work.

The document-session preparation runtime owns still-image preparation for mixed direct-media scopes. The image-document preparation runtime owns it for image-page and opened-collection scopes. Each consumes the confirmed candidate view of its owning navigation scope together with accepted selection, presentation, power-saver, resource-limit, and execution facts without acquiring navigation or system-state ownership.

Video rows may influence the position of adjacent still-image work but never produce cached video frames. Preparation results return provider-eligible still-image payloads to the owning load and supported `ImageSequence` provider boundary and must not publish navigation, viewport readiness, errors, zoom, or page selection.

Pending debounce, suspended Power Saver work, and in-flight completion retain their candidate-snapshot identity. Resume or completion is accepted only if the owning scope and generation remain current. Power Saver suppresses new background work without blocking foreground loads or visible-detail refinement.

## Decoder-Internal Tiling

Source-internal HEIF tiling is allowed only inside a decode or refinement job that produces a payload accepted by the supported provider contract. Decoder-internal tiling must not escape the decoder as an application-owned image-presentation path or become an independent presentation cache.
