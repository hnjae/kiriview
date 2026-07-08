# Provider Protocol

Provider integration adapts application-owned page sources into the same sequence and request model used by built-in frame lists. Providers own source-specific production behind the adapter boundary; the viewport engine owns request identity, token matching, stale-result filtering, display-demand projection, status projection, diagnostics, playback ordering, retained display, and render readiness.

## Descriptor And Sessions

Provider-backed sequence construction captures an `ImageSequenceProviderDescriptor`. The descriptor contains declared capability support, cheap known source-logical facts, authored animation facts, threading contract, and a bounded session factory. Descriptor construction is side-effect-free; expensive source work starts only after a viewport accepts the sequence and opens a session.

A provider-backed sequence can create an independent session for each accepted viewport page-role generation. The engine registers the generation before the provider host opens the session. Session-open failure after acceptance enters the engine as a generation event and reports provider failure for that generation.

Provider transport may create, command, cancel, close, and clean up sessions, but session identity, accepted-generation matching, token allocation, queued-work policy, cleanup interest, and retirement are engine-owned.

## Requests

Provider sessions receive the canonical request values defined by the public provider protocol. Metadata, frame, position, and playback requests carry engine-allocated opaque tokens; frame, position, and playback requests also carry engine-authored display demand for the target role. Cancel requests reference active tokens they retire. Close requests are best-effort cleanup after engine-side generation retirement and have no event contract.

Frame requests target a resolved frame. Position requests preserve the public requested position and carry the resolved frame identity needed for payload validation. Playback requests carry playback-origin token identity and may produce frame readiness or end-of-sequence events. Unknown-target initial assignment requests metadata first and sends no frame, position, or playback request until metadata selects a valid target.

Display demand is a value snapshot, not provider-owned viewport state. Its public fields are defined by `ImageSequenceProviderDisplayDemand` in [ImageSequence Provider Protocol](../spec/image-sequence-provider-protocol.md#provider-demand-and-payload-values). Demand lets providers choose complete-frame preview, exact, bounded-detail, cached, or refinement payloads without receiving scene graph resources or QML image-provider state. Exactness preference is an admission constraint: `RequireExact` rejects inexact payloads even when the demand revision matches. Demand revision is part of stale-result admission; a payload for a superseded demand revision is released and ignored. Retained visible detail comes only from payloads previously admitted for the then-active demand, and cached payload reuse must be re-emitted under the current token and demand revision.

Each session has one serialized command stream from the engine. Providers may complete work on arbitrary worker facilities, but event delivery into the viewport is normalized by the provider host before engine state changes are evaluated.

## Events

Provider sessions report the canonical event values defined by the public provider protocol. Events for metadata, frame, position, and playback work echo the token of the original request they answer. `Cancelled` events echo the original token being cancelled; `Close` has no event contract.

The engine validates session identity, generation identity, token scope, page role, and active request identity before an event can mutate state. Late events for stale, superseded, cancelled, or closed work are released and ignored without changing public state, display content, playback phase, diagnostics, or revisions.

Unsupported events carry an explicit cause: `UnsupportedRequest` or `PayloadRejection`.

Waiting and progress events are advisory. They may update request wait projection for the active token, may be coalesced, and must never dominate terminal results for the active token.

## Metadata

Metadata readiness is generation-scoped. Runtime metadata supplies frame count, sequence-wide source logical image size, timing model, seek support, timed playback support, authored animation facts, and timing intervals. It becomes authoritative only after validation against public limits and construction-time constraints.

Complete construction-time metadata may select initial frame `0` during assignment. Unknown or partial construction-time metadata leaves the initial target unknown until validated runtime metadata creates a metadata-bound display request for the still-active initial request.

Metadata may update generation facts after the active display request has changed when session and generation identities still match and the generation has not closed. It may not revive superseded initial, seek, playback, clear, or replacement requests.

Malformed metadata, contradiction of construction-time facts, non-contiguous timing, non-positive durations, invalid dimensions, provider protocol violation, and public-limit violation are generation-terminal payload-rejection errors. Accurate metadata proving unsupported sequence content is generation-terminal unsupported state. Unsupported operations for otherwise valid content are scoped to the active operation that required them.

## Payload Admission

Frame-ready events transfer a frame handle and `ImageSequenceProviderFrameEnvelope`. The preparation boundary validates source logical size, complete-frame payload raster size, source-to-payload mapping, payload byte size, frame index, frame-start position, timing, orientation, transparency, quality facts, active demand revision, and public limits before render ownership begins.

The frame envelope source logical image size must match validated sequence metadata for that role. The payload raster may differ from source logical size, but it must be finite, positive, complete-frame, and aspect-compatible after orientation normalization. Timed frame index and frame-start position must match the accepted display request's resolved frame identity. `seekToPosition(totalDuration)` resolves to the final frame index and final frame start position and is not answered with end of sequence.

Rejected, stale, superseded, retained-then-released, render-failed-and-unretained, memory-pressure-released, clear-released, replacement-released, and destruction-released frame handles are released exactly once without requiring scene graph access or provider knowledge of render-side objects. Providers may use handle release for application cache leases or backpressure, but cancellation acknowledgement is never required before the viewport can proceed. Release callbacks are normalized through the provider host according to the provider threading contract and are not invoked from the Qt Quick render thread.

Same-target refinement uses the same identity and stale-result rules as ordinary frame requests. The viewport trusts the caller's application-owned source identity intent and validates public page-set shape, metadata consistency, logical-size equality, active request identity, demand revision, and payload envelope. A refinement payload may replace display detail for the same source logical identity, source logical size, and role only when those public checks still match. Refinement never changes source logical coordinates. Presentation, geometry, device-pixel-ratio, cap, budget, allocation, or current-payload changes that can alter payload choice allocate a new demand revision and may cause a new provider request for the active role.

## Playback

Provider playback uses playback-token identity in addition to display-request identity. Frame-ready events for playback validate against the linked active display request. End-of-sequence events are interpreted by engine playback policy rather than being published as a public target.

In play-once mode, end of sequence selects or promotes the final displayable frame and stops only after that frame is committed for the accepted request or already visible same-generation final-frame pixels are promoted to the accepted display identity. In looping mode, end of sequence wraps to the first frame without exposing an out-of-range requested position.

Stop, seek, loop, clear, replacement, unsupported, and terminal failures supersede older playback tokens. Late playback events for superseded tokens are stale.

## Shutdown

Generation close is idempotent. Clear, replacement, generation-terminal failure, provider-owned external resource invalidation, item destruction, and engine shutdown converge on the same close path. Closing retires engine token and queued-request state before transport sends best-effort cancellation and close requests.

Cancellation acknowledgement is not required for the viewport to proceed. A cancellation event for engine-cancelled or closed work is cleanup acknowledgement only. A provider-originated cancellation for a still-active token that the engine did not cancel reports provider failure.
