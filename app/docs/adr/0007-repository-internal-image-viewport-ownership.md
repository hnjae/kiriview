# Repository-Internal ImageViewport Ownership

Status: Partially superseded by [ADR 0009: Non-Retained Image Target Transitions](0009-non-retained-image-target-transitions.md).

## Context

KiriView needs one presentation engine to preserve atomic page and spread transitions, physical-pixel zoom, pan and coordinate geometry, retained commits, and independently animated page roles. Keeping those responsibilities split between an application presentation runtime, QML `Flickable` and `Image` objects, page-resource owners, and a viewport library creates duplicate state authorities and makes terminal transition rollback dependent on reconstructing already retired resources.

`ImageViewport` has no second consumer. An independently installed QML module, exported CMake package, ABI boundary, and semantic-version compatibility policy would add release and adaptation constraints without isolating a real external dependency.

KiriView also owns source-specific failures whose typed detail is richer than a generic viewport provider error. Passing provider-authored diagnostic strings through the component would weaken the existing trust boundary.

## Decision

`ImageViewport` is a repository-internal component linked and registered by the KiriView application build. It has an intentional component API and focused tests, but no independent installation, QML import-version, exported package, ABI, or semantic-version compatibility contract. KiriView and the component evolve atomically.

The component is the sole owner of accepted and retained image presentation, preferred and effective zoom, pan, geometry, spread layout, per-role animation timing and loop state, render admission, and image scene graph resources. KiriView owns navigation and pairing policy, application source identity, decoding, cache and predecode policy, provider resources, scan and gesture policy, action routing, video presentation, and application projections.

KiriView supplies logical pages through the component's typed `ImageSequence` provider boundary. Payloads remain bounded complete frames; application visual tiles, QML image-provider URLs, QML load acknowledgements, and application-owned image render nodes are not integration paths.

Presentation-shape changes that must preserve the prior view use a component-owned restore-on-failure transition. The component pins the complete prior generation, provider sessions, payload handles, presentation state, and per-role playback state until the replacement commits or restores.

Provider failures cross the component as a generic typed cause plus an optional opaque application failure handle. The component exposes the handle's reference for KiriView to resolve to its own immutable typed failure record, then releases the handle exactly once when that failure observation becomes unreachable. Provider-authored free-form diagnostic text does not cross the component protocol, and the component never interprets application failure detail.

## Consequences

ADR 0006 remains authoritative for bounded whole-image provider payloads, source-internal decode tiling, and Rust-owned static SVG parsing and rasterization. Its assignment of image rendering to application-composed high-level Qt Quick image items is superseded.

The KiriView image-document graph has an integration owner and provider-resource owners, but no second image presentation or render owner. QML hosts the component item, supplies raw interaction facts, and consumes application projections without constructing the image rendering stack.

Primary and secondary animations can advance independently because playback state and scheduler effects are role-scoped inside one engine. A target-specific zoom maximum clamps effective zoom without destroying the collection-level preferred manual zoom.

The repository may change the component interface without compatibility shims, but every such change must update the KiriView integration and durable component contract in the same repository revision.
