#pragma once

#include "renderadapter_p.h"
#include "viewportrendercontract_p.h"

#include <QtCore/QRectF>

class QQuickWindow;
class QSGNode;

struct ImageViewportRenderHostResult
{
    QSGNode* node = nullptr;
    RenderAdapter::CommitResult result = RenderAdapter::CommitResult::Empty;
    ViewportRenderAcknowledgement acknowledgement;
    ViewportRenderQualityFallbackFact qualityFallback;
    bool imagePresent = false;
};

class ImageViewportRenderHost
{
public:
    ImageViewportRenderHost() = default;

    ImageViewportRenderHostResult synchronize(QSGNode* oldNode, QQuickWindow* window,
        const ViewportRenderSynchronization& synchronization);

private:
    RenderAdapter renderAdapter;
};
