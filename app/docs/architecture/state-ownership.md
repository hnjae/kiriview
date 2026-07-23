# State Ownership

This document is the canonical contract for durable runtime state ownership. Owner-group files under `state-ownership/` hold the detailed local boundaries.

## Contract

Every workflow value has one canonical owner. Another layer may receive a readonly observation, derived projection, command result, or completion fact, but it must not store a second mutable authority.

Each application runtime value belongs to the owner named by this document or an owner-group architecture contract. This includes workflow policy state, QML-facing properties, Qt notification ordering, `QUrl`, `QImage`, `QString`, async job lifetime, viewport integration objects, and provider resources. Dependency-private state is outside this ownership contract.

Policy operates on coherent owned values and returns explicit decisions for runtime owners to apply; those results are not an independent authoritative copy of the same workflow state. The concrete input and result representation is not an architecture contract.

Rust-owned state is limited to capability-local implementation state inside the media-support static library, such as an opaque APNG stream decoder. It must not mirror or own application source identity, navigation indices, cache policy, workflow state, public projections, or dependency state. KiriView consumes image-presentation observations through the supported `ImageViewport` boundary and does not maintain a second mutable presentation authority.

Moving a library-backed operation behind FFI does not move authoritative runtime state. The calling application runtime remains the lifecycle owner and decides when a support result is accepted or rejected.

QML and facade objects may observe owner projections, emit UI facts through owner APIs, and render accepted state. They must not store durable mirrors, mutate public workflow state, apply command acknowledgment state, choose cache or presentation policy, or bypass an owner to update another owner's state.

Derived public values may combine multiple application runtime states with a matched viewport observation. The derived value must not become a second mutable source of truth, and notification dependencies must follow the canonical owners that feed it.

When a public value has mode-specific ownership, only the active mode owns that value. Inactive mode state is a cache, projection, or restoration point. Transition code must synchronize the next active owner before exposing the mode change.

## Candidate Snapshot Boundary

A candidate snapshot is a coherent immutable view of the accepted rows and position facts for one active navigation source. It carries source identity and enough owner-issued freshness evidence for consumers to reject stale work and decide whether row-derived results remain reusable.

The snapshot owner accepts a complete row set for a source. Consumers may derive projections, thumbnail rows, predecode windows, deletion fallbacks, and foreground opened-collection loads from that view, but they must not keep a mutable row list as independent navigation state.

The document session owns ordinary direct-media candidate snapshots for the active direct-media scope. Its freshness evidence distinguishes a source-scope replacement from a position change within an unchanged accepted row set so sibling discovery and row-derived work are neither accepted stale nor invalidated unnecessarily.

Image-document page navigation owns confirmed page candidate snapshots for directory and opened-collection page sources. The image-document owner remains authoritative for accepted rows, pending same-scope page selection, and current page facts.

Source identity, row-set freshness, async-operation freshness, public publication ordering, and thumbnail-work freshness are distinct semantic questions. An implementation may represent them with separate values or a composite identity, provided equality and acceptance rules cannot confuse the questions, admit stale work, or invalidate compatible work merely because the current position changed.

Navigation, thumbnail, predecode, and deletion policy may compute plans from candidate snapshot metadata and row views. The runtime owner remains responsible for the accepted snapshot, Qt row values, async listing lifecycle, and publication ordering.

## Thumbnail Demand Window Boundary

The active-navigation thumbnail demand window is submitted as one immutable value correlated with the accepted navigation snapshot and owned by the thumbnail runtime after acceptance. No layer exposes a partial begin/report/finish transaction or retains a partially submitted window.

QML may report visible and nearby projected rows, their physical thumbnail size, the selected row, and the correlation supplied with the active thumbnail projection. QML must not accumulate demand history, expire demand, choose background work, schedule thumbnail jobs, or retain row readiness independently.

The runtime validates the complete window before mutation and rejects malformed, inconsistently correlated, duplicate, or unknown-row facts atomically. Demand expiration, priority, admission, cancellation, background filling, result retention, and completion acceptance follow [Thumbnail Source Adapters](thumbnail-source-adapters.md#demand-and-scheduling).

Background thumbnail fill is optional idle work and is not a full-list product guarantee.

## Owner Groups

- [Application Shell and Session](state-ownership/shell-session.md): startup routing, document-session projections, title, media information, and toasts.
- [Actions and UI Gates](state-ownership/actions-ui-gates.md): actions, shortcuts, menu presentation, command dispatch, and UI-local gates.
- [Media Runtime Owners](state-ownership/media-runtime.md): image loading, provider-backed animated sources, video loading and playback controls, and metadata parsing.
- [Navigation and Location](state-ownership/navigation-location.md): active navigation projection, supported-media lists, sibling archives, and source identity.
- [Presentation and Viewport](state-ownership/presentation-viewport.md): KiriView's `ImageViewport` integration, command flow, media workspace composition, source resources, and failure correlation.
- [Operations and Platform Effects](state-ownership/operations-platform.md): deletion, Open With, platform capability snapshots, and owner bypass rules.
- [Preparation and Cache](state-ownership/preparation-cache.md): thumbnail strip preparation, still-image predecode, caches, and HEIF source-internal tiling.
- [Source Key Contract](state-ownership/source-keys.md): top-level source identity, direct-media freshness, and family-specific key boundaries.
