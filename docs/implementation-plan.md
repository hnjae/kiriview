# Implementation Plan

## Purpose

This plan consolidates the design review findings into executable implementation work for future `/goal` sessions. It translates the reviewed architecture problems into safe, incremental milestones that preserve current behavior unless the behavior conflicts with `docs/spec/**` or `docs/architecture/**`.

## Source Basis

Durable source basis:

- `docs/spec/**`
- `docs/architecture/**`

`DESIGN_REVIEW_CORRECT_END_STATE.md` was used as a temporary planning input. Its actionable content has been fully consolidated into this plan, and future implementation sessions do not need that temporary file.

No temporary design review finding was overridden by the authoritative docs. The design review found implementation conflicts with the durable docs, but did not find a high-confidence conflict inside `docs/spec/**` or `docs/architecture/**`. No durable spec or architecture update is required for this plan.

## Execution Principles

- Preserve current behavior unless a change fixes a documented bug, a spec violation, or an architecture-contract violation.
- Add characterization tests before risky refactors. Red tests must be named and scoped to the milestone that fixes them; do not leave unspecified failing tests in the suite.
- Keep changes incremental. Prefer controller-owned transitions, role-scoped accessors, and small boundary seams over broad rewrites.
- Treat `docs/spec/**` and `docs/architecture/**` as authoritative and immutable while executing this plan. If those documents appear stale, ambiguous, or inconsistent, stop plan execution and require a separate design decision outside this plan; do not modify durable docs as part of these milestones.
- Isolate domain logic from external effects. Controller/request logic should be testable without Qt scene graph allocation, real provider threads, app bootstrapping, wall-clock timing, or QML binding setup where practical.
- Keep public APIs stable unless the durable docs explicitly require a different final API. When an API change is required, add compatibility or staged migration first.
- Update this plan if implementation discovers a blocker, invalid assumption, missing test target, or safer sequencing.
- Do not use `DESIGN_REVIEW_CORRECT_END_STATE.md`; it is intentionally temporary and should not exist during future execution.

## Consolidated Design Findings

### Target-spread terminal state is not sealed or centrally projected

- **Priority:** P0
- **Affected areas:** `ViewportController::handleProviderTerminalEvent`, `handleProviderDispatchFailure`, `handleProviderFrameTerminalResult`, `handleSecondaryProviderFrameTerminalResult`, metadata terminal handlers, provider frame admission, render failure handling, provider lifecycle and terminal tests.
- **Correct end state:** The controller stores role-scoped readiness and terminal facts for the active target spread, seals a target spread after a required-role terminal failure, and derives public aggregate status through one projection rule: `Error` over `Unsupported`, primary diagnostics on same-status ties, and no later opposite-role result can move the sealed target back to `Loading` or `Ready`.
- **Why it matters:** The specs require deterministic aggregate spread failure behavior. Mutable aggregate status can be overwritten by callback order, which creates correctness and production-risk issues.

### Page-set content-position transition uses stale geometry

- **Priority:** P0
- **Affected areas:** `ViewportController::assignSequence`, `applyPresentationTransition`, `controllerGeometryState`, `controllerMaximumContentPosition`, `clampPresentationPanToBounds`, page-set transition tests.
- **Correct end state:** Page-set transition geometry is computed against the replacement spread, after replacement role facts, rotation, mirror state, direction, and gap are known, while preserving no-partial-apply validation.
- **Why it matters:** Page-set replacement is documented as one atomic viewport-owned transaction. Using old spread geometry can publish a replacement that needs follow-up repair commands.

### Display rotation affects geometry but not rendered pixels

- **Priority:** P0
- **Affected areas:** `ViewportRenderLayer`, `ViewportRenderSnapshot`, `RenderAdapter::Input::ImageLayer`, `RenderAdapter::createNode`, render scene graph tests, presentation state tests.
- **Correct end state:** Quarter-turn display rotation is part of the render snapshot and is applied to every rendered page layer consistently with public geometry and coordinate conversion.
- **Why it matters:** Geometry and hit testing can report rotated coordinates while scene graph pixels remain unrotated.

### Built-in frames publish `Ready` before render acknowledgement

- **Priority:** P0
- **Affected areas:** `ViewportController::assignSequence`, `publishSequenceReadyState`, built-in spread staging, `beginRenderSynchronization`, `acknowledgeRenderCommit`, render commit tests.
- **Correct end state:** Built-in still and timed-list payloads use the same pending render identity and acknowledgement path as provider payloads. Request/display readiness is published only after a matching render commit for the active prepared payload or target spread.
- **Why it matters:** Public `requestStatus: Ready` and `displayStatus: Ready` must mean accepted visible content has committed, not only that a payload was staged.

### Public metadata and wait-reason projections have multiple sources

- **Priority:** P1
- **Affected areas:** `ImageViewportPrivate` metadata getters, `ViewportController` provider metadata getters, provider metadata admission, wait-reason assignments in provider/render/queue paths.
- **Correct end state:** One controller-owned role-scoped projection produces frame counts, durations, seek bounds, capability observations, and loading reasons. Wait reasons are derived through the documented priority: provider waiting, request queued, upload pending, render waiting.
- **Why it matters:** Unknown versus unavailable metadata and public request reasons are branchable API contracts. Duplicated projection logic can drift by role or timing path.

### Item-private owns controller responsibilities

- **Priority:** P1
- **Affected areas:** `ImageViewportPrivate::play(PageRole)`, `pause(PageRole)`, `stop(PageRole)`, `seek(PageRole, int)`, `seekToPosition(PageRole, int)`, `setPageSet`, `ViewportSequenceAssignment`, `ViewportController::assignSequence`.
- **Correct end state:** `ImageViewportPrivate` normalizes boundary values, forwards typed commands, applies `ViewportChangeSet`, delivers transport effects, and emits signals. The controller owns role presence, command admission, capability/bounds checks, initial target selection, page-set transaction state, metadata projection, and diagnostics.
- **Why it matters:** Split admission and projection make primary and secondary behavior hard to audit and allow internal callers to bypass controller invariants.

### Provider callback flow is item-orchestrated

- **Priority:** P1
- **Affected areas:** `ImageViewportPrivate::handleProviderMetadataReady`, `handleSecondaryProviderMetadataReady`, `flushQueuedProviderFrameRequest`, controller metadata admission/target policy methods, provider queue dispatch.
- **Correct end state:** Provider events enter the controller through single event-shaped APIs that validate identities, perform admission, mutate state, and return all change flags plus transport effects.
- **Why it matters:** Multi-call item choreography hides side effects and currently includes manual revision/signal publication outside `applyControllerChanges`.

### Primary and secondary pipelines are duplicated instead of role-indexed

- **Priority:** P1
- **Affected areas:** `RequestState`, `DisplayState`, `ProviderGenerationState`, `ViewportController`, `ImageViewportPrivate::handleProviderEvent`, provider tests.
- **Correct end state:** Per-role state is indexed by `ImageViewport::PageRole`; metadata, frame, playback, cancellation, and terminal paths are role-parameterized. Aggregate spread policy remains separate from role-local pipelines.
- **Why it matters:** The provider contract is role-symmetric. Duplicated primary/secondary method families make token matching, failure handling, playback, and metadata policy easy to diverge.

### Sequence facts cross the controller boundary indirectly

- **Priority:** P1
- **Affected areas:** `ViewportControllerContext`, `ViewportControllerPort`, `ImageViewportPrivate` sequence/provider fact methods, `ViewportController::assignSequence`.
- **Correct end state:** At assignment, the controller receives canonical role-source/facts snapshots and narrow payload access contracts. Runtime metadata updates controller-owned generation facts directly.
- **Why it matters:** The controller stores sequence pointers, then calls back through item-provided virtual accessors derived from the same controller state, obscuring ownership.

### C++ page-set API is untyped at the viewport boundary

- **Priority:** P1
- **Affected areas:** `ImageViewport::setPageSet`, `sequenceFromPageSetValue`, `ImageViewportPrivate::setPageSet`, public API tests, installed header generation, install-consumer tests.
- **Correct end state:** Public C++ API exposes typed overloads accepting `ImageSequence*` or `nullptr` plus `PageSetTransitionPolicy`. Generic conversion remains private or QML-only compatibility if still needed.
- **Why it matters:** The specs define page roles as `ImageSequence | null`; `QVariant` as the public C++ command shape preserves invalid caller shapes as part of the API.

### Core render/request behavior depends on public item test probes

- **Priority:** P1
- **Affected areas:** `ImageViewport`, `ImageViewportPrivate`, `ViewportController`, render commit tests, provider playback/terminal/request tests.
- **Correct end state:** Render acknowledgement, request identity, and playback advancement are covered through internal controller/render ports. Item tests cover public projection, signal wiring, QML conversion, and small scene graph integration cases.
- **Why it matters:** Core behavior is currently tied to `QQuickItem` bootstrapping and private public-header probes.

### Render failures collapse distinct backend causes

- **Priority:** P1
- **Affected areas:** `RenderAdapter::Output`, `RenderAdapter::createNode`, `ViewportRenderAcknowledgement`, `ViewportController::acknowledgeRenderFailure`, render tests.
- **Correct end state:** Render failure acknowledgements carry typed internal causes plus role/payload identity. Public state remains `Error/RenderFailure` with bounded redacted text; internal diagnostics preserve actionable cause data.
- **Why it matters:** Production render failures are difficult to debug when missing window, texture allocation, node creation, and role-specific failures all look identical.

### Legacy placement state remains canonical

- **Priority:** P1
- **Affected areas:** `PresentationState`, `PresentationGeometry::State`, `ContentPlacementMode`, placement tests, public geometry getters.
- **Correct end state:** Internal presentation state mirrors final concepts: fit mode, public zoom percent/manual zoom demand, content position, rotation, mirror flags, spread direction, page gap, scan state, and geometry projection. Legacy fill/alignment/raw zoom/raw pan are not persistent controller state.
- **Why it matters:** Hidden legacy placement primitives make the final presentation model harder to reason about and harder to delete.

### `ImageSequence` is not truly opaque

- **Priority:** P1
- **Affected areas:** `ImageSequence`, `ImageSequenceFactory`, provider admission paths, installed public header generation.
- **Correct end state:** `ImageSequence` is a thin public handle with source-specific data hidden behind private implementation interfaces.
- **Why it matters:** The public header currently exposes source strategy and provider facts, weakening the opaque caller-facing handle contract.

### Provider transport and render planning are too coupled to external effects

- **Priority:** P2
- **Affected areas:** `ViewportProviderBridge`, provider lifecycle/contract tests, provider test support, `RenderAdapter`, `ImageViewportPrivate::updatePaintNode`, render scene graph tests.
- **Correct end state:** Provider protocol state transitions are covered with deterministic controller/transport fake tests, with only focused Qt-affinity integration tests. Render planning is pure and tested without scene graph allocation; QSG materialization is thin.
- **Why it matters:** Tests become brittle and slow when protocol semantics depend on real Qt scheduling or scene graph resources.

### Catch-all helper header couples separate boundaries

- **Priority:** P2
- **Affected areas:** `imageviewporthelpers_p.h`, `FramePreparation`, `ImageSequenceFactory`, `ImageViewportPrivate`, `PresentationGeometry`, `ViewportController`.
- **Correct end state:** Helper policies are grouped by owner: limits/admission, provider facts, diagnostics, public value validation, and geometry utilities.
- **Why it matters:** A broad helper header lets unrelated boundary policy leak across subsystems.

## Milestones

### Milestone 0: Confirm Baseline and Characterization Targets

#### Objective

Confirm the current test baseline, identify exact test cases to add for each risky milestone, and add only commit-safe characterization coverage that can pass without source changes.

#### Source findings

All consolidated design findings.

#### Scope

- Confirm full and targeted test commands.
- Add green characterization tests that capture current behavior only when useful.
- For behavior that must change, record exact test names and expected assertions in the relevant implementation milestone before adding source changes there.

#### Out of scope

- Do not add unspecified failing tests.
- Do not use disabled, skipped, or expected-fail tests unless the test name, failure assertion, and removal milestone are documented in the same commit.
- Do not change production code.

#### Likely affected areas

- `tests/tst_imageviewport_provider_terminal.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- `tests/tst_imageviewport_presentation_state.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/imageviewport_*_test_support.h`

#### Tasks

- [x] Run `just test` or, if the full suite is too slow locally, run `just build` followed by targeted `ctest --test-dir build-ninja -R 'imageviewport_(provider_terminal|provider_lifecycle|render_commit|render_scenegraph|presentation_state|public_api|provider_playback)|viewportcontroller_(presentation|provider|playback)' --output-on-failure` to confirm baseline status.
- [x] Create an implementation test inventory listing exact future test names for target-spread terminal order, secondary provider generation-terminal behavior, page-set transition geometry, render rotation, built-in render acknowledgement, wait-reason priority, typed page-set API, secondary command admission, provider event gate, and playback-sensitive flows.
- [x] Add green current-behavior characterization tests only where they can pass without source changes and will remain useful after the implementation.
- [x] For each future failing test, record the target milestone, expected failing assertion, and whether the project will use a temporary `QEXPECT_FAIL` with an explicit reason or keep the red test in the same implementation milestone until fixed.
- [x] Characterize playback-sensitive behavior before role-command/provider-event refactors: stop restoration, play-once final-frame promotion, looping wrap, stale playback-token results, and pause/stop on a non-driver role.
- [x] Characterize simultaneous wait facts across required roles before wait-state refactoring: provider waiting, request queued, upload pending, and render waiting.

#### Execution record

Status: complete on 2026-07-04.

Authoritative-doc check: `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, `docs/spec/image-sequence-provider-adapter.md`, `docs/architecture/subsystem-boundaries.md`, `docs/architecture/provider-protocol.md`, `docs/architecture/rendering.md`, `docs/architecture/playback-state-machine.md`, and `docs/architecture/build-and-package.md` were re-read for this milestone. Milestone 0 remains an evidence and inventory milestone; no conflict with the durable end-state docs was found.

Baseline: `just test` configured, built, and ran the suite successfully on 2026-07-04. CMake reported missing `WrapVulkanHeaders` as non-fatal. Result: 20/20 tests passed in `build-ninja`.

Green characterization added in this milestone: none. The useful source-change-free characterization coverage is already present in the current suite; missing coverage below would either be red against the current implementation or would lock in behavior that later milestones intentionally replace.

Existing green characterization to retain during later refactors: `tests/tst_imageviewport_provider_playback.cpp::providerTimedStopWhileWaitingForMetadataRestoresExplicitSeek`, `providerTimedStopWhileWaitingForMetadataRestoresExplicitPositionSeek`, `providerTimedStopAfterMetadataPlaybackCreatesNonPlaybackRequest`, `providerTimedStopAfterMetadataPlaybackRestoresSupersededExplicitSeek`, `providerTimedStopCancelsPlaybackRequest`, `providerTimedStopSupersedesPlaybackRequest`, `providerTimedPlaybackEndOfSequenceRequestsFinalFrame`, `providerTimedPlaybackEndOfSequenceFinalUsesPlaybackEntryPoint`, `providerTimedLoopingPlaybackWrapsToFirstFrame`, `secondaryProviderTimedPlaybackEndOfSequenceStopsAfterFinalFrameCommits`, `secondaryProviderTimedStopCancelsPlaybackRequest`, `providerTimedStopAfterPausedMetadataWaitRestoresInitialRequest`, `providerTimedStopWhileWaitingForMetadataRestoresInitialRequest`, `tests/tst_viewportcontroller_playback.cpp::loopingPlaybackWrapsToStart`, `pauseWhileRenderWaitingCommitsWithoutResumingPlayback`, and `explicitSeekWhilePlayingWaitsForRenderCommit` characterize playback-sensitive stop restoration, final-frame promotion, looping wrap, stale playback-token/result rejection, and paused waiting behavior.

Existing green wait-state characterization to retain during later refactors: `tests/tst_imageviewport_provider_metadata.cpp::providerProgressResultsAreAdvisory`, `providerFrameReadyDominatesLateProgress`, `providerPositiveResizeWhileMetadataWaitingKeepsProviderWaiting`, `tests/tst_imageviewport_provider_frame_admission.cpp::providerFrameReadyWithPositiveGeometryPublishesUploadPending`, `providerFrameReadyWithZeroGeometryKeepsRenderWaiting`, and `tests/tst_viewportcontroller_provider.cpp::providerFrameRenderAcknowledgementCommitsFromControllerSnapshot` cover current provider-waiting stability, queued transition, upload-pending observation, render-waiting observation, and late-progress suppression after upload ownership. Milestone 4 must add the missing simultaneous required-role aggregation matrix before changing wait projection.

Implementation test inventory:

| Target milestone | Future test name | Expected failing assertion before the milestone implementation | Red-test handling |
| --- | --- | --- | --- |
| 1 | `ImageViewportProviderTerminalTest::targetSpreadTerminalProjectionPrefersErrorOverUnsupported_data` and `targetSpreadTerminalProjectionPrefersErrorOverUnsupported` | For one target spread with both required roles terminal, aggregate `requestStatus`, `requestReason`, and `errorString` follow `Error` over `Unsupported` and primary diagnostics on same-status ties. | Keep the red tests in Milestone 1 until fixed; do not use `QEXPECT_FAIL`. |
| 1 | `ImageViewportProviderTerminalTest::secondaryTerminalFailureSealsSpreadAgainstLatePrimaryReady` | After a required secondary terminal failure, a later primary loading, ready, progress, or terminal callback cannot move the sealed aggregate request back to `Loading` or `Ready`. | Keep red in Milestone 1 until fixed. |
| 1 | `ImageViewportProviderTerminalTest::primaryTerminalFailureSealsSpreadAgainstLateSecondaryReady` | After a required primary terminal failure, later secondary callbacks cannot overwrite the sealed aggregate state or diagnostics. | Keep red in Milestone 1 until fixed. |
| 1 | `ImageViewportProviderTerminalTest::clearAndReplacementEscapeSealedTargetSpread` | `clear()` and accepted page-set replacement escape generation-terminal and sealed target-spread state; unrelated late callbacks remain stale. | Keep red in Milestone 1 until fixed. |
| 1 | `ViewportControllerProviderTest::failureScopeTableClassifiesTerminalInputs_data` and `failureScopeTableClassifiesTerminalInputs` | Session-open failure, metadata production failure, malformed metadata, metadata public-limit/protocol violation, accurate unsupported metadata, active request unsupported, active request provider failure, payload admission failure, and render failure classify to documented generation-terminal or display-request-terminal scopes. | Keep red in Milestone 1 until fixed. |
| 1 | `ImageViewportProviderLifecycleTest::secondarySessionOpenFailureIsGenerationTerminalForSpread` | Secondary session-open failure reports aggregate provider failure for the accepted spread, closes secondary generation interest, and rejects further display/playback requests by generation-terminal scope. | Keep red in Milestone 1 until fixed. |
| 2 | `ImageViewportPublicApiTest::cppTypedPageSetOverloadsCompileAndReplaceSpread` | C++ callers can call typed `setPageSet(ImageSequence*, ImageSequence*, PageSetTransitionPolicy)` without `QVariant`, and the accepted primary/secondary observations update atomically. | Keep red in Milestone 2 until fixed. |
| 2 | `ImageViewportPublicApiTest::invalidPageRoleArgumentsPreserveRevisionTokens` | Invalid page-role values reject before clear-style behavior and leave request/display revision tokens unchanged. | Keep red in Milestone 2 until fixed. |
| 2 | `imageviewport_install_consumer::typed_page_set_overload` | The installed public header exposes typed page-set overloads to a downstream C++ consumer without private headers. | Keep red in Milestone 2 until fixed. |
| 3 | `ViewportControllerPresentationTest::pageSetTransitionScanStartUsesReplacementSpreadGeometry` | `ScanStart` content-position handoff is computed against replacement spread bounds, rotation, mirror state, direction, and gap in the same accepted transaction. | Keep red in Milestone 3 until fixed. |
| 3 | `ViewportControllerPresentationTest::pageSetTransitionScanEndUsesReplacementSpreadGeometry` | `ScanEnd` lands on the replacement spread end immediately after accepted replacement, with no follow-up scan or clamp command needed. | Keep red in Milestone 3 until fixed. |
| 3 | `ViewportControllerPresentationTest::pageSetTransitionClampUsesReplacementBounds` | `Clamp` clamps the previous content position to the replacement spread maximum content position, not the old spread bounds. | Keep red in Milestone 3 until fixed. |
| 3 | `ImageViewportPresentationStateTest::invalidPageSetTransitionPreservesStateAndRevisions` | Invalid transition policy preserves page set, presentation fields, retained display, playback phase, diagnostics, and request/display revisions exactly. | Keep red in Milestone 3 until fixed. |
| 4 | `ViewportControllerProviderTest::requiredRoleWaitPriorityAggregatesBeforeProjection_data` and `requiredRoleWaitPriorityAggregatesBeforeProjection` | With simultaneous waits across required primary and secondary roles, public reason priority is provider waiting, request queued, upload pending, then render waiting. | Keep red in Milestone 4 until fixed. |
| 4 | `ImageViewportProviderRequestsTest::waitProjectionRevisionChangesOnlyWhenPublicReasonChanges` | Toggling a lower-priority wait fact while the projected public reason is unchanged does not advance `requestRevision`; changing the projected reason does. | Keep red in Milestone 4 until fixed. |
| 5 | `ImageViewportRenderCommitTest::stillAssignmentWaitsForRenderCommitWithPositiveGeometry` | Built-in still assignment with positive item geometry stays `Loading/RenderWaiting` until a matching render commit acknowledgement. | Keep red in Milestone 5 until fixed. |
| 5 | `ImageViewportRenderCommitTest::timedListAssignmentWaitsForRenderCommitWithPositiveGeometry` | Built-in timed-list initial target does not publish request/display `Ready` until matching render commit. | Keep red in Milestone 5 until fixed. |
| 5 | `ImageViewportRenderCommitTest::builtInTwoPageSpreadWaitsForCompleteRenderCommit` | Built-in primary plus built-in secondary spread publishes `Ready` only after both required role payload identities commit as one target spread. | Keep red in Milestone 5 until fixed. |
| 5 | `ImageViewportRenderCommitTest::mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit_data` and `mixedBuiltInProviderSpreadWaitsForCompleteRenderCommit` | Built-in/provider and provider/built-in spreads stay loading until all required role payloads have matching render acknowledgements. | Keep red in Milestone 5 until fixed. |
| 5 | `ImageViewportRenderCommitTest::staleBuiltInRenderAcknowledgementIsIgnored` | A render acknowledgement for a superseded built-in prepared payload identity cannot publish readiness or alter diagnostics. | Keep red in Milestone 5 until fixed. |
| 6 | `ImageViewportRenderSceneGraphTest::renderLayerCarriesQuarterTurnRotation_data` and `renderLayerCarriesQuarterTurnRotation` | Render-layer snapshots for 90, 180, and 270 degrees carry normalized display rotation matching public geometry. | Keep red in Milestone 6 until fixed. |
| 6 | `ImageViewportRenderSceneGraphTest::rotationMirrorCompositionMapsLayerCorners_data` and `rotationMirrorCompositionMapsLayerCorners` | Render mapping or pixel assertions match public coordinate conversion for rotation crossed with horizontal, vertical, and combined mirrors. | Keep red in Milestone 6 until fixed. |
| 6 | `ImageViewportRenderSceneGraphTest::rotatedSourceRectsRemainLogicalPageSpace` | Source rectangles remain in public logical page space and preserve half-open edge behavior under display rotation. | Keep red in Milestone 6 until fixed. |
| 7 | `ImageViewportRenderCommitTest::renderFailureCauseIsPreservedInternally_data` and `renderFailureCauseIsPreservedInternally` | Missing window, texture creation failure, image-node creation failure, invalid or empty role payload, and unknown backend failure are distinguishable internally while public status remains `Error/RenderFailure`. | Keep red in Milestone 7 until fixed. |
| 8 | `ImageViewportProviderMetadataTest::roleScopedMetadataProjectionUsesOnePath_data` and `roleScopedMetadataProjectionUsesOnePath` | Unknown facts, partial known facts, complete metadata, runtime metadata, still, timed-list, primary role, and secondary role project through identical controller-owned metadata semantics. | Keep red in Milestone 8 until fixed. |
| 9 | `ViewportControllerPlaybackTest::roleCommandAdmissionOrder_data` and `roleCommandAdmissionOrder` | Role command admission order is malformed role, absent role, intrinsic target validity, failure scope, capability support, then accepted dispatch. | Keep red in Milestone 9 until fixed. |
| 9 | `ImageViewportPublicApiTest::secondaryCommandAdmissionMatchesController` | Item-level secondary commands and controller-level role commands produce identical outcomes, diagnostics, request state, display state, and playback phase. | Keep red in Milestone 9 until fixed. |
| 10 | `ViewportControllerProviderTest::providerEventGateRejectsStaleFamilies_data` and `providerEventGateRejectsStaleFamilies` | Metadata, frame, waiting/progress, terminal, cancellation, end-of-sequence, dispatch failure, preparation result, and render acknowledgement events all reject stale session/generation/token/role identities through one gate. | Keep red in Milestone 10 until fixed. |
| 10 | `ViewportControllerProviderTest::queuedProviderFlushReturnsChangeSetAndTransportEffects` | Queued provider transitions produce controller change sets and transport effects without item-side revision increments or event-loop draining. | Keep red in Milestone 10 until fixed. |
| 11 | `ViewportControllerProviderTest::roleIndexedTokenValidationIsSymmetric_data` and `roleIndexedTokenValidationIsSymmetric` | Primary and secondary token validation paths call the same role-parameterized helper and produce identical stale/accepted outcomes. | Keep red in Milestone 11 until fixed. |
| 12 | `ImageViewportProviderLifecycleTest::providerTransportFakeRunsCancellationCloseAndDispatchSynchronously` | Cancellation, close, dispatch failure, and queued-result protocol cases run through a fake scheduler without manual event draining unless explicitly allowlisted. | Keep red in Milestone 12 until fixed. |
| 13 | `ImageViewportRenderSceneGraphTest::renderPlanMapsBackgroundLayersAndRotationWithoutQsg_data` and `renderPlanMapsBackgroundLayersAndRotationWithoutQsg` | Background, layer order, source rectangles, rotation/mirror composition, and failure intent are testable without `QQuickWindow` or QSG allocation. | Keep red in Milestone 13 until fixed. |
| 14 | `ViewportControllerPresentationTest::presentationStateDoesNotPersistLegacyPlacementFields` | Controller state no longer persists legacy fill/alignment/raw zoom/raw pan fields while public geometry behavior remains unchanged. | Keep red in Milestone 14 until fixed. |
| 15 | `ImageViewportPublicApiTest::installedHeaderKeepsImageSequenceOpaque` | Installed public headers do not expose sequence storage, timing internals, provider facts, provider capabilities, private controller/render/provider types, or test probes. | Keep red in Milestone 15 until fixed. |
| 16 | `structural::ownerSpecificHelperHeadersOnly` | No subsystem includes `imageviewporthelpers_p.h` for unrelated policies; any remaining generic-helper include is on an explicit temporary allowlist. | Keep red in Milestone 16 until fixed. |

#### Acceptance criteria

- Full baseline or targeted baseline status is known.
- No unspecified failing tests are committed.
- Every P0 finding has named test cases planned or added.
- Playback-sensitive cases that later refactors can disturb are explicitly listed with target tests.

#### Verification

- `just test`
- Targeted fallback: `ctest --test-dir build-ninja -R 'imageviewport_(provider_terminal|provider_lifecycle|render_commit|render_scenegraph|presentation_state|public_api|provider_playback)|viewportcontroller_(presentation|provider|playback)' --output-on-failure`

#### Risks / notes

- If adding a failing test is required by local change discipline, keep it scoped to the implementation milestone that fixes it and name the expected failure precisely. Do not leave the suite with unclassified red tests.

### Milestone 1: Add Failure-Scope Model and Seal Target-Spread Terminal State

#### Objective

Classify terminal failures by scope, seal required-role target-spread failures, and prevent late results from overwriting terminal spread state.

#### Source findings

Target-spread terminal state; provider callback flow; primary and secondary pipeline duplication.

#### Scope

- Define failure-scope data and escape paths.
- Add role-scoped terminal facts for the active target spread.
- Add a narrow role-scoped provider event gate for seal-sensitive callbacks.
- Route terminal provider, preparation, render, metadata, and dispatch-failure paths through aggregate projection where they affect a required role.

#### Out of scope

- Do not complete the full provider event-boundary cleanup.
- Do not role-index all controller state.
- Do not change public enum values.

#### Likely affected areas

- `src/imageviewportstate_p.h`
- `src/viewportcontroller_p.h`
- `src/viewportcontroller.cpp`
- `src/imageviewport.cpp`
- `src/imageviewportprovider.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [x] Add a failure-scope table in code or tests covering at least: session-open failure, metadata production failure, malformed/contradictory metadata, metadata public-limit/protocol violation, accurate unsupported metadata, active request unsupported, active request provider failure, payload admission failure, and render failure.
- [x] Encode escape paths: generation-terminal failures are escaped only by `clear()` or page-set/sequence replacement; display-request-terminal failures may be superseded by later valid explicit seek or playback-selected targets when generation metadata remains valid.
- [x] Introduce target-spread terminal state with per-required-role status, reason, diagnostics, failure scope, and sealed identity.
- [x] Implement aggregate terminal projection with `Error` over `Unsupported`, primary diagnostics on same-status ties, and winning-role diagnostics otherwise.
- [x] Add a narrow role-scoped provider event gate used by all seal-sensitive callbacks: metadata, frame, waiting/progress, terminal, cancellation, end-of-sequence, dispatch failure, preparation result, and render acknowledgement.
- [x] Convert primary and secondary provider frame terminal handlers to update role terminal state instead of directly writing aggregate request status.
- [x] Convert metadata terminal and session-open failure handling to role-aware generation-terminal state.
- [x] Gate display/playback commands according to failure scope.
- [x] Add and pass tests for terminal order matrices, secondary provider metadata/session-open failure, late opposite-role callbacks, and clear/replacement escape paths.

#### Execution record

Status: complete on 2026-07-04.

Implemented target-spread terminal state in `RequestState` with role-scoped status, reason, diagnostic, failure scope, sealed generation, and request identity. Provider frame, metadata, metadata admission/target policy, end-of-sequence protocol violation, session-open failure, payload admission, dispatch failure, and render-failure paths now publish through the aggregate terminal projection where they affect the active required spread. The projection uses `Error` over `Unsupported`, preserves primary diagnostics on same-status ties, and blocks non-terminal late callbacks from moving sealed spreads back to loading or ready.

Failure-scope coverage added in `ViewportControllerProviderTest::failureScopeTableClassifiesTerminalInputs_data` and `failureScopeTableClassifiesTerminalInputs`: session-open failure, metadata production failure, malformed metadata, contradictory metadata, metadata target unsupported by accurate runtime metadata, active frame unsupported, active frame provider failure, frame payload admission failure, metadata end-of-sequence protocol violation, frame end-of-sequence protocol violation, and render failure. Generation-terminal rows reject later display commands; display-request-terminal rows accept a later valid seek.

Spread-order and escape coverage added in `ImageViewportProviderTerminalTest::targetSpreadTerminalProjectionPrefersErrorOverUnsupported`, `secondaryTerminalFailureSealsSpreadAgainstLatePrimaryReady`, `primaryTerminalFailureSealsSpreadAgainstLateSecondaryReady`, and `clearAndReplacementEscapeSealedTargetSpread`. Secondary session-open generation-terminal behavior is covered by `ImageViewportProviderLifecycleTest::secondarySessionOpenFailureIsGenerationTerminalForSpread`.

Provider request token exhaustion remains a direct status assignment because its start-result structs do not carry change sets; this path is already covered by existing lifecycle token-overflow tests and is outside this milestone's narrow event/projection refactor.

Verification: `ctest --test-dir build-ninja -R 'imageviewport_provider_terminal|imageviewport_provider_lifecycle|viewportcontroller_provider' --output-on-failure`, `ctest --test-dir build-ninja -R 'imageviewport_render_commit' --output-on-failure`, and `just test` all passed on 2026-07-04. `just test` reported the non-fatal missing `WrapVulkanHeaders` CMake message, then passed 20/20 tests in `build-ninja`.

#### Acceptance criteria

- Required-role terminal failure seals the target spread and cannot be overwritten by later opposite-role loading, ready, progress, or terminal callbacks.
- Aggregate failure projection follows documented precedence.
- Generation-terminal and display-request-terminal failures have distinct command gating and escape paths.
- Secondary provider metadata/session-open failure is generation-terminal for the accepted spread.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_provider_terminal|imageviewport_provider_lifecycle|viewportcontroller_provider' --output-on-failure`
- `just test`

#### Risks / notes

- This milestone changes correctness-sensitive state ordering. Keep commits small: failure-scope table, target-spread terminal storage, event gate, then callback families.

### Milestone 2: Establish Typed Page-Set Input and Minimal Geometry Projection

#### Objective

Prepare page-set and geometry boundaries so replacement transition fixes operate on typed inputs and controller-authored geometry projection.

#### Source findings

Page-set content-position transition; C++ page-set API; geometry projection split; item-private owns controller responsibilities.

#### Scope

- Add typed internal page-set input before changing page-set transition sequencing.
- Add public typed C++ overloads while preserving compatibility.
- Move minimum geometry projection needed by page-set transition and render snapshots into controller/`PresentationGeometry`.

#### Out of scope

- Do not remove the existing QML/binding compatibility path.
- Do not replace all metadata projections.
- Do not remove legacy placement state yet.

#### Likely affected areas

- `src/imageviewport.h`
- `src/imageviewportfacade.cpp`
- `src/imageviewport.cpp`
- `src/generate_installed_public_header.cmake`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/presentationgeometry.cpp`
- `src/presentationgeometry_p.h`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/install_consumer/*`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_imageviewport_presentation_state.cpp`

#### Tasks

- [x] Add typed C++ `setPageSet(ImageSequence*, ImageSequence*, ...)` overloads and route internals through typed values.
- [x] Keep `QVariant` conversion as compatibility glue only if needed for QML, with tests proving invalid role values do not mutate viewport state or revision tokens.
- [x] Add install-consumer or public-header compile coverage that calls typed page-set overloads without `QVariant`.
- [x] Add typed internal page-set assignment input so `ImageViewportPrivate` supplies normalized sequences and policy only.
- [x] Add controller/geometry projection fields for content position, maximum content position, visible spread rect, per-role page rects, per-role item rects, rotation, mirror, gap, and render snapshot mapping.
- [x] Replace item-side duplicate formulas only for the geometry fields needed by page-set transition and render snapshot correctness.
- [x] Add tests for invalid page-role arguments, `setPageSet(null, secondary, policy)` clear-style behavior, and no revision changes on rejected values.

#### Execution record

Status: complete on 2026-07-04.

Authoritative-doc check: `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, and `docs/architecture/subsystem-boundaries.md` were re-read for this milestone. The durable specs already state the typed page-set end state and clear-style validation behavior, so no spec or architecture edits were required during plan execution.

Implemented public C++ typed `setPageSet(ImageSequence*, ImageSequence*)` and `setPageSet(ImageSequence*, ImageSequence*, PageSetTransitionPolicy)` overloads while preserving the existing `QVariant` invokable compatibility surface. `ImageViewportPrivate` now performs page-set assignment through typed `ImageSequence*` values after the item boundary normalizes dynamic inputs, so the transaction builder no longer depends on `QVariant` decoding.

Typed page-set coverage was added in `ImageViewportPublicApiTest::cppTypedPageSetOverloadsCompileAndReplaceSpread`; invalid dynamic page-role rejection is covered by `ImageViewportPublicApiTest::invalidPageRoleArgumentsPreserveRevisionTokens`; installed public-header coverage was added to `tests/install_consumer/main.cpp`.

Geometry projection now exposes content size, content position, maximum content position, pannability, visible spread/page rects, page item rects, rotation, mirror, gap, and spread sizing through `PresentationGeometry` and controller geometry state. Item-side duplicate formulas for displayed spread size and content-position-derived public properties now read from that projection path, and controller content-position helpers use the same projection for transition/pan calculations.

Verification: `ctest --test-dir build-ninja -R 'imageviewport_public_api|imageviewport_install_consumer|viewportcontroller_presentation|imageviewport_presentation_state' --output-on-failure` and `just test` passed on 2026-07-04. `just test` reported the non-fatal missing `WrapVulkanHeaders` CMake message, then passed 20/20 tests in `build-ninja`.

#### Acceptance criteria

- C++ consumers can compile against typed page-set overloads through the installed/public header path.
- Page-set internals no longer require `QVariant` decoding after item-boundary normalization.
- The geometry fields used by page-set transition and render snapshots come from one controller/geometry projection path.
- Invalid page-role values preserve request/display state and revision tokens.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_public_api|imageviewport_install_consumer|viewportcontroller_presentation|imageviewport_presentation_state' --output-on-failure`
- `just test`

#### Risks / notes

- Do not remove `QVariant` compatibility in this milestone. Full public signature removal remains deferred until QML/downstream compatibility is proven.

### Milestone 3: Apply Page-Set Transition Geometry Against Replacement Spread

#### Objective

Make page-set transition policy application use replacement spread geometry without losing no-partial-apply validation.

#### Source findings

Page-set content-position transition; minimal geometry projection from Milestone 2.

#### Scope

- Separate transition-policy validation from geometry application.
- Build or install replacement role facts before applying content-position transition effects.
- Preserve rollback behavior for invalid page-role arguments and invalid transition-policy fields.

#### Out of scope

- Do not redesign all presentation state.
- Do not remove legacy placement state.
- Do not change public transition-policy fields.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/presentationgeometry.cpp`
- `src/imageviewport.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_imageviewport_presentation_state.cpp`
- `tests/tst_imageviewport_public_api.cpp`

#### Tasks

- [x] Add a controller-level pending page-set or target-spread facts structure sufficient for replacement transition geometry.
- [x] Move content-position transition application after replacement spread facts are available, or compute it against an immutable pending replacement geometry snapshot.
- [x] Preserve invalid-policy no-partial-apply behavior and existing command diagnostics.
- [x] Ensure clear-style page-set commands validate policy but do not mutate presentation fields.
- [x] Add and pass tests for `ScanStart`, `ScanEnd`, and `Clamp` across changed bounds, changed orientation, changed page gap, and changed spread direction.

#### Execution record

Status: complete on 2026-07-04.

Authoritative-doc check: `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, `docs/architecture/subsystem-boundaries.md`, and `docs/architecture/rendering.md` were re-read for this milestone. The implementation follows the documented requirement that content-position handoff is computed after replacement spread bounds, rotation, mirror, direction, and gap are known.

Added controller tests `ViewportControllerPresentationTest::pageSetTransitionScanStartUsesReplacementSpreadGeometry`, `pageSetTransitionScanEndUsesReplacementSpreadGeometry`, and `pageSetTransitionClampUsesReplacementBounds`. These were red before implementation: `ScanStart` landed at `200,0`, `ScanEnd` at `300,100` instead of `500,100`, and `Clamp` at `300,100` instead of preserving the replacement-clamped `100,100`.

Added `ImageViewportPresentationStateTest::invalidPageSetTransitionPreservesStateAndRevisions` to cover no-partial-apply preservation for page set, presentation fields, geometry, playback phase, request diagnostics, and request/display revision tokens when transition policy validation fails.

Implemented accepted replacement geometry projection in the controller for page-set transition handoff. `assignSequence` now validates transition policy first, captures the previous content position, installs replacement role facts, and then applies `ScanStart`, `ScanEnd`, or `Clamp` against an accepted-replacement geometry snapshot rather than the prior displayed spread. Clear-style assignments still validate policy before delegating to clear and do not apply transition fields.

Verification: `ctest --test-dir build-ninja -R 'viewportcontroller_presentation|imageviewport_presentation_state|imageviewport_public_api' --output-on-failure` and `just test` passed on 2026-07-04. `just test` reported the non-fatal missing `WrapVulkanHeaders` CMake message, then passed 20/20 tests in `build-ninja`.

#### Acceptance criteria

- `contentPosition`, `maximumContentPosition`, `visibleSpreadRect`, primary/secondary page rects, primary/secondary item rects, and relevant revision tokens reflect the replacement spread immediately after accepted page-set replacement.
- No follow-up pan, scan, or reset command is needed after `ScanStart`, `ScanEnd`, or `Clamp`.
- Invalid transition policy leaves page set, presentation state, retained display, playback phase, diagnostics, and revision tokens unchanged.
- Clear-style page-set operations preserve presentation preferences as documented.

#### Verification

- `ctest --test-dir build-ninja -R 'viewportcontroller_presentation|imageviewport_presentation_state|imageviewport_public_api' --output-on-failure`
- `just test`

#### Risks / notes

- Transition sequencing can affect revision tokens and retained display. Verify revisions together with geometry.

### Milestone 4: Add Wait-State Projection Before Render Readiness Changes

#### Objective

Centralize loading wait facts and public wait-reason priority before new pending-render paths are introduced.

#### Source findings

Public metadata and wait-reason projections; built-in render readiness; target-spread aggregation.

#### Scope

- Add wait-state facts for provider waiting, request queued, upload pending, and render waiting.
- Add one projection helper with role aggregation and documented priority.
- Convert loading-state paths that Milestone 5 will touch.

#### Out of scope

- Do not move metadata observation projection yet.
- Do not change terminal reason handling except to keep terminal reasons outside wait projection.

#### Likely affected areas

- `src/imageviewportstate_p.h`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_frame_admission.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [x] Define wait-state data for active request and required roles.
- [x] Implement one helper that projects public reason priority: provider waiting, request queued, upload pending, render waiting.
- [x] Convert provider queueing, provider frame loading, upload pending, render waiting, and secondary spread wait paths touched by upcoming render-readiness work.
- [x] Add tests for simultaneous wait facts across required roles and request revision changes only when projected public reason changes.
- [x] Add structural inspection checks documenting where direct loading-reason writes are still temporarily allowed, if any.

#### Execution record

Status: complete on 2026-07-04.

Authoritative-doc check: `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, `docs/architecture/subsystem-boundaries.md`, `docs/architecture/provider-protocol.md`, and `docs/architecture/rendering.md` were re-read for this milestone. The implementation follows the documented wait-priority projection: provider waiting, request queued, upload pending, then render waiting, after aggregating required roles.

Implemented `TargetSpreadRoleWaitState`, `TargetSpreadWaitState`, and `ImageViewportInternal::projectWaitReason(...)` in internal state. Converted provider queueing, provider frame loading, upload-pending, render-waiting, and primary/secondary provider frame-admission spread waits to publish through the projection helper.

Added `ViewportControllerProviderTest::requiredRoleWaitPriorityAggregatesBeforeProjection_data` and `requiredRoleWaitPriorityAggregatesBeforeProjection` for direct wait-priority and required-secondary aggregation coverage. Added `ImageViewportProviderRequestsTest::waitProjectionRevisionChangesOnlyWhenPublicReasonChanges` to cover a primary payload becoming upload-pending while a required secondary role remains provider-waiting; the public reason stays `ProviderWaiting`, so `requestRevision` does not advance until the projected reason changes to `UploadPending`.

Structural inspection: `rg -n "RequestReason::(ProviderWaiting|RequestQueued|UploadPending|RenderWaiting)" src/viewportcontroller.cpp` still reports direct wait-reason writes in assignment/session-open setup, secondary provider command setup, metadata-bound target selection, render-synchronization predicates, resize/render-commit bookkeeping, and playback/end-of-sequence paths. These are temporarily allowed because Milestone 4 only converts the paths Milestone 5 will touch; the remaining direct writes must be eliminated by the later wait/readiness cleanup milestones.

Verification: `ctest --test-dir build-ninja -R 'imageviewport_provider_requests|imageviewport_provider_frame_admission|viewportcontroller_provider' --output-on-failure`, the structural `rg` command above, and `just test` passed on 2026-07-04. `just test` reported the non-fatal missing `WrapVulkanHeaders` CMake message, then passed 20/20 tests in `build-ninja`.

#### Acceptance criteria

- One helper owns wait priority for converted paths.
- Tests cover multiple simultaneous waits and role aggregation.
- Terminal unsupported/error reasons bypass wait projection.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_provider_requests|imageviewport_provider_frame_admission|viewportcontroller_provider' --output-on-failure`
- `rg -n "RequestReason::(ProviderWaiting|RequestQueued|UploadPending|RenderWaiting)" src/viewportcontroller.cpp`
- `just test`

#### Risks / notes

- The `rg` command may still show direct writes in not-yet-migrated paths. If so, document the temporary allowlist in code comments or the milestone commit message, and eliminate it by Milestone 7.

### Milestone 5: Render-Acknowledged Readiness for All Payload Sources

#### Objective

Route built-in still/timed-list payloads and mixed built-in/provider spreads through pending render identity and render acknowledgement before publishing `Ready`.

#### Source findings

Built-in frames publish `Ready` before render acknowledgement; wait-state projection; core render/request behavior depends on item probes.

#### Scope

- Stage built-in primary-only payloads as pending render payloads.
- Stage built-in/built-in and mixed built-in/provider spreads through complete-spread render acknowledgement.
- Preserve non-positive geometry behavior and retained/clear-before-load display transitions.

#### Out of scope

- Do not add render rotation.
- Do not introduce pure render-plan extraction.
- Do not remove private item probes.

#### Likely affected areas

- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportrender.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_still.cpp`
- `tests/tst_imageviewport_timed.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_imageviewport_provider_frame_admission.cpp`

#### Tasks

- [ ] Add pending render identity for built-in primary-only display requests.
- [ ] Add pending render identity for built-in primary plus built-in secondary spread commits.
- [ ] Add explicit handling and tests for built-in primary/provider secondary spread readiness.
- [ ] Add explicit handling and tests for provider primary/built-in secondary spread readiness.
- [ ] Update `beginRenderSynchronization` to include all built-in and mixed pending commits.
- [ ] Update `acknowledgeRenderCommit` and `acknowledgeRenderFailure` to handle built-in and mixed pending commits with stale-identity checks.
- [ ] Update request/display status projection so all display-critical content stays loading until matching render commit.

#### Acceptance criteria

- Built-in still and timed-list assignment report loading until render commit when positive geometry exists.
- Built-in/built-in, built-in/provider, and provider/built-in spreads publish `Ready` only after all required role payloads have matching render acknowledgement.
- Stale or failed render acknowledgement does not publish readiness.
- Non-positive item geometry remains `Loading/RenderWaiting` for uncommitted display-critical content.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_render_commit|imageviewport_still|imageviewport_timed|imageviewport_provider_frame_admission|viewportcontroller_provider' --output-on-failure`
- `just test`

#### Risks / notes

- Existing item tests may rely on synchronous readiness for built-in frames. Update expectations only where durable docs require render-acknowledged readiness.

### Milestone 6: Render Display Rotation

#### Objective

Make rendered pixels match public display rotation and coordinate conversion.

#### Source findings

Display rotation affects geometry but not rendered pixels; minimal geometry projection from Milestone 2.

#### Scope

- Add normalized quarter-turn rotation to render snapshots and per-role render layer data.
- Apply rotation in scene graph materialization consistently with public geometry and mirroring.
- Add stable tests for rotation and mirror composition.

#### Out of scope

- Do not add render failure causes in this milestone.
- Do not expose render backend or scene graph resources.
- Do not change source orientation policy semantics.

#### Likely affected areas

- `src/viewportcontroller_p.h`
- `src/viewportcontroller.cpp`
- `src/renderadapter_p.h`
- `src/renderadapter.cpp`
- `src/imageviewportrender.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- `tests/tst_imageviewport_presentation_state.cpp`

#### Tasks

- [ ] Add normalized rotation to `ViewportRenderLayer`, `ViewportRenderSnapshot`, and `RenderAdapter::Input::ImageLayer`.
- [ ] Apply quarter-turn rotation through node transforms or equivalent texture-coordinate mapping.
- [ ] Add a test matrix for 90, 180, and 270 degrees crossed with horizontal mirror, vertical mirror, and both mirrors.
- [ ] Use pixel assertions, transform assertions, or a pure render-layer mapping assertion that is stable across Qt backends.
- [ ] Verify source rectangles remain in public logical page space and half-open coordinate behavior is preserved.

#### Acceptance criteria

- Rendered pixels rotate consistently with public geometry and coordinate conversion for 90, 180, and 270 degrees.
- Mirroring and rotation composition is asserted for horizontal, vertical, and combined mirror states.
- Existing presentation geometry and coordinate conversion tests still pass.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_render_scenegraph|imageviewport_presentation_state' --output-on-failure`
- `just test`

#### Risks / notes

- QSG node transform inspection may be backend-sensitive. Prefer stable pixel/transform assertions or extract a small pure mapping helper if direct scene graph assertions are brittle.

### Milestone 7: Add Render Failure Causes

#### Objective

Preserve public render failure behavior while adding typed internal failure causes for diagnostics and tests.

#### Source findings

Render failures collapse distinct backend causes.

#### Scope

- Add typed internal render failure causes to render output and render acknowledgement.
- Populate causes for known failure paths.
- Preserve public `Error/RenderFailure` status and bounded diagnostics.

#### Out of scope

- Do not expose render failure causes publicly.
- Do not extract full render planning yet.
- Do not change render readiness semantics beyond cause propagation.

#### Likely affected areas

- `src/renderadapter_p.h`
- `src/renderadapter.cpp`
- `src/viewportcontroller_p.h`
- `src/viewportcontroller.cpp`
- `src/imageviewportrender.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`

#### Tasks

- [ ] Add `RenderFailureCause` values for missing window, texture creation failure, image-node creation failure, invalid or empty role payload, and unknown backend failure.
- [ ] Add cause field to internal render output and render acknowledgement.
- [ ] Populate distinct causes in `RenderAdapter::createNode`.
- [ ] Preserve public `requestStatus: Error`, `requestReason: RenderFailure`, and bounded `errorString`.
- [ ] Add tests that distinguish internal causes while asserting public status remains unchanged.

#### Acceptance criteria

- Missing window, texture creation failure, image-node creation failure, and invalid/empty role payload are distinguishable internally.
- Public `requestStatus`, `requestReason`, and public diagnostics remain compatible with specs.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_render_commit|imageviewport_render_scenegraph' --output-on-failure`
- `just test`

#### Risks / notes

- Some failure paths may be difficult to force with real QSG objects. Add fakes or a small render-adapter seam only if needed for deterministic tests.

### Milestone 8: Centralize Metadata Projection

#### Objective

Move public metadata observations into one role-scoped controller-owned projection path.

#### Source findings

Public metadata projections have multiple sources; sequence facts cross the controller boundary indirectly.

#### Scope

- Add role-scoped metadata projection for frame counts, durations, seek bounds, and capability support.
- Convert item getters to projection forwarding.
- Keep unavailable-value semantics unchanged.

#### Out of scope

- Do not change wait-reason projection; that is handled earlier.
- Do not replace `ViewportControllerContext` fact callbacks yet except where needed for projection.

#### Likely affected areas

- `src/imageviewport.cpp`
- `src/imageviewportcontroller.cpp`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `tests/tst_imageviewport_provider_metadata.cpp`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [ ] Define a role-scoped metadata projection type or controller getter set.
- [ ] Move construction-time and runtime provider metadata observation projection out of `ImageViewportPrivate`.
- [ ] Update item getters for primary and secondary frame count, duration, seek bounds, and capability support to forward projection values.
- [ ] Add tests for unknown provider facts, partial known facts, complete known metadata, runtime metadata, still, timed-list, primary role, and secondary role.
- [ ] Add structural inspection showing public metadata getters no longer branch on raw provider known facts in `ImageViewportPrivate`.

#### Acceptance criteria

- No public metadata getter branches on raw provider known facts in `ImageViewportPrivate`.
- Primary and secondary metadata observations come from the same projection path.
- Unknown, false, zero, and unavailable values remain externally distinguishable as specified.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_provider_metadata|imageviewport_public_api|viewportcontroller_provider' --output-on-failure`
- `rg -n "m_providerKnown|providerKnownFacts|provider.*Capability" src/imageviewport.cpp src/imageviewportcontroller.cpp src/imageviewportprovider.cpp`
- `just test`

#### Risks / notes

- Some raw provider fact references may remain in assignment or provider setup paths. The structural check should verify getter/projection removal, not ban legitimate construction-time reads.

### Milestone 9: Move Role Command Admission and Page-Set Target Selection Into Controller

#### Objective

Make the item boundary a typed adapter and make the controller the single owner of command admission, role routing, and initial target selection.

#### Source findings

Item-private owns controller responsibilities; secondary command invariants are enforced at facade call sites; sequence facts cross the controller boundary indirectly.

#### Scope

- Add role-parameterized controller commands for play, pause, stop, seek, and seek-to-position.
- Move secondary built-in/provider branching, bounds checks, and capability checks into controller-owned admission.
- Move secondary initial target derivation and provider-role classification into controller-owned page-set assignment.
- Add initial `RoleSource` or role-facts input so the controller does not call back through item context for facts derived from `requestState().sequence`.

#### Out of scope

- Do not remove `QVariant` compatibility.
- Do not role-index the full provider pipeline.
- Do not change public behavior for invalid commands except where current behavior violates documented admission order.

#### Likely affected areas

- `src/imageviewport.h`
- `src/imageviewportfacade.cpp`
- `src/imageviewport.cpp`
- `src/imageviewportcontroller.cpp`
- `src/imageviewportprovider.cpp`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [ ] Add controller APIs that accept `ImageViewport::PageRole` for play, pause, stop, seek, and seek-to-position.
- [ ] Move role presence, intrinsic target validation, capability checks, failure-scope checks, and provider/built-in dispatch into those controller APIs.
- [ ] Make item role-command methods perform only public value normalization, controller call, transport effect application, change application, and timer sync.
- [ ] Add `RoleSource` or equivalent role-facts inputs for page-set assignment: construction facts, capabilities, authored facts, provider session factory, and narrow payload access.
- [ ] Move secondary initial target derivation and provider/built-in classification from `ImageViewportPrivate::setPageSet` into controller-owned assignment logic.
- [ ] Add explicit admission tests for absent secondary role, malformed role, negative target, out-of-range target, unsupported capability, generation-terminal failure, display-request-terminal failure, and accepted valid target.

#### Acceptance criteria

- Public item tests and direct controller tests produce identical outcomes for role-addressed commands.
- `ImageViewportPrivate` no longer performs sequence-type, capability, or target-bound checks for role commands.
- Controller code no longer calls back into `ImageViewportPrivate` for facts derived from `controller.requestState()` in the migrated assignment paths.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_public_api|viewportcontroller_playback|viewportcontroller_provider|imageviewport_provider_requests|imageviewport_timed' --output-on-failure`
- `rg -n "isProvider\\(|isTimedList\\(|frameCount\\(|totalDuration\\(|providerKnownFacts" src/imageviewportcontroller.cpp src/imageviewport.cpp`
- `just test`

#### Risks / notes

- Some item-side sequence reads may remain for boundary conversion or transport lookup. Keep an explicit allowlist in commit notes until later role-source migration removes them.

### Milestone 10: Collapse Provider Events Into Controller Event Boundaries

#### Objective

Reduce item-side provider choreography by making each provider event family a single controller input with complete outputs.

#### Source findings

Provider callback flow is item-orchestrated; primary and secondary pipelines are duplicated; provider transport is too coupled to external effects.

#### Scope

- Add single controller event APIs for each provider callback family.
- Move manual item-side revision/signal decisions into controller `ViewportChangeSet` outputs.
- Keep transport delivery in `ImageViewportPrivate`.

#### Out of scope

- Do not replace `ViewportProviderBridge` threading implementation.
- Do not fully role-index all state.
- Do not change provider public adapter API.

#### Likely affected areas

- `src/imageviewportprovider.cpp`
- `src/viewportcontroller.cpp`
- `src/viewportcontroller_p.h`
- `src/viewportproviderbridge.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_metadata.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`

#### Tasks

- [ ] Add event-shaped controller methods for metadata ready callbacks, including primary and secondary roles.
- [ ] Add event-shaped controller methods for frame ready callbacks, including borrowed frames and owned frame handles.
- [ ] Add event-shaped controller methods for provider terminal callbacks: failed, unsupported with cause, unsupported without cause, and cancelled.
- [ ] Add event-shaped controller methods for provider waiting and progress callbacks.
- [ ] Add event-shaped controller methods for end-of-sequence callbacks.
- [ ] Add event-shaped controller methods for dispatch failure callbacks.
- [ ] Replace queued provider flush item-side start/revision emission with a controller result carrying `ViewportChangeSet` and transport effects.
- [ ] Convert item handlers to callback receipt, controller forwarding, transport delivery, change application, and timer sync only.
- [ ] Add controller tests for queued request state, stale queued flush rejection, dispatch failure, event-family stale handling, and revision effects without relying on event-loop draining.

#### Acceptance criteria

- Provider callback handlers in `ImageViewportPrivate` contain no manual revision increments and no metadata admission target-policy sequencing.
- Each listed provider event family has a controller event method.
- Queued provider transitions are testable through controller APIs without `QCoreApplication::processEvents()`.
- Primary and secondary callbacks use the same event shape with role data.

#### Verification

- `ctest --test-dir build-ninja -R 'viewportcontroller_provider|imageviewport_provider_requests|imageviewport_provider_metadata|imageviewport_provider_playback|imageviewport_provider_terminal' --output-on-failure`
- `rg -n "incrementRequestRevision\\(|emit q->requestStateChanged\\(|handleProviderMetadataAdmission\\(|handleProviderMetadataTargetPolicy\\(" src/imageviewportprovider.cpp`
- `just test`

#### Risks / notes

- Event-boundary changes can expose hidden ordering assumptions. Convert one callback family at a time and preserve transport effects exactly.

### Milestone 11: Add Role-Indexed Compatibility Accessors

#### Objective

Introduce role-indexed access without replacing all storage at once.

#### Source findings

Primary and secondary pipelines are duplicated; sequence facts cross controller boundary indirectly; target-spread terminal state needs role data.

#### Scope

- Add a role-indexed accessor layer over existing primary/secondary fields.
- Migrate one high-value provider path and token-validation path through the accessors.
- Leave full storage replacement deferred until evidence is available.

#### Out of scope

- Do not remove all paired fields.
- Do not rewrite the full controller.
- Do not change public role enum values or property names.

#### Likely affected areas

- `src/imageviewportstate_p.h`
- `src/viewportcontroller_p.h`
- `src/viewportcontroller.cpp`
- `src/imageviewportprovider.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`

#### Tasks

- [ ] Add role-indexed accessors for sequence pointer, active request, provider generation, metadata projection, pending payload, and terminal facts over existing storage.
- [ ] Move provider generation lookup and token matching through role-indexed accessors.
- [ ] Convert primary/secondary provider terminal handling to a role-parameterized implementation if not already done by earlier milestones.
- [ ] Convert one additional duplicated `handleSecondary...` path into a role-parameterized helper.
- [ ] Add parameterized tests that run the same token-validation and terminal cases for primary and secondary roles.

#### Acceptance criteria

- Primary and secondary token validation call the same role-parameterized helper.
- At least one duplicate secondary provider handler is removed or reduced to a forwarding wrapper.
- Full role-indexed storage replacement remains explicitly deferred.

#### Verification

- `ctest --test-dir build-ninja -R 'viewportcontroller_provider|imageviewport_provider_terminal' --output-on-failure`
- `rg -n "activeSecondaryProviderFrameTokenMatchesActiveRequest|handleSecondaryProvider.*Terminal|handleSecondaryProviderFrameEvent" src/viewportcontroller.cpp src/viewportcontroller_p.h`
- `just test`

#### Risks / notes

- This milestone intentionally stops at compatibility accessors. Removing paired storage comes later only after the accessor layer proves stable.

### Milestone 12: Add Provider Transport Test Seam

#### Objective

Make provider scheduling behavior testable without relying on real Qt event loops where protocol semantics do not require them.

#### Source findings

Provider transport is too coupled to external effects; core behavior depends on item probes.

#### Scope

- Introduce a narrow scheduler/executor seam for provider command delivery and cleanup.
- Add synchronous fake test support.
- Migrate named provider protocol tests that do not require Qt affinity.

#### Out of scope

- Do not replace production Qt provider bridge behavior.
- Do not remove all Qt-affinity integration tests.
- Do not change provider public adapter API.

#### Likely affected areas

- `src/viewportproviderbridge.cpp`
- `src/viewportproviderbridge_p.h`
- `tests/imageviewport_provider_test_support.h`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_contract.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`

#### Tasks

- [ ] Add a scheduler/executor interface limited to provider command delivery and cleanup queueing.
- [ ] Implement the production Qt scheduler/executor using existing `QMetaObject::invokeMethod` and thread-affinity behavior.
- [ ] Add a synchronous fake scheduler/executor for tests.
- [ ] Migrate cancellation, close, dispatch failure, and queued-result tests that do not require real Qt affinity to the fake.
- [ ] Keep an explicit allowlist of tests that may still use `QThread`, `QTRY_*`, or manual event draining because they verify Qt integration.

#### Acceptance criteria

- Listed provider protocol tests no longer require manual event draining unless on the explicit allowlist.
- Qt-affinity behavior still has focused integration coverage.
- Provider transport semantics remain unchanged.

#### Verification

- `ctest --test-dir build-ninja -R 'viewportcontroller_provider|imageviewport_provider_lifecycle|imageviewport_provider_contract|imageviewport_provider_requests' --output-on-failure`
- `rg -n "QTRY|QThread|processEvents|drainQueuedProviderResults" tests/tst_imageviewport_provider_lifecycle.cpp tests/tst_imageviewport_provider_contract.cpp tests/tst_imageviewport_provider_requests.cpp tests/imageviewport_provider_test_support.h`
- `just test`

#### Risks / notes

- Keep the seam narrow. Provider protocol interpretation remains controller-owned, not scheduler-owned.

### Milestone 13: Extract Pure Render Planning and Reduce Probes

#### Objective

Separate pure render planning from scene graph materialization and reduce reliance on public item probes after equivalent internal coverage exists.

#### Source findings

Render planning is coupled to scene graph allocation; core render/request behavior depends on public item test probes.

#### Scope

- Extract a `RenderPlan` or equivalent pure planning layer.
- Move non-QSG mapping tests to render-plan tests.
- Reduce private item probes only after coverage exists.

#### Out of scope

- Do not remove all scene graph tests.
- Do not change public render behavior.
- Do not remove probes used by still-unmigrated tests.

#### Likely affected areas

- `src/renderadapter.cpp`
- `src/renderadapter_p.h`
- `src/imageviewportrender.cpp`
- `tests/imageviewport_paint_test_support.h`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `src/imageviewport.h`

#### Tasks

- [ ] Extract pure render planning for background primitives, role layer order, source rectangles, rotation/mirror composition, and failure intent.
- [ ] Add render-plan tests that run without `QQuickWindow` or QSG allocation.
- [ ] Keep scene graph tests focused on materializing a known valid plan and a small set of backend failures.
- [ ] Compare private item probes against controller/render-plan coverage and remove only probes with equivalent internal tests.
- [ ] Keep an explicit allowlist of remaining probes and the tests that still require them.

#### Acceptance criteria

- Background, layer ordering, source-rect, rotation/mirror mapping, and failure-intent tests run without QSG allocation.
- Scene graph tests are focused on materialization.
- Removed private probes have equivalent controller/render-plan coverage.

#### Verification

- `ctest --test-dir build-ninja -R 'imageviewport_render_scenegraph|imageviewport_render_commit' --output-on-failure`
- `rg -n "ForTest|IMAGEVIEWPORT_PRIVATE_TEST_PROBES" src tests`
- `just test`

#### Risks / notes

- Do not remove probes for behavior still covered only through item tests.

### Milestone 14: Canonicalize Presentation State

#### Objective

Remove persistent legacy placement primitives after geometry projection and render behavior are stable.

#### Source findings

Legacy placement state remains canonical.

#### Scope

- Replace persistent legacy placement fields with canonical presentation state.
- Keep temporary translation helpers only where needed during migration.

#### Out of scope

- Do not change public property names or behavior.
- Do not alter `ImageSequence` internals.
- Do not split helper headers.

#### Likely affected areas

- `src/imageviewportstate_p.h`
- `src/presentationgeometry_p.h`
- `src/presentationgeometry.cpp`
- `src/imageviewportpresentation.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_imageviewport_presentation_state.cpp`

#### Tasks

- [ ] Replace persistent `fillMode`, alignment, raw `zoom`, and raw `pan` with canonical fit mode, zoom percent/manual zoom demand, and content position.
- [ ] Localize temporary compatibility math inside `PresentationGeometry` during migration.
- [ ] Update tests to assert final public behavior rather than legacy internal fields.
- [ ] Add structural check showing legacy placement fields are no longer persistent controller state.

#### Acceptance criteria

- Viewport state no longer persists legacy fill/alignment/raw zoom/raw pan fields.
- Public geometry behavior remains unchanged except where previous behavior violated durable docs.
- Presentation tests assert final model behavior.

#### Verification

- `ctest --test-dir build-ninja -R 'viewportcontroller_presentation|imageviewport_presentation_state|imageviewport_public_api' --output-on-failure`
- `rg -n "fillMode|horizontalAlignment|verticalAlignment|ContentPlacementMode|ContentHorizontalPlacement|ContentVerticalPlacement|\\.zoom|\\.pan" src`
- `just test`

#### Risks / notes

- This milestone is intentionally late because it changes canonical state shape.

### Milestone 15: Make `ImageSequence` Opaque Internally

#### Objective

Move source-specific `ImageSequence` storage out of the public header and behind private implementation interfaces.

#### Source findings

`ImageSequence` is not truly opaque.

#### Scope

- Introduce private `ImageSequenceData` or PIMPL storage.
- Move still, timed-list, and provider-backed fields into private implementation.
- Preserve public construction and viewport APIs.

#### Out of scope

- Do not change provider adapter public API.
- Do not change sequence factory outcomes.
- Do not change viewport request behavior.

#### Likely affected areas

- `src/imageviewport.h`
- `src/imagesequence.cpp`
- `src/imagesequencefactory.cpp`
- `src/imageviewportprovider.cpp`
- `src/generate_installed_public_header.cmake`
- `tests/tst_imagesequence_factory.cpp`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/install_consumer/*`

#### Tasks

- [ ] Introduce private sequence implementation storage for still, timed-list, and provider-backed data.
- [ ] Update factory/controller/provider internals to access sequence data through private typed interfaces.
- [ ] Remove sequence storage, timing internals, provider facts, and provider capabilities from the public header surface.
- [ ] Verify installed public headers expose only caller-facing sequence API.
- [ ] Update factory and install-consumer tests.

#### Acceptance criteria

- Public installed headers do not expose sequence storage, timing internals, provider facts, provider capabilities, private controller/render/provider types, or test probes.
- Provider and built-in sequence handling operate through private implementation interfaces.
- Factory and install-consumer behavior remains compatible.

#### Verification

- `ctest --test-dir build-ninja -R 'imagesequence_factory|imageviewport_public_api|imageviewport_install_consumer|imageviewport_provider_.*' --output-on-failure`
- `rg -n "TimingModel|m_provider|m_timingIntervals|m_frameImages|friend class ImageViewportPrivate|IMAGEVIEWPORT_PRIVATE_TEST_PROBES" src/imageviewport.h`
- `just test`

#### Risks / notes

- This touches public header shape and packaging. Keep changes tightly scoped and run install-consumer verification.

### Milestone 16: Split Boundary Helper Policy

#### Objective

Reduce cross-boundary coupling from the broad private helper header.

#### Source findings

Catch-all helper header couples separate boundaries.

#### Scope

- Split helper policy by owner after prior boundary moves have stabilized.
- Include only owner-specific helper headers in each subsystem.

#### Out of scope

- Do not change behavior.
- Do not use this milestone for unrelated cleanup.
- Do not start before correctness and public-header milestones are stable.

#### Likely affected areas

- `src/imageviewporthelpers_p.h`
- `src/framepreparation.cpp`
- `src/imagesequencefactory.cpp`
- `src/imageviewport.cpp`
- `src/imageviewportcontroller.cpp`
- `src/presentationgeometry.cpp`
- `src/viewportcontroller.cpp`

#### Tasks

- [ ] Split limits/admission helpers into an owner-specific private header.
- [ ] Split provider fact/capability helpers into an owner-specific private header.
- [ ] Split diagnostics/redaction helpers into an owner-specific private header.
- [ ] Split public value validation helpers into an owner-specific private header.
- [ ] Split geometry utilities into an owner-specific private header.
- [ ] Remove or leave an empty compatibility `imageviewporthelpers_p.h` only if needed temporarily, with an explicit allowlist and removal task.

#### Acceptance criteria

- Provider capability logic is not visible to geometry code.
- Geometry utilities are not visible to factory/preparation code.
- No subsystem includes a generic helper header for unrelated policy.
- Any remaining `imageviewporthelpers_p.h` include is on an explicit temporary allowlist.

#### Verification

- `rg -n "#include \"imageviewporthelpers_p.h\"" src tests`
- `just build`
- `just test`
- `just lint` if the environment has `run-clang-tidy`, `clazy-standalone`, and `cmake-lint`.

#### Risks / notes

- This milestone should be mostly mechanical. If behavior changes are required, stop and split those changes into a separate plan update.

## Deferred / Needs Investigation

### Full removal of public `QVariant` page-set signatures

Typed C++ overloads are implementation-ready, but full removal of `QVariant` public metaobject signatures needs investigation because QML may still require a generic invokable binding path. Additional evidence needed: Qt QML overload behavior for `ImageSequence*`/`null` plus `PageSetTransitionPolicy`, and compatibility expectations in downstream consumers.

### Full role-indexed storage replacement

Role-indexed compatibility accessors are implementation-ready, but complete storage replacement may be too broad before the accessor layer proves stable. Additional evidence needed: which paired fields can be migrated independently without destabilizing playback stop-restoration and retained-display identity.

### Complete private probe removal

Shrinking probes is implementation-ready only after equivalent controller/render-plan tests exist. Additional evidence needed: coverage comparison showing render acknowledgement, playback advancement, provider token identity, and stale-result behavior are covered without item probes.

### Durable documentation ambiguity

No current durable-doc ambiguity is known. If future implementation discovers one, stop plan execution and request a separate design/doc decision outside these milestones.

## Suggested `/goal` Execution Order

1. Milestone 0: Confirm Baseline and Characterization Targets.
2. Milestone 1: Add Failure-Scope Model and Seal Target-Spread Terminal State.
3. Milestone 2: Establish Typed Page-Set Input and Minimal Geometry Projection.
4. Milestone 3: Apply Page-Set Transition Geometry Against Replacement Spread.
5. Milestone 4: Add Wait-State Projection Before Render Readiness Changes.
6. Milestone 5: Render-Acknowledged Readiness for All Payload Sources.
7. Milestone 6: Render Display Rotation.
8. Milestone 7: Add Render Failure Causes.
9. Milestone 8: Centralize Metadata Projection.
10. Milestone 9: Move Role Command Admission and Page-Set Target Selection Into Controller.
11. Milestone 10: Collapse Provider Events Into Controller Event Boundaries.
12. Milestone 11: Add Role-Indexed Compatibility Accessors.
13. Milestone 12: Add Provider Transport Test Seam.
14. Milestone 13: Extract Pure Render Planning and Reduce Probes.
15. Milestone 14: Canonicalize Presentation State.
16. Milestone 15: Make `ImageSequence` Opaque Internally.
17. Milestone 16: Split Boundary Helper Policy.

## Plan Review Notes

Implementation-feasibility review was performed by four read-only review subagents against the draft plan, the temporary design review, and the durable docs.

- Concerns addressed: unspecified failing tests were removed from Milestone 0; future red tests must be named and scoped; authoritative doc edits are prohibited during milestone execution; terminal failure scope now distinguishes generation-terminal and display-request-terminal behavior; typed page-set input and minimal geometry projection were moved earlier; wait-state projection now precedes render-readiness changes; provider sealing now has a narrow event gate before the full provider-event cleanup; playback characterization was added; mixed built-in/provider spreads are covered; render rotation, render failure causes, provider transport seams, render planning, presentation cleanup, `ImageSequence` opacity, and helper splitting are now separate milestones with concrete verification.
- Concerns deferred: full public `QVariant` signature removal, full role-indexed storage replacement, complete private probe removal, and any future durable-doc ambiguity are listed under `Deferred / Needs Investigation`.
- Remaining known limitations: some structural verification uses `rg` inspection commands that may need temporary allowlists while intermediate milestones are in progress. Each milestone that uses such a command must document any allowlist in its implementation commit or update this plan if the allowlist reveals a blocker.

## Non-Goals

- Do not implement source changes while updating this plan.
- Do not change `docs/spec/**` or `docs/architecture/**` as part of executing these milestones. If durable docs appear stale, ambiguous, or inconsistent, stop and request a separate design decision.
- Do not broaden public API surface for files, URLs, archives, raw provider objects, scene graph resources, native textures, tiled loading, region loading, render backend selection, or full color-management policy.
- Do not remove retained-display behavior, compatibility primary-only observations, provider adapter token semantics, authored animation semantics, or public revision-token semantics.
- Do not rewrite the full controller, provider transport, or render adapter in one step.
- Do not hide behavior changes inside cleanup commits.
- Do not delete characterization tests after the implementation passes; keep them as regression coverage unless superseded by clearer equivalent tests.
