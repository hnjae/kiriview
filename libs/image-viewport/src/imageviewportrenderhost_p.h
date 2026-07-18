/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "renderadapter_p.h"
#include "viewportrendercontract_p.h"

#include <QtCore/QRectF>

class QQuickWindow;
class QSGNode;

struct ImageViewportRenderHostResult
{
    QSGNode* node = nullptr;
    ViewportRenderHostFact fact;
};

class ImageViewportRenderHost
{
public:
    ImageViewportRenderHost() = default;

    ImageViewportRenderHostResult synchronize(
        QSGNode* oldNode, QQuickWindow* window, const ViewportRenderAttempt& attempt);

private:
    RenderAdapter renderAdapter;
};
