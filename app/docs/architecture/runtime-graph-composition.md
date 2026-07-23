# Runtime Graph Composition

Top-level document runtimes are public query and command shells around canonical state and named internal runtime graphs. A shell may retain its canonical state, accepted leaf snapshots, public command ports, and the graph itself, but it must not absorb the responsibilities of the graph's collaborating workflow, effect, or resource owners.

A runtime graph is the composition owner for one document boundary. It constructs internal owners in dependency order, binds their named ports, owns their joint lifetime, dispatches cross-owner workflow plans, and shuts them down in dependency-safe order. The graph is not a second domain-state owner: it receives state and snapshot access through explicit ports and does not expose mutable controller state as a parallel public API.

The document-session graph composes routing, public projection, active and direct-media navigation, displayed-media operations, still-image preparation, and video-output attachment. The document-session shell remains the canonical session-state owner and delegates cross-owner commands through the graph.

The image-document graph composes navigation, loading, `ImageViewport` integration, sequence-provider resources, preparation, and removal-fallback owners. The integration owner consumes only the dependency's supported interface and does not expose it as another application runtime owner. Each dependent application owner receives its prerequisite ports explicitly. A collaborator that needs a sibling snapshot or command must use a named port rather than capture the sibling merely to inspect or mutate its state.

Provider-resource composition separates application source identity, decode and cache reuse, entry retention, refinement work, and failure records from image presentation. Provider owners receive source, scope, page-role, and opaque public provider-request identities through their ports and use supported ownership callbacks to enforce reuse, cancellation, stale rejection, and application resource lifetime without becoming presentation owners.

Runtime graphs and resource coordinators are internal C++ boundaries. They must not change QML-facing contracts, user-visible behavior, or canonical state ownership.
