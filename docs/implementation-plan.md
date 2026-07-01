# Implementation Plan

This document is an execution queue for moving the implementation toward the end state defined by `docs/spec/**` and `docs/architecture/**`. It is not a source of truth. If this plan conflicts with those authoritative documents, the plan is wrong and must be corrected before implementation continues.

## Conformance Audit

| Area | Current conformance | Authoritative references | Gap to close |
| --- | --- | --- | --- |
| Public viewport behavior | Mostly conforming at the contract level according to `DESIGN_REVIEW_CORRECT_END_STATE.md`; the remaining risk is structural rather than a known public behavior rewrite. | `docs/spec/image-viewport.md` sections `Sequence Assignment`, `Requests And Status`, `Playback`; `docs/spec/image-viewport-api.md` sections `Public Status`, `Command Outcomes`, `Status And Diagnostics`. | Preserve all public state, retained-display, command, playback, diagnostic, revision, and unavailable-value guarantees while moving ownership into the controller. |
| Display request identity | Partially conforming. Request and payload ids exist, but the complete accepted-request model is still distributed across request records, controller-selected state, and item-private storage. | `docs/architecture/subsystem-boundaries.md` section `Controller Boundary`; `docs/architecture/provider-protocol.md` section `Requests`; `docs/spec/image-viewport.md` sections `Sequence Assignment`, `Requests And Status`, `Playback`. | Make accepted display requests first-class controller-owned records with generation id, request id, origin, target kind, public target, resolved frame identity, linked provider token, prepared payload id, terminal scope, and diagnostics. |
| Controller boundary | Partially conforming. Commands have moved toward controller transition methods, but the controller still depends on item-private state ownership and external-effect plumbing is not fully port-shaped. | `docs/architecture/subsystem-boundaries.md` sections `Item Boundary`, `Controller Boundary`; `docs/architecture/playback-state-machine.md` sections `States`, `Requests`, `Timing`. | Make `ImageViewportPrivate` a thin Qt adapter that forwards commands/geometry, emits QML notifications, schedules rendering/timers, and applies controller change/effect sets. |
| Provider protocol ownership | Partially conforming. Typed provider transport events and effects exist, but provider generation state, session creation, callback adaptation, token scope, queue state, and cleanup policy are not yet owned by one controller-side provider generation boundary. | `docs/architecture/provider-protocol.md` sections `Sessions`, `Requests`, `Shutdown`; `docs/spec/image-sequence-provider-adapter.md` sections `Public Session API`, `Provider Responsibilities`, `Failure Semantics`. | Make provider bridge transport-only; keep session identity, token matching, queued request state, stale-result filtering, cancellation policy, and metadata readiness inside controller-owned provider generation state. |
| Preparation and render boundary | Partially conforming. Prepared-payload identities and render acknowledgements exist, but render failure identity and some request/display consequences still cross boundaries. | `docs/architecture/subsystem-boundaries.md` sections `Preparation Boundary`, `Render Boundary`; `docs/architecture/rendering.md` sections `Render Data Flow`, `Geometry`; `docs/spec/image-viewport-api.md` section `Public Limits`. | Keep admission and prepared payload creation in preparation, keep scene graph work in render, and route render commit/failure acknowledgements through controller identity gates. |
| Playback and time | Partially conforming. Playback behavior is specified, but controller behavior remains tied to Qt item/timer scaffolding in tests and production plumbing. | `docs/architecture/playback-state-machine.md` section `Timing`; `docs/spec/image-viewport.md` section `Playback`; `docs/spec/image-viewport-api.md` sections `PlaybackPhase`, `Command Outcomes`. | Introduce an explicit clock/timer port so controller tests can advance deterministic time without `ImageViewport`, `QTimer`, scene graph objects, or event-loop draining. |
| Modularity and removability | Partially conforming. Provider, playback, render, and notification behavior are still interwoven through the private item even where typed boundaries now exist. | `docs/architecture/subsystem-boundaries.md`; `docs/architecture/provider-protocol.md`; `docs/architecture/rendering.md`. | Make provider support, render support, playback timing, preparation, and QML notification application replaceable through documented boundaries without changing source-neutral controller transitions. |

## Milestone Plan

### M0: Baseline Conformance Harness

- Status: Completed 2026-07-01.
- Priority: P1.
- Goal: Establish a behavioral and architectural regression harness before ownership changes begin.
- Dependencies: None.
- Relevant spec references: `docs/spec/image-viewport.md` sections `Sequence Assignment`, `Requests And Status`, `Playback`; `docs/spec/image-viewport-api.md` sections `Public Status`, `Command Outcomes`, `Public Limits`, `Status And Diagnostics`; `docs/spec/image-sequence-provider-adapter.md` sections `Public Session API`, `Provider Responsibilities`, `Failure Semantics`.
- Relevant architecture references: `docs/architecture/subsystem-boundaries.md` sections `Item Boundary`, `Controller Boundary`, `Preparation Boundary`, `Render Boundary`; `docs/architecture/provider-protocol.md` sections `Sessions`, `Requests`, `Payload Admission`, `Shutdown`; `docs/architecture/playback-state-machine.md`; `docs/architecture/rendering.md`.
- Likely code areas: Existing viewport public tests, provider adapter tests, render tests, controller-adjacent private tests, test utility/probe wiring mentioned by `DESIGN_REVIEW_CORRECT_END_STATE.md`.
- Acceptance criteria: Existing public behavior tests are green before refactoring; a conformance checklist exists in test names or test comments for retained display, command failure preservation, request/display revision changes, provider stale results, render acknowledgement identity, stop restoration, metadata-bound target selection, and playback waiting; no test weakens public behavior to match current implementation shortcuts.
- Expected tests/checks: Run the repository lint and test commands used for CI; run focused viewport/provider/render tests if the full suite is too slow during iteration; record any currently failing unrelated tests before making ownership changes.
- Completion evidence: Re-read the M0 spec and architecture references, added a baseline conformance checklist comment in `tests/CMakeLists.txt`, strengthened `providerFrameFailureRetainsDisplayAndClearsOnSeek()` to prove invalid command diagnostics preserve a retained failed replacement without changing request or display revisions, and verified the focused provider terminal test, full CTest suite, `devenv shell -- just test`, and `devenv shell -- just lint` all pass.
- Check note: `devenv tasks run --mode single ci:test` and `devenv tasks run --mode single ci:lint` are not defined in the current repo task graph; the checked-in `just test` and `just lint` recipes were used as the repository CI-style entry points.
- Stop conditions: Stop if a proposed baseline test contradicts `docs/spec/**` or `docs/architecture/**`; stop if a test encodes current incomplete implementation as desired behavior; stop if the baseline requires editing authoritative docs.

### M1: First-Class Controller DisplayRequest Model

- Status: Not started.
- Priority: P1.
- Goal: Replace distributed accepted-request facts with an explicit controller-owned `DisplayRequest` model and a single identity gate for provider, preparation, and render results.
- Dependencies: M0 baseline conformance harness.
- Relevant spec references: `docs/spec/image-viewport.md` sections `Sequence Assignment`, `Requests And Status`, `Playback`; `docs/spec/image-viewport-api.md` sections `Position Observations`, `Command Outcomes`, `Status And Diagnostics`.
- Relevant architecture references: `docs/architecture/subsystem-boundaries.md` section `Controller Boundary`; `docs/architecture/provider-protocol.md` section `Requests`; `docs/architecture/rendering.md` section `Render Data Flow`.
- Likely code areas: `src/imageviewportstate_p.h`, `src/viewportcontroller_p.h`, `src/viewportcontroller.cpp`, provider frame dispatch and callback handling, render acknowledgement handling, display-state snapshot helpers.
- Acceptance criteria: Every accepted display request has a monotonic request id scoped to the active sequence generation; request origin is recorded as initial display, metadata-bound selection, explicit seek, or playback advancement; public target and resolved frame identity are represented separately; linked provider token and prepared payload identity are attached to the request that owns them; `stop()` restores or creates request identity from controller request records rather than reconstructing it from frame/position-only fields; late provider, preparation, render commit, and render failure results are rejected through one request/payload identity gate.
- Expected tests/checks: Focused tests for assignment, explicit frame seek, position seek to `totalDuration`, metadata-bound frame `0`, `play()` before metadata, `stop()` restoration, provider late frame result, stale render acknowledgement, and retained display after failed replacement; full viewport/provider/render regression suite after the model is in use.
- Stop conditions: Stop if any path can publish `Ready`, `Unsupported`, `Error`, retained display, or playback state without passing the request identity gate; stop if display identity and retained payload identity become conflated; stop if public requested position for `seekToPosition(totalDuration)` is collapsed into final frame start position.

### M2: Controller-Owned State Boundary

- Status: Not started.
- Priority: P1.
- Goal: Move request, display, provider-generation, playback, diagnostics, revision, and stale-result ownership out of `ImageViewportPrivate` and into `ViewportController` or a controller-owned state object.
- Dependencies: M1 first-class display request model.
- Relevant spec references: `docs/spec/image-viewport-api.md` sections `Public Status`, `Command Outcomes`, `Status And Diagnostics`; `docs/spec/image-viewport.md` sections `Requests And Status`, `Presentation Controls`.
- Relevant architecture references: `docs/architecture/subsystem-boundaries.md` sections `Item Boundary`, `Controller Boundary`; `docs/architecture/playback-state-machine.md` sections `States`, `Requests`.
- Likely code areas: `src/imageviewport_p.h`, `src/imageviewport.cpp`, `src/imageviewportcontroller.cpp`, `src/viewportcontroller_p.h`, `src/viewportcontroller.cpp`, state structs, notification/change-set application code.
- Acceptance criteria: `ViewportController` no longer stores or requires broad mutable access to `ImageViewportPrivate`; `ImageViewportPrivate` forwards public commands and geometry changes, applies controller change/effect sets, emits QML notifications, schedules render updates, and synchronizes timers; controller transition methods own request/display/provider/playback state mutation and revision decisions; QML notification emission remains outside the controller; presentation properties stay item-facing but their request/display consequences enter through controller-defined inputs.
- Expected tests/checks: Pure or near-pure controller tests for command outcomes, invalid/unsupported command preservation, request/display revision decisions, retained display transitions, `resetView()`, and geometry-driven render waiting; existing QML-facing tests for property notifications and public API behavior.
- Stop conditions: Stop if the controller emits QML signals, calls scene graph update entry points, or directly invokes provider transport; stop if item code can mutate request status, request reason, playback phase, display identity, provider tokens, or diagnostics outside controller change-set application.

### M3: Provider Generation Ownership and Transport-Only Bridge

- Status: Not started.
- Priority: P1.
- Goal: Make provider generation state controller-owned and reduce the provider bridge to session transport plus normalized event delivery.
- Dependencies: M1 request identities and M2 controller-owned state boundary.
- Relevant spec references: `docs/spec/image-sequence-provider-adapter.md` sections `Public Contract`, `Public Session API`, `Provider Responsibilities`, `Failure Semantics`; `docs/spec/image-viewport-api.md` section `Provider Adapter`.
- Relevant architecture references: `docs/architecture/provider-protocol.md` sections `Sessions`, `Requests`, `Payload Admission`, `Shutdown`; `docs/architecture/subsystem-boundaries.md` section `Controller Boundary`.
- Likely code areas: `src/viewportproviderbridge_p.h`, `src/viewportproviderbridge.cpp`, provider event adaptation code, provider state structs, provider command effect application, session open/close paths.
- Acceptance criteria: Provider active tokens, metadata token, playback token links, queued target state, metadata readiness, session identity, session serial, token allocation, cancellation policy, and stale-result filtering are inaccessible outside the controller/provider-generation owner; provider bridge opens, closes, sends commands, cancels commands, normalizes callbacks, tags events with session identity, and does not decide public request state; session-open failure and successful session-open initial command selection enter controller events before item notifications or transport command delivery; provider callbacks cannot emit QML signals or mutate display/request/playback state directly.
- Expected tests/checks: Provider bridge tests for threading contract, callback affinity, session identity tagging, best-effort cancel/close, late-result suppression, and cause-less compatibility unsupported classification; controller provider-generation tests for metadata ready, metadata failure, provider waiting/progress, frame ready, unsupported operation, unsupported payload, provider failure, cancellation, end-of-sequence, queued frame dispatch, token allocation failure, and closed-session late results.
- Stop conditions: Stop if transport code inspects active tokens to infer status/reason, mutates request/display/provider state, or owns cleanup policy; stop if provider session close blocks clear, replacement, item destruction, or generation-terminal failure; stop if compatibility provider APIs are removed without an authoritative spec change.

### M4: Preparation, Render Acknowledgement, and Render Failure Identity

- Status: Not started.
- Priority: P1.
- Goal: Finish the admission and render boundaries so preparation owns admitted payloads and render reports only identity-keyed commit/failure acknowledgements.
- Dependencies: M1 request/payload identity and M2 controller-owned state boundary.
- Relevant spec references: `docs/spec/image-viewport-api.md` sections `Public Limits`, `Frames And Timed Lists`; `docs/spec/image-viewport.md` sections `Display Geometry`, `Requests And Status`; `docs/spec/image-sequence-provider-adapter.md` section `Provider Responsibilities`.
- Relevant architecture references: `docs/architecture/subsystem-boundaries.md` sections `Preparation Boundary`, `Render Boundary`; `docs/architecture/rendering.md` sections `Render Data Flow`, `Geometry`, `Background`.
- Likely code areas: `src/framepreparation_p.h`, `src/framepreparation.cpp`, render adapter input/output types, render synchronization code, render failure handling, display-state pending payload and retained display code.
- Acceptance criteria: Metadata, construction-time known facts, and frame envelopes pass through structured admission results with stable public status/reason projection and bounded diagnostics; prepared payload records carry the admitted payload identity and upload-ready data before controller publication or render snapshotting; render input consumes prepared payload plus presentation mapping and does not reinterpret request state; render commit/failure acknowledgements are explicit controller inputs keyed by generation, request, and prepared-payload identity; backend capacity failures for admitted payloads map to `Error` with `RenderFailure`; payload rejections preserve the documented unsupported/error distinction.
- Expected tests/checks: Preparation tests for malformed metadata, construction-time contradiction, public-limit violations, missing byte-size metadata, logical-size mismatch, resolved-frame mismatch, orientation normalization, and backend admission failure classification; render tests for commit/failure stale identity rejection, non-positive item geometry render waiting, retained display, background-only updates, mirroring, fill modes, and half-open coordinate behavior.
- Stop conditions: Stop if render code publishes request readiness or display ownership without controller acknowledgement; stop if preparation depends on scene graph objects; stop if public-limit admission and backend capacity failure are collapsed into the same status/reason.

### M5: Playback Clock Port and Pure Controller Coverage

- Status: Not started.
- Priority: P1.
- Goal: Make playback timing deterministic through an explicit clock/timer port and move core playback coverage to controller-level tests.
- Dependencies: M2 controller-owned state boundary; M3 provider effects for playback-backed provider requests where applicable.
- Relevant spec references: `docs/spec/image-viewport.md` section `Playback`; `docs/spec/image-viewport-api.md` sections `PlaybackPhase`, `Position Observations`, `Command Outcomes`.
- Relevant architecture references: `docs/architecture/playback-state-machine.md` sections `States`, `Requests`, `Timing`; `docs/architecture/subsystem-boundaries.md` section `Controller Boundary`.
- Likely code areas: Playback timer scheduling, elapsed time capture, controller playback advancement input, controller tests, private test probes.
- Acceptance criteria: Controller playback transitions accept explicit elapsed/clock inputs and return timer/effect changes; production implements the clock/timer port with Qt timers; tests use a fake monotonic clock; playback waiting on metadata, provider work, request queueing, upload pending, non-positive geometry, or render commit does not accumulate catch-up time; `pause()`, `stop()`, explicit seek while playing, loop wrap, play-once terminal final-frame promotion, and unsupported capability resolution are covered without constructing `ImageViewport`, `QQuickWindow`, `QSGNode`, `QTimer`, or relying on `QCoreApplication::processEvents()`.
- Expected tests/checks: Pure controller playback tests for play before metadata, metadata resolves playable/unplayable, frame and position seek while playing, pause during pending request, stop restoration, loop wrap, play-once final frame pending/promotable, zero-size geometry waiting, stale playback-token results, and invalid/unsupported commands preserving playback phase.
- Stop conditions: Stop if controller correctness depends on wall-clock sleeps or event-loop draining; stop if playback advancement can skip over blocked display requests; stop if tests require private item probes for core state-machine behavior after controller coverage exists.

### M6: Boundary Cleanup and Feature Modularity Gate

- Status: Not started.
- Priority: P2.
- Goal: Remove incomplete wrapper patterns after ownership has moved, then verify provider, render, preparation, playback, and item adapter boundaries are independently replaceable.
- Dependencies: M1 through M5.
- Relevant spec references: `docs/spec/image-viewport-api.md` section `Out-Of-Scope API`; `docs/spec/image-viewport.md` section `Non-Goals`; provider compatibility guarantees in `docs/spec/image-sequence-provider-adapter.md`.
- Relevant architecture references: `docs/architecture/subsystem-boundaries.md`; `docs/architecture/provider-protocol.md`; `docs/architecture/rendering.md`.
- Likely code areas: Dead forwarding methods, compatibility aliases that no longer serve installed APIs, private probes, item-private adapter methods, provider bridge client surface, controller transition API, test scaffolding.
- Acceptance criteria: `ViewportController` exposes non-delegating transition/event methods; `ViewportProviderBridge` communicates through typed transport events/effects only; `FramePreparation` returns admission/prepared-payload results rather than boolean-style helper outcomes; render adapter replacement does not require edits to command or playback logic; provider-backed support is isolated behind provider-event and provider-command effect interfaces; QML notification emission remains in `ImageViewportPrivate`; private probe compilation is no longer required for core controller coverage.
- Expected tests/checks: Full CI lint and test suite; targeted compile check without private probe-only controller behavior dependencies if the build supports it; provider-disabled or fake-provider controller tests; render adapter fake tests plus real render smoke tests.
- Stop conditions: Stop if cleanup removes supported compatibility provider APIs, changes public API names/defaults, introduces native texture/tiled/region/progress/color-management behavior, or collapses retained display semantics; stop if broad file moves obscure ownership changes without reducing coupling.

## Global Dependencies

- M0 must precede all behavior-preserving refactors.
- M1 should precede provider, render, and playback ownership work because those boundaries depend on stable request and prepared-payload identity.
- M2 can proceed incrementally after M1 but must land before final provider-generation ownership and pure controller playback coverage are considered complete.
- M3 and M4 may proceed in parallel after M1 if they preserve the same controller-owned identity model, but they must converge before M6 cleanup.
- M5 depends on controller-owned state and provider effects enough to test playback without item/provider/render scaffolding.

## Global Stop Conditions

- Stop if implementation work reveals a conflict between this plan and `docs/spec/**` or `docs/architecture/**`; correct this plan or the implementation approach, not the authoritative docs.
- Stop if a change weakens public guarantees for retained display, unsupported/error failure scope, command outcome preservation, request/display revision behavior, provider token scoping, playback waiting, or render waiting.
- Stop if a milestone requires adding out-of-scope product behavior such as implicit URL loading, native texture payloads, tiled loading, region loading, numeric provider progress, full color-management policy, or scene graph resource injection.
- Stop if a refactor removes installed compatibility provider APIs before a separate authoritative spec change.
- Stop if tests must be rewritten to assert current implementation structure instead of end-state behavior and architecture.
