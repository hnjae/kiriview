#pragma once

#include "imageviewport.h"

#include <QtCore/QVariantMap>

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
        ImageViewport::FillMode fillMode = ImageViewport::FillMode::Contain;
        ImageViewport::HorizontalAlignment horizontalAlignment
            = ImageViewport::HorizontalAlignment::AlignHCenter;
        ImageViewport::VerticalAlignment verticalAlignment
            = ImageViewport::VerticalAlignment::AlignVCenter;
        int rotationDegrees = 0;
        bool mirrorHorizontally = false;
        bool mirrorVertically = false;
        double zoom = 1.0;
        double devicePixelRatio = 1.0;
        QPointF pan;
    };

    static QSizeF spreadSize(const State& state);
    static QRectF primaryPageRect(const State& state);
    static QRectF secondaryPageRect(const State& state);
    static QRectF contentRect(const State& state);
    static QRectF visibleImageRect(const State& state);
    static QRectF visibleSpreadRect(const State& state);
    static QRectF visiblePageRect(const State& state, ImageViewport::PageRole role);
    static QRectF pageItemRect(const State& state, ImageViewport::PageRole role);
    static QVariantMap itemToImage(const State& state, double x, double y);
    static QVariantMap imageToItem(const State& state, double x, double y);
    static QVariantMap itemToSpread(const State& state, double x, double y);
    static QVariantMap spreadToItem(const State& state, double x, double y);
    static QVariantMap itemToPage(
        const State& state, ImageViewport::PageRole role, double x, double y);
    static QVariantMap pageToItem(
        const State& state, ImageViewport::PageRole role, double x, double y);
    static bool containsVisibleImagePoint(const State& state, double x, double y);
    static bool containsVisibleSpreadPoint(const State& state, double x, double y);
    static bool containsVisiblePagePoint(
        const State& state, ImageViewport::PageRole role, double x, double y);
    static QVariantMap invalidCoordinateResult();
};
