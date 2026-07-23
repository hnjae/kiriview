# Architecture Index

This directory defines the durable end-state architecture contract for `VideoThumbnailExtraction`. It describes component ownership, dependency direction, lifecycle, trust boundaries, and resource authority rather than implementation progress or verification inventory.

- [Repository Component Boundary](component-boundary.md)
- [Extraction Runtime](extraction-runtime.md)

## Traceability

| Public contract                                                                                                                                                     | Primary architecture enforcement                                                                                                                                                                           |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Request admission, result invariants, typed failure, and output limits in [Video Thumbnail Extraction](../spec/video-thumbnail-extraction.md)                       | Public/private header partition and limit authority in [Repository Component Boundary](component-boundary.md), with workflow admission and terminal mapping in [Extraction Runtime](extraction-runtime.md) |
| Representative-image freedom and absence of frame or timestamp guarantees in [Video Thumbnail Extraction](../spec/video-thumbnail-extraction.md)                    | Workflow versus multimedia-adapter ownership in [Extraction Runtime](extraction-runtime.md)                                                                                                                |
| Exact-once completion, receiver affinity, cancellation suppression, and destruction behavior in [Video Thumbnail Extraction](../spec/video-thumbnail-extraction.md) | Runtime serialization and terminal ordering in [Extraction Runtime](extraction-runtime.md)                                                                                                                 |
| Repository-internal C++23 include surface and excluded declarations in [Video Thumbnail Extraction](../spec/video-thumbnail-extraction.md)                          | Target, dependency, and header enforcement in [Repository Component Boundary](component-boundary.md)                                                                                                       |
