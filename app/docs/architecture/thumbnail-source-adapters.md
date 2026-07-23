# Thumbnail Source Adapters

Active-navigation thumbnails use source adapters so scheduling, cache policy, source access, and result projection remain independent from the navigation-row kind.

## Ownership

The navigation owner remains canonical for candidate rows and their freshness. The document-session thumbnail runtime retains an accepted immutable candidate-derived view as lifecycle input and owns thumbnail projection, demand state, admission, cancellation, stale-completion rejection, result publication, and thumbnail image-store lifetime. QML reports the current visible and nearby demand window and renders projected results; it must not retain readiness, choose source capability, schedule work, or mutate row state.

Source adapters classify an owner-supplied thumbnail source identity and return an explicit result for unsupported fallback, cacheable generation, or non-persistent generation. They may describe how bytes or a representative video image can be obtained, but they must not schedule jobs, mutate projections, install cache entries, or invent navigation identity.

Cache lookup, generation, and source access remain behind replaceable provider boundaries. The default persistent-cache provider delegates specification-compatible lookup and installation to the Rust support static library's [desktop thumbnail-cache capability](rust-support-boundary.md); the thumbnail runtime owns admission, capability selection, result acceptance, and publication. Thumbnail work must not borrow video playback state, mutable collection state, or another runtime's cache authority. Collection entry bytes and metadata remain owned by the collection-access boundary.

Video thumbnail extraction has one deterministic workflow owner separate from its Qt Multimedia adapter. The adapter owns backend-specific playback, metadata, frame conversion, and resource lifetime; it reports plain media facts, images, and failures through a narrow backend boundary and executes source, seek, playback, and stop decisions from the workflow. Timeout and reentrant backend events remain subordinate to the workflow lifecycle, which is invalidated before cancellation or terminal cleanup so late callbacks cannot publish a result. The concrete object, timer, and adapter interfaces are implementation choices.

## Demand And Scheduling

Each demand update is one immutable snapshot correlated with the accepted navigation view. The runtime validates the complete snapshot before replacing accepted demand; malformed, inconsistently correlated, duplicate, or unknown-row input is rejected without partial mutation.

Admission follows the visible, nearby, selected, and optional background priorities defined by [Navigation](../spec/navigation.md). Higher-priority demand can displace lower-priority work, and a failed higher-detail request preserves an already usable lower-detail result.

The thumbnail runtime bounds admitted work and backend resources. Demand that is not admitted remains pending without acquiring expensive source or multimedia resources, and optional background work yields to foreground demand. Exact capacities, queue structure, and tie-breaking among equal-priority rows are runtime resource policy.

Row identity, source freshness, requested physical-size bucket, priority, and work correlation remain associated through scheduling and completion. A completion may publish only after both the backend job owner and the thumbnail runtime accept that correlation. Cancellation is best-effort; invalidated or late callbacks are no-ops.

Position-only changes preserve compatible results and work. A source, row-identity, or ordering change invalidates incompatible demand and work before publication and releases entries no longer retained by the accepted row set. Equivalent refreshes may preserve compatible work when durable identity and ordering are unchanged.

## Cache And Provider Boundary

Thumbnail cache identity is separate from navigation-row identity. It is derived from stable source metadata supplied by the owning source adapter and must change whenever freshness-relevant source content changes. Cache-key collisions or stale cache records may affect only the thumbnail fallback; they must never affect navigation, displayed media bytes, deletion, or session identity.

Persistent cache lookup and installation are allowed only for source kinds whose identity and freshness can be established without reimplementing archive parsing or reading unrelated content solely to manufacture a key. Non-persistent generation uses the same scheduling and stale-rejection contracts but does not install results in the desktop thumbnail cache.

The thumbnail image provider is cache-only and reentrant. It may return an already published immutable image entry and its stored size; it must not decode, resize, perform I/O, schedule work, decide freshness, or mutate active navigation.

## Source Eligibility

Thumbnail eligibility, representative-preview behavior, and visible fallbacks are defined by [Navigation](../spec/navigation.md). Adapters implement that contract without expanding eligibility merely because a backend could technically render another source class.

Eligible persistent-cache work requires stable source identity and freshness supplied by the source owner. Opened-collection generation uses only collection-owned public metadata and byte access, direct archive-entry media does not borrow opened-collection authority, directory rows do not trigger representative-file traversal, and video thumbnail generation remains isolated from playback state.
