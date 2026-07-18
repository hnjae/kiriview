// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagerectmapping.h"

#include "bridge/qtgeometryconversion.h"
#include "kiriview/src/policy/imagerendergeometry.cxx.h"

namespace {
kiriview::RustImageRenderSize rustRenderSize(QSize size)
{
    return kiriview::Bridge::rustSize<kiriview::RustImageRenderSize>(size);
}

kiriview::RustImageRenderSizeF rustRenderSizeF(QSizeF size)
{
    return kiriview::Bridge::rustSizeF<kiriview::RustImageRenderSizeF>(size);
}

kiriview::RustImageRenderRect rustRenderRect(QRect rect)
{
    return kiriview::Bridge::rustRect<kiriview::RustImageRenderRect>(rect);
}

kiriview::RustImageRenderRectF rustRenderRectF(const QRectF& rect)
{
    return kiriview::Bridge::rustRectF<kiriview::RustImageRenderRectF>(rect);
}
}

namespace kiriview {
QRect boundedIntegerRect(QRect rect, QSize boundsSize)
{
    return Bridge::qtRect(rustBoundedIntegerRect(rustRenderRect(rect), rustRenderSize(boundsSize)));
}

QRect scaledIntegerRect(const QRectF& rect, QSizeF sourceSize, QSize targetSize)
{
    return Bridge::qtRect(rustScaledIntegerRect(
        rustRenderRectF(rect), rustRenderSizeF(sourceSize), rustRenderSize(targetSize)));
}
}
