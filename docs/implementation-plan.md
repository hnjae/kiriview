# Implementation Plan

## Purpose

This plan consolidates the design review findings into executable implementation work for future `/goal` sessions. It translates the reviewed correct end state into safe, incremental milestones without requiring future agents to read the temporary design review document.

## Source Basis

- `docs/spec/**`
- `docs/architecture/**`

`DESIGN_REVIEW_CORRECT_END_STATE.md` was used as a temporary planning input, and its actionable content has been fully consolidated here. Future implementation sessions must use this plan plus the durable source basis above; the temporary design review is no longer required for execution.

No temporary design review finding was overridden by the authoritative docs. The review found implementation conformance and architecture-execution issues, not defects in `docs/spec/**` or `docs/architecture/**`. This planning pass added one durable API clarification: direct assignment to writable presentation properties whose command-style mutation takes an anchor uses the current viewport item center as the implicit item-space anchor.

## Execution Principles

- Preserve current behavior unless fixing a documented bug, spec violation, or architecture-contract violation described in this plan.
- Add characterization tests before risky refactors, lifecycle changes, scheduler changes, public observable behavior changes, or diagnostic plumbing changes.
- Do not add unresolved failing tests in a baseline milestone. A milestone that fixes a conformance gap should add its focused failing test first, then make it pass in the same milestone.
- Keep changes incremental; each milestone should be small enough to review and revert independently.
- Keep authoritative docs immutable during implementation unless a newly discovered blocker proves the durable end state is ambiguous or inconsistent; if that happens, stop and update this plan before changing behavior.
- Isolate pure controller/domain logic from provider transport, render synchronization, Qt event scheduling, playback timers, and other external effects.
- Preserve public APIs unless the durable specs explicitly require an API shape change; internal diagnostics must not become public API unless the specs are updated first.
- Keep public diagnostic strings bounded, redacted, and stable unless a durable spec change says otherwise.
- Update this plan if implementation discovers a blocker, invalid assumption, or safer milestone split.

## Consolidated Design Findings

### Finding 1: Retained Secondary Display Observations Are Hidden After Primary-Only Replacement

- Priority: P1.
- Affected areas: `src/imageviewport.cpp`, `src/imageviewportstate_p.h`, retained-display tests around spread-to-single replacement.
- Correct end state: displayed role observations derive from committed display state, including retained display role presence; accepted role observations derive from the accepted page set. A primary-only replacement can expose no accepted secondary role while retained secondary displayed frame, displayed position, displayed size, and geometry remain available when retained secondary pixels are visible.
- Why it matters: the specs separate accepted request state from visible retained display state. Current secondary displayed getters can hide retained secondary target identity even while secondary retained geometry remains visible.

### Finding 2: `PreserveManualPercent` Transition Has No Distinct Implementation

- Priority: P2.
- Affected areas: `src/viewportcontrollerhelpers_p.h`, `src/viewportcontrollerpresentation.cpp`, page-set transition policy tests.
- Correct end state: `PageSetTransitionPolicy::ZoomTransition::PreserveManualPercent` is implemented at the controller page-set transition boundary and is distinguishable from `Preserve` when the resulting fit mode is `Manual`; non-manual resulting fit modes continue to derive effective zoom from fit rules.
- Why it matters: a documented public transition value currently has no distinct behavior, so callers cannot rely on the public contract.

### Finding 3: Writable Presentation Properties Need Command-Equivalent Mutation Semantics

- Priority: P2.
- Affected areas: `src/imageviewportpresentation.cpp`, `src/viewportcontrollerpresentation.cpp`, public API and presentation-state tests.
- Correct end state: direct assignment to writable presentation properties enters the same controller mutation semantics as the command-style setter. For command equivalents with an anchor parameter, direct assignment uses the current viewport item center as the implicit item-space anchor.
- Why it matters: public property assignment and imperative commands should have the same validation, notification, diagnostic, revision, and geometry effects for the same presentation mutation.

### Finding 4: Role Ownership Is Duplicated Instead Of Role-Indexed

- Priority: P1.
- Affected areas: `src/imageviewportstate_p.h`, `src/viewportcontroller_p.h`, `src/viewportcontrollerhelpers_p.h`, `src/viewportcontroller.cpp`, `src/viewportcontrollerprovider.cpp`, `src/viewportcontrollerplayback.cpp`, `src/viewportcontrollerrender.cpp`.
- Correct end state: role-owned request, display, provider, and prepared-payload state are accessed through a small role-indexed abstraction addressed by `PageRole`; aggregate spread state remains separate for complete-spread readiness, terminal projection, and retained display decisions.
- Why it matters: provider and playback behavior must be role-symmetric. Parallel primary and secondary fields make symmetry fragile and increase change impact.

### Finding 5: Revision Tokens Can Wrap To Invalid Or Reused Values

- Priority: P1.
- Affected areas: `src/imageviewport.h`, `src/imageviewportstate_p.h`, `src/viewportcontroller.cpp`, `src/imageviewport.cpp`, revision-token tests.
- Correct end state: revision-token generation is owned by one controller-level allocator that guarantees non-zero, non-reused tokens within an item lifetime, using a representation wide enough for the contract or an explicit overflow guard.
- Why it matters: public revision tokens are documented as opaque, monotonic, distinct within an item lifetime, and never reset except item destruction. Unchecked `uint` counters can publish invalid or reused tokens after overflow.

### Finding 6: Controller Internals Are Exposed Through A Broad Mutable Port

- Priority: P2.
- Affected areas: `src/viewportcontroller_p.h`, `src/viewportcontrollerhelpers_p.h`, `src/viewportcontrollerprovider.cpp`, `src/viewportcontrollerplayback.cpp`, `src/viewportcontrollerpresentation.cpp`, `src/viewportcontrollerrender.cpp`.
- Correct end state: subsystem-specific controller implementation units use narrow internal facades or explicit state slices instead of one broad mutable `ViewportControllerPort`; shared helpers are domain-specific or domain-neutral, not an all-access mutation surface.
- Why it matters: broad mutable access obscures ownership and makes provider, presentation, render, and playback logic easy to couple accidentally.

### Finding 7: `ImageViewportPrivate` Owns Too Many Boundary Roles

- Priority: P2.
- Affected areas: `src/imageviewport_p.h`, `src/imageviewportprovider.cpp`, `src/imageviewportrender.cpp`, `src/imageviewportcontroller.cpp`, `src/imageviewport_p.cpp`.
- Correct end state: `ImageViewportPrivate` remains the QML-facing owner and typed-output applier; provider transport client behavior, render synchronization, and playback timer wiring move behind narrow item-side host objects.
- Why it matters: the item private class currently acts as public item facade, controller context, provider bridge client, render adapter host, playback scheduler, and test-probe holder, making subsystem ownership hard to audit.

### Finding 8: Secondary Provider Opens After Primary Spread Failure

- Priority: P1.
- Affected areas: `src/imageviewport.cpp`, `src/imageviewportprovider.cpp`, `src/viewportcontrollerprovider.cpp`, provider lifecycle tests.
- Correct end state: provider session opening is one controller-authorized lifecycle flow. Once any required role seals the active target spread terminal, no later role session opens for that generation; if a session is somehow opened after terminal sealing, it is immediately closed through a controller-authorized close effect.
- Why it matters: current sequencing can perform external provider side effects after a two-provider spread has already failed, allocating a secondary session that receives no useful request path.

### Finding 9: Command Diagnostic Publication Is Duplicated Across Controller Units

- Priority: P2.
- Affected areas: `src/viewportcontroller.cpp`, `src/viewportcontrollerpresentation.cpp`, `src/viewportcontrollerplayback.cpp`, `src/viewportcontrollerhelpers_p.h` or a new command-outcome helper module, command admission tests.
- Correct end state: one controller-owned helper/API creates command outcomes and owns command diagnostic/revision effects for accepted, accepted no-op, invalid, unsupported, ignored, and diagnostic-preserving exceptions.
- Why it matters: command admission order and diagnostic lifetime are public contracts. Duplicated file-local helpers make future commands likely to diverge.

### Finding 10: Deferred Provider Queue Flush Can Be Swallowed

- Priority: P1.
- Affected areas: `src/imageviewportprovider.cpp`, `src/viewportcontrollerprovider.cpp`, `src/viewportcontroller_p.h`, provider request tests.
- Correct end state: scheduling a deferred provider queue flush is an explicit checked external-effect operation. If scheduling fails, the controller receives a typed scheduler dispatch failure for the affected role and active request identity, clears queued state, emits internal attribution, and projects the active request as `Error / ProviderFailure`.
- Why it matters: current code can leave an accepted request indefinitely `Loading / RequestQueued` if `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` fails.

### Finding 11: Provider Cleanup Failures Disappear In Production

- Priority: P1.
- Affected areas: `src/viewportproviderbridge.cpp`, `src/imageviewportprovider.cpp`, `src/imageviewportstate_p.h`, provider lifecycle tests.
- Correct end state: provider transport emits production-capable internal diagnostics for cleanup delivery failures, including role, operation, token ids, queued/delivered state, and pending-cleanup state, while public request state remains unchanged for best-effort cleanup.
- Why it matters: failed close/cancel paths can leave provider work or sessions alive without enough production context to diagnose leaks or late callbacks.

### Finding 12: Render Failures Lose Production Attribution

- Priority: P2.
- Affected areas: `src/renderadapter.cpp`, `src/viewportcontrollerrender.cpp`, `src/imageviewportrender.cpp`, `src/imageviewportstate_p.h`, render commit tests.
- Correct end state: public request status and `errorString` remain coarse, but accepted render failures emit production-capable internal diagnostics containing failed role, generation, request id, prepared payload id, and `RenderFailureCause`.
- Why it matters: production cannot distinguish missing window, invalid role payload, texture creation failure, image node failure, or unknown backend failure even though the render boundary already classifies those causes internally.

## Milestones

### Milestone 1: Baseline Verification And Refactor Characterization

Status: Completed 2026-07-07.

#### Objective

Establish a clean executable baseline and add behavior-preserving characterization coverage needed before structural refactors. Do not add unresolved failing tests in this milestone.

#### Source findings

Finding 4, Finding 6, Finding 7, Finding 9.

#### Scope

- Discover and record the active local build/test commands if the default `build` directory is not usable.
- Audit existing coverage for command diagnostics, role symmetry, provider dispatch, render commit identity, playback driver behavior, and item-side integration boundaries.
- Add passing characterization tests for current behavior that later refactor milestones must preserve.
- Add structural discovery scripts or documented grep inventories only when they do not fail the current baseline.

#### Out of scope

- No implementation fixes for known conformance gaps.
- No intentionally failing tests for future milestones.
- No controller refactors.
- No public API changes.

#### Likely affected areas

- `tests/tst_imageviewport_public_api_commands.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- Existing structural CMake tests under `tests/*.cmake`

#### Tasks

- [x] Confirm the local build command; start with `cmake -S . -B build -DIMAGEVIEWPORT_BUILD_TESTS=ON -DIMAGEVIEWPORT_BUILD_EXAMPLES=OFF`.
- [x] Confirm the local full test command; start with `cmake --build build` and `ctest --test-dir build --output-on-failure`.
- [x] Audit existing command diagnostic tests for invalid, unsupported, ignored, accepted, accepted no-op, and diagnostic-preserving exceptions.
- [x] Add passing characterization tests for command diagnostic ordering and `setSpreadDirection(...)` / `setPageGap(...)` diagnostic-preserving exceptions if gaps exist.
- [x] Audit primary/secondary provider request tests and add passing shared or paired cases for current role-symmetric behavior where coverage is missing.
- [x] Audit render commit tests and add passing coverage for current role/payload identity handling where coverage is missing.
- [x] Audit playback tests and add passing coverage for active-driver selection, stop restoration, and secondary-role playback behavior where coverage is missing.
- [x] Inventory current role-specific fields, helper methods, and branches for later role-parametric milestones; do not fail the build on this inventory yet.
- [x] Inventory current command diagnostic mutation sites for later structural enforcement; do not fail the build on this inventory yet.

#### Completion evidence

- Build and full test commands confirmed: `cmake -S . -B build -DIMAGEVIEWPORT_BUILD_TESTS=ON -DIMAGEVIEWPORT_BUILD_EXAMPLES=OFF`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`.
- Command diagnostics audit: public command tests already cover invalid, unsupported, ignored, accepted, accepted no-op, and item-level diagnostic-preserving behavior; Milestone 1 added `viewportcontroller_presentation/spreadDirectionAndPageGapPreserveCommandDiagnosticsForInvalidAndNoop` to characterize the controller-level preservation path for standalone spread direction and page gap.
- Provider request role-symmetry audit: existing primary/secondary coverage in `imageviewport_provider_requests`, `imageviewport_provider_lifecycle`, and `viewportcontroller_provider` covers role-scoped metadata, frame, queueing, seek, stale-result, and secondary spread behavior, so no additional provider characterization was needed in this baseline milestone.
- Render identity audit: existing `imageviewport_render_commit` coverage includes primary and secondary payload identity matching, stale render commit/failure handling, and secondary-role render failure behavior, so no additional render characterization was needed in this baseline milestone.
- Playback audit: existing `viewportcontroller_playback`, `imageviewport_provider_playback`, `imageviewport_timed`, and `imageviewport_render_commit` coverage includes active-driver selection, stop restoration, waiting/resume behavior, and secondary-role playback command admission, so no additional playback characterization was needed in this baseline milestone.
- Structural inventory added as non-failing test `structural::refactorBaselineInventory`; it writes `build/tests/refactor-role-inventory.txt` and `build/tests/refactor-command-diagnostic-inventory.txt`. The baseline run produced 2,995 role-specific inventory entries and 344 command-diagnostic inventory entries for later allowlist conversion.

#### Acceptance criteria

- The project builds and the full current test suite passes.
- Refactor-sensitive behavior has passing characterization coverage or a documented reason for deferral in `Deferred / Needs Investigation`.
- The plan executor has inventories for command diagnostic mutation sites and role-specific state/method/branch hotspots to use in later structural milestones.
- No unresolved expected-failure tests are added.

#### Verification

- `cmake -S . -B build -DIMAGEVIEWPORT_BUILD_TESTS=ON -DIMAGEVIEWPORT_BUILD_EXAMPLES=OFF`
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`
- Targeted follow-up: `ctest --test-dir build -R 'imageviewport_public_api_commands|viewportcontroller_presentation|viewportcontroller_playback|viewportcontroller_provider|imageviewport_provider_requests|imageviewport_render_commit' --output-on-failure`

#### Risks / notes

- If the repo already uses a different build directory, use that directory and update this plan with the discovered command.
- This milestone should keep the suite green. Add conformance-failure tests in the milestone that fixes each conformance gap.

### Milestone 2: Public Behavior Conformance Fixes

Status: Completed 2026-07-07.

#### Objective

Fix user-visible conformance issues already specified by the durable docs: retained secondary displayed observations, `PreserveManualPercent`, direct property assignment semantics, and revision-token lifetime guarantees.

#### Source findings

Finding 1, Finding 2, Finding 3, Finding 5.

#### Scope

- Add focused failing tests first for each behavior gap in this milestone.
- Add displayed-role availability or equivalent display-state ownership so retained secondary displayed observations can be projected independently from accepted secondary role presence.
- Implement `PreserveManualPercent` at the page-set transition application boundary.
- Route direct assignment for anchor-preserving writable presentation properties through command-equivalent semantics using the current viewport item center as the implicit item-space anchor.
- Replace unchecked `uint` revision increments with a non-zero, non-reused revision-token allocator or explicit unreachable overflow guard.

#### Out of scope

- No role-indexed state refactor beyond what is required for displayed-role availability.
- No broad presentation geometry rewrite.
- No public diagnostic string changes.
- No provider lifecycle or transport changes.

#### Likely affected areas

- `src/imageviewport.cpp`
- `src/imageviewport.h`
- `src/imageviewportstate_p.h`
- `src/imageviewportpresentation.cpp`
- `src/viewportcontroller.cpp`
- `src/viewportcontrollerpresentation.cpp`
- `src/viewportcontrollerhelpers_p.h`
- `tests/tst_imageviewport_presentation_state.cpp`
- `tests/tst_imageviewport_public_api_commands.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_imageviewport_public_api.cpp`

#### Tasks

- [x] Add a retained spread-to-primary-only replacement test proving retained secondary displayed frame, position, size, and geometry remain observable while accepted secondary request observations are unavailable.
- [x] Add `PreserveManualPercent` transition tests that distinguish it from `Preserve` when a transition results in `Manual` fit mode.
- [x] Add direct property assignment tests for anchor-preserving writable presentation properties, using the current viewport item center as the expected implicit anchor.
- [x] Add revision-token near-overflow tests using private hooks if available, or add a narrowly scoped private test hook as part of this milestone if no safe hook exists.
- [x] Make secondary displayed frame and position derive from committed display role presence, not accepted `secondarySequence()` presence.
- [x] Preserve clear-before-load behavior so secondary displayed observations become unavailable when retained display is cleared.
- [x] Implement `PreserveManualPercent` so it preserves canonical manual zoom percent when the resulting fit mode is `Manual`.
- [x] Ensure `ResetToContain` validation and non-manual fit-derived zoom behavior remain unchanged.
- [x] Route direct assignment through the same accepted-command diagnostic and revision path as equivalent command-style setters.
- [x] Prefer preserving the public `RevisionToken` value-type shape. If satisfying the token contract requires changing public field width or QML type shape, stop and update the durable API spec before implementing that API change.
- [x] Introduce a controller-owned revision-token allocator or equivalent shared allocation semantics for display, request, and command revisions.

#### Completion evidence

- Durable API and architecture intent for revision tokens was clarified before implementation in `docs/spec/image-viewport-api.md` and `docs/architecture/subsystem-boundaries.md`.
- Focused conformance tests were added for retained secondary display observations, `PreserveManualPercent` versus `Preserve`, center-anchored property assignment, and shared non-wrapping revision-token allocation.
- Secondary displayed frame and position now use committed display role presence; retained clear-before-load behavior remains covered by the retained-display test.
- Direct `fitMode`, `zoomPercent`, `mirrorHorizontally`, and `mirrorVertically` assignments use the current viewport item center and enter the command-equivalent controller path.
- Revision tokens now use one controller-owned non-zero `quint64` allocator shared by display, request, and command revisions, with an explicit exhaustion guard.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'imageviewport_presentation_state|imageviewport_public_api_commands|viewportcontroller_presentation|imageviewport_public_api' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Retained spread-to-primary-only replacement exposes retained secondary displayed target and geometry while accepted secondary observations remain unavailable.
- `PreserveManualPercent` is tested and behaves differently from `Preserve` when the resulting fit mode is `Manual`.
- Direct property assignment and command-style setters produce equivalent presentation, diagnostic, notification, and revision effects for the implicit center anchor.
- Public revision tokens never publish invalid tokens after real changes and cannot reuse earlier token values within the supported item lifetime.
- No public API shape changes occur unless durable specs are updated first.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'imageviewport_presentation_state|imageviewport_public_api_commands|viewportcontroller_presentation|imageviewport_public_api' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- The implicit direct-assignment anchor is the current viewport item center in item coordinates.
- Mirror property assignment does not return `CommandOutcome`; tests should verify observable state and diagnostics rather than return values.

### Milestone 3: Provider Lifecycle, Scheduler Failure, And Internal Diagnostics

Status: Completed 2026-07-07.

#### Objective

Make provider lifecycle and scheduler effects deterministic and controller-authorized, and establish the production-capable internal diagnostic artifact used for provider cleanup, scheduler, and later render attribution.

#### Source findings

Finding 8, Finding 10, Finding 11.

#### Scope

- Add focused failing tests first for primary-open failure, scheduler failure, and cleanup diagnostic behavior.
- Introduce a private production-compiled diagnostic sink, not gated by `IMAGEVIEWPORT_PRIVATE_TEST_PROBES`.
- Stop secondary provider session creation after primary open failure seals a two-role spread terminal.
- Make deferred provider queue flush scheduling report success/failure to the controller.
- Project deferred scheduler failure as `Error / ProviderFailure`, clear queued provider state, and emit internal scheduler attribution.
- Emit provider cleanup failure attribution through the internal diagnostic sink while preserving public cleanup behavior.

#### Out of scope

- No role-indexed state refactor beyond small accessors needed for checked effects.
- No provider threading redesign.
- No provider public adapter API changes.
- No render failure diagnostics in this milestone beyond defining the shared diagnostic sink contract.

#### Likely affected areas

- `src/imageviewportdiagnostics_p.h`
- `src/imageviewportdiagnostics.cpp`
- `src/imageviewport.cpp`
- `src/imageviewportprovider.cpp`
- `src/viewportcontrollerprovider.cpp`
- `src/viewportproviderbridge.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportstate_p.h`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [x] Define a private internal diagnostics contract in `ImageViewportInternal`, with typed event structs for provider cleanup failure, provider scheduler failure, and render failure attribution.
- [x] Include required provider cleanup fields: role, operation, metadata token validity/id, frame token validity/id, queued/delivered state, and pending-cleanup state.
- [x] Include required scheduler fields: role, active generation, active request id, queued request id, target kind, and failure operation.
- [x] Include required render fields for later use: failed role, generation, request id, prepared payload id, and `RenderFailureCause`.
- [x] Provide a production-compiled non-public observer or sink that tests can observe without relying solely on `IMAGEVIEWPORT_PRIVATE_TEST_PROBES`; do not expose it through installed public API.
- [x] Add a provider lifecycle test where primary provider session creation fails in a two-provider spread and secondary creation would otherwise succeed.
- [x] Add primary and secondary provider request tests that force deferred queue flush scheduling failure.
- [x] Add provider cleanup failure observability tests, including final item destruction or final close failure, through the production-compiled diagnostic sink.
- [x] Gate secondary session opening on post-primary controller state, or move multi-role open sequencing into a controller result that stops after terminal sealing.
- [x] Ensure any accidentally opened post-terminal session is immediately closed through the normal controller close effect.
- [x] Wrap deferred provider event scheduling in a helper that returns success/failure.
- [x] On scheduler failure, clear queued provider state, stop active playback if the failed request was playback-owned, publish `Error / ProviderFailure` with a bounded generic provider dispatch diagnostic, advance request revision for the public state change, and emit scheduler attribution.
- [x] Keep public `requestStatus`, `requestReason`, `errorString`, and `warningString` unchanged for cleanup-only failures.

#### Completion evidence

- Docs-first intent was committed for public deferred provider dispatch failure projection and the private production diagnostics contract.
- `InternalDiagnostics` now records provider cleanup, provider scheduler, and render-failure attribution through a production-compiled private sink; tests observe it only through private support.
- Primary provider session-open failure now stops two-provider spread opening before the secondary session factory is called.
- Deferred queue-flush scheduling now reports success/failure; primary and secondary scheduler failures clear queued state, publish `Error / ProviderFailure`, advance request revision, and record scheduler attribution.
- Provider cleanup delivery failure diagnostics now include pending-cleanup state while preserving public cleanup-only request, display, diagnostic, and warning observations.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'imageviewport_provider_lifecycle|imageviewport_provider_requests|viewportcontroller_provider|structural::providerEventAdmissionBoundary' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Primary provider open failure in a two-provider spread does not call the secondary provider session factory.
- No secondary provider session remains installed after primary open failure.
- Aggregate status after primary open failure is `Error / ProviderFailure`, and the primary failure diagnostic remains the aggregate diagnostic when it has precedence.
- Built-in-primary/provider-secondary open-failure behavior remains covered and unchanged.
- Failed deferred flush scheduling for primary and secondary roles projects the active request to `Error / ProviderFailure`, clears queued provider state, advances request revision when public state changes, and emits scheduler attribution.
- Cleanup enqueue failure emits provider cleanup attribution through a production-compiled internal diagnostic path, including final destruction or final close attempts.
- Public cleanup-only behavior remains best-effort and does not introduce new public command outcomes.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'imageviewport_provider_lifecycle|imageviewport_provider_requests|viewportcontroller_provider|structural::providerEventAdmissionBoundary' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Do not add public enum values for scheduler failure. Use existing `Error / ProviderFailure` public projection plus internal scheduler attribution.
- The internal diagnostic sink must be production-compiled but private. It may be tested through private test support, but event production itself must not depend on private test macros.

### Milestone 4: Command Outcome Foundation

Status: Completed 2026-07-07.

#### Objective

Centralize command diagnostic publication and revision effects before broader controller refactors.

#### Source findings

Finding 9.

#### Scope

- Create one controller-owned command outcome helper/API for accepted, accepted no-op, invalid, unsupported, ignored, and documented diagnostic-preserving cases.
- Migrate existing command paths to the shared helper incrementally.
- Add structural verification that duplicate diagnostic/revision mutation helpers are not reintroduced.

#### Out of scope

- No role-state storage refactor.
- No provider lifecycle behavior changes beyond preserving Milestone 3 behavior.
- No public API changes.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontrollerpresentation.cpp`
- `src/viewportcontrollerplayback.cpp`
- `src/viewportcontrollerhelpers_p.h`
- A new private command-outcome helper module if useful
- `tests/tst_imageviewport_public_api_commands.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_viewportcontroller_playback.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Add or update a structural test with an allowlist for command diagnostic and command revision mutation sites.
- [x] Add a single private helper/API that owns command diagnostic mutation and command revision effects.
- [x] Migrate base controller command rejection helpers to the shared command outcome helper.
- [x] Migrate presentation command paths, preserving documented `setSpreadDirection(...)` and `setPageGap(...)` diagnostic-preserving exceptions.
- [x] Migrate playback command paths in small batches, running focused tests after each batch.
- [x] Remove duplicated file-local command diagnostic helpers after migration.

#### Completion evidence

- Added structural guard `structural::commandOutcomeBoundary`, which allowlists only `RequestState` primitives and the private command outcome helper for command diagnostic and command-revision mutation.
- Added private `viewportcommandoutcome` helper API for accepted command clearing, rejected command diagnostics, and common invalid/unsupported/ignored/accepted result construction.
- Migrated base controller rejection helpers, presentation commands, and playback command paths to the shared helper while leaving `setSpreadDirection(...)` and `setPageGap(...)` as explicit diagnostic-preserving presentation exceptions.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'imageviewport_public_api_commands|viewportcontroller_presentation|viewportcontroller_playback|structural::commandOutcomeBoundary' --output-on-failure`, `ctest --test-dir build -R 'structural::' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Only the allowlisted command outcome helper and `RequestState` primitive mutate command diagnostics and command revision effects.
- Invalid, unsupported, ignored, accepted, accepted no-op, and diagnostic-preserving commands still publish diagnostics according to spec.
- `setSpreadDirection(...)` and `setPageGap(...)` keep their documented diagnostic-preserving exceptions.
- Structural verification fails if duplicate command diagnostic helper implementations are reintroduced.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'imageviewport_public_api_commands|viewportcontroller_presentation|viewportcontroller_playback' --output-on-failure`
- `ctest --test-dir build -R 'structural::' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- This milestone is a behavior-preserving foundation refactor. Split commits by command area if the diff grows.

### Milestone 5: Role-State Views And Provider Role-Parametric Dispatch

Status: Completed 2026-07-07.

#### Objective

Introduce role-state views and migrate provider dispatch, token ownership, queueing, and terminal handling to role-parametric paths.

#### Source findings

Finding 4.

#### Scope

- Create role-state view helpers over existing storage for request, provider, active request, latest non-playback request, and provider tokens.
- Migrate provider frame dispatch, queueing, token matching, session state, stale-result checks, and terminal handling to role-parametric helpers.
- Add structural allowlists for direct secondary provider/request field access.

#### Out of scope

- No render role-parametric migration.
- No playback role-parametric migration.
- No public API changes.
- No broad storage rewrite beyond wrappers/views over existing fields.

#### Likely affected areas

- `src/imageviewportstate_p.h`
- `src/viewportcontroller_p.h`
- `src/viewportcontrollerhelpers_p.h`
- `src/viewportcontrollerprovider.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Convert the Milestone 1 role-specific inventory into a structural allowlist for provider/request direct secondary-field access.
- [x] Add role-state view helpers that return explicit provider and request slices while still using existing fields.
- [x] Migrate provider queue and token ownership to role-parametric helpers.
- [x] Migrate provider stale-result checks and terminal handling to role-parametric helpers.
- [x] Replace touched direct secondary provider/request field access with role-state views.
- [x] Add or update shared primary/secondary provider request tests.

#### Completion evidence

- Durable architecture intent for role-indexed private views was added to `docs/architecture/subsystem-boundaries.md` before implementation.
- Added structural guard `structural::providerRoleStateBoundary`, which fails on direct secondary provider/request state access in `viewportcontrollerprovider.cpp`.
- Added provider role-state view helpers over existing request/provider storage, including active request and latest non-playback request slices.
- Removed the remaining direct secondary provider/session and secondary active-request access from provider controller code; existing provider queue, token, stale-result, and terminal paths already used role-parametric helpers and remain under provider-focused coverage.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'viewportcontroller_provider|imageviewport_provider_requests|imageviewport_provider_lifecycle|structural::providerRoleStateBoundary' --output-on-failure`, `ctest --test-dir build -R 'structural::' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Provider frame dispatch, queueing, token matching, session state, and terminal handling use role-parametric helpers.
- Direct secondary provider/request field access remains only inside role-state views or an explicit aggregate-spread allowlist.
- Provider request behavior remains role-symmetric for primary and secondary roles.
- Public provider behavior from earlier milestones remains unchanged.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'viewportcontroller_provider|imageviewport_provider_requests|imageviewport_provider_lifecycle|structural::' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Keep aggregate spread behavior separate from role-local provider behavior.

### Milestone 6: Render Role-Parametric Payloads And Acknowledgements

Status: Completed 2026-07-07.

#### Objective

Migrate render pending-payload and acknowledgement handling to role-parametric access while preserving complete-spread commit and stale-failure rules.

#### Source findings

Finding 4.

#### Scope

- Extend role-state views to display/pending payload state as needed.
- Migrate render pending payload selection and render acknowledgement matching to role-parametric helpers.
- Preserve target-spread aggregate commit and terminal projection as controller aggregate responsibilities.

#### Out of scope

- No provider dispatch migration beyond dependencies completed in Milestone 5.
- No playback migration.
- No render production diagnostics; that is Milestone 12.
- No scene graph backend redesign.

#### Likely affected areas

- `src/imageviewportstate_p.h`
- `src/viewportcontrollerhelpers_p.h`
- `src/viewportcontrollerrender.cpp`
- `src/renderadapter.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Convert render/display direct secondary-field access into a structural allowlist.
- [x] Add display role-state view helpers for displayed request, displayed size, pending payload, and prepared payload identity.
- [x] Migrate render pending payload selection to role-parametric helpers.
- [x] Migrate render acknowledgement matching to role-parametric helpers.
- [x] Preserve aggregate spread commit and terminal projection.
- [x] Run render commit, render scene graph, structural, and full-suite verification.

#### Completion evidence

- Durable architecture intent for render/display role-state views was added to `docs/architecture/subsystem-boundaries.md` before implementation.
- Added structural guard `structural::renderRoleStateBoundary`, which fails on direct secondary render/display state access in `viewportcontrollerrender.cpp`.
- Added display role-state view helpers over existing displayed request, displayed image size, displayed image, and pending render payload storage.
- Migrated render pending payload selection, secondary prepared-image fallback, and render acknowledgement identity matching to display role-state views.
- Preserved aggregate target-spread commit and terminal projection behavior in controller-level render synchronization.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'imageviewport_render_commit|imageviewport_render_scenegraph|structural::renderRoleStateBoundary' --output-on-failure`, `ctest --test-dir build -R 'structural::' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Render pending-payload and acknowledgement logic handles primary and secondary through shared role access.
- Atomic target-spread commit and terminal projection remain aggregate controller decisions.
- Direct secondary display/render field access remains only inside role-state views or an explicit aggregate-spread allowlist.
- Existing render commit and scene graph behavior remains unchanged.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'imageviewport_render_commit|imageviewport_render_scenegraph|structural::' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Do not treat role-local render readiness as complete spread readiness.

### Milestone 7: Playback Role-Parametric Timing And Advancement

Status: Completed 2026-07-07.

#### Objective

Migrate playback timer interval, advancement, and stop restoration away from duplicated primary/secondary built-in/provider branches.

#### Source findings

Finding 4.

#### Scope

- Use role-state and timing access helpers for playback timer interval calculations.
- Migrate playback advancement and stop restoration paths in small batches.
- Preserve single-driver playback semantics and latest non-playback target restoration.

#### Out of scope

- No provider/render role-state migrations beyond dependencies completed in Milestones 5 and 6.
- No public playback API changes.
- No new playback state machine.

#### Likely affected areas

- `src/viewportcontrollerplayback.cpp`
- `src/viewportcontrollerhelpers_p.h`
- `src/playbacktimeline_p.h`
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_imageviewport_timed.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Convert playback direct secondary-field and provider/built-in branch inventory into a structural allowlist.
- [x] Migrate playback timer interval logic to role-parametric timing access.
- [x] Migrate playback advancement paths to role-parametric request and timing helpers.
- [x] Migrate stop restoration paths while preserving explicit seek and latest non-playback behavior.
- [x] Remove secondary-specific private playback controller methods after their call sites use role-parametric replacements.
- [x] Run shared primary/secondary playback, provider playback, lifecycle, structural, and full-suite verification.

#### Completion evidence

- Durable architecture intent for playback timing, advancement, and stop restoration role-state access was added to `docs/architecture/subsystem-boundaries.md` before implementation.
- Added structural guard `structural::playbackRoleStateBoundary`, which fails on secondary-specific playback play helpers and direct secondary timing/request/provider access in guarded playback sections.
- Added role-parametric timing access over provider runtime metadata and built-in sequence-source timing facts, then migrated playback timer interval and advancement to consume those timing slices by `PageRole`.
- Migrated secondary stop restoration to use role-state request/provider access while preserving latest non-playback restoration and spread readiness checks.
- Removed the private `playSecondaryBuiltIn` and `playSecondaryProvider` controller methods; secondary play now routes through role-parametric local helpers and role transport routing.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'imageviewport_provider_lifecycle' --output-on-failure`, `ctest --test-dir build -R 'viewportcontroller_playback|imageviewport_provider_playback|imageviewport_timed|structural::playbackRoleStateBoundary|structural::' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Playback timer interval and advancement no longer duplicate primary/secondary built-in/provider branches outside an explicit allowlist.
- Single-driver playback ownership remains unchanged.
- Stop restoration preserves latest non-playback target rules for both roles.
- Secondary-specific private playback methods are removed or limited to named compatibility wrappers.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'viewportcontroller_playback|imageviewport_provider_playback|imageviewport_timed|structural::' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Playback behavior is stateful. Keep batches small and run focused tests after each batch.

### Milestone 8: Narrow Controller Ports And Helper Ownership

Status: Completed 2026-07-07.

#### Objective

Replace broad mutable controller helper access with subsystem-specific internal facades or explicit state slices.

#### Source findings

Finding 6.

#### Scope

- Define the smallest provider, presentation/geometry, render/display, and playback facades needed by controller implementation units.
- Split or shrink `viewportcontrollerhelpers_p.h` so implementation units cannot mutate unrelated state domains through a common all-access header.
- Add structural tests that enforce helper ownership boundaries.

#### Out of scope

- No item-side host extraction.
- No public API changes.
- No behavior changes beyond preserving earlier milestones.

#### Likely affected areas

- `src/viewportcontroller_p.h`
- `src/viewportcontrollerhelpers_p.h`
- `src/viewportcontrollerprovider.cpp`
- `src/viewportcontrollerplayback.cpp`
- `src/viewportcontrollerpresentation.cpp`
- `src/viewportcontrollerrender.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Define the provider facade and migrate provider controller code to it.
- [x] Define the presentation/geometry facade and migrate presentation controller code to it.
- [x] Define the render/display facade and migrate render controller code to it.
- [x] Define the playback facade if playback still depends on broad mutable access.
- [x] Split or shrink `viewportcontrollerhelpers_p.h` to domain-specific helpers or domain-neutral value helpers.
- [x] Add structural tests or grep-based CMake checks for disallowed cross-domain helper access.

#### Completion evidence

- Durable architecture intent for controller helper ownership was added to `docs/architecture/subsystem-boundaries.md` before implementation.
- Added structural guard `structural::controllerHelperOwnershipBoundary`, which fails when controller implementation units include `viewportcontrollerhelpers_p.h` directly.
- Split the previous all-access helper header into `viewportcontrollercorehelpers_p.h`, `viewportcontrollergeometryhelpers_p.h`, `viewportcontrollerplaybackhelpers_p.h`, `viewportcontrollerproviderhelpers_p.h`, and `viewportcontrollerrenderhelpers_p.h`; the old header now shrinks to a compatibility include for the domain-neutral core helpers.
- Migrated controller implementation units to owner-specific helper headers: geometry/presentation use geometry helpers, playback uses playback helpers, provider uses provider helpers, render uses render helpers, metadata uses core helpers, and the main controller uses geometry/core helpers through the geometry facade.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'structural::' --output-on-failure`, `ctest --test-dir build -R 'viewportcontroller_provider|viewportcontroller_presentation|viewportcontroller_playback|imageviewport_render_commit|imageviewport_render_scenegraph|structural::controllerHelperOwnershipBoundary|structural::ownerSpecificHelperHeadersOnly|structural::providerEventAdmissionBoundary|structural::builtInPayloadBoundary' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Provider controller code cannot directly mutate presentation-only state except through explicit controller APIs.
- Presentation controller code cannot directly mutate provider token/session state.
- Render controller code consumes display/render capabilities without unrelated provider or presentation mutation access.
- Structural checks fail when broad mutable helper access is reintroduced outside allowlisted facades.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'structural::ownerSpecificHelperHeadersOnly|structural::providerEventAdmissionBoundary|structural::builtInPayloadBoundary' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- This is a boundary refactor. Keep behavior-preserving tests green throughout.

### Milestone 9: Provider Host Extraction

Status: Completed 2026-07-07.

#### Objective

Extract provider transport client behavior from `ImageViewportPrivate` into a narrow item-side provider host.

#### Source findings

Finding 7.

#### Scope

- Introduce a provider host that owns provider bridge client behavior and delegates typed controller effects.
- Move provider session, transport, deferred event, and callback handling out of `ImageViewportPrivate`.
- Preserve provider lifecycle behavior from Milestone 3.

#### Out of scope

- No render host extraction.
- No playback scheduler extraction.
- No provider protocol redesign.
- No public API changes.

#### Likely affected areas

- `src/imageviewport_p.h`
- `src/imageviewportprovider.cpp`
- `src/imageviewportcontroller.cpp`
- `src/viewportproviderbridge_p.h`
- New private provider host files if useful
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Define the provider host ownership boundary and constructor dependencies.
- [x] Move `ViewportProviderBridgeClient` implementation from `ImageViewportPrivate` to the provider host.
- [x] Move provider bridge storage or access behind the provider host.
- [x] Keep `ImageViewportPrivate` responsible for applying public notifications and owning the host.
- [x] Add structural verification that `ImageViewportPrivate` no longer implements `ViewportProviderBridgeClient` directly.

#### Completion evidence

- Durable architecture intent for the item-side provider host was added to `docs/architecture/subsystem-boundaries.md` before implementation.
- Added structural guard `structural::providerHostBoundary`, which fails if `ImageViewportPrivate` implements `ViewportProviderBridgeClient` directly or stores provider bridge instances directly.
- Introduced `ImageViewportProviderHost` as the private owner of provider bridge client behavior, role-specific bridge instances, provider callbacks, command delivery, cancellation, session close delivery, and deferred provider-controller event scheduling.
- `ImageViewportPrivate` now owns the provider host and remains responsible for applying controller change sets, diagnostics, playback synchronization, render update scheduling, and public notifications.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'imageviewport_provider_lifecycle|imageviewport_provider_requests|viewportcontroller_provider|structural::' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- `ImageViewportPrivate` no longer implements `ViewportProviderBridgeClient` directly.
- Provider callbacks are handled by the provider host and enter the controller through typed inputs.
- Provider lifecycle, scheduler failure, and cleanup diagnostic behavior from Milestone 3 remains unchanged.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'imageviewport_provider_lifecycle|imageviewport_provider_requests|viewportcontroller_provider|structural::' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Provider host extraction is the first item-side extraction because provider methods are already clustered.

### Milestone 10: Render Host Extraction

Status: Completed 2026-07-07.

#### Objective

Extract render synchronization from `ImageViewportPrivate` into a narrow item-side render host.

#### Source findings

Finding 7.

#### Scope

- Introduce a render host around render snapshot consumption, `updatePaintNode(...)`, and render adapter invocation.
- Keep render synchronization consuming controller-authored immutable snapshots.
- Preserve render commit and render failure behavior from earlier milestones.

#### Out of scope

- No provider host changes beyond dependencies from Milestone 9.
- No playback scheduler extraction.
- No render backend redesign.
- No public API changes.

#### Likely affected areas

- `src/imageviewport_p.h`
- `src/imageviewportrender.cpp`
- `src/renderadapter.cpp`
- `src/renderadapter_p.h`
- New private render host files if useful
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Define the render host ownership boundary and constructor dependencies.
- [x] Move render synchronization entry points from `ImageViewportPrivate` to the render host.
- [x] Keep scene graph and render adapter resources encapsulated behind the render host.
- [x] Add structural verification that render synchronization code is isolated from provider transport methods.

#### Completion evidence

- Durable architecture intent for the item-side render host was added to `docs/architecture/subsystem-boundaries.md` before implementation.
- Added structural guard `structural::renderHostBoundary`, which fails if `ImageViewportPrivate` owns render adapter storage or implements render synchronization methods directly, and also guards render synchronization against provider transport access.
- Introduced `ImageViewportRenderHost` as the private owner of render adapter storage, `updatePaintNode(...)` synchronization, scene graph resource handoff, and geometry synchronization.
- `ImageViewportPrivate` now owns the render host and remains responsible for applying controller change sets, playback synchronization, render update scheduling, and public notifications after render-host calls.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'imageviewport_render_commit|imageviewport_render_scenegraph|structural::' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Render synchronization is isolated from provider transport and playback timer methods.
- The render host consumes controller-authored snapshots and reports acknowledgements without reading mutable provider/session state.
- Existing render commit and scene graph tests pass unchanged.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'imageviewport_render_commit|imageviewport_render_scenegraph|structural::' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Do not dereference scene graph resources outside the Qt Quick synchronization/render path.

### Milestone 11: Playback Scheduler Extraction

Status: Completed 2026-07-07.

#### Objective

Extract playback timer wiring from `ImageViewportPrivate` into a narrow scheduler object.

#### Source findings

Finding 7.

#### Scope

- Introduce a playback scheduler object that owns timer state, elapsed-time capture, and callbacks into controller playback APIs.
- Keep playback state and playback decisions in the controller.
- Preserve single-driver playback behavior and waiting-clock behavior.

#### Out of scope

- No provider or render host changes beyond dependencies from earlier milestones.
- No playback state-machine redesign.
- No public API changes.

#### Likely affected areas

- `src/imageviewport_p.h`
- `src/imageviewportcontroller.cpp`
- `src/playbackclock_p.h`
- New private playback scheduler files if useful
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_playback_clock.cpp`
- Structural tests under `tests/*.cmake`

#### Tasks

- [x] Define the playback scheduler ownership boundary and constructor dependencies.
- [x] Move `QTimer`, elapsed timebase, and playback timer callbacks out of `ImageViewportPrivate`.
- [x] Keep the scheduler as an external-effect adapter; it must not own playback phase, request identity, or target selection.
- [x] Add structural verification that playback timer state is not stored alongside provider bridge state in `ImageViewportPrivate`.

#### Completion evidence

- Durable architecture intent for the item-side playback scheduler was added to `docs/architecture/subsystem-boundaries.md` before implementation.
- Added structural guard `structural::playbackSchedulerBoundary`, which fails if `ImageViewportPrivate` stores playback timer, elapsed timebase, or playback clock state directly, or if direct playback timer entry points remain on the private item.
- Introduced `ImageViewportPlaybackScheduler` as the private owner of `QTimer`, elapsed timebase, `PlaybackClock`, timeout callbacks, elapsed-time flush, and timer synchronization.
- Playback decisions remain controller-owned: the scheduler asks the controller for the next timer interval and calls the item boundary to advance playback with elapsed milliseconds.
- Verification passed: `cmake --build build`, `ctest --test-dir build -R 'playback_clock|viewportcontroller_playback|imageviewport_provider_playback|structural::' --output-on-failure`, and `ctest --test-dir build --output-on-failure`.

#### Acceptance criteria

- Playback timer state is owned by a scheduler object, not directly by `ImageViewportPrivate`.
- Playback decisions remain controller-owned.
- Existing playback clock and provider playback tests pass unchanged.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'playback_clock|viewportcontroller_playback|imageviewport_provider_playback|structural::' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- The scheduler should remain a thin effect adapter. Do not move domain playback logic out of the controller.

### Milestone 12: Render Failure Production Diagnostics

#### Objective

Add production-capable attribution for accepted render failures while keeping public request projection stable and coarse.

#### Source findings

Finding 12.

#### Scope

- Reuse the private internal diagnostic sink created in Milestone 3.
- Emit accepted render failure diagnostics with failed role, generation, request id, prepared payload id, and `RenderFailureCause`.
- Ensure stale render failures remain ignored without emitting accepted-failure diagnostics.
- Keep public `errorString` as the generic bounded render failure message unless the durable specs change.

#### Out of scope

- No public exposure of `RenderFailureCause`.
- No render backend redesign.
- No scene graph resource ownership changes.
- No changes to payload admission classification.

#### Likely affected areas

- `src/imageviewportdiagnostics_p.h`
- `src/viewportcontrollerrender.cpp`
- `src/imageviewportrender.cpp`
- `src/imageviewportstate_p.h`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`

#### Tasks

- [ ] Add focused failing tests for accepted render failure attribution through the production-compiled internal diagnostic sink.
- [ ] Emit diagnostics only after the controller accepts a render failure as matching the active request/payload identity.
- [ ] Include failed role, generation, request id, prepared payload id, and `RenderFailureCause`.
- [ ] Keep stale render failures ignored and verify they do not emit misleading diagnostics.
- [ ] Preserve the public `Error / RenderFailure` projection and generic `errorString`.

#### Acceptance criteria

- Every accepted render failure records structured internal attribution in production-capable diagnostics.
- Stale render failures remain ignored and do not emit accepted-failure diagnostics.
- Public status remains `Error` with `RenderFailure`, and public `errorString` remains generic and bounded.
- Tests cover all current `RenderFailureCause` values.

#### Verification

- `cmake --build build`
- `ctest --test-dir build -R 'imageviewport_render_commit|imageviewport_render_scenegraph' --output-on-failure`
- `ctest --test-dir build --output-on-failure`

#### Risks / notes

- Do not leak backend or scene graph details into public API.
- If the Milestone 3 diagnostic sink proves insufficient for render attribution, update this plan before adding another diagnostics mechanism.

## Deferred / Needs Investigation

- Transition-policy validation consolidation: the design review noted duplicated enum/domain checks between `PageSetTransitionPolicy::isValid()` and private validation helpers. This is not a standalone milestone because no concrete divergence was identified beyond the missing `PreserveManualPercent` behavior. Revisit only if Milestone 2 changes the same validation surface; if implemented, keep `PageSetTransitionPolicy::isValid()` as the public entry point and delegate to one canonical validator.
- Revision-token overflow test hook: if no existing private hook can create near-overflow revision state safely, add a narrowly scoped private test hook in Milestone 2 or document an enforced unreachable overflow limit. Do not add broad test-only access to unrelated controller state.
- Internal diagnostic sink implementation detail: the required contract is fixed in Milestone 3, but the exact storage and observer mechanics should be the smallest private implementation that is production-compiled, test-observable, and not installed as public API.

## Suggested `/goal` Execution Order

1. Milestone 1: Baseline Verification And Refactor Characterization.
2. Milestone 2: Public Behavior Conformance Fixes.
3. Milestone 3: Provider Lifecycle, Scheduler Failure, And Internal Diagnostics.
4. Milestone 4: Command Outcome Foundation.
5. Milestone 5: Role-State Views And Provider Role-Parametric Dispatch.
6. Milestone 6: Render Role-Parametric Payloads And Acknowledgements.
7. Milestone 7: Playback Role-Parametric Timing And Advancement.
8. Milestone 8: Narrow Controller Ports And Helper Ownership.
9. Milestone 9: Provider Host Extraction.
10. Milestone 10: Render Host Extraction.
11. Milestone 11: Playback Scheduler Extraction.
12. Milestone 12: Render Failure Production Diagnostics.

## Plan Review Notes

- Concerns addressed: the plan now keeps the baseline milestone green, moves conformance-failure tests into the milestone that fixes each gap, defines the mirror implicit anchor in the durable API spec, fixes scheduler failure projection to `Error / ProviderFailure`, defines a concrete private internal diagnostic sink contract before diagnostic tests, adds structural checks for command diagnostic centralization and role-parametric refactors, and splits oversized role/boundary refactors into smaller milestones.
- Concerns deferred: transition-policy validation consolidation, exact revision overflow hook shape, and diagnostic sink storage/observer mechanics are deferred because the safe implementation detail depends on code touched during execution.
- Remaining known limitations: verification commands assume a `build` directory generated with CMake and tests enabled; future sessions should update this plan if the local project uses another build directory or preset.

## Non-Goals

- Do not implement source changes while editing this plan.
- Do not rewrite the controller as a new state machine.
- Do not expose provider token ids, render failure causes, scene graph objects, provider transport internals, or private test probes through the public API.
- Do not remove compatibility primary-only APIs while fixing role symmetry.
- Do not optimize provider threading policy before lifecycle sequencing and checked external-effect gaps are fixed.
- Do not redesign render backend selection, native texture support, tiled loading, full color management, cache policy, decoding policy, source navigation, or application routing.
- Do not modify `docs/spec/**` or `docs/architecture/**` while executing implementation milestones unless a newly discovered authoritative ambiguity blocks safe implementation.
