# State Ownership

This document defines shared ownership rules for durable runtime state and indexes the owner-group contracts that hold the detailed boundaries.

## Core Ownership Rules

For QObject-facing workflows, C++ owns the authoritative runtime state. This includes QML-facing properties, Qt notification ordering, `QUrl`, `QImage`, `QString`, async job lifetime, presentation objects, and rendering objects.

Rust reducers operate on value snapshots and plain events. They return explicit state deltas, transition plans, and effect descriptions. Those results describe what C++ applies; they are not an independent authoritative copy of the same workflow state.

Rust-owned state is reserved for self-contained Qt-independent domains where the state can be represented as plain values and does not mirror authoritative C++ state. Examples include format parsing state and geometry or zoom algorithms. Navigation indices, cache policy state, or other workflow state may move to Rust only when that ownership is documented in this file or an ADR and exposed through value-based FFI.

Avoid split-brain state. A workflow value must have one canonical owner. If both languages need to observe it, one side owns the value and the other side receives a derived snapshot, projection, delta, or completion event.

These ownership rules apply unless this document or an ADR explicitly changes the owner. Moving policy into Rust does not move the authoritative runtime state unless the same decision also names the new state owner.

QML-facing values may be derived from multiple C++ runtime states. For example, a public loading or status property may combine document state with an active presentation transition. The derived value must not become a second mutable source of truth. Keep the canonical owners explicit, and make notification dependencies follow the derived value.

When a public value has mode-specific ownership, only the active mode owns that value. Inactive mode state is a cache, projection, or restoration point, not a competing owner. Transition code must synchronize the next active owner before exposing the mode change. For example, different presentation modes may use different internal state owners, but the public value must have exactly one active owner at a time.

## Candidate Snapshot Boundary

A candidate snapshot is the immutable row-list value for one active navigation source. It carries typed source identity, a candidate-list revision, shared immutable row storage, current index facts, and count facts. Consumers may derive projections, thumbnail rows, predecode windows, deletion fallbacks, and foreground opened-collection loads from that snapshot, but they must not keep a mutable copy as independent navigation state.

The document session owns ordinary direct-media candidate snapshots for the active direct-media scope. Direct-media scope generation remains the stale-completion token for sibling discovery and is distinct from candidate-list revision; the candidate-list revision advances only when the accepted row storage for the same scope changes. Current-row facts may change because the selected target changes inside the same row storage, and those changes must not be modeled as a new candidate-list revision.

Image-document page navigation owns confirmed image-document page candidate snapshots for directory and opened-collection page sources. The snapshot source identity is the candidate-list source, the candidate-list revision identifies the accepted immutable rows for that source, and the image-document owner remains authoritative for pending same-scope page selection and current page facts. Opened-collection foreground loading and image-document predecode may reuse the snapshot only while the source identity still matches.

Candidate-list source identity, candidate-list revision, direct-media scope generation, public projection revision, and thumbnail navigation generation are separate tokens. Source identity answers which list the rows belong to. Candidate-list revision answers whether row storage can be reused. Direct-media scope generation rejects stale direct-media discovery completions. Public projection revision orders QML-facing session publication. Thumbnail navigation generation rejects stale thumbnail work derived from a projected row set. Collapsing these tokens into one generation creates false stale rejection or false reuse and is not allowed.

Rust may compute navigation, thumbnail, predecode, or deletion policy from candidate snapshot metadata and row views, but C++ remains the owner of the accepted snapshot, Qt row values, async listing lifecycle, and publication ordering.

## Owner Groups

- [Application Shell and Session](state-ownership/shell-session.md): startup routing, document-session projections, title, media information, and toasts.
- [Actions and UI Gates](state-ownership/actions-ui-gates.md): actions, shortcuts, menu presentation, command dispatch, and UI-local gates.
- [Media Runtime Owners](state-ownership/media-runtime.md): image loading, video loading, playback controls, metadata parsing, and animation playback.
- [Navigation and Location](state-ownership/navigation-location.md): active navigation projection, supported-media lists, sibling archives, and source identity.
- [Presentation and Viewport](state-ownership/presentation-viewport.md): image presentation, viewport command flow, media workspace composition, display-source projection, and render context.
- [Operations and Platform Effects](state-ownership/operations-platform.md): deletion, Open With, platform capability snapshots, and owner bypass rules.
- [Preparation and Cache](state-ownership/preparation-cache.md): thumbnail strip preparation, still-image predecode, caches, and HEIF source-internal tiling.
- [Source Key Contract](state-ownership/source-keys.md): top-level source identity, direct-media freshness, and family-specific key boundaries.
