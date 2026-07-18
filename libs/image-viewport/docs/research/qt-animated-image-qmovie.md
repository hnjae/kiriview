---
researched_on: 2026-06-21
---

# Qt AnimatedImage And QMovie Playback Internals

This note records how Qt Quick `AnimatedImage` delegates animated image playback to `QMovie`, based on Qt 6 documentation, `qtdeclarative` commit `355fbd45bc7659387be1cce5e86608c88210f25e`, and `qtbase` commit `bc3078624441930150acd6338fb4c31a30b55f59`.

## Sources

- Qt documentation: [`AnimatedImage` QML Type](https://doc.qt.io/qt-6/qml-qtquick-animatedimage.html)
- Qt documentation: [`QMovie`](https://doc.qt.io/qt-6/qmovie.html)
- Qt documentation: [`QImageReader`](https://doc.qt.io/qt-6/qimagereader.html)
- Qt documentation: [`QImageIOHandler`](https://doc.qt.io/qt-6/qimageiohandler.html)
- Qt source: [`QQuickAnimatedImagePrivate::infoForCurrentFrame`](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickanimatedimage.cpp#L20-L49)
- Qt source: [`QQuickAnimatedImagePrivate` storage](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickanimatedimage_p_p.h#L33-L65)
- Qt source: [`QQuickAnimatedImage::load`](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickanimatedimage.cpp#L413-L460)
- Qt source: [`QQuickAnimatedImage::movieRequestFinished`](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickanimatedimage.cpp#L462-L532)
- Qt source: [`QQuickAnimatedImage::movieUpdate`](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickanimatedimage.cpp#L534-L558)
- Qt source: [`QMoviePrivate` state](https://github.com/qt/qtbase/blob/bc3078624441930150acd6338fb4c31a30b55f59/src/gui/image/qmovie.cpp#L162-L243)
- Qt source: [`QMoviePrivate::infoForFrame`](https://github.com/qt/qtbase/blob/bc3078624441930150acd6338fb4c31a30b55f59/src/gui/image/qmovie.cpp#L310-L421)
- Qt source: [`QMoviePrivate::next` and `_q_loadNextFrame`](https://github.com/qt/qtbase/blob/bc3078624441930150acd6338fb4c31a30b55f59/src/gui/image/qmovie.cpp#L434-L520)
- Qt source: [`QMovie::start`, `stop`, `setScaledSize`, and `supportedFormats`](https://github.com/qt/qtbase/blob/bc3078624441930150acd6338fb4c31a30b55f59/src/gui/image/qmovie.cpp#L954-L1025)

## Findings

Qt Quick `AnimatedImage` is implemented as a `QQuickImage` subclass that owns a `QMovie`. Its private state stores playback flags, speed, loop bookkeeping, a `QMovie *movie`, an optional network reply, and a per-frame `QMap<int, QQuickPixmap *> frameMap`.

`AnimatedImage` does not use `QQuickImageProvider` for animated image content. The QML documentation and source comments state that `AnimatedImage` can load image formats supported by Qt from URL schemes supported by Qt, but is not compatible with `QQuickImageProvider`.

The load path first resolves the QML `source` URL. If it resolves to a local file or Qt resource path, `AnimatedImage` constructs `QMovie` from the local path and completes immediately. Otherwise, when QML networking is enabled, it starts a `QNetworkReply`, then wraps that reply in `QMovie` after the request finishes; the reply is reparented to the movie so it remains alive as the movie's data source.

After the movie is accepted as valid, `AnimatedImage` connects `QMovie::stateChanged`, `QMovie::frameChanged`, and `QMovie::finished` to its own state and display update slots. It applies `cache` by switching the movie to `QMovie::CacheAll`, applies speed by converting the QML ratio to a QMovie percentage, starts playback when `playing` is true, and sets the first pixmap from the current movie frame.

On each `QMovie::frameChanged`, `AnimatedImage` refreshes the displayed image by asking `QMovie` for the current frame number and current image, wrapping that image in `QQuickPixmap`, and passing the pixmap to the inherited `QQuickImage` display path. When `cache` is false, `AnimatedImage` clears its own `QQuickPixmap` frame cache before inserting the current frame.

`QMovie` itself is a `QImageReader`-based playback helper. Its private state includes a `QImageReader`, a single-shot `QTimer`, current and next frame numbers, a current `QPixmap`, frame delay state, loop counter state, and an optional `std::map<int, QFrameInfo>` frame cache.

`QMoviePrivate::infoForFrame` is the central frame acquisition path. It asks the image handler whether the format supports `QImageIOHandler::Animation`; animated handlers are read until `canRead()` becomes false because the frame count may be unknown, while non-animated multi-image formats use `imageCount()` and a default 1000 ms frame delay.

With `CacheNone`, `QMovie` normally reads frames sequentially through `QImageReader::read()`. Non-sequential access attempts `jumpToImage`; rewinding to frame 0 can recreate the `QImageReader` and seek the device back to its initial position, but this fails for sequential devices. This explains the documented restriction that looping from a sequential device requires `CacheAll`.

With `CacheAll`, `QMovie` reads forward until the requested frame is available and stores each decoded frame as `QPixmap` plus delay in its frame map. This cache is CPU-side/GUI-side decoded frame storage from the perspective of Qt Quick, not a prepared scene graph texture cache.

Playback timing is timer driven. `QMoviePrivate::next` records the time spent reading the frame, stores the next frame delay after applying `speed`, subtracts frame processing time from the delay, and `_q_loadNextFrame` emits `updated`, `frameChanged`, and restarts the single-shot timer while the movie is running.

`QMovie::supportedFormats()` starts from `QImageReader::supportedImageFormats()` and filters out formats whose image handler does not report `QImageIOHandler::Animation`. In practice, `QMovie` support is therefore bounded by Qt image plugins and image handlers that advertise animation support.

## Implications For ImageViewport

Qt's built-in stack is source-centric: `AnimatedImage` owns the load URL and adapts local files or completed network replies into `QMovie`. It does not model a caller-supplied decoded sequence provider, archive member provider, or application-owned decoder.

Qt's built-in caching has two relevant layers: `QMovie::CacheAll` stores decoded `QPixmap` frames inside the movie, and `AnimatedImage` can additionally keep `QQuickPixmap` entries per frame. Neither layer is an explicit render-thread prepared texture cache with caller-visible prefetch intent.

`ImageViewport` should keep the sequence playback behavior that Qt users expect from `AnimatedImage`, but its provider design should avoid depending on `QMovie` as the only sequence source. A caller-provided decoder can preserve per-frame durations and frame count semantics without requiring Qt image handler animation support.

If `ImageViewport` later offers a compatibility loader for ordinary animated image URLs, a `QMovie`-backed source adapter could be useful. That adapter should remain one provider implementation, not the core viewport model.

The cache direction already chosen for `ImageViewport` remains distinct from Qt's current implementation: decoded-frame caching corresponds roughly to `QMovie::CacheAll`, while prepared texture caching is a separate render-thread optimization that Qt `AnimatedImage` does not expose as a policy surface.
