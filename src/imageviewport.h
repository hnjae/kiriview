#pragma once

#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ImageViewport(QQuickItem *parent = nullptr);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
};
