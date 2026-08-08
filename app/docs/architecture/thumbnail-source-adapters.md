# Thumbnail Source Adapters

Active-navigation thumbnails use source adapters so scheduling, cache policy, source access, and result projection remain independent from the navigation-row kind.

## Ownership

The navigation owner remains canonical for candidate rows and their freshness. The document-session thumbnail runtime retains an accepted immutable candidate-derived view as lifecycle input and owns thumbnail projection, demand state, admission, cancellation, stale-completion rejection, result publication, and thumbnail image-store lifetime. QML reports the current visible and nearby demand window and renders projected results; it must not retain readiness, choose source capability, schedule work, or mutate row state.

Source adapters classify an owner-supplied thumbnail source identity and return an explicit result for unsupported fallback, cacheable generation, or non-persistent generation. They may expose source bytes or an authorized media source through public provider inputs, but they must not schedule jobs, mutate projections, install cache entries, or invent navigation identity.

Cache lookup, generation, and source access remain behind replaceable provider boundaries. The default persistent-cache provider delegates specification-compatible lookup and installation to the Rust support static library's [desktop thumbnail-cache capability](rust-support-boundary.md); the thumbnail runtime owns admission, capability selection, result acceptance, and publication. Thumbnail work must not borrow video playback state, mutable collection state, or another runtime's cache authority. Collection entry bytes and metadata remain owned by the collection-access boundary.

Video thumbnail extraction remains behind a provider boundary. KiriView supplies an eligible authorized source and requested bound, keeps caller-owned access prerequisites valid while an extraction job is active, and maps the typed result into application failure and publication policy. KiriView treats the extraction job as an opaque asynchronous capability and must not depend on provider implementation mechanisms or borrow application playback state for extraction.

## Demand And Scheduling

Each demand update is one immutable snapshot correlated with the accepted navigation view. The runtime validates the complete snapshot before replacing accepted demand; malformed, inconsistently correlated, duplicate, or unknown-row input is rejected without partial mutation.

Admission follows the visible, nearby, selected, and optional background priorities defined by [Navigation](../spec/navigation.md). Higher-priority demand can displace lower-priority work, and a failed higher-detail request preserves an already usable lower-detail result.

The thumbnail runtime bounds aggregate admitted work and resource pressure across source providers. Each provider bounds the resources acquired by its admitted operations according to its own contract. Demand that is not admitted remains pending without acquiring expensive provider resources, and optional background work yields to foreground demand. Exact capacities, queue structure, and tie-breaking among equal-priority rows are runtime resource policy.

Image-thumbnail source bytes share the application source-data admission boundary with foreground and preparation work. Direct files and opened-collection entries cannot bypass its per-source or aggregate byte limits, including when an archive entry expands beyond its stored size. Source-data admission failure completes the thumbnail work as failed without decoding, cache installation, image-store publication, or a retained partial buffer.

Row identity, source freshness, requested physical-size bucket, priority, and work correlation remain associated through scheduling and completion. A completion may publish only after both the backend job owner and the thumbnail runtime accept that correlation. Cancellation is best-effort; invalidated or late callbacks are no-ops.

Position-only changes preserve compatible results and work. A source, row-identity, or ordering change invalidates incompatible demand and work before publication and releases entries no longer retained by the accepted row set. Equivalent refreshes may preserve compatible work when durable identity and ordering are unchanged.

## Cache And Provider Boundary

Thumbnail cache identity is separate from navigation-row identity. Except for the opened-ZIP exception below, it is derived from stable source metadata supplied by the owning source adapter and must change whenever freshness-relevant source content changes. Cache-key collisions or stale cache records may affect only the thumbnail fallback; they must never affect navigation, displayed media bytes, deletion, or session identity.

ZIP-backed opened-collection image entries intentionally use only the ZIP record CRC32 and uncompressed byte size as a probabilistic persistent cache identity. Archive location, entry path, timestamps, and a stronger payload digest do not participate, so entries with the same pair share an identity across archives and sessions. The resulting risk of reusing a stale or unrelated preview is accepted so cache lookup can precede reading and decompressing the entry. This identity is non-authoritative and must never serve as freshness evidence for navigation, displayed media bytes, deletion, or session identity.

Persistent cache lookup and installation are allowed only for source kinds whose identity and freshness can be established without reimplementing archive parsing or reading unrelated content solely to manufacture a key. Non-persistent generation uses the same scheduling and stale-rejection contracts but does not install results in the desktop thumbnail cache.

Persistent installation is a best-effort side effect after a current usable thumbnail has been generated. Installation failure leaves that image eligible for normal acceptance and publication, retains no installed-cache path, and crosses the provider boundary as a typed non-fatal diagnostic; it must not turn the usable generation result into a failed thumbnail.

The thumbnail image provider is cache-only and reentrant. It may return an already published immutable image entry and its stored size; it must not decode, resize, perform I/O, schedule work, decide freshness, or mutate active navigation.

## Source Eligibility

Thumbnail eligibility, representative-preview behavior, and visible fallbacks are defined by [Navigation](../spec/navigation.md). Adapters implement that contract without expanding eligibility merely because a backend could technically render another source class.

Eligible persistent-cache work requires source-owned identity metadata with the freshness resolution declared above. Opened-collection generation uses only collection-owned public metadata and byte access, direct archive-entry media does not borrow opened-collection authority, directory rows do not trigger representative-file traversal, and video thumbnail generation remains isolated from playback state.
