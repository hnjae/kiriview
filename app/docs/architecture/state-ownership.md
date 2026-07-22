# State Ownership

This document is the canonical contract for durable runtime state ownership. Owner-group files under `state-ownership/` hold the detailed local boundaries.

## Contract

Every workflow value has one canonical owner. If another layer needs the value, it receives a derived snapshot, projection, delta, command, or completion event rather than storing a second mutable copy.

C++ owns application runtime state unless this document or an owner-group architecture contract explicitly names another owner. This includes workflow policy state, QML-facing properties, Qt notification ordering, `QUrl`, `QImage`, `QString`, async job lifetime, presentation objects, and rendering objects.

C++ policy operates on value snapshots and plain events. It returns explicit state deltas, transition plans, and effect descriptions for runtime owners to apply; those results are not an independent authoritative copy of the same workflow state.

Rust-owned state is limited to capability-local implementation state inside the media-support static library, such as an opaque APNG stream decoder. It must not mirror or own application source identity, navigation indices, cache policy, workflow state, public projections, or component state. Canonical image presentation geometry and zoom remain inside `ImageViewport`.

Moving a library-backed operation behind FFI does not move authoritative runtime state. The C++ caller remains the application lifecycle owner and decides when a support result is accepted or rejected.

QML and facade objects may observe owner projections, emit UI facts through owner APIs, and render accepted state. They must not store durable mirrors, mutate public workflow state, apply command acknowledgment state, choose cache or render policy, or bypass an owner to update another owner's state.

Derived public values may combine multiple C++ runtime states, such as document state and a matched `ImageViewport` snapshot. The derived value must not become a second mutable source of truth, and notification dependencies must follow the canonical owners that feed it.

When a public value has mode-specific ownership, only the active mode owns that value. Inactive mode state is a cache, projection, or restoration point. Transition code must synchronize the next active owner before exposing the mode change.

## Candidate Snapshot Boundary

A candidate snapshot is the immutable row-list value for one active navigation source. It carries typed source identity, a candidate-list revision, shared immutable row storage, current index facts, and count facts.

The snapshot owner installs one immutable row storage value for an accepted source and assigns the candidate-list revision for that storage. Consumers may derive projections, thumbnail rows, predecode windows, deletion fallbacks, and foreground opened-collection loads from the snapshot, but they must not keep a mutable row list as independent navigation state.

The document session owns ordinary direct-media candidate snapshots for the active direct-media scope. Direct-media scope generation remains the stale-completion token for sibling discovery and is distinct from candidate-list revision; current-row changes inside unchanged row storage do not create a new candidate-list revision.

Image-document page navigation owns confirmed page candidate snapshots for directory and opened-collection page sources. The snapshot source identity is the candidate-list source, the candidate-list revision identifies accepted immutable rows for that source, and the image-document owner remains authoritative for pending same-scope page selection and current page facts.

Candidate-list source identity, candidate-list revision, direct-media scope generation, public projection revision, and thumbnail navigation generation are separate tokens. Source identity answers which list the rows belong to, candidate-list revision answers whether row storage can be reused, direct-media scope generation rejects stale direct-media discovery completions, public projection revision orders QML-facing session publication, and thumbnail navigation generation rejects stale thumbnail work derived from a projected row set. Collapsing these tokens creates false stale rejection or false reuse and is not allowed.

C++ navigation, thumbnail, predecode, and deletion policy may compute plans from candidate snapshot metadata and row views. The runtime owner remains responsible for the accepted snapshot, Qt row values, async listing lifecycle, and publication ordering.

## Thumbnail Demand Window Boundary

The active-navigation thumbnail demand window is submitted as one immutable, generation-scoped value and owned by the C++ thumbnail runtime after acceptance. No layer exposes a partial begin/report/finish transaction or retains a partially submitted window.

QML may report visible and nearby projected rows, their physical thumbnail size, the selected row, and the active thumbnail navigation generation. QML must not accumulate demand history, expire demand, choose background work, schedule thumbnail jobs, or retain row readiness independently.

The runtime validates the complete window before mutation and rejects malformed, mixed-generation, duplicate, or unknown-row facts atomically. Demand expiration, priority, admission, cancellation, background filling, result retention, and completion acceptance follow [Thumbnail Source Adapters](thumbnail-source-adapters.md#demand-and-scheduling).

Background thumbnail fill is optional idle work and is not a full-list product guarantee.

## Owner Groups

- [Application Shell and Session](state-ownership/shell-session.md): startup routing, document-session projections, title, media information, and toasts.
- [Actions and UI Gates](state-ownership/actions-ui-gates.md): actions, shortcuts, menu presentation, command dispatch, and UI-local gates.
- [Media Runtime Owners](state-ownership/media-runtime.md): image loading, provider-backed animated sources, video loading and playback controls, and metadata parsing.
- [Navigation and Location](state-ownership/navigation-location.md): active navigation projection, supported-media lists, sibling archives, and source identity.
- [Presentation and Viewport](state-ownership/presentation-viewport.md): `ImageViewport` ownership, application integration, viewport command flow, media workspace composition, source resources, and failure correlation.
- [Operations and Platform Effects](state-ownership/operations-platform.md): deletion, Open With, platform capability snapshots, and owner bypass rules.
- [Preparation and Cache](state-ownership/preparation-cache.md): thumbnail strip preparation, still-image predecode, caches, and HEIF source-internal tiling.
- [Source Key Contract](state-ownership/source-keys.md): top-level source identity, direct-media freshness, and family-specific key boundaries.
