#include "renderadapter_p.h"

#include <QtQuick/QSGSimpleRectNode>

#include <algorithm>
#include <cmath>

RenderAdapter::Output RenderAdapter::createNode(QSGNode* oldNode, const Input& input) const
{
    delete oldNode;

    const auto& payload = input.preparedPayload;
    const ImageViewportInternal::PreparedPayloadIdentity payloadIdentity {
        payload.generation,
        payload.requestId,
        payload.payloadId,
    };
    if (input.itemSize.width() <= 0.0 || input.itemSize.height() <= 0.0) {
        return { nullptr, RenderAdapter::CommitResult::Empty, payloadIdentity };
    }

    const bool hasBackground = input.backgroundMode != ImageViewport::BackgroundMode::Transparent;
    if (!hasBackground && payload.image.isNull()) {
        return { nullptr, RenderAdapter::CommitResult::Empty, payloadIdentity };
    }

    auto* root = new QSGNode;
    if (input.backgroundMode == ImageViewport::BackgroundMode::SolidColor) {
        root->appendChildNode(
            new QSGSimpleRectNode(QRectF(0.0, 0.0, input.itemSize.width(), input.itemSize.height()),
                input.backgroundColor));
    } else if (input.backgroundMode == ImageViewport::BackgroundMode::Checkerboard) {
        constexpr double checkerboardTileSize = 8.0;
        const QColor lightSquare(238, 238, 238);
        const QColor darkSquare(204, 204, 204);
        const int rowCount
            = static_cast<int>(std::ceil(input.itemSize.height() / checkerboardTileSize));
        const int columnCount
            = static_cast<int>(std::ceil(input.itemSize.width() / checkerboardTileSize));
        for (int row = 0; row < rowCount; ++row) {
            const double y = row * checkerboardTileSize;
            for (int column = 0; column < columnCount; ++column) {
                const double x = column * checkerboardTileSize;
                const QColor color = ((row + column) % 2 == 0) ? lightSquare : darkSquare;
                const QRectF tile(x, y, std::min(checkerboardTileSize, input.itemSize.width() - x),
                    std::min(checkerboardTileSize, input.itemSize.height() - y));
                root->appendChildNode(new QSGSimpleRectNode(tile, color));
            }
        }
    }

    if (payload.image.isNull()) {
        return { root, CommitResult::Empty, payloadIdentity };
    }

    if (!input.window) {
        delete root;
        return { nullptr, CommitResult::Failed, payloadIdentity };
    }

    QQuickWindow::CreateTextureOptions textureOptions;
    if (input.mipmap) {
        textureOptions |= QQuickWindow::TextureHasMipmaps;
    }
    QSGTexture* texture = input.window->createTextureFromImage(payload.image, textureOptions);
    QSGImageNode* imageNode = input.window->createImageNode();
    if (!texture || !imageNode) {
        delete texture;
        delete imageNode;
        delete root;
        return { nullptr, CommitResult::Failed, payloadIdentity };
    }

    imageNode->setTexture(texture);
    imageNode->setOwnsTexture(true);
    imageNode->setRect(input.targetRect);
    const qreal devicePixelRatio = payload.image.devicePixelRatio();
    const QRectF physicalSourceRect(input.sourceRect.x() * devicePixelRatio,
        input.sourceRect.y() * devicePixelRatio, input.sourceRect.width() * devicePixelRatio,
        input.sourceRect.height() * devicePixelRatio);
    imageNode->setSourceRect(physicalSourceRect);
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
    return { root, CommitResult::Committed, payloadIdentity };
}
