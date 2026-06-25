#include "imageviewport.h"

#include <QtQuick/QSGNode>

ImageViewport::ImageViewport(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

QSGNode *ImageViewport::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;
    return nullptr;
}
