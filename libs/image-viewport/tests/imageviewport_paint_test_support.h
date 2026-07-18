/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewport_test_support.h"

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGSimpleRectNode>

namespace {

class PaintProbeViewport final : public ImageViewport
{
public:
    using ImageViewport::ImageViewport;

    QSGNode* takePaintNode(QSGNode* oldNode = nullptr) { return updatePaintNode(oldNode, nullptr); }
};

bool commitPaintNode(PaintProbeViewport& item)
{
    QScopedPointer<QSGNode> root(item.takePaintNode());
    return !root.isNull();
}

}
