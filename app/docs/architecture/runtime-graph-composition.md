# Runtime Graph Composition

Top-level document runtimes are public query and command shells around canonical state and named internal runtime graphs. A shell may retain its canonical state, accepted leaf snapshots, public command ports, and the graph itself, but it must not absorb the responsibilities of the graph's collaborating workflow, effect, or resource owners.

A runtime graph is the composition owner for one document boundary. It constructs internal owners in dependency order, binds their named ports, owns their joint lifetime, dispatches cross-owner workflow plans, and shuts them down in dependency-safe order. The graph is not a second domain-state owner: it receives state and snapshot access through explicit ports and does not expose mutable controller state as a parallel public API.

The document-session graph composes routing, public projection, active and direct-media navigation, displayed-media operations, still-image preparation, and video-output attachment. The document-session shell remains the canonical session-state owner and delegates cross-owner commands through the graph.

The image-document graph composes navigation, loading, presentation, page-resource, preparation, and removal-fallback owners. Later owners receive the ports of earlier dependencies explicitly. A collaborator that needs a sibling snapshot or command must use a named port rather than capture the sibling merely to inspect or mutate its state.

Page-resource composition separates image and display-source publication from entry retention and refinement work. Page-resource owners receive source, scope, page-role, and demand identities through their ports so they can enforce reuse, cancellation, and stale rejection without becoming presentation-state owners.

Runtime graphs and resource coordinators are internal C++ boundaries. They must not change QML-facing contracts, user-visible behavior, or canonical state ownership.
