# ImageViewport Packaging

This document defines public limits and the supported installed package boundary for downstream consumers of `ImageViewport`.

## Public Limits

`ImageSequenceLimits` is a QML singleton and C++ type for source admission limits. `ImageViewportDisplayLimits` is a QML singleton and C++ type for display-demand limits such as maximum manual zoom percent.

## Installed Package Boundary

The supported install target is Linux with a static Qt QML module plugin. Installed consumers use public headers and the package target only. Public headers must not expose private controller, provider transport, render adapter, scene graph, native texture, or instrumentation types.
