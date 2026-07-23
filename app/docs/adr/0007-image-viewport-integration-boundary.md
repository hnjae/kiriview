# ImageViewport Integration Boundary

## Context

KiriView needs image presentation behavior without maintaining its own competing zoom, pan, geometry, animation, and rendering state. Splitting those concerns between application runtimes, QML visual objects, and a presentation dependency creates duplicate authorities and couples application source workflows to presentation implementation details.

KiriView still owns navigation, source identity, decoding, cache policy, application failures, actions, and user-facing projections. Those responsibilities need a narrow adapter boundary to `ImageViewport` without making KiriView architecture authoritative for the dependency's private design.

## Decision

KiriView consumes only the supported `ImageViewport` interface. One image-document integration owner adapts application targets, commands, interaction facts, and source resources to that interface and correlates supported observations back to application source identity.

KiriView supplies image data through the supported `ImageSequence` provider boundary. Application source URLs, archive access, credentials, decoder objects, cache keys, navigation policy, and detailed failures remain behind the KiriView adapter.

KiriView treats dependency correlation values and application failure references as opaque and follows the public provider ownership contract. It does not include private dependency headers, invoke private entry points, infer private lifecycle from callback timing, or document private state, algorithms, scheduling, rendering, and resource management.

## Consequences

The KiriView image-document graph has an integration owner and provider-resource owners but no application-owned replacement presentation path. General QML code uses the KiriView facade, supplies raw interaction facts, and consumes application projections without depending directly on the dependency.

User-visible image behavior remains specified by KiriView product specifications. The dependency's own contract governs its public behavior and interface; KiriView architecture governs only how the application integrates with that boundary.

An intentional public interface change requires a corresponding KiriView integration change. A private dependency change does not require or authorize a KiriView architecture change.
