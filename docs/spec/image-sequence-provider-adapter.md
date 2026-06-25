# ImageSequence Provider Adapter

The provider adapter is the public extension point for application-owned image sources. It lets applications expose custom decoding, storage, or streaming services as an `ImageSequence` without giving `ImageViewport` direct access to files, URLs, archives, or arbitrary JavaScript objects.

## Public Contract

A downstream provider should be implementable from installed public headers and linkable through the installed package target. It must not require private headers, internal controller types, test-only probes, source-tree include paths, or build-tree-only QML imports.

The adapter base should be an abstract QObject-compatible type suitable for C++ implementations and QML ownership. It should not be directly creatable as a complete provider from QML.

Factory construction should adapt a concrete provider adapter into an `ImageSequence` using side-effect-free metadata and a bounded provider-session factory. Expensive probing, network access, archive access, decoder startup, or scene graph work should begin only after a viewport accepts the sequence.

## Provider Responsibilities

Provider entry points should return after bounded local bookkeeping and report asynchronous progress through the result channel defined by the public adapter API.

Provider results should identify the logical request they answer and distinguish sequence metadata, frame readiness, waiting/progress, end of sequence, unsupported requests, cancellation, and provider failure.

Frame-ready results should provide enough cheap metadata for the viewport to validate dimensions, timing, and payload admission before retaining or uploading the frame payload.

Provider shutdown is best-effort bounded bookkeeping. Clear, replacement, item destruction, or terminal provider failure should not be blocked by provider cleanup exceptions.

## Failure Semantics

Failures detectable during factory construction should make sequence construction fail with factory diagnostics.

Failures detected after sequence assignment should leave the accepted sequence observable until the caller clears or replaces it, while request status reports unsupported or error state according to the failure.

Provider protocol violations should not expose private provider details through public strings. Public diagnostics should be redacted and stable enough for user presentation.
