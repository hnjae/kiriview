# Async Lifecycle

Out-of-order workflows must have one local lifecycle owner. The owner is the only object allowed to accept a completion and publish public state from it.

Every async owner contract names its owner token, QObject affinity, queued acceptance path, identity kind, destruction invalidation point, cancellation behavior, and result publication point.

The identity kind is one of operation id, scoped operation id, source key plus freshness generation, demand key, component generation, provider request token, or component demand revision. A completion must carry that identity back to the owner, and the owner must accept or reject the identity before mutating authoritative state.

Cancellation is either guaranteed or best-effort. Guaranteed cancellation means no completion callback can run after cancel returns. Best-effort cancellation means the owner invalidates identity and treats any later callback as stale or no-op. Qt and KIO jobs are usually best-effort unless the provider contract explicitly proves otherwise.

QObject-backed work must use a live owner token or weak Qt reference before queued delivery can run. Functor connections that capture owner state provide a receiver or context object, and captures that can outlive another object use `QPointer`, weak ownership, or an explicit lifetime handle rather than an unguarded raw pointer. Owner destruction must disconnect external signals where the owner owns the connection, cancel active jobs, invalidate operation identity, and leave queued callbacks unable to dereference the destroyed owner. The complete native lifetime rules are defined in [C++ And Qt Safety](cpp-qt-safety.md).

Worker-thread work and calls into the Rust support library return plain payloads only. Payloads crossing a worker or language boundary must be safe to move to the GUI acceptance path, must not contain QObject pointers, and must not carry an alternate authoritative URL, page, frame, or status beside the owner's current identity.

Worker-backed owners must receive scheduling through an explicit dependency port. Execution adapters may choose an appropriate worker facility, but runtime owners must not bypass the scheduler contract that preserves owner-guarded delivery, cancellation, and stale-completion acceptance.

Worker scheduling returns an owner-held cancellation handle. Canceling work that has not started must withdraw it from the scheduler queue and release its captured payload without consuming an execution slot; work that has already started may finish best-effort, but cancellation suppresses its completion and repeated replacement must retain only a bounded number of obsolete running payloads. A scheduler adapter that completes work synchronously may return an inactive handle.

Worker-backed providers resolved from runtime dependencies must capture the scheduler dependency supplied to that runtime when one is present. Resolving a data loader, opened-collection candidate loader, thumbnail cache lookup, or thumbnail generation provider must not silently bypass the scheduler boundary carried by the consuming runtime.

Shared async support owns reusable callback delivery, cancelable job wrappers, operation-state helpers, directory-listing provider contracts, and worker scheduling primitives. Default domain loaders must live with the owning domain: image decode data loading belongs to decoding, page-candidate loading belongs to navigation, opened-collection byte access belongs to collection media-entry owners, and thumbnail byte or cache work belongs to thumbnail runtime adapters.

Session runtime dependencies own active-navigation thumbnail worker scheduling. Thumbnail lookup and generation providers must use the scheduler supplied to the thumbnail runtime. Scheduling identity and backend-job identity remain distinct; a completion is publishable only after both owners accept their respective identity, and cancellation invalidates acceptance before canceling backend work. The detailed ownership contract is defined in [Thumbnail Source Adapters](thumbnail-source-adapters.md).

Image provider-resource owners schedule decode and refinement through an injected worker scheduler. Work carries separate application reuse identity and active component-demand identity; a completion may populate a compatible bounded cache by reuse identity but may enter the viewport only after the provider accepts the source generation, request token, and demand revision. In-flight sharing and worker start gates are best-effort cancellation tools, not alternate publication authority.

Timer-backed owners must receive monotonic time and timer firing through dependency ports when behavior depends on elapsed time. Runtime state must consume plain timestamps and scheduled callback events rather than owning a second wall-clock-derived state.

Directory listing and watching must cross an injectable provider boundary. Candidate loaders, direct-media navigation, and live candidate stores consume directory snapshots, change events, deletion events, and listing failures through provider ports. Live candidate stores are permitted only for scope kinds whose product contract is live; snapshot scopes consume listing results without subscribing to external change events. Live image page candidate entries own subscriber and pending-load state, but they must not construct or connect platform directory watchers directly.

QML may own UI-local timers and physical item transients, but it must not use delayed callbacks to reconcile durable domain state. If a delayed UI callback observes public session state, the C++ owner must already have published a coherent snapshot for that state.

Reusable async primitives must preserve each owner's operation identity, stale-completion rejection, cancellation semantics, and QObject lifetime rules without becoming a shared authoritative state machine.
