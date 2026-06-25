#include "renderadapter_p.h"

#include <QtQuick/QSGSimpleRectNode>

#include <algorithm>

RenderAdapter::Output RenderAdapter::createNode(QSGNode *oldNode, const Input &input) const
{
    delete oldNode;

    if (input.itemSize.width() <= 0.0 || input.itemSize.height() <= 0.0) {
        return {};
    }

    const bool hasBackground = input.backgroundMode != ImageViewport::BackgroundMode::Transparent;
    if (!hasBackground && input.image.isNull()) {
        return {};
    }

    auto *root = new QSGNode;
    if (input.backgroundMode == ImageViewport::BackgroundMode::SolidColor) {
        root->appendChildNode(new QSGSimpleRectNode(QRectF(0.0, 0.0, input.itemSize.width(), input.itemSize.height()), input.backgroundColor));
    } else if (input.backgroundMode == ImageViewport::BackgroundMode::Checkerboard) {
        constexpr double checkerboardTileSize = 8.0;
        const QColor lightSquare(238, 238, 238);
        const QColor darkSquare(204, 204, 204);
        for (double y = 0.0; y < input.itemSize.height(); y += checkerboardTileSize) {
            for (double x = 0.0; x < input.itemSize.width(); x += checkerboardTileSize) {
                const int column = static_cast<int>(x / checkerboardTileSize);
                const int row = static_cast<int>(y / checkerboardTileSize);
                const QColor color = ((row + column) % 2 == 0) ? lightSquare : darkSquare;
                const QRectF tile(x,
                    y,
                    std::min(checkerboardTileSize, input.itemSize.width() - x),
                    std::min(checkerboardTileSize, input.itemSize.height() - y));
                root->appendChildNode(new QSGSimpleRectNode(tile, color));
            }
        }
    }

    if (input.image.isNull()) {
        return {root, CommitResult::Empty};
    }

    if (!input.window) {
        delete root;
        return {nullptr, CommitResult::Failed};
    }

    QSGTexture *texture = input.window->createTextureFromImage(input.image);
    QSGImageNode *imageNode = input.window->createImageNode();
    if (!texture || !imageNode) {
        delete texture;
        delete imageNode;
        delete root;
        return {nullptr, CommitResult::Failed};
    }

    imageNode->setTexture(texture);
    imageNode->setOwnsTexture(true);
    imageNode->setRect(input.targetRect);
    imageNode->setSourceRect(input.sourceRect);
    imageNode->setFiltering(input.smoothing ? QSGTexture::Linear : QSGTexture::Nearest);
    imageNode->setMipmapFiltering(input.mipmap ? QSGTexture::Linear : QSGTexture::None);
    QSGImageNode::TextureCoordinatesTransformMode transform = {};
    if (input.mirrorHorizontally) {
        transform |= QSGImageNode::MirrorHorizontally;
    }
    if (input.mirrorVertically) {
        transform |= QSGImageNode::MirrorVertically;
    }
    imageNode->setTextureCoordinatesTransform(transform);
    root->appendChildNode(imageNode);
    return {root, CommitResult::Committed};
}
