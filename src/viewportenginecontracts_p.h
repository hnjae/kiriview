#pragma once

#include "imageviewport.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>

struct ViewportEngineViewportInput
{
    QRectF itemBounds;
    double devicePixelRatio = 1.0;
};

struct ViewportEngineGeometryInput
{
    bool primaryPresent = false;
    QRectF itemBounds;
    QSizeF primarySize;
    QSizeF secondarySize;
    double devicePixelRatio = 1.0;
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
    ImageViewportPageRole activeRole = ImageViewportPageRole::Primary;
    bool activeRoleValid = false;
};
