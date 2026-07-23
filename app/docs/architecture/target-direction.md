# Policy Contracts

KiriView product policy is organized around meaningful workflow or algorithmic domains such as opening, navigation, deletion, page pairing, scan planning, format routing, and cache policy. Policy that does not need Qt remains value-oriented and must not become QObject state or signal plumbing without a runtime need.

Policy boundaries must clarify ownership, preserve value semantics, and let multiple runtime paths share one named decision without creating duplicate state. A product decision must not cross into the Rust support library; only the capabilities named by [Rust Support Boundary](rust-support-boundary.md) cross its bridge.

Policy units must be cohesive:

- Side-effect-free workflow policy that maps coherent inputs to explicit decisions.
- Geometry policy computes scan, nearest-point, or placement decisions without owning presentation objects.
- Format and routing policy inspects explicit inputs and returns classification decisions.
- Cache prioritization and navigation policy remain independent of Qt event loops and do not own Qt runtime objects.

Document-session policy consumes coherent application state rather than reconstructing decisions from unrelated property reads. Stable responsibilities include active navigation projection, direct media routing, ordinary direct media deletion fallback, action availability, and still-image-only direct media predecode eligibility.

Session-to-leaf boundaries provide coherent observations and owner-routed commands. Their internal representation may use snapshots, direct owner APIs, callbacks, or other application mechanisms, provided callers cannot mutate another owner's state or observe a partially committed transition.

The document session may centralize a concern or delegate it to collaborators according to cohesion and lifecycle needs. Regardless of internal decomposition, public mixed-media output still passes through the document-session state owner.

Runtime composition must not create a second owner for public projection commits, leaf-to-public mapping, route-local sequencing, or stale-completion acceptance. Refactoring may change component and call topology when those ownership and lifecycle contracts remain intact.

Scan-start and stepped-reading policy belong to the image-document integration layer, not QML. The policy computes scan or nearest-point plans from a matched public viewport observation, uses only supported coordinate queries and commands, and does not reproduce presentation geometry. The integration owner routes UI facts and rejects application commands or projections correlated to a superseded application target or dependency observation.

Those boundaries keep the document-session state owner as the public owner. Policy computes projections and decisions from coherent inputs; runtime owners apply the resulting state, execute document routing and Qt/KDE effects, publish coherent observable state, and preserve direct-media freshness checks for async completions.

Policy types must not depend on Qt runtime objects without a domain need. Qt value types may be used when they are the canonical application representation and do not obscure deterministic ownership or require an event loop.
