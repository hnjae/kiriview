# ImageViewport Application Public API Implementation Plan

This plan splits the application-facing viewport policy API work into Codex-sized milestones. It is an execution plan, not a normative contract; each public behavior or durable boundary change must still land first in `docs/spec/` or `docs/architecture/` before implementation.

## Scope

- Add item-owned manual zoom helper APIs so callers can query and calculate manual zoom policy without duplicating viewport geometry, DPR, render-limit, or display-demand rules.
- Add nearest-visible anchor helpers so interaction code can safely anchor margin-originated wheel and double-click zooms against the current visible spread or page domain.
- Add one generic zoom convenience command so callers can zoom by a viewport-owned step calculation through the same validation, command outcome, diagnostics, revision, clamping, and anchor-preservation path as `setZoomPercent(...)`.
- Keep ImageViewport as the owner of fit, zoom, pan, scan, rotation, mirroring, content position, visible rectangles, coordinate conversion, retained display geometry, and command admission.
- Keep application as the owner of source selection, navigation scope, page pairing and fallback, decoding, cache, predecode, refinement policy, same-source transition intent, and gesture interpretation before it becomes a viewport command.
- Do not add URL or file loading, cache ownership, native texture injection, tile rendering, source identity inference, application navigation concepts, or application route semantics to image-viewport.

## Milestone 0: Contract And Boundary Intent

Goal for one Codex session: update and commit the intended public API and durable ownership boundary before any implementation.

Edit `docs/spec/image-viewport-api.md` to declare the final QML and C++ surface for the three API groups. Prefer `minimumManualZoomPercent`, `maximumManualZoomPercent`, `manualZoomStepFactor`, `clampedManualZoomPercent(percent)`, `steppedManualZoomPercent(stepCount)`, `nearestVisibleSpreadPoint(x, y)`, `nearestVisiblePagePoint(role, x, y)`, `nearestVisibleImagePoint(x, y)`, and `zoomByStep(stepCount, anchor)`.

Edit `docs/spec/image-viewport.md` to state observable behavior: helpers are side-effect-free, helper calls do not advance request/display/command revisions, nearest-visible helpers return invalid when no visible presentation geometry exists, manual zoom commands above the command maximum remain invalid rather than silently clamped, and `zoomByStep(...)` uses the same mutation path as `setZoomPercent(...)`.

Declare the zoom-range fallback before implementation. With no accepted request, empty display, loading before first commit, no committed or retained visible geometry, or non-positive item geometry, the item-level `maximumManualZoomPercent` should fall back to the existing public display-demand ceiling so callers can still set zoom preference before content is ready. `minimumManualZoomPercent` must either preserve the existing direct-command rule that every finite positive `setZoomPercent(...)` value below the maximum is valid, or the spec must explicitly call out a deliberate lower-bound behavior change.

Declare step semantics before implementation. `manualZoomStepFactor` is finite and greater than `1`; `steppedManualZoomPercent(stepCount)` uses the current effective public `zoomPercent` as its base, computes `base * pow(manualZoomStepFactor, stepCount)`, and clamps the result into the item-level helper range without rounding. Very large positive or negative step counts saturate through clamping rather than overflowing into invalid helper results. `zoomByStep(0, anchor)` follows the same shared set-zoom path as any other step: it is an accepted no-op only when that path finds no state change, and it may enter `Manual` from a fit-derived mode.

Declare notification semantics for dynamic helper properties before implementation. QML bindings must update when the item-level manual zoom range changes because of visible display identity, retained display identity, item size, DPR, presentation geometry, or public display-demand limit inputs. Milestone 1 may use a dedicated notify signal or an existing signal only if that signal is emitted for every dynamic range input; tests must cover binding refresh, not only property presence.

Declare nearest-visible edge semantics against the existing half-open coordinate domains. Nearest helpers should return valid coordinates inside the same half-open spread or page domains used by ordinary coordinate conversion; when the nearest point lies on an excluded right or bottom edge, the helper returns the nearest representable in-domain coordinate rather than returning the excluded edge or reporting invalid.

Edit `docs/architecture/subsystem-boundaries.md` to state controller ownership for dynamic manual zoom range calculation, nearest-visible anchor calculation, and stepped zoom command admission. This should reinforce that the item projects controller-owned presentation state and does not introduce application geometry state or render-resource injection.

Keep milestone wording out of normative docs. Commit only the contract and architecture intent with a message such as `docs(api): define viewport interaction helper intent`.

Suggested verification: `git diff --check`, then review the docs for absence of implementation progress language.

## Milestone 1: Dynamic Manual Zoom Helper API

Goal for one Codex session: expose side-effect-free manual zoom range and helper calculations to QML, C++, generated module metadata, and installed consumers.

Add focused public API tests in `tests/tst_imageviewport_public_api.cpp` and QML tests in `tests/tst_imageviewport_public_api_qml.cpp` for property and invokable presence, typed return values, default values, and no revision advancement from helper calls.

Add behavior tests in `tests/tst_imageviewport_presentation_state.cpp` or `tests/tst_viewportcontroller_presentation.cpp` for dynamic maximum calculation against item geometry, DPR/render-display rules, retained display geometry, no-visible-geometry fallback, non-positive item geometry fallback, and the existing static `ImageViewportDisplayLimits::maximumManualZoomPercent()` ceiling.

Implement public properties and invokables on `ImageViewport` and `ImageViewportPrivate`. The item should expose C++ methods and QML `Q_PROPERTY`/`Q_INVOKABLE` entries for `minimumManualZoomPercent`, `maximumManualZoomPercent`, `manualZoomStepFactor`, `clampedManualZoomPercent(double)`, and `steppedManualZoomPercent(int)`, with the notification strategy chosen in Milestone 0.

Keep helper calculations non-mutating. Invalid helper inputs should return a deterministic safe value documented by the spec rather than mutating diagnostics; recommended behavior is to clamp non-finite or non-positive input to the minimum for `clampedManualZoomPercent(...)`, while command-style invalidity remains owned by mutating commands.

Introduce one private manual-zoom range calculator instead of scattering the formula across item, controller, and tests. Route calculations through controller or presentation helper ownership rather than duplicating geometry in the item. The maximum must be derived from the currently committed or retained presentation state when visible geometry exists, item geometry, DPR/render limits, and the public display-demand ceiling; if the current build has no concrete backend render ceiling to query, Milestone 0 must document the static display-demand ceiling as that input until a real render-demand source exists.

Update install-consumer coverage in `tests/install_consumer/main.cpp` so an installed C++ consumer and installed QML module can read the properties and call the helper methods.

Suggested verification: `cmake --build build-ninja --target imageviewport_public_api imageviewport_public_api_qml imageviewport_presentation_state viewportcontroller_presentation`, `ctest --test-dir build-ninja -R 'imageviewport_public_api|imageviewport_public_api_qml|imageviewport_presentation_state|viewportcontroller_presentation|imageviewport_install_consumer' --output-on-failure`, and `just format`.

Commit this milestone with a message such as `feat(api): expose dynamic manual zoom helpers`.

## Milestone 2A: Nearest Visible Anchor Core API

Goal for one Codex session: add the core nearest-visible coordinate helpers and enough public behavior to prove their geometry contract without taking on the full transform matrix.

Add public API tests for `nearestVisibleSpreadPoint(double,double)`, `nearestVisiblePagePoint(ImageViewport::PageRole,double,double)`, and the primary-only alias `nearestVisibleImagePoint(double,double)`. Verify the meta-object return type is `CoordinateResult`.

Add behavior tests around primary-only content, a simple two-page spread, page gap rejection versus nearest-page clamping, no visible presentation geometry, non-positive item geometry, missing secondary role, and half-open edge handling. Include tests proving ordinary `itemToSpread(...)` and `itemToPage(...)` still return invalid outside visible bounds.

Implement the core calculations in `PresentationGeometry`, then project them through `ImageViewportPrivate` and `ImageViewport` using the existing controller-owned geometry snapshot path. Add new `ViewportController` facade methods only if the Milestone 0 architecture update explicitly requires them; the controller facade should remain narrow.

For spread helpers, clamp within the currently visible spread rectangle and return invalid only when no visible presentation geometry exists or input is non-finite. For role-scoped page helpers, clamp within that role's visible page rectangle and return invalid when the role is not visible or the presentation has no visible geometry.

Return only coordinates that are valid under the existing half-open domain rules. If clamping lands on a right or bottom edge, move to the nearest representable coordinate inside the visible spread or page domain.

Suggested verification: `cmake --build build-ninja --target imageviewport_public_api imageviewport_presentation_state imageviewport_still`, `ctest --test-dir build-ninja -R 'imageviewport_public_api|imageviewport_presentation_state|imageviewport_still' --output-on-failure`, and `just format`.

Commit this milestone with a message such as `feat(api): add nearest visible anchor helpers`.

## Milestone 2B: Nearest Visible Anchor Transform And Install Coverage

Goal for one Codex session: complete transform coverage and installed-consumer confidence for the nearest-visible helpers.

Add behavior tests for left-to-right and right-to-left placement, rotation, mirroring, panning, retained display geometry, item DPR effects where they already affect geometry, and interactions that begin in viewport margins. Keep these tests focused on observable helper results rather than implementation source patterns.

Update QML and install-consumer checks so installed callers can use the new invokables from headers and module metadata.

Suggested verification: `cmake --build build-ninja --target imageviewport_public_api_qml imageviewport_presentation_state imageviewport_still`, `ctest --test-dir build-ninja -R 'imageviewport_public_api_qml|imageviewport_presentation_state|imageviewport_still|imageviewport_install_consumer' --output-on-failure`, and `just format`.

Commit this milestone with a message such as `test(api): cover nearest visible anchor transforms`.

## Milestone 3: Stepped Zoom Convenience Command

Goal for one Codex session: add a generic zoom command that reuses viewport-owned step math and the existing anchor-preserving manual zoom command path.

Use `zoomByStep(stepCount, anchor)` as the initial public command unless Milestone 0 explicitly chose `zoomByFactor(...)`. The step command composes naturally with `manualZoomStepFactor` and `steppedManualZoomPercent(stepCount)` and keeps arbitrary factor policy inside the viewport.

Add public API tests for QML/C++ method presence, command outcome type, and installed-consumer availability.

Add behavior tests showing positive and negative steps move from the current public zoom percent, zero steps follow the shared set-zoom path, large steps clamp to the dynamic helper range before command admission, invalid anchors return `Invalid`, step-count overflow in factor calculation saturates through clamping without partial mutation, and helper-only calls do not advance revisions.

Implement `zoomByStep(int stepCount, QPointF anchor)` by calculating the target percent through the manual zoom helper, then calling the same controller mutation path used by `setZoomPercent(...)`. Do not duplicate diagnostic or revision logic outside that command path.

Preserve existing `setZoomPercent(...)` semantics: direct over-maximum percent remains `Invalid`, while `zoomByStep(...)` intentionally computes a clamped target inside the manual helper range. If the current fit mode is not `Manual`, the command should use the current effective public `zoomPercent` as the step base and then enter `Manual` via the shared set-zoom path.

Suggested verification: `cmake --build build-ninja --target imageviewport_public_api imageviewport_public_api_commands imageviewport_presentation_state viewportcontroller_presentation`, `ctest --test-dir build-ninja -R 'imageviewport_public_api|imageviewport_public_api_commands|imageviewport_presentation_state|viewportcontroller_presentation|imageviewport_install_consumer' --output-on-failure`, and `just format`.

Commit this milestone with a message such as `feat(api): add stepped zoom command`.

## Milestone 4: Full Public Surface Audit

Goal for one Codex session: close packaging, generated metadata, and regression gaps after the three API groups are implemented.

Run the focused tests from Milestones 1, 2A, 2B, and 3 plus `imageviewport_install_consumer`. Inspect generated installed public header output and QML type metadata if failures suggest a missing export.

Run structural boundary checks that could be affected by new helper ownership: `structural::controllerFacadeBoundary`, `structural::controllerContractHeaderBoundary`, `structural::controllerHelperOwnershipBoundary`, `structural::commandOutcomeBoundary`, and `structural::refactorBaselineInventory`.

Run `just test` if the focused and structural checks pass. Fix only regressions caused by the new API work; leave unrelated failures documented in the final response if they preexist or are environmental.

Check that no new public API exposes source loading, application navigation, cache/predecode ownership, scene graph resources, native textures, tiled rendering, or provider-private types.

Commit any audit fixes with a message such as `test(api): cover viewport helper install surface`.

## Cross-Milestone Completion Criteria

- Public docs describe the intended end state, not current progress or migration phases.
- Every public member is usable from installed C++ headers and QML module metadata.
- Helper APIs are deterministic, side-effect-free, and do not advance request, display, or command revisions.
- Mutating commands use existing command outcome, diagnostic, revision, validation, manual zoom clamping, and anchor-preservation paths.
- Invalid commands do not partially mutate fit mode, manual zoom, effective zoom, content position, display state, request state, diagnostics, or revision tokens.
- Coordinate helpers and nearest-anchor helpers use retained visible geometry when `displayStatus` is `Retained` and return invalid when no visible presentation geometry exists.
- Application concerns remain outside ImageViewport: no URL/file loading, cache ownership, native texture injection, tiling, app navigation, source identity inference, page pairing policy, or same-source decision logic is added.
