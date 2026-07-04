# Implementation Plan

## Purpose

This plan consolidates the design review findings into executable implementation work for future `/goal` sessions. It is self-contained: future sessions can execute this plan using the durable specs and architecture docs without reading the temporary design review document.

## Source Basis

- `docs/spec/**`
- `docs/architecture/**`

`DESIGN_REVIEW_CORRECT_END_STATE.md` was used as a temporary planning input during plan creation. Its actionable implementation content has been consolidated here, and the temporary document is no longer required for future execution.

No temporary design review finding was overridden by authoritative docs. The design review found both `docs/spec/**` and `docs/architecture/**` present and found no high-confidence conflict between them. No durable spec or architecture update was required for this planning task because the documented end state already covers public-header opacity, command diagnostics, page-set transactions, provider role symmetry, provider unsupported causes, sequence ownership, pure render planning, render failure causes, provider transport seams, and controller ownership.

## Execution Principles

- Preserve current behavior unless fixing a documented bug, spec violation, or architecture-contract violation.
- Add characterization tests before risky refactors, especially for provider request ordering, retained display, stale-result rejection, render acknowledgements, page-set transitions, playback restoration, and cleanup/cancel public-state preservation.
- Keep changes incremental; do not combine behavior fixes with broad ownership cleanup when the plan separates them.
- Keep `docs/spec/**` and `docs/architecture/**` immutable during implementation unless a new blocker proves the durable end-state docs are ambiguous or wrong; if that happens, stop, update this plan, and handle the durable-doc change as a separate intent step.
- Isolate domain logic from external effects such as Qt event-loop scheduling, provider transport, QObject affinity, scene graph materialization, clocks, and render backend allocation.
- Preserve public APIs unless a durable spec explicitly requires a public behavior correction.
- Keep public diagnostics bounded and stable; add private structured diagnostics for operational cause context.
- Update this plan if implementation discovers a blocker, invalid assumption, or unsafe milestone split.

## Consolidated Design Findings

### Finding F1: Installed public header exposes private and scene graph details

- Priority: P1.
- Affected areas: `src/imageviewport.h`, `src/generate_installed_public_header.cmake`, `src/imageviewportfacade.cpp`, `tests/install_consumer/run.cmake`.
- Correct end state: Installed consumers see only supported public API and value types. `ImageSequence` remains opaque at the public-header boundary, private access helpers stay in private headers, and `QSGNode` or other scene graph ownership types do not appear in installed headers.
- Why it matters: The package contract says public headers must not expose private controller, provider transport, render adapter, scene graph, native texture, or test-probe types. Current header generation leaks `ImageViewportInternal`, `ImageSequenceData`, private access helpers, and `QSGNode`.

### Finding F2: Mirror commands accept anchors but do not preserve them

- Priority: P0.
- Affected areas: `src/viewportcontrollerpresentation.cpp`, `src/viewportcontrollerhelpers_p.h`, `tests/tst_imageviewport_presentation_state.cpp`, `tests/tst_viewportcontroller_presentation.cpp`.
- Correct end state: `setMirrorHorizontally(enabled, anchor)` and `setMirrorVertically(enabled, anchor)` capture previous geometry, mutate one mirror flag, preserve the spread point under the anchor when possible, and clamp content position when preservation is impossible.
- Why it matters: The public API spec says mirror commands keep the anchor stable when possible. Current implementations validate the anchor and then call unanchored setters, causing visible geometry jumps.

### Finding F3: Invalid page-set arguments bypass command diagnostics

- Priority: P2.
- Affected areas: `src/imageviewport.cpp`, `src/viewportcontrollerhelpers_p.h`, `tests/tst_imageviewport_public_api_commands.cpp`, QML command diagnostics tests if needed.
- Correct end state: Invalid page-role arguments preserve accepted page-set, request status, display status, playback phase, request revision, and display revision, but publish `CommandReason::InvalidRequest` and advance `commandRevision` through the normal command-diagnostic boundary.
- Why it matters: Invalid commands that reach the viewport command boundary publish command diagnostics unless explicitly documented otherwise. Current invalid `setPageSet(...)` page-role conversions return `Invalid` directly and leave command diagnostics stale.

### Finding F4: Provider still known facts can contradict frame-seek support

- Priority: P0.
- Affected areas: `src/imageviewportproviderfacts_p.h`, `src/imagesequencefactory.cpp`, `src/viewportcontrollerhelpers_p.h`, `src/viewportcontroller.cpp`, provider known-facts tests.
- Correct end state: Provider construction-fact admission is the single source of truth for fact/capability compatibility. Still known facts reject declared-false frame seek support and always project one indexed frame with frame seek support `True` and bounds `0..0`.
- Why it matters: The specs define still-image sequences and still provider timing as one indexed frame with frame seeking supported. Current validation accepts `ImageSequenceProviderKnownFacts::still(...)` with declared-false frame seek support, allowing contradictory public metadata.

### Finding F5: Explicit metadata unsupported payload causes are collapsed

- Priority: P0.
- Affected areas: `src/viewportproviderbridge_p.h`, `src/viewportproviderbridge.cpp`, `src/viewportcontrollerhelpers_p.h`, provider terminal projection and diagnostics tests.
- Correct end state: Provider events preserve whether an unsupported cause was explicit. Metadata-token results default to `UnsupportedRequest` only for legacy `providerUnsupported(...)`; explicit `providerUnsupportedWithCause(..., PayloadRejection, ...)` maps to `RequestReason::PayloadRejection`.
- Why it matters: The provider adapter spec distinguishes unsupported operation from unsupported payload. Current metadata terminal projection ignores explicit `PayloadRejection`, so callers and diagnostics lose a documented public distinction.

### Finding F6: Provider role handling is primary-specialized with secondary overlays

- Priority: P1.
- Affected areas: `src/imageviewportstate_p.h`, `src/viewportcontroller_p.h`, `src/viewportcontrollerhelpers_p.h`, `src/viewportcontrollerprovider.cpp`, `src/viewportcontrollerplayback.cpp`, `src/viewportcontrollerrender.cpp`, provider role tests.
- Correct end state: Primary and secondary role state use role-scoped private slots. Provider dispatch, playback, seek, metadata, frame admission, terminal handling, progress, cancellation, close, payload release, render-facing acknowledgement, token matching, queued work, and same-result-channel behavior operate through role-parameterized paths before spread aggregation.
- Why it matters: The provider protocol requires primary and secondary sessions to use the same result channel and stale-result rules. Current secondary provider work has separate direct-start paths and several secondary-specific handlers, increasing risk around supersession, cancellation, queued requests, terminal results, and future provider changes.

### Finding F7: Provider queue flushing is tied to raw Qt event-loop delivery

- Priority: P1.
- Affected areas: `src/imageviewportprovider.cpp`, `src/viewportcontrollerprovider.cpp`, `src/viewportcontroller_p.h`, `src/viewportproviderbridge_p.h`, `src/viewportproviderbridge.cpp`, provider request tests and provider test support.
- Correct end state: Deferred provider queue flushing is a typed scheduler effect. Production uses a Qt queued scheduler for reentrancy-sensitive delivery, while deterministic tests can substitute synchronous delivery for cases that do not assert Qt affinity or event-loop behavior.
- Why it matters: Controller-owned provider queue transitions currently depend on `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` from item-side transport code. Tests must pump global `MetaCall` events even when the behavior under test is provider serialization rather than Qt delivery.

### Finding F8: Sequence source ownership is split across item context and a global registry

- Priority: P1.
- Affected areas: `src/imagesequenceownership.cpp`, `src/imagesequenceownership_p.h`, `src/imagesequencefactory.cpp`, `src/imageviewport.cpp`, `src/imageviewportcontroller.cpp`, `src/imageviewportprovider.cpp`, `src/viewportcontroller_p.h`, `src/imageviewportstate_p.h`.
- Correct end state: Each accepted role generation owns one internal sequence-source handle or value snapshot that encapsulates lifetime plus immutable/provider construction facts. The item does not recover owners from raw pointers through a process-global registry, and the controller does not need broad item-service getters to inspect sequence facts.
- Why it matters: The sequence architecture says `ImageSequence` is an opaque handle and sequence data is the authority for construction-time facts. Current ownership is split across factory results, a global `ImageSequence* -> weak_ptr` registry, item-private accessors, controller context getters, and request state storing both `QPointer` and `shared_ptr`.

### Finding F9: Page-set transaction logic and display-transition intent leak out of the controller

- Priority: P1.
- Affected areas: `src/imageviewport.cpp`, `src/viewportcontroller_p.h`, `src/viewportcontroller.cpp`, `src/viewportcontrollerhelpers_p.h`, page-set transition tests.
- Correct end state: `ImageViewportPrivate::setPageSet(...)` validates and converts public page-role inputs, then passes sequence handles and `PageSetTransitionPolicy` to the controller. The controller derives role facts through the sequence-source boundary and applies `displayTransition` exactly once inside the page-set transaction. `ViewportSequenceAssignment` does not carry a second `retainPreviousDisplay` source of truth.
- Why it matters: Page-set replacement is a controller-owned atomic transaction. Current item code reads private sequence facts, constructs role-source data, and derives retain-versus-clear intent before controller policy normalization.

### Finding F10: Controller business transitions live in a shared helper header

- Priority: P1.
- Affected areas: `src/viewportcontrollerhelpers_p.h`, `src/viewportcontrollerprovider.cpp`, `src/viewportcontrollerplayback.cpp`, `src/viewportcontrollerpresentation.cpp`, `src/viewportcontrollerrender.cpp`, `src/viewportcontrollermetadata.cpp`.
- Correct end state: `viewportcontrollerhelpers_p.h` contains only small pure utilities, role selectors, and value projections. Mutating transitions live in responsibility-owned controller modules.
- Why it matters: The helper header currently contains core state-machine mutations for command diagnostics, presentation transitions, provider/request terminals, explicit seek acceptance, stop restoration, render synchronization, and playback effects. This obscures ownership.

### Finding F11: Render planning still depends on `QQuickWindow`

- Priority: P2.
- Affected areas: `src/renderadapter_p.h`, `src/renderadapter.cpp`, `src/imageviewportrender.cpp`, `tests/tst_imageviewport_render_scenegraph.cpp`.
- Correct end state: Render planning is a Qt-free value step that consumes prepared payload and presentation values and returns background primitives, ordered role layers, transforms, source/target rectangles, payload identities, and value-detectable invalid-payload failures. Scene graph materialization owns missing-window, texture creation, image-node creation, and backend allocation failures.
- Why it matters: The rendering architecture separates pure render planning from scene graph materialization. Current `RenderAdapter::createPlan(...)` accepts `QQuickWindow*` and reports `MissingWindow`, so pure layer-mapping tests need Qt Quick window setup.

### Finding F12: Render and cleanup operational diagnostics lose actionable cause context

- Priority: P1.
- Affected areas: `src/viewportcontroller_p.h`, `src/viewportcontrollerrender.cpp`, `src/imageviewportstate_p.h`, `src/viewportproviderbridge_p.h`, `src/viewportproviderbridge.cpp`, `src/imageviewportprovider.cpp`, render/controller tests, provider lifecycle tests.
- Correct end state: Public request projection remains bounded and stable, but private diagnostics retain structured operational cause context: role, generation/request/payload identity, `RenderFailureCause`, cleanup operation, and safe token identity. Cleanup/cancel delivery failures do not mutate public state, but they are observable through private test probes or structured logs.
- Why it matters: Render failure causes are selected and forwarded, then dropped at the controller boundary. Provider close/cancel delivery failures are ignored or logged generically.

## Milestones

### Milestone 1: Establish Baseline and Characterization Coverage

Status: Complete. Verified against the authoritative spec and architecture docs before execution. `build-ninja` is the active configured test build, the targeted baseline suite passed, and no unrelated baseline failures were observed. Existing characterization coverage already protects the named preservation observations, including request/display status and revision preservation, retained versus empty display, stale provider and render result rejection, stop restoration, metadata-bound target selection, playback waiting, role-scoped provider token handling, token cleanup, late cleanup acknowledgements, and no public-state mutation after cleanup/cancel callbacks from closed generations.

#### Objective

Establish a reliable verification baseline and add behavior-preserving characterization coverage before changing controller ownership, provider dispatch, sequence ownership, render boundaries, or external-effect seams.

#### Source findings

F6, F7, F8, F9, F10, F11, F12.

#### Scope

- Confirm the active build directory and test invocation.
- Run named existing tests that already protect request/display separation, retained display, provider stale-result rejection, render acknowledgement identity, stop restoration, metadata-bound target selection, playback waiting, role-scoped provider behavior, and cleanup/cancel public-state preservation.
- Add missing characterization tests only for behavior that should be preserved during later refactors.
- Record any currently failing baseline separately from implementation failures.

#### Out of scope

- Do not fix source behavior in this milestone.
- Do not add tests that intentionally fail for known conformance bugs here; add those immediately before the corresponding implementation milestone.
- Do not modify `docs/spec/**` or `docs/architecture/**`.

#### Likely affected areas

- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`
- `tests/tst_imageviewport_provider_metadata.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_timed.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`

#### Tasks

- [x] Confirm whether `build-ninja` is the active configured test build with `ctest --test-dir build-ninja -N`; if not, identify the active build directory or configure one with tests enabled.
- [x] Run these existing baseline tests: `ImageViewportProviderTerminalRecoveryTest::providerFrameFailureRetainsDisplayAndClearsOnSeek`, `ImageViewportProviderRequestsTest::providerTimedFrameSeekCancelsSupersededRequest`, `ImageViewportRenderCommitTest::providerSupersededRenderFailureIsIgnored`, `ImageViewportTimedTest::timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay`, `ImageViewportProviderMetadataTest::providerTimedMetadataSelectsInitialFrameRequest`, `ImageViewportProviderPlaybackTest::providerTimedPlaybackWaitsForMetadata`, `ViewportControllerProviderTest::queuedProviderFlushReturnsChangesAndTransport`, `ViewportControllerProviderTest::providerFrameEventsRejectStaleTokensByRole`, and `ViewportControllerProviderTest::providerTerminalEventsCloseMetadataGenerationByRole`.
- [x] Run cleanup/lifecycle baseline tests that assert cleanup/cancel behavior does not block or mutate public state: `ImageViewportProviderLifecycleTest::providerDestructionCancelsActiveFrameRequestBeforeClose`, `ImageViewportProviderLifecycleTest::providerReplacementCancelsActiveFrameRequestBeforeClose`, `ImageViewportProviderLifecycleTest::providerClearCancelsActiveFrameRequestBeforeClose`, `ImageViewportProviderLifecycleTest::providerClearIgnoresLateSecondaryCallbacks`, `ImageViewportProviderLifecycleTest::providerClearDoesNotBlockOnSessionCleanup`, `ImageViewportProviderLifecycleTest::providerClearIgnoresCancelledMetadataAcknowledgement`, and `ImageViewportProviderLifecycleTest::providerClearIgnoresCancelledFrameAcknowledgement`.
- [x] Add missing characterization tests, if any, for the same public observations: request/display status, request/display revision changes, playback phase, retained versus empty display, stale-result rejection, token cleanup, and no public-state mutation on best-effort cleanup/cancel delivery. Existing tests already cover the Milestone 1 preservation observations; no additional tests were needed here. Failed cleanup/cancel delivery preservation remains the explicit Milestone 4 pre-scheduler characterization task.
- [x] Keep characterization tests independent of private implementation names where possible; use private probes only where the existing test layer already exposes the needed identity.
- [x] Document any unrelated baseline failure in this plan before continuing. None observed.

#### Acceptance criteria

- The future implementation session knows the active build/test command and build directory.
- The named baseline tests pass or have documented unrelated failures before behavior changes begin.
- New characterization tests protect behavior that should survive later refactors and pass against the current implementation.
- No implementation source behavior is changed in this milestone.

#### Verification

- `ctest --test-dir build-ninja -N`
- `ctest --test-dir build-ninja -R 'imageviewport_provider_requests|imageviewport_provider_terminal|imageviewport_provider_metadata|imageviewport_provider_playback|imageviewport_provider_lifecycle|imageviewport_render_commit|imageviewport_timed|viewportcontroller_provider|viewportcontroller_playback|viewportcontroller_presentation' --output-on-failure`
- If `build-ninja` is not configured, first run `cmake -S . -B build-ninja -G Ninja -DIMAGEVIEWPORT_BUILD_TESTS=ON`, then rerun the commands above.

#### Risks / notes

- Characterization coverage must not bless current spec violations. For known violations, add failing end-state tests in the milestone that fixes the behavior.
- If current baseline failures exist, avoid mixing them with implementation changes.

### Milestone 2: Fix Public API and Command Conformance

Status: Complete. Verified against the authoritative public API, package, item-boundary, controller-boundary, and rendering docs before execution. The installed public header is generated without private namespace helpers, `ImageSequenceData`, private access helpers, or `QSGNode`; mirror presentation commands preserve valid anchors; invalid page-role arguments now publish `InvalidRequest` command diagnostics while preserving accepted request/display state and request/display revisions.

#### Objective

Correct the public/package and command conformance gaps that are already fully specified: installed-header opacity, anchor-preserving mirror commands, and invalid page-set command diagnostics.

#### Source findings

F1, F2, F3.

#### Scope

- Add end-state tests for forbidden installed-header tokens.
- Add end-state tests for horizontal and vertical mirror anchor preservation.
- Add end-state tests for invalid page-role arguments publishing command diagnostics while preserving request/display state.
- Implement only the minimal source changes needed for those conformance fixes.

#### Out of scope

- Do not change the public QML or C++ API shape except to remove private/scene graph exposure from installed headers.
- Do not refactor sequence ownership beyond what is required to keep installed public headers opaque.
- Do not change page-set transition behavior beyond invalid command diagnostics.
- Do not modify provider dispatch, role state, render planning, or helper-header ownership.

#### Likely affected areas

- `src/imageviewport.h`
- `src/generate_installed_public_header.cmake`
- `src/imageviewportfacade.cpp`
- `src/imageviewport.cpp`
- `src/viewportcontrollerpresentation.cpp`
- `src/viewportcontrollerhelpers_p.h`
- `tests/install_consumer/run.cmake`
- `tests/tst_imageviewport_presentation_state.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`
- `tests/tst_imageviewport_public_api_commands.cpp`

#### Tasks

- [x] Add install-consumer assertions that the installed header does not contain `ImageViewportInternal`, `ImageSequenceData`, private access helper names, or `QSGNode`.
- [x] Move or hide sequence private storage and scene graph override exposure from the installed public header while preserving downstream public API compilation.
- [x] Add mirror-anchor tests that compare `itemToSpread(anchor)` before and after `setMirrorHorizontally(...)` and `setMirrorVertically(...)` on geometry where preservation is possible.
- [x] Update mirror command implementations to use the same anchored preservation and clamping path as rotation.
- [x] Add invalid page-set argument tests that assert `CommandOutcome::Invalid`, unchanged accepted page-set, unchanged request/display status, unchanged request/display revisions, unchanged playback phase, `CommandReason::InvalidRequest`, and advanced `commandRevision`.
- [x] Route invalid page-role conversion through the normal command-diagnostic boundary without accepting a new request or display state.

#### Acceptance criteria

- Installed `include/ImageViewport/imageviewport.h` contains no private namespace helpers, no `ImageSequenceData`, and no `QSGNode`.
- Existing install consumers still compile against the installed package target.
- Horizontal and vertical mirror commands preserve a valid anchor when possible and clamp when preservation is impossible.
- Invalid page-role values return `Invalid`, preserve accepted page-set, request/display state, playback phase, and request/display revisions, publish `InvalidRequest`, and advance `commandRevision`.

#### Verification

- `cmake --build build-ninja --target imageviewport_public_api_commands imageviewport_presentation_state viewportcontroller_presentation`
- `ctest --test-dir build-ninja -R 'imageviewport_install_consumer|imageviewport_public_api_commands|imageviewport_presentation_state|viewportcontroller_presentation' --output-on-failure`
- Completed run also built `ImageViewportInstalledPublicHeader` before the install-consumer check so the installed package used the regenerated sanitized header.

#### Risks / notes

- Header cleanup may need a generated installed-header facade before deeper sequence ownership work. Keep the public package boundary correct without destabilizing the runtime item implementation.
- Invalid argument diagnostics must not alter accepted request/display state or request/display revision tokens.

### Milestone 3: Centralize Provider Still Facts and Terminal Cause Semantics

Status: Complete. Verified against the authoritative provider API, provider adapter, and provider protocol docs before execution. Still construction facts now reject frame-seek declared-false capabilities before a provider sequence is created; existing still-fact projection tests continue to prove the public still timing contract; explicit metadata-token unsupported causes now preserve `PayloadRejection` while legacy metadata unsupported results continue to default to `UnsupportedRequest` for both provider roles.

#### Objective

Make the documented provider still-fact invariant and explicit unsupported-cause mapping single-source before larger provider dispatch changes.

#### Source findings

F4, F5.

#### Scope

- Reject contradictory still known facts plus declared-false frame seek support during provider-backed sequence construction.
- Ensure still known facts always project frame seek support `True` and bounds `0..0`.
- Preserve explicit unsupported causes for metadata tokens while maintaining legacy defaults for cause-less unsupported results.
- Cover primary and secondary roles where provider terminal projection is role-scoped.

#### Out of scope

- Do not change provider session command ordering or role-dispatch structure in this milestone.
- Do not change public enum names or add new public statuses.
- Do not reinterpret legacy `providerUnsupported(...)` defaults.
- Do not invent a complete-timed-facts capability contradiction rule; that ambiguity is deferred below unless durable docs are clarified first.

#### Likely affected areas

- `src/imageviewportproviderfacts_p.h`
- `src/imagesequencefactory.cpp`
- `src/viewportcontrollerhelpers_p.h`
- `src/viewportcontroller.cpp`
- `src/viewportproviderbridge_p.h`
- `src/viewportproviderbridge.cpp`
- `tests/tst_imagesequence_factory.cpp`
- `tests/tst_imageviewport_provider_metadata.cpp`
- `tests/tst_imageviewport_provider_terminal_projection.cpp`
- `tests/tst_imageviewport_provider_terminal_diagnostics.cpp`

#### Tasks

- [x] Add provider construction tests for `ImageSequenceProviderKnownFacts::still(...)` combined with declared-false frame seek support.
- [x] Add assignment/projection tests proving valid still known facts expose frame count `1`, frame seek support `True`, frame seek bounds `0..0`, position seek support `False`, and timed playback support `False`. Existing role-scoped projection coverage already proved this still timing contract.
- [x] Update provider fact/capability contradiction validation so still known facts cannot be combined with declared-false frame seek support.
- [x] Add provider metadata terminal tests for `providerUnsupportedWithCause(..., PayloadRejection, ...)` and legacy `providerUnsupported(...)`. New explicit-cause coverage was added for primary and secondary metadata tokens; existing legacy metadata-token coverage was preserved.
- [x] Preserve an explicit-cause bit across provider bridge events and terminal projection.
- [x] Update metadata terminal projection to honor explicit `PayloadRejection` while preserving legacy metadata-token default `UnsupportedRequest`.
- [x] Add a note to the deferred timed-fact item if implementation discovers durable docs already define a precise complete-timed-facts capability contradiction. No durable complete-timed-facts contradiction rule was found during implementation, so no deferred note change was needed.

#### Acceptance criteria

- Contradictory still known facts and declared-false frame seek support fail construction deterministically with factory diagnostics.
- Valid still known facts always project the still timing contract.
- Metadata token plus explicit `PayloadRejection` reports `Unsupported/PayloadRejection`.
- Metadata token plus legacy unsupported reports `Unsupported/UnsupportedRequest`.
- Existing frame/playback unsupported cause mapping remains unchanged.
- No new complete-timed-facts capability rejection is implemented without a durable authoritative rule.

#### Verification

- `cmake --build build-ninja --target imagesequence_factory imageviewport_provider_metadata imageviewport_provider_terminal_projection imageviewport_provider_terminal_diagnostics`
- `ctest --test-dir build-ninja -R 'imagesequence_factory|imageviewport_provider_metadata|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_diagnostics' --output-on-failure`
- Additional executed coverage: `cmake --build build-ninja --target imagesequence_factory viewportcontroller_provider imageviewport_provider_metadata imageviewport_provider_terminal imageviewport_provider_terminal_diagnostics imageviewport_provider_terminal_projection imageviewport_provider_terminal_recovery imageviewport_provider_terminal_playback && ctest --test-dir build-ninja -R 'imagesequence_factory|viewportcontroller_provider|imageviewport_provider_metadata|imageviewport_provider_terminal|imageviewport_provider_terminal_diagnostics|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_provider_terminal_playback' --output-on-failure`

#### Risks / notes

- Runtime metadata contradiction handling must remain separate from construction-time fact contradiction handling.
- Preserve cause-less result defaults exactly as specified so existing providers using legacy APIs do not change behavior unexpectedly.

### Milestone 4: Introduce a Deferred Provider Scheduler Seam

#### Objective

Move queued provider flush scheduling behind an explicit scheduler effect before refactoring provider dispatch, so the new role-parametric dispatch path does not bake in raw Qt event-loop assumptions.

#### Source findings

F7.

#### Scope

- Keep controller queue state as the single source of queued target identity.
- Replace direct `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` scheduling with a typed deferred-controller-event scheduler effect.
- Own the seam at the item/provider transport boundary: controller returns the effect, `ImageViewportPrivate` applies it, production schedules through Qt queued delivery, and tests can install a synchronous scheduler through provider test support.
- Preserve real event-loop and affinity tests for behavior that genuinely depends on Qt queued delivery.

#### Out of scope

- Do not change provider role dispatch semantics beyond routing existing queue-flush scheduling through the seam.
- Do not remove event-loop tests that validate QObject affinity, queued result delivery, or scene graph synchronization.
- Do not let transport own token matching, queue state, or failure-scope projection.

#### Likely affected areas

- `src/imageviewportprovider.cpp`
- `src/viewportcontrollerprovider.cpp`
- `src/viewportcontroller_p.h`
- `src/viewportproviderbridge_p.h`
- `src/viewportproviderbridge.cpp`
- `src/imageviewport_testhooks_p.h`
- `tests/imageviewport_provider_counting_test_support.h`
- `tests/imageviewport_provider_synchronous_test_support.h`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`

#### Tasks

- [ ] Identify tests that call `drainQueuedProviderResults()` only to flush queued provider dispatch, starting with `ImageViewportProviderRequestsTest::providerFrameSeekQueuesBehindActiveFrameRequest`.
- [ ] Define a typed provider queue-flush scheduler effect in the controller transport-effect model rather than a direct `QMetaObject::invokeMethod` call.
- [ ] Implement a Qt queued production scheduler in `ImageViewportPrivate::applyProviderFrameTransportEffect(...)` or the narrow provider transport effect application path.
- [ ] Expose a private test hook or provider test-support path that installs synchronous provider queue-flush scheduling for tests that do not assert Qt event-loop or affinity behavior.
- [ ] Convert queue-dispatch tests such as `providerFrameSeekQueuesBehindActiveFrameRequest` to use synchronous scheduler support instead of draining global `MetaCall` events solely to leave `RequestQueued`.
- [ ] Keep lifecycle and affinity tests using real queued delivery where they assert entry-point queueing, QObject affinity, or cleanup ordering.
- [ ] Add preservation characterization for failed cleanup/cancel delivery before editing transport behavior: public request/display status, request/display revisions, playback phase, retained display, and error string must not change solely because best-effort cleanup delivery failed.

#### Acceptance criteria

- Provider queue-flush tests can advance queued request dispatch without pumping global `MetaCall` events just to leave `RequestQueued`.
- Production still uses queued delivery for reentrancy-sensitive provider flushes.
- Controller queue state remains authoritative for queued target identity and stale-result rejection.
- Existing provider affinity and lifecycle tests still cover real Qt queued/thread behavior.
- Failed cleanup/cancel delivery preservation tests exist before structured diagnostics are added in a later milestone.

#### Verification

- `cmake --build build-ninja --target imageviewport_provider_requests imageviewport_provider_lifecycle viewportcontroller_provider`
- `ctest --test-dir build-ninja -R 'imageviewport_provider_requests|imageviewport_provider_lifecycle|viewportcontroller_provider' --output-on-failure`

#### Risks / notes

- Do not substitute synchronous scheduling globally. Use it only in deterministic tests where event-loop ordering is not the behavior under test.
- Ensure scheduler callbacks cannot revive stale provider work after clear, replacement, or generation close.

### Milestone 5: Make Provider Frame Dispatch Role-Parametric

#### Objective

Replace primary-specialized provider frame dispatch and secondary direct-start paths with one controller-owned, role-parameterized dispatch boundary.

#### Source findings

F6, F7.

#### Scope

- Add role-local accessors or role-scoped state wrappers around existing primary and secondary provider/request fields.
- Introduce one provider frame dispatch path that accepts `PageRole` and owns active-token checks, queueing, cancellation intent, token allocation, token exhaustion, and stale-deferred validation.
- Convert secondary explicit seek, position seek, playback, metadata-bound target selection, and playback end-of-sequence paths to the shared dispatch boundary.
- Strengthen secondary token matching so active request identity and provider token identity are linked consistently with primary behavior.

#### Out of scope

- Do not rewrite all role state into a final `RoleState` structure in one step if accessors can make the dispatch path shared safely.
- Do not change public spread aggregation or primary-only compatibility aliases.
- Do not fold all provider terminal/progress/lifecycle role-symmetry work into this milestone; Milestone 6 closes the full result-channel path.

#### Likely affected areas

- `src/imageviewportstate_p.h`
- `src/viewportcontroller_p.h`
- `src/viewportcontrollerhelpers_p.h`
- `src/viewportcontrollerprovider.cpp`
- `src/viewportcontrollerplayback.cpp`
- `src/viewportcontrollermetadata.cpp`
- `tests/tst_imageviewport_public_api_provider_roles.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [ ] Add focused tests for secondary provider supersession while a previous secondary frame token is active.
- [ ] Add tests for secondary provider playback and seek paths using the same stale-result behavior as primary.
- [ ] Introduce role-local provider state accessors around current fields before moving call sites.
- [ ] Implement `dispatchProviderFrameRequest(PageRole, ...)` or equivalent role-parametric boundary.
- [ ] Move secondary direct-start call sites to the shared dispatch path.
- [ ] Ensure token exhaustion, cancellation intent, queued target identity, and stale deferred-work validation address the correct role.
- [ ] Remove direct secondary active-token overwrite paths after tests cover the replacement behavior.
- [ ] Add structural verification that no remaining secondary frame-start call site bypasses the shared dispatch boundary.

#### Acceptance criteria

- Primary and secondary provider frame requests use the same dispatch/queue helper.
- Secondary provider frame requests cannot overwrite an active token without a controller-owned queue, cancellation, or stale-deferred decision.
- Role-local display-request state records the active provider token identity used for stale-result matching.
- Secondary supersession, playback, explicit seek, position seek, and metadata-bound target selection are covered by tests.
- `rg -n 'startSecondaryProviderFrameRequest|handleSecondaryProviderPlaybackEndOfSequence|handleSecondaryProviderMetadataTargetPolicy' src/viewportcontrollerprovider.cpp src/viewportcontrollerplayback.cpp` finds no bypassing implementation; any remaining name must be a trivial wrapper that delegates immediately to the role-parametric path and is scheduled for removal in Milestone 6 or 13.

#### Verification

- `cmake --build build-ninja --target imageviewport_public_api_provider_roles imageviewport_provider_requests imageviewport_provider_playback viewportcontroller_provider`
- `ctest --test-dir build-ninja -R 'imageviewport_public_api_provider_roles|imageviewport_provider_requests|imageviewport_provider_playback|viewportcontroller_provider' --output-on-failure`
- `rg -n 'startSecondaryProviderFrameRequest|handleSecondaryProviderPlaybackEndOfSequence|handleSecondaryProviderMetadataTargetPolicy' src/viewportcontrollerprovider.cpp src/viewportcontrollerplayback.cpp`

#### Risks / notes

- This milestone changes internal request dispatch shape, not caller-visible role semantics except where current secondary behavior violates the provider protocol.
- Keep role accessors small and mechanical so later role-state consolidation remains possible without a broad rewrite.

### Milestone 6: Close Provider Result and Lifecycle Role Symmetry

#### Objective

Verify and close the remaining provider role-symmetry gaps after frame dispatch is shared, covering the full provider result and lifecycle channel required by the provider protocol.

#### Source findings

F6.

#### Scope

- Ensure primary and secondary metadata, frame admission, progress/waiting, terminal results, cancellation, close, payload release, lifecycle cleanup, and render-facing acknowledgement paths use the same result-channel semantics.
- Convert remaining secondary-specific handlers to role-parameterized implementations where they own policy rather than trivial projection.
- Keep spread aggregation separate from role-local result handling.

#### Out of scope

- Do not change frame dispatch queue semantics already handled in Milestone 5 except to remove transitional wrappers.
- Do not collapse public primary-only aliases or spread aggregate status projection.
- Do not use this milestone for sequence ownership or page-set transaction cleanup.

#### Likely affected areas

- `src/imageviewportprovider.cpp`
- `src/viewportcontrollerprovider.cpp`
- `src/viewportcontrollerhelpers_p.h`
- `src/viewportcontrollerrender.cpp`
- `src/imageviewportstate_p.h`
- `tests/tst_imageviewport_provider_metadata.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`
- `tests/tst_imageviewport_provider_terminal_projection.cpp`
- `tests/tst_imageviewport_provider_terminal_recovery.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_frame_admission.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [ ] Add or confirm primary/secondary data-driven tests for metadata projection, terminal projection, frame admission, progress/waiting, provider cancellation, close, and render failure acknowledgement.
- [ ] Convert secondary metadata ready/admission/target-policy handlers to shared role-parametric paths where they contain policy rather than presentation-specific aggregation.
- [ ] Convert secondary frame admission and terminal handling to shared role-parametric paths while keeping target-spread aggregate projection owned by display/request state.
- [ ] Confirm payload release and stale frame-ready handling are identical for primary and secondary roles.
- [ ] Confirm cancellation and close effects identify the addressed role and do not use reduced secondary cleanup semantics.
- [ ] Remove or reduce secondary-specific provider methods such as `handleSecondaryProviderFrameAdmission`, `handleSecondaryProviderMetadataAdmission`, and `acceptSecondaryProviderMetadataEvent` when their logic can be represented by role-parametric helpers.
- [ ] Add structural checks for remaining `Secondary`-named provider methods; any remaining method must be a public compatibility wrapper, test helper, or spread-aggregation projection with documented ownership.

#### Acceptance criteria

- Provider metadata, frame admission, waiting/progress, terminal, cancellation, close, payload release, and render-facing acknowledgement behavior is covered for primary and secondary roles.
- Secondary provider sessions accept the same result classes as primary sessions before spread aggregation.
- Remaining secondary-specific provider/controller methods are either removed or justified as projection/compatibility wrappers rather than independent policy paths.
- `rg -n 'handleSecondaryProvider|acceptSecondaryProvider|secondaryProvider' src/viewportcontrollerprovider.cpp src/imageviewportprovider.cpp src/viewportcontrollerplayback.cpp` has no unexplained role-policy implementations outside role-parametric paths.

#### Verification

- `cmake --build build-ninja --target imageviewport_provider_metadata imageviewport_provider_terminal imageviewport_provider_terminal_projection imageviewport_provider_terminal_recovery imageviewport_provider_lifecycle imageviewport_provider_frame_admission imageviewport_render_commit viewportcontroller_provider`
- `ctest --test-dir build-ninja -R 'imageviewport_provider_metadata|imageviewport_provider_terminal|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_recovery|imageviewport_provider_lifecycle|imageviewport_provider_frame_admission|imageviewport_render_commit|viewportcontroller_provider' --output-on-failure`
- `rg -n 'handleSecondaryProvider|acceptSecondaryProvider|startSecondaryProvider|secondaryProvider' src/viewportcontrollerprovider.cpp src/imageviewportprovider.cpp src/viewportcontrollerplayback.cpp`

#### Risks / notes

- Some secondary names may remain for QObject-facing bridge entry points or public projection helpers. The closure criterion is that they do not own a reduced provider policy.
- Keep aggregate spread failure precedence and ready commit separate from role-local result handling.

### Milestone 7: Make Page-Set Display Transition a Controller-Owned Single Source

#### Objective

Remove duplicate retain/clear transition intent before introducing the sequence-source boundary needed to finish role fact ownership.

#### Source findings

F9.

#### Scope

- Remove `retainPreviousDisplay` or equivalent duplicate display-transition state from controller assignment input.
- Make `PageSetTransitionPolicy` and its normalized controller representation the only source of retain-versus-clear behavior.
- Preserve current item-side sequence fact access temporarily if needed; Milestones 8 and 9 replace that boundary.

#### Out of scope

- Do not move all role fact derivation out of `ImageViewportPrivate::setPageSet(...)` yet.
- Do not retire the global sequence owner registry in this milestone.
- Do not alter public page-set semantics, retained display behavior, or clear-style handling beyond removing duplicate internal state.

#### Likely affected areas

- `src/imageviewport.cpp`
- `src/viewportcontroller_p.h`
- `src/viewportcontroller.cpp`
- `src/viewportcontrollerhelpers_p.h`
- `tests/tst_imageviewport_public_api_commands.cpp`
- `tests/tst_imageviewport_provider_terminal_recovery.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_viewportcontroller_presentation.cpp`

#### Tasks

- [ ] Add tests that `RetainPrevious` keeps retained display while the accepted replacement loads or fails.
- [ ] Add tests that `ClearBeforeLoad` exposes empty display before replacement load and after terminal failure until content commits.
- [ ] Add tests that valid clear-style operations preserve presentation preferences such as fit mode, zoom, content position, rotation, mirror, spread direction, page gap, background, quality preferences, and looping.
- [ ] Add tests that invalid transition-policy or page-role values preserve accepted page set, request/display revisions, playback phase, retained content, provider work, and publish command diagnostics where specified.
- [ ] Remove `ViewportSequenceAssignment::retainPreviousDisplay` or equivalent duplicate transition state.
- [ ] Derive retain/clear behavior inside `ViewportController::assignSequence(...)` from normalized transition policy.
- [ ] Keep invalid transition-policy rejection before page-set, presentation, retained-display, provider, playback, or revision changes.

#### Acceptance criteria

- `ViewportSequenceAssignment` does not carry both full transition policy and a derived retain/clear boolean.
- Retain/clear behavior is decided once inside the controller transaction.
- `RetainPrevious` keeps retained display while the new accepted request loads or fails.
- `ClearBeforeLoad` reports empty display before load and after terminal failure until the replacement commits.
- Valid clear-style operations preserve presentation preferences while clearing accepted request/display state.
- Invalid policy/page-role values preserve accepted page set, request/display revisions, playback phase, retained content, provider work, and publish command diagnostics according to the spec.

#### Verification

- `cmake --build build-ninja --target imageviewport_public_api_commands imageviewport_provider_terminal_recovery imageviewport_provider_requests viewportcontroller_presentation`
- `ctest --test-dir build-ninja -R 'imageviewport_public_api_commands|imageviewport_provider_terminal_recovery|imageviewport_provider_requests|viewportcontroller_presentation' --output-on-failure`

#### Risks / notes

- Retained display and accepted request readiness are distinct public observations. Tests must assert both.
- Avoid creating transitional sequence fact access that Milestone 9 immediately removes.

### Milestone 8: Introduce a Sequence-Source Handle

#### Objective

Introduce the internal sequence-source ownership type without trying to retire every old assignment path in one step.

#### Source findings

F8.

#### Scope

- Define a minimal internal sequence-source handle or value snapshot that carries the strong owner plus immutable/provider construction facts needed by accepted role generations.
- Adapt factory results and item assignment enough to create and pass the sequence-source handle alongside current paths.
- Add tests around provider-backed sequence lifetime and explicit sequence-source fixtures where feasible.

#### Out of scope

- Do not remove the global owner registry yet.
- Do not narrow the entire controller context yet.
- Do not change public `ImageSequence` construction helpers or `ImageSequenceFactoryResult` fields.

#### Likely affected areas

- `src/imagesequencefactory.cpp`
- `src/imagesequence_p.h`
- `src/imageviewport.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportstate_p.h`
- `tests/tst_imagesequence_factory.cpp`
- `tests/tst_imageviewport_provider_contract.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [ ] Add or confirm tests proving provider-backed sequences survive adapter QObject destruction after successful construction.
- [ ] Add controller-level tests that can provide explicit sequence-source fixtures without requiring the full item context, if current seams permit.
- [ ] Introduce an internal sequence-source type that carries the strong owner plus immutable/provider construction facts needed by assignment.
- [ ] Adapt factory result creation and item assignment to construct the sequence-source handle without changing public API.
- [ ] Keep old owner lookup in place only as a transitional fallback, and mark every remaining call site for removal in Milestone 9.

#### Acceptance criteria

- A minimal internal sequence-source handle exists and can carry provider-backed lifetime plus construction facts.
- Public sequence construction and assignment APIs remain unchanged.
- Provider-backed sequence lifetime remains stable after adapter QObject destruction.
- The plan for removing each remaining global owner lookup call site is explicit in code comments or Milestone 9 tasks, not implicit.

#### Verification

- `cmake --build build-ninja --target imagesequence_factory imageviewport_provider_contract imageviewport_provider_lifecycle viewportcontroller_provider`
- `ctest --test-dir build-ninja -R 'imagesequence_factory|imageviewport_provider_contract|imageviewport_provider_lifecycle|viewportcontroller_provider' --output-on-failure`

#### Risks / notes

- Keep the first source handle minimal and internal. Avoid redesigning all sequence data structures at once.
- Do not make caller source identity part of the sequence-source contract.

### Milestone 9: Route Page-Set Role Facts Through Sequence Source and Remove Global Owner Lookup

#### Objective

Finish the sequence ownership boundary by routing page-set role fact derivation through sequence source and retiring process-global raw-pointer owner recovery.

#### Source findings

F8, F9.

#### Scope

- Move `ImageSequencePrivateAccess` reads for controller assignment behind the sequence-source boundary.
- Store one coherent sequence source per accepted role generation in request/controller state.
- Reduce `ImageViewportPrivate::setPageSet(...)` to public input normalization, source handle transfer, controller invocation, and transport-effect application.
- Remove the global `ImageSequence* -> weak_ptr` owner registry once assignment no longer needs it.
- Narrow or delete controller context getters that exist only to read sequence facts through `ImageViewportPrivate`.

#### Out of scope

- Do not change public `ImageSequence` construction helpers or `ImageSequenceFactoryResult` fields.
- Do not expose sequence source identity, provider internals, or source identity comparison as public API.
- Do not change retain/clear behavior already handled in Milestone 7.

#### Likely affected areas

- `src/imagesequenceownership.cpp`
- `src/imagesequenceownership_p.h`
- `src/imagesequencefactory.cpp`
- `src/imageviewport.cpp`
- `src/imageviewportcontroller.cpp`
- `src/imageviewportprovider.cpp`
- `src/viewportcontroller_p.h`
- `src/imageviewportstate_p.h`
- `tests/tst_imagesequence_factory.cpp`
- `tests/tst_imageviewport_provider_contract.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [ ] Pass sequence-source handles into `ViewportSequenceAssignment` or equivalent controller assignment input for both roles.
- [ ] Move assignment-time role fact extraction from `ImageViewportPrivate::setPageSet(...)` to the controller/sequence-source boundary.
- [ ] Update role request state to store one sequence-source owner per accepted role generation.
- [ ] Remove controller context getters that only proxy sequence facts or payloads from `ImageViewportPrivate`.
- [ ] Delete or retire `imagesequenceownership` registry code after `registerImageSequenceOwner(...)` and `lookupImageSequenceOwner(...)` have no assignment path users.
- [ ] Add structural verification that no viewport assignment path calls global owner lookup or item-side sequence fact getters.

#### Acceptance criteria

- No process-global sequence owner registry is required for viewport assignment.
- Accepted role state stores one coherent sequence source per role generation.
- `ImageViewportPrivate` no longer acts as a broad service locator for sequence fact and payload queries needed only by the controller.
- `ImageViewportPrivate::setPageSet(...)` no longer calls `ImageSequencePrivateAccess` for role classification.
- Public sequence construction and assignment APIs remain unchanged.
- `rg -n 'registerImageSequenceOwner|lookupImageSequenceOwner|ImageSequenceOwnership|imagesequenceownership' src` shows no runtime assignment usage; if files remain, they are unused transitional files scheduled for deletion before Milestone 13.

#### Verification

- `cmake --build build-ninja --target imagesequence_factory imageviewport_provider_contract imageviewport_provider_lifecycle viewportcontroller_provider`
- `ctest --test-dir build-ninja -R 'imagesequence_factory|imageviewport_provider_contract|imageviewport_provider_lifecycle|viewportcontroller_provider' --output-on-failure`
- `rg -n 'registerImageSequenceOwner|lookupImageSequenceOwner|ImageSequenceOwnership|imagesequenceownership' src`
- `rg -n 'ImageSequencePrivateAccess' src/imageviewport.cpp src/imageviewportcontroller.cpp src/imageviewportprovider.cpp`

#### Risks / notes

- This remains high coupling even after splitting. Keep changes role-by-role and preserve public assignment behavior after each commit.
- Lifetime tests should cover both built-in sequences and provider-backed sequences.

### Milestone 10: Split Pure Render Planning From Scene Graph Materialization

#### Objective

Make render planning a Qt-free value step and move scene graph preconditions such as missing window to materialization.

#### Source findings

F11.

#### Scope

- Extract a Qt-free render planning input/output path.
- Move missing-window and backend allocation failures to scene graph materialization/preflight.
- Update pure render-plan tests to require only value inputs.

#### Out of scope

- Do not add operational diagnostics for render failures here; Milestone 11 owns cause retention.
- Do not expose scene graph objects, native texture handles, or backend policy through public API.
- Do not change payload ownership or scene graph cleanup semantics.

#### Likely affected areas

- `src/renderadapter_p.h`
- `src/renderadapter.cpp`
- `src/imageviewportrender.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`

#### Tasks

- [ ] Add or update pure render-plan tests so role layer mapping, source/target rectangles, background primitives, quality flags, and payload identities can be verified without constructing `QQuickWindow`.
- [ ] Split render planning input from scene graph materialization input so pure planning does not require `QQuickWindow`, `QSGNode`, `QSGTexture`, or `QSGImageNode`.
- [ ] Move `MissingWindow` classification to materialization/preflight while preserving public render failure projection.
- [ ] Keep scene graph tests for texture creation, image-node creation, ownership cleanup, and materialization failure causes.

#### Acceptance criteria

- Pure render-plan tests do not instantiate `QQuickWindow`.
- `RenderAdapter` pure plan input/header does not include `QQuickWindow*`, `QSGNode*`, `QSGTexture*`, or `QSGImageNode*`.
- Missing-window failures are still reported when materialization is attempted.
- Scene graph materialization tests continue to cover backend allocation failures and cleanup.

#### Verification

- `cmake --build build-ninja --target imageviewport_render_scenegraph imageviewport_render_commit`
- `ctest --test-dir build-ninja -R 'imageviewport_render_scenegraph|imageviewport_render_commit' --output-on-failure`
- `rg -n 'QQuickWindow\\*|QSGNode\\*|QSGTexture\\*|QSGImageNode\\*' src/renderadapter_p.h`

#### Risks / notes

- The implementation file may still contain scene graph types in the materialization half. The structural check targets the pure plan interface.
- Do not reinterpret render failure causes during this milestone beyond moving missing-window ownership.

### Milestone 11: Preserve Render and Provider Cleanup Operational Diagnostics

#### Objective

Retain structured private cause context for active render failures and provider cleanup/cancel delivery failures while keeping public status semantics compatible.

#### Source findings

F12.

#### Scope

- Extend the private test hook surface in `src/imageviewport_testhooks_p.h` or an equivalent private diagnostic probe to expose last accepted render failure diagnostics and provider transport diagnostics under `IMAGEVIEWPORT_PRIVATE_TEST_PROBES`.
- Required render diagnostic fields: role, generation identity, request identity, prepared payload identity, and `RenderFailureCause`.
- Required provider transport diagnostic fields: role, operation kind (`cancel` or `close`), metadata token validity/value when applicable, frame token validity/value when applicable, and whether command delivery was queued.
- Ensure cleanup/cancel delivery failures do not mutate public request/display state, revisions, playback phase, retained display, or public strings except where already documented.

#### Out of scope

- Do not expose private diagnostic fields as public QML or C++ API.
- Do not change public `RequestStatus`, `RequestReason`, `errorString`, display status, or revision semantics for render and cleanup failures.
- Do not introduce new backend selection policy.

#### Likely affected areas

- `src/imageviewport_testhooks_p.h`
- `src/viewportcontroller_p.h`
- `src/viewportcontrollerrender.cpp`
- `src/imageviewportstate_p.h`
- `src/viewportproviderbridge_p.h`
- `src/viewportproviderbridge.cpp`
- `src/imageviewportprovider.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`

#### Tasks

- [ ] Add private test hooks or structured internal diagnostics for last accepted active render failure cause with role, generation, request, prepared payload, and `RenderFailureCause`.
- [ ] Add render tests proving each active render failure cause survives controller acknowledgement and stale render failures do not overwrite active diagnostics.
- [ ] Thread `acknowledgement.failureCause` through controller diagnostics without changing public request projection.
- [ ] Add provider lifecycle tests for failed cancel and close delivery diagnostics, including role, operation, token validity/value, and queued-delivery result.
- [ ] Return and handle structured transport diagnostic results from provider close/cancel paths without mutating public request/display state.
- [ ] Assert cleanup/cancel delivery failure leaves public request status, request reason, display status, error string, request/display revisions, playback phase, and retained display unchanged unless a separate documented event changes them.

#### Acceptance criteria

- Active render failure diagnostics expose role, generation identity, request identity, prepared payload identity, and `RenderFailureCause` through private tests or structured logs.
- Failed provider close/cancel delivery diagnostics expose role, operation kind, safe token identity, and queued-delivery result.
- Stale render failures and stale cleanup/cancel failures do not overwrite active diagnostics.
- Public request status, request reason, `errorString`, display status, display content, playback phase, and revisions remain compatible when cleanup/cancel delivery fails.

#### Verification

- `cmake --build build-ninja --target imageviewport_render_commit imageviewport_provider_lifecycle`
- `ctest --test-dir build-ninja -R 'imageviewport_render_commit|imageviewport_provider_lifecycle' --output-on-failure`

#### Risks / notes

- Keep public diagnostics redacted and bounded. Operational cause context is internal unless a future spec explicitly exposes it.
- Prefer private test probes over parsing log text when deterministic tests need structured fields.

### Milestone 12: Move Mutating Controller Transitions Out of the Shared Helper Header

#### Objective

Clarify controller ownership by moving mutating transition logic from `viewportcontrollerhelpers_p.h` into responsibility-owned controller modules after behavior-sensitive dispatch, transaction, sequence-source, and render boundaries are stable.

#### Source findings

F10.

#### Scope

- Move cohesive helper clusters to owning `.cpp` files without changing behavior.
- Keep the helper header limited to pure predicates, role accessors, small projections, and value helpers.
- Prefer one cluster per commit during execution: provider token/queue helpers, render snapshot helpers, presentation transition helpers, playback/seek helpers, then terminal projection helpers.

#### Out of scope

- Do not rewrite controller state or public behavior as part of moving functions.
- Do not introduce new abstractions unless they remove real duplicated mutation ownership.
- Do not combine this cleanup with P0/P1 behavior fixes that should already be complete.

#### Likely affected areas

- `src/viewportcontrollerhelpers_p.h`
- `src/viewportcontrollerprovider.cpp`
- `src/viewportcontrollerplayback.cpp`
- `src/viewportcontrollerpresentation.cpp`
- `src/viewportcontrollerrender.cpp`
- `src/viewportcontrollermetadata.cpp`
- `src/viewportcontroller.cpp`
- `tests/owner_specific_helper_headers_only.cmake`
- Relevant controller and imageviewport tests for each moved cluster

#### Tasks

- [ ] Inventory mutating functions in `viewportcontrollerhelpers_p.h` and group them by owning controller subsystem.
- [ ] Move provider token, queue, cancellation, and terminal helper logic into provider-owned controller code.
- [ ] Move render snapshot and acknowledgement helper logic into render-owned controller code.
- [ ] Move presentation transition mutation logic into presentation-owned controller code.
- [ ] Move playback stop restoration and explicit seek acceptance helpers into playback/request-owned controller code.
- [ ] Leave only pure utilities, role selectors, and value projections in `viewportcontrollerhelpers_p.h`.
- [ ] Update structural tests or add a structural check that the helper header does not contain broad mutating transition functions.
- [ ] Run the full targeted controller/provider/render test set after each cluster move.

#### Acceptance criteria

- `viewportcontrollerhelpers_p.h` no longer contains functions that mutate `RequestState`, `DisplayState`, `ProviderGenerationState`, `PresentationState`, playback phase, diagnostics, or provider queues except trivial role accessors if unavoidable.
- Each state transition has one owning controller module aligned with its responsibility.
- Public behavior remains unchanged from the preceding milestones.
- Structural checks prevent the shared helper header from growing back into a broad mutation owner.

#### Verification

- `cmake --build build-ninja --target viewportcontroller_playback viewportcontroller_presentation viewportcontroller_provider imageviewport_provider_requests imageviewport_render_commit imageviewport_presentation_state`
- `ctest --test-dir build-ninja -R 'structural::ownerSpecificHelperHeadersOnly|viewportcontroller_playback|viewportcontroller_presentation|viewportcontroller_provider|imageviewport_provider_requests|imageviewport_render_commit|imageviewport_presentation_state' --output-on-failure`
- `rg -n 'RequestState&|DisplayState&|ProviderGenerationState&|PresentationState&|setCommandDiagnostic|recordTargetSpreadTerminal|acceptExplicitSeek|applyPresentationTransition' src/viewportcontrollerhelpers_p.h`

#### Risks / notes

- Move logic only after earlier milestones reduce duplicate state and role asymmetry. Moving a broad helper before ownership is clarified can preserve the same ambiguity in a different file.
- Keep each extraction mechanically reviewable.

### Milestone 13: Integration Sweep and Regression Closure

#### Objective

Validate the complete end state, remove obsolete compatibility scaffolding created during migration, and ensure no milestone left temporary implementation paths behind.

#### Source findings

F1 through F12.

#### Scope

- Run the full test suite.
- Remove obsolete secondary-specific provider methods, old scheduler bypasses, global owner registry files if unused, duplicate transition fields, transitional helper wrappers, and temporary diagnostic scaffolding not intended to remain.
- Verify installed-header, role-symmetry, sequence ownership, render planning, diagnostics, and helper-header structural constraints.
- Update this plan only if an implementation blocker or invalid assumption remains.

#### Out of scope

- Do not add new public features.
- Do not change durable specs or architecture docs unless a concrete contradiction is discovered and handled as a separate intent step.
- Do not broaden into source loading, navigation, cache, decoder, tiled rendering, native texture, or backend-selection policy.

#### Likely affected areas

- All files touched by Milestones 2 through 12.
- `tests/CMakeLists.txt`
- `tests/owner_specific_helper_headers_only.cmake`
- Full CTest suite under `tests/`.

#### Tasks

- [ ] F1 closure: verify installed header forbidden-token tests cover `ImageViewportInternal`, `ImageSequenceData`, private access helper names, and `QSGNode`.
- [ ] F2 closure: verify mirror anchor tests cover both axes, valid anchor preservation, invalid anchor rejection, and no-op current-value behavior.
- [ ] F3 closure: verify invalid page-set argument tests assert command diagnostics and unchanged request/display state.
- [ ] F4 closure: verify still known facts plus declared-false frame seek are rejected and valid still facts project the still contract.
- [ ] F5 closure: verify metadata explicit `PayloadRejection` and legacy unsupported defaults are both tested.
- [ ] F6 closure: verify provider role symmetry tests and structural checks cover dispatch, metadata, frame admission, terminal, progress, cancellation, close, payload release, and render-facing paths.
- [ ] F7 closure: verify provider queue flush scheduling goes through the typed scheduler seam and deterministic tests no longer drain global `MetaCall` events solely for dispatch.
- [ ] F8 closure: verify no assignment path uses global sequence owner lookup and role state stores sequence-source ownership.
- [ ] F9 closure: verify page-set retain/clear decisions are controller-owned and `ImageViewportPrivate::setPageSet(...)` no longer classifies role facts through private sequence access.
- [ ] F10 closure: verify mutating controller transitions no longer live in `viewportcontrollerhelpers_p.h`.
- [ ] F11 closure: verify pure render-plan tests run without `QQuickWindow` and pure plan interfaces exclude scene graph types.
- [ ] F12 closure: verify private diagnostics retain render failure cause and provider cleanup/cancel delivery context without changing public status semantics.
- [ ] Remove temporary compatibility wrappers or duplicated old/new paths after all callers use the new boundaries.
- [ ] Run the full test suite and targeted install-consumer and structural tests.
- [ ] Update this plan's deferred section if any intended cleanup cannot be completed safely.

#### Acceptance criteria

- Full configured test suite passes.
- Every F1 through F12 closure task has an observable test, structural check, or explicit deferred reason.
- Temporary migration paths are removed or explicitly justified.
- The implementation still matches `docs/spec/**` and `docs/architecture/**`.

#### Verification

- `cmake --build build-ninja`
- `ctest --test-dir build-ninja --output-on-failure`
- `ctest --test-dir build-ninja -R 'imageviewport_install_consumer|structural::ownerSpecificHelperHeadersOnly' --output-on-failure`
- `rg -n 'startSecondaryProvider|handleSecondaryProvider|acceptSecondaryProvider|registerImageSequenceOwner|lookupImageSequenceOwner|retainPreviousDisplay|QQuickWindow\\* window' src`

#### Risks / notes

- Full-suite failures may expose unrelated existing instability. Isolate unrelated failures before changing implementation further.
- Do not use integration cleanup as a reason to start new architectural work not covered by this plan.

## Deferred / Needs Investigation

### Complete timed provider facts capability compatibility

The design review noted that complete timed provider facts should have one documented capability-compatibility rule enforced by the same validation helper as still facts. The durable docs identify fixed-duration and timed-frame-list facts as complete timed facts, but they do not state a specific capability declaration that must be rejected at construction time in the same concrete way they state still timing requires frame seek support `True`.

Additional evidence needed: inspect the public provider known-facts API, existing capability projection tests, and durable docs to determine whether complete timed facts imply a specific frame seek, position seek, or timed playback capability rule. If the rule is intended but not authoritative, update durable docs through the normal intent process before implementing it. If the rule is already authoritative in code comments or tests, add it to Milestone 3 before implementation.

## Suggested `/goal` Execution Order

1. Milestone 1: Establish baseline and characterization coverage.
2. Milestone 2: Fix public API and command conformance.
3. Milestone 3: Centralize provider still facts and terminal cause semantics.
4. Milestone 4: Introduce a deferred provider scheduler seam.
5. Milestone 5: Make provider frame dispatch role-parametric.
6. Milestone 6: Close provider result and lifecycle role symmetry.
7. Milestone 7: Make page-set display transition a controller-owned single source.
8. Milestone 8: Introduce a sequence-source handle.
9. Milestone 9: Route page-set role facts through sequence source and remove global owner lookup.
10. Milestone 10: Split pure render planning from scene graph materialization.
11. Milestone 11: Preserve render and provider cleanup operational diagnostics.
12. Milestone 12: Move mutating controller transitions out of the shared helper header.
13. Milestone 13: Integration sweep and regression closure.

## Plan Review Notes

Concerns addressed: the plan now names baseline tests and cleanup/cancel characterization, moves the scheduler seam before provider dispatch refactoring, expands role symmetry beyond frame dispatch, adds structural checks for dispatch and sequence ownership, splits page-set transition work from sequence-source work, splits sequence-source migration into introduction and retirement phases, splits render planning from diagnostics, sharpens page-set acceptance criteria, adds exact diagnostic fields for private render/transport diagnostics, and adds an F1-F12 closure checklist.

Concerns deferred: complete timed provider-facts capability compatibility is not safe to implement from the current durable docs alone, so it is recorded under `Deferred / Needs Investigation`.

Remaining known limitations: verification commands assume `build-ninja` is the active configured build directory, with an explicit Milestone 1 task to confirm or configure that build. Some structural `rg` checks may still show compatibility wrappers during intermediate milestones; each milestone states when wrappers are acceptable and when they must be removed or justified.

## Non-Goals

- Do not implement source code changes while creating this plan.
- Do not use this plan to rewrite the full controller in one step.
- Do not change `docs/spec/**` or `docs/architecture/**` during future execution unless a new authoritative-doc issue is discovered and handled explicitly before implementation continues.
- Do not expose scene graph handles, native textures, provider transport types, render backend policy, or private test probes as public API.
- Do not add `ImageViewport` source loading for files, URLs, archives, directories, byte buffers, or raw provider objects.
- Do not add caller-owned navigation, page selection, secondary fallback policy, source identity comparison, decoder selection, cache policy, predecode policy, route meaning, toolbar/action routing, gestures, tiled loading, region loading, or full color-management policy.
- Do not remove public primary-role compatibility observations while making internal role state symmetric.
- Do not remove Qt event-loop or affinity tests where queued delivery or QObject affinity is the behavior under test.
