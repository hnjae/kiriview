#pragma once

#include "imageviewport.h"

#include <QtCore/QVariantMap>

class PresentationGeometry
{
public:
    struct State {
        bool hasReadyDisplay = false;
        QRectF itemBounds;
        QSizeF imageSize;
        ImageViewport::FillMode fillMode = ImageViewport::FillMode::Contain;
        ImageViewport::HorizontalAlignment horizontalAlignment = ImageViewport::HorizontalAlignment::AlignHCenter;
        ImageViewport::VerticalAlignment verticalAlignment = ImageViewport::VerticalAlignment::AlignVCenter;
        bool mirrorHorizontally = false;
        bool mirrorVertically = false;
        double zoom = 1.0;
        QPointF pan;
    };

    static QRectF contentRect(const State &state);
    static QRectF visibleImageRect(const State &state);
    static QVariantMap itemToImage(const State &state, double x, double y);
    static QVariantMap imageToItem(const State &state, double x, double y);
    static bool containsVisibleImagePoint(const State &state, double x, double y);
    static QVariantMap invalidCoordinateResult();
};
