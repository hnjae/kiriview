# Testing Strategy

This document owns boundary-test placement for architecture contracts. It does not replace focused behavior tests in the implementation layer that owns a feature.

## Policy And Runtime Tests

Test Rust policy in Rust unit tests when the logic is independent of Qt. These tests cover state transitions, edge cases, and policy tables.

Test C++ runtime code with Qt tests when behavior depends on Qt object lifetime or signals, `QImage`, `QUrl`, display-source data, render-context capability data, KIO or file-operation adapters, or controller integration across async boundaries.

C++ tests do not duplicate every Rust policy test. They verify that the runtime layer applies plans correctly and preserves integration behavior at the Qt boundary.

## Boundary Enforcement Tests

Boundary tests must fail when production code reintroduces a second owner for durable public state or bypasses the owner named by architecture.

The QML and facade boundary tests cover shared action state, action proxy state, active media readiness, viewport command acknowledgment, shortcut routing, route-state mutation, display-load acknowledgment, provider ids, and cache or render policy ownership. QML may report UI facts and render projections, but it must not own these values.

Runtime owner-bypass tests cover direct leaf-route state mutation, public image-document mutators that bypass presentation owners, page presentation controllers retaining mutable active zoom or rotation owners, action runtime accepting stale UI gate revisions, projection owners querying arbitrary leaf facades while applying named projection snapshots, and callbacks that capture sibling controllers instead of crossing named ports.

Provider and render boundary tests cover cache-only provider requests, immutable display-entry lifetime, display-source stale-completion rejection, typed key families across adapter/display boundaries, render-context freshness after window, DPR, scene-graph, or texture-capability changes, and absence of custom image render nodes, visual tile fallbacks, low-level rendering resource paths, and generic source-key APIs at production extension boundaries.

Candidate snapshot tests cover reuse by source identity and candidate-list revision, current-row-only updates that do not force full row projection, stale scope generations that reject async completions, and preservation of narrow candidate-snapshot APIs on hot paths.

Extension contract tests cover unsupported media, stale source keys or generations, cache identity, opened-collection entry freshness, video-versus-image eligibility, provider request boundaries, display-entry lifetime, and display-source stale-completion rejection.

Prefer focused C++ tests where the contract depends on Qt URL, image, object-lifetime, or display data, and Rust tests where the logic is plain value policy.
