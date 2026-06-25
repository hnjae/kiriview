# Rendering Architecture

The library is structured as a Qt 6 QML module backed by C++ Qt Quick types.

`ImageViewport` is implemented as a `QQuickItem` subclass so it can participate directly in the Qt Quick scene graph. Rendering behavior is intentionally left as a scaffold concern for now; future implementation should choose the narrowest scene graph strategy that satisfies image display, scaling, and viewport interaction requirements.

The module boundary is the QML import:

```qml
import ImageViewport
```

Applications should consume the item through QML first. C++ classes in the backing library exist to implement the QML module rather than to define the primary public API.
