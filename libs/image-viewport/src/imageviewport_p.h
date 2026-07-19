/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportdiagnostics_p.h"
#include "imageviewportplaybackscheduler_p.h"
#include "imageviewportproviderhost_p.h"
#include "imageviewportrenderhost_p.h"
#include "imageviewportstate_p.h"
#include "viewportengine_p.h"
#include <ImageViewport/imageviewport.h>

#include <QtCore/QMutex>

#include <array>
#include <memory>
#include <optional>

class ImageViewportPrivate
{
public:
    using CommandOutcome = ImageViewportCommandOutcome;
    using PageRole = ImageViewportPageRole;
    using ProviderRequestTargetKind = ImageViewportInternal::ProviderRequestTargetKind;

    explicit ImageViewportPrivate(ImageViewport* viewport);
    ~ImageViewportPrivate();
    static ImageViewportPrivate* get(ImageViewport& viewport) { return viewport.d.get(); }
    static const ImageViewportPrivate* get(const ImageViewport& viewport)
    {
        return viewport.d.get();
    }

    ImageViewportStateSnapshot state() const;

    ImageViewportCommandResult clear();
    ImageViewportCommandResult play(PageRole role);
    ImageViewportCommandResult pause(PageRole role);
    ImageViewportCommandResult stop(PageRole role);
    ImageViewportCommandResult seek(PageRole role, int frame);
    ImageViewportCommandResult seekToPosition(PageRole role, int milliseconds);
    ImageViewportCommandResult setPresentationTarget(
        const ImageViewportPresentationTarget& presentationTarget,
        PresentationTargetTransitionPolicy policy);
    ImageViewportCommandResult resetView();
    ImageViewportCommandResult setPresentation(ImageViewportPresentationCommand command);
    ImageViewportCoordinateResult mapPoint(const ImageViewportCoordinateInput& input) const;
    bool containsPoint(const ImageViewportCoordinateInput& input) const;
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds, PageRole role = PageRole::Primary);
    void setPendingPlaybackSchedulerElapsedForTest(
        int elapsedMilliseconds, PageRole role = PageRole::Primary);
    void setNextProviderRequestTokenForTest(quint64 token);
    void setNextProviderRequestTokenForTest(PageRole role, quint64 token);
    void setNextRevisionTokenForTest(quint64 token);
    void failNextProviderCommandDeliveryForTest(PageRole role);
    void failNextProviderQueueFlushSchedulingForTest(PageRole role);
    void useSynchronousProviderExecutorForTest();
    void useSynchronousProviderEventDeliveryForTest();
    void useSynchronousProviderQueueFlushSchedulerForTest();
    ViewportRenderAttempt beginRenderSynchronizationForTest();
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    quint64 secondaryPendingRenderPayloadIdForTest() const;
    quint64 currentRenderAttemptForTest() const;
    void reportRenderQualityFallbackForTest(
        quint64 renderAttempt, bool smoothingUnavailable, bool mipmapUnavailable);
    void discardRetainedDisplayForResourcePressureForTest();
    ImageViewportInternal::RenderFailureDiagnostic
    lastAcceptedRenderFailureDiagnosticForTest() const;
    ImageViewportInternal::ProviderTransportDiagnostic
    lastProviderTransportDiagnosticForTest() const;
    ImageViewportInternal::ProviderSchedulerDiagnostic
    lastProviderSchedulerDiagnosticForTest() const;
    QVector<ImageViewportInternal::InternalObservation> internalObservationsForTest() const;
    void acknowledgeRenderCommitForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderCommitForTest(quint64 generation, quint64 requestId,
        quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId);
    void acknowledgeRenderFailureForTest(
        quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(
        PageRole failedRole, quint64 generation, quint64 requestId, quint64 preparedPayloadId);
    void acknowledgeRenderFailureForTest(PageRole failedRole, quint64 generation, quint64 requestId,
        quint64 preparedPayloadId, RenderFailureCause cause);
#endif
    ImageViewportStateSnapshot applyEngineTransition(ViewportEngineTransition transition);
    ImageViewportStateSnapshot finalizeItemTransaction();
    void enqueueProviderHostEvent(ViewportProviderHostEvent event);
    void drainProviderHostEvents();
    void drainExternalWork();
    void viewportChanged();
    void discardRetainedDisplayForResourcePressure();
    QRectF itemBounds() const;

    ImageViewportCommandResult executePlaybackCommand(ViewportPlaybackCommand command);
    ImageViewportCommandResult commandResult(
        CommandOutcome outcome, const ImageViewportStateSnapshot& snapshot) const;
    void advancePlayback(ViewportPlaybackTimeoutFact fact);
    void flushPlaybackSchedulers();

    double width() const;
    double height() const;
    QQuickWindow* window() const;
    ViewportEngineViewportState viewportState() const;
    void update();
    QSGNode* updatePaintNode(QSGNode* oldNode);
    void prepareRenderSynchronization();
    std::optional<ViewportRenderAttempt> renderAttemptForHost() const;
    void applyRenderHostFact(ViewportRenderHostFact fact);

    ImageViewport* q = nullptr;
    ViewportEngine engine;
    std::array<std::unique_ptr<ImageViewportPlaybackScheduler>, 2> playbackSchedulers;
    ImageViewportProviderHost providerHost;
    ImageViewportRenderHost renderHost {};
    ImageViewportInternal::InternalObservability internalObservability;
    ImageViewportStateSnapshot lastStateSnapshot;
    int transitionApplicationDepth = 0;
    int itemTransactionDepth = 0;
    ViewportPlaybackScheduleBatch pendingPlaybackSchedules;
    QVector<ViewportEngineProviderHostEventRequest> pendingProviderHostEvents;
    bool drainingProviderHostEvents = false;
    ViewportProviderTransportBatch pendingProviderTransport;
    bool drainingExternalWork = false;
    mutable QMutex renderMailboxMutex;
    ViewportRenderAttempt renderMailbox;
    bool renderMailboxValid = false;
};
