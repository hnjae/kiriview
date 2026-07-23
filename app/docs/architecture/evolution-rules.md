# Evolution Rules

The architecture must preserve these invariants as its internal structure evolves:

1. Authoritative mutable workflow state has one canonical owner; derived projections and caches must not become alternate authorities.
1. The Rust support library remains a capability adapter: product policy, authoritative application state, source identity, scheduling, lifecycle decisions, and result acceptance remain with named application owners, as defined by [Rust Support Boundary](rust-support-boundary.md).
1. Native and asynchronous paths preserve explicit ownership, valid lifetimes, thread affinity, and owned cross-boundary payloads as defined by [C++ And Qt Safety](cpp-qt-safety.md) and [Async Lifecycle](async-lifecycle.md).
1. QML-facing facades expose application APIs without becoming owners of domain state or workflow policy.
1. KiriView architecture constrains dependencies only through their supported interfaces; it does not freeze private state, algorithms, scheduling, rendering, resource management, or types.
1. Internal formats have one authoritative schema unless an explicit product or interoperability contract requires compatibility behavior.
