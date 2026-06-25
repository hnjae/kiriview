#pragma once

#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>

#include <QUrl>

class ImageViewport : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)

public:
    explicit ImageViewport(QQuickItem *parent = nullptr);

    QUrl source() const;
    void setSource(const QUrl &source);

signals:
    void sourceChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    QUrl m_source;
};
