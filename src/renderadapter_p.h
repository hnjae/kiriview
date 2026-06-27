#pragma once

#include "imageviewport.h"

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
        QImage image;
        QRectF targetRect;
        QRectF sourceRect;
        bool smoothing = true;
        bool mipmap = false;
        bool mirrorHorizontally = false;
        bool mirrorVertically = false;
        quint64 generation = 0;
        quint64 requestId = 0;
        quint64 preparedPayloadId = 0;
        QQuickWindow* window = nullptr;
    };

    struct Output
    {
        QSGNode* node = nullptr;
        CommitResult result = CommitResult::Empty;
        quint64 generation = 0;
        quint64 requestId = 0;
        quint64 preparedPayloadId = 0;
    };

    Output createNode(QSGNode* oldNode, const Input& input) const;
};
