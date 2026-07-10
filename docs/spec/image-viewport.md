# ImageViewport Behavior

`ImageViewport` is a Qt Quick viewport presentation engine for caller-supplied image sequences and page roles. It consumes accepted `ImageSequence` page sources, presents a primary page and optional secondary page as one viewport presentation, exposes coherent snapshot state to QML and C++ callers, owns rendering for the accepted presentation target, and leaves source lookup, navigation scope, page selection, page pairing and fallback policy, source identity, decoding, caching, display-store lifetime, predecode policy, and refinement policy to application code, factories, and provider adapters.

## Sequence Assignment

The viewport is a sequence and presentation-target consumer, not a URL loader or QML image-provider presenter. It must not interpret strings, file paths, URLs, provider URLs, image-provider ids, archives, byte buffers, JavaScript objects, raw provider objects, load acknowledgements, or arbitrary variants as image sources.

`setPresentationTarget(target, policy)` is the canonical atomic mutating API for presentation-target and spread replacement. A caller that intends a two-page spread supplies both roles in one command. A secondary role without an accepted primary role is not a non-empty presentation and is not addressable for playback or seek commands.

A valid non-empty presentation-target replacement creates a new accepted sequence generation and an initial accepted display request for each present role. Complete validated construction-time metadata may select frame `0` immediately. Unknown or partial provider metadata leaves the target unknown until validated metadata can create a metadata-bound request for the still-active initial request.

Sequence metadata defines a source logical coordinate space per role. Replacement or refinement payloads for the same target may change delivered raster detail without changing this logical coordinate space. If metadata changes the logical source size, the change is a new source contract for that role and the viewport must recompute fit, zoom limits, pan bounds, visible rects, and coordinate conversion from that accepted logical size.

Presentation-target replacement is a viewport-owned transition transaction. The viewport validates the whole command before applying any state, accepts the new presentation target, updates target presentation state from transition intent, chooses retained or empty visible display, stops or preserves playback according to the accepted rules, and publishes ready display ownership only through a complete target-spread commit.

Retained display content is a visual fallback only. `state.request` describes the accepted replacement; `state.presentation` describes the accepted target presentation; `state.display` describes whether visible pixels are empty, ready for the accepted request, or retained from an earlier request. Retained display geometry and coordinate helpers use the displayed presentation identified by `state.display.displayedPresentationRevision`, not the newer accepted target presentation. Retained pixels never satisfy accepted-target readiness, capability checks, playback support, command admission, or diagnostics for the newer accepted request.

Same-target refinement may replace the visible payload detail for an already accepted logical source without changing the selected target or source logical size. Caller-supplied refinement is marked with `PresentationTargetTransitionPolicy.replacementIntent: SameTargetRefinement`; provider-supplied refinement is admitted only through a frame-ready payload whose demand revision still matches the active demand. Refinement preserves fit mode, manual zoom percent, scan state, content position where valid, role placement, playback policy, source coordinates, page bounds, and 100% zoom. A logical source-size change is a new source contract and must not be treated as same-target refinement.

## Page Roles And Spread Presentation

The viewport presents one accepted presentation target at a time. A presentation target has a required primary role and an optional secondary role. The caller decides which source page belongs to each role and whether a target spread should fall back to a single page.

Provider-backed primary and secondary roles support the same public provider request and event channel. Role selection changes viewport aggregation and page placement; it does not restrict which provider event kinds are valid.

When both roles are supplied in one accepted presentation-target command, the viewport presents them as one virtual spread canvas. Reading direction controls page placement, and page gap participates in spread bounds.

A target spread with a secondary role commits only after all required role payloads are validated and render-committed. The viewport must not expose a partially prepared target spread as ready and must not publish secondary-less geometry for a target that includes a secondary role. During replacement, `state.request.acceptedRoleSet`, `state.display.displayedRoleSet`, `state.display.targetRoleSet`, and `state.display.phase` identify whether callers are observing `PreviousActive`, `TransitioningPlaceholder`, or `CommittedActive` display-phase content.

Terminal spread projection is deterministic. Loading remains loading while any required role is pending and no required role has failed terminally. Terminal error takes precedence over unsupported. When terminal status ties, primary role reason and diagnostics take precedence; when only one role has the winning status, that role supplies the aggregate reason and diagnostics.

## Snapshot State

The public `state` snapshot separates accepted request, visible display, presentation preferences, role metadata, role geometry, diagnostics, and revisions. The canonical snapshot schema is defined in [ImageViewport State](image-viewport-state.md). Every snapshot is internally coherent: callers never need to correlate separately emitted flat properties to understand one viewport state.

Request readiness and visible presentation readiness are separate observable facts. `state.request.status == Ready` means the accepted presentation target is ready. `state.display.status == Ready` means visible pixels belong to that accepted presentation target. `state.display.status == Retained` means visible pixels are inspectable but belong to an earlier request while the accepted presentation target is loading, unsupported, or failed.

Role snapshots expose requested and displayed frame identity, requested and displayed position, metadata facts, capability support, displayed source logical size, displayed payload raster size, displayed payload quality and exactness, current-for-demand state, page rectangles, item rectangles, and visible page rectangles for each role. Role display fields describe visible pixels and carry ownership flags that identify whether those pixels belong to the accepted presentation target or are retained from an earlier generation. Unavailable role fields use invalid sentinel values instead of guessed values.

Revision tokens advance only for their ownership domains. Request revisions advance for accepted request observations and request diagnostics. Display revisions advance for display ownership, visible geometry, retained-display transitions, and presentation effects that change visible display observations. Presentation revisions advance for canonical presentation preference changes. Command revisions advance for command diagnostics and command-result-visible effects. Snapshot revision advances whenever any snapshot field changes.

## Display Geometry

The viewport owns fit mode, effective zoom percent, manual zoom percent, manual zoom range, content size, content position, maximum content position, pannability, viewport frame, visible spread rectangle, visible page rectangles, per-page item rectangles, coordinate conversion, scan position, display rotation, mirror flags, spread direction, page gap, background, smoothing, mipmap, and looping preference.

Public zoom is physical-pixel-aware. At `zoomPercent: 100`, one source logical pixel in the displayed page or virtual spread maps to one physical display pixel after device-pixel-ratio is applied. Payload raster size, preview scale, SVG raster bucket size, QML image cache behavior, and texture source rectangles do not change the public zoom definition.

When item geometry is non-positive, the viewport exposes no presentable image area for public rectangles or coordinate helpers. Already committed pixels that belong to the accepted request may keep ready request and display ownership, but new target payloads remain render-waiting until positive item geometry allows a commit. Non-positive item geometry is not a render failure.

Display-only rotation and mirroring are presentation state. They affect rendering, content rectangles, visible rectangles, hit testing, coordinate conversion, fit, zoom, pan bounds, scroll bounds, display-demand projection, and scan movement without changing source metadata.

Coordinate helpers are deterministic snapshot reads. They return invalid coordinate results for unavailable geometry, invalid page roles, gap points for page-scoped mapping, non-finite inputs, or points outside the requested conversion domain. They do not mutate presentation state, diagnostics, or revisions.

## Requests, Commands, And Playback

Command admission is deterministic. Malformed enum values, invalid public value objects, non-finite numbers, missing required fields in command value types, invalid transition policies, invalid presentation commands, and invalid coordinate inputs are rejected before capability or failure-scope checks. Role-scoped commands addressed to absent roles return ignored-no-request. Intrinsic seek-domain errors are invalid before unsupported-capability or generation-terminal checks.
Role-scoped source commands must name the addressed role. `clear()` stays roleless because it clears the accepted target rather than addressing primary or secondary source content.

Terminal failures have explicit scope. Session-open failure, metadata production failure, malformed or contradictory metadata, provider protocol violation, metadata public-limit violation, and accurate metadata proving unsupported sequence content are generation-terminal. Invalid accepted targets resolved after metadata, unsupported operations for otherwise valid content, frame payload admission rejection, provider frame failure, and render commit failure are display-request-terminal when generation metadata remains valid.

Timed sequences support playback advancement through the same accepted request path as explicit seeking. The viewport has one playback driver at a time. Playback waits without accumulating catch-up time while metadata, request queueing, provider work, preparation, non-positive geometry, or render commit blocks the selected target.

Authored autoplay is metadata-driven. A newly accepted generation may start playback only when validated authored animation facts declare autoplay for a playable timed sequence; autoplay uses the same request path, playback phases, retained-display rules, loop policy, and terminal failure handling as an accepted `play(role)` command.

Provider frame, position, and playback requests include `ImageSequenceProviderDisplayDemand` for the requested role; the canonical demand schema is defined in [ImageSequence Provider Protocol](image-sequence-provider-protocol.md#provider-demand-and-payload-values). Providers may use demand to select preview, exact, bounded-detail, cached, or refinement complete-frame payloads, but unsupported or degraded detail must not change the accepted logical source identity or coordinate space and must satisfy the active exactness preference. Cached payload reuse must be re-emitted under the current request token and current demand revision. Demand revisions advance when presentation, geometry, device-pixel-ratio, render constraints, caps, budgets, or current accepted payload facts change in a way that can affect payload choice; late payloads for stale demand revisions are ignored.

Playback defaults to play-once unless authored animation facts declare finite or infinite looping. The caller looping override forces infinite looping when true and otherwise follows authored loop policy. End-of-sequence handling never exposes a beyond-final requested target.

`stop(role)` retires playback-generated pending work for the active playback driver and restores the latest non-playback target when one exists. It does not restart playback and does not mutate unrelated roles.

## Non-Goals

The viewport does not own file discovery, URL selection, provider URL generation, QML image-provider entry lifetime, load acknowledgement policy, network access, archive extraction, directory or archive navigation, active navigation scope, page number, primary page selection, secondary fallback policy, source identity, decoder backend choice, cache backend choice, display-store backend choice, predecode backend choice, refinement scheduling policy, temporary files, permissions, sandbox policy, built-in gestures, or application command routing.

The viewport validates public sequence and frame metadata before presentation. Malformed dimensions, invalid timing, unsupported payload shape, oversized payloads, and unsafe diagnostic text are rejected or redacted through public status and diagnostics rather than exposed as undefined behavior.
