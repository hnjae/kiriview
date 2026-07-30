# Repository Component Boundary

`ImageViewport` is a repository-internal component with one consumer, KiriView. The application build compiles and links the component directly and exposes it to production QML only through an application-owned facade boundary that controls component access and lifetime. The component does not directly register or expose `ImageViewport` as a production QML type. The exact facade and child-item decomposition is not an architecture contract. The build must not install or advertise a standalone SDK, QML URI and import version, exported package target, compatibility version, or independently consumable plugin.

The component must remain a separately named build target so ownership and dependencies are explicit. KiriView production code depends only on the declared component interface; it must not include engine-private headers, provider-host transport internals, render-host types, scene graph resources, native texture handles, or instrumentation types.

## Language And Build Baseline

`ImageViewport` must follow the repository application build's ISO C++23 and toolchain baseline rather than own an independent compiler, standard-library, or Qt compatibility policy. The component implementation, supported headers, and Qt-generated translation units must compile as ISO C++23 with vendor language extensions disabled.

The named component target must publish its C++23 compile requirement to KiriView so the supported headers and their sole consumer cannot be compiled under different language modes. Every supported build configuration that compiles the component must enforce the same C++23 requirement; any locally repeated minimum toolchain or Qt value must mirror the application build authority and must not diverge from it.

KiriView and `ImageViewport` coordinate any toolchain-baseline change atomically. The component does not provide lower-standard headers, conditional C++17 or C++20 implementations, compatibility shims, or an alternate target that weakens the shared language requirement.

## Component Header Boundary

Component declarations are partitioned into five canonical interface subjects: shared values and tokens, image sequences and factories, the provider adapter contract, the viewport item and presentation commands, and state snapshots, command results, and coordinate input and result values. The supported include forms are defined by the [component-boundary specification](../spec/image-viewport-component-boundary.md). Other declarations and all implementation definitions remain private to the component.

The umbrella header is declaration-free and includes all canonical subject headers. It is a convenience entry point, not a separate declaration source. Supported subject headers remain acyclic, independently usable for their declared subjects, and free of private component declarations; their exact internal include arrangement is not an architecture contract.

Shared enums, opaque tokens, roles, ranges, and value primitives needed by more than one subject belong to shared values rather than the viewport item. State and operation values do not depend on the item declaration, provider implementations do not acquire a Qt Quick item dependency merely to use protocol values, and snapshot consumers do not acquire provider-session transport merely to observe state.

KiriView and the component must evolve atomically in one repository. A contract change must update the component interface and application integration together; it must not preserve old signatures, duplicate values, compatibility adapters, ABI shims, or versioned QML imports for hypothetical consumers.

Backend libraries are private component build dependencies unless KiriView must use one through another application-owned boundary. Backend choice does not make graphics API resources, scene graph object lifetime, texture injection, or render synchronization part of the component interface.

## Limit Authority

Source and display limits have one immutable value authority shared by the C++ and QML projections. Sequence factories, presentation-command admission, provider metadata admission, and frame preparation consume that authority rather than duplicating constants. Source logical dimensions use the full positive signed-`int` domain and their pixel-count ceiling is derived from that type authority rather than a handwritten decimal.

Payload raster and byte ceilings are separate unconditional allocation limits. Effective payload admission satisfies those ceilings and every valid tighter runtime cap for the active render environment. An unavailable runtime cap remains unavailable in provider demand and never relaxes a component limit or constrains source logical geometry. Limit checks and derived dimension, scale, pixel, duration, and byte calculations complete with overflow-safe arithmetic before allocation, payload copying, or render ownership.
