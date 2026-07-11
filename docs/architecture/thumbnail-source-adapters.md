# Thumbnail Source Adapters

Active navigation thumbnails use source adapters to keep thumbnail scheduling and result projection independent from the kind of navigation row being displayed.

## Contract

The C++ document-session thumbnail runtime is a composition and delegation shell around separate row-resource and async-work owners. The demand-window owner contract is defined in [State Ownership](state-ownership.md#thumbnail-demand-window-boundary).

The document session composes the active-navigation thumbnail strip through one strip dependency port. The strip lookup provider, generation provider, source adapter, image store, and worker scheduler cross the session boundary as one active-navigation thumbnail dependency value rather than unrelated raw providers.

Adapters consume the active thumbnail source key and demand bucket defined by [Extension Contracts](extension-contracts.md#thumbnail-and-predecode-keys). They return plans: unsupported fallback, cacheable local-file generation, cacheable opened-collection entry generation, or in-memory-only generation. They must not mutate QML-facing model state, schedule jobs independently, install cache entries directly, or bypass runtime stale-completion identity.

Thumbnail cache lookup, generation requests, cache original identity, source-kind classification, and bucket sizing are neutral thumbnail infrastructure. Active-navigation row kinds are mapped into thumbnail source kinds before crossing the generation provider boundary, and decoding code imports thumbnail cache contracts from the thumbnail boundary rather than from `session/`.

The thumbnail generation core resolves source bytes or video frame extraction, bucket scaling, image decode/render, opened-collection cache identity, and cache lookup/install through injectable thumbnail dependencies before it publishes a generated thumbnail. Production dependencies may use local files, media-entry source metadata, media-entry source bytes, KiriView image decoding, Qt Multimedia video frame extraction, rendering refinement sources, and the XDG thumbnail cache. Tests and independent callers must be able to replace source loading, scaling policy, decoding, frame extraction, and cache repository behavior without constructing session runtimes, mutating global opened-collection state, borrowing playback state, or writing to the user's thumbnail cache.

## Runtime Owner Boundary

`ActiveNavigationThumbnailRowStore` owns row identity, thumbnail navigation generation, current-row projection, source-key indexes, model publication, result projection, and the acquisition, replacement, reprioritization, and release of `ThumbnailImageStore` entries.

`ActiveNavigationThumbnailScheduler` owns the immutable scheduling row snapshot and source-key indexes, source adapter, committed demand snapshot epoch, current-row input, accepted demand, priority tier, completed background buckets, stable work ids, row claims, and foreground, nearby, and background admission. It returns ordered value effects and has no access to QObject state, the row store, image store, model, or providers. A work id is allocated once for an admitted cache lookup or generation request and remains unchanged when a cache miss advances that work into generation.

`ActiveNavigationThumbnailWorkCoordinator` interprets scheduler effects in order, delegates starts and cancellation to the job executor, publishes accepted completions through the row port, installs ready images, and records failure diagnostics. It does not retain an independent demand window or work claim state.

`ActiveNavigationThumbnailJobExecutor` owns cache-lookup and generation providers, backend phase tokens, cancelable backend jobs, the cache-miss-to-generation chain, phase-level stale-completion rejection, and callback lifetime safety. It accepts plain work requests and returns plain typed completions carrying the original work id, source key, bucket, and work kind. It does not access row storage, model publication, the image store, retention policy, demand windows, or failure diagnostics.

The work coordinator reaches row-owned state only through `ActiveNavigationThumbnailRowPort`. The port groups pending, unsupported, and failed result application, ready-image installation, usable-ready-image checks, and retention-priority updates. Scheduling lookup and source-key snapshot access do not cross this mutation port, and the port does not expose mutable row storage, model publication, image-store ids, or image-store operations.

A current-row-only change preserves thumbnail navigation generation, accepted results, and provider entries. When row identity changes, the runtime first invalidates coordinator work and demand identity, then lets the row store release provider entries, advance thumbnail navigation generation, replace rows, rebuild source-key indexes, and publish the new model snapshot.

Every executor completion is accepted by the scheduler only after matching the active work id, source key including thumbnail navigation generation, demand bucket, and work kind. The executor separately accepts provider callbacks only when their private backend phase token still matches the active work phase. Only a completion accepted at both boundaries may be published by the coordinator through the row port. A background completion does not replace an existing usable foreground-ready image, and a failed larger foreground request preserves an existing usable ready image.

Runtime shutdown invalidates demand identity and asks the executor to cancel active work before row-store destruction releases image entries. Executor callbacks use weak lifetime state, so destruction or cancellation leaves forced late provider callbacks unable to reach the coordinator. When a provider completes synchronously, the executor installs its phase record before invoking the provider and retains the returned job only if that phase remains active; otherwise it cancels the already-stale returned job. The document-session dependency value, QML model roles, session command APIs, and image-provider boundary remain outside this internal owner split.

## Provider Publication

The active-navigation thumbnail image provider follows the cache-only provider contract in [Extension Contracts](extension-contracts.md#display-provider-contracts). It may look up a runtime-published image-store entry by id and report that entry's stored size. It returns the stored raster as published and performs no decode, resampling, generation, cache lookup, scheduling, freshness decision, or active-navigation mutation from a provider request.

Thumbnail bucket ownership ends before image-provider publication. QML reports visible demand and renders the projected provider result in item geometry, but accepted demand bucket selection, cache lookup, generated-image scaling, stale-completion rejection, and image-store insertion are owned by the thumbnail runtime and generation boundary before a provider id is exposed.

## Scheduling

The thumbnail runtime consumes each QML demand window as one immutable snapshot value defined by [State Ownership](state-ownership.md#thumbnail-demand-window-boundary). Source adapters receive only the runtime-selected source key, demand bucket, and priority for the row being scheduled; they must not inspect viewport geometry, retain demand epochs, or decide whether stale demand remains active.

A matching-generation demand snapshot atomically replaces committed demand. The runtime validates the complete snapshot before invoking source adapters or mutating scheduler state; a malformed entry, mixed generation, or unknown row rejects the complete snapshot and preserves the last committed demand. Duplicate facts for one row are reduced deterministically to the largest bucket and visible priority before source planning. Current-row and visible work are admitted together and may run in parallel. Nearby work may run in parallel only after all current-row and visible work is neither pending nor active. Background fill remains single-work and may run only after all demand work is neither pending nor active. Newly committed higher-tier demand cancels active lower-tier or background work before it is admitted.

A higher-priority demand for the same source key and bucket promotes retention and its existing claim without changing the work id or thumbnail navigation generation. A lower-priority or expired demand must not demote already visible/current demand until a newer demand snapshot says that row is no longer visible/current. A current row with an already committed bucket is foreground even when its QML geometry fact is nearby; a current row without a bucket waits for the next demand snapshot rather than inventing a size.

Background fill may run only when no visible, current-row, or nearby work is pending or active. It may use a bounded cursor over eligible placeholder rows and bucket levels so idle filling can continue over time, but it must yield immediately to newer foreground demand and must not synchronously traverse the full active-navigation row list after each demand report, completion, or current-row update.

## Original Identity

Thumbnail cache identity is separate from row source identity. Local files use Freedesktop file-original identity derived from the local path. Cacheable non-file originals use an explicit virtual original identity with URI, mtime, byte size, and optional MIME type supplied by the owning adapter or generation path.

Opened-collection entry thumbnails use archive-record virtual originals when the collection policy allows cache writes. The URI format is `x-kiriview://thumbnail/archive-entry/v1/crc32/<8-lower-hex-crc32>/<decimal-uncompressed-size>`, where the checksum and size come from collection-backend public ZIP entry metadata. The virtual original mtime is always `0` because freshness is carried by the archive-record URI. The original byte size is the archive entry uncompressed byte length.

KiriView must not parse archive formats solely to derive thumbnail cache identity, require patched archive-library APIs for cache identity, or use decoded-byte or entry-byte hashes for opened-collection cache keys. The thumbnail generation job derives the CRC32 metadata URI before reading entry bytes, performs the XDG lookup with that virtual original, decodes and renders only on cache miss, and installs the generated thumbnail with the same virtual identity. CRC32 cache identity is advisory and isolated to the thumbnail payload; cache collisions must not affect opened image bytes, navigation, deletion, or source document state.

## Source Classes

### Direct Local Images And Videos

The direct-image and direct-video adapters may return cacheable generation plans only for active-navigation rows that satisfy the spec's direct-local thumbnail eligibility and provide stable local-file original identity. They expose local path bytes used by Freedesktop thumbnail lookup and generation, enable XDG cache lookup, and allow generated results to be installed into the personal thumbnail cache when freshness requirements are satisfied by the cache boundary. Direct image and direct video rows outside that eligibility return unsupported fallback.

On XDG cache miss, direct-video generation may use an embedded representative image or a decoded representative frame through an isolated, cancelable thumbnail extraction job. The thumbnail path owns extraction lifecycle and result publication, must not reuse the video document playback backend, and returns a representative preview rather than a frame-accurate timestamp guarantee. Video thumbnail orientation and mirroring follow the image or frame delivered by the platform extraction path.

### Direct Archive-Entry Media

Direct archive-entry media thumbnails represent KDE archive-entry URLs opened as individual direct media items, such as `zip:///path/archive.zip!/page.png`, not entries inside a directly opened collection. These rows return unsupported fallback. They must not reuse opened-collection cache identity, media-entry source metadata, or collection entry byte access.

### Opened Collection Entries

Opened-collection entry thumbnails represent navigation rows inside a directly opened collection whose bytes are owned by a media entry source. The collection access layer owns the shared policy that decides whether an entry can be cached, and both thumbnail source planning and media entry source metadata loading consume that policy instead of duplicating archive-scheme or entry-kind checks.

Only opened-collection image rows that satisfy the spec's archive-entry thumbnail eligibility and whose collection backend exposes usable public record metadata may return a cacheable opened-collection entry plan. Opened-collection rows outside that eligibility return unsupported fallback.

### Collection And Directory Rows

Archive-collection thumbnails represent a collection row rather than an entry inside a directly opened collection. Collection-row sources return unsupported fallback and must not choose representative entries or composed previews.

Directory-collection thumbnails represent directory-backed collection rows or entries. Directory-collection sources return unsupported fallback and must not choose representative files or perform directory traversal from thumbnail demand.

### In-Memory-Only Sources

In-memory-only plans skip XDG lookup and disable cache installation. The runtime may still store the generated image in its image-provider cache, apply retention priority, publish the same ready result roles, and reject stale completions using the same source key and bucket checks as cacheable sources.

This path must not expand the active-navigation thumbnail product contract: rows excluded by the spec return unsupported fallback even if a generation dependency could technically render them.
