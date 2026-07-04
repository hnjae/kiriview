#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "renderfailurecause_p.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QVector>
#include <QtGui/QColor>

class RenderAdapter
{
public:
    enum class CommitResult {
        Empty,
        Committed,
        Failed,
    };

    struct RolePayload
    {
        ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
        ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    };

    struct Input
    {
        struct ImageLayer
        {
            ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
            ImageViewportInternal::PreparedPayload preparedPayload;
            QRectF targetRect;
            QRectF sourceRect;
            int rotationDegrees = 0;
            bool mirrorHorizontally = false;
            bool mirrorVertically = false;
        };

        QSizeF itemSize;
        ImageViewport::BackgroundMode backgroundMode = ImageViewport::BackgroundMode::Transparent;
        QColor backgroundColor = Qt::transparent;
        ImageViewportInternal::PreparedPayload preparedPayload;
        QRectF targetRect;
        QRectF sourceRect;
        int rotationDegrees = 0;
        bool smoothing = true;
        bool mipmap = false;
        bool mirrorHorizontally = false;
        bool mirrorVertically = false;
        QVector<ImageLayer> imageLayers;
    };

    struct RenderPlan
    {
        struct BackgroundRect
        {
            QRectF rect;
            QColor color;
        };

        struct ImageLayer
        {
            ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
            ImageViewportInternal::PreparedPayload preparedPayload;
            ImageViewportInternal::PreparedPayloadIdentity preparedPayloadIdentity;
            QRectF targetRect;
            QRectF unrotatedTargetRect;
            QRectF sourceRect;
            QRectF physicalSourceRect;
            int rotationDegrees = 0;
            bool mirrorHorizontally = false;
            bool mirrorVertically = false;
        };

        CommitResult result = CommitResult::Empty;
        ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
        QVector<RolePayload> rolePayloads;
        ImageViewport::PageRole failedRole = ImageViewport::PageRole::Primary;
        RenderFailureCause failureCause = RenderFailureCause::None;
        QVector<BackgroundRect> backgroundRects;
        QVector<ImageLayer> imageLayers;
        bool smoothing = true;
        bool mipmap = false;
    };

    RenderPlan createPlan(const Input& input) const;
};
