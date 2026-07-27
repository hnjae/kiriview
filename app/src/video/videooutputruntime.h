// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEOOUTPUTRUNTIME_H
#define KIRIVIEW_VIDEOOUTPUTRUNTIME_H

#include <QMetaObject>
#include <QPointer>
#include <QRectF>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {
class VideoOutputRenderContextObserver;

struct VideoOutputRuntimeCallbacks
{
    std::function<void(QObject*)> setBackendVideoOutput;
    std::function<void(bool)> attachmentProjectionCommitted;
};

class VideoOutputRuntime final
{
public:
    explicit VideoOutputRuntime(QObject* context, VideoOutputRuntimeCallbacks callbacks = {});
    ~VideoOutputRuntime();
    Q_DISABLE_COPY_MOVE(VideoOutputRuntime)

    QObject* videoOutput() const;
    void setVideoOutputAttachment(
        QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect);
    void backendVideoOutputTargetChanged();
    std::optional<int> zoomPercent() const;

private:
    void connectVideoOutputDestroyed(QObject* videoOutput, quint64 outputGeneration);
    void disconnectVideoOutputDestroyed();
    void handleVideoOutputDestroyed(quint64 outputGeneration);
    void handleRenderContextChanged();
    void advanceOutputGeneration();
    void advanceBackendTargetRevision();
    void advanceProjectionDemandRevision();
    void reconcileEffects();
    void scheduleEffectReconciliation();
    [[nodiscard]] std::optional<int> desiredZoomPercent() const;
    [[nodiscard]] bool effectsConverged() const;

    VideoOutputRuntimeCallbacks m_callbacks;
    QPointer<QObject> m_context;
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    std::unique_ptr<VideoOutputRenderContextObserver> m_renderContextObserver;
    QPointer<QObject> m_videoOutput;
    QRectF m_contentRect;
    QRectF m_sourceRect;
    QPointer<QObject> m_committedVideoOutput;
    std::optional<int> m_committedZoomPercent;
    QMetaObject::Connection m_videoOutputDestroyedConnection;
    quint64 m_desiredOutputGeneration = 0;
    quint64 m_observerAppliedOutputGeneration = 0;
    quint64 m_backendAppliedOutputGeneration = 0;
    quint64 m_backendTargetRevision = 0;
    quint64 m_backendAppliedTargetRevision = 0;
    quint64 m_committedOutputGeneration = 0;
    quint64 m_projectionDemandRevision = 0;
    quint64 m_committedProjectionRevision = 0;
    bool m_attachmentPresent = false;
    bool m_committedAttachmentPresent = false;
    bool m_reconcileActive = false;
    bool m_reconcileScheduled = false;
    bool m_closing = false;
};
}

#endif
