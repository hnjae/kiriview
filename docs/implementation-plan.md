# ImageViewport Implementation Plan

This document is a non-normative execution queue for bringing the current implementation into conformance with the authoritative end-state documents in `docs/spec/**` and `docs/architecture/**`.

If this plan conflicts with `docs/spec/**` or `docs/architecture/**`, the spec and architecture documents win. Update this plan rather than changing authoritative documents unless a separate user instruction explicitly asks for a spec or architecture change.

## Conformance Audit

- Baseline source state: the current implementation is a partial single-sequence viewport with meaningful request identity, provider token, metadata admission, frame admission, retained-display, render-acknowledgement, revision-token, and playback mechanics already present.
- Baseline public API state: `ImageViewport` exposes `sequence`, primary-only request/display observations, legacy `FillMode`, alignments, `zoom`, `pan`, primary-only coordinate helpers, and provider construction helpers; it does not expose the documented page-set, role-scoped, spread, final fit/zoom, rotation, or transition-policy surface.
- Baseline controller state: the controller owns one active sequence, one active display request, one provider generation, and one display payload identity; it does not yet model primary and secondary roles as one accepted page set.
- Baseline geometry state: geometry is single-image placement using legacy `Contain`, `Cover`, `Stretch`, `Center`, alignment, `zoom`, and `pan`; it does not yet implement spread bounds, `FitMode`, physical-pixel-aware `zoomPercent`, content position, rotation, page gap, spread direction, scan state, or role-scoped page conversion.
- Baseline render state: scene graph ownership is encapsulated and render acknowledgements exist for a prepared payload identity; render synchronization does not yet acknowledge complete role sets for a two-page target spread.
- Baseline provider state: public provider adapter/session concepts, tokens, metadata results, frame-ready results, cancellation, close, and failure classification are partially implemented and tested for one active role.
- Baseline playback state: timed playback, waiting, stop restoration, end-of-sequence handling, and provider playback entry points are partially implemented for one active sequence; role-scoped playback drivers and authored animation facts remain incomplete.
- Baseline tests: the existing build-tree tests passed with `env LD_LIBRARY_PATH=/nix/store/41i5n4fkcz3zbjz9wbi5r35j2x1gziyc-image-viewport-qt-query-prefix/lib ctest --test-dir build-ninja --output-on-failure`.
- Baseline test environment gap: fresh `just test` failed at CMake configure because the Qt query prefix lacked `metatypes/qt6qml_metatypes.json` and `metatypes/qt6core_metatypes.json`; direct `ctest` also needed an explicit Qt `LD_LIBRARY_PATH`.
- Current high-priority gaps: missing `setPageSet`, `PageRole`, `PageSetTransitionPolicy`, `primarySequence`, `secondarySequence`, role-scoped observations, spread direction, page gap, final presentation commands, rotation, typed public value types, authored animation facts, display-demand limits, and complete-spread render commits.
- Current compatibility risk: existing tests intentionally assert legacy public API such as `FillMode`, alignment, `zoom`, and `pan`; removing or changing those installed names is destructive unless explicitly approved.

## Documented Requirements

- Sequence and page-set assignment must accept only `ImageSequence` objects or `null`, use `setPageSet(primary, secondary, policy)` as the canonical page-set replacement API, treat `sequence` as primary-only compatibility assignment, expose `primarySequence` and `secondarySequence` as observations, reject unsupported raw values before state changes, and preserve state on invalid page-role arguments. References: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Page roles and spread presentation must model one accepted page set with required primary and optional secondary roles, place pages by spread direction, include explicit page gap in spread bounds, expose spread/page geometry, and commit two-page target spreads only after all required roles are ready. References: [ImageViewport Behavior](spec/image-viewport.md), [Rendering](architecture/rendering.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Page-set transition policy must atomically apply retained-or-empty display selection, fit/zoom/content-position handoff, rotation and mirror reset or preservation, spread direction, page gap, scan handoff, and same-target refinement annotation, with no partial state application for invalid policy fields. References: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Presentation geometry must use `Contain`, `FitWidth`, `FitHeight`, and `Manual`, expose physical-pixel-aware `zoomPercent`, own content size, content position, maximum content position, pannability, visible spread/page rectangles, quarter-turn rotation, mirroring, scan position, and real-valued half-open coordinate helpers. References: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Rendering](architecture/rendering.md).
- Request and display state must keep accepted request readiness separate from visible presentation readiness, support retained display as visual fallback only, project wait reasons in priority order, enforce deterministic command admission, preserve state on invalid/unsupported commands, and scope terminal failures as generation-terminal or display-request-terminal. References: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Provider integration must be implementable from installed public headers, create independent role-local sessions, use session-scoped tokens, keep transport narrow, classify metadata/frame/protocol failures deterministically, and ignore stale or closed-generation results. References: [ImageSequence Provider Adapter](spec/image-sequence-provider-adapter.md), [Provider Protocol](architecture/provider-protocol.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Playback must have one active role driver per viewport, keep waiting clock time from accumulating, restore non-playback targets on `stop()`, handle end of sequence without beyond-final targets, and apply authored autoplay, progressive readiness, finite loops, and infinite loops through the same request path as explicit playback. References: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Playback State Machine](architecture/playback-state-machine.md).
- Public limits and diagnostics must expose source limits, display-demand limits including `maximumManualZoomPercent`, bounded redacted plain-text diagnostics, and stable public status/reason pairs. References: [ImageViewport API](spec/image-viewport-api.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).

## Milestone Plan

### M0 Build And Test Harness

- Status: Completed.
- Verification: after removing the stale generated `build-ninja` tree, `just test` configured, built, and ran all 18 CTest tests successfully in the current project shell, including `imageviewport_install_consumer`, without manual `LD_LIBRARY_PATH`.
- Dependencies: None.
- Acceptance criteria: `just test` configures, builds, and runs from a clean project shell without manual `LD_LIBRARY_PATH`; the existing 18 tests pass; the install-consumer test still passes; no spec or architecture files change.
- Relevant references: [Specification Index](spec/README.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Expected tests/checks: `just test`; `ctest --test-dir build-ninja --output-on-failure`; inspect generated CTest environment for Qt runtime paths.
- Likely code areas: `devenv.nix`, `justfile`, root `CMakeLists.txt`, `tests/CMakeLists.txt`.
- Commit expectation: commit only the harness changes for this milestone.

### M1 Final Public API Scaffolding

- Status: Completed.
- Assumption: M1 is an additive public-surface scaffold only; primary-role observations alias the existing compatibility state, secondary-role observations expose documented unavailable defaults, and M2 owns accepting and storing a real two-role page set.
- Verification: added public API/QML scaffold coverage, confirmed generated `ImageViewport.qmltypes` exports the new value types and API entries, ran `ctest --test-dir build-ninja --output-on-failure -R '^imageviewport_public_api$'`, `git diff --check`, and `just test`.
- Dependencies: M0.
- Acceptance criteria: `ImageViewport` metaobject exposes documented enums, public value types, properties, role-scoped commands, page-set replacement commands, compatibility aliases, and default/unavailable values; unsupported raw values through `sequence` or page-set arguments preserve request and display state; legacy public API remains only as an explicitly documented compatibility layer or is left untouched until a separate approval.
- Relevant references: [ImageViewport API](spec/image-viewport-api.md), [ImageViewport Behavior](spec/image-viewport.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Expected tests/checks: public API metaobject tests; QML construction and assignment tests; invalid type-assignment preservation tests; install-consumer public-header test.
- Likely code areas: `src/imageviewport.h`, `src/imageviewportfacade.cpp`, `src/imageviewport_p.h`, `src/imageviewport_p.cpp`, QML value-type registration, `tests/tst_imageviewport_public_api.cpp`.
- Commit expectation: commit failing public API tests first when practical, then commit the implementation.

### M2 Page-Set And Role State Model

- Status: Completed.
- Assumption: M2 stores the accepted secondary role and construction-time secondary observations, while M4/M6 remain responsible for independent secondary provider/request/render pipelines and complete-spread commit readiness.
- Verification: added page-set role observation coverage, confirmed the focused `imageviewport_public_api` test passes, ran `just test`, and ran `git diff --check`.
- Dependencies: M1.
- Acceptance criteria: primary-only page sets and two-role page sets are accepted through one transaction; `primarySequence` and `secondarySequence` are observations only; `sequence` assignment is equivalent to primary-only `setPageSet`; secondary-without-primary is inert or clear-style according to the command contract; no transient primary-only accepted page set is exposed for a two-role command; role-scoped metadata observations alias or default correctly.
- Relevant references: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Expected tests/checks: controller unit tests for page-set transactions; QML tests for primary/secondary observations; tests for clear-style `setPageSet(null, secondary, policy)`; stale-result tests around page-set replacement.
- Likely code areas: `src/imageviewportstate_p.h`, `src/viewportcontroller_p.h`, `src/viewportcontroller.cpp`, `src/imageviewport_p.cpp`, provider generation state, test support helpers.
- Commit expectation: commit failing role/page-set tests first when practical, then commit the state-model implementation.

### M3 Transition Policy And Retained Display Semantics

- Status: Completed.
- Assumption: M3 establishes the page-set transition command boundary, policy validation, default retain behavior, clear-before-load display clearing, and basic presentation reset/explicit-field application; later spread geometry and complete-spread render milestones own the full content-position, scan, and per-role commit geometry behavior.
- Verification: added public API transition-policy coverage for `ClearBeforeLoad`, invalid non-empty replacement policies, and invalid clear-style policies; confirmed the focused `imageviewport_public_api` test passes, ran `just test`, and ran `git diff --check`.
- Dependencies: M2.
- Acceptance criteria: default page-set policy retains previous display, preserves fit/zoom intent, clamps content position, preserves rotation, mirror flags, spread direction, and page gap; invalid policy fields return `Invalid` with no partial application; `RetainPrevious` and `ClearBeforeLoad` produce documented display status; same-target refinement creates a new accepted request and never overrides explicit fields; clear-style operations validate policy but do not mutate presentation state.
- Relevant references: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Expected tests/checks: transition policy value-validation tests; retained-vs-empty replacement tests; revision-token preservation tests for invalid policy; provider-work non-start tests for invalid policy; same-target stale-result tests.
- Likely code areas: `src/viewportcontroller.cpp`, transition-policy value types, command admission helpers, provider close/cancel paths, display state.
- Commit expectation: commit failing transition tests first when practical, then commit implementation.

### M4 Role-Scoped Provider And Request Pipeline

- Status: Completed.
- Assumption: M4 establishes role-local provider session ownership, token-scoped secondary metadata startup, and secondary runtime metadata observations; M6 remains responsible for full secondary frame payload/render synchronization and complete-spread ready commits, while M7 owns role-scoped playback drivers.
- Verification: added public API coverage for secondary provider session startup and metadata observation projection, confirmed the focused `imageviewport_public_api` test passes, ran `just test`, and ran `git diff --check`.
- Dependencies: M2, M3.
- Acceptance criteria: each accepted role has independent generation/session/token/request state; provider sessions are role-local; metadata, frame, waiting, unsupported, cancellation, failure, and end-of-sequence events are validated against role, session, generation, and token identity; spread request status aggregates required role waits and terminal failures; stale provider/preparation/render results cannot mutate newer page sets or roles.
- Relevant references: [ImageSequence Provider Adapter](spec/image-sequence-provider-adapter.md), [Provider Protocol](architecture/provider-protocol.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Expected tests/checks: provider metadata tests for primary and secondary roles; provider lifecycle and cancellation tests per role; token collision and stale-result tests across roles and generations; aggregate failure projection tests.
- Likely code areas: `src/viewportcontroller.*`, `src/viewportproviderbridge.*`, `src/framepreparation.*`, `src/imageviewportprovider.cpp`, provider test support.
- Commit expectation: commit failing role-scoped provider tests first when practical, then commit implementation.

### M5 Spread Geometry And Presentation Commands

- Status: Completed.
- Assumption: M5 makes the public geometry and presentation command model spread-aware for accepted primary/secondary roles, including secondary provider metadata when known; M6 remains responsible for making the render snapshot and scene graph present both role payloads atomically rather than only exposing their canonical geometry.
- Verification: added presentation-state coverage for two-page spread placement, page gap, reading direction, role coordinate rejection in gaps and half-open edges, fit modes, effective zoom, pannability, clamped pan commands, and quarter-turn coordinate mapping; confirmed the focused `imageviewport_presentation_state`, neighboring public/still/render-scenegraph tests, full `just test`, and `git diff --check` pass.
- Dependencies: M2.
- Acceptance criteria: spread canvas is built from primary and optional secondary roles; `LeftToRight` and `RightToLeft` placement, explicit page gap, `Contain`, `FitWidth`, `FitHeight`, `Manual`, physical-pixel-aware `zoomPercent`, content position, maximum content position, pannability, visible spread/page rectangles, page/item rectangles, scan commands, pan commands, mirror, quarter-turn rotation, and reset view match the documented public behavior; helpers reject half-open right/bottom edges and non-presentable geometry.
- Relevant references: [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Rendering](architecture/rendering.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Expected tests/checks: geometry tests for primary-only and two-page spreads; DPR-aware zoom tests; rotation/mirror coordinate tests; non-positive item geometry tests; invalid presentation command tests; scan and pan boundary tests.
- Likely code areas: `src/presentationgeometry.*`, `src/imageviewportpresentation.cpp`, `src/imageviewportstate_p.h`, `src/viewportcontroller.cpp`, `src/renderadapter_p.h`.
- Commit expectation: commit failing geometry/presentation tests first when practical, then commit implementation.

### M6 Complete-Spread Render Synchronization

- Status: Completed.
- Assumption: M6 synchronizes the render payload set for the active target spread and keeps provider-backed secondary roles from publishing partial primary-only displays; M7 remains responsible for role-scoped playback advancement, timed secondary driver selection, and authored animation policy.
- Verification: added scenegraph coverage for built-in/built-in spreads, built-in primary plus secondary provider spreads, and primary-plus-secondary provider spreads; added render-commit coverage for secondary spread render failure retaining the previous complete display; confirmed focused `imageviewport_render_scenegraph`, `imageviewport_render_commit`, and `imageviewport_public_api` tests, full `just test`, and `git diff --check` pass.
- Dependencies: M4, M5.
- Acceptance criteria: render snapshots carry prepared payload identity, target spread data, role payload set, and presentation mapping; complete two-role spreads commit atomically only after all required roles are prepared and acknowledged; render failures are scoped to active role, generation, request, payload, and spread identity; presentation-only changes reuse committed payloads and advance display revisions without changing request readiness.
- Relevant references: [Rendering](architecture/rendering.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md), [ImageViewport Behavior](spec/image-viewport.md).
- Expected tests/checks: render commit tests for complete spread acknowledgement; partial role render tests; stale render acknowledgement tests; render failure aggregation tests; scene graph tests for background, mirror, rotation, gap, and role placement.
- Likely code areas: `src/renderadapter.*`, `src/imageviewportrender.cpp`, `src/viewportcontroller.cpp`, `src/imageviewportstate_p.h`, render test support.
- Commit expectation: commit failing render synchronization tests first when practical, then commit implementation.

### M7 Playback And Authored Animation Conformance

- Status: In progress.
- Progress: secondary `pause(role)` and `stop(role)` now admit present non-driver roles as accepted no-ops without mutating the active primary playback driver; negative and out-of-range secondary frame and position seek commands now use present-role invalid-command precedence; secondary still-image `play(role)` now reports unsupported capability rather than no-request; secondary timed `play(role)`, valid secondary `seek(role, frame)`, valid `seekToPosition(role, milliseconds)`, provider role-driver advancement, and authored animation policy remain.
- Dependencies: M4, M5, M6.
- Acceptance criteria: role-scoped `play`, `pause`, `stop`, `seek`, and `seekToPosition` follow command admission precedence; exactly one active playback driver exists per viewport; non-driver pause/stop commands are accepted no-ops; waiting pauses elapsed time; stop restores the latest non-playback target for the addressed active driver; end-of-sequence handling never publishes beyond-final targets; authored autoplay, progressive animation readiness, authored finite loops, and authored infinite loops are supported through the viewport request path.
- Relevant references: [Playback State Machine](architecture/playback-state-machine.md), [ImageViewport Behavior](spec/image-viewport.md), [ImageViewport API](spec/image-viewport-api.md), [Provider Protocol](architecture/provider-protocol.md).
- Expected tests/checks: playback controller tests for primary and secondary roles; provider playback entry-point tests; end-of-sequence tests; stop restoration tests; authored autoplay and authored loop policy tests; waiting-clock no-catch-up tests.
- Likely code areas: `src/viewportcontroller.cpp`, `src/playbackclock_p.h`, `src/playbacktimeline_p.h`, `src/timingintervals.*`, provider playback dispatch, sequence authored-facts storage.
- Commit expectation: commit failing playback/authored-animation tests first when practical, then commit implementation.

### M8 Public Limits, Diagnostics, And Packaging

- Status: Not started.
- Dependencies: M1 through M7.
- Acceptance criteria: `ImageSequenceLimits` and display-demand limits expose the documented public fields including `maximumManualZoomPercent`; manual zoom above the published display limit returns `Invalid` without changing canonical presentation state; source admission limits and display-demand limits stay separate; diagnostics are bounded, redacted, plain text, and owned by request, command, or warning lifetimes as documented; installed headers and QML import expose only public API and hide private scene graph/backend details.
- Relevant references: [ImageViewport API](spec/image-viewport-api.md), [ImageSequence Provider Adapter](spec/image-sequence-provider-adapter.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md), [Rendering](architecture/rendering.md).
- Expected tests/checks: limit singleton tests; manual zoom limit tests; diagnostic redaction/bounding tests; install-consumer tests; QML import tests.
- Likely code areas: `src/imageviewport.h`, `src/imagesequencefactory.cpp`, `src/framepreparation.cpp`, diagnostic helpers, install CMake.
- Commit expectation: commit failing limit/diagnostic tests first when practical, then commit implementation.

### M9 Final Conformance Sweep

- Status: Not started.
- Dependencies: M0 through M8.
- Acceptance criteria: all documented observable behavior in `docs/spec/**` and durable architecture constraints in `docs/architecture/**` have implementation coverage or explicit deferred status in this plan; no known implementation behavior conflicts with authoritative docs; compatibility behavior is either non-conflicting or explicitly approved; `just test` passes; lint/format checks pass or documented environment blockers remain.
- Relevant references: [Specification Index](spec/README.md), [Architecture Index](architecture/README.md), all referenced spec and architecture files.
- Expected tests/checks: `just test`; `just lint`; `just format` followed by `git diff --check`; install-consumer; focused manual API review against `docs/spec/image-viewport-api.md`.
- Likely code areas: public headers, QML registrations, CMake packaging, tests, documentation status in this file only.
- Commit expectation: commit final cleanup and status updates after the full sweep.

## Execution Rules

- Execute milestones from top to bottom unless a stop condition applies.
- Keep `docs/spec/**` and `docs/architecture/**` unchanged during milestone execution unless the user explicitly asks for a normative document change.
- Add a failing test before implementation when practical, especially for public API, role transitions, provider state, render identity, geometry, playback, and diagnostics.
- Update the status field for the active milestone in this file as work progresses, and commit status updates with the milestone when completed.
- Commit after each completed milestone using Conventional Commits with a clear scope.
- When Codex materially authors a commit, include `Co-authored-by: Codex <noreply@openai.com>` in the commit footer.
- Do not use this plan as a source of truth for behavior; use the referenced spec and architecture documents to resolve behavioral questions.

## Stop Conditions

- Stop if implementation behavior appears to require changing `docs/spec/**` or `docs/architecture/**`; ask whether to update the authoritative documents or revise the implementation approach.
- Stop if Qt/QML value-type representation for `PageSetTransitionPolicy`, ranges, rectangles, points, coordinate results, page geometry, command outcomes, or revision tokens cannot be implemented unambiguously with Qt 6.5 public APIs.
- Stop if provider threading or session-affinity behavior cannot be made to match the documented contract without a structural architecture decision.
- Stop if tests cannot be run because of environment defects that are outside the current milestone, and record the blocker in this file before asking for direction.
