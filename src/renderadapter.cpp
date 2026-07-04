#include "renderadapter_p.h"

#include <QtGui/QMatrix4x4>
#include <QtQuick/QSGSimpleRectNode>
#include <QtQuick/QSGTransformNode>

#include <algorithm>
#include <cmath>

namespace {

int normalizedRotation(int degrees)
{
    int normalized = degrees % 360;
    if (normalized < 0) {
        normalized += 360;
    }
    return normalized;
}

QRectF unrotatedTargetRect(const QRectF& targetRect, int rotationDegrees)
{
    const int rotation = normalizedRotation(rotationDegrees);
    if (rotation != 90 && rotation != 270) {
        return targetRect;
    }
    const QSizeF unrotatedSize(targetRect.height(), targetRect.width());
    return QRectF(targetRect.center().x() - unrotatedSize.width() / 2.0,
        targetRect.center().y() - unrotatedSize.height() / 2.0,
        unrotatedSize.width(),
        unrotatedSize.height());
}

QMatrix4x4 rotationTransform(const QRectF& targetRect, int rotationDegrees)
{
    QMatrix4x4 matrix;
    matrix.translate(targetRect.center().x(), targetRect.center().y());
    matrix.rotate(normalizedRotation(rotationDegrees), 0.0f, 0.0f, 1.0f);
    matrix.translate(-targetRect.center().x(), -targetRect.center().y());
    return matrix;
}

}

QSGTexture* RenderAdapter::SceneGraphFactory::createTexture(QQuickWindow* window,
    const QImage& image, QQuickWindow::CreateTextureOptions options) const
{
    return window ? window->createTextureFromImage(image, options) : nullptr;
}

QSGImageNode* RenderAdapter::SceneGraphFactory::createImageNode(QQuickWindow* window) const
{
    return window ? window->createImageNode() : nullptr;
}

RenderAdapter::Output RenderAdapter::createNode(QSGNode* oldNode, const Input& input) const
{
    delete oldNode;

    const ImageViewportInternal::PreparedPayloadIdentity emptyPayload;
    if (input.itemSize.width() <= 0.0 || input.itemSize.height() <= 0.0) {
        return { nullptr, RenderAdapter::CommitResult::Empty, emptyPayload };
    }

    QVector<Input::ImageLayer> imageLayers = input.imageLayers;
    if (imageLayers.isEmpty() && !input.preparedPayload.image.isNull()) {
        imageLayers.append({ ImageViewport::PageRole::Primary, input.preparedPayload,
            input.targetRect, input.sourceRect,
            input.rotationDegrees,
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
        return { nullptr, CommitResult::Failed, firstPayloadIdentity(), {},
            ImageViewport::PageRole::Primary, RenderFailureCause::MissingWindow };
    }

    const SceneGraphFactory defaultSceneGraphFactory;
    const SceneGraphFactory& sceneGraphFactory = input.sceneGraphFactory
        ? *input.sceneGraphFactory
        : defaultSceneGraphFactory;
    QVector<Output::RolePayload> rolePayloads;
    for (const Input::ImageLayer& layer : imageLayers) {
        const auto& payload = layer.preparedPayload;
        const ImageViewportInternal::PreparedPayloadIdentity payloadIdentity {
            payload.generation,
            payload.requestId,
            payload.payloadId,
        };
        if (payload.image.isNull()) {
            delete root;
            return { nullptr, CommitResult::Failed, payloadIdentity, rolePayloads, layer.role,
                RenderFailureCause::InvalidRolePayload };
        }
        rolePayloads.append({ layer.role, payloadIdentity });

        QQuickWindow::CreateTextureOptions textureOptions;
        if (input.mipmap) {
            textureOptions |= QQuickWindow::TextureHasMipmaps;
        }
        QSGTexture* texture
            = sceneGraphFactory.createTexture(input.window, payload.image, textureOptions);
        if (!texture) {
            delete root;
            return { nullptr, CommitResult::Failed, payloadIdentity, rolePayloads, layer.role,
                RenderFailureCause::TextureCreationFailure };
        }
        QSGImageNode* imageNode = sceneGraphFactory.createImageNode(input.window);
        if (!imageNode) {
            delete texture;
            delete root;
            return { nullptr, CommitResult::Failed, payloadIdentity, rolePayloads, layer.role,
                RenderFailureCause::ImageNodeCreationFailure };
        }

        imageNode->setTexture(texture);
        imageNode->setOwnsTexture(true);
        imageNode->setRect(unrotatedTargetRect(layer.targetRect, layer.rotationDegrees));
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
        const int rotation = normalizedRotation(layer.rotationDegrees);
        if (rotation == 0) {
            root->appendChildNode(imageNode);
        } else {
            auto* transformNode = new QSGTransformNode;
            transformNode->setMatrix(rotationTransform(layer.targetRect, rotation));
            transformNode->appendChildNode(imageNode);
            root->appendChildNode(transformNode);
        }
    }
    return { root, CommitResult::Committed, firstPayloadIdentity(), rolePayloads };
}
