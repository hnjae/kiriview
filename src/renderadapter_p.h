#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"

#include <QtCore/QVector>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGNode>
#include <QtQuick/QSGTexture>

class RenderAdapter
{
public:
    enum class CommitResult {
        Empty,
        Committed,
        Failed,
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
        QQuickWindow* window = nullptr;
    };

    struct Output
    {
        struct RolePayload
        {
            ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
            ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
        };

        QSGNode* node = nullptr;
        CommitResult result = CommitResult::Empty;
        ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
        QVector<RolePayload> rolePayloads;
        ImageViewport::PageRole failedRole = ImageViewport::PageRole::Primary;
    };

    Output createNode(QSGNode* oldNode, const Input& input) const;
};
