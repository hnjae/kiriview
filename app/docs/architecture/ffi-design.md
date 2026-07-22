# FFI Design

FFI code must be explicit, typed, and audit-friendly.

The bridge exposes only the canonical Rust support capabilities defined by [Language Boundary](language-boundary.md). Use small bridge structs, enums, and opaque handles for:

- Embedded metadata inputs and parsed metadata results.
- APNG decoder construction, stream input, animation metadata, frame controls, and raw subframe payloads.
- SVG source bytes, intrinsic dimensions, target dimensions, and raster payloads.
- Thumbnail-cache identity, lookup, installation, and explicit result status.

The bridge does not expose general application policy, workflow snapshots, state deltas, navigation plans, cache policy, or effect descriptions. Those are native C++ contracts even when they are Qt-independent.

Avoid bridges that expose:

- Raw Qt object ownership.
- Long-lived Rust references to C++ objects.
- Rust-to-C++ callbacks.
- Application owner tokens, QObject affinity, source generations, or QML-facing state.
- Implicit I/O or cache mutation hidden behind operations that are not explicitly named as lookup or installation.
- One-off wrappers whose only purpose is to move a local C++ decision into Rust.

The bridge must preserve allocation ownership and failure status across every call. Rust panics and C++ exceptions must not cross the boundary; support functions return explicit failure results, and opaque Rust handles are released through generated ownership-safe interfaces.

Rust bridge exposure is an explicit allowlist within support-library ownership. Source colocation or Cargo discovery must not make an internal helper callable across FFI without an intentional capability contract.

C++ conversion helpers for Rust bridge values are value-conversion adapters only. They must stay limited to conversion between Qt/C++ types and generated Rust bridge types; they must not own runtime state, execute application side effects, or hide product policy decisions.

The support library is a repository-internal `staticlib` consumed by the CMake application build. Static linkage removes any independent dynamic-library ABI promise but does not relax the typed boundary or ownership rules.
