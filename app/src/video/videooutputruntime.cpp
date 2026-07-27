// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videooutputruntime.h"

#include "video/videooutputrendercontextobserver.h"
#include "video/videozoomstate.h"

#include <QMetaObject>
#include <QObject>
#include <cstddef>
#include <utility>

namespace {
constexpr std::size_t maximumEffectsPerReconciliation = 64;
}

namespace kiriview {
VideoOutputRuntime::VideoOutputRuntime(QObject* context, VideoOutputRuntimeCallbacks callbacks)
    : m_callbacks(std::move(callbacks))
    , m_context(context)
    , m_renderContextObserver(std::make_unique<VideoOutputRenderContextObserver>(
          [this]() { handleRenderContextChanged(); }))
{
}

VideoOutputRuntime::~VideoOutputRuntime()
{
    m_closing = true;
    const QPointer<QObject> context = m_context;
    const auto setBackendVideoOutput = m_callbacks.setBackendVideoOutput;
    m_callbackLifetime.reset();
    disconnectVideoOutputDestroyed();
    m_renderContextObserver.reset();
    m_attachmentPresent = false;
    m_videoOutput = nullptr;
    m_contentRect = {};
    m_sourceRect = {};
    m_committedAttachmentPresent = false;
    m_committedVideoOutput = nullptr;
    m_committedZoomPercent.reset();
    if (context != nullptr && setBackendVideoOutput) {
        setBackendVideoOutput(nullptr);
    }
}

QObject* VideoOutputRuntime::videoOutput() const
{
    return m_closing || !m_committedAttachmentPresent ? nullptr : m_committedVideoOutput.data();
}

void VideoOutputRuntime::setVideoOutputAttachment(
    QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect)
{
    if (m_closing) {
        return;
    }

    const bool requestedAttachmentPresent = videoOutput != nullptr;
    const QRectF requestedContentRect = requestedAttachmentPresent ? contentRect : QRectF();
    const QRectF requestedSourceRect = requestedAttachmentPresent ? sourceRect : QRectF();
    const bool outputChanged = m_attachmentPresent != requestedAttachmentPresent
        || (requestedAttachmentPresent && m_videoOutput.data() != videoOutput);
    const bool geometryChanged = requestedAttachmentPresent
        && (m_contentRect != requestedContentRect || m_sourceRect != requestedSourceRect);
    if (!outputChanged && !geometryChanged) {
        reconcileEffects();
        return;
    }

    if (outputChanged) {
        disconnectVideoOutputDestroyed();
        advanceOutputGeneration();
    }

    m_attachmentPresent = requestedAttachmentPresent;
    m_videoOutput = videoOutput;
    m_contentRect = requestedContentRect;
    m_sourceRect = requestedSourceRect;
    if (outputChanged && requestedAttachmentPresent) {
        connectVideoOutputDestroyed(videoOutput, m_desiredOutputGeneration);
    }

    advanceProjectionDemandRevision();
    reconcileEffects();
}

void VideoOutputRuntime::backendVideoOutputTargetChanged()
{
    if (m_closing) {
        return;
    }
    advanceBackendTargetRevision();
    reconcileEffects();
}

std::optional<int> VideoOutputRuntime::zoomPercent() const
{
    return m_closing ? std::nullopt : m_committedZoomPercent;
}

void VideoOutputRuntime::connectVideoOutputDestroyed(QObject* videoOutput, quint64 outputGeneration)
{
    if (m_closing || m_renderContextObserver == nullptr) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    m_videoOutputDestroyedConnection = QObject::connect(videoOutput, &QObject::destroyed,
        m_renderContextObserver.get(), [this, lifetime, outputGeneration]() {
            if (!lifetime.expired()) {
                handleVideoOutputDestroyed(outputGeneration);
            }
        });
}

void VideoOutputRuntime::disconnectVideoOutputDestroyed()
{
    if (m_videoOutputDestroyedConnection) {
        QObject::disconnect(m_videoOutputDestroyedConnection);
        m_videoOutputDestroyedConnection = {};
    }
}

void VideoOutputRuntime::handleVideoOutputDestroyed(quint64 outputGeneration)
{
    if (m_closing || !m_attachmentPresent || outputGeneration != m_desiredOutputGeneration) {
        return;
    }
    setVideoOutputAttachment(nullptr, {}, {});
}

void VideoOutputRuntime::handleRenderContextChanged()
{
    if (m_closing) {
        return;
    }
    advanceProjectionDemandRevision();
    reconcileEffects();
}

void VideoOutputRuntime::advanceOutputGeneration()
{
    ++m_desiredOutputGeneration;
    if (m_desiredOutputGeneration == 0) {
        m_desiredOutputGeneration = 1;
        m_observerAppliedOutputGeneration = 0;
        m_backendAppliedOutputGeneration = 0;
        m_committedOutputGeneration = 0;
    }
}

void VideoOutputRuntime::advanceBackendTargetRevision()
{
    ++m_backendTargetRevision;
    if (m_backendTargetRevision == 0) {
        m_backendTargetRevision = 1;
        m_backendAppliedTargetRevision = 0;
    }
}

void VideoOutputRuntime::advanceProjectionDemandRevision()
{
    ++m_projectionDemandRevision;
    if (m_projectionDemandRevision == 0) {
        m_projectionDemandRevision = 1;
        m_committedProjectionRevision = 0;
    }
}

void VideoOutputRuntime::reconcileEffects()
{
    if (m_closing || m_reconcileActive || m_context == nullptr) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const QPointer<QObject> context = m_context;
    m_reconcileActive = true;
    std::size_t appliedEffectCount = 0;
    while (appliedEffectCount < maximumEffectsPerReconciliation) {
        if (m_observerAppliedOutputGeneration != m_desiredOutputGeneration) {
            const quint64 outputGeneration = m_desiredOutputGeneration;
            const QPointer<QObject> videoOutput = m_attachmentPresent ? m_videoOutput : nullptr;
            m_observerAppliedOutputGeneration = outputGeneration;
            ++appliedEffectCount;
            m_renderContextObserver->setVideoOutput(videoOutput.data());
            if (lifetime.expired()) {
                return;
            }
            if (context.isNull()) {
                m_reconcileActive = false;
                return;
            }
            continue;
        }

        if (m_backendAppliedOutputGeneration != m_desiredOutputGeneration
            || m_backendAppliedTargetRevision != m_backendTargetRevision) {
            const quint64 outputGeneration = m_desiredOutputGeneration;
            const quint64 backendTargetRevision = m_backendTargetRevision;
            const QPointer<QObject> videoOutput = m_attachmentPresent ? m_videoOutput : nullptr;
            const auto setBackendVideoOutput = m_callbacks.setBackendVideoOutput;
            m_backendAppliedOutputGeneration = outputGeneration;
            m_backendAppliedTargetRevision = backendTargetRevision;
            ++appliedEffectCount;
            if (setBackendVideoOutput) {
                setBackendVideoOutput(videoOutput.data());
                if (lifetime.expired()) {
                    return;
                }
                if (context.isNull()) {
                    m_reconcileActive = false;
                    return;
                }
            }
            continue;
        }

        if (m_committedOutputGeneration != m_desiredOutputGeneration
            || m_committedProjectionRevision != m_projectionDemandRevision) {
            const quint64 outputGeneration = m_desiredOutputGeneration;
            const quint64 projectionRevision = m_projectionDemandRevision;
            const bool attachmentPresent = m_attachmentPresent && !m_videoOutput.isNull();
            const QPointer<QObject> videoOutput = attachmentPresent ? m_videoOutput : nullptr;
            const std::optional<int> zoomPercent = desiredZoomPercent();
            const bool outputChanged = m_committedAttachmentPresent != attachmentPresent
                || (attachmentPresent && m_committedVideoOutput != videoOutput);
            const auto attachmentProjectionCommitted = m_callbacks.attachmentProjectionCommitted;
            m_committedAttachmentPresent = attachmentPresent;
            m_committedVideoOutput = videoOutput;
            m_committedZoomPercent = zoomPercent;
            m_committedOutputGeneration = outputGeneration;
            m_committedProjectionRevision = projectionRevision;
            ++appliedEffectCount;
            if (attachmentProjectionCommitted) {
                attachmentProjectionCommitted(outputChanged);
                if (lifetime.expired()) {
                    return;
                }
                if (context.isNull()) {
                    m_reconcileActive = false;
                    return;
                }
            }
            continue;
        }

        m_reconcileActive = false;
        return;
    }

    m_reconcileActive = false;
    if (!effectsConverged()) {
        scheduleEffectReconciliation();
    }
}

void VideoOutputRuntime::scheduleEffectReconciliation()
{
    if (m_closing || m_reconcileScheduled || m_context == nullptr) {
        return;
    }

    m_reconcileScheduled = true;
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const bool scheduled = QMetaObject::invokeMethod(
        m_context.data(),
        [this, lifetime]() {
            if (lifetime.expired()) {
                return;
            }
            if (m_closing) {
                return;
            }
            m_reconcileScheduled = false;
            reconcileEffects();
        },
        Qt::QueuedConnection);
    if (!scheduled) {
        m_reconcileScheduled = false;
    }
}

std::optional<int> VideoOutputRuntime::desiredZoomPercent() const
{
    if (!m_attachmentPresent || m_renderContextObserver == nullptr) {
        return std::nullopt;
    }

    const std::optional<qreal> devicePixelRatio = m_renderContextObserver->devicePixelRatio();
    if (!devicePixelRatio.has_value()) {
        return std::nullopt;
    }

    return videoZoomPercentForRects(m_contentRect, m_sourceRect, devicePixelRatio.value());
}

bool VideoOutputRuntime::effectsConverged() const
{
    return m_observerAppliedOutputGeneration == m_desiredOutputGeneration
        && m_backendAppliedOutputGeneration == m_desiredOutputGeneration
        && m_backendAppliedTargetRevision == m_backendTargetRevision
        && m_committedOutputGeneration == m_desiredOutputGeneration
        && m_committedProjectionRevision == m_projectionDemandRevision;
}
}
