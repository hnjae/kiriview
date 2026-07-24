# Repository Component Boundary

`VideoThumbnailExtraction` is a repository-internal component with one consumer, KiriView. It owns the capability from one admitted video-source request to one bounded representative image or typed failure. The application compiles and links it as a separately named library target and does not install or advertise an SDK, exported package target, compatibility version, QML module, or plugin.

## Ownership

The component owns:

- request and result values, public limits, typed failure causes, the move-only job handle, and callback delivery;
- request admission, representative-image selection, bounded scaling, output validation, terminal state, and cancellation suppression;
- read-only source access, the extraction deadline, Qt Multimedia integration, metadata observation, frame conversion, and extraction-only playback resources.

KiriView owns:

- source discovery, source-class eligibility, authorization and access-prerequisite lifetime, freshness, and navigation or session identity;
- thumbnail demand, priority, aggregate concurrency and resource pressure, preemption, cache lookup and installation, cache identity, publication, image-store lifetime, and stale-demand rejection;
- collection-entry access, direct-media routing, application playback state, user-visible failure projection, and localization.

The component receives a source URL only as extraction input, requests no write authority, and releases source-facing resources within the job lifecycle. It must not mutate the source or caller-managed metadata, create source-adjacent artifacts, derive application identity from the URL, traverse neighboring media, open a collection entry through application-private authority, install a cache record, publish an image-provider URL, or borrow a media player from application playback.

KiriView may adapt `VideoThumbnailExtractionJob` to an application job or provider port, but that adapter remains outside the component. The component must not depend on KiriView's async wrappers or callback types merely to satisfy application composition.

## Language And Build Baseline

The component follows the repository application build's ISO C++23, compiler, standard-library, and Qt baseline. Its implementation and supported headers compile as ISO C++23 with vendor language extensions disabled, and the target publishes that compile requirement to KiriView.

The canonical build target is `VideoThumbnailExtraction`. Public headers require only Qt Core, Qt GUI, and the C++ standard library. Qt Multimedia is a private target dependency. The component must not depend on Qt QML, Qt Quick, KIO, KDE Frameworks, the Rust support library, `KiriViewCore`, or application source directories.

```mermaid
flowchart LR
    KiriView[KiriView thumbnail policy] --> API[VideoThumbnailExtraction public API]
    API --> Component[Extraction component]
    Component --> QtMM[Qt Multimedia]
```

Arrows point in the allowed dependency direction. The component owns all coordination between request state, deadline state, and Qt Multimedia resources without exposing those mechanisms to KiriView. Internal object decomposition, event representation, multimedia adaptation, and deadline implementation remain implementation choices.

## Header Boundary

The supported include forms are defined by the [public specification](../spec/video-thumbnail-extraction.md#repository-internal-interface). The umbrella header is declaration-free and includes the canonical subject header. All supported declarations use the `kiriview` namespace and component-prefixed names.

The public subject header declares only public limits, request, result, failure, job, callback, and start-operation values. It must not include private workflow, backend, timer, media-fact, candidate-selection, effect-plan, player, sink, cache, navigation, session, collection, or test-support declarations.

Private headers remain within the component compilation boundary. They must not be exposed through supported include directories or KiriView consumer dependencies.

KiriView and the component evolve atomically in one repository. An intentional contract change updates the component and its sole consumer together and does not preserve duplicate signatures, app-owned aliases, legacy provider shapes, compatibility adapters, or ABI shims.

## Limit And Diagnostic Authority

`VideoThumbnailExtractionLimits` is the single immutable authority for the public output and diagnostic limits. Request admission, embedded-image acceptance, decoded-frame acceptance, scaling, terminal result construction, and diagnostic normalization consume that authority rather than duplicating constants.

Dimension and byte calculations are overflow-safe. Final admission uses `QImage::sizeInBytes()` rather than inferring output size solely from width, height, or an assumed pixel format. A successful result does not borrow pixel storage whose lifetime is controlled by Qt Multimedia or another source object.

The limits bound one operation's public output and component-retained diagnostics. They do not describe aggregate KiriView resource policy, allocator capacity, or platform-codec transient decode buffers. The component nevertheless bounds its retained candidate data, releases rejected or superseded images promptly, and stops multimedia work after terminal invalidation.

Backend messages and source URLs are untrusted diagnostic inputs. The component maps outcome categories to typed causes before text projection, normalizes and bounds any retained diagnostic, removes credentials and URL user information, and never branches on diagnostic wording.
