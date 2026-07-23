# Architecture Overview

KiriView is a KDE Kirigami image viewer built from cooperating UI, facade, runtime, policy, and media-support layers:

```mermaid
flowchart TD
    UI["QML / Kirigami UI composition"]
    Facade["C++ facade boundary"]
    Runtime["C++ Qt/KDE runtime and effect owners"]
    Policy["C++ product policy"]
    Support["Rust media-support static library"]

    UI --> Facade --> Runtime --> Policy
    Policy -- "typed plans, state deltas, and effect descriptions" --> Runtime
    Runtime --> Support
    Support -- "plain results and decoded payloads" --> Runtime
```

The C++ application owns product policy, runtime state, Qt object lifetime, KDE side effects, stale-completion rejection, and coherent QML projections. Qt-independent policy remains plain value-oriented C++ rather than becoming QObject plumbing. The statically linked Rust support library provides only the canonical allowlisted media and desktop-format capabilities defined by [Language Boundary](language-boundary.md).

## Dependency Direction

- QML owns declarative composition and consumes the facade as a placement and interaction surface only.
- The facade owns the QML API boundary, type conversion, and forwarding. It must not own domain workflow state.
- C++ policy and runtime owners own product decisions, Qt/KDE effects, async lifecycles, projections, and platform integration.
- The Rust support library accepts plain values or byte buffers and returns plain results, decoded payloads, or opaque capability-local handles. It must not own product policy, call Qt/KDE adapters, or publish QML-facing state.
- Shared support domains provide explicit capability snapshots or provider ports. Runtime owners consume those ports instead of probing platform state independently.

## Component Ownership Shape

The component graph is a responsibility contract, not a complete call graph:

```mermaid
flowchart TD
    UI["UI composition"]
    Facade["Facade boundary"]
    Shell["Application shell"]
    Session["Document session"]
    Image["Image runtime"]
    Video["Video runtime"]
    Navigation["Navigation and candidate lists"]
    Integration["ImageViewport integration"]
    Viewport["Repository-internal ImageViewport"]
    Provider["Image sequence provider resources"]
    Decoding["Image decode and metadata"]
    Collections["Opened collection access"]
    Predecode["Adjacent still-image preparation"]
    Actions["Actions, shortcuts, and UI gates"]
    System["System, localization, location, and async support"]
    Policy["C++ product policy"]
    Support["Rust media support"]

    UI --> Facade
    Facade --> Shell
    Facade --> Session
    Shell --> Actions
    Shell --> System
    Session --> Image
    Session --> Video
    Session --> Navigation
    Session --> Predecode
    Session --> Actions
    Session --> System
    Image --> Navigation
    Image --> Integration
    Image --> Decoding
    Image --> Collections
    Integration --> Viewport
    Viewport --> Provider
    Provider --> Decoding
    Video --> Navigation
    Video --> Collections
    Navigation --> Collections
    Shell --> Policy
    Session --> Policy
    Image --> Policy
    Video --> Policy
    Navigation --> Policy
    Integration --> Policy
    Provider --> Policy
    Decoding --> Policy
    Collections --> Policy
    Predecode --> Policy
    Decoding --> Support
    Session --> Support
```

- The document session owns top-level mixed-media routing, public source identity, active navigation projection, active zoom projection, title subject, displayed-media operation availability, displayed-media operation planning inputs, direct-media deletion follow-up, thumbnail-strip projection, and action-availability inputs.
- Image runtime owns image-mode loading, opened collection page state, viewport target and command adaptation, still-image provider resources, embedded image metadata, image-mode removal fallback facts, and image-specific navigation facts.
- Video runtime owns direct-video resolution, opened-collection video source-device acceptance, playback state, video status, video zoom readout, video metadata where supported, and playback-control readiness.
- Navigation owns candidate ordering, page/media cursor state, boundary facts, live direct-media refresh, and sibling archive discovery. It exposes snapshots and plans rather than public UI state.
- Collection access owns directly opened archive and directory listing, entry-byte access, entry metadata, and eligible collection-video playback devices. It must not update document, video, thumbnail, or QML state directly.
- The repository-internal `ImageViewport` component owns accepted image presentation, target transitions, geometry, per-role animation playback, render admission, and scene graph resources. The KiriView integration owner maps navigation targets and application commands to the component and correlates component generations back to application source identity without keeping a second presentation state.
- Image provider resources own source access, decode and refinement work, reusable whole-image payloads, predecode adoption, application cache and display-store pressure, typed failure detail, and provider handle leases. They answer component demand but do not own viewport presentation or rendering.
- Decoding owns route-specific image decoding, animation frame enumeration, metadata extraction, and whole-image refinement payloads. Decoder failures preserve typed diagnostics before any user-facing projection is derived.
- Predecode owns still-image-only adjacent preparation. Video rows may be cursor positions for scheduling, but they do not produce video-frame quick-navigation payloads.
- Actions own `QAction` identity, shortcut routing, accepted UI-gate revisions, command dispatch, and unsupported-media shortcut interception. QML reports UI-local gate facts and renders action placements.

## Build and Tooling Ownership

Each build boundary has one owned source and configuration inventory. Application builds, tests, lint, and editor tooling consume that inventory rather than maintaining divergent source lists or compiler settings.

The top-level CMake application build is the authority for the executable, production C++, generated configuration, QML resources, the repository-internal `ImageViewport` component, and application test targets. It requires ISO C++23 for every application-owned C++ target and generated CXX bridge translation unit, disables vendor C++ language extensions, and owns the shared minimum compiler, standard-library, Qt 6, and KDE Frameworks 6 baselines. Cargo owns the Rust support-library source inventory and produces one repository-internal `staticlib` plus generated CXX boundary artifacts for the CMake build to consume. Test builds own test-local artifacts but consume production targets and the same language and dependency baselines through these build boundaries rather than rebuilding production sources or selecting compatibility modes independently.

The application build owns one canonical installed application identity. Desktop metadata, icon identity, runtime metadata, generated configuration, and application artifacts must consume `org.hnjae.kiriview` from that authority and must not introduce alternate application IDs.

The Rust support library is linked into the application and has no independent installation, dynamic loading, stable ABI, or release-version contract. CXX may implement its typed bridge; CXX-Qt is not an application build or runtime boundary.

Derived build metadata follows the owner of the artifact it describes. Development tooling may orchestrate that metadata but must not become an independent authority for production or test compilation. Shared Qt, language-boundary, runtime, lint, and editor inputs come from one tooling context.
