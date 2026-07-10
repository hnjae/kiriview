# State Ownership

This document is the canonical contract for durable runtime state ownership. Owner-group files under `state-ownership/` hold the detailed local boundaries.

## Contract

Every workflow value has one canonical owner. If another layer needs the value, it receives a derived snapshot, projection, delta, command, or completion event rather than storing a second mutable copy.

C++ owns QObject-facing runtime state unless this document or an ADR explicitly names another owner. This includes QML-facing properties, Qt notification ordering, `QUrl`, `QImage`, `QString`, async job lifetime, presentation objects, and rendering objects.

Rust reducers operate on value snapshots and plain events. They return explicit state deltas, transition plans, and effect descriptions for C++ owners to apply; those results are not an independent authoritative copy of the same workflow state.

Rust-owned state is limited to self-contained Qt-independent domains where the state is plain data and does not mirror authoritative C++ state. Format parsing, geometry, and zoom algorithms may live there; navigation indices, cache policy state, or other workflow state may move to Rust only when this document or an ADR names the new owner and exposes it through value-based FFI.

Moving policy into Rust does not move authoritative runtime state. The ownership decision must name both the policy boundary and the state owner.

QML and facade objects may observe owner projections, emit UI facts through owner APIs, and render accepted state. They must not store durable mirrors, mutate public workflow state, apply command acknowledgment state, choose cache or render policy, or bypass an owner to update another owner's state.

Derived public values may combine multiple C++ runtime states, such as document state and active presentation transition state. The derived value must not become a second mutable source of truth, and notification dependencies must follow the canonical owners that feed it.

When a public value has mode-specific ownership, only the active mode owns that value. Inactive mode state is a cache, projection, or restoration point. Transition code must synchronize the next active owner before exposing the mode change.

## Candidate Snapshot Boundary

A candidate snapshot is the immutable row-list value for one active navigation source. It carries typed source identity, a candidate-list revision, shared immutable row storage, current index facts, and count facts.

The snapshot owner installs one immutable row storage value for an accepted source and assigns the candidate-list revision for that storage. Consumers may derive projections, thumbnail rows, predecode windows, deletion fallbacks, and foreground opened-collection loads from the snapshot, but they must not keep a mutable row list as independent navigation state.

The document session owns ordinary direct-media candidate snapshots for the active direct-media scope. Direct-media scope generation remains the stale-completion token for sibling discovery and is distinct from candidate-list revision; current-row changes inside unchanged row storage do not create a new candidate-list revision.

Image-document page navigation owns confirmed page candidate snapshots for directory and opened-collection page sources. The snapshot source identity is the candidate-list source, the candidate-list revision identifies accepted immutable rows for that source, and the image-document owner remains authoritative for pending same-scope page selection and current page facts.

Candidate-list source identity, candidate-list revision, direct-media scope generation, public projection revision, and thumbnail navigation generation are separate tokens. Source identity answers which list the rows belong to, candidate-list revision answers whether row storage can be reused, direct-media scope generation rejects stale direct-media discovery completions, public projection revision orders QML-facing session publication, and thumbnail navigation generation rejects stale thumbnail work derived from a projected row set. Collapsing these tokens creates false stale rejection or false reuse and is not allowed.

Rust may compute navigation, thumbnail, predecode, or deletion policy from candidate snapshot metadata and row views. C++ remains the owner of the accepted snapshot, Qt row values, async listing lifecycle, and publication ordering.

## Thumbnail Demand Window Boundary

The active-navigation thumbnail demand window is runtime state owned by the C++ document-session thumbnail work coordinator. The document-session thumbnail runtime only composes that coordinator with the active-navigation thumbnail row store.

QML may report which projected rows are visible in the strip viewport, which instantiated rows are nearby the viewport or reveal target, each reported row's physical thumbnail size, and the active thumbnail navigation generation. QML must not accumulate demand history, expire demand, choose background-fill rows, schedule thumbnail work, or retain row readiness independently of the runtime.

Visible demand, nearby demand, and the current user-selected row are distinct priority inputs over the same confirmed thumbnail row set. Visible rows and the current selected row are foreground demand. Nearby rows are prefetch demand around the viewport or reveal target and yield to foreground work. Rows outside the latest visible, nearby, and selected demand window are not foreground demand.

Demand facts are scoped to active thumbnail row identity, thumbnail navigation generation, source key, demand bucket, and runtime demand epoch. A visible or nearby demand expires when a newer demand-window epoch no longer contains that row at that priority, when the row source key or thumbnail navigation generation changes, when the requested bucket is superseded, or when the thumbnail row set resets. Completion acceptance still uses the source key, navigation generation, bucket, and active job identity.

Background thumbnail fill is optional idle work owned by the runtime after foreground demand is no longer pending or active. It may progressively fill additional eligible placeholders, but it is not a full-list product guarantee and must not require scanning the complete thumbnail row list on every foreground demand, completion, or model update.

## Owner Groups

- [Application Shell and Session](state-ownership/shell-session.md): startup routing, document-session projections, title, media information, and toasts.
- [Actions and UI Gates](state-ownership/actions-ui-gates.md): actions, shortcuts, menu presentation, command dispatch, and UI-local gates.
- [Media Runtime Owners](state-ownership/media-runtime.md): image loading, video loading, playback controls, metadata parsing, and animation playback.
- [Navigation and Location](state-ownership/navigation-location.md): active navigation projection, supported-media lists, sibling archives, and source identity.
- [Presentation and Viewport](state-ownership/presentation-viewport.md): image presentation, viewport command flow, media workspace composition, display-source projection, and render context.
- [Operations and Platform Effects](state-ownership/operations-platform.md): deletion, Open With, platform capability snapshots, and owner bypass rules.
- [Preparation and Cache](state-ownership/preparation-cache.md): thumbnail strip preparation, still-image predecode, caches, and HEIF source-internal tiling.
- [Source Key Contract](state-ownership/source-keys.md): top-level source identity, direct-media freshness, and family-specific key boundaries.
