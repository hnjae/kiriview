# Workflow Shape

This document defines how product policy and runtime coordination preserve ownership.

## Ownership-Preserving Workflow Boundary

The workflow owner receives UI and runtime facts, applies authoritative state, executes required effects through their lifecycle owners, and accepts or rejects completions. Policy may assist with any decision that can remain independent of runtime side effects.

Event loops, reducers, direct owner APIs, and other internal coordination shapes are implementation choices. Request, loading, decoding, failure, presentation, completion, and other out-of-order work must carry enough owner-held identity for the workflow owner to reject stale results.

Policy may compute loading status, error recovery, navigation updates, cache policy, and follow-up decisions from coherent owned values. Runtime owners keep the actual KIO job, decoder job, ImageViewport integration owner, image provider resource, and Qt notification. The ImageViewport integration owner uses only the supported dependency boundary and does not model presentation internals.

Workflows that update visible state must distinguish committed public state from pending targets. They publish the new state only after the resources required for that state are ready, unless the user-visible spec explicitly defines an intermediate placeholder or retained-display state.

## Runtime Operation Ownership

When multiple policy concerns require runtime operations for one workflow, the workflow owner must receive one coherent decision and remain the sole interpreter of its effects. Internal operation representation and grouping may change as long as dispatch ownership and effect semantics remain unambiguous.

Cross-owner interactions use explicit owner APIs or observation boundaries when ownership, stale-completion rejection, or public projection ordering is at stake. Direct collaborator references are allowed when they do not expose mutable state, bypass lifecycle checks, or create a second publication path.

Runtime operations use a coherent vocabulary at the workflow boundary and delegate effects to their lifecycle owners. Internal refactoring that preserves operation meaning, dispatch ownership, and stale-completion behavior does not require an architecture update.

## Image Document Workflow

Image-open workflow transitions apply document state and produce explicit follow-up operations. The image-document workflow owner remains the sole dispatcher for those operations.

Image-open transitions preserve invariant-coupled document facts: source URL, source kind, displayed location, loading, status, error text, sibling archive navigation, unsupported opened-collection video, playable opened-collection video handoff, and embedded metadata. Resource owners may prepare decoded images and metadata, but publication of those facts happens only through the image-document workflow owner.

Same-scope image-to-image active navigation is a target-selection workflow rather than a scope replacement. The workflow preserves the active scope and compatible source resources while applying the transitions defined by [Navigation](../spec/navigation.md) and [Image Display](../spec/image-display.md).

Source replacement remains the workflow for top-level source assignment, active scope changes, image-to-video or video-to-image mode changes, empty/error clearing, and sibling archive navigation that changes the opened collection scope.

Image-document source-load effects resolve requested source URL, displayed opened-collection scope, sibling archive URL, and directly opened source facts into an opened-collection scope command before crossing into collection source lifetime code. Production filesystem and archive probing belongs to a resolver or adapter boundary that supplies resolved facts; pure image-load planning consumes those facts and must not perform host-environment probes.

The media-entry source boundary owns reuse for an already resolved opened-collection scope. It must not depend on image-document source-load requests or image-load planning representations.

## Candidate Snapshot Consumers

Candidate-list owners publish confirmed snapshots before dependent workflows derive public active navigation, thumbnail rows, predecode windows, deletion fallback targets, or opened-collection foreground loads. The snapshot identity contract is defined in [State Ownership](state-ownership.md#candidate-snapshot-boundary).

Direct-media sibling discovery accepts a snapshot only when the current direct-media scope still matches the discovery request. Image-document page candidate refresh accepts a snapshot only when its candidate-list source still matches the page navigation owner.

The direct-media candidate owner may observe changes for a currently supported scope. It publishes only the newest accepted candidate view. A failed or resource-rejected live observation makes navigation unavailable without replacing it with an empty or partial candidate view, while the current media remains usable and observation may recover. The owner treats a missing current item as removal only after a successful complete snapshot for that same scope has confirmed the item; first discovery without the current item and unavailable refresh outcomes do not establish a removal transition.

A pending same-source refresh retains the last matching confirmed snapshot while work is in flight. A source-identity change clears the prior snapshot before consumers can use it. Consumers must not synthesize a partial list or use rows from a different source.

Projection and thumbnail workflows consume candidate snapshots with their source and freshness evidence. They may retain compatible row-derived state across position-only changes, but a source, row-identity, or ordering change invalidates any work whose correlation no longer matches.

Opened collection foreground loading and image-document predecode planning may reuse a confirmed page candidate snapshot only when the snapshot source matches the requested opened-collection or directory source. Pending refreshes, deletion fallback that changes the retained list, sibling archive navigation that changes scope, and direct-media source replacement invalidate or replace the reusable snapshot before downstream consumers can rely on it.

## Routing And Dispatch

Direct media routing is a document-session decision. A routing result may classify a requested source as empty, direct video, direct image, archive collection, directory collection, or another image-document input, while the session lifecycle owner executes Qt/KDE effects through the responsible image, video, navigation, and preparation owners.

Opened collection video routing is separate from ordinary direct media routing. When the active opened collection selection resolves to an eligible playable video entry, the session keeps the opened collection active-navigation context, asks the media-entry source boundary for a playback source device, and assigns that device to the video document while preserving the collection entry URL as public source identity.

Eligible archive entries and directory collection entries may both provide playback source devices. Ineligible archive video entries remain in image-mode opened-collection context as unsupported-video placeholders. This path must not synthesize a direct media scope, remount the archive through the direct-media navigation-source path, or reuse the direct-video playback-preparation boundary.

Video-mode adjacent navigation, scan navigation, and shared Previous, Next, First, and Last commands dispatch through the document-session active navigation projection. Direct video mode and opened collection video mode differ by the active scope supplied to the session; shortcut handlers, toolbar actions, and video controls must not hard-code ordinary direct media navigation when the active video belongs to an opened collection.

Document-session policy may compute active navigation projections, action-availability gates, direct media routing, deletion fallback, and predecode eligibility from coherent owned values. Policy results must not publish QML-facing values directly, store independent workflow state, or bypass session-owned stale-completion checks.

Shared navigation dispatch belongs to the document session. The action runtime combines the session projection with accepted UI gate snapshots for shared action and shortcut availability; QML reports UI-local gate facts and renders action placements. Route selection and boundary dispatch policy stay behind session methods.
