// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRIIMAGEVIEWPORTSURFACE_H
#define KIRIVIEW_KIRIIMAGEVIEWPORTSURFACE_H

#include <ImageViewport/imageviewport.h>

#include <QtQml/qqmlregistration.h>

class KiriImageViewportSurface : public ImageViewport
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KiriImageViewportSurface)

public:
    explicit KiriImageViewportSurface(QQuickItem* parent = nullptr);
};

#endif
