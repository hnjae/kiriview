#pragma once

#include <ImageViewport/ImageViewport>

#include <QtCore/QRectF>
#include <QtCore/QSizeF>

struct ViewportEngineViewportInput
{
    QRectF itemBounds;
    double devicePixelRatio = 1.0;
    bool renderAvailable = true;
};

struct ViewportEngineGeometryInput
{
    bool primaryPresent = false;
    QRectF itemBounds;
    QSizeF primarySize;
    QSizeF secondarySize;
    double devicePixelRatio = 1.0;
    bool renderAvailable = true;
};

struct ViewportEngineSnapshotInput
{
    ViewportEngineGeometryInput acceptedGeometry;
    ViewportEngineGeometryInput displayedGeometry;
};

struct ViewportEnginePresentationTargetState
{
    struct PendingPresentationTransition
    {
        quint64 generation = 0;
        PresentationTargetTransitionPolicy::ContentPositionTransition contentPositionTransition
            = PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp;
        QPointF previousContentPosition;

        bool isValid() const { return generation != 0; }
    };

    ImageViewportPresentationTarget presentationTarget = ImageViewportPresentationTarget::clear();
    ImageViewportRoleSet acceptedRoleSet;
    ImageViewportRoleSet targetRoleSet;
    quint64 generation = 0;
    quint64 primaryRoleGeneration = 0;
    quint64 secondaryRoleGeneration = 0;
    ImageViewportPageRole activeRole = ImageViewportPageRole::Primary;
    bool activeRoleValid = false;
    PendingPresentationTransition pendingPresentationTransition;
};
