#include "renderadapter_p.h"

#include <QtQuick/QSGSimpleRectNode>

#include <algorithm>
#include <cmath>

RenderAdapter::Output RenderAdapter::createNode(QSGNode* oldNode, const Input& input) const
{
    delete oldNode;

    const ImageViewportInternal::PreparedPayloadIdentity emptyPayload;
    if (input.itemSize.width() <= 0.0 || input.itemSize.height() <= 0.0) {
        return { nullptr, RenderAdapter::CommitResult::Empty, emptyPayload };
    }

    QVector<Input::ImageLayer> imageLayers = input.imageLayers;
    if (imageLayers.isEmpty() && !input.preparedPayload.image.isNull()) {
        imageLayers.append({ input.preparedPayload, input.targetRect, input.sourceRect,
            input.mirrorHorizontally, input.mirrorVertically });
    }
    const auto firstPayloadIdentity = [&]() {
        if (imageLayers.isEmpty()) {
            return emptyPayload;
        }
        const auto& payload = imageLayers.constFirst().preparedPayload;
        return ImageViewportInternal::PreparedPayloadIdentity {
            payload.generation,
            payload.requestId,
            payload.payloadId,
        };
    };

    const bool hasBackground = input.backgroundMode != ImageViewport::BackgroundMode::Transparent;
    if (!hasBackground && imageLayers.isEmpty()) {
        return { nullptr, RenderAdapter::CommitResult::Empty, emptyPayload };
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

    if (imageLayers.isEmpty()) {
        return { root, CommitResult::Empty, emptyPayload };
    }

    if (!input.window) {
        delete root;
        return { nullptr, CommitResult::Failed, firstPayloadIdentity() };
    }

    for (const Input::ImageLayer& layer : imageLayers) {
        const auto& payload = layer.preparedPayload;
        const ImageViewportInternal::PreparedPayloadIdentity payloadIdentity {
            payload.generation,
            payload.requestId,
            payload.payloadId,
        };
        if (payload.image.isNull()) {
            continue;
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
        imageNode->setRect(layer.targetRect);
        const qreal devicePixelRatio = payload.image.devicePixelRatio();
        const QRectF physicalSourceRect(layer.sourceRect.x() * devicePixelRatio,
            layer.sourceRect.y() * devicePixelRatio, layer.sourceRect.width() * devicePixelRatio,
            layer.sourceRect.height() * devicePixelRatio);
        imageNode->setSourceRect(physicalSourceRect);
        imageNode->setFiltering(input.smoothing ? QSGTexture::Linear : QSGTexture::Nearest);
        imageNode->setMipmapFiltering(input.mipmap ? QSGTexture::Linear : QSGTexture::None);
        QSGImageNode::TextureCoordinatesTransformMode transform = {};
        if (layer.mirrorHorizontally) {
            transform |= QSGImageNode::MirrorHorizontally;
        }
        if (layer.mirrorVertically) {
            transform |= QSGImageNode::MirrorVertically;
        }
        imageNode->setTextureCoordinatesTransform(transform);
        root->appendChildNode(imageNode);
    }
    return { root, CommitResult::Committed, firstPayloadIdentity() };
}
