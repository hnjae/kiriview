// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRIIMAGEVIEWPORTSURFACE_H
#define KIRIVIEW_KIRIIMAGEVIEWPORTSURFACE_H

#include "facade/kiriimagedocument.h"

#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class ImageViewport;

class KiriImageViewportSurface : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KiriImageViewportSurface)
    Q_PROPERTY(KiriImageDocument* document READ document WRITE setDocument NOTIFY documentChanged)

public:
    explicit KiriImageViewportSurface(QQuickItem* parent = nullptr);
    ~KiriImageViewportSurface() override;
    Q_DISABLE_COPY_MOVE(KiriImageViewportSurface)

    KiriImageDocument* document() const;
    void setDocument(KiriImageDocument* document);
    ImageViewport* viewport() const;

Q_SIGNALS:
    void documentChanged();

private:
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

    QPointer<KiriImageDocument> m_document;
    QMetaObject::Connection m_documentDestroyedConnection;
    ImageViewport* m_viewport = nullptr;
};

#endif
