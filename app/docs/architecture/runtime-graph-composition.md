# Runtime Composition

Top-level document runtimes expose public queries and commands around canonical state and collaborating workflow, effect, and resource responsibilities. Runtime composition owns collaborator construction, joint lifetime, dependency-safe shutdown, and the connections needed to route commands and observations without creating another domain-state authority.

The internal composition mechanism is not an architecture contract. A runtime may use direct members, focused collaborators, coordinators, or an explicit graph when the chosen structure keeps ownership visible, avoids dependency cycles, and prevents mutation through observation-only boundaries.

Document-session composition covers routing, public projection, active and direct-media navigation, displayed-media operations, still-image preparation, and video-output attachment. The document-session state owner remains authoritative for public mixed-media state regardless of how those responsibilities are grouped.

Image-document composition covers navigation, loading, `ImageViewport` integration, sequence-provider resources, preparation, and removal fallback. The integration responsibility consumes only the dependency's supported interface. Collaborators may communicate through any explicit application boundary that preserves the recipient's ownership and lifecycle checks; a direct reference is not itself an ownership violation.

Provider-resource composition keeps application source access, reusable payloads, refinement work, failure records, and resource lifetime separate from image presentation. It preserves application identity, opaque dependency correlation, cancellation, stale rejection, and supported ownership callbacks without becoming a presentation owner.

Internal composition may change without an architecture update when QML-facing contracts, user-visible behavior, canonical ownership, dependency direction, and lifecycle guarantees remain unchanged.
