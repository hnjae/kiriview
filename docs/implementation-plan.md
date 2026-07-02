# Implementation Plan

This plan translates the approved target state in `docs/spec/` and `docs/architecture/` into implementation milestones. It is a planning document only and does not approve product behavior beyond those target-state docs.

Status values are `Not Started`, `In Progress`, `Blocked`, and `Complete`. Milestone status is tracked on each milestone section below.

## Confirmed Requirements

- Secondary provider-backed page roles must support the full provider result channel before spread aggregation; the secondary role is not metadata-only and does not have a reduced event set. Sources: [ImageViewport Behavior](spec/image-viewport.md), [ImageSequence Provider Adapter](spec/image-sequence-provider-adapter.md), [Provider Protocol](architecture/provider-protocol.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- Public range, rectangle, size, point, coordinate-result, page-geometry, page-set-transition-policy, command-outcome, and revision-token surfaces are typed public value types for C++ and registered QML value types, not `QVariantMap` contracts. Sources: [ImageViewport API](spec/image-viewport-api.md), [Subsystem Boundaries](architecture/subsystem-boundaries.md).
- `PageSetTransitionPolicy` exposes typed transition fields directly, including fit mode, spread direction, and page gap transitions; callers do not express final policy through untyped key/value maps. Source: [ImageViewport API](spec/image-viewport-api.md).
- Legacy presentation aliases such as raw Qt Quick fill modes, horizontal or vertical alignment properties, raw scale-style `zoom`, and raw `pan` properties are not part of the final public API and are not compatibility commitments. Source: [ImageViewport API](spec/image-viewport-api.md).
- Canonical viewport presentation state is controller-owned; the item may expose projected values but must not store a second mutable source of truth. Sources: [Subsystem Boundaries](architecture/subsystem-boundaries.md), [Rendering](architecture/rendering.md).
- The supported install target is Linux with a static Qt QML module plugin; cross-platform plugin filenames, dynamic-plugin package layouts, and non-Linux install-consumer behavior are out of scope. Sources: [ImageViewport API](spec/image-viewport-api.md), [Build And Package Boundary](architecture/build-and-package.md).

## Current Implementation Facts To Plan Against

- `src/viewportproviderbridge.cpp` already normalizes all provider signal kinds into `ViewportProviderEvent`, but `ImageViewportPrivate::handleProviderEvent` in `src/imageviewportprovider.cpp` currently handles only metadata and borrowed-frame events for the secondary role and drops other secondary event kinds after releasing any frame handle.
- `src/imageviewport.h` currently exposes `QVariantMap` range properties and coordinate helper results, numeric `uint` revision properties, a `PageSetTransitionPolicy` value type without explicit fit/spread/gap transition fields, and legacy presentation aliases including `FillMode`, alignments, raw `zoom`, and raw `pan`.
- `src/imageviewport.cpp` currently accepts undocumented `QVariantMap` transition-policy keys for fit mode, spread direction, and page gap, while typed `PageSetTransitionPolicy` conversion only carries the narrower field set.
- `ImageViewportPrivate` currently stores `ImageViewportInternal::PresentationState presentation` beside `ViewportController`, and `src/imageviewportpresentation.cpp`, `src/imageviewport.cpp`, and `src/presentationgeometry.cpp` mutate or consume that state directly.
- `tests/CMakeLists.txt` registers unit and integration-style Qt tests plus `imageviewport_install_consumer`; no tracked CI workflow file was found during planning.

## Implementation Discipline

- Follow the repository change discipline: for each product or structure milestone, add the focused failing or missing tests first when practical, then implement the product or code changes, and keep commits layered by intent.
- Keep specs and architecture unchanged during implementation unless an unresolved intent issue is found; if implementation requires changing the approved target state, stop and request approval before changing docs or code.
- Do not add non-Linux portability work while implementing the package milestone; the approved package target is Linux/static QML plugin only.

## Verification Inventory

- Safe local build path: `just configure`, `just build`, and `just test`, which configure `build-ninja`, build with Ninja, and run `ctest --test-dir build-ninja --output-on-failure`.
- Safe targeted test path after configure/build: `ctest --test-dir build-ninja --output-on-failure -R '<test-name-or-regex>'`.
- Lint path: `just lint`, which runs `run-clang-tidy`, `clazy-standalone`, and `cmake-lint` after configure.
- Mutating maintenance path: `just format` rewrites C++ and CMake files and should be run only when the implementation phase intentionally formats touched code.
- Cleanup path: `just clean` removes `build-ninja` and is destructive to local build artifacts.
- Package smoke path: `ctest --test-dir build-ninja --output-on-failure -R imageviewport_install_consumer`.

## Milestone Plan

1. M1: Provider role-symmetric result handling.
2. M2: Typed public value API and transition policy.
3. M3: Legacy presentation API removal.
4. M4: Controller-owned presentation state.
5. M5: Linux/static package contract conformance.
6. M6: Final conformance sweep.

## M1: Provider Role-Symmetric Result Handling

- Status: Complete
- Goal: Make primary and secondary provider sessions use the same public result channel and controller event boundary.
- Scope: Add missing secondary-provider tests first, then route secondary frame handles, frame handles with metadata, waiting/progress hints, end-of-sequence, unsupported results, provider failures, and cancellation results through the same role-scoped controller path used by primary providers.
- Acceptance Criteria: Secondary `FrameHandleReady` and `FrameHandleWithMetadataReady` results are validated, rendered or rejected, and released exactly once according to the provider payload contract in `docs/spec/image-sequence-provider-adapter.md`.
- Acceptance Criteria: Secondary waiting/progress events participate in spread request wait-reason projection without exposing numeric progress, matching `docs/spec/image-viewport.md` request/status behavior and `docs/spec/image-sequence-provider-adapter.md` progress semantics.
- Acceptance Criteria: Secondary unsupported, failure, cancellation, and end-of-sequence results map through the same token-scope and failure-scope rules as primary results before aggregate spread projection, matching `docs/spec/image-viewport.md`, `docs/spec/image-viewport-api.md`, and `docs/architecture/provider-protocol.md`.
- Acceptance Criteria: A target spread with a secondary role never publishes partial ready display ownership or secondary-less ready geometry, matching `docs/spec/image-viewport.md` and `docs/architecture/subsystem-boundaries.md`.
- Spec References: `docs/spec/image-viewport.md` sections `Page Roles And Spread Presentation` and `Requests And Status`; `docs/spec/image-sequence-provider-adapter.md` sections `Public Session API`, `Provider Responsibilities`, and `Failure Semantics`; `docs/spec/image-viewport-api.md` sections `Public Status` and `Status And Diagnostics`.
- Architecture References: `docs/architecture/provider-protocol.md` sections `Requests`, `Payload Admission`, and `Shutdown`; `docs/architecture/subsystem-boundaries.md` sections `Controller Boundary`, `Preparation Boundary`, and `Render Boundary`.
- Expected Tests/Checks: Integration tests in `tests/tst_imageviewport_provider_terminal.cpp` for secondary provider failure, unsupported, and cancellation projection; integration tests in `tests/tst_imageviewport_provider_frame_admission.cpp` for secondary frame-handle admission and release; integration tests in `tests/tst_imageviewport_provider_requests.cpp` for secondary request queueing, cancellation, stale-result rejection, and wait projection; integration tests in `tests/tst_imageviewport_provider_playback.cpp` for secondary end-of-sequence playback handling; integration or smoke tests in `tests/tst_imageviewport_render_commit.cpp` and `tests/tst_imageviewport_render_scenegraph.cpp` for complete-spread commit and no partial ready state; targeted command `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_provider_(terminal|frame_admission|requests|playback)|imageviewport_render_(commit|scenegraph)'`.
- Likely Code Areas: `src/imageviewportprovider.cpp`, `src/imageviewport_p.h`, `src/viewportproviderbridge.cpp`, `src/viewportproviderbridge_p.h`, `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/framepreparation.cpp`, `src/imageviewportrender.cpp`, and provider test support in `tests/imageviewport_provider_test_support.h`.
- Dependencies: Approved provider role-symmetry spec and architecture docs; existing provider test support capable of emitting all result kinds for primary and secondary sessions.
- Stop Conditions: Stop if a secondary provider result needs public semantics that are not covered by `docs/spec/image-viewport.md` or `docs/spec/image-sequence-provider-adapter.md`; update docs only after approval.

## M2: Typed Public Value API And Transition Policy

- Status: Not Started
- Goal: Replace map-shaped public value surfaces with documented typed C++ and QML value types, and expose the full typed page-set transition policy.
- Scope: Add or complete public value types for ranges, rectangles, sizes, points, coordinate results, page geometry, revision tokens, command outcomes where needed, and page-set transition policy; update public properties and invokables to return typed values instead of `QVariantMap`; expose typed fit-mode, spread-direction, and page-gap transitions on `PageSetTransitionPolicy`; stop relying on undocumented transition-policy map keys as the final contract.
- Acceptance Criteria: Public header and QML type metadata expose documented typed value fields for ranges, rectangles, sizes, points, coordinate results, page geometry, revision tokens, command outcomes, and `PageSetTransitionPolicy`, matching `docs/spec/image-viewport-api.md`.
- Acceptance Criteria: C++ callers can use typed results without parsing `QVariantMap`, and QML callers receive registered value types with stable named fields, matching `docs/spec/image-viewport-api.md`.
- Acceptance Criteria: Typed rectangle, size, point, range, coordinate-result, and page-geometry values document and test their units, named fields, equality behavior, and invalid or unavailable values, matching `docs/spec/image-viewport-api.md`.
- Acceptance Criteria: Revision-token tests assert validity and equality/distinctness, not numeric ordering or exact increments, matching `docs/spec/image-viewport-api.md`.
- Acceptance Criteria: Invalid public value objects and malformed transition policy values are rejected before request state, provider work, display state, playback state, or revision tokens change, matching `docs/spec/image-viewport-api.md` and `docs/architecture/subsystem-boundaries.md`.
- Spec References: `docs/spec/image-viewport-api.md` sections `QML Item`, `Public Value Types`, `Public Status`, and `Status And Diagnostics`.
- Architecture References: `docs/architecture/subsystem-boundaries.md` sections `Item Boundary`, `Controller Boundary`, and `Provider Protocol` traceability in `docs/architecture/README.md`.
- Expected Tests/Checks: Public API unit/integration tests in `tests/tst_imageviewport_public_api.cpp` for meta-object property types, typed coordinate helpers, typed ranges, rectangles, sizes, points, page geometry, typed revision tokens, and typed transition-policy fields; C++ install-consumer compatibility tests in `tests/install_consumer/main.cpp`; factory/public type tests in `tests/tst_imagesequence_factory.cpp` where typed construction results are exposed; targeted command `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_public_api|imagesequence_factory|imageviewport_install_consumer'`.
- Likely Code Areas: `src/imageviewport.h`, `src/imageviewport.cpp`, `src/imageviewport_p.h`, `src/imageviewportfacade.cpp`, `src/presentationgeometry.cpp`, `src/presentationgeometry_p.h`, `src/ImageViewportConfig.cmake.in`, `tests/imageviewport_test_support.h`, and `tests/install_consumer/main.cpp`.
- Dependencies: M1 is independent but should remain green; this milestone depends on the approved typed-value API target and may be implemented before or after provider role symmetry if branch management requires.
- Stop Conditions: Stop if Qt 6.5 QML value-type registration cannot support the documented type-stable field access without an alternate public contract; request a spec decision before keeping `QVariantMap` as a compatibility surface.
- Stop Conditions: Do not treat M2 as a release boundary for full page-set transition semantics unless explicit fit-mode, spread-direction, and page-gap policy fields are routed through controller-owned transition handling; if typed policy exposure would otherwise add or preserve item-side canonical transition ownership, combine that part of M2 with M4 or stop for an architecture decision.

## M3: Legacy Presentation API Removal

- Status: Not Started
- Goal: Remove legacy presentation aliases that are explicitly outside the final public API.
- Scope: Add failing public API tests that assert the absence of legacy properties/enums and then remove `FillMode`, horizontal and vertical alignment API, raw scale-style `zoom`, raw `pan`, and related tests or geometry dependencies.
- Acceptance Criteria: Installed public header, QML type metadata, and public meta-object no longer expose `FillMode`, `HorizontalAlignment`, `VerticalAlignment`, `fillMode`, `horizontalAlignment`, `verticalAlignment`, raw `zoom`, or raw `pan`, matching `docs/spec/image-viewport-api.md`.
- Acceptance Criteria: Final presentation behavior remains expressed through `fitMode`, `zoomPercent`, `contentPosition`, spread direction, page gap, rotation, mirroring, and commands, matching `docs/spec/image-viewport.md` and `docs/spec/image-viewport-api.md`.
- Acceptance Criteria: Geometry, coordinate conversion, fit, zoom, and pan behavior continue to satisfy final public tests after legacy aliases are removed, matching `docs/spec/image-viewport.md` display geometry requirements.
- Spec References: `docs/spec/image-viewport-api.md` sections `QML Item`, `Public Value Types`, and `Out-Of-Scope API`; `docs/spec/image-viewport.md` sections `Display Geometry` and `Presentation Controls`.
- Architecture References: `docs/architecture/subsystem-boundaries.md` sections `Caller Policy Boundary`, `Item Boundary`, and `Controller Boundary`; `docs/architecture/rendering.md` section `Geometry`.
- Expected Tests/Checks: Public API unit/integration tests in `tests/tst_imageviewport_public_api.cpp` for removed meta-object entries; presentation behavior tests in `tests/tst_imageviewport_presentation_state.cpp`; geometry/render integration tests in `tests/tst_imageviewport_render_scenegraph.cpp` and `tests/tst_imageviewport_render_commit.cpp`; targeted command `ctest --test-dir build-ninja --output-on-failure -R 'imageviewport_public_api|imageviewport_presentation_state|imageviewport_render_(scenegraph|commit)'`.
- Likely Code Areas: `src/imageviewport.h`, `src/imageviewport_p.h`, `src/imageviewportpresentation.cpp`, `src/presentationgeometry.cpp`, `src/presentationgeometry_p.h`, `src/imageviewportrender.cpp`, `tests/tst_imageviewport_public_api.cpp`, and `tests/tst_imageviewport_presentation_state.cpp`.
- Dependencies: M2 should land first when possible so the public API shape is already moving toward typed final surfaces; otherwise legacy removal can proceed independently if it does not conflict with typed value work.
- Stop Conditions: Stop if any downstream-supported compatibility requirement for legacy aliases is reintroduced; that would contradict the approved target and needs documentation approval before implementation.

## M4: Controller-Owned Presentation State

- Status: Not Started
- Goal: Move canonical presentation state and page-set transition effects behind controller-owned inputs and outputs.
- Scope: Add controller-focused tests for presentation mutations and transition transactions, then move canonical fit mode, effective and manual zoom percent, content position, scan state, rotation, mirroring, spread direction, page gap, virtual spread geometry, and revision effects out of item-private mutable ownership and into controller-owned state or controller-owned collaborators.
- Acceptance Criteria: The item no longer mutates canonical presentation state directly before or after `ViewportController::assignSequence`; page-set transition policy is validated and applied as one controller transaction, matching `docs/architecture/subsystem-boundaries.md`.
- Acceptance Criteria: Standalone spread-direction, page-gap, fit, zoom, pan, scan, rotation, mirror, and reset-view commands enter the controller as presentation mutations and return item-visible change sets, matching `docs/spec/image-viewport-api.md` and `docs/architecture/subsystem-boundaries.md`.
- Acceptance Criteria: Invalid presentation commands and invalid transition policies preserve accepted page set, presentation state, retained display state, queued provider work, playback phase, diagnostics, and request/display revision tokens exactly as documented.
- Acceptance Criteria: Render synchronization consumes controller-authored immutable presentation snapshots and does not read mutable pending request state for readiness publication, matching `docs/architecture/subsystem-boundaries.md` and `docs/architecture/rendering.md`.
- Spec References: `docs/spec/image-viewport.md` sections `Sequence Assignment`, `Display Geometry`, `Requests And Status`, and `Presentation Controls`; `docs/spec/image-viewport-api.md` sections `QML Item`, `Public Value Types`, and `Public Status`.
- Architecture References: `docs/architecture/subsystem-boundaries.md` sections `Controller Boundary`, `Preparation Boundary`, and `Render Boundary`; `docs/architecture/rendering.md` sections `Render Data Flow`, `Geometry`, and `Background`.
- Expected Tests/Checks: New or expanded controller unit tests, likely `tests/tst_viewportcontroller_presentation.cpp` or additions near `tests/tst_viewportcontroller_playback.cpp`; integration tests in `tests/tst_imageviewport_presentation_state.cpp`, `tests/tst_imageviewport_public_api.cpp`, `tests/tst_imageviewport_render_commit.cpp`, `tests/tst_imageviewport_provider_terminal.cpp`, and `tests/tst_imageviewport_provider_requests.cpp`; targeted command `ctest --test-dir build-ninja --output-on-failure -R 'viewportcontroller|imageviewport_presentation_state|imageviewport_public_api|imageviewport_render_commit|imageviewport_provider_(terminal|requests)'`.
- Likely Code Areas: `src/viewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/imageviewport.cpp`, `src/imageviewport_p.h`, `src/imageviewportpresentation.cpp`, `src/imageviewportrender.cpp`, `src/presentationgeometry.cpp`, `src/presentationgeometry_p.h`, `src/renderadapter.cpp`, and private test probes.
- Dependencies: M2 and M3 reduce public API churn before internal ownership changes; M1 should be complete or actively protected by tests because controller state changes can affect provider wait, terminal, and render projections.
- Stop Conditions: Stop if implementing controller-owned presentation state requires a new durable subsystem boundary outside the controller rather than a controller-owned collaborator; update architecture docs and get approval before proceeding.

## M5: Linux/Static Package Contract Conformance

- Status: Not Started
- Goal: Make the installed package and package smoke test match the approved Linux/static QML plugin contract.
- Scope: Verify and adjust install rules, exported target usage, static QML plugin installation, public header exposure, QML type metadata, and install-consumer checks for the supported Linux/static target only.
- Acceptance Criteria: The installed package exposes only public headers, exported CMake package target, QML module metadata, and static QML plugin archive needed by Linux consumers, matching `docs/spec/image-viewport-api.md` and `docs/architecture/build-and-package.md`.
- Acceptance Criteria: Install-consumer validation checks the supported Linux/static package shape and does not add non-Linux or dynamic-plugin portability requirements, matching `docs/architecture/build-and-package.md`.
- Acceptance Criteria: Public headers do not expose private controller, provider transport, render adapter, scene graph, native texture, or private test-probe types, matching `docs/spec/image-viewport-api.md` and `docs/architecture/build-and-package.md`.
- Spec References: `docs/spec/image-viewport-api.md` sections `Package Contract`, `Provider Adapter`, and `Out-Of-Scope API`; `docs/spec/image-sequence-provider-adapter.md` section `Public Contract`.
- Architecture References: `docs/architecture/build-and-package.md`; `docs/architecture/rendering.md` section `Render Data Flow`; `docs/architecture/subsystem-boundaries.md` section `Item Boundary`.
- Expected Tests/Checks: Compatibility/smoke test `ctest --test-dir build-ninja --output-on-failure -R imageviewport_install_consumer`; build smoke `just build`; optional install inspection through the existing install-consumer test only.
- Likely Code Areas: `CMakeLists.txt`, `src/CMakeLists.txt`, `src/ImageViewportConfig.cmake.in`, `tests/install_consumer/run.cmake`, `tests/install_consumer/main.cpp`, and public header installation checks.
- Dependencies: M2 and M3 should be complete so install-consumer tests validate the final public API shape; M5 can also run after M4 if controller ownership changes affect exported symbols or QML metadata.
- Stop Conditions: Stop if a requirement appears for non-Linux package portability, dynamic plugin layouts, or downstream source compatibility beyond the approved Linux/static target.

## M6: Final Conformance Sweep

- Status: Not Started
- Goal: Verify that implementation, tests, package behavior, and documentation all match the approved target state.
- Scope: Run the full local check suite, remove obsolete tests or private probes that only supported transitional implementation, confirm docs do not describe unimplemented approved guarantees, and document any environment-only check gaps in the final implementation report.
- Acceptance Criteria: Full build and test suite passes with the final public API, provider role symmetry, controller-owned presentation state, and Linux/static package contract implemented.
- Acceptance Criteria: Lint passes or all remaining diagnostics are either fixed or explicitly configured when the flagged content is correct, matching `AGENTS.md`.
- Acceptance Criteria: Repository searches find no unintended public `QVariantMap` value contracts, legacy presentation public API symbols including raw `zoom` or `pan`, secondary metadata-only shortcuts, or unsupported package portability work.
- Acceptance Criteria: Specs and architecture remain the source of truth and contain no milestone checklist content; implementation progress belongs only in planning or roadmap documents, matching `AGENTS.md`.
- Spec References: `docs/spec/image-viewport-api.md`, `docs/spec/image-viewport.md`, and `docs/spec/image-sequence-provider-adapter.md`.
- Architecture References: `docs/architecture/subsystem-boundaries.md`, `docs/architecture/provider-protocol.md`, `docs/architecture/rendering.md`, `docs/architecture/playback-state-machine.md`, and `docs/architecture/build-and-package.md`.
- Expected Tests/Checks: Build smoke `just build`; full integration/unit/package test `just test`; lint `just lint`; documentation whitespace check `git diff --check`; targeted repository searches such as `rg -n 'QVariantMap|FillMode|HorizontalAlignment|VerticalAlignment|setZoom|setPan|raw zoom|raw pan|metadata-only' src tests docs`; manual verification that any remaining matches are private implementation details, historical test helpers, or approved docs.
- Likely Code Areas: Cross-cutting cleanup across `src/`, `tests/`, CMake files, and docs only when docs need implementation-status clarification without changing approved behavior.
- Dependencies: M1 through M5 complete.
- Stop Conditions: Stop if any failing check reveals a spec or architecture contradiction rather than an implementation defect; resolve the documentation decision first.

## Open Questions And Stop Conditions

- No product questions are currently blocking this plan because the target-state spec and architecture docs have been approved.
- Assumption to verify during implementation: downstream consumers can absorb the approved legacy API removal in the same release branch. If that is false, stop and request an approved migration or deprecation plan before preserving compatibility aliases.
- Assumption to verify during implementation: Qt 6.5 registered value types can provide the documented typed QML field access without retaining `QVariantMap` as a public data model. If that is false, stop and request a spec decision.
- Assumption to verify during implementation: internal controller-owned collaborators are acceptable as long as canonical presentation ownership remains inside the controller boundary. If a separate durable subsystem must own canonical presentation state, stop and update architecture docs before implementation.
