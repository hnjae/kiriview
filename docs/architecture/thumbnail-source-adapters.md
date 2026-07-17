# Thumbnail Source Adapters

Active-navigation thumbnails use source adapters so scheduling, cache policy, source access, and result projection remain independent from the navigation-row kind.

## Ownership

The document-session thumbnail runtime owns the accepted row snapshot, current-row projection, demand state, scheduling, cancellation, stale-completion rejection, result publication, and thumbnail image-store lifetime. QML reports the current visible and nearby demand window and renders projected results; it must not retain readiness, choose source capability, schedule work, or mutate row state.

Source adapters classify an owner-supplied thumbnail source revision and return a typed plan for unsupported fallback, cacheable generation, or non-persistent generation. They may describe how bytes or a representative video image can be obtained, but they must not schedule jobs, mutate projections, install cache entries, or invent navigation identity.

Cache lookup, generation, and source access execute behind injectable provider ports. Thumbnail work must not borrow video playback state, mutable collection state, or another runtime's cache authority. Collection entry bytes and metadata remain owned by the collection-access boundary.

Video thumbnail extraction has one deterministic workflow owner separate from its Qt Multimedia adapter. The adapter owns media-player, video-sink, metadata, and frame conversion objects; it reports plain media facts, images, and failures through a narrow backend port and executes source, seek, playback, and stop commands returned by the workflow. The runtime owner supplies timeout firing through `TimerScheduler`, serializes reentrant backend events, and invalidates the workflow before cancellation or terminal cleanup so late callbacks cannot publish a result.

## Demand And Scheduling

Each demand update is one immutable, generation-scoped snapshot. The runtime validates the complete snapshot before replacing accepted demand; malformed, mixed-generation, duplicate, or unknown-row input is rejected without partial mutation.

The selected row and visible rows are foreground demand. Nearby rows are admitted only after foreground demand is satisfied or inactive. Optional background filling runs only while foreground and nearby demand are idle. New higher-priority demand preempts lower-priority work, and a failed higher-detail request preserves an already usable lower-detail result.

The document-session thumbnail scheduler owns an explicit foreground admission capacity. It admits the selected row first, then visible rows in navigation order, then nearby rows in navigation order; demand beyond capacity remains pending and must not construct lookup, generation, or multimedia backend resources until a slot is available. Production admits at most two simultaneous foreground jobs, while background filling remains limited to one job and yields to any foreground demand.

Row identity, source revision, requested physical-size bucket, priority, and work identity travel together through scheduling and completion. A completion may publish only after both the backend job owner and the scheduling owner accept those identities. Cancellation is best-effort; invalidated or late callbacks are no-ops.

Changing only the current row preserves compatible results and work. Replacing row identity or order advances the navigation generation, invalidates incompatible demand and work before publication, and releases entries no longer referenced by the new row set. Equivalent source-payload refreshes may preserve generation only when durable row identity and ordering are unchanged.

## Cache And Provider Boundary

Thumbnail cache identity is separate from navigation-row identity. It is derived from stable source metadata supplied by the owning source adapter and must change whenever freshness-relevant source content changes. Cache-key collisions or stale cache records may affect only the thumbnail fallback; they must never affect navigation, displayed media bytes, deletion, or session identity.

Persistent cache lookup and installation are allowed only for source kinds whose identity and freshness can be established without reimplementing archive parsing or reading unrelated content solely to manufacture a key. Non-persistent generation uses the same scheduling and stale-rejection contracts but does not install results in the desktop thumbnail cache.

The thumbnail image provider is cache-only and reentrant. It may return an already published immutable image entry and its stored size; it must not decode, resize, perform I/O, schedule work, decide freshness, or mutate active navigation.

## Source Classes

- Eligible direct local image and video rows may use persistent lookup and generation from stable local-file identity. Video generation may use an embedded representative image or a representative decoded frame, but it must remain isolated from playback state and does not promise a frame or timestamp.
- Direct KDE archive-entry media rows return the specification's fallback and must not reuse opened-collection entry access or cache identity.
- Eligible image rows inside supported ZIP-backed opened collections may use collection-owned public entry metadata and byte access. Other opened-collection rows return the specification's fallback.
- Directory-collection rows and collection-container rows return the specification's fallback and must not trigger representative-file selection or directory traversal.

Adapters must not expand the thumbnail eligibility declared in the product specification merely because a backend could technically render another source class.
