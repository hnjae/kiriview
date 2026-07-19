# Presentation And Viewport

## Image Presentation Ownership

The repository-internal `ImageViewport` component is the single mutable owner of accepted presentation targets, transition phase, committed and retained presentation, page-role visibility, fit and zoom state, content position, rotation and mirroring, spread direction and gap, visible source geometry, display-refinement demand, per-role playback state, render readiness, and image scene graph resources.

The image-document integration owner retains only application-side facts: selected and pending source identity, navigation scope, page pairing and role assignment, reading-direction and scan policy, action routing, component-generation correlation, typed failure-reference resolution, and the public application projection derived from a matching component snapshot. It must not copy component presentation fields into a parallel mutable runtime.

Reading direction remains an application navigation and pairing policy because it affects page order and scan commands. The integration owner submits the corresponding component spread direction atomically with a target or presentation command. The component is authoritative for the applied spread direction and geometry.

Presentation transitions distinguish the complete prior component commit, an accepted target in progress, and the complete new commit. Selected-target transitions use a keep-failed-target policy when the target's error must remain active; presentation-shape changes use a restore-previous policy when the product contract restores the prior view. A restore transition pins the component's complete previous generation, provider sessions, payload handles, presentation state, and per-role playback state; KiriView does not reconstruct or re-decode that state. The application operation record retains only the prior application-owned presentation-shape policy and restores it atomically with the application projection when the matching component snapshot reports a restored transition.

## Viewport Command Boundary

Fit, preferred manual zoom, effective zoom, pan bounds and position, rotation, mirroring, viewport geometry, coordinate conversion, and position clamping belong to the component reducer. The image-document integration owner is the sole KiriView production submitter of typed component commands. QML reports raw interaction facts through that owner, application action handlers route through the application command router, and shared application state consumes only the resulting command observation and matching coherent component snapshot through the integration publication.

Owner-issued generations and revisions that cross QML are opaque tokens. QML must not generate them, narrow them into numeric types, compare them arithmetically, or replay a superseded command. Delayed UI callbacks may submit new inputs but cannot overwrite canonical viewport state.

Anchored zoom, drag pan, wheel steps, and scan movement may begin from UI-local pointer positions or deltas. KiriView owns scan-step, nearest-point, reading-order, and snapping policy; it consumes the component's coordinate-query boundary and submits typed commands. The component alone resolves the final zoom, content position, bounds, and visible geometry.

The toolbar and action runtime derive fit selection, zoom readout, editability, pannability, and image readiness from the current matched component snapshot. The toolbar must not preserve an independent applied zoom. The preferred manual zoom survives a narrower target range while the effective zoom readout reports the applied target value.

## Media Workspace Boundary

QML owns layout composition of the media viewport, layout-reserving panels, runtime-only panel size and visibility, delegate selection, focus handoff, raw gesture sampling, and context-menu signal forwarding. Shared workspace code communicates with image and video viewports only through their declared interaction contracts and must not reach into mode-specific internals.

The image delegate hosts the `ImageViewport` item as the image visual surface. Video playback, seeking, and backend attachment remain behind the video and session boundaries. QML may own the physical video-output item and report an owner-tokened surface claim, but C++ owns attachment, detachment, stale-claim rejection, destroyed-item handling, and public video attachment state.

## Source, Failure, And Resource Boundary

KiriView's `ImageSequence` provider owns source access, decode and refinement jobs, cache and predecode reuse, typed source failure values, application failure-registry records, and application display-store leases. It answers component demand with validated metadata, complete-frame payload handles, or typed generic failures with optional opaque application failure handles and does not own viewport state.

A component failure snapshot exposes a typed generic cause and optional opaque provider failure reference for the current failed target or a restored transition. The integration owner resolves that reference only against the provider registry associated with the matching application target and publishes the KiriView-owned user-facing message. The component owns the corresponding failure handle until the observation is no longer reachable and releases it exactly once through the declared provider boundary; release, rather than snapshot polling, authorizes retirement of the registry record. Free-form provider diagnostics never cross the component protocol.

Frame-handle and failure-handle release are the provider-resource lifetime acknowledgements. Provider-resource, cache, display-store, and failure-registry owners may retire, reuse, or evict an entry only according to their own lease policy after the component releases its handle. QML load status, QML engine caching, snapshot polling, and visual-item destruction are not application resource authorities.
