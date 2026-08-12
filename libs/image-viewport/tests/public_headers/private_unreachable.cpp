// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#if __has_include("viewportengine_p.h")
#error "ImageViewport consumers must not resolve private engine headers"
#endif

#if __has_include("viewportproviderbridge_p.h")
#error "ImageViewport consumers must not resolve private provider-transport headers"
#endif

#if __has_include("imageviewportrenderhost_p.h")
#error "ImageViewport consumers must not resolve private render-host headers"
#endif

#if __has_include("renderadapter_scenegraph_p.h")
#error "ImageViewport consumers must not resolve private scenegraph headers"
#endif

#if __has_include("imageviewport_testhooks_p.h")
#error "ImageViewport consumers must not resolve private instrumentation headers"
#endif
