#pragma once

#include "imageviewport.h"

class PresentationGeometry
{
public:
    struct State
    {
        bool hasReadyDisplay = false;
        QRectF itemBounds;
        QSizeF primaryImageSize;
        QSizeF secondaryImageSize;
        double pageGap = 0.0;
        ImageViewport::SpreadDirection spreadDirection
            = ImageViewport::SpreadDirection::LeftToRight;
        ImageViewport::FitMode fitMode = ImageViewport::FitMode::Contain;
        int rotationDegrees = 0;
        bool mirrorHorizontally = false;
        bool mirrorVertically = false;
        double manualZoom = 1.0;
        double devicePixelRatio = 1.0;
        QPointF contentPosition;
    };

    static QSizeF spreadSize(const State& state);
    static QRectF primaryPageRect(const State& state);
    static QRectF secondaryPageRect(const State& state);
    static QRectF contentRect(const State& state);
    static QSizeF contentSize(const State& state);
    static QPointF contentPosition(const State& state);
    static QPointF contentPositionForAnchoredSpreadPoint(
        const State& state, QPointF spreadPoint, QPointF itemPoint);
    static QPointF maximumContentPosition(const State& state);
    static bool horizontalPannable(const State& state);
    static bool verticalPannable(const State& state);
    static QRectF visibleImageRect(const State& state);
    static QRectF visibleSpreadRect(const State& state);
    static QRectF visiblePageRect(const State& state, ImageViewport::PageRole role);
    static QRectF pageItemRect(const State& state, ImageViewport::PageRole role);
    static CoordinateResult itemToImage(const State& state, double x, double y);
    static CoordinateResult imageToItem(const State& state, double x, double y);
    static CoordinateResult itemToSpread(const State& state, double x, double y);
    static CoordinateResult spreadToItem(const State& state, double x, double y);
    static CoordinateResult itemToPage(
        const State& state, ImageViewport::PageRole role, double x, double y);
    static CoordinateResult pageToItem(
        const State& state, ImageViewport::PageRole role, double x, double y);
    static bool containsVisibleImagePoint(const State& state, double x, double y);
    static bool containsVisibleSpreadPoint(const State& state, double x, double y);
    static bool containsVisiblePagePoint(
        const State& state, ImageViewport::PageRole role, double x, double y);
    static CoordinateResult invalidCoordinateResult();
};
