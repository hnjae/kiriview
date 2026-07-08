#pragma once

#include "renderadapter_p.h"
#include "viewportcontrollerrendercontract_p.h"

#include <QtCore/QRectF>

class ImageViewportPrivate;
class QSGNode;

class ImageViewportRenderHost
{
public:
    explicit ImageViewportRenderHost(ImageViewportPrivate& viewport);

    QSGNode* updatePaintNode(QSGNode* oldNode);
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry,
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);

private:
    ImageViewportPrivate& viewport;
    RenderAdapter renderAdapter;
};
