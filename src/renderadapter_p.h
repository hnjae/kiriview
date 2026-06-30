#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"

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
        QSizeF itemSize;
        ImageViewport::BackgroundMode backgroundMode = ImageViewport::BackgroundMode::Transparent;
        QColor backgroundColor = Qt::transparent;
        ImageViewportInternal::PreparedPayload preparedPayload;
        QRectF targetRect;
        QRectF sourceRect;
        bool smoothing = true;
        bool mipmap = false;
        bool mirrorHorizontally = false;
        bool mirrorVertically = false;
        QQuickWindow* window = nullptr;
    };

    struct Output
    {
        QSGNode* node = nullptr;
        CommitResult result = CommitResult::Empty;
        ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    };

    Output createNode(QSGNode* oldNode, const Input& input) const;
};
