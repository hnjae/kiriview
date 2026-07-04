# Implementation Plan

## Purpose

This plan consolidates the design review findings into executable implementation work for `/goal`. It turns the correct end-state architecture into incremental, verifiable milestones that future implementation sessions can execute without consulting any temporary review artifact.

## Source Basis

- `docs/spec/**`
- `docs/architecture/**`

`DESIGN_REVIEW_CORRECT_END_STATE.md` was used as a temporary planning input, fully consolidated into this plan, and is no longer required for future execution. Future implementation sessions must use this plan plus the durable source basis above; they must not depend on the deleted design review document.

No temporary design review finding was overridden by the authoritative docs. The durable docs already describe the intended user-visible and architectural end state for the findings below, so no `docs/spec/**` or `docs/architecture/**` updates are required before implementation starts.

## Execution Principles

- Preserve current behavior unless fixing a documented bug, a spec violation, or an implementation conflict with `docs/architecture/**`.
- Keep each milestone green when it finishes. Do not commit a standalone red-test milestone unless the current `/goal` explicitly requests red-green commits.
- Add contract tests before changing risky behavior inside the milestone that fixes that behavior, then make those tests pass in the same milestone.
- Keep changes incremental; split a milestone if the implementation starts spanning unrelated ownership boundaries.
- Treat `docs/spec/**` and `docs/architecture/**` as authoritative and immutable unless implementation uncovers a real ambiguity or blocker. If that happens, update this plan before changing durable docs.
- Isolate domain logic from external effects such as Qt scene graph state, provider callbacks, filesystem-backed sequence loading, clocks, timers, and app bootstrapping.
- Prefer central domain decisions over repeated call-site rules; one concept should have one owning module.
- Preserve public APIs unless changing them is explicitly required by the durable end-state docs.
- Update this plan if implementation discovers a blocker, an invalid assumption, or a safer milestone split.

## Consolidated Design Findings

### F1: Public page-geometry value type is missing

- Priority: P1
- Affected areas: `src/imageviewport.h`, `src/imageviewport.cpp`, `src/imageviewport_p.h`, public/QML type registration, `src/CMakeLists.txt`, `src/generate_installed_public_header.cmake`, static plugin/QML metadata, `tests/install_consumer/*`, public API and QML tests.
- Correct end state: The public API exposes the typed page geometry value described by `docs/spec/image-viewport-api.md` and `docs/architecture/subsystem-boundaries.md`: page role, page rect in spread coordinates, item rect, visible page rect, and availability cross the QML/C++ boundary as a typed value. Existing compatibility rectangle properties remain unless the spec explicitly removes them.
- Why it matters: The implementation cannot satisfy the documented public contract while page geometry is only available through older or less precise rectangle accessors.

### F2: Direction and gap command diagnostics violate preservation rules

- Priority: P1
- Affected areas: `src/viewportcontrollerpresentation.cpp`, `src/imageviewport.cpp`, command/property wrappers, `tests/tst_imageviewport_public_api_commands.cpp`, presentation state tests.
- Correct end state: Invalid `setSpreadDirection` and `setPageGap` requests, and current-value no-ops, preserve state, diagnostics, and revision tokens as documented. Only valid changed values update presentation state and diagnostics according to the command outcome contract.
- Why it matters: Callers rely on command outcomes and revision tokens for deterministic state observation. Clearing diagnostics on rejected or no-op commands hides existing failure state and violates the durable API contract.

### F3: Secondary provider seeks do not share the retained/empty display fallback path

- Priority: P1
- Affected areas: `src/viewportcontrollerplayback.cpp`, `src/viewportcontrollerprovider.cpp`, provider request tests, provider playback tests, display status/revision tests.
- Correct end state: Primary and secondary provider seek targets use the same role-aware request acceptance path. `seek(Secondary, frame)` and `seekToPosition(Secondary, milliseconds)` publish `requestStatus: Loading` and either `displayStatus: Retained` or `displayStatus: Empty` until both roles needed for the spread have committed; `DisplayStatus::Ready` appears only after the complete spread commit.
- Why it matters: Two-page spread readiness and accepted request identity are documented independently from which provider role is involved. Divergent secondary handling can expose stale or inconsistent display state.

### F4: Provider end-of-sequence bypasses the shared authored loop policy

- Priority: P1
- Affected areas: `src/viewportcontrollerprovider.cpp`, `src/viewportcontrollerplayback.cpp`, shared playback helpers, provider terminal playback tests.
- Correct end state: Provider end-of-sequence handling uses the same playback-domain target selector and effective loop policy as controller playback. Finite provider sequences stop at authored end, infinite sequences loop only when documented loop policy allows it, primary/secondary provider roles share the same policy owner, and no beyond-final public requested target is exposed.
- Why it matters: Loop behavior is a domain rule, not a provider callback detail. Duplicated terminal handling can make provider playback diverge from authored sequence playback.

### F5: Built-in frame payloads cross controller/render boundaries as `QImage`

- Priority: P1
- Affected areas: `src/viewportcontroller_p.h`, `src/imageviewportcontroller.cpp`, `src/imagesequencesource.cpp`, `src/viewportcontroller.cpp`, `src/viewportcontrollerrender.cpp`, `src/framepreparation_p.h`, `src/framepreparation.cpp`, still/timed/render tests.
- Correct end state: Built-in and provider-backed frames both pass through the same `PreparedPayload` preparation/admission boundary before render. Controller/render code no longer exposes or calls `sequenceFrameImage(...)`, `secondarySequenceFrameImage(...)`, `sourceFrameImage(...)`, or `ImageSequencePrivateAccess::frameImage(...)` across subsystem boundaries.
- Why it matters: The architecture requires payload storage to stay behind the sequence boundary and prepared payloads to cross into rendering. Direct `QImage` access spreads ownership across controller and render layers, making invalidation, resource lifetime, and test isolation harder.

### F6: Provider event admission and stale filtering are split before the controller boundary

- Priority: P2
- Affected areas: `src/imageviewportprovider.cpp`, `src/viewportproviderbridge.cpp`, `src/viewportcontrollerprovider.cpp`, provider request/frame-admission tests.
- Correct end state: Provider results enter the controller through one normalized event API that owns session-token validation, stale result filtering, provider payload release, terminal-cause validation hooks, and returned side effects. Bridge and provider wrapper code transport events; they do not duplicate admission rules.
- Why it matters: When admission is split across layers, it is difficult to prove that stale frames, wrong-session callbacks, terminal causes, and payload ownership are handled consistently.

### F7: Provider effects can run before assignment changes publish

- Priority: P2
- Affected areas: `src/imageviewport.cpp`, `src/imageviewportprovider.cpp`, provider lifecycle tests, provider terminal/recovery tests.
- Correct end state: Provider assignment changes publish their intended public state before provider-dispatch side effects can synchronously fail and publish follow-up state. State mutation, signal emission, and effect execution have a documented order.
- Why it matters: Synchronous provider failures can create surprising signal order and make public state hard to observe reliably.

### F8: Provider result delivery lacks a deterministic non-event-loop test seam

- Priority: P2
- Affected areas: `src/viewportproviderbridge.cpp`, provider bridge test support, `tests/imageviewport_provider_counting_test_support.h`, provider request and lifecycle tests.
- Correct end state: Provider result delivery has a small deterministic seam for protocol tests that can capture normalized events and drive callback scheduling without wall-clock waits or full Qt event-loop timing. Integration tests that intentionally verify queued delivery, callback affinity, thread cleanup, or scene graph synchronization remain event-loop based.
- Why it matters: Core admission and ordering rules should be testable without brittle integration timing or excessive mocking.

### F9: Render failure can destroy retained pixels before publishing retained fallback

- Priority: P1
- Affected areas: `src/renderadapter.cpp`, `src/imageviewportrender.cpp`, `src/viewportcontrollerrender.cpp`, render commit and scene graph tests.
- Correct end state: Render commit failure for the active request reports `requestStatus: Error` and `requestReason: RenderFailure`. `displayStatus` remains `Retained` only when retained pixels are preserved and the render path does not return `nullptr`; otherwise `displayStatus` is `Empty` when no retained content is valid or clear-before-load applies.
- Why it matters: The spec promises retained display content as a visual fallback. Reporting retained state after old pixels are already destroyed creates a correctness and production debugging risk.

### F10: Provider close can drop session ownership on cleanup scheduling failure

- Priority: P2
- Affected areas: `src/viewportproviderbridge.cpp`, provider lifecycle tests.
- Correct end state: Provider cleanup ownership is exception-safe and failure-safe. If cleanup scheduling fails, ownership is either still held by an RAII cleanup object or restored to a state that can be retried safely; repeated close is idempotent and late callbacks from the failed-close session cannot mutate public state.
- Why it matters: Dropping ownership during cleanup failure risks leaks, double cleanup assumptions, or unrecoverable provider session state.

### F11: Invalid provider unsupported causes are accepted as ordinary unsupported outcomes

- Priority: P2
- Affected areas: `src/viewportcontrollerprovider.cpp`, `src/imageviewport.h`, provider terminal projection and diagnostics tests.
- Correct end state: Invalid or unknown provider unsupported causes after token validation are provider protocol violations. They report `RequestStatus::Error` with `RequestReason::PayloadRejection`, include bounded protocol-violation diagnostics, and close provider interest. Valid unsupported-operation and unsupported-payload causes remain normal unsupported outcomes.
- Why it matters: Treating unknown causes as ordinary unsupported data hides provider contract violations and prevents callers from distinguishing expected unsupported media from malformed provider behavior.

## Milestones

### Milestone 1: Baseline Verification And Contract-Test Matrix

#### Objective

Establish a green verification baseline and use the matrix below to guide red/green tests in the owning implementation milestones.

#### Source findings

F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11.

#### Scope

- Confirm the local build/test commands and focused CTest targets.
- Run the current suite before source changes so later failures can be attributed to milestone work.
- Add only passing characterization tests or harmless test support if they document behavior that is already correct.
- Leave intended-contract tests that fail on the current implementation to the milestone that implements the fix.

#### Out of scope

- No production code changes.
- No public API shape changes.
- No standalone committed red tests unless the current `/goal` explicitly requests red-green commits.

#### Likely affected areas

- `justfile`
- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- Existing tests listed in the contract-test matrix, only when adding passing characterization coverage.

#### Contract-test matrix

| Finding | Proposed test location and name | Initial expectation | Final expectation | Focused verification |
| --- | --- | --- | --- | --- |
| F1 | `tests/tst_imageviewport_public_api.cpp::pageGeometryValueTypeFields`, `tests/tst_imageviewport_public_api_qml.cpp::pageGeometryQmlValueType`, `tests/install_consumer/*::usesInstalledPageGeometry` | New contract tests fail until the type is implemented. | C++ callers, QML callers, and installed consumers observe typed page geometry. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_public_api|imageviewport_public_api_qml|imageviewport_install_consumer'` |
| F2 | `tests/tst_imageviewport_public_api_commands.cpp::invalidPresentationCommandsPreserveDiagnostics` | New contract test fails if invalid/no-op commands clear diagnostics or advance revisions. | Invalid/no-op direction and gap commands preserve diagnostics and revision tokens. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_public_api_commands|viewportcontroller_presentation'` |
| F3 | `tests/tst_imageviewport_provider_requests.cpp::secondarySeekPublishesFallbackLikePrimary`, `tests/tst_imageviewport_provider_playback.cpp::secondaryPositionSeekPublishesFallbackLikePrimary` | New contract tests fail where secondary requests bypass fallback publication. | Secondary frame and position seeks publish `Loading` plus retained/empty fallback until complete spread commit. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_requests|imageviewport_provider_playback|viewportcontroller_provider'` |
| F4 | `tests/tst_imageviewport_provider_terminal_playback.cpp::providerEosUsesAuthoredLoopPolicy` | New contract tests fail where provider EOS uses provider-specific looping. | Finite and infinite provider EOS follow authored loop policy and avoid beyond-final public targets. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_terminal_playback|viewportcontroller_playback'` |
| F5 | `tests/tst_imageviewport_still.cpp::builtInStillUsesPreparedPayload`, `tests/tst_imageviewport_timed.cpp::timedFrameUsesPreparedPayload`, static boundary search | Static search initially finds forbidden controller/render `QImage` crossings. | Built-in and provider paths hand render code `PreparedPayload` through one boundary. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_still|imageviewport_timed|imageviewport_render_commit|viewportcontroller_playback'` plus the `rg` command in Milestone 3. |
| F6 | `tests/tst_imageviewport_provider_frame_admission.cpp::staleEventsAreRejectedByControllerAdmission` | New contract tests fail where bridge/item code owns stale filtering or payload release. | Controller admission owns session validation, stale handling, classification, and release decisions. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_frame_admission|imageviewport_provider_requests|viewportcontroller_provider'` |
| F7 | `tests/tst_imageviewport_provider_lifecycle.cpp::assignmentPublishesBeforeDispatchFailure` | New contract test fails if synchronous dispatch failure publishes before assignment state. | Assignment notifications precede forced dispatch failure state. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_lifecycle|imageviewport_provider_terminal_recovery'` |
| F8 | `tests/imageviewport_provider_counting_test_support.h` seam coverage and protocol tests that do not call arbitrary waits | Seam tests fail until deterministic delivery exists. | Protocol tests drive normalized events without arbitrary waits; affinity/thread tests remain event-loop based. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_requests|imageviewport_provider_lifecycle|imageviewport_provider_frame_admission'` |
| F9 | `tests/tst_imageviewport_render_commit.cpp::retainedRenderFailureReportsErrorAndKeepsPixels`, `tests/tst_imageviewport_render_scenegraph.cpp::retainedFailureDoesNotReturnNullNode` | New contract tests fail if retained status is reported after pixels are destroyed. | Active request reports `Error/RenderFailure`; retained status is used only with retained pixels. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_render_commit|imageviewport_render_scenegraph'` |
| F10 | `tests/tst_imageviewport_provider_lifecycle.cpp::cleanupSchedulingFailureKeepsOwnership` | New contract test fails if ownership is dropped on cleanup scheduling failure. | No leak/orphan, repeated close is idempotent, and late callbacks cannot mutate public state. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_lifecycle'` |
| F11 | `tests/tst_imageviewport_provider_terminal_projection.cpp::invalidUnsupportedCauseIsPayloadRejection`, `tests/tst_imageviewport_provider_terminal_diagnostics.cpp::invalidCauseDiagnosticIsBounded` | New contract tests fail if invalid causes project as ordinary unsupported. | Invalid metadata, frame, and playback causes report `Error/PayloadRejection`, bounded diagnostics, and closed provider interest. | `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_terminal_projection|imageviewport_provider_terminal_diagnostics|imageviewport_provider_terminal'` |

#### Tasks

- [ ] Confirm the current verification commands by inspecting `justfile`, `CMakeLists.txt`, and `tests/CMakeLists.txt`.
- [ ] Run `just test` before implementation changes.
- [ ] If the baseline fails, stop and record the failing targets in this plan before beginning architectural work.
- [ ] Confirm that each focused CTest target in the matrix exists or update the target name before adding tests.
- [ ] Add only green characterization tests in this milestone; move any red intended-contract test into its owning implementation milestone.

#### Acceptance criteria

- `just test` is green before implementation work starts, or this plan records the baseline failure as a blocker.
- The contract-test matrix maps every consolidated finding to a test location, expected initial result, expected final result, and focused verification command.
- No production source files are changed in this milestone.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -N`

#### Risks / notes

- The proposed test names are implementation guidance. Future sessions may choose equivalent names, but they must preserve the same contract coverage.

### Milestone 2: Public API Conformance For Page Geometry And Command Outcomes

#### Objective

Bring documented public page geometry and command outcome behavior into conformance with the durable API contract.

#### Source findings

F1, F2.

#### Scope

- Add the public typed page-geometry value required by the docs.
- Register the type for QML/C++ boundary use according to existing project conventions.
- Expose the type through installed headers, QML module metadata, and install-consumer coverage.
- Preserve existing compatibility properties unless the spec explicitly removes them.
- Fix invalid and no-op direction/gap commands so they preserve diagnostics and revision tokens.

#### Out of scope

- No broad public API redesign.
- No changes to provider playback, render payload preparation, or provider lifecycle behavior.
- No removal of compatibility rectangle properties.

#### Likely affected areas

- `src/imageviewport.h`
- `src/imageviewport.cpp`
- `src/imageviewport_p.h`
- `src/imageviewportfacade.*`
- `src/imageviewportpresentation.*`
- `src/viewportcontrollerpresentation.cpp`
- `src/CMakeLists.txt`
- `src/generate_installed_public_header.cmake`
- QML type registration and static plugin metadata under `src/`
- `tests/tst_imageviewport_public_api.cpp`
- `tests/tst_imageviewport_public_api_qml.cpp`
- `tests/tst_imageviewport_public_api_commands.cpp`
- `tests/tst_imageviewport_presentation_state.cpp`
- `tests/install_consumer/*`

#### Tasks

- [ ] Add the F1 and F2 intended-contract tests from the matrix before changing production code.
- [ ] Locate the existing public value type pattern for range, rectangle, size, point, command outcome, and revision token.
- [ ] Add a page-geometry value type using the project’s typed value pattern, such as `Q_GADGET`/`QML_VALUE_TYPE`, not a public `QVariantMap` contract.
- [ ] Include fields for page role, page rect in spread coordinates, item rect, visible page rect, and availability.
- [ ] Expose page-geometry accessors through C++ and QML consistently with the existing public API style.
- [ ] Update installed public header generation, QML module metadata, and install-consumer coverage so package consumers see the value type without private headers.
- [ ] Ensure page-geometry values are derived from existing presentation/domain state instead of introducing a second geometry source of truth.
- [ ] Add C++ and QML tests for one-page geometry, two-page geometry, explicit unavailable secondary geometry, non-`QVariantMap` field access, and compatibility rectangle equality.
- [ ] Change direction/gap command handling so rejected invalid requests and current-value no-ops leave diagnostics and revision tokens unchanged.
- [ ] Add or update command tests to cover invalid, no-op, and valid changed values.

#### Acceptance criteria

- Public C++ and QML callers can observe typed page geometry with all fields required by the spec.
- The page-geometry value type is visible through installed headers, QML module metadata, static plugin/package metadata, and the install-consumer test boundary.
- The public contract does not expose page geometry as `QVariantMap`.
- Secondary geometry is explicitly unavailable when no secondary page is part of the current spread.
- Existing rectangle compatibility properties equal the corresponding page-geometry projections.
- Invalid and current-value direction/gap commands do not clear diagnostics or advance revision tokens.
- Valid changed direction/gap commands still update presentation state and produce the documented command outcome.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_public_api|imageviewport_public_api_qml|imageviewport_public_api_commands|imageviewport_presentation_state|viewportcontroller_presentation|imageviewport_install_consumer'`

#### Risks / notes

- Public API additions may require generated QML type metadata or install-consumer coverage. Follow the existing project pattern rather than adding a parallel registration mechanism.
- Do not let compatibility accessors become a second geometry model; they should project from the same authoritative page-geometry state.

### Milestone 3: Built-In Payload Preparation Boundary

#### Objective

Perform the behavior-preserving payload-boundary refactor before render-fallback behavior changes touch adjacent code.

#### Source findings

F5.

#### Scope

- Stop controller/render code from directly passing mutable built-in `QImage` frame data across subsystem boundaries.
- Reuse or extend the existing prepared payload model so built-in and provider frames follow one render admission contract.
- Preserve existing still-image, timed-sequence, playback, and render behavior.

#### Out of scope

- No public API changes.
- No provider request semantics changes.
- No retained-render fallback behavior changes; that belongs to Milestone 4.
- No wholesale rewrite of image sequence storage or render scene graph.

#### Likely affected areas

- `src/viewportcontroller_p.h`
- `src/imageviewportcontroller.cpp`
- `src/imagesequencesource.cpp`
- `src/viewportcontroller.cpp`
- `src/viewportcontrollerrender.cpp`
- `src/framepreparation_p.h`
- `src/framepreparation.cpp`
- `tests/tst_imageviewport_still.cpp`
- `tests/tst_imageviewport_timed.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_viewportcontroller_playback.cpp`

#### Tasks

- [ ] Add the F5 intended-contract tests and static boundary checks from the matrix before changing production code.
- [ ] Trace built-in still and timed sequence frame flow from source loading through controller state to render commit.
- [ ] Remove `QImage sequenceFrameImage(...)` and `secondarySequenceFrameImage(...)` from controller context/port APIs.
- [ ] Prevent `src/viewportcontroller*.cpp` from reaching `ImageSequencePrivateAccess::frameImage(...)` directly or through `sourceFrameImage(...)`.
- [ ] Make built-in frame materialization return `PreparedPayload` or a structured preparation rejection through the preparation boundary.
- [ ] Keep sequence ownership and frame identity behind the sequence boundary.
- [ ] Add or update still, timed, playback, and render commit tests for displayed size, frame identity, render commit behavior, and playback behavior.

#### Acceptance criteria

- Built-in and provider frame paths both hand render code prepared payloads through one narrow `PreparedPayload` contract.
- `sequenceFrameImage(...)` and `secondarySequenceFrameImage(...)` are gone from controller context/port APIs.
- `src/viewportcontroller*.cpp` no longer calls `sourceFrameImage(...)` or `ImageSequencePrivateAccess::frameImage(...)`.
- Existing still-image, timed playback, and render behavior remains unchanged.
- Tests cover at least one still frame, one timed sequence frame, one playback frame transition, and one render commit path.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_still|imageviewport_timed|imageviewport_render_commit|viewportcontroller_playback'`
- `rg -n 'sequenceFrameImage|secondarySequenceFrameImage|ImageSequencePrivateAccess::frameImage|sourceFrameImage' src` should show no controller/render-boundary hits; any remaining hits must be confined to the sequence/preparation owner.

#### Risks / notes

- This structural change is broad enough to split if needed. If split, first remove render-facing direct `QImage` crossings behind a behavior-preserving handoff, then do cleanup in a follow-up milestone.

### Milestone 4: Retained Render Fallback Preserves Pixels

#### Objective

Ensure render failure semantics match the documented retained display fallback: retained state must correspond to retained visible content.

#### Source findings

F9.

#### Scope

- Fix render commit failure handling so old pixels remain available when `DisplayStatus::Retained` is published.
- Keep request failure projection distinct from visual display fallback.
- Add focused scene graph or render commit tests around retained fallback.

#### Out of scope

- No provider request or playback policy changes.
- No broad scene graph rewrite.
- No payload-boundary refactor; that should already be handled by Milestone 3.

#### Likely affected areas

- `src/renderadapter.cpp`
- `src/imageviewportrender.cpp`
- `src/viewportcontrollerrender.cpp`
- `tests/tst_imageviewport_render_commit.cpp`
- `tests/tst_imageviewport_render_scenegraph.cpp`
- `tests/imageviewport_paint_test_support.h`

#### Tasks

- [ ] Add the F9 intended-contract tests from the matrix before changing production code.
- [ ] Trace the current render commit lifecycle for successful commit, failed commit, retained fallback, and empty fallback.
- [ ] Identify the point where the old scene graph node or equivalent retained snapshot is destroyed.
- [ ] Change the lifecycle so a failed new commit keeps or rebuilds retained visible content before publishing retained state.
- [ ] Ensure render commit failure for the active request reports `requestStatus: Error` and `requestReason: RenderFailure`.
- [ ] Add tests that distinguish retained state with pixels from retained state without pixels, including a scenegraph or pixel-visible assertion.
- [ ] Ensure empty fallback behavior still clears content when no retained content is valid or clear-before-load applies.

#### Acceptance criteria

- A failed render commit for the active request reports `requestStatus: Error` and `requestReason: RenderFailure`.
- `displayStatus: Retained` is published only when retained pixels remain visible and the render path does not return `nullptr`.
- If no retained content is valid, or clear-before-load applies, the visual fallback is `displayStatus: Empty`.
- Render tests prove both request-state projection and visible-content behavior.
- No unrelated scene graph ownership changes are included.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_render_commit|imageviewport_render_scenegraph'`

#### Risks / notes

- Scene graph objects may have thread-affinity and lifetime constraints. Keep ownership changes narrow and use existing render adapter conventions.

### Milestone 5: Provider Assignment Publication Ordering

#### Objective

Make provider assignment publication order deterministic before broader provider event-boundary changes.

#### Source findings

F7.

#### Scope

- Ensure provider assignment state and notifications publish before provider-dispatch side effects can synchronously fail.
- Keep the change narrow to assignment/effect ordering.

#### Out of scope

- No normalized provider event API redesign.
- No provider playback or terminal cause behavior changes.
- No replacement of event-loop integration tests.

#### Likely affected areas

- `src/imageviewport.cpp`
- `src/imageviewportprovider.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_terminal_recovery.cpp`

#### Tasks

- [ ] Add the F7 intended-contract test from the matrix before changing production code.
- [ ] Trace provider assignment mutation, signal emission, and effect execution order.
- [ ] Reorder assignment publication so observers see the intended assignment state before synchronous provider-dispatch failure or recovery state.
- [ ] Add assertions for signal order and final public state after forced dispatch failure.

#### Acceptance criteria

- Provider assignment notifications precede forced provider-dispatch failure notifications.
- Final public state after synchronous dispatch failure remains the documented failure or recovery state.
- No provider event admission, playback, or terminal cause behavior changes are included.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_lifecycle|imageviewport_provider_terminal_recovery'`

#### Risks / notes

- This milestone may expose existing tests that assumed the old surprising signal order. Update those tests only when the new order is supported by the durable docs.

### Milestone 6: Deterministic Provider Delivery Seam

#### Objective

Add a behavior-preserving provider result delivery seam so later provider behavior changes can be tested without arbitrary event-loop waits.

#### Source findings

F8.

#### Scope

- Add a deterministic test seam for provider protocol tests to enqueue, capture, or directly deliver normalized provider events.
- Keep integration tests event-loop based where queued delivery, callback affinity, thread cleanup, or scene graph synchronization is the behavior under test.

#### Out of scope

- No provider event admission ownership change beyond what is needed to expose the seam.
- No provider request, playback, terminal cause, or cleanup behavior changes.
- No removal of integration tests that verify Qt event-loop or thread-affinity behavior.

#### Likely affected areas

- `src/viewportproviderbridge.cpp`
- Provider bridge/internal helper types under `src/`
- `tests/imageviewport_provider_counting_test_support.h`
- `tests/imageviewport_provider_synchronous_test_support.h`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_frame_admission.cpp`

#### Tasks

- [ ] Add the F8 seam tests from the matrix before changing production code.
- [ ] Identify protocol tests that use `QCoreApplication::processEvents()`, arbitrary waits, or wall-clock timing without testing event-loop behavior.
- [ ] Add the smallest internal seam that can drive provider result delivery deterministically in tests.
- [ ] Convert protocol-only timing tests to the deterministic seam.
- [ ] Preserve or add explicit event-loop tests for queued delivery, callback affinity, thread cleanup, and scene graph synchronization.

#### Acceptance criteria

- Provider protocol tests can drive delivery deterministically without arbitrary waits.
- Tests that verify Qt event-loop delivery, thread affinity, cleanup, or scene graph synchronization remain integration tests.
- The seam does not change public provider APIs or documented behavior.
- Non-affinity protocol tests no longer need `QCoreApplication::processEvents()` or arbitrary sleeps to observe provider results.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_requests|imageviewport_provider_lifecycle|imageviewport_provider_frame_admission'`

#### Risks / notes

- Keep this seam internal. If it starts becoming a public provider abstraction, stop and update the plan.

### Milestone 7: Controller-Owned Provider Event Admission

#### Objective

Move provider event admission, stale filtering, payload release, and terminal-cause validation hooks into one controller-owned boundary.

#### Source findings

F6, F8.

#### Scope

- Normalize provider results into one controller-facing event shape.
- Move session-token validation, stale filtering, and provider payload release decisions into one controller-owned admission path.
- Define terminal-cause representation and validation hook at the event boundary so Milestone 9 can project invalid causes without reopening event shape.

#### Out of scope

- No provider playback behavior changes.
- No public terminal cause projection changes beyond preserving existing behavior until Milestone 9.
- No provider cleanup ownership changes.

#### Likely affected areas

- `src/imageviewportprovider.cpp`
- `src/viewportproviderbridge.cpp`
- `src/viewportcontrollerprovider.cpp`
- Provider effect/result helper types under `src/`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_frame_admission.cpp`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_viewportcontroller_provider.cpp`

#### Tasks

- [ ] Add the F6 intended-contract tests from the matrix before changing production code.
- [ ] Inventory each provider result entry point and note which layer currently validates session identity, rejects stale results, releases payloads, and emits side effects.
- [ ] Define the smallest internal normalized provider event structure with session, generation, role, token, event kind, payload handle, terminal cause where relevant, change sets, follow-up effects, cleanup effects, and payload-release decision.
- [ ] Route bridge and provider wrapper callbacks into the normalized event path without changing the public provider contract.
- [ ] Move stale filtering and payload release ownership to a controller event admission function such as `handleProviderEvent(...)`.
- [ ] Remove item-side calls to `acceptsProviderSessionResult(...)`.
- [ ] Remove item/bridge ownership of stale frame-handle release.
- [ ] Ensure bridge/provider wrapper code only transports normalized events and returned effects.
- [ ] Add tests for metadata, frame, waiting, failure, unsupported, cancellation, EOS, stale-session, wrong-token, and payload-release behavior through the admission boundary.

#### Acceptance criteria

- There is one controller-owned provider event admission path for session validation, stale filtering, event classification, terminal-cause validation hooks, and payload release.
- Item-side code no longer calls `acceptsProviderSessionResult(...)`.
- Item and bridge code no longer own stale frame-handle release.
- Stale ownership-bearing payloads are released exactly once.
- Wrong-session or wrong-token events cannot mutate public state.
- Active events still update public state as documented.
- Bridge/provider wrapper code transports normalized events and returned effects rather than duplicating admission decisions.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_requests|imageviewport_provider_frame_admission|imageviewport_provider_lifecycle|viewportcontroller_provider'`
- `rg -n 'acceptsProviderSessionResult|stale.*release|release.*stale|handleProviderEvent' src/imageviewport.cpp src/imageviewportprovider.cpp src/viewportproviderbridge.cpp src/viewportcontrollerprovider.cpp` should show admission ownership centered in controller code, with no item/bridge stale-release decision path.

#### Risks / notes

- This milestone changes control flow. Keep public events and state transitions covered by tests from Milestones 1 and 6.
- If the normalized event structure grows beyond internal admission needs, split the milestone before continuing.

### Milestone 8: Provider Role-Aware Request And Playback Invariants

#### Objective

Centralize provider request acceptance and provider end-of-sequence playback decisions so primary and secondary roles follow the same domain rules.

#### Source findings

F3, F4.

#### Scope

- Introduce or reuse a shared helper for role-aware provider target materialization.
- Route primary and secondary accepted provider requests through the same retained/empty fallback path.
- Route provider end-of-sequence through the shared authored loop policy used by controller playback.

#### Out of scope

- No provider bridge event API redesign; that belongs to Milestone 7.
- No built-in payload preparation boundary changes; that belongs to Milestone 3.
- No changes to documented playback policy.

#### Likely affected areas

- `src/viewportcontrollerplayback.cpp`
- `src/viewportcontrollerprovider.cpp`
- `src/viewportcontrollerhelpers_p.h`
- `tests/tst_viewportcontroller_playback.cpp`
- `tests/tst_viewportcontroller_provider.cpp`
- `tests/tst_imageviewport_provider_requests.cpp`
- `tests/tst_imageviewport_provider_playback.cpp`
- `tests/tst_imageviewport_provider_terminal_playback.cpp`

#### Tasks

- [ ] Add the F3 and F4 intended-contract tests from the matrix before changing production code.
- [ ] Identify the current accepted-request path for primary provider targets and the divergent secondary path.
- [ ] Extract the minimum shared role-aware target materialization logic needed to preserve public behavior.
- [ ] Route `seek(Secondary, frame)` through the same fallback, revision, and role-state update rules as primary seek.
- [ ] Route `seekToPosition(Secondary, milliseconds)` through the same fallback, revision, and role-state update rules as primary position seek.
- [ ] Ensure secondary seek and position seek publish `requestStatus: Loading` and `displayStatus: Retained` or `displayStatus: Empty` until complete spread commit.
- [ ] Ensure `DisplayStatus::Ready` is not published until all required roles for the spread commit.
- [ ] Identify the existing authored loop policy owner in playback code.
- [ ] Replace provider-specific end-of-sequence target selection with the shared playback-domain selector.
- [ ] Add tests for playback-token EOS handling, play-once final-frame pending/promotable behavior, no beyond-final requested target, `looping: true` override, `looping: false` authored finite policy, `looping: false` authored infinite policy, and secondary EOS using secondary role state.

#### Acceptance criteria

- Primary and secondary frame seeks produce consistent retained or empty fallback behavior for equivalent role state.
- Primary and secondary position seeks produce consistent retained or empty fallback behavior for equivalent role state.
- Secondary seek paths do not project primary-only loading state, primary-only final-frame state, or beyond-final requested targets.
- Provider end-of-sequence behavior matches authored finite and infinite loop policy, including `looping: true` override and `looping: false` authored policy.
- Playback policy is owned in one playback-domain location, not duplicated in provider terminal handling.
- Existing provider request and playback tests pass without weakening assertions.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'viewportcontroller_playback|viewportcontroller_provider|imageviewport_provider_requests|imageviewport_provider_playback|imageviewport_provider_terminal_playback'`

#### Risks / notes

- Avoid changing provider event admission and playback policy in the same patch unless the existing call graph makes a small shared helper impossible.

### Milestone 9: Provider Failure Semantics And Cleanup Ownership

#### Objective

Make provider terminal cause classification and provider cleanup ownership failure-safe and diagnosable.

#### Source findings

F10, F11.

#### Scope

- Validate provider unsupported causes before projecting public terminal state.
- Classify unknown or invalid causes as provider protocol errors using the documented `Error`/`PayloadRejection` path.
- Make provider close cleanup ownership robust when cleanup scheduling fails.
- Preserve normal unsupported behavior for valid unsupported-operation and unsupported-payload causes.

#### Out of scope

- No new public error taxonomy unless existing documented errors cannot represent the distinction.
- No provider event boundary redesign beyond using the Milestone 7 event shape and validation hook.
- No unrelated logging or metrics framework changes.

#### Likely affected areas

- `src/viewportproviderbridge.cpp`
- `src/viewportcontrollerprovider.cpp`
- `src/imageviewport.h`
- `tests/tst_imageviewport_provider_lifecycle.cpp`
- `tests/tst_imageviewport_provider_terminal_projection.cpp`
- `tests/tst_imageviewport_provider_terminal_diagnostics.cpp`
- `tests/tst_imageviewport_provider_terminal.cpp`

#### Tasks

- [ ] Add the F10 and F11 intended-contract tests from the matrix before changing production code.
- [ ] Identify the enum or value validation point for provider terminal unsupported causes.
- [ ] Classify invalid or unknown unsupported causes after token validation as provider protocol violations.
- [ ] Project invalid cause public state as `RequestStatus::Error` with `RequestReason::PayloadRejection`.
- [ ] Emit bounded protocol-violation diagnostics for invalid causes without leaking unbounded provider text.
- [ ] Close provider interest consistently for invalid causes.
- [ ] Preserve expected behavior and diagnostics for valid unsupported-operation and unsupported-payload causes.
- [ ] Audit provider close cleanup scheduling for ownership transfer before failure-prone operations.
- [ ] Introduce RAII cleanup ownership or restore ownership on scheduling failure so cleanup can be retried safely.
- [ ] Add tests for invalid metadata, frame, and playback causes; valid unsupported projection; cleanup scheduling failure; repeated close idempotence; and late callbacks from the failed-close session.

#### Acceptance criteria

- Invalid metadata, frame, and playback unsupported causes report `RequestStatus::Error` with `RequestReason::PayloadRejection`.
- Invalid cause diagnostics are bounded and identify a provider protocol violation.
- Invalid causes close provider interest consistently.
- Valid unsupported-operation and unsupported-payload causes still produce documented unsupported state and diagnostics.
- Provider cleanup ownership is not lost when cleanup scheduling fails.
- Repeated close after cleanup scheduling failure is idempotent.
- Late callbacks from a failed-close session cannot mutate public state.

#### Verification

- `just test`
- `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_lifecycle|imageviewport_provider_terminal_projection|imageviewport_provider_terminal_diagnostics|imageviewport_provider_terminal'`

#### Risks / notes

- Prefer existing public error categories. If they cannot faithfully represent provider protocol violations, pause and update the durable docs plus this plan before expanding public taxonomy.

## Deferred / Needs Investigation

- Lower-priority provider test maintenance: The design review noted timing-sensitive cleanup tests and source-text metadata scans as weaker design signals. These are not implementation-ready as architecture work. Reassess after Milestone 6 adds deterministic provider delivery and Milestone 7 centralizes admission.
- Public error taxonomy expansion: If Milestone 9 proves existing documented error categories cannot represent invalid provider causes, pause implementation and update the durable docs plus this plan before adding new public error categories.
- Durable documentation updates: No spec or architecture changes are currently required. If implementation discovers that the intended end state cannot be determined from `docs/spec/**` and `docs/architecture/**`, record the ambiguity here before modifying durable docs.

## Suggested `/goal` Execution Order

1. Milestone 1: Baseline Verification And Contract-Test Matrix
2. Milestone 2: Public API Conformance For Page Geometry And Command Outcomes
3. Milestone 3: Built-In Payload Preparation Boundary
4. Milestone 4: Retained Render Fallback Preserves Pixels
5. Milestone 5: Provider Assignment Publication Ordering
6. Milestone 6: Deterministic Provider Delivery Seam
7. Milestone 7: Controller-Owned Provider Event Admission
8. Milestone 8: Provider Role-Aware Request And Playback Invariants
9. Milestone 9: Provider Failure Semantics And Cleanup Ownership

## Plan Review Notes

- Concerns addressed: Review feedback made Milestone 1 green-only, added a contract-test matrix, made page-geometry install/QML exposure explicit, moved the built-in prepared-payload boundary before retained-render fixes, moved deterministic provider delivery before provider playback changes, split provider boundary work into smaller milestones, clarified render request-status versus display-status outcomes, enumerated secondary provider seek/EOS cases, added provider event removal targets, and made invalid provider cause mapping exact.
- Concerns deferred: Lower-priority provider test maintenance remains deferred until deterministic provider delivery and centralized admission exist. Public error taxonomy expansion remains deferred unless existing documented categories prove insufficient.
- Remaining known limitations: Proposed test names are guidance, not existing symbols. Future sessions must confirm exact target names with `ctest -N` before implementation and update this plan if the local test topology changes.

## Non-Goals

- Do not perform broad rewrites or unrelated cleanup while executing these milestones.
- Do not remove compatibility public API properties unless the durable specs are updated first.
- Do not change `docs/spec/**` or `docs/architecture/**` during milestone execution except to resolve a documented ambiguity or blocker.
- Do not use `docs/architecture/**` as a file-by-file implementation inventory.
- Do not add new public error categories, provider APIs, or rendering abstractions unless the relevant milestone proves existing contracts are insufficient.
- Do not replace integration tests that are intentionally verifying Qt event-loop delivery, callback affinity, thread cleanup, or scene graph synchronization.
- Do not commit generated files, build artifacts, dependency folders, snapshots, lockfiles, vendored code, or compiled outputs as part of milestone work.
