# Testability Boundary

Deterministic verification is an architectural constraint. Owners must expose their decisions, effects, and lifecycle transitions through the same typed boundaries used at runtime; verification-only mutation paths must not bypass canonical ownership.

Qt-independent policy must operate on plain snapshots and events and return plain plans or values. It must not require a Qt event loop, filesystem, desktop service, renderer, or wall clock to exercise its contract.

Runtime owners whose behavior depends on scheduling, time, directory changes, platform operations, media backends, or rendering completion must consume those dependencies through explicit ports. Those ports must preserve the production identity, ordering, cancellation, and stale-completion rules when driven deterministically.

Boundary observations must be sufficient to detect a second durable state owner, a bypass of a named command or effect port, partial public projection publication, stale completion acceptance, or a resource handle released other than exactly once.

Verification adapters may observe or drive an owner through dedicated typed ports, but production facades and UI layers must use the same ownership and dispatch boundaries.
