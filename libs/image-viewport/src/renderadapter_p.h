/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"
#include "renderfailurecause_p.h"
#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QVector>
#include <QtGui/QColor>

#include <optional>

class RenderAdapter
{
public:
    enum class CommitResult {
        Empty,
        Committed,
        Failed,
    };

    struct RolePayload
    {
        ImageViewportPageRole role = ImageViewportPageRole::Primary;
        ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    };

    struct Input
    {
        struct ImageLayer
        {
            ImageViewportPageRole role = ImageViewportPageRole::Primary;
            ImageViewportInternal::PreparedPayload preparedPayload;
            QRectF targetRect;
            QRectF sourceRect;
            int rotationDegrees = 0;
            bool mirrorHorizontally = false;
            bool mirrorVertically = false;
        };

        QSizeF itemSize;
        ImageViewportBackgroundMode backgroundMode = ImageViewportBackgroundMode::Transparent;
        QColor backgroundColor = Qt::white;
        QColor checkerboardLightColor = Qt::white;
        QColor checkerboardDarkColor = QColor(220, 220, 220);
        double checkerboardCellSize = 8.0;
        bool smoothing = true;
        bool mipmap = false;
        ImageViewportRoleSet requiredRoleSet;
        QVector<ImageLayer> imageLayers;
    };

    struct RenderPlan
    {
        struct BackgroundLayer
        {
            ImageViewportBackgroundMode mode = ImageViewportBackgroundMode::Transparent;
            QRectF bounds;
            QColor solidColor;
            QColor checkerboardLightColor;
            QColor checkerboardDarkColor;
            double checkerboardCellSize = 0.0;
        };

        struct ImageLayer
        {
            ImageViewportPageRole role = ImageViewportPageRole::Primary;
            ImageViewportInternal::PreparedPayload preparedPayload;
            ImageViewportInternal::PreparedPayloadIdentity preparedPayloadIdentity;
            QRectF targetRect;
            QRectF unrotatedTargetRect;
            QRectF sourceRect;
            QRectF physicalSourceRect;
            int rotationDegrees = 0;
            bool mirrorTextureHorizontally = false;
            bool mirrorTextureVertically = false;
        };

        CommitResult result = CommitResult::Empty;
        QVector<RolePayload> rolePayloads;
        ImageViewportPageRole failedRole = ImageViewportPageRole::Primary;
        RenderFailureCause failureCause = RenderFailureCause::None;
        std::optional<BackgroundLayer> backgroundLayer;
        QVector<ImageLayer> imageLayers;
        bool smoothing = true;
        bool mipmap = false;
    };

    [[nodiscard]] RenderPlan createPlan(const Input& input) const;
};
