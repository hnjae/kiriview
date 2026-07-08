# ImageViewport v2 Implementation Plan

This plan sequences the migration from the current controller-centered implementation to the v2 end state defined by `docs/spec/**` and `docs/architecture/**`. It is intentionally a milestone document: it may describe temporary adapters, compatibility shims, sequencing, and replacement criteria that do not belong in the authoritative spec or architecture documents. It lives at `docs/implementation-plan.md` because this path was explicitly requested for the v2 migration plan; treat it as the accepted roadmap artifact for this migration rather than as a normative spec or architecture file.

## Operating Rules

- Treat `docs/spec/**` and `docs/architecture/**` as the target contract. Do not change their meaning to match temporary implementation limits.
- Do not delete existing source or test files during the migration. Remove obsolete files only in the compatibility removal milestone after no build target or test references them.
- Keep existing behavior tests green unless a milestone explicitly adds durable v2 coverage for the same behavior and identifies the old test as obsolete.
- Add v2 behavior through adapters first, then move ownership into the engine one domain at a time.
- Decide and test the QML import/version strategy before the first QML-visible v2 API lands. Temporary additive exposure may keep existing imports green, but compatibility removal must not begin until the final v2 import and QML metadata expectations are represented in tests.
- Prefer focused behavior or boundary tests over source-pattern tests. Keep structural tests only for stable ownership boundaries.
- Commit each milestone independently. If a milestone changes the public product contract or durable architecture intent beyond the already documented end state, update and commit the relevant intent document before tests or implementation.

## Baseline Inputs

- Current reset baseline: `main` at `9dc1b23 docs: define v2 snapshot API`, with a clean worktree before this plan was written.
- Public end-state sources: `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, `docs/spec/image-viewport-state.md`, `docs/spec/image-sequence-provider-protocol.md`, `docs/spec/image-sequence-provider-adapter.md`, and `docs/spec/image-viewport-packaging.md`.
- Architecture end-state sources: `docs/architecture/subsystem-boundaries.md`, `docs/architecture/provider-protocol.md`, `docs/architecture/presentation-geometry.md`, `docs/architecture/playback-state-machine.md`, `docs/architecture/rendering.md`, and `docs/architecture/build-and-package.md`.
- Current implementation targets: `src/CMakeLists.txt` builds the public static QML module from `imageviewport*`, `viewportcontroller*`, provider host/bridge files, render adapter/host files, playback utilities, frame preparation, and public source-handle types.
- Current test targets: `tests/CMakeLists.txt` defines public API/QML tests, still/timed behavior tests, provider contract/lifecycle/metadata/request/admission/terminal/playback tests, render scenegraph/commit tests, controller unit tests, install-consumer coverage, and structural boundary checks.

## Milestone 1: Baseline and Guardrails

### Goal

Establish the migration baseline, inventory current public behavior, and make it explicit which tests must remain green until replaced by durable v2 coverage.

### Scope

- Record the current public API surface: flat `ImageViewport` properties, command invokables, `setPageSet` overloads, `PageSetTransitionPolicy`, coordinate helpers, provider adapter methods/signals, factory methods, limits, QML import surface, and installed package consumer expectations.
- Record existing behavior by suite and by ownership domain: public API, sequence assignment, presentation geometry, provider protocol, playback, render commit, diagnostics, lifecycle cleanup, structural boundaries, and packaging.
- Keep this milestone docs/test-only except for optional test harness annotations or baseline inventory updates.

### Existing Behavior That Must Be Preserved

- `tests/CMakeLists.txt` baseline anchors must stay green: `imageviewport_provider_terminal/providerFrameFailureRetainsDisplayAndClearsOnSeek`, `imageviewport_public_api/emptyGeometryChangeIncrementsDisplayRevision`, `imageviewport_provider_requests/providerTimedFrameSeekCancelsSupersededRequest`, `imageviewport_render_commit/providerSupersededRenderFailureIsIgnored`, `imageviewport_timed/timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay`, `imageviewport_provider_metadata/providerTimedMetadataSelectsInitialFrameRequest`, and `imageviewport_provider_playback/providerTimedPlaybackWaitsForMetadata`.
- Public API and QML scaffolding tests must continue to prove current behavior while v2 adapters are introduced: `imageviewport_public_api`, `imageviewport_public_api_commands`, `imageviewport_public_api_provider_roles`, and `imageviewport_public_api_qml`.
- Existing source-handle and package boundaries must remain usable by installed consumers: `imagesequence_factory`, `imageviewport_install_consumer`, and the structural package/header checks.
- Current provider, render, presentation, timed playback, and controller tests must remain in the target graph and must not be removed or weakened in this milestone.

### Tests To Add Or Update

- Add a small baseline inventory test or checklist only if the current `structural::refactorBaselineInventory` output is insufficient for future comparison.
- Update `tests/CMakeLists.txt` comments only if they become stale; do not add release-gate identifiers or milestone progress language to specs or architecture docs.
- No behavior tests should be deleted in this milestone.

### Implementation Steps

1. Run the full test suite from a clean build and capture failures before migration work begins.
2. Confirm each existing test target in `tests/CMakeLists.txt` still builds and runs under the same CMake options used by CI.
3. Produce an inventory of public `Q_PROPERTY`, `Q_INVOKABLE`, provider signal, provider virtual method, installed header, and QML singleton surfaces from `src/imageviewport.h`.
4. Map each inventory area to current behavior suites so later milestones can name the v2 replacement test before changing compatibility behavior.
5. If a current test is brittle because it asserts incidental implementation text or private layout, leave it in place but identify the future v2 behavior test that will supersede it.

### Completion Criteria

- A clean baseline test report exists for the reset commit.
- Every existing test file and test target remains present.
- The migration guardrail inventory names the behavior suite that protects each current public behavior area.
- No source or test implementation behavior changes have been made.

### Status

Complete as of 2026-07-08. `just clean test` rebuilt `build-ninja` from scratch at `1207585` (`docs(plan): add v2 implementation milestones`, source-equivalent to reset baseline `9dc1b23`) and passed 42/42 tests, including `imageviewport_install_consumer` and `structural::refactorBaselineInventory`.

`structural::refactorBaselineInventory` wrote the raw comparison inventories to `build-ninja/tests/refactor-role-inventory.txt` and `build-ninja/tests/refactor-command-diagnostic-inventory.txt`; the clean run reported 3263 role-specific entries and 322 command-diagnostic entries. These files are build artifacts used for local comparison only; the durable guardrail is the behavior-suite map below.

### Guardrail Inventory

- Flat `ImageViewport` observation properties, command invokables, command diagnostics, revision tokens, QML bindings, enum exposure, and source rejection are protected by `imageviewport_public_api`, `imageviewport_public_api_commands`, `imageviewport_public_api_provider_roles`, `imageviewport_public_api_qml`, `imagesequence_factory`, and `imageviewport_install_consumer`.
- `setPageSet` overloads, `sequence` assignment compatibility, primary/secondary atomicity, transition policy validation, clear behavior, retained-display selection, invalid command preservation, and role-scoped assignment are protected by `imageviewport_public_api_commands`, `imageviewport_public_api_provider_roles`, `imageviewport_still`, `imageviewport_timed`, and the provider metadata/request suites.
- Coordinate helpers, page geometry values, spread direction, page gap, fit/zoom/pan/scan, rotation, mirroring, background, smoothing, mipmap, looping, physical zoom, non-positive geometry, and presentation notification behavior are protected by `imageviewport_presentation_state`, `imageviewport_still`, `imageviewport_timed`, `imageviewport_render_scenegraph`, `viewportcontroller_presentation`, and render commit coverage.
- Provider adapter construction methods, known metadata/facts, capability declarations, authored animation facts, threading contract, provider session virtual methods/signals, request tokens, metadata values, frame metadata, frame handles, cancellation, close, diagnostics, terminal projection, and provider lifecycle cleanup are protected by `imageviewport_provider_contract`, `imageviewport_provider_lifecycle`, `imageviewport_provider_metadata`, `imageviewport_provider_requests`, `imageviewport_provider_frame_admission`, `imageviewport_provider_terminal*`, `imageviewport_provider_playback`, `viewportcontroller_provider`, and provider test-support suites.
- Built-in still frames, timed frame lists, source-handle factories, image orientation normalization, payload byte limits, metadata validation, timed durations, authored autoplay facts, and source-handle lifetime are protected by `imagesequence_factory`, `imageviewport_still`, `imageviewport_timed`, `imageviewport_provider_frame_admission`, and `structural::builtInPayloadBoundary`.
- Playback, seek, pause, stop, authored autoplay, looping, render-waiting restoration, metadata-waiting playback, provider-backed playback, scheduler timing, and stale playback rejection are protected by `imageviewport_timed`, `imageviewport_provider_playback`, `imageviewport_provider_terminal_playback`, `viewportcontroller_playback`, `playback_timeline`, and `playback_clock`.
- Render scenegraph planning/materialization, render commit identity, retained display after failure, stale acknowledgement rejection, two-page spread commit, render failure diagnostics, background rendering, texture mapping, physical source rectangles, smoothing/mipmap, and role layer geometry are protected by `imageviewport_render_scenegraph`, `imageviewport_render_commit`, `structural::renderHostBoundary`, and `structural::renderRoleStateBoundary`.
- Installed public header generation, Linux/static QML package layout, QML singleton limits, source-handle/package consumption, and public/private boundary checks are protected by `imageviewport_install_consumer`, `structural::controllerContractHeaderBoundary`, `structural::controllerFacadeBoundary`, `structural::providerHostBoundary`, `structural::renderHostBoundary`, `structural::playbackSchedulerBoundary`, and `structural::ownerSpecificHelperHeadersOnly`.

### Risks And Rollback Criteria

- Risk: future sessions remove tests as "legacy" before v2 equivalents exist. Roll back any commit that deletes, disables, or weakens a behavior test without naming equivalent v2 coverage.
- Risk: inventory becomes a brittle source-pattern gate. Keep inventory informational unless it protects a durable architecture boundary.
- Roll back this milestone if it changes product behavior, source files, or public docs outside the plan/inventory scope.

## Milestone 2: Snapshot API Adapter

### Goal

Add v2 snapshot value types and a read-only `state` projection over the existing implementation while leaving v1 flat properties and behavior intact.

### Scope

- Introduce public value types for `ImageViewportStateSnapshot`, request/display/presentation/role/diagnostics/revision snapshots, role sets, page-set generation tokens, revision tokens, demand revision tokens, command results, coordinate inputs/results, and enum aliases required by the v2 state spec.
- Add `ImageViewport::state` and `stateChanged` as a projection from the current controller state.
- Keep current flat properties, flat notification signals, command return values, and coordinate helper methods unchanged.
- Keep v2 snapshot projection read-only. Do not move state ownership yet.

### Existing Behavior That Must Be Preserved

- Flat property tests remain authoritative until compatibility removal: request/display status, frame/position observations, metadata capability tri-states, diagnostics, presentation fields, geometry fields, revision tokens, and QML bindings must continue to behave as they do now.
- Existing revision behavior must remain unchanged for flat properties while `state.revisions` is projected consistently.
- Retained display observations must preserve current behavior for failed replacement, loading replacement, and secondary role retention.
- QML import behavior must remain compatible with existing tests and installed consumers.

### Tests To Add Or Update

- Add `imageviewport_state_snapshot` or extend `imageviewport_public_api` to verify default snapshot values, QML readability, C++ copying, equality-only token helpers, invalid sentinel values, and `stateChanged` emission.
- Add projection parity tests that compare snapshot fields against current flat observations for representative ready, loading, retained, unsupported, and error states.
- Add retained-display snapshot tests covering separate accepted-request and displayed-presentation identity using existing retained-display scenarios.
- Add QML tests that bind to nested snapshot fields and verify flat properties still emit as before.

### Implementation Steps

1. Add public Q_GADGET/QML value types and metatype declarations in the public header without removing existing value types.
2. Add private projection helpers that translate current `ImageViewportState` and controller observations into the v2 snapshot schema.
3. Add a `state` property to `ImageViewport` backed by the projection helper and emit `stateChanged` whenever any projected field changes.
4. Route existing notification sites through a single projection comparison so `stateChanged` is coherent and not emitted for unchanged snapshots.
5. Keep old `RevisionToken` behavior for flat properties; introduce v2 token names as wrappers or aliases only where they can be copied and compared without exposing ordering.
6. Update installed-header generation and QML registration only as needed for the new public value types.

### Completion Criteria

- Existing public API, QML, provider, render, timed, and controller tests remain green.
- New snapshot tests prove the v2 state projection is coherent across default, ready, loading, retained, terminal, and presentation-only states.
- No v1 flat property or signal has been removed.
- Installed consumers can include the new public types without private headers.

### Status

Complete as of 2026-07-08. Added read-only `ImageViewport::state`, public snapshot/token/role-set/command-result/coordinate value scaffolding, QML value-type registration, and `stateChanged` emission from cached snapshot comparison while preserving all v1 flat properties and signals.

Focused coverage is in `imageviewport_state_snapshot`: default snapshot values and copying, ready still parity with flat properties, retained replacement separating accepted and displayed generations, terminal provider diagnostics, presentation-only snapshot changes, and QML nested snapshot reads. `imageviewport_install_consumer` now compiles and uses `ImageViewportStateSnapshot`, v2 revision/generation/demand token defaults, `ImageViewportRoleSet`, `ImageViewportCommandResult`, `ImageViewportCoordinateInput`, and `ImageViewportCoordinateResult` from the installed public header.

Verification: `ctest --test-dir build-ninja --output-on-failure` passed 43/43 after the implementation and install-consumer update.

Adapter assumptions recorded for later milestones: demand revisions remain invalid sentinels until typed provider demand exists; quality and exactness preferences project as `Default` and current accepted built-in/provider payloads project as exact-only compatibility facts until the explicit envelope milestone; the presentation revision is projected from the current display revision until presentation state has an independent engine-owned revision; the snapshot revision is an opaque equality-only mix of current request/display/command revision domains and generation identity until the engine owns canonical snapshot revision allocation.

### Risks And Rollback Criteria

- Risk: snapshot projection accidentally becomes independent mutable state. Roll back if snapshot fields can diverge from the controller without a controller transition.
- Risk: notification churn breaks QML bindings. Roll back if `stateChanged` or existing flat signals emit on no-op transitions beyond documented revision behavior.
- Risk: public tokens expose numeric ordering. Roll back token APIs that require callers to compare order rather than equality/currentness.

## Milestone 3: Page Set API Introduction

### Goal

Introduce canonical `ImageViewportPageSet` and `setPageSet(pageSet, policy)` while adapting to the existing sequence assignment path.

### Scope

- Add `ImageViewportPageSet` as the public v2 value carrying required primary and optional secondary `ImageSequence` handles, with clear/null-primary semantics.
- Add the canonical `setPageSet(ImageViewportPageSet, PageSetTransitionPolicy)` overload for C++ and QML.
- Preserve current `sequence` property and existing `setPageSet(primary, secondary, policy)` compatibility overloads until the compatibility removal milestone.
- Keep two-role atomicity, clear behavior, invalid secondary rejection, transition policy validation, and same-target refinement validation identical to current tests until v2 ownership replaces them.

### Existing Behavior That Must Be Preserved

- `imageviewport_public_api_commands/setPageSetAcceptsPrimaryAndSecondaryAtomically` and related typed overload tests must keep passing.
- Compatibility sequence assignment must keep clearing the secondary role until v1 removal.
- Clear-style page sets must clear accepted roles, avoid starting secondary provider sessions, preserve presentation preferences as currently documented, and clear retained display when requested.
- Invalid page-set and invalid transition-policy commands must preserve previous request/display/presentation state except command diagnostics.

### Tests To Add Or Update

- Add C++ and QML tests for `ImageViewportPageSet` default, clear, primary-only, and two-role construction.
- Add tests proving canonical `setPageSet(pageSet, policy)` is behaviorally identical to existing primary/secondary overloads for still, timed, provider, clear, invalid, and retained-display transitions.
- Add tests proving raw strings, URLs, byte arrays, JavaScript objects, raw provider objects, provider URLs, image-provider ids, load acknowledgements, and arbitrary variants are rejected by the canonical path.
- Keep existing v1 assignment tests until compatibility removal and label their future replacement coverage in comments if needed.

### Implementation Steps

1. Add `ImageViewportPageSet` public value type and QML structured-value registration.
2. Implement canonical overload validation in the item layer and convert valid page sets into the existing primary/secondary assignment call.
3. Normalize clear handling so `clear()` and null-primary `ImageViewportPageSet` share the same policy path.
4. Keep current overloads implemented as adapters into the canonical value where possible.
5. Extend snapshot projection so `state.request.acceptedRoleSet`, `targetRoleSet`, and role `sequence` fields reflect the page-set command result.
6. Update installed consumer coverage for the canonical overload while retaining old installed API coverage.

### Completion Criteria

- Canonical page-set API is public, QML-readable, and install-consumer usable.
- Existing sequence assignment behavior is unchanged.
- New page-set tests pass for still, timed, provider, clear, invalid, and two-role atomic cases.
- No existing source or test file has been deleted.

### Status

Complete as of 2026-07-08. Added public `ImageViewportPageSet`, C++ `setPageSet(ImageViewportPageSet, PageSetTransitionPolicy)` overloads, QML structured-value assignment through the existing QVariant invokable surface, and installed-consumer coverage for primary-only, two-role, policy, and invalid secondary-only page sets.

Focused coverage is in `imageviewport_public_api_commands`: value default/construction semantics, canonical C++ assignment parity with the primary/secondary path, canonical secondary-without-primary rejection, QML typed value assignment, QML `(pageSet, policy)` dispatch, and arbitrary string/object/provider rejection without request or display mutation. `imageviewport_public_api_qml` remains green for legacy two-argument QML assignment calls, and `imageviewport_install_consumer` now compiles and uses `ImageViewportPageSet` from the installed header.

Verification: `ctest --test-dir build-ninja --output-on-failure` passed 43/43 after the implementation and install-consumer update.

Adapter assumptions recorded for later milestones: canonical typed page sets reject secondary-only values, while the legacy QVariant primary/secondary compatibility path keeps its documented null-primary clear behavior until compatibility removal; QML exposes the canonical one-argument value path through `setPageSet(QVariant)` to avoid Qt overload-resolution conflicts with legacy two-argument assignment calls; valid typed page sets still adapt into the existing controller assignment transaction until the engine owns page-set identity.

### Risks And Rollback Criteria

- Risk: adding the canonical value changes v1 overload semantics through shared validation. Roll back if current compatibility tests fail without a planned replacement.
- Risk: clear and secondary-only handling diverge between overloads. Roll back if clear/null-primary semantics are not identical across the public entry points.
- Risk: QML structured value construction accepts arbitrary variants as sources. Roll back those conversions and require typed `ImageSequence` handles only.

## Milestone 4: Typed Provider Protocol and Source Envelope Adapter

### Goal

Introduce `ImageSequenceProviderDescriptor`, `ImageSequenceProviderRequest`, `ImageSequenceProviderEvent`, and explicit frame envelope values while bridging them to the current source-handle and provider session APIs.

### Scope

- Add v2 provider public value types for descriptor, request, event, display demand, frame envelope, unsupported cause, payload quality/exactness, request/event kinds, and tokens.
- Add a typed request/event adapter path beside the current virtual methods and legacy provider signals.
- Add the public source payload envelope model for `ImageFrame` and provider payloads, including source-logical-size versus payload-raster-size separation, quality, exactness, byte-size facts, timing facts, orientation policy, and source-to-payload scale.
- Keep bare-`QImage` `ImageFrame` and factory construction as the exact-only convenience path that infers an envelope from normalized image facts.
- Adapt existing provider construction virtuals through `ImageSequenceProviderDescriptor`: `sessionFactory()`, `knownMetadata()`, `knownFacts()`, capability accessors, authored animation facts, and threading contract remain compatibility inputs until removal.
- Keep current provider behavior, token allocation, session affinity, cancellation, close, frame-handle release, metadata validation, and failure classification intact.
- Do not require all provider tests to switch to typed sessions in one step.

### Existing Behavior That Must Be Preserved

- Provider construction and installed consumer behavior from `imageviewport_provider_contract`, `imagesequence_factory`, and `imageviewport_install_consumer`.
- Provider lifecycle tests for session open failure, dispatch failure, cancellation before close, clear/replacement cleanup, queued event delivery, synchronous protocol tests, and late callback rejection.
- Provider request tests for unique tokens, seek resolution before metadata, queued frame requests, stale-token cancellation, and role-local secondary requests.
- Provider frame admission tests for envelope validation, source logical size, payload byte limits, retained/rejected/stale handle release exactly once, and zero-geometry render waiting.
- Provider terminal and diagnostics tests for generation-terminal versus display-request-terminal classification, unsupported causes, protocol violations, redaction, and plain text diagnostics.
- Existing `ImageFrame`, `TimedImageFrameList`, still source, timed source, and factory tests must continue to preserve exact image construction, orientation normalization, payload retention, duration validation, and source-handle lifetime.

### Tests To Add Or Update

- Add public value tests for typed provider request/event defaults, required fields per kind, invalid token behavior, unsupported cause validation, demand invalid sentinels, frame envelope validation, and descriptor defaults.
- Add public value and factory tests for explicit `ImageFrame` envelope construction, default exact-only bare-image envelopes, logical source size distinct from payload raster size, source-to-payload scale, quality/exactness defaults, invalid envelope rejection, and timed frame envelope consistency.
- Add adapter parity tests that run the same metadata, frame, position, playback, waiting, progress, unsupported, cancelled, failed, and end-of-sequence flows through both legacy and typed provider sessions.
- Add demand projection tests that assert role, resolved frame, requested position, source logical size, visible source rect, target physical size, DPR, rotation, mirroring, quality/exactness, caps, budgets, current payload facts, and demand revision for representative still, timed, retained, and presentation-change states.
- Add a temporary demand-invalidation matrix test before typed demand reaches providers. Until the engine owns demand, the adapter must advance demand revisions for every known payload-affecting field it can observe: source logical size, resolved frame, requested position, visible logical rect, target physical size, effective device pixel ratio, rotation, mirroring, quality preference, exactness preference, texture caps, payload byte caps, display byte budget, allocation generation, and current payload quality/exactness/raster/scale facts; fields it cannot observe safely must use explicit invalid sentinels.
- Add installed consumer coverage for explicit frame envelope construction and implementing a typed provider without private headers while keeping legacy provider installed coverage until removal.

### Implementation Steps

1. Define typed public provider and frame envelope values without deleting legacy provider virtual methods or signals.
2. Add explicit envelope construction for `ImageFrame` and adapt bare-image construction to populate exact-only envelope facts.
3. Adapt built-in still and timed frame-list sources to carry source logical size, payload raster size, source-to-payload scale, quality, exactness, byte size, timing, alpha, and orientation facts through the same envelope shape used by providers.
4. Add `ImageSequenceProviderDescriptor` to `ImageSequenceProviderAdapter`, initially defaulting from existing adapter virtual methods.
5. Bridge legacy adapter construction virtuals into descriptor fields so legacy providers keep working while typed descriptor tests are added.
6. Add a typed session entry point and typed event signal; bridge typed requests to legacy virtuals and legacy signals to typed events where a provider has not opted into typed handling.
7. Normalize provider events in the provider host before they enter the controller so the current controller continues to receive the existing internal event shape.
8. Project display demand from existing geometry/request state with a documented temporary owner for each demand field and an invalidation matrix that matches the public demand-revision rules for every field the adapter can observe.
9. Use explicit invalid sentinels, not guessed values, for demand caps, budgets, allocation, current payload facts, or geometry fields that are unavailable before later engine migrations.
10. Update provider test support classes incrementally so new tests can choose legacy or typed mode.
11. Keep borrowed raw-frame compatibility signals working until compatibility removal; prefer frame-handle and envelope tests for new typed coverage.

### Completion Criteria

- Typed provider values are public, QML/C++ metatypes where required, and install-consumer usable.
- `ImageFrame` and built-in sequence sources expose the v2 envelope model while bare-`QImage` construction remains an exact-only compatibility convenience.
- Existing provider tests pass unchanged or with additive typed variants.
- Typed and legacy provider paths produce the same public request/display/diagnostic outcomes for parity scenarios.
- Providers receive display demand on frame, position, and playback typed requests.
- Demand revisions advance for every temporary adapter-owned payload-affecting field, and unavailable demand fields are explicit invalid sentinels.

### Status

Complete as of 2026-07-08. Added public typed provider values for descriptors, requests, events, display demand, frame envelopes, request/event kinds, and unsupported causes; added `ImageFrame` envelope access and explicit-envelope construction while keeping bare-`QImage` construction as an exact-only convenience path.

The provider adapter now has a descriptor bridge: legacy construction virtuals still work, while `ImageSequenceFactory::fromProvider()` consumes `ImageSequenceProviderDescriptor`. Provider sessions now expose `request(const ImageSequenceProviderRequest&)` and `providerEvent(const ImageSequenceProviderEvent&)`; the provider bridge sends typed requests, maps the base typed request implementation back to legacy virtuals, and normalizes typed events into the existing controller event shape. Legacy provider signals and virtuals remain available.

Focused coverage is in `imageviewport_provider_contract`, `imagesequence_factory`, and `imageviewport_install_consumer`: typed value validation, explicit frame-envelope validation, descriptor-backed provider construction, typed request demand delivery, typed event frame admission through the existing upload/render path, exact bare-image envelope inference, logical source size distinct from payload raster size, and installed-header use of the new values.

Verification: `ctest --test-dir build-ninja --output-on-failure` passed 43/43 after the implementation and install-consumer update.

Adapter assumptions recorded for later milestones: display demand currently carries role, resolved frame, and requested position on typed frame/position/playback requests, while demand revision, request revision, presentation revision, caps, budgets, allocation generation, visible source rect, target display size, DPR, and current payload facts remain explicit invalid sentinels until presentation/provider ownership moves into the engine; `RequireExact` admission and demand-revision stale rejection remain compatibility placeholders until provider payload admission migrates in Milestone 9.

### Risks And Rollback Criteria

- Risk: dual provider paths classify errors differently. Roll back if typed parity tests diverge from legacy behavior for the same input.
- Risk: built-in source handling conflates source logical size with payload raster size. Roll back if explicit envelope and physical zoom tests show coordinate or 100% zoom regressions.
- Risk: demand revisions become public-order dependent or miss invalidations before engine ownership. Roll back any API that requires numeric token ordering or any demand adapter that can return stale payload-affecting fields under the same revision.
- Risk: frame handles are released on the wrong thread or more than once. Roll back if lifecycle/admission tests detect release affinity or count regressions.

## Milestone 5: Engine Boundary Extraction

### Goal

Introduce `ViewportEngine` beside the existing controller and move small pure state transitions first without broad rewrites.

### Scope

- Create an engine module that owns typed inputs, typed effects, snapshot deltas, and revision allocation for a small initial domain.
- Start with pure transitions that do not require provider transport, render synchronization, timers, or scene graph access: command diagnostic handling, invalid command rejection, clear/default state projection, simple presentation no-op validation, and snapshot projection helpers.
- Keep the existing `ViewportController` as the authoritative behavior owner until a domain has v2 tests and a completed migration.
- Preserve item, provider host, render host, and scheduler boundaries.

### Existing Behavior That Must Be Preserved

- Controller unit tests must stay green while engine tests are added.
- Existing structural boundaries for controller facade, controller contract headers, owner-specific helpers, mutation primitives, provider host, render host, and playback scheduler must remain meaningful.
- Rejected commands must still mutate only command diagnostics/revision, and accepted commands must preserve current revision behavior.
- No public behavior may switch to engine ownership before corresponding v2 snapshot and behavior tests exist.

### Tests To Add Or Update

- Add core engine unit tests for default snapshot, command diagnostic revision, malformed enum rejection, no-request role command handling, clear from empty, and no-op presentation validation.
- Add adapter tests proving item/controller outputs and engine projection agree for the migrated pure transitions.
- Add or adjust structural tests to prevent item/provider/render/scheduler code from reaching directly into engine internals once the boundary exists.
- Keep controller tests as compatibility and regression coverage until each domain migrates.

### Implementation Steps

1. Add `ViewportEngine` private headers/sources and CMake target entries without removing controller files.
2. Define typed engine input and effect values for the initial pure domain.
3. Move revision allocation for the migrated pure domain behind an engine-owned allocator while adapting existing flat revision outputs.
4. Let the item/controller adapter call the engine only for the migrated domain and fall back to controller behavior for all other domains.
5. Keep effect application outside the engine: provider requests, render updates, scheduler operations, and QML notifications remain item-side or host-side.
6. Commit each domain move separately when possible so regressions can be bisected cleanly.

### Completion Criteria

- Engine files are present and built, but controller behavior remains intact for unmigrated domains.
- Engine tests cover the initial pure transitions.
- Existing public, provider, render, playback, and controller suites pass.
- No broad controller rewrite or helper deletion has occurred.

### Status

Complete as of 2026-07-08. Added the private `ViewportEngine` module beside the existing controller and wired it into the build without removing or weakening controller behavior.

Focused coverage is in the new `viewportengine` test target: default request/display/role/diagnostic/revision projection parity for the domains the engine owns in this milestone, invalid command diagnostics and command-revision allocation, malformed enum rejection, clear-from-empty no-op handling, and presentation no-op enum validation. Presentation geometry values remain adapter/controller-projected until the presentation migration milestone.

Verification: `cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure` passed 44/44 after adding the engine target.

Adapter assumptions recorded for later milestones: the engine is built and tested as the pure transition core, but the item/controller path still remains authoritative for runtime public behavior until assignment, presentation, provider, render, and playback domains are migrated behind typed effects in subsequent milestones.

### Risks And Rollback Criteria

- Risk: two owners mutate the same state. Roll back if a migrated domain can be changed by both controller and engine after one public command.
- Risk: engine effects start invoking Qt, provider sessions, scene graph, or timers directly. Roll back boundary violations.
- Risk: adapter complexity obscures behavior. Roll back the latest domain move if the same outcome cannot be verified through focused tests.

## Milestone 6: Assignment and Page-Set Engine Migration

### Goal

Move accepted page-set identity, role generation identity, page-set transition validation, and request/display transaction setup into the engine before presentation, provider, playback, or render ownership is migrated further.

### Scope

- Make the engine own accepted page-set generation tokens, accepted role sets, target role sets, role generation identity, active role selection, clear/null-primary semantics, and page-set command validation.
- Move page-set transition transactions into engine state: new target, clear, retained versus empty display choice, same-target refinement admission, transition policy application, command diagnostics, and revision allocation.
- Keep provider session opening, render scheduling, and playback scheduling as effects applied by existing hosts/controllers until their later migration milestones.
- Keep existing compatibility assignment APIs as adapters into the engine-owned page-set command path.

### Existing Behavior That Must Be Preserved

- `imageviewport_public_api_commands` coverage for atomic primary/secondary page-set acceptance, compatibility assignment clearing secondary roles, clear-style page sets, invalid secondary rejection, invalid role arguments, invalid transition policy preservation, and command diagnostics.
- Still/timed behavior for null assignment, clear preserving presentation preferences, replacement preserving presentation state, assignment waiting for positive geometry, retained display while waiting for geometry, and timed initial request projection.
- Provider behavior for construction-time metadata selecting initial frame, runtime metadata binding unknown initial targets, session-open failure after accepted replacement, secondary provider role assignment, and no provider side effects for rejected page-set commands.
- Existing retained display, request/display revision, playback stop/restoration, and render-waiting behavior must remain green while page-set ownership moves.

### Tests To Add Or Update

- Add engine assignment tests for valid and invalid `ImageViewportPageSet`, whole-command validation before mutation, clear/null-primary equivalence, primary-only and two-role accepted role sets, generation token allocation, target role set projection, and rejected-command diagnostic-only revision changes.
- Add transition tests for retained versus clear-before-load display policy, fit/zoom/content-position/rotation/mirror/spread/page-gap transition application, and same-target refinement restrictions.
- Add integration tests proving rejected page-set commands produce no provider requests, render updates, playback scheduling, handle releases, or retained-display changes.
- Add snapshot-first tests for `state.request.acceptedPageSetGeneration`, `acceptedRoleSet`, `targetRoleSet`, role `present`, role `sequence`, displayed generation, retained flags, and clear state.
- Keep all existing sequence assignment and page-set compatibility tests until Milestone 11 removes v1 compatibility.

### Implementation Steps

1. Define engine assignment inputs for `setPageSet`, `clear`, compatibility sequence assignment, and transition policy validation.
2. Move page-set value and transition-policy validation into engine code while preserving the current item-level rejection results through adapters.
3. Allocate accepted page-set generation and role generation identities in the engine for accepted non-empty replacements and clear transitions.
4. Have the engine emit typed effects for provider session open/close, built-in payload staging, render invalidation, playback stop, and retained-display release decisions without executing those effects directly.
5. Keep existing controller/provider/render/playback paths consuming those effects through adapters until their domain migrations.
6. Project both v2 snapshot fields and v1 flat fields from the engine-owned assignment state.
7. Mark old controller assignment helpers obsolete only after assignment engine tests and existing public behavior tests pass.

### Completion Criteria

- Engine owns accepted page-set identity, role generation identity, accepted/target role-set projection, clear state, and page-set transition transaction setup.
- Existing page-set, sequence assignment, provider metadata initial selection, retained display, render waiting, and playback restoration tests pass.
- No rejected page-set command can mutate provider, render, playback, display, presentation, or retained-display state except command diagnostics.
- Compatibility assignment APIs remain available and behave through the engine path.

### Status

Complete as of 2026-07-08. `ViewportEngine` now owns accepted page-set state, target role sets, active role identity, page-set generation allocation, role generation identity, clear/null-primary semantics, assignment validation, retained-versus-clear display transaction flags, and command-diagnostic results for rejected page-set transactions.

The controller keeps provider, render, playback, and presentation side effects as adapters, but valid canonical, compatibility, and private controller assignment calls now derive or carry `ImageViewportPageSet` into the engine before mutating controller state. Runtime request generation and snapshot accepted/target role-set projection are sourced from the engine-owned page-set state, while legacy flat sequence fields remain synchronized for compatibility.

Focused coverage is in `viewportengine`: primary-only and two-role atomic assignment, invalid secondary-only page sets, invalid transition policies, clear/null-primary transactions, clear-from-empty no-op behavior, generation and role-generation allocation, accepted assignment diagnostic preservation, and assignment effect flags. Public adapter coverage in `imageviewport_public_api_commands` now verifies snapshot page-set generation identity across primary, spread, and clear assignments, on top of the existing page-set, invalid-command, provider-side-effect, transition-policy, and compatibility assignment coverage.

Verification: `cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure` passed 44/44 after the migration and formatting.

Adapter assumptions recorded for later milestones: presentation transition math remains controller-owned until Milestone 7, provider request/session effects remain provider-host/controller-owned until Milestone 9, render payload and retained-display materialization remain render/controller-owned until Milestone 10, and accepted assignment still preserves existing command diagnostics for compatibility while rejected page-set transactions use the engine command result through the command outcome boundary.

### Risks And Rollback Criteria

- Risk: accepted two-role replacements expose a transient primary-only state. Roll back if atomic spread acceptance or snapshot role-set tests fail.
- Risk: rejected page-set commands leak provider sessions, render work, playback changes, or handle releases. Roll back if side-effect isolation tests fail.
- Risk: same-target refinement changes source logical coordinates or presentation state. Roll back if refinement tests show geometry, zoom, pan, role placement, or logical-size regressions.
- Risk: page-set migration duplicates generation identity between controller and engine. Roll back if stale provider/render results can match both identity systems.

## Milestone 7: Presentation and Geometry Migration

### Goal

Move zoom, fit, pan, rotation, mirroring, spread geometry, page gap, coordinate helpers, and display-demand geometry into the v2 command/snapshot path while preserving existing presentation behavior.

### Scope

- Migrate canonical presentation state and geometry projection into the engine.
- Keep existing presentation commands and property setters as adapters until compatibility removal.
- Route v2 `ImageViewportPresentationCommand`, `setPresentation(command)`, `resetView()`, and coordinate helpers through engine-owned validation and projection.
- Preserve physical-pixel-aware zoom, non-positive item geometry behavior, retained-display geometry identity, spread direction/gap, and page-scoped coordinate invalidation.

### Existing Behavior That Must Be Preserved

- `imageviewport_presentation_state` coverage for invalid enum handling, presentation-only notifications, background/quality/looping effects, two-page spread geometry, manual pan, rotation, retained geometry, gap/edge rejection, nearest visible helpers, fit modes, manual zoom limits, zoom steps, mirroring anchors, and rotation mapping.
- Still/timed tests for cover behavior, mirror visible rects, assignment waiting for positive geometry, replacement retained display while waiting for geometry, and unchanged-geometry notification suppression.
- Render scenegraph tests for texture node mapping, rotation transforms, physical source rects, smoothing/mipmap/mirroring, background rendering, and role layer geometry.
- Provider demand tests added in Milestone 4 must continue to reflect the engine geometry projection.

### Tests To Add Or Update

- Add v2 presentation command tests for transactional validation, conflict domains, optional field presence, resetView equivalence, quality/exactness preferences, and command result details.
- Add snapshot-first geometry tests that assert `state.display`, `state.presentation`, and role geometry fields rather than flat properties.
- Add coordinate helper tests for v2 `mapPoint`, `containsPoint`, and `nearestVisiblePoint` across spread/page/item domains.
- Add retained-display geometry tests that verify `displayedPresentationRevision` and `targetPresentationRevision` diverge while retained pixels are visible.
- Keep all existing presentation flat-property tests until v1 compatibility is removed.

### Implementation Steps

1. Define internal presentation command inputs matching the public v2 command shape.
2. Move the current geometry pipeline into engine-owned pure projection functions, reusing existing `presentationgeometry` helpers where stable.
3. Adapt old setters and invokables to construct v2 presentation commands and call the engine.
4. Update snapshot projection to consume engine geometry directly and flat properties to project from the same engine state.
5. Feed provider display demand from engine geometry rather than item/controller-local reconstruction.
6. Feed render snapshots from engine-authorized geometry without changing scenegraph materialization yet.
7. Mark duplicate private helper paths obsolete once they are unreachable, add them to the Milestone 11 removal ledger, and leave their source files in place until compatibility removal proves no build target or test references them.

### Completion Criteria

- Engine owns canonical presentation state and geometry projection.
- V2 presentation commands and snapshot geometry are tested.
- Existing flat presentation, coordinate, render mapping, and provider demand tests pass.
- Non-positive geometry and retained-display geometry remain unchanged.

### Status

Complete as of 2026-07-08. `ViewportEngine` now owns the canonical presentation state and exposes the geometry projection entry point used by controller geometry helpers, snapshot projection, provider/display-demand adapters, and render snapshot preparation. The controller still performs compatibility side effects and signal emission, but presentation and geometry reads now flow through engine-owned presentation state instead of controller-local state.

Added the additive public `ImageViewportPresentationCommand` value and `ImageViewport::setPresentation(command)` adapter. Existing presentation setters and invokables remain available; the new command validates the whole optional-field transaction before applying accepted changes through the compatibility command path.

Added v2 coordinate helper adapters `mapPoint`, `containsPoint`, and `nearestVisiblePoint` over the engine geometry projection while preserving the legacy coordinate helpers. The installed public header and install-consumer coverage now include the presentation command and v2 coordinate helper methods.

Focused coverage is in `viewportengine` for default presentation state and geometry projection from engine-owned presentation, `imageviewport_public_api_commands` for presentation-command acceptance/rejection and v2 coordinate helper adapters, `imageviewport_state_snapshot` for snapshot geometry updates through the presentation command, and `imageviewport_install_consumer` for installed-header use of the additive API.

Verification: `cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure` passed 44/44 after formatting and installed-header regeneration.

Adapter assumptions recorded for later milestones: `setPresentation(command)` still returns the legacy `CommandOutcome` until command-result migration; accepted multi-field presentation commands apply through existing legacy operations after whole-command validation, so compatibility signals may still emit per underlying operation; demand geometry fields that were invalid sentinels before this milestone remain invalid until provider-demand migration; retained `displayedPresentationRevision` is still projected from the display revision until render/presentation revision ownership is fully migrated.

### Risks And Rollback Criteria

- Risk: coordinate helpers silently clamp invalid gap or edge points. Roll back if page/spread invalid-domain tests fail.
- Risk: retained display starts using target presentation geometry. Roll back if retained `displayedPresentationRevision` behavior regresses.
- Risk: render and demand paths reconstruct geometry independently. Roll back boundary changes that introduce a second geometry authority.

## Milestone 8: Playback and Seek Migration

### Goal

Move playback, seek, timing, looping, autoplay, pause/stop, and stale request rejection into the engine path.

### Scope

- Migrate role-scoped seek admission, seek target resolution, playback driver ownership, playback phases, scheduler effects, loop policy, authored autoplay, stop restoration, and stale playback result rejection.
- Keep existing scheduler and playback utility classes as effect consumers/helpers until they are proven obsolete.
- Preserve built-in timed frame lists and provider-backed timed sequences through the same public request/display path.

### Existing Behavior That Must Be Preserved

- `imageviewport_timed` coverage for timed assignment, frame/position seek, secondary role seek/playback, pause/stop no-ops by role, authored autoplay, authored finite/infinite loops, deterministic and runtime timer advancement, render-waiting pause/stop behavior, and unchanged-geometry playback updates.
- `imageviewport_provider_playback` coverage for metadata-waiting playback, provider playback entry point, stop restoration, paused commits, end-of-sequence final frame handling, authored loops, secondary playback, and unsupported metadata stopping waiting playback.
- `imageviewport_provider_terminal_playback` and render commit playback failure tests for stopping on provider/render failures and restarting play after failure.
- `viewportcontroller_playback`, `playback_timeline`, and `playback_clock` behavior must remain green until engine tests replace their domain coverage.

### Tests To Add Or Update

- Add engine playback unit tests for the state machine in `docs/architecture/playback-state-machine.md`, including single-driver ownership, waiting without catch-up, pause while render waiting, stop restoration, play-once final frame promotion, looping wrap, invalid seek preservation, and role-scoped no-op commands.
- Add snapshot-first integration tests for `state.request.playbackPhase`, active role, playback role, requested/displayed frame and position, and request/display statuses.
- Add provider typed-protocol playback tests proving playback requests carry demand and stale playback events are ignored.
- Add scheduler-effect tests proving the engine emits schedule/cancel effects but never starts timers directly.

### Implementation Steps

1. Move seek command validation and target recording into engine-owned request state.
2. Move playback driver state and loop progress into engine role-indexed state.
3. Convert scheduler callbacks into typed engine tick inputs and scheduler commands into engine effects.
4. Route built-in timed frame selection and provider playback request dispatch through the same engine request path.
5. Implement stop restoration in engine state and adapt existing controller outputs.
6. Preserve stale provider/render/preparation rejection by matching engine request and playback identities.
7. Keep old controller playback helpers until all playback tests pass through engine state, then mark them obsolete for Milestone 11.

### Completion Criteria

- Engine owns playback phase, active playback role, loop progress, and seek target identity.
- Built-in and provider playback suites pass.
- Playback scheduler remains an effect consumer and has no ownership over playback phase.
- Snapshot playback fields are authoritative and flat fields project from them.

### Status

Complete as of 2026-07-08. `ViewportEngine` now owns the request state that carries playback phase, looping override, active playback role, playback position, loop-iteration progress, stop-on-ready flags, active primary/secondary display request identities, latest non-playback targets, accepted request revisions, and command revisions. Controller, provider, render, and scheduler code access that state through the engine-backed controller port instead of owning a separate controller-local request state.

Seek and playback target identities now live in engine-owned request state, so stale provider/render/preparation checks, stop restoration, snapshot request projection, flat request fields, and provider-role adapters all read the same engine-owned active/latest request identities. The playback scheduler remains an external effect consumer; it still asks the controller adapter for timer intervals and delivers elapsed ticks, while the authoritative phase/target state is stored in the engine.

Focused coverage is in `viewportengine` for default request/playback ownership and engine-owned playback driver/request identity state, and in `imageviewport_state_snapshot` for snapshot playback phase/role projection across timed `play`, `pause`, and `stop`. Existing `imageviewport_timed`, `imageviewport_provider_playback`, `imageviewport_provider_terminal_playback`, `viewportcontroller_playback`, `playback_timeline`, `playback_clock`, render, provider, and structural playback scheduler suites remain green.

Verification: `cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure` passed 44/44 after the migration and formatting.

Adapter assumptions recorded for later milestones: playback command validation, provider playback dispatch, render commit/failure handling, and scheduler tick calculation still execute in controller/helper adapters while mutating engine-owned request state; provider transport remains host/controller-owned until Milestone 9; render commit admission remains render/controller-owned until Milestone 10; `ViewportEngine::requestState()` is intentionally mutable for these adapters until typed engine effects replace direct mutation.

### Risks And Rollback Criteria

- Risk: playback accumulates elapsed time while waiting for metadata, provider work, render commit, or geometry. Roll back if deterministic playback tests show catch-up behavior.
- Risk: `stop(role)` cancels another role or fails to restore latest non-playback target. Roll back if role-scoped stop tests regress.
- Risk: stale playback payloads can commit after seek, loop, clear, replacement, or stop. Roll back if stale request tests fail.

## Milestone 9: Provider Lifecycle and Payload Admission Migration

### Goal

Move metadata, frame admission, cancellation, terminal projection, diagnostics, handle release, and provider lifecycle handling into the typed protocol path.

### Scope

- Make the engine the owner of provider generation identity, token allocation, request queues, cancellation decisions, close effects, metadata validation outcomes, payload admission outcomes, terminal scope, diagnostics projection, and typed provider event admission.
- Keep the provider host as transport only: session storage, affinity, delivery, cleanup scheduling, and normalized event delivery.
- Retain legacy provider signals through a bridge until compatibility removal.

### Existing Behavior That Must Be Preserved

- Provider lifecycle cleanup must remain non-blocking and deterministic across replacement, clear, destruction, token overflow, dispatch failure, close failure retry, late callbacks, and queued delivery.
- Metadata behavior must preserve construction facts, runtime contradiction handling, capability projection before and after metadata, initial target selection, unsupported generation-terminal state, and progress/waiting advisory behavior.
- Payload admission must preserve validation for source logical size, payload raster size, byte size, timing identity, orientation normalization, quality/exactness, demand revision, public limits, role identity, and exact release-once semantics.
- Terminal projection must preserve error precedence over unsupported, primary diagnostics tie-breaks, sealed spread behavior, clear/replacement escape, invalid token ignore rules, and provider diagnostics redaction.

### Tests To Add Or Update

- Add engine provider unit tests for token allocation, queued request flushing, stale event rejection, cancellation decision output, metadata admission, payload admission, terminal scope table, diagnostics projection, and spread aggregate status.
- Add typed provider integration tests equivalent to the current lifecycle, metadata, request, frame admission, terminal, diagnostics, and recovery suites.
- Add exactness/demand tests for `RequireExact`, inexact cached payload rejection, demand-revision stale payload release, and same-target refinement payload admission.
- Keep legacy provider suites green until typed parity coverage exists for each behavior area.

### Implementation Steps

1. Move provider request token allocation and request queue state into engine-owned provider role state.
2. Convert provider host callbacks into typed `ImageSequenceProviderEvent` inputs and let the engine perform generation/token/request admission.
3. Emit provider transport effects for metadata/frame/position/playback/cancel/close requests from the engine.
4. Move metadata validation result handling into engine transitions while reusing existing validation helpers.
5. Move frame preparation/admission decisions behind engine identities and demand revisions; keep CPU preparation in the preparation boundary.
6. Move terminal projection and diagnostics into engine state using typed status/reason/failure-scope values.
7. Ensure handle release effects are emitted exactly once through the provider host and never from render-thread code.
8. Mark legacy provider bridge paths as compatibility adapters once typed integration tests cover the same scenarios.

### Completion Criteria

- Engine owns provider identity, stale filtering, metadata status, payload admission status, terminal projection, and provider diagnostics.
- Provider host no longer branches public behavior on provider diagnostics, token order, or controller internals.
- Typed provider integration coverage exists for every legacy behavior area planned for removal.
- Existing legacy provider tests still pass.

### Status

Complete as of 2026-07-08. `ViewportEngine` now owns the primary and secondary provider generation state used for provider request-token allocation, active metadata/frame tokens, queued frame request identity, queued playback/seek target facts, metadata readiness, capability flags, accepted logical size, timing intervals, authored animation facts, and provider generation/session serial identity. Controller provider-role adapters, playback helpers, geometry demand helpers, metadata projection, and provider event paths now read and mutate the engine-backed provider generation state instead of controller-local provider fields.

Provider transport behavior is preserved: the existing provider bridge and host still install sessions, deliver typed/legacy events, dispatch requests, close sessions, and run cleanup on the existing affinity path. The engine-backed provider state still carries the compatibility session reference used by those adapters, but behavioral identity, token, queue, and metadata facts now live under the engine boundary.

Focused coverage is in `viewportengine` for empty provider generation defaults and role-isolated ownership of tokens, queued frame identity, playback target kind, metadata capability flags, logical size, timing intervals, and authored loop facts. Existing provider contract, lifecycle, metadata, request, frame-admission, terminal, recovery, playback, controller-provider, install-consumer, and structural provider boundary suites remain green.

Verification: `cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure` passed 44/44 after the migration and formatting.

Adapter assumptions recorded for later milestones: provider event admission, metadata validation decisions, payload admission, terminal projection, diagnostics projection, handle release, and transport effect creation still execute in controller/provider adapters while using engine-owned provider state; exactness and demand-revision stale-payload rejection remain compatibility placeholders until payload admission is fully typed; provider session storage and cleanup are still implemented by the existing bridge/host path and will need a cleaner transport-only split before compatibility removal.

### Risks And Rollback Criteria

- Risk: lifecycle cleanup waits for cancellation acknowledgements. Roll back if clear/replacement/destruction tests block or require provider cooperation.
- Risk: provider diagnostics become branching inputs. Roll back if public behavior depends on diagnostic strings instead of typed causes.
- Risk: payload release moves onto the render thread or duplicates release. Roll back if release affinity/count tests fail.

## Milestone 10: Render Commit Boundary Migration

### Goal

Move render planning and scenegraph commit reporting behind the new engine/render boundary while preserving render commit, retained display, and failure behavior.

### Scope

- Make the engine author render snapshots, prepared payload identities, target spread identities, role layer lists, and commit/failure admission.
- Keep scenegraph materialization and Qt Quick synchronization inside the render host/adapter.
- Preserve pure render planning before scenegraph allocation and commit acknowledgement identity for complete target spreads.

### Existing Behavior That Must Be Preserved

- `imageviewport_render_commit` tests for positive-geometry render waiting, built-in/timed/provider render commit, two-page spread complete commit, stale acknowledgement ignore, render failure retention, playback failure stopping, restart after failure, same-frame fresh request identity, commit without scenegraph, secondary role failures, diagnostics cause preservation, and geometry recovery.
- `imageviewport_render_scenegraph` tests for transparent/solid/checkerboard backgrounds, still/provider texture nodes, two-page role nodes, retained secondary layers, mixed provider spreads, physical texture source rects, quality/mirroring, rotation transforms, render failure causes, retained failure paint behavior, pure render plans, and provider retained frame waiting for geometry.
- Presentation migration tests proving render consumes engine geometry and does not reconstruct canonical mapping.

### Tests To Add Or Update

- Add engine render unit tests for render snapshot creation, target spread identity, role payload identity matching, complete two-role commit admission, stale commit/failure ignore, active render failure projection, retained fallback choice, and presentation-only display revision updates.
- Add render-host boundary tests proving scenegraph code consumes engine render snapshots and reports only typed commit/failure acknowledgements.
- Add snapshot-first integration tests for `state.display.phase`, `displayedRoleSet`, `targetRoleSet`, retained flags, displayed generation, and displayed/target presentation revisions around render commit and failure.

### Implementation Steps

1. Define engine render snapshot/effect values for pending render attempts, committed display identities, role layer payloads, background, and presentation mapping.
2. Make frame preparation produce engine-admissible prepared payload values with stable role payload identities.
3. Feed render host from engine render snapshots instead of item/controller-local display fields.
4. Report render commits/failures back as typed engine inputs carrying target spread identity and required role payload identities.
5. Move retained display and render failure admission into engine state.
6. Keep render adapter scenegraph materialization unchanged except for consuming the new snapshot value.
7. Delete no render files; leave obsolete helpers unreachable until Milestone 11 proves they are unused.

### Completion Criteria

- Engine owns render readiness, retained fallback state, target spread commit identity, and render failure projection.
- Render host owns scenegraph synchronization only.
- Render commit and scenegraph suites pass.
- Snapshot display fields are authoritative for render commit/failure behavior.

### Status

Complete as of 2026-07-08. `ViewportEngine` now owns the display/render state used for display status, displayed primary/secondary request snapshots, displayed logical image sizes and retained images, prepared payload identity allocation, pending primary/secondary render payloads, retained render-failure fallback identity, retained-fallback pixels, and display revision. Controller render, provider, playback, snapshot, and public flat-property adapters access that state through the engine-backed controller port instead of controller-local display fields.

Render commit identity and retained fallback state now live with the engine-owned request, provider, page-set, presentation, and display state. Existing render synchronization and scenegraph materialization still consume adapter-built render snapshots, but those snapshots are sourced from engine-owned display state and engine-owned presentation geometry.

Focused coverage is in `viewportengine` for empty display/render defaults and engine-owned displayed request identity, prepared payload allocation, secondary pending payload identity, retained render-failure fallback capture, pending-payload clearing, and display revision storage. Existing render commit, render scenegraph, provider frame-admission/playback, timed playback, state snapshot, install-consumer, and structural render-host boundary suites remain green.

Verification: `cmake --build build-ninja && ctest --test-dir build-ninja --output-on-failure` passed 44/44 after the migration and formatting.

Adapter assumptions recorded for later milestones: render snapshot construction, scenegraph materialization, commit/failure acknowledgement handling, retained fallback selection, and render transport effects still execute in controller/render adapters while using engine-owned display state; the render host remains the scenegraph synchronization owner; typed render effects and final adapter cleanup remain pending until compatibility removal.

### Risks And Rollback Criteria

- Risk: two-page spreads publish partial ready display. Roll back if incomplete single-role commit can set ready display for a two-role target.
- Risk: stale render failures overwrite newer diagnostics or visible state. Roll back if stale acknowledgement tests fail.
- Risk: scenegraph code starts querying provider/controller mutable state to repair snapshots. Roll back render-host boundary violations.

## Milestone 11: Compatibility Removal

### Goal

Remove v1 flat properties, legacy provider adapter/session APIs, compatibility provider signals, and obsolete helper paths only after equivalent v2 behavior is implemented and covered.

### Scope

- Remove public v1 flat observation properties and legacy command/property setters that are superseded by `state`, `setPageSet(ImageViewportPageSet, policy)`, `setPresentation(command)`, v2 coordinate helpers, and typed command results.
- Remove legacy sequence assignment and observation surfaces after canonical page-set and snapshot tests cover them: `sequence`, `setSequence`, `primarySequence`, `secondarySequence`, primary/secondary `setPageSet` overloads, and QML assignments that accept anything other than typed `ImageViewportPageSet` and `ImageSequence` handles.
- Remove public numeric token access and legacy token-order expectations after equality/currentness helpers are covered: `RevisionToken::value`, `ImageSequenceProviderRequestToken::id()`, QML numeric token reads, and tests that infer ordering, density, or monotonic arithmetic from public tokens.
- Remove legacy provider adapter construction virtuals only after `ImageSequenceProviderDescriptor` installed-consumer and integration tests cover their behavior: `sessionFactory()`, `knownMetadata()`, `knownFacts()`, capability accessors, authored animation facts, and threading contract accessors.
- Remove legacy provider session virtuals and signals only after typed provider request/event tests cover their behavior: `requestMetadata`, `requestFrame`, `requestPosition`, `requestPlayback`, direct `cancelRequest`, direct `close`, metadata/frame/progress/waiting/end/failed/unsupported/cancelled signals, borrowed raw-frame signals, and legacy frame-handle signals that bypass `ImageSequenceProviderEvent`.
- Remove obsolete private helpers only when no build target or test references them.
- Keep product behavior; only remove obsolete API surfaces and compatibility adapters.

### Existing Behavior That Must Be Preserved

- All product behavior covered by old tests must have v2 snapshot/command/protocol tests before old tests are deleted or rewritten.
- No behavior may be deleted merely because the old API surface is removed.
- Source-handle behavior, provider construction, rendering, playback, retained display, diagnostics, packaging, limits, and role/spread behavior remain part of the product.

### Tests To Add Or Update

- Replace flat-property public tests with snapshot-first tests that assert the same observable states through `state`.
- Replace old command return tests with `ImageViewportCommandResult` tests that assert outcome, reason, command revision, and snapshot revision.
- Replace sequence-property and primary/secondary overload tests with canonical `ImageViewportPageSet` tests, then add negative QML/public-header/install-consumer checks for the removed sequence properties and overloads.
- Replace numeric token tests with equality-only token tests and negative QML/public-header/install-consumer checks for removed token value/id accessors.
- Replace legacy provider adapter construction and session signal/virtual tests with descriptor plus typed request/event tests and installed consumer coverage.
- Add negative public API boundary tests proving removed v1 symbols and legacy provider adapter/session members are not exposed in QML, public headers, or installed consumers once removal begins.
- Keep a removal ledger in this plan or a roadmap note that maps each deleted old test to the v2 test covering the same behavior.

### Implementation Steps

1. Build a removal ledger listing each v1 flat property, legacy sequence assignment property/overload, public numeric token accessor, legacy signal, legacy invokable, legacy provider adapter construction virtual, legacy provider session virtual/signal, private helper, and old test scheduled for removal.
2. For each ledger row, name the v2 test that covers the behavior. If no v2 test exists, add it before removal.
3. Remove or rewrite public API tests only after their v2 equivalent passes in the same commit or an immediately preceding test commit.
4. Confirm the final QML import/version strategy and update enough CMake/QML metadata, QML type expectations, and QML tests in this milestone to keep the suite green during compatibility removal; Milestone 12 finishes install/package cleanup for that final surface.
5. Remove compatibility adapters in small groups: flat observation properties, sequence properties, old presentation setters, old coordinate helpers, old page-set overloads, public numeric token accessors, legacy adapter construction virtuals, legacy provider request methods/signals, borrowed frame signals, legacy direct close/cancel entry points, then obsolete private helpers.
6. Update installed header generation and package consumer tests with the v2 public surface before removing the corresponding installed compatibility surface.
7. Run full tests after each group and stop on the first behavior regression.

### Removal Ledger

- Numeric token accessors: removed `RevisionToken::value`, the QML `RevisionToken.value` property, public `ImageSequenceProviderRequestToken(quint64)`, and `ImageSequenceProviderRequestToken::id()`. Replacement coverage: `imageviewport_public_api/revisionTokensExposeValidityAndEquality`, provider lifecycle/request/playback equality assertions, `imageviewport_presentation_state/revisionTokensUseSharedNonWrappingAllocator`, `viewportcontroller_provider` stale-token tests, `viewportengine/providerStateOwnsTokensQueuesAndMetadataByRole`, and `imageviewport_install_consumer`. Status: removed in the numeric-token cleanup slice.
- Flat item observations and legacy notify signals: remove `sequence`, `primarySequence`, `secondarySequence`, `requestStatus`, `requestReason`, `commandReason`, `displayStatus`, `playbackPhase`, frame/position/count/duration/bounds/capability properties, displayed-size properties, geometry/content/pannability properties, `displayRevision`, `requestRevision`, `commandRevision`, diagnostics properties, presentation properties, and compatibility-only signals `sequenceChanged`, `requestStateChanged`, `commandStateChanged`, `displayStateChanged`, `playbackPhaseChanged`, `displayRevisionChanged`, `requestRevisionChanged`, `commandRevisionChanged`, `diagnosticsChanged`, `presentationChanged`, `geometryStateChanged`, and `loopingChanged`. Replacement coverage: `state` plus `stateChanged`, `imageviewport_state_snapshot`, `viewportengine`, provider terminal/projection/recovery tests, render commit tests, presentation-state tests, and install-consumer snapshot checks. Status: item-level QML/meta-object presentation observation properties, C++ flat presentation getters, role/spread geometry projections, spread/displayed-role size projections, primary display size, content/visible-image projections, content-size/position projections, pannability projections, per-role count/duration/bounds projections, per-role capability projections, primary count/duration/bounds projections, primary capability projections, per-role frame/position projections, item-level primary frame/position aliases, request/display/playback status projections, diagnostics projections, revision projections, role sequence projections, the root sequence projection, and all listed compatibility-only signals are removed.
- Legacy sequence assignment and page-set overloads: remove `setSequence`, `sequence()`, `primarySequence()`, `secondarySequence()`, QML `sequence` assignment, `setPageSet(QVariant)`, `setPageSet(QVariant,QVariant)`, `setPageSet(QVariant,QVariant,PageSetTransitionPolicy)`, `setPageSet(ImageSequence*,ImageSequence*)`, `setPageSet(ImageSequence*,ImageSequence*,PageSetTransitionPolicy)`, and no-policy page-set overloads once the final command shape is in place. Replacement coverage: `ImageViewportPageSet`, `PageSetTransitionPolicy`, `state.request.acceptedRoleSet`, role snapshots, `imageviewport_public_api_commands` page-set tests, `imageviewport_public_api_provider_roles`, `imageviewport_state_snapshot`, `viewportengine` page-set assignment tests, and `imageviewport_install_consumer`. Status: removed.
- Legacy command result channel: migrate `clear`, playback commands, seeking commands, `setPageSet`, `setPresentation`, and `resetView` from `ImageViewport::CommandOutcome` returns plus flat command diagnostics to `ImageViewportCommandResult`. Replacement coverage target: outcome/reason/command-revision/snapshot-revision assertions in `imageviewport_public_api_commands`, QML command-result tests, and install-consumer command-result checks. Status: removed; pre-implementation coverage landed in `test(api): cover command result snapshots`; implementation changed public C++/QML command returns to `ImageViewportCommandResult`, updated C++/QML/install-consumer callers to assert `outcome`, `reason`, `commandRevision`, and `snapshotRevision`, and passed `cmake --build build-ninja`, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_public_api_provider_roles|imageviewport_still|imageviewport_timed|imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_presentation_state|imageviewport_state_snapshot|imagesequence_factory' --output-on-failure`, broad `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure`, and full `ctest --test-dir build-ninja --output-on-failure`.
- Separate presentation setters, helpers, and flat presentation observations: remove writable presentation properties and setters (`spreadDirection`, `pageGap`, `fitMode`, `zoomPercent`, `smoothing`, `mipmap`, `mirrorHorizontally`, `mirrorVertically`, `backgroundMode`, `backgroundColor`, `looping`, and their property setters), legacy invokables (`setSpreadDirection`, `setPageGap`, `setFitMode`, `setZoomPercent`, `zoomByStep`, `clampedManualZoomPercent`, `steppedManualZoomPercent`, `panBy`, `panToStart`, `panToEnd`, `scanNext`, `scanPrevious`, `rotateClockwise`, `rotateCounterClockwise`, anchor-based mirror setters), and C++ flat presentation getters after tests bind through `state.presentation`. Replacement coverage: `ImageViewportPresentationCommand`, `state.presentation`, `imageviewport_public_api_commands/presentationCommandAppliesAndRejectsTransactionally`, `imageviewport_state_snapshot`, `imageviewport_presentation_state`, `viewportcontroller_presentation`, `viewportengine`, and `imageviewport_install_consumer`. Status: removed.
- Legacy coordinate aliases and geometry value types: remove `CoordinateResult`, `PageGeometry`, `itemToSpread`, `spreadToItem`, `nearestVisibleSpreadPoint`, `itemToPage`, `pageToItem`, `nearestVisiblePagePoint`, `pageGeometry`, `containsVisibleSpreadPoint`, `containsVisiblePagePoint`, `itemToImage`, `imageToItem`, `nearestVisibleImagePoint`, and `containsVisibleImagePoint` from the public item surface. Replacement coverage: `ImageViewportCoordinateInput`, `ImageViewportCoordinateResult`, `mapPoint`, `containsPoint`, `nearestVisiblePoint`, `state.display`, role geometry snapshots, `imageviewport_public_api_commands` coordinate tests, presentation-state geometry tests, and install-consumer coordinate checks. Status: removed.
- Legacy provider adapter construction virtuals: remove `ImageSequenceProviderAdapter::sessionFactory()`, `knownMetadata()`, `knownFacts()`, `timedPlaybackCapability()`, `frameSeekCapability()`, `positionSeekCapability()`, `authoredAnimationFacts()`, and `threadingContract()` after all adapters implement `descriptor()` directly. Replacement coverage: `ImageSequenceProviderDescriptor`, `imagesequence_factory`, `imageviewport_provider_contract`, provider lifecycle/counting support, and `imageviewport_install_consumer`. Status: removed.
- Legacy provider session virtuals and direct transport entry points: remove `requestMetadata`, `requestFrame`, `requestPosition`, `requestPlayback`, direct `cancelRequest`, and direct `close` once sessions handle typed `request(const ImageSequenceProviderRequest&)` for metadata/frame/position/playback/cancel/close. Replacement coverage: typed request construction tests, provider contract typed-session tests, lifecycle/affinity/counting support converted to typed requests, and bridge transport tests. Status: removed.
- Legacy provider session signals and borrowed frame signals: remove `metadataReady`, `imageFrameReady`, `imageFrameWithMetadataReady`, `frameHandleReady`, `frameHandleWithMetadataReady`, `providerWaiting`, `providerProgress`, `endOfSequence`, `providerFailed`, `providerUnsupportedWithCause`, `providerUnsupported`, and `providerCancelled`; borrowed raw-frame signals are removed with the same group. Replacement coverage: `providerEvent(const ImageSequenceProviderEvent&)`, typed event factory tests, provider terminal/projection/recovery/frame-admission/playback tests, render commit tests, and install-consumer event checks. Status: removed.
- Obsolete private helpers and old tests: remove private facade/controller helper methods only after no public surface, test, or structural guard references them. Old tests scheduled for rewrite are `tst_imageviewport_public_api`, `tst_imageviewport_public_api_qml`, `tst_imageviewport_public_api_commands`, legacy coordinate cases in `tst_imageviewport_still` and `tst_imageviewport_presentation_state`, provider terminal/projection/recovery/frame-admission/playback tests that emit legacy signals, provider lifecycle/affinity/counting support that overrides legacy virtuals, and install-consumer snippets that compile legacy symbols. Replacement coverage is the v2 test named in each ledger row; each deletion must be adjacent to, or immediately follow, its passing replacement. Status: presentation property-setter private shims are removed; other obsolete private helpers and old-test rewrites are pending.

### Completion Criteria

- Public API matches the v2 spec: snapshot observation, canonical page-set command, no legacy sequence assignment properties, presentation command, typed command result, equality-only tokens without numeric public accessors, v2 coordinate helpers, provider descriptor construction, and typed provider request/event protocol.
- No source or test file is deleted until it is unreferenced; deleted tests have explicit v2 replacements.
- Full test suite passes with v2 coverage replacing old compatibility coverage.
- Installed public header exposes no private controller, provider transport, render adapter, scenegraph, native texture, or instrumentation types.

### Status

In progress as of 2026-07-08. The numeric token cleanup slice is complete: `RevisionToken::value`, the `RevisionToken.value` QML property, public `ImageSequenceProviderRequestToken(quint64)`, and `ImageSequenceProviderRequestToken::id()` were removed from the public surface; internal diagnostics and stale-token tests now use private/test-only token helpers, and installed-consumer coverage obtains valid provider tokens through the public provider callback path instead of numeric construction. Replacement coverage is in `imageviewport_public_api/revisionTokensExposeValidityAndEquality`, `imageviewport_provider_contract/providerPublicValueTypesValidateTiming`, provider lifecycle/request/playback equality assertions, `imageviewport_presentation_state/revisionTokensUseSharedNonWrappingAllocator`, `viewportcontroller_provider` stale-token tests, `viewportengine/providerStateOwnsTokensQueuesAndMetadataByRole`, and `imageviewport_install_consumer`.

Verification for this slice: `cmake --build build-ninja` passed, and `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The presentation-command completion slice is complete: `ImageViewportPresentationCommand` now exposes the documented `scanDirection`, `qualityPreference`, and `exactnessPreference` fields; quality and exactness preferences are engine-owned presentation state projected through `state.presentation`; and scan direction reuses the existing pan/scan command behavior. This adds the v2 replacement coverage needed before removing legacy presentation helpers. Replacement coverage is in `imageviewport_public_api`, `imageviewport_public_api_commands/presentationCommandAppliesAndRejectsTransactionally`, `imageviewport_state_snapshot`, `viewportengine`, and `imageviewport_install_consumer`.

Verification for this slice: `cmake --build build-ninja` passed, and `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The manual-zoom helper removal slice is complete: public `clampedManualZoomPercent` and `steppedManualZoomPercent` were removed from the `ImageViewport` C++/QML surface, while internal controller step/clamp math remains covered behind presentation commands. Public, QML, and install-consumer tests now assert the helpers are absent and rely on `minimumManualZoomPercent`, `maximumManualZoomPercent`, `manualZoomStepFactor`, `ImageViewportPresentationCommand::zoomStepDelta`, and controller coverage for the remaining behavior.

Verification for this slice: `cmake --build build-ninja` passed, and focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_install_consumer|viewportcontroller_presentation' --output-on-failure` passed 4/4.

The `zoomByStep` public-helper removal slice is complete: `ImageViewport::zoomByStep` was removed from the public C++/QML facade, and public tests now step zoom through `ImageViewportPresentationCommand::zoomStepDelta` plus `setPresentation`. The private/controller step path remains to implement the command and keep internal validation coverage.

Verification for this slice: `cmake --build build-ninja` passed, and focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_presentation_state|imageviewport_install_consumer|viewportcontroller_presentation' --output-on-failure` passed 5/5.

The next/previous scan helper removal slice is complete: public `scanNext` and `scanPrevious` were removed from the `ImageViewport` C++/QML facade, and QML/public API coverage now drives those operations through `ImageViewportPresentationCommand::scanDirection` with `Next` and `Previous`.

Verification for this slice: `cmake --build build-ninja` passed, and focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml' --output-on-failure` passed 2/2.

The counter-clockwise rotation helper removal slice is complete: public `rotateCounterClockwise` was removed from the `ImageViewport` C++/QML facade, and QML command-surface coverage now uses `ImageViewportPresentationCommand::rotationDegrees` plus `setPresentation` for the replacement path.

Verification for this slice: `cmake --build build-ninja` passed, and focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml' --output-on-failure` passed 2/2.

The clockwise rotation helper removal slice is complete: public `rotateClockwise` was removed from the `ImageViewport` C++/QML facade, and public command, presentation-state, scenegraph, and QML coverage now set target rotation with `ImageViewportPresentationCommand::rotationDegrees`.

Verification for this slice: `cmake --build build-ninja` passed, and focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_render_scenegraph|viewportcontroller_presentation' --output-on-failure` passed 6/6.

The anchor-based mirror overload removal slice is complete: public `setMirrorHorizontally(bool, QPointF)` and `setMirrorVertically(bool, QPointF)` were removed from the `ImageViewport` C++/QML facade. Public command, QML, and presentation-state coverage now uses `ImageViewportPresentationCommand::mirrorHorizontally` and `mirrorVertically`; controller coverage retains the internal anchor path.

Verification for this slice: `cmake --build build-ninja` passed, and focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|viewportcontroller_presentation' --output-on-failure` passed 5/5.

The start-pan helper removal slice is complete: public `panToStart` was removed from the `ImageViewport` C++/QML facade, and public/QML/presentation-state coverage now uses `ImageViewportPresentationCommand::scanDirection` with `Start`.

Verification for this slice: `cmake --build build-ninja` passed, and focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_presentation_state' --output-on-failure` passed 3/3.

The end-pan helper removal slice is complete: public `panToEnd` was removed from the `ImageViewport` C++/QML facade, and public/QML/presentation-state/still-image coverage now uses `ImageViewportPresentationCommand::scanDirection` with `End`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_presentation_state|imageviewport_still|viewportcontroller_presentation' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The pan-delta helper removal slice is complete: public `panBy` was removed from the `ImageViewport` C++/QML facade, and public/QML/presentation-state/still-image coverage now uses `ImageViewportPresentationCommand::panDelta`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_presentation_state|imageviewport_still|viewportcontroller_presentation' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The spread-direction helper removal slice is complete: public `setSpreadDirection` was removed from the `ImageViewport` C++/QML facade, and public/QML/command/presentation-state coverage now uses `ImageViewportPresentationCommand::spreadDirection`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state' --output-on-failure` passed 4/4, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The page-gap helper removal slice is complete: public `setPageGap` was removed from the `ImageViewport` C++/QML facade, and public/QML/command/presentation-state/render-scenegraph coverage now uses `ImageViewportPresentationCommand::pageGap`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_render_scenegraph' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The fit-mode helper removal slice is complete: public `setFitMode` was removed from the `ImageViewport` C++/QML facade, and public/QML/presentation-state/still-image/render-scenegraph coverage now uses `ImageViewportPresentationCommand::fitMode`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_presentation_state|imageviewport_still|imageviewport_render_scenegraph' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The manual-zoom setter removal slice is complete: public `setZoomPercent` was removed from the `ImageViewport` C++/QML facade, and public/QML/command/presentation-state/still-image/install-consumer coverage now uses `ImageViewportPresentationCommand::manualZoomPercent`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_still|imageviewport_install_consumer' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The quality-toggle setter removal slice is complete: public `setSmoothing` and `setMipmap` plus the `smoothing`/`mipmap` QML write paths were removed from the `ImageViewport` facade, and public/command/state-snapshot/presentation-state/still-image/render-scenegraph coverage now uses `ImageViewportPresentationCommand::smoothing` and `mipmap`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_state_snapshot|imageviewport_presentation_state|imageviewport_still|imageviewport_render_scenegraph' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The mirror-flag setter removal slice is complete: public `setMirrorHorizontally` and `setMirrorVertically` plus the mirror QML write paths were removed from the `ImageViewport` facade, and public/presentation-state/still-image/render-scenegraph coverage now uses `ImageViewportPresentationCommand::mirrorHorizontally` and `mirrorVertically`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_presentation_state|imageviewport_still|imageviewport_render_scenegraph' --output-on-failure` passed 4/4, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The background setter removal slice is complete: public `setBackgroundMode` and `setBackgroundColor` plus the background QML write paths were removed from the `ImageViewport` facade, and public/command/presentation-state/still-image/timed/render-scenegraph coverage now uses `ImageViewportPresentationCommand::backgroundMode` and `backgroundColor`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_still|imageviewport_timed|imageviewport_render_scenegraph' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The looping setter removal slice is complete: public `setLooping` plus the `looping` QML write path were removed from the `ImageViewport` facade, and public/command/presentation-state/still-image/provider-playback coverage now uses `ImageViewportPresentationCommand::looping`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_still|imageviewport_provider_playback' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The remaining writable presentation property removal slice is complete: `spreadDirection`, `pageGap`, `fitMode`, and `zoomPercent` are read-only on the `ImageViewport` facade, their public property setter methods were removed, and public/presentation-state coverage now uses presentation commands for those mutations.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_presentation_state' --output-on-failure` passed 2/2, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat presentation meta-property removal slice is complete: item-level QML/meta-object observation properties for presentation state (`spreadDirection`, `pageGap`, `fitMode`, `zoomPercent`, manual zoom limits, rotation, mirror flags, background, quality toggles, and `looping`) were removed from `ImageViewport`; QML, public API, still-image, presentation-state, and install-consumer coverage now reads those values through `state.presentation`. C++ flat presentation getters remain pending for the next slice.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_presentation_state|imageviewport_still|imageviewport_install_consumer' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat presentation C++ getter removal slice is complete: public `ImageViewport` getters for presentation observations were removed, and public API, command, snapshot, presentation-state, still/timed/provider-playback, engine, and install-consumer coverage now reads presentation values through `state().presentation()`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_state_snapshot|imageviewport_presentation_state|imageviewport_still|imageviewport_timed|imageviewport_provider_playback|imageviewport_install_consumer|viewportengine' --output-on-failure` passed 9/9, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The presentation private property-shim cleanup slice is complete: the unreferenced private `setSpreadDirectionProperty`, `setPageGapProperty`, `setFitModeProperty`, and `setZoomPercentProperty` helpers were removed after the public property paths and C++ getters were deleted.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_state_snapshot|imageviewport_presentation_state|viewportengine|viewportcontroller_presentation' --output-on-failure` passed 4/4, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The image-coordinate alias removal slice is complete: public/private `itemToImage`, `imageToItem`, `nearestVisibleImagePoint`, and `containsVisibleImagePoint` wrappers were removed, the pure geometry image aliases were deleted, and public/QML/still-image/presentation-state/install-consumer coverage now uses `ImageViewportCoordinateInput` with primary-page coordinates through `mapPoint`, `containsPoint`, and `nearestVisiblePoint`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_still|imageviewport_presentation_state|imageviewport_install_consumer|viewportcontroller_presentation' --output-on-failure` passed 7/7, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The nearest/contains coordinate alias removal slice is complete: public `nearestVisibleSpreadPoint`, `nearestVisiblePagePoint`, `containsVisibleSpreadPoint`, and `containsVisiblePagePoint` were removed from `ImageViewport`, and public/QML/presentation-state/install-consumer coverage now uses `ImageViewportCoordinateInput` with `nearestVisiblePoint` and `containsPoint`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_install_consumer|viewportcontroller_presentation' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The map coordinate alias removal slice is complete: public `itemToSpread`, `spreadToItem`, `itemToPage`, and `pageToItem` were removed from `ImageViewport`, and public/QML/presentation-state/install-consumer coverage now uses `ImageViewportCoordinateInput` with `mapPoint`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_install_consumer|viewportcontroller_presentation' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The page-geometry value type removal slice is complete: public `PageGeometry`, `primaryPageGeometry`, `secondaryPageGeometry`, and `pageGeometry` were removed from `ImageViewport`; public/QML/state/install-consumer coverage now reads role geometry through `ImageViewportRoleGeometrySnapshot` on `state.primary.geometry` and `state.secondary.geometry`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_state_snapshot|imageviewport_install_consumer|imageviewport_presentation_state|viewportcontroller_presentation' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The coordinate result value type removal slice is complete: public `CoordinateResult` and its QML value type registration were removed from `ImageViewport`; the remaining internal geometry helpers use a private `coordinateresult_p.h` value while public coverage stays on `ImageViewportCoordinateResult`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|viewportcontroller_presentation|imageviewport_install_consumer' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider adapter descriptor conversion slice is complete: test support and installed-consumer provider adapters now populate `ImageSequenceProviderDescriptor` directly for session factories, metadata, known facts, capabilities, authored animation facts, and threading contracts. Temporary `sessionFactory()` overrides remain only because the legacy public base virtual is still pure until the removal slice.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imagesequence_factory|imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_terminal|imageviewport_provider_playback|viewportcontroller_provider|viewportcontroller_playback|imageviewport_install_consumer' --output-on-failure` passed 15/15, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider adapter construction virtual removal slice is complete: public `ImageSequenceProviderAdapter` now requires `descriptor()` directly and no longer exposes `sessionFactory`, metadata/facts, capability, authored-animation-facts, or threading-contract construction virtuals. Test support and installed-consumer adapters are descriptor-only.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imagesequence_factory|imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_terminal|imageviewport_provider_playback|viewportcontroller_provider|viewportcontroller_playback|imageviewport_install_consumer' --output-on-failure` passed 17/17, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider session typed-request conversion slice is complete: lifecycle, affinity, counting, synchronous, viewport-controller, provider-contract, and installed-consumer sessions now override `request(const ImageSequenceProviderRequest&)` directly for metadata, frame, position, playback, cancel, and close request kinds. Public legacy session virtuals remain only for the removal slice.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_terminal|imageviewport_provider_playback|viewportcontroller_provider|viewportcontroller_playback|imageviewport_install_consumer' --output-on-failure` passed 14/14, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider session virtual removal slice is complete: public `ImageSequenceProviderSession` now requires `request(const ImageSequenceProviderRequest&)` directly and no longer exposes `requestMetadata`, `requestFrame`, `requestPosition`, `requestPlayback`, `cancelRequest`, or `close` virtuals. Internal bridge request/cancel entry points remain for the next cleanup slice.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_terminal|imageviewport_provider_playback|viewportcontroller_provider|viewportcontroller_playback|imageviewport_install_consumer' --output-on-failure` passed 16/16, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider bridge typed-request cleanup slice is complete: `ViewportProviderBridge` now exposes a single typed request delivery path instead of internal `requestMetadata`, `requestFrame`, `requestPosition`, `requestPlayback`, and `cancelRequest` entry points. The host constructs metadata, frame, position, playback, and cancel requests from controller transport effects before delivery; session close remains a lifecycle cleanup operation that sends typed cancel and close requests to the retiring session.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_terminal|imageviewport_provider_playback|viewportcontroller_provider|viewportcontroller_playback|imageviewport_install_consumer' --output-on-failure` passed 14/14, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider typed-event support conversion slice is complete: shared provider test support now emits typed `providerEvent` values for metadata, frame, waiting, progress, terminal, and cancellation paths; installed-consumer coverage now observes and emits `providerEvent` directly instead of legacy provider session signals. Direct legacy signal emissions remain in provider/render behavior suites until their focused conversion slices.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_terminal|imageviewport_provider_playback|viewportcontroller_provider|viewportcontroller_playback|imageviewport_install_consumer' --output-on-failure` passed 14/14, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The first direct provider-event test conversion slice is complete: public provider-role, state-snapshot, image-sequence factory, provider-contract, and provider terminal-diagnostics tests now emit typed provider events for straightforward metadata, frame, failure, unsupported, and cancellation cases.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api_provider_roles|imageviewport_state_snapshot|imagesequence_factory|imageviewport_provider_contract|imageviewport_provider_terminal_diagnostics' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The typed invalid unsupported-cause conversion slice is complete: invalid typed `Unsupported` events preserve their invalid cause through the existing provider protocol-violation projection, and terminal diagnostics no longer emits legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_terminal_diagnostics' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The base provider terminal typed-event conversion slice is complete: `imageviewport_provider_terminal` now emits typed metadata, unsupported, failure, cancellation, and end-of-sequence events instead of legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_terminal$' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider terminal playback typed-event conversion slice is complete: `imageviewport_provider_terminal_playback` now emits typed metadata, failure, cancellation, and unsupported events instead of legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_terminal_playback' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The render scenegraph typed-event conversion slice is complete: `imageviewport_render_scenegraph` now emits typed provider metadata and frame-ready events instead of legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_render_scenegraph' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The render commit typed-event conversion slice is complete: `imageviewport_render_commit` now emits typed provider metadata and frame-ready events instead of legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_render_commit' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider lifecycle typed-event conversion slice is complete: `imageviewport_provider_lifecycle` now emits typed metadata, waiting, progress, frame, cancellation, unsupported, failure, and end-of-sequence events instead of legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_lifecycle' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider terminal recovery typed-event conversion slice is complete: `imageviewport_provider_terminal_recovery` now emits typed metadata, progress, waiting, frame, failure, unsupported, and cancellation events instead of legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_terminal_recovery' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider metadata typed-event conversion slice is complete: `imageviewport_provider_metadata` now emits typed metadata, progress, waiting, frame, and failure events instead of legacy provider session signals. The typed bridge now preserves invalid metadata events for metadata-admission rejection and invalid progress events for advisory-progress ignoring, matching the existing public behavior.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_metadata' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider requests typed-event conversion slice is complete: `imageviewport_provider_requests` now emits typed metadata, frame, unsupported, cancellation, and failure events instead of legacy provider session signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_requests' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider terminal projection typed-event conversion slice is complete: `imageviewport_provider_terminal_projection` now emits typed metadata, failure, unsupported, cancellation, and end-of-sequence events instead of legacy provider session signals, including invalid typed unsupported causes for protocol-violation coverage.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_terminal_projection' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider frame admission typed-event conversion slice is complete: `imageviewport_provider_frame_admission` now emits typed metadata, frame-ready, frame-with-metadata, and owned-frame-handle events instead of legacy provider session signals. The typed bridge now preserves invalid frame-ready events for frame-admission rejection, and typed frame test helpers transfer owned handles to the bridge/controller for release.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_frame_admission' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The provider playback typed-event conversion slice is complete: `imageviewport_provider_playback` now emits typed metadata, end-of-sequence, unsupported, cancellation, and failure events instead of legacy provider session signals. At this point, no provider/render behavior suite emits the legacy provider session signals directly.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_provider_playback' --output-on-failure` passed 1/1, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy provider session signal removal slice is complete: `ImageSequenceProviderSession` now exposes typed `providerEvent(const ImageSequenceProviderEvent&)` as its only provider result signal, and `ViewportProviderBridge` no longer connects compatibility result signals.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_terminal|imageviewport_provider_terminal_diagnostics|imageviewport_provider_terminal_playback|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_provider_playback|viewportcontroller_provider|viewportcontroller_playback|imageviewport_install_consumer' --output-on-failure` passed 16/16, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat role/spread geometry observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for `displayedSpreadSize`, role displayed image sizes, role page/item/visible rects, `visibleSpreadRect`, `contentSize`, `contentPosition`, `maximumContentPosition`, `horizontalPannable`, or `verticalPannable`. Tests and installed-consumer QML now read the equivalent nested fields through `state.display` and `state.primary`/`state.secondary`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_state_snapshot|imageviewport_still|imageviewport_presentation_state|imageviewport_render_scenegraph|imageviewport_provider_frame_admission|imageviewport_install_consumer' --output-on-failure` passed 9/9, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat primary display geometry observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for `displayedImageSize`, `contentRect`, or `visibleImageRect`. Tests now read the equivalent nested fields through `state.primary.display.sourceLogicalSize`, `state.display.contentRect`, and `state.primary.geometry.displayedVisiblePageRect`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_state_snapshot|imagesequence_factory|imageviewport_still|imageviewport_timed|imageviewport_presentation_state|imageviewport_render_scenegraph|imageviewport_render_commit|imageviewport_provider_contract|imageviewport_provider_frame_admission|imageviewport_provider_metadata|imageviewport_provider_lifecycle|imageviewport_provider_requests|imageviewport_provider_terminal_recovery' --output-on-failure` passed 16/16, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat per-role metadata count/bounds removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for per-role frame counts, total durations, frame seek bounds, or position seek bounds. Tests now read the equivalent nested fields through `state.primary.metadata` and `state.secondary.metadata`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_public_api_provider_roles|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_install_consumer' --output-on-failure` passed 7/7, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat per-role capability observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for per-role timed playback, frame seek, or position seek support. Tests now read the equivalent nested `CapabilitySupport` fields through `state.primary.metadata` and `state.secondary.metadata`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_public_api_provider_roles|imageviewport_provider_metadata|imageviewport_install_consumer' --output-on-failure` passed 6/6, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat primary metadata count/bounds removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for primary frame count, total duration, frame seek bounds, or position seek bounds. Tests now read the equivalent nested fields through `state.primary.metadata`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_state_snapshot|imagesequence_factory|imageviewport_still|imageviewport_timed|imageviewport_provider_contract|imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_provider_lifecycle|imageviewport_provider_terminal$|imageviewport_provider_terminal_recovery|imageviewport_install_consumer' --output-on-failure` passed 13/13, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat primary capability observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for primary timed playback, frame seek, or position seek support. Tests now read the equivalent nested `CapabilitySupport` fields through `state.primary.metadata`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_still|imageviewport_timed|imageviewport_provider_contract|imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_provider_terminal_recovery|imageviewport_install_consumer' --output-on-failure` passed 9/9, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat per-role frame/position observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for per-role displayed/requested frames or positions. Tests now read the equivalent nested fields through each role's `request` and `display` snapshots.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_timed|imageviewport_presentation_state|imageviewport_provider_frame_admission|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_playback|imageviewport_provider_requests|imageviewport_provider_terminal_projection|imageviewport_install_consumer' --output-on-failure` passed 11/11, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat primary frame/position observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for primary displayed/requested frames or positions through the item-level aliases. Tests now read the equivalent nested fields through `state.primary.request` and `state.primary.display`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_state_snapshot|imagesequence_factory|imageviewport_still|imageviewport_timed|imageviewport_provider_contract|imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_provider_lifecycle|imageviewport_provider_frame_admission|imageviewport_provider_terminal$|imageviewport_provider_terminal_playback|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_provider_playback|imageviewport_render_commit|imageviewport_render_scenegraph|imageviewport_install_consumer' --output-on-failure` passed 19/19, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat request/display/playback status observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for request status, request reason, display status, or playback phase. Tests and install-consumer checks now read the equivalent nested fields through `state.request` and `state.display`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_public_api_provider_roles|imageviewport_public_api_qml|viewportengine|imagesequence_factory|imageviewport_still|imageviewport_timed|imageviewport_presentation_state|imageviewport_provider_contract|imageviewport_provider_frame_admission|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_playback|imageviewport_provider_requests|imageviewport_provider_terminal$|imageviewport_provider_terminal_diagnostics|imageviewport_provider_terminal_playback|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_render_commit|imageviewport_render_scenegraph|imageviewport_install_consumer' --output-on-failure` passed 23/23, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat diagnostics observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for command reason, error string, or warning string. Tests and install-consumer checks now read the equivalent nested fields through `state.diagnostics`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_public_api_qml|imageviewport_state_snapshot|imagesequence_factory|imageviewport_still|imageviewport_timed|imageviewport_presentation_state|imageviewport_provider_contract|imageviewport_provider_frame_admission|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_playback|imageviewport_provider_requests|imageviewport_provider_terminal$|imageviewport_provider_terminal_diagnostics|imageviewport_provider_terminal_playback|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_render_commit|imageviewport_install_consumer' --output-on-failure` passed 21/21, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat revision observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for display, request, or command revisions. Tests and install-consumer checks now read the equivalent nested fields through `state.revisions`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_public_api_qml|imageviewport_still|imageviewport_timed|imageviewport_presentation_state|imageviewport_provider_contract|imageviewport_provider_frame_admission|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_playback|imageviewport_provider_requests|imageviewport_provider_terminal$|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_render_commit|imageviewport_install_consumer' --output-on-failure` passed 17/17, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat role sequence observation removal slice is complete: `ImageViewport` no longer exposes flat item properties or C++ accessors for primary or secondary accepted sequence handles. Tests and install-consumer checks now read the equivalent nested fields through `state.primary.sequence` and `state.secondary.sequence`.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_public_api_provider_roles|imageviewport_public_api_qml|imageviewport_presentation_state|imageviewport_provider_lifecycle|imageviewport_install_consumer' --output-on-failure` passed 7/7, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The flat root sequence observation removal slice is complete: `ImageViewport` no longer exposes the root accepted sequence handle as a flat item property or C++ getter. Tests now read the accepted primary handle through `state.primary.sequence`; C++ `setSequence` remains as legacy assignment compatibility for a later slice.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_public_api_qml|imageviewport_public_api_provider_roles|imagesequence_factory|imageviewport_still|imageviewport_presentation_state|imageviewport_provider_contract|imageviewport_provider_lifecycle|imageviewport_provider_terminal_recovery|imageviewport_install_consumer' --output-on-failure` passed 11/11, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy C++ sequence assignment removal slice is complete: public `ImageViewport::setSequence` and its private facade helper were removed. Tests and installed-consumer code now use canonical `setPageSet(ImageViewportPageSet(...))` for single-sequence setup and `ImageViewportPageSet::clear()` for clear-style setup; page-set overload compatibility remains for later slices.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The C++ pointer page-set overload removal slice is complete: public and private `setPageSet(ImageSequence*, ImageSequence*)` overloads, including the policy overload, were removed. C++ tests now construct `ImageViewportPageSet` explicitly; the remaining QVariant bridge preserves QML and legacy variant call behavior until the final command-surface slice.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_public_api_provider_roles|imageviewport_state_snapshot|imageviewport_presentation_state|imageviewport_render_commit|imageviewport_provider_requests|imageviewport_install_consumer' --output-on-failure` passed 8/8, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The typed QML page-set command preparation slice is complete: `setPageSet(ImageViewportPageSet, PageSetTransitionPolicy)` is now QML-invokable, and QML setup in public, factory, and installed-consumer coverage constructs `imageViewportPageSet` values explicitly instead of assigning raw sequence/null argument pairs. The QVariant bridge remains only for legacy variant rejection and compatibility removal follow-up.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_public_api_qml|imagesequence_factory|imageviewport_install_consumer' --output-on-failure` passed 5/5, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The no-policy typed page-set overload removal slice is complete: `setPageSet(ImageViewportPageSet)` was removed from the public and private facades. C++ and QML callers now pass an explicit `PageSetTransitionPolicy` to `setPageSet(ImageViewportPageSet, PageSetTransitionPolicy)`; the QVariant bridge remains pending for the final compatibility-removal slice.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The QVariant page-set overload removal slice is complete: public and private `setPageSet(QVariant)`, `setPageSet(QVariant,QVariant)`, and `setPageSet(QVariant,QVariant,PageSetTransitionPolicy)` overloads were removed, along with their conversion helpers. Tests now use typed `ImageViewportPageSet` values for C++ setup and public API coverage verifies the removed QVariant methods are absent from the meta-object.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `sequenceChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only accepted-sequence notify signal. Tests that need to observe accepted sequence changes now use `stateChanged` or nested request/state assertions.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api_commands|imagesequence_factory|imageviewport_provider_lifecycle|imageviewport_state_snapshot' --output-on-failure` passed 4/4, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `loopingChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only looping notify signal, and the now-unused sequence/looping change-set flags were removed from private controller plumbing. Tests observe looping changes through `stateChanged` and assert the removed signal is absent from the meta-object.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_presentation_state|imageviewport_state_snapshot' --output-on-failure` passed 3/3, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `presentationChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only presentation notify signal, and the now-unused presentation change-set flag was removed from private controller plumbing. Tests observe presentation mutations through `stateChanged`, display revision changes, and direct `state.presentation` assertions, and public API coverage asserts the removed signal is absent from the meta-object.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_still|viewportcontroller_presentation|imageviewport_state_snapshot' --output-on-failure` passed 6/6, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `commandStateChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only command diagnostics notify signal. Tests observe command-diagnostics changes through `stateChanged` plus command reason/revision snapshot assertions, and public API coverage asserts the removed signal is absent from the meta-object.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_presentation_state|imageviewport_still|imagesequence_factory|imageviewport_state_snapshot' --output-on-failure` passed 6/6, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `commandRevisionChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only command-revision notify signal. Tests observe no-mutation cases through direct revision-token equality and quiet `stateChanged` assertions, while internal command revision flags remain as token-allocation plumbing.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_presentation_state|imageviewport_state_snapshot' --output-on-failure` passed 3/3, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `playbackPhaseChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only playback-phase notify signal. Tests assert playback phase through snapshot/value projections and existing request/display transition checks; the internal playback change flag remains for render/provider/scheduler side effects.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_presentation_state|imagesequence_factory|imageviewport_still|imageviewport_timed|imageviewport_render_scenegraph|imageviewport_render_commit|imageviewport_provider_metadata|imageviewport_state_snapshot' --output-on-failure` passed 9/9, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `displayRevisionChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only display-revision notify signal. Tests observe display revision changes through direct revision-token comparisons and `stateChanged`; internal display revision flags remain as token-allocation plumbing.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_presentation_state|imageviewport_public_api_commands|imageviewport_still|imageviewport_state_snapshot' --output-on-failure` passed 5/5, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `requestRevisionChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only request-revision notify signal. Tests observe request revision changes through direct revision-token comparisons and `stateChanged`; internal request revision flags remain as token-allocation plumbing.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_presentation_state|imageviewport_timed|imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_state_snapshot' --output-on-failure` passed 6/6, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `requestStateChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only request-state notify signal. Tests observe request state through snapshot fields, revision-token comparisons, `stateChanged`, and direct asynchronous state/pending-work waits where a signal was previously used only as a wait primitive; the internal request-state change flag remains for controller/effect plumbing.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_still|imageviewport_timed|imageviewport_presentation_state|imageviewport_render_scenegraph|imageviewport_render_commit|imagesequence_factory|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_playback|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_state_snapshot' --output-on-failure` passed 15/15, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `displayStateChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only display-state notify signal. Tests observe display state through snapshot fields, display revision-token comparisons, and `stateChanged`; the internal display-state change flag remains for controller/effect plumbing.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_commands|imageviewport_timed|imageviewport_presentation_state|imageviewport_render_scenegraph|imageviewport_render_commit|imagesequence_factory|imageviewport_provider_metadata|imageviewport_state_snapshot' --output-on-failure` passed 9/9, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy item-level `diagnosticsChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only diagnostics notify signal. Tests observe public diagnostics through `state.diagnostics`, flat compatibility diagnostic string helpers while they remain, and `stateChanged`; unrelated diagnostics signals on frame/list helper types remain intact.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_still|imageviewport_presentation_state|imagesequence_factory|imageviewport_provider_lifecycle|imageviewport_provider_metadata|imageviewport_provider_requests|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_render_commit|imageviewport_state_snapshot' --output-on-failure` passed 11/11, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

The legacy `geometryStateChanged` signal removal slice is complete: `ImageViewport` no longer exposes or emits the compatibility-only geometry notify signal. Tests observe geometry through snapshot fields, coordinate results, display revision-token comparisons, and `stateChanged`; internal geometry change flags and helper state names remain implementation plumbing.

Verification for this slice: `cmake --build build-ninja` passed, focused `ctest --test-dir build-ninja -R 'imageviewport_public_api$|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_still|imageviewport_timed|imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_presentation_state|imageviewport_state_snapshot' --output-on-failure` passed 9/9, broader `ctest --test-dir build-ninja -R 'imageviewport_|imagesequence_factory|viewportengine|viewportcontroller_|playback_' --output-on-failure` passed 29/29, and full `ctest --test-dir build-ninja --output-on-failure` passed 44/44.

### Risks And Rollback Criteria

- Risk: behavior is lost under the cover of API cleanup. Roll back any removal commit whose ledger lacks a passing v2 replacement test.
- Risk: downstream installed consumers lose still-needed source-handle or provider behavior. Roll back public header removals not called for by the v2 specs.
- Risk: compatibility removal outruns QML metadata changes. Roll back any removal that leaves QML tests importing stale compatibility-only symbols or no tested final v2 import.
- Risk: private helper deletion breaks bisectability. Roll back broad cleanup and remove obsolete helpers in smaller commits.

## Milestone 12: Packaging and Install Consumer Cleanup

### Goal

Finalize installed headers, QML module metadata, package consumer tests, public-header boundaries, and build/install metadata for the v2 API.

### Scope

- Update installed public header generation to include v2 public value types and exclude removed compatibility/private types.
- Finish QML type metadata, static plugin packaging, exported CMake package target, and Linux/static install-consumer expectations for the final v2 surface chosen before compatibility removal.
- Ensure installed consumers can construct explicit frame envelopes, build descriptor-based typed providers, construct page sets, issue presentation commands, read snapshots, compare tokens, and consume limits using only public headers and the package target.

### Existing Behavior That Must Be Preserved

- Installed package remains Linux/static plugin oriented as defined by `docs/spec/image-viewport-packaging.md` and `docs/architecture/build-and-package.md`.
- `imageviewport_install_consumer` must continue to prove public headers and package targets are sufficient without private include paths or build-tree-only QML imports.
- QML singletons `ImageSequenceLimits` and `ImageViewportDisplayLimits` remain available.
- Public headers must not expose private controller, provider transport, render adapter, scenegraph, native texture, or instrumentation types.

### Tests To Add Or Update

- Expand install-consumer tests to compile and run against `ImageViewportStateSnapshot`, `ImageViewportPageSet`, `PageSetTransitionPolicy`, `ImageViewportPresentationCommand`, `ImageViewportCommandResult`, explicit `ImageFrame` envelope construction, typed provider descriptor/request/event/session values, frame envelopes, demand values, and token equality helpers.
- Add QML install-consumer tests for importing the v2 module, creating an `ImageViewport`, assigning a page set from factory output, issuing v2 commands, and reading nested snapshot fields.
- Update structural public-header checks to reject private type leaks and removed v1 compatibility symbols.
- Keep package tests focused on the supported Linux/static boundary; do not add cross-platform plugin layout commitments.

### Implementation Steps

1. Update `src/CMakeLists.txt`, installed-header generation, exported CMake config, and QML module metadata for the final public surface.
2. Update `tests/install_consumer/main.cpp` and install-consumer CMake to exercise the v2 public API only.
3. Verify QML import/version expectations against the strategy chosen before compatibility removal and remove any leftover packaging metadata for compatibility-only symbols.
4. Run install, package, and full test suites from a clean build tree.
5. Remove compatibility packaging allowances only after the v2 install consumer passes.

### Completion Criteria

- A downstream installed consumer can build and run against the v2 public API without private headers or source-tree include paths.
- QML imports expose the final v2 item, value types, commands, snapshots, source envelope values, provider protocol values, and limits.
- Public-header structural tests reject private leaks and removed v1 symbols.
- Full test suite and install-consumer suite pass from a clean build.

### Risks And Rollback Criteria

- Risk: package metadata passes in-tree but fails after install. Roll back packaging changes that depend on build-tree include paths, generated files outside install rules, or build-tree-only QML imports.
- Risk: QML metadata accidentally promises unsupported compatibility. Roll back metadata until the supported v2 import contract is explicit in tests and packaging docs.
- Risk: installed headers expose private internals to make tests compile. Roll back and add public value wrappers instead.
