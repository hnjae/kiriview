# Implementation Plan: Opened-Collection Page Navigation Performance

This plan splits the PgUp/PgDown performance work into goal-sized milestones for future Codex sessions. It is planning guidance, not the durable architecture contract. When a milestone changes behavior or structure, update the owning `docs/spec/` or `docs/architecture/` document first, commit that intent, add practical failing tests next, commit them, then implement and commit the code.

## Problem Statement

Opened archive and directory collection page navigation already has a fast target-selection path when the page model has a known selection. A predecode hit also skips the source decode path. The remaining slowness during repeated PgUp/PgDown, especially alternating between two pages, is expected to come from presentation and display-resource churn: each navigation still enters the open workflow, promotes the predecoded payload into a new display-store entry, publishes a new provider URL, makes QML reload a provider image, may scale in the provider request, and may schedule raster refinement work that is later discarded.

The end-state goal is that adjacent navigation inside an opened collection behaves like a small page buffer: already prepared pages are selected and displayed by reusing display-ready resources whenever the render bucket still matches, while preserving the current provider-backed Qt Quick architecture and existing navigation semantics.

## Global Constraints

- Do not add, replace, vendor, or patch external dependencies for this work unless the user explicitly approves that dependency change.
- Do not introduce a custom scene graph renderer, visual tile renderer, shader path, framebuffer path, or low-level texture owner as part of this plan.
- Do not change user-visible navigation semantics, page numbering, reading direction, two-page spread rules, video unsupported states, or animation playback behavior while optimizing still-image navigation.
- Treat direct directory listing refreshes as out of scope unless instrumentation proves they are still on the opened-collection hot path.
- Keep each milestone small enough for one `/goal` run and commit each declared intent, test, and implementation layer separately according to `AGENTS.md`.

## Useful Starting Points

- `src/navigation/imagedocumentpagenavigationcontroller.cpp`: opened-collection known-selection adjacent target selection.
- `src/document/imagedocumentnavigationcontroller.cpp`: page navigation runtime plan that schedules adjacent predecode and loads the target URL.
- `src/document/imageloader.cpp`: predecode lookup and predecoded image completion.
- `src/presentation/imagepagesurfacecontroller.cpp`: static display publication, stale retention, display-load acknowledgment, and raster refinement scheduling.
- `src/rendering/displayimagestore.cpp`: provider-entry insertion, pin leases, provider requests, and provider-side downscale.
- `src/qml/DisplayImagePage.qml`: provider image binding, cache policy, synchronous loading, `sourceSize`, and load acknowledgment.
- `docs/architecture/provider-rendering.md`: current durable provider-rendering ownership rules. Milestones 2-4 will likely need architecture intent changes here before code.

## Milestone 1: Confirm The Hot Path And Add Low-Risk Guardrails

### Goal

Prove the opened-collection PgUp/PgDown bottleneck with targeted instrumentation, then make only low-risk hot-path changes that do not alter the display resource ownership model.

### Intent Documents

Update architecture docs only if this milestone adds durable observability or changes ownership rules. Pure debug logging or focused tests do not require spec or architecture changes.

### Work

- Add or extend debug logging around page-navigation target selection, opened-collection candidate snapshot reuse, predecode hit/miss, display-source publication, provider request timing, and raster refinement schedule/accept/drop.
- Make the logs correlate one navigation with one source identity, displayed location, provider entry id, display-source revision, display quality, requested provider size, and refinement demand key.
- Verify that alternating two predecoded archive pages does not reload the collection candidate list and does not run a full source decode.
- Add a focused test where practical for the already-intended fast branch: known-selection adjacent navigation should report an open-page target without starting a candidate listing.
- Add any tiny guardrail only if it is clearly local, such as avoiding duplicate work for a request that already targets the current accepted page and display revision. Do not implement page-buffer reuse in this milestone.

### Acceptance Criteria

- A developer can run KiriView with debug logging and see whether each PgUp/PgDown step is a predecode hit, which display entry/provider URL is published, whether the provider scales, and whether refinement work is started or discarded.
- Alternating two predecoded archive pages is confirmed to spend time after predecode lookup, not in collection listing.
- The codebase has at least one focused regression test for the known-selection adjacent-navigation branch or an equivalent lower-level policy test if GUI-level coverage is impractical.
- No broad display-cache, QML cache, or refinement ownership redesign is included yet.

### Status

Completed. The slice adds focused opened-collection controller coverage for known-selection adjacent navigation without relisting, plus debug logs for adjacent target selection, foreground opened-collection candidate snapshot reuse/listing, foreground predecode hit/miss/rejection, page-navigation runtime load/predecode dispatch, static display-source publication and load acknowledgment, provider request scaling/timing, and raster refinement schedule/duplicate/drop/fail/accept/cancel outcomes. No spec or architecture document changed because this milestone only added diagnostics and a regression guardrail without changing user-visible behavior or durable ownership rules.

### Suggested Verification

- Run the narrow C++ test target added or touched by this milestone.
- Run `devenv tasks run --mode single ci:test:cpp` if C++ navigation, presentation, or provider tests changed.
- Run `devenv tasks run --mode single ci:lint:cpp` if C++ source changed.
- Run `devenv tasks run --mode single ci:lint:qml` only if QML changed.
- For final code changes in this milestone, run `devenv tasks run --mode single ci:test`.

### Suggested `/goal`

Use `docs/implementation-plan.md` Milestone 1 as the active goal. Instrument and verify the opened-collection PgUp/PgDown hot path, add focused coverage for the known-selection fast branch where practical, and keep implementation changes limited to low-risk guardrails.

## Milestone 2: Introduce Reusable Display Handles And A Small Page Buffer

### Goal

Make repeated navigation between already prepared opened-collection pages reuse display-ready resources instead of inserting a fresh display-store entry and publishing a fresh provider URL on every visit.

### Intent Documents

Before code, update `docs/architecture/provider-rendering.md` or add an ADR if the ownership model changes. The architecture must distinguish immutable entry ids that are never recycled from reusable handles that may reacquire the same immutable entry for the same page, source identity, transform, quality, raster bucket, and render constraints.

### Work

- Define a display reuse key that is specific enough to be safe: displayed location or opened-collection scope plus source identity, image-reader transform, original size, raster size or bucket key, quality, preview origin, page role when needed, and render constraints that affect displayed bytes.
- Add a reusable display handle abstraction or lookup/acquire API to `DisplayImageStore`. Reuse must return the same immutable entry only for the same display bytes and compatible metadata.
- Introduce a small opened-collection page display buffer owned at the presentation/page-surface layer or adjacent runtime layer. It should retain the current page and nearby pages that are inside the active navigation/predecode window.
- Teach predecode promotion or static display publication to publish from an existing handle when a buffered handle matches the target page and render bucket.
- Ensure stale-retained current display behavior still works while a target page is pending, and ensure load acknowledgments release only matching pending or retained leases.
- Keep two-page spread behavior explicit: primary and secondary page roles may share immutable image bytes only when leases and role-specific presentation state remain correct.

### Acceptance Criteria

- Alternating between two predecoded archive pages reuses the existing display handles for those two pages while they remain in the page buffer.
- Repeated A/B navigation does not grow display-store entry count or emit monotonically new provider ids for the same unchanged page bucket.
- Cache eviction remains bounded by the existing display-image byte budget and active navigation window.
- Existing semantics for load acknowledgment, stale retention, clear transitions, two-page spread, and source replacement remain covered by tests or unchanged focused assertions.

### Status

Completed. The slice records reusable display-handle ownership in the provider-rendering architecture, adds display-store reuse keys and a `BufferedDisplay` lease kind, and teaches the page-surface owner to keep a bounded two-entry static display buffer. Alternating A/B static page payloads with distinct source identities reuse the original provider entry id while the buffer retains them, same-source refinement or replacement drops older buffered keys, and clear/destruction paths release buffer leases. Focused display-store and page-surface tests cover reusable acquisition, exact-key matching, release-request clearing after reacquisition, A/B/A provider URL reuse, bounded buffer eviction, same-source replacement, and existing load/refinement behavior.

### Suggested Verification

- Add focused unit tests for display-store lookup/acquire and lease release behavior.
- Add focused presentation tests for predecoded A/B/A page navigation reusing handles without leaking pins.
- Run `devenv tasks run --mode single ci:test:cpp`.
- Run `devenv tasks run --mode single ci:lint:cpp`.
- Run `devenv tasks run --mode single ci:test` before finishing the milestone.

### Suggested `/goal`

Use `docs/implementation-plan.md` Milestone 2 as the active goal. First update the provider-rendering architecture intent for reusable immutable display handles, then add tests and implement a small opened-collection display buffer so A/B predecoded page navigation reuses display handles safely.

## Milestone 3: Coalesce And Cache Raster Refinement Work

### Goal

Prevent rapid PgUp/PgDown from repeatedly starting redundant or stale raster refinement work for pages and buckets that are already refined, in flight, or no longer visible.

### Intent Documents

Update `docs/architecture/provider-rendering.md` and, if cancellation ownership changes, `docs/architecture/async-lifecycle.md`. The durable rule should say where refinement demand keys are cached, how in-flight work is coalesced, and how cancellation or stale-demand checks prevent wasted worker work.

### Work

- Cache accepted refinement results by source identity, displayed location, page role when necessary, original size, quality, and raster bucket key.
- Reuse a cached refined bucket before scheduling new refinement when navigating back to a page.
- Coalesce identical in-flight refinement demands so repeated projection updates or A/B/A navigation do not enqueue duplicate worker jobs for the same source and bucket.
- Add a cancellation or start-gate mechanism so outdated refinement work can be skipped before expensive decode/scale begins when the demand is no longer current.
- Keep the existing late-completion acceptance checks. Coalescing and cancellation should reduce wasted work, not replace stale-result safety.
- Consider delaying refinement until the target display source has been acknowledged and remains visible, if instrumentation shows immediate refinement during rapid navigation is a major waste source.

### Acceptance Criteria

- Returning to a page whose refined bucket is still cached displays or promotes that refined bucket without launching another identical refinement job.
- Identical in-flight refinement demands are shared or suppressed.
- Rapid A/B PgUp/PgDown does not accumulate expensive stale refinement work in the global thread pool.
- Stale refinement completion still cannot replace the current display source.

### Suggested Verification

- Add focused tests for refinement demand-key equality, cache hit, in-flight coalescing, and stale completion rejection.
- Run `devenv tasks run --mode single ci:test:cpp`.
- Run `devenv tasks run --mode single ci:lint:cpp`.
- Run `devenv tasks run --mode single ci:test` before finishing the milestone.

### Suggested `/goal`

Use `docs/implementation-plan.md` Milestone 3 as the active goal. Update refinement ownership intent, then implement cached and coalesced raster refinement so rapid opened-collection page navigation stops launching redundant stale refinement work.

## Milestone 4: Reduce QML And Provider Hot-Path Cost

### Goal

Make provider-backed display attachment cheap enough that a reused page-buffer handle can be shown without visible stutter from provider-side scaling, synchronous QML loading, or avoidable texture/cache churn.

### Intent Documents

Update `docs/architecture/provider-rendering.md` before changing provider or QML cache policy. If enabling QML image caching for stable immutable handles, document the freshness key and why it is safe.

### Work

- Move repeated display scaling out of `DisplayImageProvider::requestImage()` for page-buffered entries. Provider requests should usually return an already prepared raster bucket, not perform smooth downscale on the hot path.
- Audit `sourceSizeHint` and raster bucket selection so the displayed bucket matches the QML request without repeated provider-side resampling.
- Evaluate enabling `Image.cache` only for stable immutable handle URLs whose key includes the display bytes and render bucket. Keep cache disabled for dynamic entries that cannot prove freshness.
- Evaluate `asynchronous: true` or a Qt async image-provider path only after provider requests are made cheap and stable-handle caching is defined. Async loading can hide latency, but it should not be the primary fix for repeated CPU scaling.
- Keep load acknowledgment semantics revision-specific and role-specific after any QML cache or async loading change.

### Acceptance Criteria

- Provider request logs for A/B/A predecoded archive navigation show near-zero provider CPU time and no repeated smooth scaling for unchanged buckets.
- QML source changes for buffered pages do not create unnecessary visible stalls.
- Load acknowledgment still releases only the matching current provider URL, revision, source identity, and page role.
- Texture/cache behavior is documented well enough that future changes do not reintroduce stale image display.

### Suggested Verification

- Add focused tests for provider downscale policy where practical.
- Run `devenv tasks run --mode single ci:lint:qml` for QML changes.
- Run `devenv tasks run --mode single ci:test:cpp` for provider or presentation changes.
- Run `devenv tasks run --mode single ci:lint:cpp` for C++ changes.
- Run `devenv tasks run --mode single ci:test` before finishing the milestone.

### Suggested `/goal`

Use `docs/implementation-plan.md` Milestone 4 as the active goal. Update provider-rendering intent for stable-handle QML/provider cache policy, then remove repeated provider-side scaling and reduce QML image reload cost for buffered opened-collection pages.

## End-To-End Completion

The overall work is complete when repeated PgUp/PgDown between two predecoded pages in an opened archive collection uses fast target selection, predecode hits, reusable display handles, cached or coalesced refinement, and cheap provider/QML attachment. A successful run should show no candidate relisting, no full source decode, no unbounded display-entry churn, no duplicate refinement jobs for the same bucket, and no repeated provider-side smooth scaling for unchanged buckets.
