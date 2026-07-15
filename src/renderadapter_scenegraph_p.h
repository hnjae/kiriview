#pragma once

#include "renderadapter_p.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGNode>
#include <QtQuick/QSGTexture>

namespace RenderAdapterSceneGraph {

class Factory
{
public:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;
    virtual ~Factory() = default;
    virtual QSGTexture* createTexture(QQuickWindow* window, const QImage& image,
        QQuickWindow::CreateTextureOptions options) const;
    virtual QSGImageNode* createImageNode(QQuickWindow* window) const;
};

struct Input
{
    RenderAdapter::Input planInput;
    QQuickWindow* window = nullptr;
    const Factory* sceneGraphFactory = nullptr;
};

struct Output
{
    QSGNode* node = nullptr;
    RenderAdapter::CommitResult result = RenderAdapter::CommitResult::Empty;
    ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    QVector<RenderAdapter::RolePayload> rolePayloads;
    ImageViewportPageRole failedRole = ImageViewportPageRole::Primary;
    RenderFailureCause failureCause = RenderFailureCause::None;
    bool smoothingUnavailable = false;
    bool mipmapUnavailable = false;
};

Output createNode(RenderAdapter adapter, QSGNode* oldNode, const Input& input);

}
