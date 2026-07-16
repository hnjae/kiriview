# Testing Strategy

This document owns boundary-test placement for architecture contracts. It does not replace focused behavior tests in the implementation layer that owns a feature.

## Policy And Runtime Tests

Test Rust policy in Rust unit tests when the logic is independent of Qt. These tests cover state transitions, edge cases, and policy tables.

Test C++ runtime code with Qt tests when behavior depends on Qt object lifetime or signals, `QImage`, `QUrl`, display-source data, render-context capability data, KIO or file-operation adapters, or controller integration across async boundaries.

C++ tests do not duplicate every Rust policy test. They verify that the runtime layer applies plans correctly and preserves integration behavior at the Qt boundary.

## Boundary Enforcement Tests

Boundary tests must fail when production code reintroduces a second owner for durable public state or bypasses the owner named by architecture.

QML and facade boundary tests cover shared action state, media readiness, viewport command acknowledgment, shortcut routing, display-load acknowledgment, and cache or render-policy ownership. They verify that QML reports UI facts and renders accepted projections without becoming an alternate domain owner.

Runtime owner-bypass tests cover route mutation, presentation mutation, stale UI-gate acceptance, partial public projection updates, and cross-owner calls that bypass named ports.

Provider and render boundary tests cover cache-only requests, immutable entry lifetime, typed identity families, display-source stale rejection, render-context freshness, bounded whole-image publication, and exclusion of custom visual-tile or low-level rendering paths.

Candidate snapshot tests cover reuse by source identity and list revision, same-source refresh retention, source-change invalidation, current-row-only updates, and stale scope generations.

Extension contract tests cover unsupported capabilities, typed failures, stale keys or generations, cache identity, opened-collection freshness, video-versus-image eligibility, and provider publication boundaries.

Prefer focused C++ tests where the contract depends on Qt URL, image, object-lifetime, or display data, and Rust tests where the logic is plain value policy.
