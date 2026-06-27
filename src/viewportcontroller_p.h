#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"

#include <QtCore/QRectF>

class ImageViewportPrivate;

struct ViewportCommandResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::ViewportChangeSet changes;
};

struct ViewportRenderAcknowledgement
{
    quint64 generation = 0;
    quint64 requestId = 0;
    quint64 preparedPayloadId = 0;
};

struct ViewportRenderSynchronization
{
    bool pendingProviderCommit = false;
    ImageViewportInternal::PreparedPayload preparedPayload;
    ImageViewport::DisplayStatus oldDisplayStatus = ImageViewport::DisplayStatus::Empty;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
};

class ViewportController
{
public:
    explicit ViewportController(ImageViewportPrivate& viewport);

    ViewportCommandResult clear();
    ViewportCommandResult play();
    ViewportCommandResult pause();
    ViewportCommandResult stop();
    ViewportCommandResult seek(int frame);
    ViewportCommandResult seekToPosition(int milliseconds);
    ViewportCommandResult resetView();
    ImageViewportInternal::ViewportChangeSet handleGeometryChanged(
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);
    ViewportRenderSynchronization beginRenderSynchronization();
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderCommit(
        ViewportRenderAcknowledgement acknowledgement, bool renderedImagePresent,
        const ViewportRenderSynchronization& synchronization);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderFailure(
        ViewportRenderAcknowledgement acknowledgement);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void advancePlaybackForTest(int elapsedMilliseconds);
    void setNextProviderRequestTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
#endif

private:
    ImageViewportPrivate& viewport;
};
