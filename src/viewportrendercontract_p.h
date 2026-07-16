#pragma once

#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "renderfailurecause_p.h"
#include <ImageViewport/ImageViewport>

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QVector>
#include <QtGui/QColor>

struct ViewportRenderRolePayload
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
};

struct ViewportRenderAcknowledgement
{
    QVector<ViewportRenderRolePayload> rolePayloads;
    ImageViewportPageRole failedRole = ImageViewportPageRole::Primary;
    RenderFailureCause failureCause = RenderFailureCause::None;
    quint64 attempt = 0;
};

struct ViewportRenderQualityFallbackFact
{
    bool smoothingUnavailable = false;
    bool mipmapUnavailable = false;
};

struct ViewportRenderLayer
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportInternal::PreparedPayload preparedPayload;
    QRectF targetRect;
    QRectF sourceRect;
    int rotationDegrees = 0;
    bool mirrorHorizontally = false;
    bool mirrorVertically = false;
};

struct ViewportRenderSnapshot
{
    QSizeF itemSize;
    ImageViewportBackgroundMode backgroundMode = ImageViewportBackgroundMode::Transparent;
    QColor backgroundColor = Qt::white;
    QColor checkerboardLightColor = Qt::white;
    QColor checkerboardDarkColor = QColor(220, 220, 220);
    double checkerboardCellSize = 8.0;
    bool smoothing = true;
    bool mipmap = false;
    QVector<ViewportRenderLayer> imageLayers;
};

struct ViewportRenderSnapshotInput
{
    QSizeF itemSize;
    bool pendingTargetCommit = false;
    ImageViewportInternal::PreparedPayload preparedPayload;
    PresentationGeometry::State geometryState;
};

struct ViewportRenderAttempt
{
    quint64 attempt = 0;
    ViewportRenderSnapshot snapshot;
};

struct ViewportRenderHostFact
{
    enum class Outcome {
        Empty,
        Committed,
        Failed,
    };

    Outcome outcome = Outcome::Empty;
    ViewportRenderAcknowledgement acknowledgement;
    ViewportRenderQualityFallbackFact qualityFallback;
    bool imagePresent = false;
};
