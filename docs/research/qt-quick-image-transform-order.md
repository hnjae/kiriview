---
title: Qt Quick Image transform order
researched_on: 2026-06-21
---

# Qt Quick Image Transform Order

This note records how Qt Quick `Image` appears to order image loading, placement, mirroring, sampling, and item-level transforms, based on Qt 6 documentation and `qtdeclarative` commit `355fbd45bc7659387be1cce5e86608c88210f25e`.

## Sources

- Qt documentation: [`Image` QML Type](https://doc.qt.io/qt-6/qml-qtquick-image.html)
- Qt documentation: [`Item` QML Type](https://doc.qt.io/qt-6/qml-qtquick-item.html)
- Qt documentation: [`Transform` QML Type](https://doc.qt.io/qt-6/qml-qtquick-transform.html)
- Qt source: [`QQuickImage::updatePaintNode`](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickimage.cpp#L721-L868)
- Qt source: [`QQuickImageBase::setSourceClipRect`](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickimagebase.cpp#L147-L164)
- Qt source: [`QQuickImageBase::setAutoTransform`](https://github.com/qt/qtdeclarative/blob/355fbd45bc7659387be1cce5e86608c88210f25e/src/quick/items/qquickimagebase.cpp#L482-L498)

## Findings

Qt `Image` separates source preparation concerns from scene graph presentation transforms. `sourceSize`, `sourceClipRect`, `autoTransform`, and target color space affect loading, provider options, decoded image preparation, or resulting image characteristics before local scene graph presentation is chosen in `QQuickImage::updatePaintNode`. In particular, `autoTransform` is represented as a `QQuickImageProviderOptions` value rather than a later item transform, so metadata orientation should be treated as source-frame normalization.

`fillMode` and alignment determine the image node's target rectangle and source rectangle. At the referenced `qtdeclarative` commit, `updatePaintNode` first computes the displayed pixel dimensions used for alignment, then computes `targetRect` and `sourceRect` according to `Stretch`, `PreserveAspectFit`, `PreserveAspectCrop`, tiling modes, or `Pad`.

`PreserveAspectFit` maps the full source image into an aspect-preserving target rectangle positioned by horizontal and vertical alignment. `PreserveAspectCrop` maps the item rectangle as the target and chooses a cropped source rectangle according to the same alignment settings. `Pad` maps an unscaled or clipped source region into an aligned target region.

`smooth` and `mipmap` are sampling controls, not geometry controls. They affect texture filtering and mipmap filtering after the source and target rectangles have been chosen.

`mirror` and `mirrorVertically` are applied to the image scene graph node after the target rectangle and normalized source rectangle are set. Behaviorally, this means mirroring should be understood as flipping the selected source content inside the chosen displayed rectangle, not as changing layout, alignment, or item geometry.

Qt `Image` does not define an image-specific rotation property. Rotation in ordinary QML usage comes from `Item::rotation` or from the `Item::transform` list. Those transforms operate on the whole item after the `Image` item has produced its local painted content. The `Transform` documentation states that transforms in the list are applied in declaration order.

## Recommended Order For ImageViewport

`ImageViewport` should follow this order for behavior compatible with the mental model of Qt Quick `Image`.

1. Normalize the supplied frame according to source-level metadata policy, including EXIF-style orientation when enabled.
2. Identify the source-space content represented by the supplied frame. Region and level-of-detail selection are deferred source-model concerns until ImageViewport defines an explicit provider protocol.
3. Compute the base image-to-item mapping from placement policy, aspect fit/crop/stretch/pad behavior, and alignment.
4. Apply horizontal and vertical mirroring inside the selected source-to-target mapping.
5. Compose ImageViewport-specific viewport state, such as zoom and pan, into the image-to-item mapping.
6. Apply sampling policy, including smooth or nearest-neighbor sampling and mipmap filtering, while rendering the mapped content.
7. Leave inherited `QQuickItem` transforms, such as `rotation`, `scale`, and `transform`, outside the ImageViewport image-to-item mapping unless an API explicitly asks for scene or window coordinates.

Coordinate conversion APIs should reflect the final local geometry mapping through step 5. Sampling in step 6 should not alter coordinate conversion, and coordinate conversion should not silently include parent item transforms when converting between image coordinates and local item coordinates.

## Design Notes

Metadata orientation and explicit content rotation should remain distinct if explicit content rotation is added later. Metadata orientation normalizes the source frame before layout; explicit content rotation would be a viewport presentation transform rather than inherited `QQuickItem` rotation.

Mirroring should not change the rendered content rectangle. It should reverse the coordinate mapping inside that rectangle, which keeps it compatible with Qt `Image` behavior and with alignment expectations.

If `ImageViewport` exposes both local item coordinate conversion and scene/window coordinate conversion, the API should name those spaces explicitly because Qt `Image` relies on normal `QQuickItem` transforms outside its own image presentation.
