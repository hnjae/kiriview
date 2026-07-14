#pragma once

#include "imageviewportstate_p.h"
#include "renderfailurecause_p.h"
#include <ImageViewport/ImageViewport>

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
        ImageViewportPageRole role = ImageViewportPageRole::Primary;
        ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    };

    struct Input
    {
        struct ImageLayer
        {
            ImageViewportPageRole role = ImageViewportPageRole::Primary;
            ImageViewportInternal::PreparedPayload preparedPayload;
            QRectF targetRect;
            QRectF sourceRect;
            int rotationDegrees = 0;
            bool mirrorHorizontally = false;
            bool mirrorVertically = false;
        };

        QSizeF itemSize;
        ImageViewportBackgroundMode backgroundMode = ImageViewportBackgroundMode::Transparent;
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
            ImageViewportPageRole role = ImageViewportPageRole::Primary;
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
        ImageViewportPageRole failedRole = ImageViewportPageRole::Primary;
        RenderFailureCause failureCause = RenderFailureCause::None;
        QVector<BackgroundRect> backgroundRects;
        QVector<ImageLayer> imageLayers;
        bool smoothing = true;
        bool mipmap = false;
    };

    RenderPlan createPlan(const Input& input) const;
};
