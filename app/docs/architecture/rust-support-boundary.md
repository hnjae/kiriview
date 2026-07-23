# Rust Support Boundary

The Rust support library is a repository-internal implementation adapter for capabilities provided by Rust libraries. It is not an application policy or runtime subsystem.

## Capability Scope

The support library provides only:

- Image and direct-video embedded metadata parsing.
- Streaming APNG byte decoding and raw subframe metadata.
- Self-contained static SVG parsing and rasterization that satisfies the product's SVG behavior and resource-isolation requirements.
- Desktop thumbnail-cache lookup and installation compatible with the required desktop cache contract.

Rust code may contain validation, byte conversion, capability-local adapter logic, and opaque state needed to expose those capabilities safely. Concrete libraries and their replacement rationale belong to build metadata and ADRs. A supplier change that preserves capability behavior, security constraints, payload ownership, and failure semantics does not change this boundary.

## Bridge Contract

The CXX bridge exchanges plain values and byte buffers. Capability-local opaque handles are allowed when an operation requires persistent library state, such as APNG stream decoding. Allocation ownership and failure status remain explicit, and opaque handles use generated ownership-safe interfaces.

Rust support must not own product policy, application source identity, scheduling, Qt objects, QML-facing state, or stale-completion acceptance. It does not call back into the application. Rust panics and C++ exceptions must not cross the bridge.

The calling application runtime owns invocation, scheduling, cancellation, result acceptance, conversion to Qt values, user-facing failure projection, and all other application lifecycle decisions. Native ownership, borrowing, buffer, and callback code follows [C++ And Qt Safety](cpp-qt-safety.md).

## Build Contract

The support library is linked as a repository-internal `staticlib`. It has no independent installation, dynamic loading, stable ABI, or release-version contract.
