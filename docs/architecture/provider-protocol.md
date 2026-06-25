# Provider Protocol

Provider integration should adapt application-owned sources into the same sequence and request model used by built-in frame lists. The protocol exists to keep blocking or source-specific work outside `ImageViewport` while preserving deterministic request state.

## Sessions

A provider-backed sequence should be able to create an independent provider session for each accepted viewport generation. Session creation may fail before generation registration; after registration, failures are reported as request or provider errors for that generation.

Provider methods called by the item-side controller should perform bounded local bookkeeping and return promptly. Long-running decoding, I/O, archive access, network access, or service calls should be marshalled by the provider and reported asynchronously.

## Requests

The controller asks for sequence metadata and display frames using generation-scoped request identifiers. Provider results must identify the request they answer so stale, superseded, or late results can be ignored cheaply.

Provider results should distinguish metadata readiness, frame readiness, waiting/progress, end of sequence, unsupported requests, cancellation, and provider failure. Terminal results seal the logical request attempt they answer.

## Payload Admission

Frame-ready results should expose cheap envelope metadata before the viewport retains expensive payload storage. The controller and preparation stages can then reject invalid dimensions, invalid timing, oversized payloads, or mismatched payload metadata before upload.

Payload ownership should be explicit. Dropped stale or rejected payload handles should release provider resources exactly once without requiring scene graph access.

## Shutdown

Generation close should be idempotent from the viewport's perspective. Clear, replacement, provider failure, facade destruction, item destruction, and overflow defense should all converge on the same close path and ignore later results for the closed generation.
