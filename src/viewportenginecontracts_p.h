#pragma once

#include "imageviewport.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>

struct ViewportEngineGeometryInput
{
    bool primaryPresent = false;
    QRectF itemBounds;
    QSizeF primarySize;
    QSizeF secondarySize;
    double devicePixelRatio = 1.0;
};

enum class ViewportEngineGeometryProjectionTarget {
    CurrentDisplay,
    PendingRender,
};

struct ViewportEngineSnapshotInput
{
    ViewportEngineGeometryInput acceptedGeometry;
    ViewportEngineGeometryInput displayedGeometry;
};

struct ViewportEnginePresentationTargetState
{
    ImageViewportPresentationTarget presentationTarget
        = ImageViewportPresentationTarget::clear();
    ImageViewportRoleSet acceptedRoleSet;
    ImageViewportRoleSet targetRoleSet;
    quint64 generation = 0;
    quint64 primaryRoleGeneration = 0;
    quint64 secondaryRoleGeneration = 0;
    ImageViewport::PageRole activeRole = ImageViewport::PageRole::Primary;
    bool activeRoleValid = false;
};
