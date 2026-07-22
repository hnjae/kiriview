# Testability Boundary

Deterministic verification is an architectural constraint. Owners must expose their decisions, effects, and lifecycle transitions through the same typed boundaries used at runtime; verification-only mutation paths must not bypass canonical ownership.

Qt-independent C++ policy must operate on plain snapshots and events and return plain plans or values. It must not require a Qt event loop, filesystem, desktop service, renderer, or wall clock to exercise its contract.

The Rust support boundary must expose only the capability results named by [Language Boundary](language-boundary.md) without acquiring application ownership. The C++ application must consume conversion, failure, and payload-ownership outcomes through the same FFI surface used at runtime; no verification-only FFI or ownership path may exist.

Runtime owners whose behavior depends on scheduling, time, directory changes, platform operations, media backends, or rendering completion must consume those dependencies through explicit ports. Those ports must preserve the production identity, ordering, cancellation, and stale-completion rules when driven deterministically.

Raw-buffer, FFI, codec-adapter, and cross-thread boundaries must expose enough typed ownership, failure, and synchronization state to validate the native lifetime rules in [C++ And Qt Safety](cpp-qt-safety.md) without adding a production bypass.

Boundary observations must be sufficient to detect a second durable state owner, a bypass of a named command or effect port, partial public projection publication, stale completion acceptance, or a resource handle released other than exactly once.

Verification adapters may observe or drive an owner through dedicated typed ports, but production facades and UI layers must use the same ownership and dispatch boundaries.
