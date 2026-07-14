#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "renderfailurecause_p.h"

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
    ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    QVector<ViewportRenderRolePayload> rolePayloads;
    ImageViewportPageRole failedRole = ImageViewportPageRole::Primary;
    RenderFailureCause failureCause = RenderFailureCause::None;
    quint64 synchronizationAttempt = 0;
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
    ImageViewport::BackgroundMode backgroundMode = ImageViewport::BackgroundMode::Transparent;
    QColor backgroundColor = Qt::transparent;
    ImageViewportInternal::PreparedPayload preparedPayload;
    QRectF targetRect;
    QRectF sourceRect;
    int rotationDegrees = 0;
    bool smoothing = true;
    bool mipmap = false;
    bool mirrorHorizontally = false;
    bool mirrorVertically = false;
    QVector<ViewportRenderLayer> imageLayers;
};

struct ViewportRenderSnapshotInput
{
    QSizeF itemSize;
    bool pendingTargetCommit = false;
    ImageViewportInternal::PreparedPayload preparedPayload;
    PresentationGeometry::State geometryState;
};

struct ViewportRenderSynchronization
{
    quint64 attempt = 0;
    bool pendingTargetCommit = false;
    bool pendingSecondaryProviderCommit = false;
    ImageViewportInternal::PreparedPayload preparedPayload;
    ImageViewport::DisplayStatus oldDisplayStatus = ImageViewport::DisplayStatus::Empty;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
    PresentationGeometry::State geometryState;
    ViewportRenderSnapshot renderSnapshot;
};
