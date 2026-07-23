# Architecture

This directory defines KiriView's durable implementation contracts. It is not a product behavior specification; user-visible behavior belongs in the files under `../spec/`.

Architecture documents define ownership boundaries, dependency direction, data ownership, state-transition enforcement, async lifecycle rules, failure-handling contracts, and testability boundaries. They describe the intended end state; milestone acceptance criteria, deferred-work lists, implementation progress notes, and temporary migration rationale do not belong here. Architecture decision records with dated rationale live in `../adr/`.

Normative rules state durable boundary outcomes and invariants. They do not prescribe an internal class graph, value representation, notification choreography, dependency-injection style, or test harness unless that mechanism is itself required by a cross-component contract. Internal components may be combined, split, or replaced without an architecture change when ownership, dependency direction, lifecycle safety, and observable product behavior remain unchanged.

- [Overview](overview.md)
- [Terminology](terminology.md)
- [Layer Ownership](layer-ownership.md)
- [Rust Support Boundary](rust-support-boundary.md)
- [C++ and Qt Safety](cpp-qt-safety.md)
- [State Ownership](state-ownership.md)
    - [Application Shell and Session](state-ownership/shell-session.md)
    - [Actions and UI Gates](state-ownership/actions-ui-gates.md)
    - [Media Runtime Owners](state-ownership/media-runtime.md)
    - [Navigation and Location](state-ownership/navigation-location.md)
    - [Presentation and Viewport](state-ownership/presentation-viewport.md)
    - [Operations and Platform Effects](state-ownership/operations-platform.md)
    - [Preparation and Cache](state-ownership/preparation-cache.md)
    - [Source Key Contract](state-ownership/source-keys.md)
- [ImageViewport Integration Architecture](provider-rendering.md)
- [Thumbnail Source Adapters](thumbnail-source-adapters.md)
- [Extension Contracts](extension-contracts.md)
- [Workflow Shape](workflow-shape.md)
- [Runtime Composition](runtime-graph-composition.md)
- [Async Lifecycle](async-lifecycle.md)
- [Policy Contracts](target-direction.md)
- [Testability Boundary](testing-strategy.md)
- [Evolution Rules](evolution-rules.md)
- [Architecture Decisions](architecture-decisions.md)
