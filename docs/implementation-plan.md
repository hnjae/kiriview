# Performance Architecture Implementation Plan

This plan breaks the next performance architecture changes into milestones that should each fit one Codex `/goal` session. It is a planning document: it does not override current behavior in `spec/` or durable boundaries in `architecture/`. This root-level file exists as the `/goal` queue requested for this work; decision records or milestone-specific planning artifacts created by later sessions should live under `docs/planning/`.

Each milestone should follow the repository intent order when it changes behavior or structure: land architecture or spec intent first, add focused failing coverage when practical, then implement and commit. Pure maintenance inside a milestone may go directly to implementation.

## End-State Documentation Gates

- Do not update `docs/architecture/` or `docs/spec/` merely because this planning document names a future milestone. Those documents describe the intended end state for work being landed, not a queue of possible future implementation phases.
- When a milestone intentionally changes structure or ownership, make the relevant `docs/architecture/` update the first commit of that milestone. The architecture update must describe the durable final boundary for that milestone, not temporary migration steps or deferred acceptance criteria.
- When a milestone changes user-visible behavior, unsupported states, disabled controls, navigation behavior, or thumbnail guarantees, make the relevant `docs/spec/` update before tests and implementation. Internal provider, cache, snapshot, index, or ownership changes do not require a spec update when visible behavior remains unchanged.
- The current plan expects architecture updates for Milestones 1, 4, and 10; Milestone 12 should update architecture only if the load-path identity contract is not already explicit enough; Milestone 15 should update architecture only if Milestone 14 justifies a shared prepared-display cache. The current plan does not require a `docs/spec/` update unless Milestone 10 changes the user-visible offscreen thumbnail expectation or another milestone explicitly changes product behavior.

## Goals

- Make active-navigation thumbnail image delivery match the cheap provider boundary already used by the main display provider.
- Replace repeated value copies of large navigation candidate lists with revisioned immutable snapshots shared across projection, thumbnail, and predecode consumers.
- Make active-navigation thumbnail projection and runtime cost proportional to unchanged snapshot revisions and visible or demanded rows where practical.
- Tighten display reuse and refinement cache identity before any broader prepared-display reuse is measured or implemented.
- Add indexed store primitives where entry counts can grow enough for linear scans to matter.

## Non-Goals

- Do not introduce visual tile scheduling, decoded tile caches, custom scene graph items, custom shaders, or low-level texture ownership.
- Do not merge direct-media predecode and image-document predecode runtimes only to reduce duplication.
- Do not add or replace external dependencies without an explicit user request.
- Do not change user-visible thumbnail eligibility, unsupported states, navigation behavior, or offscreen thumbnail guarantees unless the milestone explicitly updates `docs/spec/` first.
- Do not treat provider-boundary cleanup or store indexing as sufficient to achieve the visible/demanded-row cost goal; that goal depends on the later candidate snapshot and thumbnail runtime milestones.

## Cross-Cutting Acceptance Guards

- Provider-boundary milestones must reconcile every durable architecture contract that mentions provider request scaling, including `docs/architecture/extension-contracts.md`, so future milestones do not inherit contradictory intent.
- Candidate snapshot milestones must distinguish candidate-list source identity, candidate-list revision, direct-media scope generation, public projection revision, and thumbnail navigation generation. These values serve different stale-rejection and reuse purposes and must not be collapsed into one token.
- Performance-sensitive implementation milestones should add focused counters, fake ports, or test seams that prove the intended structural property, such as no by-value candidate return from projection ports, unchanged snapshot revisions skipping full row projection, indexed completion lookup, or background fill inspecting only demanded/windowed rows.
- Display reuse milestones must cover both display-store reuse keys and refinement cache keys before any prepared-display cache measurement or implementation. Architecture must define how the presentation load path preserves displayed-location or opened-collection scope identity before implementation threads that identity through payloads.
- Prepared-display cache architecture must not be added until Milestone 14 records a measured decision to build it. If the decision is negative, Milestones 15 through 18 remain skipped rather than becoming end-state documentation.

## Milestone 1: Thumbnail Provider Boundary Intent

Suggested `/goal`: Update architecture intent so active-navigation thumbnail provider requests are cache-only and thumbnail bucket scaling is complete before image-provider publication.

Scope: Update `docs/architecture/thumbnail-source-adapters.md`, `docs/architecture/provider-rendering.md`, and `docs/architecture/extension-contracts.md` to state that active-navigation thumbnail provider requests return stored bucket images only, do not rescale in `requestImage`, and receive entries already sized for the accepted demand bucket. Reconcile existing provider-request wording so the main display provider, thumbnail provider, and generic extension contract all agree that ordinary cache-only providers return published raster entries instead of performing hot-path requested-size downscale. Clarify that QML may report demand and render projected results, but ordinary provider request size is not a scaling contract.

Acceptance: Architecture docs name the thumbnail provider boundary, bucket ownership, cache freshness expectations, and relation to the existing main display provider boundary. No durable architecture document still permits ordinary provider hot-path downscaling for these cache-only providers. No code changes are required in this milestone.

Verification: Review rendered Markdown or run a narrow docs diff check. Full CI is not required for intent-only documentation.

## Milestone 2: Thumbnail Provider Stored-Bucket Implementation

Suggested `/goal`: Make active-navigation thumbnails publish bucket-sized images and make `ThumbnailImageProvider::requestImage` return stored images without requested-size smooth scaling.

Scope: Add focused failing tests for thumbnail provider requests with a valid smaller `requestedSize`, then remove requested-size scaling from `ThumbnailImageProvider::requestImage`. Add tests around thumbnail generation or runtime publication that clarify whether lookup/generation providers are trusted to return bucket-sized images or whether runtime/store publication validates the accepted bucket maximum edge. Preserve QML visual scaling through Qt Quick `Image` geometry, `smooth`, and `mipmap`; this milestone is primarily a boundary correction and guardrail, not the main visible-row performance win.

Acceptance: Provider requests for existing thumbnail ids return the stored image size regardless of requested size. Published thumbnail images are already compatible with the selected demand bucket by trusted-provider contract or runtime validation. Existing visible thumbnails still render with the same visual fit. Thumbnail cache lookup, generation, and stale-completion rejection remain unchanged.

Verification: Run focused thumbnail provider/store/runtime tests while iterating, then `devenv tasks run --mode single ci:test:cpp` if C++ thumbnail runtime/provider tests are touched. Run `devenv tasks run --mode single ci:lint:cpp` for touched C++.

## Milestone 3: Indexed Thumbnail Image Store

Suggested `/goal`: Replace linear thumbnail image store lookup and eviction bookkeeping with indexed store primitives while preserving retention priority and byte-budget behavior.

Scope: Add tests around insert, lookup, release, priority update, image lookup refreshing LRU, priority update refreshing LRU, budget trimming, stable id behavior, and immediate eviction of a newly inserted entry when the byte budget cannot retain it after trimming. Refactor `ThumbnailImageStore` internals to maintain id-indexed lookup plus explicit eviction metadata. Keep public API stable unless a later milestone needs a metadata-only readiness API.

Acceptance: Store operations no longer require vector scans for ordinary id lookup. Eviction still removes the lowest priority and oldest eligible entry. Byte budget accounting, LRU refresh behavior, release behavior, and shared-store configuration stay unchanged.

Verification: Run focused thumbnail image store tests, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 4: Candidate Snapshot Architecture Intent

Suggested `/goal`: Define a revisioned immutable candidate-list snapshot boundary for direct media, image-document page navigation, opened collections, thumbnails, and predecode consumers.

Scope: Update `docs/architecture/state-ownership.md`, `docs/architecture/workflow-shape.md`, and possibly `docs/architecture/extension-contracts.md` to define typed candidate snapshots with source identity, candidate-list revision, scope generation when applicable, current index facts, count, and shared immutable row storage. Preserve the rule that page navigation owns confirmed image-document snapshots and the document session owns direct-media active-navigation state. Define when each snapshot is invalidated, when consumers may reuse it, and how it differs from public projection revision, direct-media scope generation, and thumbnail navigation generation. Do not put milestone sequencing, migration phases, or acceptance criteria into the architecture documents.

Acceptance: The architecture describes how projection, active-navigation thumbnails, predecode planning, deletion fallback, and opened-collection foreground loading consume confirmed snapshots without keeping independent candidate-list state. The architecture explicitly distinguishes the revision and generation tokens used for stale rejection, public projection, and thumbnail work. The milestone also identifies performance test seams needed by later implementation work.

Verification: Documentation review or narrow docs diff check only.

## Milestone 5: Direct-Media Snapshot For Projection And Predecode

Suggested `/goal`: Move direct-media navigation candidates from repeated projection/predecode value-copy callbacks to a revisioned immutable snapshot.

Scope: Add tests that demonstrate unchanged public navigation projection when a direct-media candidate snapshot is reused. Introduce a direct-media candidate snapshot value or shared immutable storage owned by `DocumentSessionState` or a narrow session navigation owner. Change `DocumentSessionProjectionRuntimePorts::directMediaNavigationCandidates` and media predecode schedule input to consume the snapshot or a const view instead of returning/copying the vector. Explicitly leave direct-media refresh/open result transport and deletion fallback vector migration as non-goals for this milestone unless they must change to keep the new owner coherent.

Acceptance: Public readout, previous/next/open-at-number behavior, and media predecode eligibility remain unchanged. Recomputing public projection no longer copies the full direct-media candidate vector just to build active-navigation thumbnail rows. Tests can detect that the projection port no longer returns candidates by value.

Verification: Run focused session projection, direct-media navigation, and media predecode tests, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 6: Direct-Media Residual Candidate Boundary Cleanup

Suggested `/goal`: Audit and, where still useful, migrate remaining direct-media candidate vector boundaries after projection and predecode no longer copy full lists.

Scope: Inspect direct-media refresh/open results, session state update paths, deletion fallback, and any test fixtures that still require whole-vector copies. Convert remaining hot or durable boundaries to the snapshot model only when it reduces steady-state copies without obscuring ownership. Keep deletion fallback lifecycle owned by the deletion runtime, and do not broaden this milestone into image-document page snapshots.

Acceptance: Any remaining direct-media vector copies are either removed or documented as cold-path/result transport. The snapshot owner remains single and clear. No public behavior changes.

Verification: Run focused direct-media navigation and deletion fallback tests touched by the change, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 7: Image-Document Candidate Snapshot Storage

Suggested `/goal`: Introduce shared immutable page candidate snapshot storage inside image-document page navigation without migrating every downstream consumer yet.

Scope: Add tests for page-navigation candidate snapshot identity, revision, stale refresh rejection, and candidate-list source matching. Replace value-owned candidate snapshots in image-document page navigation model internals with revisioned immutable storage or an equivalent shared snapshot type. Preserve existing public snapshot APIs where needed by current consumers, but make the internal owner capable of handing out shared confirmed snapshots.

Acceptance: Page navigation owns one confirmed candidate snapshot for its active candidate-list source. Pending refreshes and source changes replace or invalidate that snapshot. Existing page selection, adjacent navigation, and page count behavior remain unchanged.

Verification: Run focused image-document page navigation tests, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 8: Image-Document Snapshot Consumers

Suggested `/goal`: Move opened-collection foreground loading and image-document predecode planning to consume confirmed page candidate snapshots when source identity matches.

Scope: Add focused tests around opened-collection candidate reuse, foreground loading, image-document predecode scheduling, and stale-scope rejection. Update foreground loading and predecode inputs to consume the confirmed snapshot introduced by Milestone 7 when available and to fall back to the candidate provider when no fresh snapshot exists. Do not change public session projection or thumbnail row projection in this milestone.

Acceptance: Opened-collection foreground loading and image-document predecode consume the same confirmed candidate snapshot for the matching source. Snapshot lists are invalidated on pending refreshes, deletion fallback list changes, and sibling archive scope changes. No independent downstream candidate-list copies are needed for these steady-state consumers.

Verification: Run focused opened-collection foreground-load and image-document predecode tests, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 9: Active-Navigation Thumbnail Projection Reuse

Suggested `/goal`: Make active-navigation thumbnail row projection reuse candidate snapshot revisions and avoid full row projection when only the active number changes.

Scope: Add tests for unchanged candidate snapshot revision, current-row-only changes, source-kind changes, and stale generation rejection. Change `DocumentSessionProjectionRuntime` and `ActiveNavigationThumbnailRuntime::setRows` integration so unchanged candidate snapshot revisions can update current-row state without rebuilding rows from full candidate lists. Keep QML-facing roles and activation commands unchanged.

Acceptance: Current-row changes emit narrow model updates and do not invoke the full candidate row projector when the snapshot revision is unchanged. Source or row-identity changes still reset thumbnail navigation generation and reject stale thumbnail work. Projection cost for unchanged candidate snapshots is no longer proportional to full candidate count.

Verification: Run focused active-navigation thumbnail projection/model tests with counters or fake projectors, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 10: Thumbnail Demand Windowing Intent

Suggested `/goal`: Define the active-navigation thumbnail runtime's visible/nearby demand window policy before changing background fill behavior.

Scope: Update `docs/architecture/state-ownership.md` and `docs/architecture/thumbnail-source-adapters.md` to define demand expiry, visible versus nearby priority, background fill ordering, and whether full-list background fill is a runtime optimization or a durable product guarantee. Update `docs/spec/navigation.md` only if the user-visible offscreen thumbnail expectation changes from the current best-effort wording.

Acceptance: The policy gives implementation tests a clear success condition for which rows may be inspected, scheduled, retained, or expired after QML reports visible or nearby demand. No code changes are required in this milestone.

Verification: Documentation review or narrow docs diff check only.

## Milestone 11: Active-Navigation Thumbnail Demand Indexing

Suggested `/goal`: Make thumbnail demand lookup, completion lookup, and background fill use indexed row state and the documented visible/nearby demand window.

Scope: Add tests for demand lookup by row identity, completion lookup by source key, stale generation rejection, foreground priority, demand expiry, and background fill ordering. Add row identity and source-key indexes inside `ActiveNavigationThumbnailRuntime`. Replace full scans in demand and completion hot paths. Limit background fill to demanded or nearby rows according to Milestone 10.

Acceptance: Foreground visible demand remains prioritized and stale jobs remain rejected. Completion lookup uses a source-key index. Background jobs do not block foreground work and do not traverse the entire row list on every completion when only a small viewport is active. Tests can detect that background fill inspects only demanded/windowed candidates.

Verification: Run focused active-navigation thumbnail runtime tests, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 12: Display Location Identity Intent

Suggested `/goal`: Tighten architecture intent for display reuse and refinement cache keys so both include real displayed-location or opened-collection scope identity.

Scope: Update `docs/architecture/provider-rendering.md` and `docs/architecture/state-ownership.md` if the existing wording does not explicitly cover the load-path owner contract. State that `StaticDisplayImagePayload` or the presentation load path must preserve enough location/scope identity for page-surface owners to build correct display reuse keys and refinement cache keys. Keep this architecture update focused on the durable identity boundary, not on the implementation sequence for threading the field.

Acceptance: Architecture docs cover display-store reuse keys, refinement cache keys, page role, predecode promotion, decoded foreground load, secondary-page load, and opened-collection identity. No code changes are required in this milestone.

Verification: Documentation review or narrow docs diff check only.

## Milestone 13: Display Location Identity Implementation

Suggested `/goal`: Thread displayed-location or opened-collection scope identity through static display publication and use it in display reuse and refinement cache keys.

Scope: Add tests for same source bytes in different scopes, opened-collection entries, page roles, source identity changes, predecode promotion, decoded foreground load, and secondary-page load. Thread the displayed-location or opened-collection scope identity from existing load inputs into `StaticDisplayImagePayload`, `ImagePresentationStaticImageLoad`, or the narrowest equivalent presentation boundary. Use that identity in `DisplayImageReuseKey` construction and `RasterDisplayRefinementCacheKey` construction.

Acceptance: Reuse can still hit for the same immutable display bytes in the same compatible scope. It cannot cross incompatible opened-collection or displayed-location identities. Page-local refinement cache hits are also scope-correct. Existing same-scope navigation buffering continues to work.

Verification: Run focused display reuse, page-surface, refinement cache, predecode-promotion, and secondary-page tests, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 14: Prepared-Display Cache Decision Point

Suggested `/goal`: Decide, with explicit measurement, whether cross-owner prepared-display reuse is needed beyond page-local refinement caches and display-store reusable handles.

Scope: Add low-risk counters or logs around refinement cache hits/misses, in-flight coalescing hits, display-store reusable hits, predecode hits, repeated decode/refinement for primary versus secondary page roles, and reopened same-scope entries. Record the decision in a concrete planning artifact under `docs/planning/`, including scenarios measured and the threshold used to justify or reject a shared cache. Do not introduce a shared prepared-display cache in this milestone.

Acceptance: The repository records a concrete decision with measured scenarios, counters, and conclusion. If measurements do not justify a shared cache, stop after the planning artifact. If they do justify it, the artifact lists the minimal follow-up milestones to pursue.

Verification: Run only checks touched by instrumentation or docs. If code instrumentation is committed, run the narrow affected C++ tests and lint.

## Milestone 15: Shared Prepared-Display Cache Architecture Intent

Suggested `/goal`: Define the shared prepared-display cache owner only if Milestone 14 justifies implementation.

Scope: Update architecture docs to define the cache owner below document public state and above page-surface resource owners only after Milestone 14 records a measured decision to build this cache. Specify byte budget, identity keys, lease rules, stale rejection, relationship to predecode cache entries, page-surface promotion, display-store publication, and why the existing page-local refinement caches are insufficient. If Milestone 14 rejects a shared cache, do not add this end-state architecture.

Acceptance: Architecture docs define one owner, allowed consumers, forbidden ownership bypasses, and test ownership. No implementation code is required in this milestone.

Verification: Documentation review or narrow docs diff check only.

## Milestone 16: Shared Prepared-Display Cache Primitive

Suggested `/goal`: Implement the shared prepared-display cache primitive and tests without integrating it into page-surface or predecode promotion yet.

Scope: Add tests for cache key identity, byte budget eviction, owner leases, priority or recency behavior, and stale-safe immutable payload retrieval. Implement the C++ cache primitive and keep runtime objects in C++; Rust may compute policy from plain metadata only.

Acceptance: The cache primitive can store, find, lease, and evict immutable prepared display payloads under the architecture-defined keys without publishing provider entries or mutating public state.

Verification: Run focused prepared-display cache tests, then `devenv tasks run --mode single ci:test:cpp` and `devenv tasks run --mode single ci:lint:cpp`.

## Milestone 17: Shared Prepared-Display Page-Surface Integration

Suggested `/goal`: Integrate the shared prepared-display cache with page-surface refinement and display-source promotion.

Scope: Add tests for page-surface cache hit promotion, stale demand rejection, display-source revision handling, provider load acknowledgment, page role isolation, and retained current display behavior. Wire page-surface refinement completions and cache hits through the shared cache while preserving page-surface ownership of display-source publication.

Acceptance: Repeated compatible page-surface refinement requests can reuse one immutable prepared payload without bypassing display-source revision, provider load acknowledgment, page role, or stale-demand checks.

Verification: Run focused page-surface/refinement/display-source tests, then `devenv tasks run --mode single ci:test:cpp`, `devenv tasks run --mode single ci:lint:cpp`, and `devenv tasks run --mode single ci:test` before finishing because this touches shared display ownership.

## Milestone 18: Shared Prepared-Display Predecode Integration

Suggested `/goal`: Integrate the shared prepared-display cache with predecode promotion only after page-surface integration is correct.

Scope: Add tests for predecode cache interaction, predecode promotion, byte-budget behavior across caches, stale schedule rejection, and source identity isolation. Wire predecode-produced provider-ready display payloads into the shared prepared-display cache when the architecture allows it.

Acceptance: Predecode and page-surface consumers can reuse compatible prepared display payloads without duplicating ownership, bypassing predecode lifecycle, or changing public session state. Existing predecode cancellation, power-saver, and schedule generation behavior remain unchanged.

Verification: Run focused predecode/page-surface/cache tests, then `devenv tasks run --mode single ci:test:cpp`, `devenv tasks run --mode single ci:lint:cpp`, and `devenv tasks run --mode single ci:test`.

## Milestone 19: Indexed Main Display Store

Suggested `/goal`: Add indexed lookup and eviction primitives to `DisplayImageStore` without changing provider semantics.

Scope: Add tests for insert, reusable acquire, all pin kinds, release-request handling, release-request clearing on reusable reacquire, byte-budget trimming, provider lookup, stale-retained entries, pending-load retention, frame-retention behavior, buffered display entries, and index consistency after eviction. Refactor store internals from vector-only scans to id and reuse-key indexes with eviction metadata. Keep provider ids generated by the runtime store and keep provider requests returning stored rasters only.

Acceptance: Main display provider semantics remain unchanged. Ordinary id lookup and reusable-entry lookup no longer scan all entries. Pin lease accounting, pending-load retention, stale-retained entries, frame retention, buffered display entries, release-request behavior, and byte-budget behavior remain correct.

Verification: Run focused display image store/provider tests, then `devenv tasks run --mode single ci:test:cpp`, `devenv tasks run --mode single ci:lint:cpp`, and `devenv tasks run --mode single ci:test` before finishing because this store is shared by display, animation, refinement, and predecode promotion paths.

## Recommended Execution Order

1. Milestones 1 through 3 first, because thumbnail provider boundary cleanup and thumbnail store indexing are clear and isolated, while the broader visible/demanded-row goal waits for later milestones.
2. Milestones 4 through 11 next, because candidate snapshots, projection reuse, and thumbnail demand indexing reinforce each other and should avoid reworking the same session interfaces twice.
3. Milestones 12 and 13 before any broader display cache measurement, because reuse and refinement identity must be correct before measurements are meaningful.
4. Milestone 14 as a decision gate. Run Milestones 15 through 18 only if the measured decision justifies a shared prepared-display cache.
5. Milestone 19 last, unless profiling shows display-store lookup itself has become a bottleneck before the higher-priority work is complete.

## Per-Session Closeout Checklist

- Confirm whether the milestone changed user-visible behavior, architecture structure, tests, or code, and commit the applicable layers separately. Do not update `docs/architecture/` or `docs/spec/` just to restate future work that remains outside the milestone's end state.
- Keep commits scoped and conventional, for example `docs(architecture): define thumbnail provider boundary`, `test(session): cover thumbnail row revision reuse`, or `perf(rendering): index display image store lookups`.
- Add `Co-authored-by: Codex <noreply@openai.com>` to commits materially authored by Codex.
- Report focused checks run and any skipped final checks.
