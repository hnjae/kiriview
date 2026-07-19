// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiriimageviewportsurface.h"

#include "facade/kiriimagedocument.h"

#include <ImageViewport/imageviewport.h>

KiriImageViewportSurface::KiriImageViewportSurface(QQuickItem* parent)
    : QQuickItem(parent)
    , m_viewport(new ImageViewport(this))
{
    m_viewport->setSize(size());
}

KiriImageViewportSurface::~KiriImageViewportSurface()
{
    if (m_document != nullptr) {
        m_document->detachImageViewport(m_viewport);
    }
}

KiriImageDocument* KiriImageViewportSurface::document() const { return m_document; }

void KiriImageViewportSurface::setDocument(KiriImageDocument* document)
{
    if (m_document == document) {
        return;
    }
    if (m_document != nullptr) {
        m_document->detachImageViewport(m_viewport);
    }
    QObject::disconnect(m_documentDestroyedConnection);
    m_document = document;
    if (m_document != nullptr) {
        m_documentDestroyedConnection = connect(m_document, &QObject::destroyed, this, [this]() {
            m_document = nullptr;
            Q_EMIT documentChanged();
        });
        m_document->attachImageViewport(m_viewport);
    }
    Q_EMIT documentChanged();
}

ImageViewport* KiriImageViewportSurface::viewport() const { return m_viewport; }

void KiriImageViewportSurface::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    m_viewport->setPosition(QPointF());
    m_viewport->setSize(newGeometry.size());
}
