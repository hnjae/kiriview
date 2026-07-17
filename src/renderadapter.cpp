#include "renderadapter_scenegraph_p.h"

#include <QtGui/QMatrix4x4>
#include <QtQuick/QSGSimpleRectNode>
#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QSGTransformNode>

#include <cmath>
#include <limits>

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

bool positiveFinite(QSizeF size)
{
    return std::isfinite(size.width()) && std::isfinite(size.height()) && size.width() > 0.0
        && size.height() > 0.0;
}

bool finiteRect(const QRectF& rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y()) && std::isfinite(rect.width())
        && std::isfinite(rect.height()) && std::isfinite(rect.right())
        && std::isfinite(rect.bottom());
}

bool sceneGraphRect(const QRectF& rect)
{
    constexpr double maximum = std::numeric_limits<float>::max();
    return finiteRect(rect) && qAbs(rect.x()) <= maximum && qAbs(rect.y()) <= maximum
        && qAbs(rect.width()) <= maximum && qAbs(rect.height()) <= maximum
        && qAbs(rect.right()) <= maximum && qAbs(rect.bottom()) <= maximum;
}

bool validBackground(const RenderAdapter::Input& input)
{
    switch (input.backgroundMode) {
    case ImageViewportBackgroundMode::Transparent:
        return true;
    case ImageViewportBackgroundMode::SolidColor:
        return input.backgroundColor.isValid();
    case ImageViewportBackgroundMode::Checkerboard: {
        if (!input.checkerboardLightColor.isValid() || !input.checkerboardDarkColor.isValid()
            || !std::isfinite(input.checkerboardCellSize) || input.checkerboardCellSize <= 0.0) {
            return false;
        }
        const QSizeF sourceSize(input.itemSize.width() / input.checkerboardCellSize,
            input.itemSize.height() / input.checkerboardCellSize);
        return positiveFinite(sourceSize);
    }
    }
    return false;
}

bool payloadFactsValid(const ImageViewportInternal::PreparedPayload& payload)
{
    if (payload.image.isNull() || !positiveFinite(payload.sourceLogicalSize)
        || !positiveFinite(payload.payloadRasterSize)
        || !positiveFinite(payload.sourceToPayloadScale)
        || payload.payloadRasterSize != QSizeF(payload.image.size())) {
        return false;
    }
    const QSizeF mapped(payload.sourceLogicalSize.width() * payload.sourceToPayloadScale.width(),
        payload.sourceLogicalSize.height() * payload.sourceToPayloadScale.height());
    return positiveFinite(mapped)
        && qAbs(mapped.width() - payload.payloadRasterSize.width()) < 0.0001
        && qAbs(mapped.height() - payload.payloadRasterSize.height()) < 0.0001;
}

QImage checkerboardImage(const RenderAdapter::RenderPlan::BackgroundLayer& background)
{
    QImage image(2, 2, QImage::Format_ARGB32_Premultiplied);
    image.setPixelColor(0, 0, background.checkerboardLightColor);
    image.setPixelColor(1, 0, background.checkerboardDarkColor);
    image.setPixelColor(0, 1, background.checkerboardDarkColor);
    image.setPixelColor(1, 1, background.checkerboardLightColor);
    return image;
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
    if ((std::isfinite(input.itemSize.width()) && input.itemSize.width() <= 0.0)
        || (std::isfinite(input.itemSize.height()) && input.itemSize.height() <= 0.0)) {
        plan.result = RenderAdapter::CommitResult::Empty;
        return plan;
    }

    const QVector<Input::ImageLayer>& imageLayers = input.imageLayers;
    const bool requiredRolesValid
        = input.requiredRoleSet.primary() || !input.requiredRoleSet.secondary();
    const qsizetype requiredRoleCount = input.requiredRoleSet.secondary() ? 2
        : input.requiredRoleSet.primary()                                 ? 1
                                                                          : 0;

    if (!requiredRolesValid || imageLayers.size() != requiredRoleCount) {
        plan.result = CommitResult::Failed;
        plan.failedRole = input.requiredRoleSet.secondary() && imageLayers.size() < 2
            ? ImageViewportPageRole::Secondary
            : ImageViewportPageRole::Primary;
        plan.failureCause = RenderFailureCause::InvalidRolePayload;
        return plan;
    }

    for (const Input::ImageLayer& layer : imageLayers) {
        plan.rolePayloads.append({ layer.role, layer.preparedPayload.identity() });
    }

    const QRectF itemBounds(QPointF(), input.itemSize);
    if (!positiveFinite(input.itemSize) || !sceneGraphRect(itemBounds) || !validBackground(input)) {
        plan.result = CommitResult::Failed;
        plan.failedRole = imageLayers.isEmpty() ? ImageViewportPageRole::Primary
                                                : imageLayers.constFirst().role;
        plan.failureCause = RenderFailureCause::InvalidRenderGeometry;
        return plan;
    }

    if (input.backgroundMode != ImageViewportBackgroundMode::Transparent) {
        plan.backgroundLayer = RenderPlan::BackgroundLayer { input.backgroundMode, itemBounds,
            input.backgroundColor, input.checkerboardLightColor, input.checkerboardDarkColor,
            input.checkerboardCellSize };
    }

    for (qsizetype index = 0; index < imageLayers.size(); ++index) {
        const Input::ImageLayer& layer = imageLayers.at(index);
        const auto& payload = layer.preparedPayload;
        const ImageViewportInternal::PreparedPayloadIdentity payloadIdentity {
            payload.generation,
            payload.payloadId,
        };
        const ImageViewportPageRole expectedRole
            = index == 0 ? ImageViewportPageRole::Primary : ImageViewportPageRole::Secondary;
        const bool targetEmpty = layer.targetRect.isEmpty();
        const bool sourceEmpty = layer.sourceRect.isEmpty();
        if (index > 1 || layer.role != expectedRole || !payloadIdentity.isValid()
            || !payloadFactsValid(payload) || !finiteRect(layer.sourceRect)
            || targetEmpty != sourceEmpty) {
            plan.result = CommitResult::Failed;
            plan.failedRole = layer.role;
            plan.failureCause = RenderFailureCause::InvalidRolePayload;
            return plan;
        }
        if (!sceneGraphRect(layer.targetRect)) {
            plan.result = CommitResult::Failed;
            plan.failedRole = layer.role;
            plan.failureCause = RenderFailureCause::InvalidRenderGeometry;
            return plan;
        }
        if (targetEmpty)
            continue;
        const QSizeF scale = payload.sourceToPayloadScale;
        const QRectF physicalSourceRect(layer.sourceRect.x() * scale.width(),
            layer.sourceRect.y() * scale.height(), layer.sourceRect.width() * scale.width(),
            layer.sourceRect.height() * scale.height());
        const QRectF rasterBounds(QPointF(), payload.payloadRasterSize);
        const QRectF unrotated = unrotatedTargetRect(layer.targetRect, layer.rotationDegrees);
        if (!finiteRect(physicalSourceRect) || !sceneGraphRect(unrotated)
            || !rasterBounds.contains(physicalSourceRect)) {
            plan.result = CommitResult::Failed;
            plan.failedRole = layer.role;
            plan.failureCause = !finiteRect(physicalSourceRect) || !sceneGraphRect(unrotated)
                ? RenderFailureCause::InvalidRenderGeometry
                : RenderFailureCause::InvalidRolePayload;
            return plan;
        }
        plan.imageLayers.append({ layer.role, payload, payloadIdentity, layer.targetRect, unrotated,
            layer.sourceRect, physicalSourceRect, normalizedRotation(layer.rotationDegrees),
            layer.mirrorHorizontally, layer.mirrorVertically });
    }

    plan.result = imageLayers.isEmpty() ? CommitResult::Empty : CommitResult::Committed;
    return plan;
}

RenderAdapterSceneGraph::Output RenderAdapterSceneGraph::createNode(
    RenderAdapter adapter, QSGNode* oldNode, const Input& input)
{
    const RenderAdapter::RenderPlan plan = adapter.createPlan(input.planInput);
    if (plan.result == RenderAdapter::CommitResult::Failed) {
        return { oldNode, plan.result, plan.rolePayloads, plan.failedRole, plan.failureCause };
    }
    if (!plan.backgroundLayer && plan.imageLayers.isEmpty()) {
        delete oldNode;
        return { nullptr, plan.result, plan.rolePayloads, plan.failedRole, plan.failureCause };
    }

    auto* root = new QSGNode;
    if (plan.backgroundLayer) {
        const auto& background = *plan.backgroundLayer;
        if (background.mode == ImageViewportBackgroundMode::SolidColor) {
            root->appendChildNode(new QSGSimpleRectNode(background.bounds, background.solidColor));
        } else if (background.mode == ImageViewportBackgroundMode::Checkerboard) {
            if (!input.window) {
                delete root;
                if (plan.imageLayers.isEmpty()) {
                    delete oldNode;
                    return { nullptr, RenderAdapter::CommitResult::Empty, plan.rolePayloads,
                        ImageViewportPageRole::Primary, RenderFailureCause::None };
                }
                return { oldNode, RenderAdapter::CommitResult::Empty, plan.rolePayloads,
                    ImageViewportPageRole::Primary, RenderFailureCause::None };
            }
            const Factory defaultSceneGraphFactory;
            const Factory& sceneGraphFactory
                = input.sceneGraphFactory ? *input.sceneGraphFactory : defaultSceneGraphFactory;
            QSGTexture* texture
                = sceneGraphFactory.createTexture(input.window, checkerboardImage(background), {});
            if (!texture) {
                delete root;
                if (plan.imageLayers.isEmpty()) {
                    delete oldNode;
                    return { nullptr, RenderAdapter::CommitResult::Empty, plan.rolePayloads,
                        ImageViewportPageRole::Primary, RenderFailureCause::None };
                }
                return { oldNode, RenderAdapter::CommitResult::Failed, plan.rolePayloads,
                    ImageViewportPageRole::Primary, RenderFailureCause::TextureCreationFailure };
            }
            texture->setHorizontalWrapMode(QSGTexture::Repeat);
            texture->setVerticalWrapMode(QSGTexture::Repeat);
            texture->setFiltering(QSGTexture::Nearest);
            auto* checkerboard = new QSGSimpleTextureNode;
            checkerboard->setTexture(texture);
            checkerboard->setOwnsTexture(true);
            checkerboard->setFiltering(QSGTexture::Nearest);
            checkerboard->setRect(background.bounds);
            checkerboard->setSourceRect(0.0, 0.0,
                background.bounds.width() / background.checkerboardCellSize,
                background.bounds.height() / background.checkerboardCellSize);
            root->appendChildNode(checkerboard);
        }
    }

    if (plan.imageLayers.isEmpty()) {
        delete oldNode;
        return { root, plan.result, plan.rolePayloads, plan.failedRole, plan.failureCause };
    }

    if (!input.window) {
        delete root;
        return { oldNode, RenderAdapter::CommitResult::Empty, plan.rolePayloads,
            ImageViewportPageRole::Primary, RenderFailureCause::None };
    }

    const Factory defaultSceneGraphFactory;
    const Factory& sceneGraphFactory
        = input.sceneGraphFactory ? *input.sceneGraphFactory : defaultSceneGraphFactory;
    bool mipmapUnavailable = false;
    for (const RenderAdapter::RenderPlan::ImageLayer& layer : plan.imageLayers) {
        const auto& payload = layer.preparedPayload;
        const ImageViewportInternal::PreparedPayloadIdentity payloadIdentity
            = layer.preparedPayloadIdentity;
        QQuickWindow::CreateTextureOptions textureOptions;
        if (plan.mipmap) {
            textureOptions |= QQuickWindow::TextureHasMipmaps;
        }
        QSGTexture* texture
            = sceneGraphFactory.createTexture(input.window, payload.image, textureOptions);
        if (!texture) {
            delete root;
            return { oldNode, RenderAdapter::CommitResult::Failed, plan.rolePayloads, layer.role,
                RenderFailureCause::TextureCreationFailure };
        }
        mipmapUnavailable = mipmapUnavailable || (plan.mipmap && !texture->hasMipmaps());
        QSGImageNode* imageNode = sceneGraphFactory.createImageNode(input.window);
        if (!imageNode) {
            delete texture;
            delete root;
            return { oldNode, RenderAdapter::CommitResult::Failed, plan.rolePayloads, layer.role,
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
    return { root, plan.result, plan.rolePayloads, ImageViewportPageRole::Primary,
        RenderFailureCause::None, false, mipmapUnavailable };
}
