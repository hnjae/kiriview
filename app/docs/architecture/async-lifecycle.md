# Async Lifecycle

Out-of-order workflows must have one local lifecycle owner. The owner is the only object allowed to accept a completion and publish public state from it.

Every async owner contract makes its lifecycle identity, destruction invalidation, cancellation behavior, and result publication authority explicit. QObject affinity and queued delivery are specified only for work that actually crosses those Qt boundaries.

An operation id, scoped source freshness, demand identity, or opaque public dependency correlation may provide lifecycle identity. The representation is owner-specific, but a completion carries enough evidence for the owner to accept or reject it before mutating authoritative state.

Cancellation is either guaranteed or best-effort. Guaranteed cancellation means no completion callback can run after cancel returns. Best-effort cancellation means the owner invalidates identity and treats any later callback as stale or no-op. Qt and KIO jobs are usually best-effort unless the provider contract explicitly proves otherwise.

QObject-backed work must use a live owner token or weak Qt reference before queued delivery can run. Functor connections that capture owner state provide a receiver or context object, and captures that can outlive another object use `QPointer`, weak ownership, or an explicit lifetime handle rather than an unguarded raw pointer. Owner destruction must disconnect external signals where the owner owns the connection, cancel active jobs, invalidate operation identity, and leave queued callbacks unable to dereference the destroyed owner. The complete native lifetime rules are defined in [C++ And Qt Safety](cpp-qt-safety.md).

Worker-thread work and calls into the Rust support library return plain payloads only. Payloads crossing a worker or language boundary must be safe to move to the GUI acceptance path, must not contain QObject pointers, and must not carry an alternate authoritative URL, page, frame, or status beside the owner's current identity.

Worker-backed owners schedule work through an owner-controlled execution boundary that preserves guarded delivery, cancellation, and stale-completion acceptance. Owners may share scheduling capacity when that boundary preserves each owner's lifecycle and permits deterministic control; no particular scheduler interface or dependency-injection mechanism is required.

Cancellation invalidates publication before backend cancellation is attempted. Queued captures and obsolete work are released promptly enough to preserve declared resource bounds; an implementation may remove queued work, mark it skipped before execution, or use another mechanism with the same effect. Work that has already started may finish best-effort, but its completion cannot publish after invalidation.

Providers used by a runtime must preserve that runtime's scheduling, capacity, cancellation, and delivery guarantees. Provider construction must not silently move work onto an unmanaged path that bypasses those guarantees.

Reusable callback delivery, cancelable jobs, operation-state helpers, directory-listing boundaries, and worker scheduling may be shared where doing so does not create a shared authoritative state machine. Domain work remains with its owner: image decode data loading belongs to decoding, page-candidate loading belongs to navigation, opened-collection byte access belongs to collection media-entry owners, and thumbnail byte or cache work belongs to thumbnail runtime adapters.

The thumbnail runtime owns admission and completion acceptance for active-navigation thumbnail work. Backend completion is publishable only while both runtime demand and backend work remain current, and cancellation invalidates acceptance before backend cleanup. The detailed ownership contract is defined in [Thumbnail Source Adapters](thumbnail-source-adapters.md).

Image provider-resource owners schedule decode and refinement through an owner-controlled execution boundary. Work carries application reuse identity and active public provider-request correlation; a completion may populate a compatible bounded cache by reuse identity but may be returned only after the provider accepts current application freshness and the complete request correlation. In-flight sharing and worker start gates are best-effort cancellation tools, not alternate publication authority.

Elapsed-time behavior uses a monotonic clock and keeps timer callbacks subordinate to the owning lifecycle. Time and firing may be supplied through a controllable boundary when deterministic verification requires it; the concrete timer and injection mechanism are implementation choices.

Directory listing and watching remain behind the navigation or collection-access ownership boundary. Candidate loading and live navigation consume accepted snapshots, change events, deletion events, and failures without exposing platform watcher state as navigation authority. Only scope kinds whose product contract is live subscribe to external changes; snapshot scopes consume listing results without subscribing.

QML may own UI-local timers and physical item transients, but it must not use delayed callbacks to reconcile durable domain state. If a delayed UI callback observes public session state, the session owner must already have published a coherent snapshot for that state.

Reusable async primitives must preserve each owner's operation identity, stale-completion rejection, cancellation semantics, and QObject lifetime rules without becoming a shared authoritative state machine.
