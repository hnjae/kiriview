# Presentation And Viewport

## Image Presentation Ownership

KiriView delegates image presentation to `ImageViewport` through its supported public interface. Presentation state exposed by that dependency is not application-owned state, and KiriView architecture does not define its internal representation or enforcement mechanism.

The ImageViewport integration owner is the image-document runtime's adapter and retains only application-side integration facts: selected and pending source identity, navigation scope, page pairing and role assignment, reading-direction and scan policy, action routing, opaque dependency correlation, typed failure-reference resolution, and the public application projection derived from a matching supported observation. It must not copy dependency observations into a parallel mutable presentation runtime or become a second owner of image-document state.

Reading direction remains an application navigation and pairing policy because it affects page order and scan commands. The integration owner supplies the requested presentation direction through the supported command boundary and publishes only the resulting matched observation.

Target selection and presentation-shape changes publish application state according to [Image Display](../../spec/image-display.md) and [Navigation](../../spec/navigation.md). The integration owner keeps selected, displayed, and refinement identities distinct so a prior presentation or stale completion cannot be attributed to the new target.

An accepted image selection owns physical presentation authority even while collection or page resolution is pending. Acceptance immediately revokes every older target's authority to render-commit, publish provisional pixels, or alter the accepted selection's projection; older work may complete only for cleanup or independently valid resource reuse.

## Viewport Command Boundary

The ImageViewport integration owner is the sole KiriView production caller of supported `ImageViewport` commands. QML reports raw interaction facts through that owner, application action handlers route through the application command router, and shared application state consumes only matched public observations through the integration publication.

Dependency-supplied correlation values that cross QML are opaque. QML must not generate them, narrow them into numeric types, compare them arithmetically, or replay a superseded command. Delayed UI callbacks may submit new inputs but cannot overwrite a newer application projection.

Anchored zoom, drag pan, wheel steps, and scan movement may begin from UI-local pointer positions or deltas. KiriView owns scan-step, nearest-point, reading-order, and snapping policy; it consumes supported coordinate queries and submits supported commands without reproducing presentation geometry.

The toolbar and action runtime derive fit selection, zoom readout, editability, pannability, and image readiness from current matched public observations. A retained toolbar appearance is a readonly projection of the last complete presentation rather than an independent applied zoom or action state.

Retaining a prior complete display during a selected-target replacement does not retain the prior selected target. Window-title identity and collection-specific control placement follow the accepted selected target, while retained pixels remain attributed to their displayed generation and controls that operate on an image remain gated by a matching ready observation. Still-valid toolbar placements retain the last complete visual presentation for the retained display's entire lifetime and transition directly to the matching target's presentation when it becomes ready; target identity, placement, action state, and command routing never inherit authority from that visual fallback. Presentation-shape changes revoke the old single-page or spread presentation immediately and do not use this selected-target retention path. The application publishes the current, retained, or unavailable presentation relationship coherently enough that consumers cannot expose an intermediate unavailable toolbar presentation between compatible retained and current states. QML renders that projection and must not assemble or time a durable retained presentation from individual action, zoom, readiness, or loading-feedback changes.

## Media Workspace Boundary

QML owns layout composition of the media viewport, layout-reserving panels, runtime-only panel size and visibility, focus handoff, raw gesture sampling, context-menu forwarding, and delayed loading-feedback presentation. Delayed feedback must remain correlated with the accepted target lifecycle: supersession or terminal publication prevents an older callback from changing the current surface, and feedback timing cannot shorten or extend a retained toolbar presentation. The ImageViewport integration owner provides sufficient opaque correlation for that guarantee; its representation and the internal routing of UI facts are not architecture contracts, and QML must not reconstruct lifecycle identity from individual loading, target, spread, or presentation properties. Panel size and visibility are canonical UI-local state; QML reports their current facts through owner inputs so action checked state and Escape routing can mirror the layout without becoming another panel-state owner. Fullscreen state is not part of this panel state and remains owned by the application shell. Shared workspace code communicates with image and video viewports only through their declared interaction contracts and must not reach into mode-specific internals.

The application exposes its image visual surface only through the KiriView image-viewport facade boundary, which owns dependency access and lifetime and hides the dependency surface from general QML code. The exact facade, visual-item, and attachment decomposition is not an architecture contract. Video playback, seeking, and backend attachment remain behind the video and session boundaries. QML may host the physical video-output surface and report its lifetime-correlated availability, but the video-output attachment owner controls attachment, detachment, stale-claim rejection, destroyed-surface handling, and public video attachment state.

## Source, Failure, And Resource Boundary

KiriView's `ImageSequence` provider owns source access, decode and refinement jobs, cache and predecode reuse, typed source failure values, application failure-registry records, and application display-store leases. It answers supported provider requests without owning or inferring presentation state.

A supported failure observation may expose a generic cause and opaque application failure reference. The ImageViewport integration owner resolves that reference only against the provider registry associated with the matching application target and publishes the KiriView-owned user-facing message. Registry retirement follows the public provider ownership callbacks rather than snapshot polling or QML state. Free-form application diagnostics do not cross the provider boundary.

Provider-resource, cache, display-store, and failure-registry owners may retire, reuse, or evict entries only according to their own lease policy and the supported provider ownership callbacks. QML load status, QML engine caching, observation polling, and visual-item destruction are not application resource authorities.

Cache or predecode availability may accelerate provider response but does not establish presentation readiness. Only a matching supported observation that confirms the accepted target's committed display may make image operations ready or retire loading feedback.
