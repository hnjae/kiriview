# Post-Performance-Plan Test Cleanup Plan

## Objective

Reduce brittle, slow, and over-specified tests while preserving the repository’s useful boundary protection. Breaking changes to tests and internal test seams are allowed.

## Problems To Address

1. Several tests use fixed `QTest::qWait(...)` delays to prove that nothing happened.
2. Some architecture-boundary tests inspect source text or build-manifest tokens in ways that may over-constrain harmless refactors.
3. Some C++ tests duplicate Rust policy table coverage instead of testing only FFI/wrapper/runtime integration.
4. Some QML integration tests assert detailed geometry/timing behavior that could be covered more cheaply at runtime/model level.

## Milestone 1: Replace Fixed-Time Negative Assertions

Audit direct `QTest::qWait(...)` uses in `tests/cpp`, especially:

- `test_imagedecodejob.cpp`
- `test_imagepagesurfacecontroller.cpp`
- `test_thumbnailpanel.cpp`
- `test_predecodeloadcontroller.cpp`
- `test_imageioworkerjob.cpp`
- `test_kiridocumentsession.cpp`

For each case, decide whether the test can use:

- manual worker schedulers,
- explicit completion barriers,
- controlled fake providers,
- `QSignalSpy` with an explicit event-loop drain,
- or a runtime-owned observable counter.

Acceptance: tests no longer sleep merely to assert “nothing happened” where a deterministic seam is practical.

## Milestone 2: Split Architecture Boundary Tests By Ownership

Review `tests/cpp/test_architectureboundaries.cpp`.

Keep source-pattern tests when a match almost certainly means an architecture violation, such as QML directly mutating runtime-owned state or production code depending on forbidden render primitives.

Move or rewrite checks that mainly validate build wiring, deleted file names, or historical implementation names. Prefer a dedicated lint/check task for repository artifact policy, and prefer compile-time or behavior tests for C++ API boundaries when practical.

Acceptance: architecture tests protect durable ownership rules, not incidental filenames or current implementation layout.

## Milestone 3: Reduce Rust/C++ Policy Duplication

Audit C++ tests that mirror Rust policy tests, starting with video playback control planning.

Keep Rust as the exhaustive policy-test owner. In C++, retain only tests that prove:

- FFI conversion is correct,
- C++ wrapper names map to the expected policy calls,
- runtime code applies returned plans correctly.

Acceptance: duplicated policy tables are removed or reduced to smoke/integration coverage.

## Milestone 4: Rebalance QML Integration Tests

Audit large QML-facing tests, especially `test_thumbnailpanel.cpp`, `test_toolbarapplicationmenu.cpp`, `test_imageshortcuts.cpp`, and `test_imageviewport.cpp`.

Keep a small number of end-to-end tests for public user workflows. Move detailed scheduling, reveal, scroll, action-state, or model behavior into lower-level C++ runtime/model tests where possible.

Acceptance: QML tests verify visible integration contracts, while detailed mechanics are covered by narrower deterministic tests.

## Verification

For each milestone, run the focused affected CTest targets first. Before finishing the full cleanup, run:

- `devenv tasks run --mode single ci:test:cpp`
- `devenv tasks run --mode single ci:lint:cpp`
- `devenv tasks run --mode single ci:lint:qml` if QML tests or QML helper files changed
- `devenv tasks run --mode single ci:test` before final closeout

## Non-Goals

- Do not remove architecture boundary coverage simply because it uses regex.
- Do not weaken tests that protect stale-completion rejection, ownership boundaries, cache identity, or provider-request contracts.
- Do not add broad UI integration tests as replacements for narrow deterministic tests.
- Do not change product behavior.
