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
    ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    QVector<ViewportRenderRolePayload> rolePayloads;
    ImageViewportPageRole failedRole = ImageViewportPageRole::Primary;
    RenderFailureCause failureCause = RenderFailureCause::None;
    quint64 synchronizationAttempt = 0;
};

struct ViewportRenderQualityFallbackFact
{
    quint64 synchronizationAttempt = 0;
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
    ImageViewportDisplayStatus oldDisplayStatus = ImageViewportDisplayStatus::Empty;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
    PresentationGeometry::State geometryState;
    ViewportRenderSnapshot renderSnapshot;
};
