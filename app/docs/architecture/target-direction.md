# Policy Contracts

KiriView product policy is native C++ organized around meaningful workflow or algorithmic domains such as opening, navigation, deletion, page pairing, scan planning, format routing, and cache policy. Policy that does not need Qt remains plain value-oriented C++ and must not become QObject state or signal plumbing merely to share the application language.

Policy boundaries must clarify ownership, preserve value semantics, and let multiple runtime paths share one named decision without creating duplicate state. A product decision must not cross into the Rust support library; only the media-support capabilities named by the language-boundary contract cross FFI.

Policy units must be cohesive:

- Workflow reducers that accept plain events and return state deltas plus effects.
- Geometry and rendering policy modules that compute values without owning renderer objects.
- Format and routing modules that inspect plain inputs and return typed classification values.
- Cache prioritization and navigation policies that remain independent of Qt event loops and do not own Qt runtime objects.

Document-session policy changes must cross coherent internal C++ contracts rather than individual property-shaped adapters. Stable boundaries include active navigation projection from session and image-document snapshots, direct media routing plans, ordinary direct media deletion fallback plans, action-availability projections, and a named still-image-only direct media predecode eligibility policy.

Session-to-leaf document boundaries expose cohesive snapshot families and explicit command/effect ports, not property-shaped signal bags or direct leaf facade queries. Projection refresh, thumbnail-source adaptation, routing, page navigation, deletion, playback stop, and video-output attachment must cross the session boundary through typed ports whose ownership is visible at the boundary.

Document-session subowners are justified only when a concern needs its own lifecycle, stale-completion checks, projection commit order, or grouped command/effect ports. Regardless of internal decomposition, public output still passes through the document-session state owner.

Session runtime code may supply committed snapshots and bind ports, but it must not become a second owner for projection commits, leaf-to-public mapping, route-local sequencing, or stale-completion acceptance. These boundaries stay stable at the contract level across internal refactoring that preserves the same ownership, port, and stale-completion contracts.

Scan-start and stepped-reading policy belong to the C++ image-document integration layer, not QML. The policy computes scan or nearest-point plans from a plain matched component snapshot. `ImageViewport` owns point-query geometry, anchored zoom resolution, final pan and zoom state, command revisions, and snapshot ordering; the integration owner routes UI facts and rejects application commands or projections correlated to superseded component generations.

Those boundaries keep the document-session state owner as the public owner. Plain C++ policy computes projections and plans from snapshots; runtime owners apply the resulting state, execute document routing and Qt/KDE effects, publish signal batches, and preserve cursor-generation rejection for stale async completions.

Policy types must not depend on Qt runtime objects merely because they live in C++. Qt value types may be used when they are the canonical application representation and do not obscure deterministic ownership or require an event loop.
