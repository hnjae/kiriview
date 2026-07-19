# FFI Design

FFI code must be explicit, typed, and audit-friendly.

Use small bridge structs and enums for:

- Stable policy inputs.
- State snapshots.
- Change sets.
- Effect plans.

Policy inputs describe capabilities and source-access profiles rather than concrete storage backends. C++ may translate runtime source facts into plain policy values, but Rust policy must not branch on backend implementation names when the decision depends only on those capabilities.

Avoid bridges that expose:

- Raw Qt object ownership.
- Long-lived Rust references to C++ objects.
- Implicit side effects hidden behind policy functions.
- One-off wrappers whose only purpose is to move a local C++ `switch` into Rust.

The bridge must not expose isolated policy branches whose bridge types and conversions are larger than the decision. Such branches remain with their C++ runtime owner unless they form part of a coherent Rust workflow or algorithm.

Rust bridge exposure is an explicit allowlist within Rust policy ownership. Source colocation or build discovery must not make a policy module callable across FFI without an intentional bridge contract.

C++ conversion helpers for Rust bridge values are value-conversion adapters only. They must stay limited to conversion between Qt/C++ types and generated Rust bridge types; they must not own runtime state, execute side effects, or hide policy decisions.
