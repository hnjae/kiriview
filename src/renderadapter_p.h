#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "renderfailurecause_p.h"

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

    class SceneGraphFactory
    {
    public:
        virtual ~SceneGraphFactory() = default;
        virtual QSGTexture* createTexture(QQuickWindow* window, const QImage& image,
            QQuickWindow::CreateTextureOptions options) const;
        virtual QSGImageNode* createImageNode(QQuickWindow* window) const;
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
        const SceneGraphFactory* sceneGraphFactory = nullptr;
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
        RenderFailureCause failureCause = RenderFailureCause::None;
    };

    Output createNode(QSGNode* oldNode, const Input& input) const;
};
