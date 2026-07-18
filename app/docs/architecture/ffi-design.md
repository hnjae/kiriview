# FFI Design

FFI code must be explicit, typed, and audit-friendly.

KiriView's internal C++ and CXX bridge namespace is `kiriview`. Qt type names, generated KConfig classes, QML facade names, and visible product strings may remain PascalCase when that matches Qt or branding conventions.

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

When a Rust module starts to look like glue, either move the branch back to C++ or absorb it into a larger Rust workflow reducer where it becomes part of a coherent policy decision.

Rust bridge exposure is an explicit subset of Rust policy ownership. When tooling requires bridge sources to share a policy source root, domain ownership must remain visible through module names, bridge type names, tests, and architecture contracts. Source manifests must distinguish ordinary Rust policy sources from CXX-exposed bridge sources so adding a policy module does not automatically expose it across the FFI boundary.

C++ conversion helpers for Rust bridge values are value-conversion adapters only. They must stay limited to conversion between Qt/C++ types and generated Rust bridge types; they must not own runtime state, execute side effects, or hide policy decisions.
