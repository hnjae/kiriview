# Extension Contracts

KiriView extension points are adapter contracts, not state backdoors.

## Contract

A media source, thumbnail source, predecode planner, decoder, or image-sequence provider returns typed keys, capabilities, plans, demands, payloads, or completion results to its owning application runtime or supported dependency boundary. It must not mutate QML state, physical item state, public session state, platform action state, cache state, or image presentation directly.

Adapters are synchronous policy providers or asynchronous payload providers behind an owner. Synchronous adapters return plans and capability facts. Asynchronous adapters return payloads through the owner's lifecycle contract, carrying the owner-held operation id, source key plus generation, demand key, or opaque public dependency correlation needed for stale-completion rejection.

Adapter APIs must not accept QML objects, facade objects, mutable public projection objects, or platform action objects. If a capability needs Qt runtime data, the owner captures that data into a plain demand before calling the adapter.

Capabilities are descriptive. They may say whether bytes can be read, thumbnails can be generated or cached, a still image can be predecoded, a collection video source device can be opened, a decode route is supported, an `ImageSequence` can be produced, or a refinement source is available. Capabilities must not imply ownership of public state, image presentation, or platform side effects.

## Identity

Durable identity is the stable key for the thing being addressed. Freshness generation is the owner's proof that a result still belongs to the accepted lifecycle. Equality compares durable identity within one key family only; generation is used to accept or reject work for that lifecycle.

Boundary APIs must encode key families as distinct value types or explicit tagged structs so code cannot accidentally compare ordinary files, opened-collection entries, thumbnails, predecode candidates, and provider work through one generic URL identity.

Candidate snapshots are owner publications defined by [State Ownership](state-ownership.md#candidate-snapshot-boundary). A boundary that consumes active-navigation, thumbnail, deletion, opened-collection foreground-load, or predecode rows receives a typed candidate snapshot or a row/key derived from one, including the source identity and candidate-list revision needed for reuse decisions. Adapter contracts may carry owner-supplied scope generations or thumbnail navigation generations for stale rejection, but they must not invent candidate-list revisions or public projection revisions.

A demand describes requested work from an owner snapshot: key, generation, size bucket, priority, display context, visible area, decode window, or similar inputs. A result reports ready, pending, unsupported, failed, canceled, or stale without changing ownership. Unsupported means the adapter cannot provide the capability for that key; failed means the adapter accepted the demand and could not complete it.

Key-family types are operational contracts, not marker structs. Each family that crosses an adapter, cache, predecode, thumbnail, or provider boundary provides construction from its owning snapshot, durable equality, hashing where used in sets or caches, freshness generation access, and explicit result or capability status. The generic top-level `SourceKey` may appear inside family factories as an implementation detail, but boundary APIs accept and return the family type.

## Source Key Families

### Direct Media Keys

Ordinary direct-media keys use the top-level source-key contract defined in [Source Key Contract](state-ownership/source-keys.md). Extension boundaries consume the typed direct-media key produced by that owner instead of restating URL normalization, platform restoration, or freshness rules.

Direct video keys use the same ordinary direct-media identity rules as direct images. Playback source preparation belongs to the video document runtime; source keys identify routing, navigation, thumbnails, predecode eligibility, and stale completion checks only.

Direct-media scope identity and freshness are owned by the source-key contract. Adapter and cache contracts may carry that identity for stale-completion checks, but they must not derive competing direct-media generations.

### Collection And Entry Keys

Image-document page keys identify a page URL plus its image-document source scope. Ordinary direct image pages use the direct media key. Opened-collection pages include the opened collection scope and page URL. Page keys preserve page kind so image and video rows do not share provider, thumbnail, or predecode capabilities by accident.

Opened-collection scope keys identify the backing collection file URL, collection root URL, and collection kind. Comic-book archive, general archive, and directory scopes are distinct even if URLs match. A collection entry key adds the entry URL relative to that scope and carries the page kind needed to keep playable video source devices separate from image byte reads.

Archive root keys identify the backing archive file and root URL. Archive-entry freshness is owned by the opened collection backend. Thumbnail cache writes may use archive-record virtual originals only when the backend exposes public entry metadata that satisfies the thumbnail contract.

Directory collection keys identify the directory scope root and selected entry. Directory traversal belongs to a collection-owned adapter or runtime, not to QML demand. Active-navigation thumbnail adapters do not use directory keys for cache writes.

### Thumbnail And Predecode Keys

Thumbnail identity uses separate typed families for durable row reuse, generation-scoped UI demand, adapter work, and persistent cache identity. A sealed demand snapshot carries one navigation generation and rejects duplicate or mixed-generation identities. Implementations must not use delimiter-assembled strings as substitutes for typed equality and hashing. Cache and scheduling rules are defined by [Thumbnail Source Adapters](thumbnail-source-adapters.md).

Predecode candidate keys identify still-image payloads eligible for adjacent decode. Direct media predecode is still-image-only; videos may be cursor positions for window planning, but they do not produce cached video frame payloads. Opened-collection predecode candidates carry the opened collection scope so byte access stays behind the media entry source owner.

### ImageViewport Provider Work Keys

Application provider-work keys combine source and scope identity, page role, application source generation, the dependency's opaque public request correlation, and the application reuse identity needed for decode or cache work. Dependency correlation remains opaque and is never replaced by application URLs or cache keys.

A completion may populate an application cache under its reuse identity, but the provider may return it only when the application source generation and the complete public request correlation remain current. KiriView does not define how the dependency creates, advances, or admits that correlation. The application lifecycle is defined by [ImageViewport Integration Architecture](provider-rendering.md#sequence-provider-boundary).

Provider work returns payloads supported by the dependency's public provider contract. Source-internal tiling may remain a private decoder or refinement technique but must not create an application-owned image-presentation path.

## Adapter Contracts

### Media Entry Source Adapters

Media entry source adapters list and read opened-collection entries. They return candidates, image bytes, optional thumbnail metadata, typed failure payloads, and eligible video playback source devices through the media entry source owner.

An opened-collection listing is published as one immutable, revisioned candidate snapshot after traversal and filtering complete. Partial traversal results must not become active navigation state. An accepted empty snapshot retains the opened collection identity while exposing no selected row.

Archive entry paths are normalized as collection-relative logical paths. Collection access must reject absolute paths and traversal that escapes the archive root, and byte, metadata, thumbnail, and playback-device access must be authorized by an entry key from the current accepted collection snapshot.

Directory collection traversal must resolve candidates within the selected root, exclude entries that resolve outside it, and prevent directory-link cycles. Consumers receive collection-relative identities and must not reopen candidates through an unvalidated path.

Video playback source devices may be exposed only for collection entries whose storage backend can provide the final product's playable collection-video contract. Unsupported video entries remain navigation candidates without playback devices. A returned video playback source device keeps backing archive, entry, and device lifetime behind the media entry source contract until the video source owner clears or supersedes it.

Failure payloads preserve backend, operation, collection URL, optional entry path, user-facing text, and diagnostic detail before any document, video, or thumbnail owner projects them into broader UI messages. They do not update document source state, page navigation, deletion state, thumbnails, playback state, or QML models directly.

### Thumbnail Source Adapters

Thumbnail source adapters consume generation-scoped thumbnail source revision keys and demand buckets, then return unsupported fallback, cacheable local-file generation, cacheable opened-collection entry generation, or in-memory-only generation. The document-session thumbnail runtime owns scheduling, lookup, generation jobs, cache installation, image-store retention, result projection, cancellation, and stale-completion rejection.

Thumbnail row and source revision keys are derived from a confirmed candidate snapshot row, with the revision additionally carrying the current thumbnail navigation generation. The thumbnail adapter may classify the row and choose cache or generation capability, but it does not own candidate-list source, candidate-list revision, active row selection, demand-window state, or public active-navigation projection.

Thumbnail image-provider publication happens only after the thumbnail runtime accepts a lookup or generation result for the current source revision key and demand bucket.

### Predecode Planners

Predecode planners consume session or image-document snapshots and produce still-image decode windows. The predecode runtime owns debounce, power-saver suppression, active load admission, decode jobs, cache lifetime, and generation acceptance.

Predecode planners consume candidate snapshots as immutable row inputs. Direct-media predecode uses the document-session direct-media candidate snapshot for window planning and still-image eligibility, while image-document predecode uses the page-navigation candidate snapshot when the snapshot source matches the requested page source. A planner may return candidate keys and decode windows, but it must not retain a mutable row list, advance candidate-list revision, or publish active-navigation state.

### Decoder Contracts

Decoder contracts are route based. C++ image-format policy owns advertised extension/MIME metadata, decoder-family capability, and byte/file-name classification that selects one decode route from plain bytes and file-name context. The C++ decoder runtime executes the selected route and treats selected-decoder failure as final for that request. A selected route may invoke the Rust support static library for APNG decoding, SVG rasterization, or embedded metadata parsing, but the support result does not select a different route.

A decoder returns decoded static image, animation reader payload, metadata, unsupported, or failure; it must not route to another decoder or mutate document state. Failure payloads preserve selected route, decoder operation, user-facing text, diagnostic detail, severity, and retryability before the image document maps them into its load-failure projection.

Source-neutral display diagnostics expose typed operation outcomes for first-display, blocking-preview, and raster-refinement paths. String error outputs are derived views over those diagnostics. Source-specific decoder boundaries may expose richer diagnostics, but production owners must always have a typed source-neutral outcome.

### ImageSequence Provider Contracts

The KiriView `ImageSequence` provider adapts application source access, decode, cache, predecode, and refinement owners to the supported dependency protocol. It receives opaque public request correlation and returns supported results without owning or inferring presentation state.

Provider request handling may reuse an already accepted application cache entry or schedule source work through the owning runtime's lifecycle boundary. The provider may apply tighter application cache, display-store, or source-work budgets when selecting a supported result, but it must preserve the public request correlation, obey supplied public limits, and never use QML engine caching as freshness authority.

Application payload ownership may carry display-store leases across the provider boundary. Provider sessions and application resource owners follow supported ownership callbacks for lifetime and backpressure but must not infer presentation state from callback timing.

Provider failures preserve application detail in the existing typed KiriView failure value. Only values allowed by the supported provider protocol cross the dependency boundary. An opaque application reference is resolved only for a matching application target, and the application registry follows public ownership callbacks for retirement.

### Excluded Extension Paths

Application image extensions stop at the supported sequence-provider boundary. They must not access dependency-private APIs or introduce an alternate application-owned image-presentation path.
