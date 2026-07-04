# Implementation Plan

## Purpose

This plan consolidates temporary design review findings into executable implementation work for a future `/goal` session. It is self-contained, converts each finding into incremental engineering milestones, and sequences characterization, behavior fixes, and refactors so the repository should not be left with a long-lived red test suite.

## Source Basis

The durable source basis for this plan is:

- `docs/spec/**`
- `docs/architecture/**`

`DESIGN_REVIEW_CORRECT_END_STATE.md` was used as a temporary input, fully consolidated into this plan, and is no longer required for future execution.

No temporary design review finding was overridden by the authoritative specs or architecture documents during consolidation. The design review reported that `docs/spec/**` and `docs/architecture/**` were internally consistent in the reviewed areas. If future implementation discovers a conflict, `docs/spec/**` and `docs/architecture/**` win, the conflicting task must pause, and this plan must be updated with the authoritative resolution before code changes continue.

## Execution Principles

- Preserve current behavior unless the change fixes a documented bug, a spec violation, or an architecture-boundary violation described in this plan.
- Add characterization tests immediately before the risky implementation they protect; confirm the focused test fails or exposes the current gap, then complete the paired implementation before the milestone is done.
- Do not leave intentionally failing end-state tests in the normal suite at a milestone boundary. If a test must be committed before its fix, mark or isolate it explicitly and document the focused exclusion in this plan before running full-suite verification.
- Keep changes incremental; each milestone should be independently reviewable and should leave the project buildable with `ctest --test-dir build --output-on-failure` passing unless the plan is amended with an explicit temporary exception.
- Keep `docs/spec/**` and `docs/architecture/**` immutable while executing this plan.
- Do not add, remove, or reshape public API in this plan unless a specific milestone cannot meet its documented behavior without that API change; if that happens, pause and amend this plan with a narrowly scoped, docs-backed API task before changing code.
- Isolate pure controller/domain logic from external effects such as provider transport, QObject lifetime, QML notifications, scene graph render synchronization, timers, clocks, filesystem, network, and randomness.
- Prefer role-parametric helpers for the existing primary and secondary roles, but do not introduce a generic multi-role framework.
- Keep provider, preparation, render, and command diagnostics typed; do not branch implementation logic on public diagnostic strings.
- Before focused verification, run `ctest -N --test-dir build` or equivalent and confirm the focused `ctest -R ...` expression selects the expected test executable names.
- Update this plan if implementation discovers a blocker, an invalid assumption, or a finding that is too vague to implement safely.

## Consolidated Design Findings

### Clear-style operations are not true clear transactions

- Priority: P0
- Affected areas: `src/imageviewport.cpp`, `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewportcontroller.cpp`, `src/imageviewportprovider.cpp`, provider lifecycle tests, public API tests.
- Correct end state: `setPageSet(null, null, policy)`, `setPageSet(null, secondary, policy)`, compatibility null assignment, and `clear()` are one controller-owned clear transaction after page-role argument and policy validation. They ignore the supplied secondary role, ignore transition effects, close and retire provider generations for every accepted role, stop playback, clear retained and displayed content, publish `NoRequest` and `Empty`, publish empty geometry observations, and preserve presentation preferences unless a separate presentation command changes them.
- Why it matters: The public spec states that a secondary role without a primary is not presentable and that clear-style transition fields are validated but have no presentation effect. Current behavior can derive provider side effects from a secondary argument that should be ignored, mutate presentation state while clearing, and leave secondary provider sessions or tokens alive after public state says the viewport is empty.

### Provider role semantics are split between primary and secondary paths

- Priority: P1
- Affected areas: `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewportprovider.cpp`, `src/imageviewportcontroller.cpp`, `src/imageviewport_p.h`, provider metadata/request/playback/terminal tests.
- Correct end state: Provider generation state, construction facts, metadata admission, target selection, play, seek, position seek, token allocation, terminal projection, cancellation, close, and stale-result filtering are role-parametric. Primary and secondary provider sessions use the same request, cancellation, close, payload ownership, progress, terminal-result, and end-of-sequence paths. Role differences are limited to page placement, role-scoped observations, and aggregate spread projection.
- Why it matters: Authoritative docs require primary and secondary provider-backed page roles to support the same provider session API and result channel. Divergent secondary shortcuts risk accepting malformed metadata, rejecting valid secondary commands, selecting the wrong target after metadata, and fixing provider bugs only for primary.

### Role-scoped command admission and command diagnostics are split

- Priority: P1
- Affected areas: `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewport_p.h`, `src/imageviewportcontroller.cpp`, role command tests in `tests/tst_imageviewport_public_api.cpp`, `tests/tst_imageviewport_still.cpp`, `tests/tst_imageviewport_timed.cpp`, and provider request/playback tests.
- Correct end state: The item normalizes public value types only. The controller owns role presence, intrinsic command-domain validation, failure scope, capability support, command diagnostics, request identity, playback effects, and provider transport effects for `play`, `pause`, `stop`, `seek`, and `seekToPosition` across both roles. Every typed command that reaches the viewport boundary and returns `Invalid`, `Unsupported`, or `IgnoredNoRequest` updates `commandReason` and `commandRevision` unless the public contract documents diagnostic preservation for that exact command.
- Why it matters: Current item-private wrappers can return outcomes directly and bypass controller diagnostic revision updates, leaving stale `commandReason` for QML controls and tests.

### Provider transport failures can strand accepted requests

- Priority: P1
- Affected areas: `src/viewportproviderbridge.cpp`, `src/viewportproviderbridge_p.h`, `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewportprovider.cpp`, provider lifecycle and request tests.
- Correct end state: Failed delivery of a controller-authorized provider command becomes a typed controller event. The active generation/request reports `Error` with `ProviderFailure`, bounded diagnostics are published, provider interest is closed, active tokens are retired, and the request cannot remain indefinitely in `Loading` with `ProviderWaiting`.
- Why it matters: The controller can allocate tokens and publish provider waiting before the bridge attempts queued delivery. If the session is null or `invokeMethod` fails and the bridge silently drops the command, callers observe a request waiting for work the provider never received.

### Geometry has multiple runtime authorities

- Priority: P1
- Affected areas: `src/presentationgeometry.cpp`, `src/presentationgeometry_p.h`, `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewportpresentation.cpp`, `src/imageviewportrender.cpp`, presentation and render tests.
- Correct end state: One controller-authored geometry projection includes accepted or pending role payload sizes, spread placement, effective device pixel ratio, fit/zoom/pan state, page gap, spread direction, visible rectangles, page rectangles, content size/position, pannability, scan state, and coordinate mappings. Public getters and render consume that projection rather than reconstructing `PresentationGeometry::State` from item, sequence, provider, or render-local mutable state.
- Why it matters: Fit, manual zoom, pan, scan, page gap, spread direction, and coordinate conversion can diverge between controller commands, public observations, and render target rectangles, especially for two-page spreads and non-1 device-pixel-ratio displays.

### Render path rebuilds target spread decisions from mutable state

- Priority: P1
- Affected areas: `src/imageviewportrender.cpp`, `src/renderadapter.cpp`, `src/renderadapter_p.h`, `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, render scenegraph/commit tests, presentation tests.
- Correct end state: The controller produces an immutable render snapshot containing the target spread, role payload identities, role images or prepared handles, per-role source and target rectangles, presentation mapping, background, and quality inputs. `ImageViewportPrivate::updatePaintNode()` becomes snapshot-to-render-adapter glue and does not reconstruct secondary layers, clamp secondary frames, query sequences, or read requested/displayed frame state after snapshot creation.
- Why it matters: Render code currently makes fallback, clamping, and layer assembly decisions that belong to the controller/preparation boundary. That weakens stale-result reasoning and makes render acknowledgements depend on identities assembled outside the controller event boundary.

### Spread render acknowledgement is not complete-role scoped

- Priority: P2
- Affected areas: `src/renderadapter.cpp`, `src/renderadapter_p.h`, `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewportrender.cpp`, render commit tests.
- Correct end state: Render snapshots and acknowledgements identify the target spread plus every required role payload. The render adapter reports either a complete spread commit or a role-scoped failure identity, and the controller validates every required current role identity before publishing `Ready`.
- Why it matters: A two-page spread is rendered as multiple image layers but currently acknowledged through a single payload identity. This conflicts with the documented complete-role-set render boundary and increases risk that a stale or failed secondary role could be misclassified.

### Controller depends on item-private context and ambient mutable reads

- Priority: P1
- Affected areas: `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewport_p.h`, `src/imageviewportcontroller.cpp`, `src/framepreparation.cpp`, `src/framepreparation_p.h`, controller tests.
- Correct end state: `ViewportController` compiles without including or referring to `ImageViewportPrivate`. It consumes explicit snapshots or immutable facts for page-set assignment, provider generation facts, geometry input, prepared payload identities, render snapshots, and time inputs. Item-private code adapts public calls, applies controller outputs, delivers provider transport, schedules render updates, manages timers, and emits QML notifications.
- Why it matters: Controller decisions currently consult a broad mutable Qt-facing context and even call item-private helpers. That blurs ownership and makes high-value state-machine behavior difficult to test from explicit inputs.

### Provider protocol coverage is too dependent on Qt event delivery

- Priority: P1
- Affected areas: controller-level test support, `tests/imageviewport_provider_test_support.h`, provider lifecycle/metadata/requests/terminal/playback tests, possible new controller provider harness.
- Correct end state: Core provider protocol policy is tested by feeding normalized provider events and transport effects directly to the controller. A smaller bridge suite remains for QObject delivery, affinity, cleanup, and failed command delivery.
- Why it matters: Many policy tests currently depend on real QObject sessions, queued signals, event draining, and thread-affinity behavior. That makes provider refactors slower and order-sensitive, and it mixes controller policy failures with transport delivery failures.

### Provider role behavior is hard to remove or change independently

- Priority: P2
- Affected areas: `src/viewportcontroller_p.h`, `src/viewportcontroller.cpp`, `src/imageviewportprovider.cpp`, `src/imageviewportcontroller.cpp`, provider-related tests.
- Correct end state: Provider behavior is modularized around a small role-indexed provider pipeline and consistent provider transport/result shapes for assignment, commands, playback, clear, replacement, and shutdown. New provider lifecycle or terminal behavior is implemented once for `PageRole` and covered by role-parametric tests.
- Why it matters: Current primary and secondary state/result shapes differ, so changing provider behavior requires auditing multiple role-specific side paths.

## Milestones

### Milestone 1: Baseline Verification And Test Selection

#### Objective

Establish the build and test commands, record exact focused test names, and set the verification rules that later milestones must follow.

#### Source findings

Provider protocol coverage is too dependent on Qt event delivery; all findings need safe characterization before behavior changes.

#### Prerequisites

- None.

#### Scope

- Configure and build tests.
- Run `ctest -N --test-dir build` and record the exact test executable names available in this checkout.
- Confirm focused `ctest -R` expressions select the expected test names before any behavior work begins.
- Decide the local convention for short-lived failing characterization: add the test, run the focused test to confirm the current gap, implement the fix in the same milestone, and leave full `ctest` passing.

#### Out of scope

- Do not add end-state tests for all findings in this milestone.
- Do not change production source behavior.
- Do not modify `docs/spec/**` or `docs/architecture/**`.

#### Likely affected areas

- `docs/implementation-plan.md` only if command discovery changes the plan.
- No source or test file changes are expected unless this milestone uncovers stale test documentation inside the plan.

#### Tasks

- [x] Run `cmake -S . -B build -DIMAGEVIEWPORT_BUILD_TESTS=ON -DIMAGEVIEWPORT_BUILD_EXAMPLES=OFF`.
- [x] Run `cmake --build build`.
- [x] Run `ctest -N --test-dir build` and confirm the expected tests are present: `imageviewport_public_api`, `imagesequence_factory`, `playback_clock`, `playback_timeline`, `viewportcontroller_playback`, `viewportcontroller_presentation`, `imageviewport_still`, `imageviewport_timed`, `imageviewport_provider_contract`, `imageviewport_provider_lifecycle`, `imageviewport_provider_metadata`, `imageviewport_provider_requests`, `imageviewport_provider_frame_admission`, `imageviewport_provider_terminal`, `imageviewport_provider_playback`, `imageviewport_render_scenegraph`, `imageviewport_render_commit`, `imageviewport_presentation_state`, and `imageviewport_install_consumer`.
- [x] Run `ctest --test-dir build --output-on-failure` and record any pre-existing failures before starting behavior work.
- [x] For each later milestone, confirm its focused `ctest -R` filter selects at least one expected test before using it as verification.
- [x] If a focused filter selects zero tests, stop and correct the filter or the plan before implementation proceeds.

#### Acceptance criteria

- The repository configures, builds, and has known CTest names.
- Full-suite baseline status is known before implementation starts.
- Later milestones have an explicit rule that focused verification must not silently select zero tests.

#### Verification

- `cmake -S . -B build -DIMAGEVIEWPORT_BUILD_TESTS=ON -DIMAGEVIEWPORT_BUILD_EXAMPLES=OFF`
- `cmake --build build`
- `ctest -N --test-dir build`
- `ctest --test-dir build --output-on-failure`

#### Implementation notes

- Completed on 2026-07-04 after regenerating `build`; stale pre-configure CTest output listed only `imageviewport` and `imageviewport_install_consumer`, while the fresh configure produced all 19 expected test names.
- Baseline full-suite result: `ctest --test-dir build --output-on-failure` passed, 19/19 tests.
- Milestone 2 focused filter was checked as a sample of the rule: `ctest -N --test-dir build -R '^(imageviewport_public_api|imageviewport_provider_lifecycle|imageviewport_provider_requests|imageviewport_provider_playback)$'` selected the expected 4 tests.

#### Risks / notes

- If the local build directory uses a different name, update only the command examples in this plan or record the alternate local command in implementation notes.

### Milestone 2: Clear Transaction Characterization And Fix

#### Objective

Make clear-style page-set operations and `clear()` one atomic controller transaction that preserves presentation preferences and closes all accepted provider-role generations.

#### Source findings

Clear-style operations are not true clear transactions; provider role behavior is hard to remove or change independently; provider role semantics are split between primary and secondary paths.

#### Prerequisites

- Milestone 1 complete.
- Focused test filters for `imageviewport_public_api`, `imageviewport_provider_lifecycle`, `imageviewport_provider_requests`, and `imageviewport_provider_playback` confirmed with `ctest -N --test-dir build`.

#### Scope

- Add focused characterization tests for clear-style behavior at the start of this milestone.
- Normalize null-primary page-set assignment before transition effects or provider work.
- Validate page-role arguments and transition policy before clearing.
- Ignore any validated secondary argument for clear-style operations.
- Preserve presentation preferences during clear-style operations.
- Close and retire primary and secondary provider sessions/tokens through the same lifecycle shape.
- Return provider transport effects for every role that needs cancellation or close.

#### Out of scope

- Do not refactor the full provider pipeline beyond the lifecycle shape needed for clear correctness.
- Do not change non-empty page-set transition semantics except where shared code must preserve existing behavior.
- Do not change public API shape.

#### Likely affected areas

- `src/imageviewport.cpp`
- `src/imageviewport_p.h`
- `src/imageviewportcontroller.cpp`
- `src/imageviewportprovider.cpp`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`

#### Tasks

- [x] Add characterization tests for `setPageSet(null, providerSecondary, validPolicy)`: outcome `Accepted`, no secondary provider command/session startup, both accepted role observations null, retained content removed, `requestStatus: NoRequest`, `displayStatus: Empty`, and empty geometry observations.
- [x] Add characterization tests for `setPageSet(null, null, policyWithResetAndExplicitFields)`: transition fields validate but preserve fit, manual zoom, effective zoom preference where applicable, content position preference, rotation, mirror flags, spread direction, page gap, scan preference, background, quality preferences, and looping.
- [x] Add characterization tests for `clear()` with primary and secondary provider work active: primary and secondary tokens are cancelled or retired, sessions are closed exactly once, playback stops, retained content is removed, accepted roles are null, request/display/geometry observations are empty, and presentation preferences are preserved.
- [x] Add characterization tests proving late secondary metadata, frame, terminal, waiting/progress, cancellation, and end-of-sequence callbacks after clear cannot change request status, display status, diagnostics, playback phase, or revisions.
- [x] In `ImageViewportPrivate::setPageSet(...)`, ensure null-primary assignment does not preserve a secondary provider flag or secondary sequence as request intent after argument validation.
- [x] In `ViewportController::assignSequence(...)`, branch to clear-style handling after policy validation and before `applyPresentationTransition(...)` or provider request startup.
- [x] Ensure clear-style handling ignores transition effects for display transition, zoom, fit mode, content position, rotation, mirror flags, spread direction, page gap, scan state, and replacement intent.
- [x] Introduce or reuse a role-local provider close helper so primary and secondary provider generations retire active metadata/frame/playback tokens, queued provider work, and session serials consistently.
- [x] Extend clear and assignment result transport shapes as needed so secondary cancellation/close effects are delivered by item-private provider transport.
- [x] Keep compatibility `sequence = null`, `setPageSet(null, null)`, `setPageSet(null, null, policy)`, `setPageSet(null, secondary, policy)`, and `clear()` behavior aligned with the public clear-style contract.

#### Acceptance criteria

- `setPageSet(null, providerSecondary, validPolicy)` returns `Accepted`, performs no provider session request for the supplied secondary, leaves `primarySequence` and `secondarySequence` null, removes retained display content, publishes `requestStatus: NoRequest`, `displayStatus: Empty`, and empty public geometry observations.
- `setPageSet(null, null, policyWithResetAndExplicitFields)` validates the policy but preserves fit mode, manual zoom percent, effective zoom preference where applicable, content position preference, rotation, mirror flags, spread direction, page gap, scan preference, background, quality preferences, and looping.
- `clear()` leaves `primarySequence` and `secondarySequence` null, publishes empty request/display/geometry observations, stops playback, removes retained content, preserves presentation preferences, and closes all accepted provider-role generations exactly once.
- Late provider callbacks for any role after clear are ignored without changing public state.
- Invalid clear-style policy preserves the prior accepted page set, presentation state, retained pixels, playback phase, provider/session ownership, request/display revisions, and request diagnostics while publishing command diagnostics according to the public contract.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_public_api|imageviewport_provider_lifecycle|imageviewport_provider_requests|imageviewport_provider_playback)$'`
- `ctest --test-dir build -R '^(imageviewport_public_api|imageviewport_provider_lifecycle|imageviewport_provider_requests|imageviewport_provider_playback)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Implementation notes

- Completed on 2026-07-04. Characterization failures before implementation were `imageviewport_public_api` for secondary-provider clear-style startup and transition-field mutation, plus `imageviewport_provider_lifecycle` for missing secondary provider cancellation/close on clear.
- Compatibility `sequence = null` now routes through the same controller clear transaction as `clear()`, so the stale still-image test expectation that command diagnostics were preserved was updated to expect diagnostics to clear.
- Verification passed: `cmake --build build`; `ctest -N --test-dir build`; focused filter selected 4 tests; focused CTest passed 4/4; full CTest passed 19/19.

#### Risks / notes

- This milestone touches lifecycle and public state at the same time. Keep provider close/retire helpers small and role-local rather than starting the full role-parametric provider refactor here.

### Milestone 3: Mechanical Controller/Item Separation Precursor

#### Objective

Perform behavior-preserving controller/item file separation before command and provider behavior changes, so later diffs are easier to isolate.

#### Source findings

Controller depends on item-private context and ambient mutable reads; role-scoped command admission and command diagnostics are split.

#### Prerequisites

- Milestone 1 complete.
- Milestone 2 complete or explicitly deferred only if clear changes are blocked by an unrelated build/test issue.

#### Scope

- Move item-private public command wrapper definitions and item adaptation code out of `src/viewportcontroller.cpp`.
- Replace controller references to item-private diagnostic helpers with neutral helpers.
- Keep behavior unchanged.

#### Out of scope

- Do not change command admission semantics in this milestone.
- Do not shrink the full `ViewportControllerContext` yet.
- Do not change provider, render, or geometry behavior.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportcontroller.cpp`
- `src/imageviewportprovider.cpp`
- `src/imageviewport_p.h`
- `src/framepreparation.cpp`
- `src/framepreparation_p.h`
- Existing tests only.

#### Tasks

- [x] Move `ImageViewportPrivate` command wrapper definitions currently implemented in `src/viewportcontroller.cpp` into item/private implementation files such as `src/imageviewportcontroller.cpp`, preserving call order and result application behavior.
- [x] Replace controller calls to `ImageViewportPrivate::boundedDiagnostic(...)` with `FramePreparation::boundedDiagnostic(...)` or another neutral helper that does not require `ImageViewportPrivate`.
- [x] Remove any unnecessary `imageviewport_p.h` include from `src/viewportcontroller.cpp` if possible without changing behavior.
- [x] Run focused command, provider, and public API tests to prove the move was mechanical.

#### Acceptance criteria

- Public command outcomes, diagnostics, request/display state, provider transport behavior, and render behavior are unchanged by this milestone.
- `src/viewportcontroller.cpp` no longer contains item-private wrapper method definitions.
- Any remaining `ImageViewportPrivate` references in controller code are documented as deferred to the later controller boundary milestone.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_public_api|imageviewport_still|imageviewport_timed|imageviewport_provider_requests|imageviewport_provider_playback|viewportcontroller_playback)$'`
- `ctest --test-dir build -R '^(imageviewport_public_api|imageviewport_still|imageviewport_timed|imageviewport_provider_requests|imageviewport_provider_playback|viewportcontroller_playback)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Implementation notes

- Completed on 2026-07-04 as a mechanical move: `ImageViewportPrivate` command wrappers, controller change application, and item-private test-probe wrappers moved from `src/viewportcontroller.cpp` to `src/imageviewportcontroller.cpp`.
- `src/viewportcontroller.cpp` now uses `FramePreparation::boundedDiagnostic(...)` directly and no longer includes `imageviewport_p.h` or references `ImageViewportPrivate`.
- Verification passed: `cmake --build build`; `ctest -N --test-dir build`; focused filter selected 6 tests; focused CTest passed 6/6; full CTest passed 19/19.

#### Risks / notes

- This is a mechanical precursor. If a behavior assertion changes, revert the mechanical shape within this milestone and split the move into smaller patches.

### Milestone 4: Command Diagnostics For Invalid And Absent Roles

#### Objective

Move invalid-role and absent-role command diagnostics into the controller without prematurely accepting valid secondary provider commands before role-symmetric dispatch exists.

#### Source findings

Role-scoped command admission and command diagnostics are split; controller depends on item-private context and ambient mutable reads.

#### Prerequisites

- Milestones 1 and 3 complete.
- Focused test filters for role command tests confirmed with `ctest -N --test-dir build`.

#### Scope

- Add focused characterization tests for invalid role values and absent role commands.
- Add controller-owned diagnostic paths for `Invalid` and `IgnoredNoRequest`.
- Thin item-private wrappers for invalid/absent-role cases.
- Preserve existing valid secondary built-in and valid secondary provider command behavior until provider dispatch is unified in later milestones.

#### Out of scope

- Do not route valid secondary provider `play`, `seek`, or `seekToPosition` through a new accepted path unless dispatch and transport effects are implemented in the same provider-pipeline milestone.
- Do not change provider metadata, frame, playback, terminal, cancellation, or stale-result policy here.
- Do not change accepted no-op behavior for present non-driver `pause(role)` and `stop(role)` beyond documented diagnostic clearing.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewport_p.h`
- `src/imageviewportcontroller.cpp`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/tst_imageviewport_still.cpp`
- `tests/tst_imageviewport_timed.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_viewportcontroller_playback.cpp`

#### Tasks

- [ ] Add characterization tests for invalid role values across `play`, `pause`, `stop`, `seek`, and `seekToPosition`: expected outcome, `commandReason`, `commandRevision`, unchanged request revision, and unchanged display revision.
- [ ] Add characterization tests for absent secondary-role commands across `play`, `pause`, `stop`, `seek`, and `seekToPosition`: expected `IgnoredNoRequest`, `commandReason`, `commandRevision`, unchanged request revision, and unchanged display revision.
- [ ] Add a controller helper for `IgnoredNoRequest` that updates `commandReason` and `commandRevision` consistently with invalid and unsupported command helpers.
- [ ] Route invalid enum values that reach the typed viewport boundary through controller invalid-command diagnostics.
- [ ] Route absent primary or secondary role commands through the controller `IgnoredNoRequest` path.
- [ ] Preserve validation ordering for the covered cases: malformed role/value first, absent role next, then leave present-role domain/capability dispatch to existing behavior or later provider milestones.
- [ ] Ensure command-only failures leave request status, request reason, accepted target, display status, retained content, playback phase, provider work, request revision, and display revision unchanged.

#### Acceptance criteria

- Invalid role values and absent-role commands update `commandReason` and advance `commandRevision` for `play`, `pause`, `stop`, `seek`, and `seekToPosition`.
- Repeating the same invalid or ignored typed command advances `commandRevision` as specified for command diagnostic generations.
- Request and display revision tokens remain unchanged for rejected role commands that do not accept a new request or presentation change.
- Valid secondary provider commands retain their pre-milestone behavior until the provider dispatch milestones implement role-symmetric handling.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_public_api|imageviewport_still|imageviewport_timed|imageviewport_provider_requests|imageviewport_provider_playback|viewportcontroller_playback)$'`
- `ctest --test-dir build -R '^(imageviewport_public_api|imageviewport_still|imageviewport_timed|imageviewport_provider_requests|imageviewport_provider_playback|viewportcontroller_playback)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Avoid a partial state where a valid secondary provider command is admitted by a new controller path but cannot produce the correct role transport effect.

### Milestone 5: Provider Harness And Dispatch-Failure Foundation

#### Objective

Create the minimum provider-policy test harness and transport failure contract needed before role-symmetric provider dispatch is refactored.

#### Source findings

Provider protocol coverage is too dependent on Qt event delivery; provider transport failures can strand accepted requests; provider role semantics are split between primary and secondary paths.

#### Prerequisites

- Milestone 1 complete.
- If no direct controller provider harness can be built safely, stop and update `Deferred / Needs Investigation` before changing metadata, token, terminal, cancellation, or stale-result policy.

#### Scope

- Add a controller-level provider harness that can feed normalized provider events and inspect controller-authorized transport effects without constructing `ImageViewport`, `ImageSequenceProviderSession`, `QCoreApplication::processEvents`, or `QThread` for core policy cases.
- Add bridge delivery success/failure return shape and controller dispatch-failure input before provider dispatch unification.
- Implement provider command delivery failure classification for active primary and secondary requests.
- Keep bridge-only tests for QObject delivery and threading behavior.

#### Out of scope

- Do not refactor provider metadata/seek/playback policy beyond what is required for the harness and dispatch-failure event.
- Do not retry provider commands.
- Do not change provider public session method signatures unless the internal bridge cannot represent delivery success/failure without doing so; if public API change appears necessary, pause and amend this plan.

#### Likely affected areas

- `src/viewportproviderbridge.cpp`
- `src/viewportproviderbridge_p.h`
- `src/imageviewportprovider.cpp`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `tests/imageviewport_provider_test_support.h`
- New or expanded controller provider test support under `tests/`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`

#### Tasks

- [ ] Add a minimal controller provider harness for role, session serial, generation identity, request tokens, metadata events, frame events, terminal events, cancellation events, and transport effects.
- [ ] Add harness tests for token scope and stale-result rejection that can run without full Qt event delivery.
- [ ] Change internal bridge command helpers to return delivery success for metadata, frame, position, playback, cancellation, and close commands as needed.
- [ ] Treat null session after controller acceptance as delivery failure for the active command rather than a silent no-op.
- [ ] Check `QMetaObject::invokeMethod(...)` return values for affinity-bound delivery and propagate failure.
- [ ] Add a controller event or transition for provider dispatch failure that validates role, generation, session, and active token identity.
- [ ] Publish `requestStatus: Error`, `requestReason: ProviderFailure`, and bounded diagnostics for active accepted requests whose provider command could not be delivered.
- [ ] Retire active tokens and close provider interest exactly once after dispatch failure.
- [ ] Ensure stale dispatch failure reports for superseded or closed generations are ignored.

#### Acceptance criteria

- Core token-scope and stale-result provider policy tests run through a controller-level harness without `ImageViewport`, `ImageSequenceProviderSession`, event-loop drains, or `QThread`.
- Simulated null or destroyed active provider session after request acceptance reports terminal `Error` with `ProviderFailure` instead of remaining `Loading`.
- Simulated failed queued invocation reports terminal `Error` with `ProviderFailure`.
- Dispatch failure closes the affected provider generation interest and prevents later callbacks from mutating public state.
- Primary and secondary roles share the same dispatch-failure classification.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_provider_lifecycle|imageviewport_provider_requests|imageviewport_provider_terminal|viewportcontroller_playback|viewportcontroller_presentation)$'`
- `ctest --test-dir build -R '^(imageviewport_provider_lifecycle|imageviewport_provider_requests|imageviewport_provider_terminal|viewportcontroller_playback|viewportcontroller_presentation)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Close delivery can itself fail. The controller state must retire local interest before best-effort transport cleanup.
- Keep transport failure diagnostics bounded and generic; they should not expose private session details.

### Milestone 6: Role-Indexed Provider State And Shared Metadata Admission

#### Objective

Introduce the small role-indexed provider state boundary and make secondary provider metadata use the same admission and construction-fact validation path as primary.

#### Source findings

Provider role semantics are split between primary and secondary paths; provider role behavior is hard to remove or change independently; provider protocol coverage is too dependent on Qt event delivery.

#### Prerequisites

- Milestones 1, 2, and 5 complete.
- Controller provider harness can express metadata-ready, metadata-terminal, session close, token retirement, and stale metadata events for both roles.

#### Scope

- Introduce role-indexed accessors or a small `RoleProviderState` facade.
- Share provider metadata admission for primary and secondary.
- Preserve aggregate spread status projection outside role-local provider helpers.

#### Out of scope

- Do not change explicit frame seek, position seek, playback, end-of-sequence, or terminal frame handling except where metadata admission requires target revalidation.
- Do not redesign provider public APIs.
- Do not introduce arbitrary-role abstractions.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportstate_p.h`
- `src/imageviewportprovider.cpp`
- `src/imageviewportcontroller.cpp`
- `tests/tst_imageviewport_provider_metadata.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`
- Controller provider harness tests under `tests/`

#### Tasks

- [ ] Define a role-indexed provider state accessor that returns the correct `ProviderGenerationState`, accepted sequence facts, active display request, latest non-playback request, and transport result slot for `PageRole`.
- [ ] Add characterization tests for secondary provider metadata contradictions against construction-time facts and declared capabilities: expected terminal `Error` with `PayloadRejection`, generation interest closed, and no secondary metadata observation update.
- [ ] Replace secondary-specific metadata admission with the shared provider metadata validation path used for primary, including construction-time capability contradictions and known-fact contradictions.
- [ ] Route secondary metadata-bound initial target selection through the same active-request policy as primary rather than always selecting frame `0`.
- [ ] Ensure metadata updates generation facts without reviving superseded explicit seek, playback, stop, clear, or replacement state.
- [ ] Keep aggregate spread status projection outside provider role helpers; provider role code should produce role-local outcomes only.

#### Acceptance criteria

- Secondary provider runtime metadata contradicting construction facts or declared capabilities reports `Error` with `PayloadRejection`, closes active secondary provider interest, and does not update secondary metadata observations.
- Secondary metadata resolves the current accepted target and does not override newer explicit seek, playback, stop, clear, or replacement state.
- Primary metadata behavior remains unchanged except where shared validation fixes a documented spec violation.
- Shared metadata tests cover both roles through the controller provider harness.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_provider_metadata|imageviewport_provider_terminal|imageviewport_provider_lifecycle|viewportcontroller_playback|viewportcontroller_presentation)$'`
- `ctest --test-dir build -R '^(imageviewport_provider_metadata|imageviewport_provider_terminal|imageviewport_provider_lifecycle|viewportcontroller_playback|viewportcontroller_presentation)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- If metadata admission needs broader request-target machinery, keep the change limited to metadata-bound selection and move explicit seek/playback dispatch to later provider milestones.

### Milestone 7: Role-Symmetric Explicit Provider Requests

#### Objective

Route secondary provider `seek(frame)` and `seekToPosition(milliseconds)` through the same target admission, token allocation, dispatch, and stale-result policy as primary.

#### Source findings

Provider role semantics are split between primary and secondary paths; role-scoped command admission and command diagnostics are split; provider role behavior is hard to remove or change independently.

#### Prerequisites

- Milestones 4, 5, and 6 complete.
- Controller provider harness can express explicit frame and position targets, unknown-metadata waiting, request dispatch, provider frame/position callbacks, cancellation, and stale results.

#### Scope

- Add characterization tests for secondary provider frame and position seeks before and after metadata.
- Move valid secondary provider seek admission and dispatch into role-symmetric provider request paths.
- Preserve public requested position for position seeks while resolving frame identity from metadata.

#### Out of scope

- Do not change playback start, playback advancement, end-of-sequence, or stop restoration in this milestone.
- Do not change built-in secondary seek behavior except where shared command admission requires unchanged behavior through a new path.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportstate_p.h`
- `src/imageviewportprovider.cpp`
- `src/imageviewportcontroller.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_metadata.cpp`
- `tests/tst_imageviewport_provider_frame_admission.cpp`
- Controller provider harness tests under `tests/`

#### Tasks

- [ ] Add characterization tests for secondary provider `seek(frame)` with unknown metadata, known valid bounds, known invalid bounds, unsupported frame seek, and stale result after supersession.
- [ ] Add characterization tests for secondary provider `seekToPosition(milliseconds)` with unknown metadata, known valid bounds including `totalDuration`, known invalid bounds, unsupported position seek, and stale result after supersession.
- [ ] Route secondary `seek(frame)` through shared target admission and dispatch, including unknown-metadata waiting, known bounds invalidity, unsupported frame seek, provider frame request dispatch, cancellation, and stale-result filtering.
- [ ] Route secondary `seekToPosition(milliseconds)` through shared position target admission and dispatch, preserving public requested position while resolving frame identity after metadata.
- [ ] Ensure role-symmetric transport effects include the correct role, token, request kind, resolved frame, and requested position.
- [ ] Ensure command diagnostics for invalid or unsupported present-role seeks remain controller-owned.

#### Acceptance criteria

- Secondary provider `seek(frame)` dispatches `requestFrame` when valid both before and after metadata according to the same precedence as primary.
- Secondary provider `seekToPosition(milliseconds)` dispatches `requestPosition` with resolved frame and requested position when valid after metadata, and waits for metadata when required.
- Invalid and unsupported secondary provider seek commands publish command diagnostics without changing request/display revisions.
- Late results for superseded secondary seek requests are ignored.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_provider_frame_admission|imageviewport_public_api)$'`
- `ctest --test-dir build -R '^(imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_provider_frame_admission|imageviewport_public_api)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Keep explicit seek request origin distinct from playback request origin so later stop/playback work does not revive superseded identities.

### Milestone 8: Role-Symmetric Provider Playback And Terminal Lifecycle

#### Objective

Route secondary provider playback, end-of-sequence handling, terminal projection, cancellation, and lifecycle cleanup through the same provider pipeline as primary.

#### Source findings

Provider role semantics are split between primary and secondary paths; provider role behavior is hard to remove or change independently; provider protocol coverage is too dependent on Qt event delivery.

#### Prerequisites

- Milestones 5, 6, and 7 complete.
- Controller provider harness can express playback tokens, end-of-sequence, provider waiting/progress, unsupported results, cancellation results, provider failure, payload rejection, token overflow, session close, and late callbacks for both roles.

#### Scope

- Add characterization tests for secondary provider `play()` before and after metadata.
- Add controller-harness tests for terminal mapping, cancellation, token overflow, and stale-result behavior.
- Move secondary provider playback, terminal, cancellation, and lifecycle cleanup into shared role-parametric helpers.

#### Out of scope

- Do not change render acknowledgement or geometry snapshot behavior.
- Do not change public provider API shape.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportstate_p.h`
- `src/imageviewportprovider.cpp`
- `src/imageviewportcontroller.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- Controller provider harness tests under `tests/`

#### Tasks

- [ ] Add characterization tests for secondary provider `play()` waiting on unknown metadata unless construction facts declare timed playback false.
- [ ] Add characterization tests for secondary provider playback resolving to supported playback, unsupported playback, construction-fact contradiction, end-of-sequence final-frame handling, and stop restoration.
- [ ] Ensure secondary provider waiting/progress, unsupported, cancellation, provider failure, payload rejection, and end-of-sequence results use the same token-scope and failure-scope projection as primary.
- [ ] Replace duplicated secondary provider lifecycle helpers only after shared tests cover active tokens, queued work, cancellation, close, terminal generation failure, replacement, clear, and item destruction.
- [ ] Add role-parametric tests for token overflow and stale terminal results.
- [ ] Ensure removing or changing a provider lifecycle behavior now requires changing one role-parametric path rather than several secondary-specific side paths.

#### Acceptance criteria

- Secondary provider `play()` can enter `Waiting` on unknown metadata and later resolves to supported playback, unsupported playback, or payload rejection according to validated metadata and construction facts.
- Secondary provider end-of-sequence follows the same final-frame, looping, and stop behavior as primary.
- Secondary provider terminal, cancellation, provider failure, payload rejection, token overflow, close, clear, replacement, and stale-result behavior are covered by role-parametric tests.
- New provider lifecycle or terminal behavior is implemented once for `PageRole` and covered for both roles.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_provider_playback|imageviewport_provider_terminal|imageviewport_provider_lifecycle|imageviewport_provider_requests|viewportcontroller_playback)$'`
- `ctest --test-dir build -R '^(imageviewport_provider_playback|imageviewport_provider_terminal|imageviewport_provider_lifecycle|imageviewport_provider_requests|viewportcontroller_playback)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Keep playback request origin and latest non-playback target restoration explicit; do not conflate provider token identity with public display-request identity.

### Milestone 9: Controller-Authored Geometry Projection

#### Objective

Create one controller-authored geometry projection for public presentation observations and render input.

#### Source findings

Geometry has multiple runtime authorities; render path rebuilds target spread decisions from mutable state; controller depends on item-private context and ambient mutable reads.

#### Prerequisites

- Milestones 1 and 2 complete.
- Device-pixel-ratio test control has either a stable test hook/window setup or a documented `Needs investigation` entry that narrows this milestone to DPR-independent geometry while blocking final geometry completion.

#### Scope

- Add focused characterization tests for two-page geometry before implementation.
- Define a role-aware geometry snapshot at the controller/item boundary.
- Include accepted or pending role payload sizes, spread placement, effective device pixel ratio, fit/zoom/pan state, page gap, spread direction, rotation, mirror flags, visible rectangles, page rectangles, content size/position, pannability, scan state, and coordinate mappings.
- Make public getters consume the controller-authored projection.
- Make render synchronization carry the exact geometry snapshot authorized for the render attempt.

#### Out of scope

- Do not change public geometry API names or result value shapes.
- Do not rewrite render adapter internals beyond consuming the snapshot fields needed by current rendering.
- Do not change provider request behavior except to supply role payload sizes into the snapshot.

#### Likely affected areas

- `src/presentationgeometry.cpp`
- `src/presentationgeometry_p.h`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportpresentation.cpp`
- `src/imageviewportrender.cpp`
- `tests/tst_imageviewport_presentation_state.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`

#### Tasks

- [ ] Add characterization tests for two-page geometry with secondary page, non-zero page gap, both spread directions, manual zoom, pan or scan, rotation or mirror where practical, and non-positive item geometry.
- [ ] Add DPR characterization if a stable test hook exists; otherwise update `Deferred / Needs Investigation` with the blocked behavior, missing hook, dependent acceptance criterion, and exit criterion.
- [ ] Identify every production path constructing `PresentationGeometry::State` and classify whether it is controller command logic, public getter projection, or render synchronization.
- [ ] Add a controller-owned geometry snapshot type or extend existing controller output types to carry canonical geometry projection.
- [ ] Include secondary role size and placement in the snapshot for accepted ready display, retained display, and pending render target spreads.
- [ ] Include effective device pixel ratio in the snapshot according to the physical-pixel-aware zoom contract.
- [ ] Route public geometry getters in `imageviewportpresentation.cpp` to use the controller-authored snapshot instead of reconstructing state from current image/provider sizes.
- [ ] Route render synchronization to carry the same snapshot to `imageviewportrender.cpp`.
- [ ] Preserve non-positive item geometry behavior: ready accepted requests may remain ready while presentable rectangles and coordinate conversion report unavailable; pending new requests remain render waiting until positive geometry.
- [ ] Remove or isolate duplicate geometry reconstruction paths once tests prove public and render geometry agree.

#### Acceptance criteria

- Only one production path constructs canonical `PresentationGeometry::State` for accepted or pending display.
- Public getters, controller command results, and render target rectangles agree for primary-only and two-page spreads.
- Tests cover both spread directions, non-zero page gap, pan or scan, manual zoom, rotation or mirror where practical, non-positive item geometry, and non-1 device pixel ratio with a secondary page unless the DPR test hook is explicitly blocked in `Deferred / Needs Investigation`.
- Existing public coordinate conversion rules remain unchanged except where current behavior violates authoritative geometry docs.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_presentation_state|imageviewport_render_commit|imageviewport_render_scenegraph|viewportcontroller_presentation)$'`
- `ctest --test-dir build -R '^(imageviewport_presentation_state|imageviewport_render_commit|imageviewport_render_scenegraph|viewportcontroller_presentation)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Be careful not to use render backend texture/source rectangles as public logical geometry.

### Milestone 10: Immutable Render Snapshot

#### Objective

Make render synchronization consume immutable controller snapshots for layer assembly, geometry, presentation mapping, background, and quality inputs.

#### Source findings

Render path rebuilds target spread decisions from mutable state; geometry has multiple runtime authorities.

#### Prerequisites

- Milestone 9 complete or narrowed by a documented geometry blocker that does not affect render snapshot ownership.

#### Scope

- Add focused characterization for render-time mutable reads and stale secondary payloads.
- Expand `ViewportRenderSynchronization` or introduce `ViewportRenderSnapshot`.
- Move secondary layer assembly and built-in frame lookup out of `ImageViewportPrivate::updatePaintNode()`.
- Make render adapter input come from the controller-authorized snapshot.

#### Out of scope

- Do not change complete-role acknowledgement semantics in this milestone unless the snapshot shape makes it purely mechanical and already covered by tests.
- Do not rewrite the render backend or change scene graph resource ownership.
- Do not expose scene graph, texture, QRhi, native handle, or backend resource identities publicly.

#### Likely affected areas

- `src/imageviewportrender.cpp`
- `src/renderadapter.cpp`
- `src/renderadapter_p.h`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- `tests/imageviewport_paint_test_support.h`

#### Tasks

- [ ] Add characterization tests for stale secondary payload or render attempt after replacement and clear.
- [ ] Define snapshot fields for layer list, role payload identities, source rectangles, target rectangles, presentation mapping, background mode/color, smoothing, mipmap, fit/zoom/pan, rotation, mirror, spread direction, page gap, and any presentation identity needed by render.
- [ ] Ensure `beginRenderSynchronization()` returns a complete immutable layer list or equivalent role payload map authorized by the controller.
- [ ] Remove render-time calls that query `secondarySequence()`, `sequence->frameImage(...)`, secondary requested/displayed frame state, or provider logical size to construct target layers after snapshot creation.
- [ ] Route `RenderAdapter::Input` construction entirely from the controller-authorized snapshot plus render-node ownership handles.
- [ ] Preserve presentation-only updates that reuse latest committed payload identities and advance display revision without resetting request readiness.

#### Acceptance criteria

- `updatePaintNode()` is snapshot-to-adapter glue only: all render adapter inputs, including layer list, source/target rectangles, presentation mapping, background mode/color, smoothing, mipmap, fit/zoom/pan, rotation, mirror, spread direction, and page gap, come from the controller-authorized snapshot.
- `updatePaintNode()` no longer constructs render layers by reading live secondary sequence, provider, requested frame, displayed frame, or frame payload state after controller snapshot creation.
- Stale secondary payloads or render attempts after replacement and clear are ignored without changing newer accepted request or display state.
- Scene graph resource ownership remains encapsulated in the render adapter/item boundary.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_render_commit|imageviewport_render_scenegraph|imageviewport_presentation_state|imageviewport_provider_frame_admission)$'`
- `ctest --test-dir build -R '^(imageviewport_render_commit|imageviewport_render_scenegraph|imageviewport_presentation_state|imageviewport_provider_frame_admission)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Keep render backend changes minimal and behind existing render adapter boundaries.

### Milestone 11: Complete-Role Spread Render Acknowledgement

#### Objective

Make spread render acknowledgement validate the complete required role set before publishing `Ready`.

#### Source findings

Spread render acknowledgement is not complete-role scoped; render path rebuilds target spread decisions from mutable state.

#### Prerequisites

- Milestone 10 complete.
- Characterization for complete-role acknowledgement exists before implementation. Sufficient evidence is a focused failing test or trace for stale secondary acknowledgement, secondary-layer render failure, or partial-spread commit risk. This is a sequencing and testability gate, not permission to skip the required architecture fix.

#### Scope

- Extend render snapshot and acknowledgement identities to include target spread identity plus required role payload identities.
- Extend render adapter output to report complete spread commit or role-scoped failure.
- Update controller acknowledgement validation to require all current role payload identities for the active target spread.

#### Out of scope

- Do not rewrite the render backend.
- Do not change payload admission classification outside render commit/failure semantics.
- Do not expose render identities through public API.

#### Likely affected areas

- `src/imageviewportrender.cpp`
- `src/renderadapter.cpp`
- `src/renderadapter_p.h`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- `tests/imageviewport_paint_test_support.h`

#### Tasks

- [ ] Add render acknowledgement characterization for stale secondary payload acknowledgement, secondary-layer render failure, and complete spread commit requiring all active role payload identities.
- [ ] Define acknowledgement identity fields for target spread identity, primary payload identity, optional secondary payload identity, and role-scoped failure identity.
- [ ] Extend `RenderAdapter::Output` to report complete spread commit or role-scoped failure identity.
- [ ] Extend `ViewportRenderAcknowledgement` to carry complete role identity data instead of one payload identity for multi-role spreads.
- [ ] Update controller acknowledgement validation to accept commit only when every required current role payload identity matches the active target spread.
- [ ] Ensure stale secondary acknowledgement or stale role failure is ignored without affecting newer accepted spread state.

#### Acceptance criteria

- A simulated secondary-layer render failure for the active spread reports `Error` with `RenderFailure` and does not publish a partial spread.
- A stale secondary role acknowledgement or failure is ignored without affecting a newer spread.
- A two-page spread commit is accepted only when all required current role payload identities match.
- Primary-only render acknowledgement remains valid and does not require a secondary identity.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(imageviewport_render_commit|imageviewport_render_scenegraph)$'`
- `ctest --test-dir build -R '^(imageviewport_render_commit|imageviewport_render_scenegraph)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- The design review did not reproduce a visible failure, but architecture requires complete-role acknowledgement. Treat characterization as the guardrail for a safe required change, not as a reason to defer the change indefinitely.

### Milestone 12: Controller Boundary Decoupling And Layered Tests

#### Objective

Remove remaining upward controller dependencies on `ImageViewportPrivate` and shrink ambient mutable context reads after behavior-critical paths are protected by tests.

#### Source findings

Controller depends on item-private context and ambient mutable reads; provider protocol coverage is too dependent on Qt event delivery; geometry has multiple runtime authorities; render path rebuilds target spread decisions from mutable state.

#### Prerequisites

- Milestones 3, 5, 9, 10, and 11 complete.
- Any remaining `ImageViewportPrivate` references in controller code are identified with exact call sites before editing.

#### Scope

- Remove remaining controller dependency on `ImageViewportPrivate`.
- Replace broad ambient context reads in controller transitions with explicit value snapshots or immutable facts.
- Strengthen controller-level tests for assignment, playback waiting, command admission, geometry changes, provider events, and render acknowledgement using value inputs.
- Keep item-level tests focused on QML projection, provider bridge integration, timers, and scene graph integration.

#### Out of scope

- Do not perform broad source file reshuffling without behavior coverage.
- Do not remove Qt integration tests that still cover item, provider bridge, or scene graph behavior.
- Do not change public API or package contract.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportcontroller.cpp`
- `src/imageviewportprovider.cpp`
- `src/imageviewportpresentation.cpp`
- `src/imageviewportrender.cpp`
- `src/imageviewport_p.h`
- `src/framepreparation.cpp`
- `src/framepreparation_p.h`
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- Controller provider harness tests under `tests/`

#### Tasks

- [ ] Remove `#include "imageviewport_p.h"` from `src/viewportcontroller.cpp`.
- [ ] Replace any remaining `ImageViewportPrivate` references in controller code with neutral helpers or explicit controller inputs.
- [ ] Classify `ViewportControllerContext` accessors into these buckets before editing: page-set assignment facts, provider construction/runtime facts, viewport geometry inputs, prepared payload identities/data, render snapshots/acknowledgements, time/playback inputs, public notification/item-only projection.
- [ ] For page-set assignment transitions, replace mutable item reads with explicit assignment facts and provider generation facts.
- [ ] For provider events, replace mutable item reads with normalized event values, role, session, generation, token, metadata, payload, terminal cause, and transport-effect inputs.
- [ ] For geometry mutations and projection, consume the controller-authored geometry snapshot from Milestone 9 instead of item-side size or presentation reconstruction.
- [ ] For preparation and render acknowledgement, consume prepared payload identities/data and render snapshots instead of live sequence/provider/item state.
- [ ] For playback transitions, consume explicit monotonic time/playback inputs rather than sleeping wall-clock threads or item-side timer state.
- [ ] Add or extend controller tests that instantiate controller state with value inputs for assignment, playback waiting, command admission, geometry changes, render acknowledgement, and normalized provider events.
- [ ] Keep item tests for QML signal projection, provider bridge threading/queued delivery, timers, scene graph synchronization, and package behavior.
- [ ] Remove dead role-specific provider or render helper paths only when their behavior is covered by role-parametric or render snapshot tests.

#### Acceptance criteria

- `src/viewportcontroller.cpp` has no `imageviewport_p.h` include and no `ImageViewportPrivate` references.
- Controller transitions for assignment, provider events, geometry, preparation, render acknowledgement, playback, and commands consume explicit value snapshots or immutable facts instead of ambient item-private state.
- Controller behavior tests can exercise assignment, playback waiting, command admission, geometry changes, render acknowledgement, and normalized provider results without constructing `ImageViewport` where external Qt effects are not under test.
- Item-level tests still cover QML notifications, provider bridge integration, timers, and scene graph smoke behavior.
- No public API, installed package contract, or authoritative docs are changed.

#### Verification

- `cmake --build build`
- `ctest -N --test-dir build`
- Confirm focused filter selects expected tests: `ctest -N --test-dir build -R '^(viewportcontroller_playback|viewportcontroller_presentation|imageviewport_public_api|imageviewport_provider_lifecycle|imageviewport_render_scenegraph|imageviewport_render_commit)$'`
- `ctest --test-dir build -R '^(viewportcontroller_playback|viewportcontroller_presentation|imageviewport_public_api|imageviewport_provider_lifecycle|imageviewport_render_scenegraph|imageviewport_render_commit)$' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- This milestone should happen after behavior-critical paths are covered; otherwise dependency cleanup can mask regressions.
- Keep refactors behavior-preserving unless a focused test proves the current behavior violates authoritative docs.

## Deferred / Needs Investigation

- Direct controller-level provider harness feasibility. Blocked behavior: role-symmetric provider metadata, token, terminal, cancellation, overflow, and stale-result policy refactors. Missing evidence: a safe test seam that feeds normalized provider events and observes transport effects without full `ImageViewport`/Qt event delivery. Dependent milestones: 5, 6, 7, and 8. Exit criterion: controller harness tests cover token scope and stale-result rejection for both roles, or this plan is amended with a concrete alternative before provider-policy refactors proceed.
- Provider command delivery failure simulation. Blocked behavior: classifying null/destroyed session and failed queued invocation as `ProviderFailure`. Missing evidence: the least invasive bridge test hook to simulate failed delivery without public API changes. Dependent milestone: 5. Exit criterion: focused tests can simulate null session and failed `invokeMethod`/delivery and observe terminal `Error` with `ProviderFailure`.
- Device-pixel-ratio characterization for two-page spreads. Blocked behavior: final proof that physical-pixel-aware zoom geometry agrees between controller, public getters, and render for non-1 DPR. Missing evidence: a stable Qt offscreen-window or test hook for effective DPR. Dependent milestone: 9. Exit criterion: reliable DPR test exists, or the plan is amended to add a narrow internal test seam for DPR input.
- Complete-role render acknowledgement characterization. Blocked behavior: safe implementation of complete spread acknowledgement identity. Missing evidence: focused failing test or trace for stale secondary acknowledgement, secondary-layer render failure, or partial-spread commit risk. Dependent milestone: 11. Exit criterion: characterization exists before acknowledgement logic changes; implementation remains required because architecture requires complete-role acknowledgement.

## Suggested `/goal` Execution Order

- [x] Milestone 1: Baseline Verification And Test Selection
- [x] Milestone 2: Clear Transaction Characterization And Fix
- [x] Milestone 3: Mechanical Controller/Item Separation Precursor
- [ ] Milestone 4: Command Diagnostics For Invalid And Absent Roles
- [ ] Milestone 5: Provider Harness And Dispatch-Failure Foundation
- [ ] Milestone 6: Role-Indexed Provider State And Shared Metadata Admission
- [ ] Milestone 7: Role-Symmetric Explicit Provider Requests
- [ ] Milestone 8: Role-Symmetric Provider Playback And Terminal Lifecycle
- [ ] Milestone 9: Controller-Authored Geometry Projection
- [ ] Milestone 10: Immutable Render Snapshot
- [ ] Milestone 11: Complete-Role Spread Render Acknowledgement
- [ ] Milestone 12: Controller Boundary Decoupling And Layered Tests

## Plan Review Notes

- Concerns addressed: The plan now avoids a broad up-front red characterization suite, confirms CTest selection before focused runs, gates `Needs investigation` items to dependent milestones, makes the controller provider harness required before provider-policy refactors, moves transport delivery failure shape before provider dispatch unification, narrows command diagnostics work so valid provider commands are not accepted before dispatch exists, adds a behavior-preserving controller/item separation precursor, splits provider pipeline work into metadata, explicit request, and playback/terminal lifecycle milestones, separates immutable render snapshot work from complete-role acknowledgement, and keeps complete-role render acknowledgement required by architecture rather than optional.
- Concerns deferred: Device-pixel-ratio test control, provider delivery failure simulation details, and complete-role render acknowledgement characterization remain explicit investigation gates with exit criteria.
- Remaining known limitations: Exact new test function names are intentionally not prescribed; future sessions should use `ctest -N --test-dir build` after configuring to confirm current executable names before running narrow filters. The plan names likely affected areas from the design review and repository scan, but implementation should re-read nearby code before editing.

## Non-Goals

- Do not modify `docs/spec/**` or `docs/architecture/**` while executing this plan.
- Do not implement source code changes as part of planning consolidation.
- Do not expose scene graph resources, backend texture handles, QRhi objects, native texture payloads, source identity, file loading, provider progress, or render backend selection as public API.
- Do not add implicit file, URL, archive, byte-buffer, JavaScript-object, or raw-provider assignment paths to `ImageViewport`.
- Do not rewrite the render backend before render snapshot and acknowledgement characterization exists.
- Do not introduce a broad generic multi-role framework; keep shared role logic explicit to primary and secondary roles.
- Do not change caller-visible behavior except where current behavior conflicts with the authoritative docs or with a consolidated design finding in this plan.
- Do not use public diagnostic strings as internal branch conditions.
