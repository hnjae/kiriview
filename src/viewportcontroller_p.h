#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>

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

struct ViewportProviderFrameTerminalResult
{
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
    QString diagnostic;
    QString fallbackDiagnostic;
};

struct ViewportProviderMetadataTerminalResult
{
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
    QString diagnostic;
    QString fallbackDiagnostic;
};

struct ViewportProviderMetadataContradiction
{
    QString diagnostic;
};

struct ViewportProviderMetadataAdmissionRejection
{
    QString diagnostic;
};

struct ViewportProviderMetadataTargetRejection
{
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::Unsupported;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::UnsupportedRequest;
    int selectedFrame = -1;
    bool updateActiveTarget = false;
    bool selectedFromPosition = false;
    bool clearPlaybackStartPending = false;
};

struct ViewportProviderMetadataTargetSelection
{
    ImageViewportInternal::ProviderRequestTargetKind targetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    int selectedFrame = -1;
    bool selectedFromPosition = false;
    bool timedMetadata = false;
};

struct ViewportProviderAcceptedMetadataFacts
{
    bool timedMetadata = false;
    bool timedPlaybackSupport = false;
    bool frameSeekSupport = false;
    bool positionSeekSupport = false;
    QSizeF logicalSize;
    TimingIntervals timingIntervals;
};

struct ViewportProviderEndOfSequenceProtocolViolation
{
    bool activeMetadataToken = false;
    bool activeFrameToken = false;
};

struct ViewportProviderSessionClose
{
    ImageSequenceProviderRequestToken metadataToken;
    ImageSequenceProviderRequestToken frameToken;
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
    FramePreparation::ProviderFrameState providerFramePreparationState() const;
    ImageViewportInternal::ViewportChangeSet handleProviderFrameAdmission(
        const FramePreparation::ProviderFrameAdmissionResult& admission);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameTerminalResult(
        const ViewportProviderFrameTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTerminalResult(
        const ViewportProviderMetadataTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataContradiction(
        const ViewportProviderMetadataContradiction& contradiction);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataAdmissionRejection(
        const ViewportProviderMetadataAdmissionRejection& rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTargetRejection(
        ViewportProviderMetadataTargetRejection rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTargetSelection(
        ViewportProviderMetadataTargetSelection selection);
    ImageViewportInternal::ViewportChangeSet handleProviderAcceptedMetadataFacts(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ImageViewportInternal::ViewportChangeSet handleProviderWaiting();
    ImageViewportInternal::ViewportChangeSet handleProviderEndOfSequenceProtocolViolation(
        ViewportProviderEndOfSequenceProtocolViolation violation);
    ImageViewportInternal::ViewportChangeSet handleProviderPlaybackEndOfSequence();
    ViewportProviderSessionClose handleProviderSessionClose();
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
