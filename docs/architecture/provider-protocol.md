# Provider Protocol

Provider integration adapts application-owned page sources into the same sequence and request model used by built-in frame lists. Providers own source-specific production behind the adapter boundary; the viewport engine owns request identity, token matching, stale-result filtering, display-demand projection, status projection, diagnostics, playback ordering, retained display, and render readiness.

## Descriptor And Sessions

Provider-backed sequence construction captures an `ImageSequenceProviderDescriptor` containing construction metadata, threading contract, and a stored session factory. Every available construction-metadata field is authoritative, and unknown facts use explicit unavailable values. Descriptor construction and storage are side-effect-free; expensive source work starts only when the engine asks the provider host to invoke the factory for an accepted role generation.

A provider-backed sequence creates an independent session for each accepted viewport page-role generation, including concurrent uses of the same public handle. Construction validates only descriptor facts available without invoking the factory. Provider hosts may invoke the immutable factory concurrently from different viewport command-processing threads; the descriptor owns the callable lifetime, while synchronization of provider state captured by that callable remains a provider responsibility. The engine registers each generation before invocation, and the provider host rejects a failed or malformed result, including a session object already owned by another live or closing generation, as provider failure for that generation alone before storing it.

Provider transport may create, command, cancel, close, and clean up sessions, but session identity, accepted-generation matching, token allocation, queued-work policy, cleanup interest, and retirement are engine-owned.

## Requests

Provider sessions receive the canonical request values defined by the public provider protocol. Metadata, frame, position, and playback requests carry engine-allocated opaque tokens; frame, position, and playback requests also carry engine-authored display demand for the target role. Cancel requests identify provider work whose engine tokens have been retired or marked for retirement; they do not control engine progress. Close requests are best-effort cleanup after engine-side generation retirement and have no event contract.

Frame requests target a resolved frame. Position requests preserve the public requested position and carry the resolved frame identity needed for payload validation. Playback requests carry playback-origin token identity and may produce frame readiness or end-of-sequence events. Unknown-target initial assignment requests metadata first and sends no frame, position, or playback request until metadata selects a valid target.

Display demand is a value snapshot, not provider-owned viewport state. Its public fields are defined by `ImageSequenceProviderDisplayDemand` in [ImageSequence Provider Protocol](../spec/image-sequence-provider-protocol.md#provider-demand-and-payload-values). Demand lets providers choose complete-frame preview, exact, bounded-detail, cached, or refinement payloads without receiving scene graph resources or QML image-provider state. Exactness preference is an admission constraint: `RequireExact` classifies a newly offered well-formed inexact payload as unsupported payload rejection even when the demand revision matches. An event for a retired token or demand is stale and is released and ignored; an envelope that fails to echo the demand recorded for its still-active token is an active error payload rejection scoped to the owning display work or auxiliary refinement. Visible detail must have been admitted under the demand active at its commit, and cached payload reuse must be re-emitted under the current token and demand revision.

The engine allocates per-role payload budgets as one target-spread decision before issuing concurrent role work. Reservations account for every required role and retained payload ownership, and preparation consumes or releases them atomically so concurrent provider completions cannot oversubscribe the display budget. A cap, reservation, or allocation-accounting change advances allocation generation and demand revision for affected work; it cannot silently reinterpret an already issued demand.

Public opaque identity types preserve their engine ownership domains. Presentation-target generation identifies accepted or displayed target ownership, allocation generation identifies payload-budget and accounting state, demand revision identifies payload admission, and state revision identifies a public observation domain. These identities are not substitutable or cross-comparable even when their internal allocators happen to produce equal representations.

Each session has one serialized command stream from the engine and an engine-owned token ledger. Providers may complete work on arbitrary worker facilities, but event delivery into the viewport is normalized by the provider host before engine state changes are evaluated. Event ingress synchronously captures transferred frame ownership and immutable session identity at signal emission, while a separate queued handoff serializes engine reduction on the viewport command-processing affinity. The ledger records request kind, role, generation, demand, cancellation, and terminality so admission never depends on callback order or provider object identity.

## Events

Provider sessions report the canonical event values defined by the public provider protocol. Events for metadata, frame, position, and playback work echo the token of the original request they answer. `Cancelled` events echo the original token being cancelled; `Close` has no event contract.

The engine validates session identity, generation identity, token scope, request/event kind compatibility, page role, active request identity, cancellation state, and terminality before an event can mutate state. Each response-bearing token accepts at most one terminal event. Late or duplicate events for stale, cancelled, terminal, or closed work are released and ignored without changing public state, display content, playback phase, diagnostics, or revisions. A token, kind, session, or role mismatch for still-active work is a protocol violation; a syntactically compatible frame-ready event proceeds to payload admission, where demand, frame, and timing mismatches are payload rejection with display-request-terminal or auxiliary-only scope according to the owning work.

Unsupported events carry an explicit cause: `UnsupportedRequest` or `PayloadRejection`.

Waiting and progress events are advisory. They may update request wait projection for the active token, may be coalesced, and must never dominate terminal results for the active token.

## Terminal Ownership

The engine stores generation-terminal facts by generation and role, independently of display-request-terminal facts stored by generation, display-request identity, and role. A new accepted display request retires only the matching display-request terminal; it cannot erase or bypass a generation terminal. Clear and non-empty presentation-target replacement retire the generation and therefore both terminal domains. Aggregate request projection selects from the applicable terminal domains with one deterministic role and severity precedence.

Provider session availability is transport execution state, not a second authority for command recovery. Session-open failure, active protocol violation, failure to deliver any response-bearing command, and provider request-token exhaustion record a generation terminal before converging on generation close. No same-generation request may reopen, replace, or retry that session. A queued-frame scheduling failure occurs before provider delivery, records only a display-request terminal, and leaves the accepted session available for a later request.

## Metadata

Metadata readiness is generation-scoped. Runtime metadata supplies frame count, sequence-wide source logical size, timing model, seek support, timed playback support, authored animation facts, and timing intervals. It becomes authoritative only after validation against public limits and construction-time constraints.

The engine may project each validated authoritative construction-time fact while other metadata fields remain explicitly unavailable. This partial projection must not select an unknown initial target or relax later runtime validation; target resolution waits for the complete metadata required by the selected still or timed model.

Complete construction-time metadata may select initial frame `0` during assignment. Unknown or partial construction-time metadata leaves the initial target unknown until validated runtime metadata creates a metadata-bound display request for the still-active initial request.

Validated metadata may complete generation facts after the active display request has changed when session and generation identities still match and the generation has not closed. It may bind only a still-active unknown target and must not revive stale initial, seek, playback, clear, or replacement requests.

Malformed metadata, contradiction of construction-time facts, non-contiguous timing, non-positive durations, invalid dimensions, provider protocol violation, and public-limit violation are generation-terminal payload-rejection errors. Accurate metadata proving unsupported sequence content is generation-terminal unsupported state. Unsupported operations for otherwise valid content are scoped to the active operation that required them.

## Payload Admission

Frame-ready events transfer a handle owning one `ImageFrame` plus an `ImageSequenceProviderFrameEnvelope`. The preparation boundary validates reusable payload facts from the frame, request-scoped demand and timing identity from the envelope, their consistency with validated metadata and the active request, and all public and effective runtime limits before render ownership begins.

The frame's source logical size must match validated sequence metadata for that role. Its payload raster may differ from source logical size, but it must be finite, positive, complete-frame, and aspect-compatible after orientation normalization. The envelope's demand revision must match the answered request, and its timed frame index and frame-start position must match the accepted display request's resolved frame identity. `seekToPosition(totalDuration)` resolves to the final frame index and final frame start position and is not answered with end of sequence.

Frame-handle ownership is recorded at event ingress before any queued handoff or payload admission. Every rejected, stale, retained-then-released, render-failed-and-unretained, memory-pressure-released, clear-released, replacement-released, undelivered-event-released, or destruction-released handle converges on one idempotent release path and is released exactly once without scene graph access. Release callbacks are normalized through the provider host according to the provider threading contract, are not invoked from the Qt Quick render thread, and complete before their session is destroyed.

Each session has one host-owned lifetime barrier shared by event ingress, frame leases, close delivery, and affinity destruction. Close completion seals new provider emission, but destruction waits for every ingress already in progress and every captured handle to finish release and affinity-correct disposal. Queued provider events use a revocable delivery endpoint rather than item-private lifetime; host shutdown revokes engine delivery, retires undelivered frame leases, and leaves cleanup work owned until it converges.

Provider refinement is auxiliary generation-scoped work owned separately from the accepted display request. Every non-bookkeeping presentation, geometry, device-pixel-ratio, cap, budget, or allocation change that advances demand for a committed provider-backed role marks that role non-current, retires any older auxiliary token for the role, and issues exactly one refinement request for the new revision. The request carries a new active token and the then-current demand for the unchanged target, role, frame, and source logical coordinates. Updating current-payload facts from a commit may advance the revision available to later requests but does not itself schedule work or make that payload non-current. Fixed in-memory roles bypass provider demand scheduling and retain current status until another frame or target is admitted.

The engine admits a matching refinement through the normal preparation and render-commit identities, then swaps payload ownership without changing logical target, ready request status, or playback phase. Unsupported, failed, cancelled, or payload-rejected refinement results retire only the auxiliary token and preserve the committed payload; if the demand remains unsatisfied, `currentForDemand` remains false until a later qualifying demand change or successful refinement. Refinement token failure does not populate request-terminal diagnostics. A kind, session, token, or role protocol violation and failure to deliver the refinement command remain generation-terminal because they invalidate the session rather than only the optional detail upgrade.

## Playback

Provider playback uses playback-token identity in addition to display-request identity. Frame-ready events for playback validate against the linked active display request. End-of-sequence events are interpreted by engine playback policy rather than being published as a public target.

In play-once mode, end of sequence selects or promotes the final displayable frame and stops only after that frame is committed for the accepted request or already visible same-generation final-frame pixels are promoted to the accepted display identity. In looping mode, end of sequence wraps to the first frame without exposing an out-of-range requested position.

Stop, seek, loop, clear, replacement, unsupported, and terminal failures retire older playback tokens. Late playback events for retired tokens are stale.

## Shutdown

Generation close is idempotent. Clear, replacement, generation-terminal failure, item destruction, and engine shutdown converge on the same close path. Closing retires engine token and queued-request state before transport sends best-effort cancellation and close requests. A failure while sending cancellation or close cannot create another public terminal because those commands expect no response and cleanup remains best-effort.

Cancellation acknowledgement is not required for the viewport to proceed. A cancellation event for engine-cancelled or closed work is cleanup acknowledgement only. A provider-originated cancellation for a still-active token that the engine did not cancel reports provider failure.
