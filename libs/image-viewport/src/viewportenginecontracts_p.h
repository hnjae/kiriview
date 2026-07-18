/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportengineinput_p.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>

struct ViewportEngineViewportState
{
    QRectF itemBounds;
    double devicePixelRatio = 1.0;
    bool renderAvailable = false;

    friend bool operator==(
        const ViewportEngineViewportState& lhs, const ViewportEngineViewportState& rhs)
    {
        return lhs.itemBounds.x() == rhs.itemBounds.x() && lhs.itemBounds.y() == rhs.itemBounds.y()
            && lhs.itemBounds.width() == rhs.itemBounds.width()
            && lhs.itemBounds.height() == rhs.itemBounds.height()
            && lhs.devicePixelRatio == rhs.devicePixelRatio
            && lhs.renderAvailable == rhs.renderAvailable;
    }
    friend bool operator!=(
        const ViewportEngineViewportState& lhs, const ViewportEngineViewportState& rhs)
    {
        return !(lhs == rhs);
    }
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
        ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition
            contentPositionTransition
            = ViewportEnginePresentationTargetTransitionPolicy::ContentPositionTransition::Clamp;
        QPointF previousContentPosition;

        bool isValid() const { return generation != 0; }
    };

    ViewportEnginePresentationTarget presentationTarget = ViewportEnginePresentationTarget::clear();
    ImageViewportRoleSet acceptedRoleSet;
    ImageViewportRoleSet targetRoleSet;
    quint64 generation = 0;
    quint64 primaryRoleGeneration = 0;
    quint64 secondaryRoleGeneration = 0;
    ImageViewportPageRole activeRole = ImageViewportPageRole::Primary;
    bool activeRoleValid = false;
    PendingPresentationTransition pendingPresentationTransition;
};
