# Policy Boundary Contracts

KiriView uses coherent Rust policy units rather than many small FFI entry points. A Rust module must represent a meaningful workflow or algorithmic domain, such as opening, navigation, deletion, page pairing, scan planning, parsing, or cache policy. Component-owned viewport geometry, zoom reduction, and payload-demand projection are not application FFI policy domains.

Small local branches stay in C++ unless they are part of a larger Qt-independent policy decision. Rust policy boundaries must clarify ownership, preserve value-based tests, or let multiple C++ runtime paths share one policy result. A branch must not cross the language boundary when the bridge types and conversions are larger than the decision being represented.

Policy boundaries must be fewer and larger:

- Workflow reducers that accept plain events and return state deltas plus effects.
- Geometry and rendering policy modules that compute values without owning renderer objects.
- Parsing modules that inspect bytes and return plain metadata or decoded domain values.
- Cache prioritization and navigation policies that can be tested without Qt event loops and do not own Qt runtime objects.

Document-session language-boundary changes must cross coherent contracts rather than individual property-shaped adapters. Stable boundaries include active navigation projection from session and image-document snapshots, direct media routing plans, ordinary direct media deletion fallback plans, action-availability projections, and a named still-image-only direct media predecode eligibility policy.

Session-to-leaf document boundaries expose cohesive snapshot families and explicit command/effect ports, not property-shaped signal bags or direct leaf facade queries. Projection refresh, thumbnail-source adaptation, routing, page navigation, deletion, playback stop, and video-output attachment must cross the session boundary through typed ports whose ownership is visible at the boundary.

Document-session subowners are justified only when a concern needs its own lifecycle, stale-completion checks, projection commit order, or grouped command/effect ports. Regardless of internal decomposition, public output still passes through the document-session state owner.

Session runtime code may supply committed snapshots and bind ports, but it must not become a second owner for projection commits, leaf-to-public mapping, route-local sequencing, or stale-completion acceptance. These boundaries stay stable at the contract level across internal refactoring that preserves the same ownership, port, and stale-completion contracts.

Scan-start and stepped-reading policy belong to the image-document integration layer, not QML. Rust may compute scan or nearest-point plans from a plain matched component snapshot. `ImageViewport` owns point-query geometry, anchored zoom resolution, final pan and zoom state, command revisions, and snapshot ordering; the C++ integration owner routes UI facts and rejects application commands or projections correlated to superseded component generations.

Those boundaries keep the document-session state owner as the public C++ owner and keep Qt/KDE effects in C++. Rust hosts pure projection or plan computation only from plain snapshots; C++ applies the resulting state, executes document routing, publishes signal batches, and preserves cursor-generation rejection for stale async completions.

C++ remains the place where those plans are executed through Qt/KDE APIs.
