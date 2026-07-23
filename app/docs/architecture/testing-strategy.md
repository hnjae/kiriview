# Testability Boundary

Deterministic verification is an architectural constraint for durable policy and lifecycle behavior. Tests may use focused adapters or instrumentation, but verification-only mutation must not create a production path that bypasses canonical ownership.

Policy without Qt runtime dependencies must accept explicit owned inputs and return explicit decisions or values. It must not require a Qt event loop, filesystem, desktop service, renderer, or wall clock to exercise its contract.

The Rust support bridge must expose only the capability results named by [Rust Support Boundary](rust-support-boundary.md) without acquiring application ownership. The application must consume conversion, failure, and payload-ownership outcomes through the same bridge used at runtime; no verification-only bridge or ownership path may exist.

Runtime behavior that depends on scheduling, time, directory changes, platform operations, media backends, or rendering completion must be controllable at an appropriate boundary for deterministic verification. The test boundary preserves production identity, ordering, cancellation, and stale-completion semantics without prescribing one dependency-injection style.

Raw-buffer, FFI, codec-adapter, and cross-thread boundaries expose outcomes sufficient to validate ownership, failure, and synchronization rules from [C++ And Qt Safety](cpp-qt-safety.md) without adding a production bypass.

Verification must detect contract failures such as partial public projection publication, stale completion acceptance, unsafe lifetime use, or incorrect resource release. Tests need not freeze an internal component graph, call path, private type, or notification count that is not itself a durable contract.

Verification adapters may observe or drive an owner through focused interfaces or test-local instrumentation. Production facades and UI layers continue to use the declared ownership and dispatch boundaries.
