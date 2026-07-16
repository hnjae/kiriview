#include "renderadapter_scenegraph_p.h"

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
        targetRect.center().y() - unrotatedSize.height() / 2.0, unrotatedSize.width(),
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

QSGTexture* RenderAdapterSceneGraph::Factory::createTexture(
    QQuickWindow* window, const QImage& image, QQuickWindow::CreateTextureOptions options) const
{
    return window ? window->createTextureFromImage(image, options) : nullptr;
}

QSGImageNode* RenderAdapterSceneGraph::Factory::createImageNode(QQuickWindow* window) const
{
    return window ? window->createImageNode() : nullptr;
}

RenderAdapter::RenderPlan RenderAdapter::createPlan(const Input& input) const
{
    RenderPlan plan;
    plan.smoothing = input.smoothing;
    plan.mipmap = input.mipmap;
    if (input.itemSize.width() <= 0.0 || input.itemSize.height() <= 0.0) {
        plan.result = RenderAdapter::CommitResult::Empty;
        return plan;
    }

    const QVector<Input::ImageLayer>& imageLayers = input.imageLayers;

    if (input.backgroundMode == ImageViewportBackgroundMode::SolidColor) {
        plan.backgroundRects.append(
            { QRectF(0.0, 0.0, input.itemSize.width(), input.itemSize.height()),
                input.backgroundColor });
    } else if (input.backgroundMode == ImageViewportBackgroundMode::Checkerboard) {
        const int rowCount
            = static_cast<int>(std::ceil(input.itemSize.height() / input.checkerboardCellSize));
        const int columnCount
            = static_cast<int>(std::ceil(input.itemSize.width() / input.checkerboardCellSize));
        for (int row = 0; row < rowCount; ++row) {
            const double y = row * input.checkerboardCellSize;
            for (int column = 0; column < columnCount; ++column) {
                const double x = column * input.checkerboardCellSize;
                const QColor color = ((row + column) % 2 == 0) ? input.checkerboardLightColor
                                                               : input.checkerboardDarkColor;
                const QRectF tile(x, y,
                    std::min(input.checkerboardCellSize, input.itemSize.width() - x),
                    std::min(input.checkerboardCellSize, input.itemSize.height() - y));
                plan.backgroundRects.append({ tile, color });
            }
        }
    }

    if (plan.backgroundRects.isEmpty() && imageLayers.isEmpty()) {
        plan.result = RenderAdapter::CommitResult::Empty;
        return plan;
    }

    if (imageLayers.isEmpty()) {
        plan.result = CommitResult::Empty;
        return plan;
    }

    for (qsizetype index = 0; index < imageLayers.size(); ++index) {
        const Input::ImageLayer& layer = imageLayers.at(index);
        const auto& payload = layer.preparedPayload;
        const ImageViewportInternal::PreparedPayloadIdentity payloadIdentity {
            payload.generation,
            payload.requestId,
            payload.payloadId,
        };
        plan.rolePayloads.append({ layer.role, payloadIdentity });
        const ImageViewportPageRole expectedRole
            = index == 0 ? ImageViewportPageRole::Primary : ImageViewportPageRole::Secondary;
        if (index > 1 || layer.role != expectedRole || !payloadIdentity.isValid()
            || payload.image.isNull() || layer.targetRect.isEmpty() || layer.sourceRect.isEmpty()) {
            plan.result = CommitResult::Failed;
            plan.failedRole = layer.role;
            plan.failureCause = RenderFailureCause::InvalidRolePayload;
            return plan;
        }
        const qreal devicePixelRatio = payload.image.devicePixelRatio();
        const QRectF physicalSourceRect(layer.sourceRect.x() * devicePixelRatio,
            layer.sourceRect.y() * devicePixelRatio, layer.sourceRect.width() * devicePixelRatio,
            layer.sourceRect.height() * devicePixelRatio);
        plan.imageLayers.append({ layer.role, payload, payloadIdentity, layer.targetRect,
            unrotatedTargetRect(layer.targetRect, layer.rotationDegrees), layer.sourceRect,
            physicalSourceRect, normalizedRotation(layer.rotationDegrees), layer.mirrorHorizontally,
            layer.mirrorVertically });
    }

    plan.result = CommitResult::Committed;
    return plan;
}

RenderAdapterSceneGraph::Output RenderAdapterSceneGraph::createNode(
    RenderAdapter adapter, QSGNode* oldNode, const Input& input)
{
    const RenderAdapter::RenderPlan plan = adapter.createPlan(input.planInput);
    if (plan.result == RenderAdapter::CommitResult::Failed) {
        return { oldNode, plan.result, plan.rolePayloads, plan.failedRole, plan.failureCause };
    }
    if (plan.backgroundRects.isEmpty() && plan.imageLayers.isEmpty()) {
        delete oldNode;
        return { nullptr, plan.result, plan.rolePayloads, plan.failedRole, plan.failureCause };
    }

    auto* root = new QSGNode;
    for (const RenderAdapter::RenderPlan::BackgroundRect& background : plan.backgroundRects) {
        root->appendChildNode(new QSGSimpleRectNode(background.rect, background.color));
    }

    if (plan.imageLayers.isEmpty()) {
        delete oldNode;
        return { root, plan.result, plan.rolePayloads, plan.failedRole, plan.failureCause };
    }

    if (!input.window) {
        delete root;
        return { oldNode, RenderAdapter::CommitResult::Failed, plan.rolePayloads,
            ImageViewportPageRole::Primary, RenderFailureCause::MissingWindow };
    }

    const Factory defaultSceneGraphFactory;
    const Factory& sceneGraphFactory
        = input.sceneGraphFactory ? *input.sceneGraphFactory : defaultSceneGraphFactory;
    QVector<RenderAdapter::RolePayload> rolePayloads;
    rolePayloads.reserve(plan.imageLayers.size());
    bool mipmapUnavailable = false;
    for (const RenderAdapter::RenderPlan::ImageLayer& layer : plan.imageLayers) {
        const auto& payload = layer.preparedPayload;
        const ImageViewportInternal::PreparedPayloadIdentity payloadIdentity
            = layer.preparedPayloadIdentity;
        rolePayloads.append({ layer.role, payloadIdentity });

        QQuickWindow::CreateTextureOptions textureOptions;
        if (plan.mipmap) {
            textureOptions |= QQuickWindow::TextureHasMipmaps;
        }
        QSGTexture* texture
            = sceneGraphFactory.createTexture(input.window, payload.image, textureOptions);
        if (!texture) {
            delete root;
            return { oldNode, RenderAdapter::CommitResult::Failed, rolePayloads, layer.role,
                RenderFailureCause::TextureCreationFailure };
        }
        mipmapUnavailable = mipmapUnavailable || (plan.mipmap && !texture->hasMipmaps());
        QSGImageNode* imageNode = sceneGraphFactory.createImageNode(input.window);
        if (!imageNode) {
            delete texture;
            delete root;
            return { oldNode, RenderAdapter::CommitResult::Failed, rolePayloads, layer.role,
                RenderFailureCause::ImageNodeCreationFailure };
        }

        imageNode->setTexture(texture);
        imageNode->setOwnsTexture(true);
        imageNode->setRect(layer.unrotatedTargetRect);
        imageNode->setSourceRect(layer.physicalSourceRect);
        imageNode->setFiltering(plan.smoothing ? QSGTexture::Linear : QSGTexture::Nearest);
        imageNode->setMipmapFiltering(
            plan.mipmap && texture->hasMipmaps() ? QSGTexture::Linear : QSGTexture::None);
        QSGImageNode::TextureCoordinatesTransformMode transform = {};
        if (layer.mirrorHorizontally) {
            transform |= QSGImageNode::MirrorHorizontally;
        }
        if (layer.mirrorVertically) {
            transform |= QSGImageNode::MirrorVertically;
        }
        imageNode->setTextureCoordinatesTransform(transform);
        if (layer.rotationDegrees == 0) {
            root->appendChildNode(imageNode);
        } else {
            auto* transformNode = new QSGTransformNode;
            transformNode->setMatrix(rotationTransform(layer.targetRect, layer.rotationDegrees));
            transformNode->appendChildNode(imageNode);
            root->appendChildNode(transformNode);
        }
    }
    delete oldNode;
    return { root, plan.result, rolePayloads, ImageViewportPageRole::Primary,
        RenderFailureCause::None, false, mipmapUnavailable };
}
