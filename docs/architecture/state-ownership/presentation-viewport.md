# Presentation And Viewport

## Image Presentation Ownership

The image-presentation runtime is the single mutable owner of presentation mode, reading direction, transition phase, committed presentation, pending target, zoom, rotation, logical viewport frame, visible source region, scan handoff, page-role visibility, display-source projections, restoration state, and display-refinement demand. Rust may compute geometry or transition policy from snapshots but does not retain live presentation state.

Display-source state for each page role is an explicit variant: empty, provider-ready, retained committed presentation, or display error. Only provider-ready and retained variants may project a visible provider entry. A display error may remain the active target without pretending that a provider image exists.

Presentation transitions distinguish the previously committed presentation, a transition in which no partial target may be exposed, and the fully committed target. Preparing page resources must not publish a partial spread or copy presentation state into page-resource owners. Failure restores the retained committed presentation when the product contract allows retention.

Page-resource owners retain decoded payloads, display entries, animation resources, pending-load retention, reuse leases, and predecode facts. They may validate a reported load outcome against the retained provider identity and release its matching lease, but the image-presentation runtime alone owns display-load acknowledgment state and the resulting presentation transition. Page-resource owners must not own or expose mutable zoom, rotation, viewport, page visibility, reading direction, transition, or display-policy state.

## Viewport Command Boundary

C++ owns the canonical logical viewport frame and revisioned commands for fit, zoom, pan, scan, rotation, resizing, and position clamping. QML owns physical scrolling items, gesture samples, inertia, overshoot, and animation. QML applies only the latest applicable command and reports physical observations or acknowledgments back to the owner.

Owner-issued revisions that cross QML are opaque tokens. QML must not generate them, narrow them into numeric types, compare them arithmetically, or replay a superseded command. Delayed UI callbacks may report observations but must not overwrite canonical viewport state.

The presentation owner records whether a command was applied, superseded, rejected, or reconciled by a newer physical observation. Anchored zoom and scan commands may consume UI-local pointer anchors or deltas, but QML must not calculate the resulting canonical zoom, scan target, or content position.

The presentation runtime also owns the selected fit-mode projection used by checked menu and toolbar state. Manual zoom may preserve the last fit-mode selection for later use without making QML a second owner.

## Media Workspace Boundary

QML owns layout composition of the media viewport, layout-reserving panels, runtime-only panel size and visibility, delegate selection, focus handoff, and context-menu signal forwarding. Shared workspace code communicates with image and video viewports only through their declared interaction contracts and must not reach into mode-specific internals.

Image-specific pan, zoom, spread, and provider-backed item instances remain behind the image interaction boundary. Video playback, seeking, and backend attachment remain behind the video and session boundaries. QML may own the physical video-output item and report an owner-tokened surface claim, but C++ owns attachment, detachment, stale-claim rejection, destroyed-item handling, and public attachment state.

## Display And Render Context

Display-source projection and immutable provider entries are the image visual boundary. QML supplies viewport and scroll observations only; it must not compute display buckets, provider identity, refinement demand, fallback quality, or cache retention.

A non-rendering context owner supplies attachment, scene, device-pixel-ratio, and conservative safe display-limit facts. It must not draw, use private rendering interfaces, own textures, or schedule decode work. Changes to those facts advance render-context freshness so obsolete decode or refinement completions cannot replace the accepted presentation.

Image presentation and page-resource owners translate accepted visible demand into bounded whole-image publication. Production display must not schedule visual tiles, maintain decoded visual-tile caches, or own low-level scene-graph resources.
