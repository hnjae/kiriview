#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>

class ImageViewportPrivate;

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

struct ViewportProviderFrameEvent
{
    ImageSequenceProviderRequestToken token;
};

struct ViewportProviderFrameEventAcceptance
{
    bool accepted = false;
    FramePreparation::ProviderFrameState preparationState;
};

struct ViewportProviderMetadataEvent
{
    ImageSequenceProviderRequestToken token;
};

struct ViewportProviderMetadataEventAcceptance
{
    bool accepted = false;
};

struct ViewportProviderMetadataTerminalResult
{
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::NoRequest;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::NoRequest;
    QString diagnostic;
    QString fallbackDiagnostic;
};

struct ViewportProviderTerminalEvent
{
    enum class Kind {
        Failure,
        Unsupported,
        Cancellation,
    };

    ImageSequenceProviderRequestToken token;
    Kind kind = Kind::Failure;
    ImageSequenceProviderSession::UnsupportedCause unsupportedCause
        = ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
    QString diagnostic;
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

struct ViewportProviderMetadataAdmissionResult
{
    bool accepted = false;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderAcceptedMetadataFacts facts;
};

struct ViewportProviderWaitingEvent
{
    ImageSequenceProviderRequestToken token;
    bool progress = false;
    double progressValue = 0.0;
};

struct ViewportProviderEndOfSequenceEvent
{
    ImageSequenceProviderRequestToken token;
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

struct ViewportProviderRequestTokenAllocation
{
    ImageSequenceProviderRequestToken token;
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
};

struct ViewportProviderMetadataRequestStartResult
{
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
    bool sendCommand = false;
    ImageSequenceProviderRequestToken token;
};

struct ViewportProviderFrameQueueRequest
{
    int frame = -1;
    ImageViewportInternal::ProviderRequestTargetKind targetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
};

struct ViewportProviderFrameQueueResult
{
    ImageSequenceProviderRequestToken cancelToken;
    bool scheduleFlush = false;
};

struct ViewportProviderFrameQueueFlush
{
    bool startRequest = false;
    int frame = -1;
    ImageViewportInternal::ProviderRequestTargetKind targetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
};

struct ViewportProviderFrameRequestStart
{
    int frame = -1;
    ImageViewportInternal::ProviderRequestTargetKind targetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
};

struct ViewportProviderFrameCommand
{
    ImageSequenceProviderRequestToken token;
    int frame = -1;
    int position = -1;
    ImageViewportInternal::ProviderRequestTargetKind targetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
};

struct ViewportProviderFrameRequestStartResult
{
    bool accepted = false;
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
    bool sendCommand = false;
    ViewportProviderFrameCommand command;
};

struct ViewportProviderFrameTransportEffect
{
    ImageSequenceProviderRequestToken cancelToken;
    bool scheduleFlush = false;
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
    bool sendCommand = false;
    ViewportProviderFrameCommand command;
};

struct ViewportProviderFrameDispatchResult
{
    bool accepted = false;
    ViewportProviderFrameTransportEffect transport;
};

struct ViewportProviderTerminalEventResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportProviderEndOfSequenceResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    bool closeSession = false;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportProviderMetadataTargetPolicyResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportCommandResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
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
    ImageViewportInternal::ViewportChangeSet handleProviderFrameEvent(
        ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata);
    ViewportProviderMetadataEventAcceptance acceptProviderMetadataEvent(
        ViewportProviderMetadataEvent event);
    ViewportProviderMetadataAdmissionResult handleProviderMetadataAdmission(
        const ImageSequenceProviderMetadata& metadata);
    ViewportProviderTerminalEventResult handleProviderTerminalEvent(
        const ViewportProviderTerminalEvent& event);
    ImageViewportInternal::ViewportChangeSet handleProviderAcceptedMetadataFacts(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetPolicy(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ViewportProviderWaitingEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderWaiting();
    ViewportProviderEndOfSequenceResult handleProviderEndOfSequenceEvent(
        ViewportProviderEndOfSequenceEvent event);
    ViewportProviderSessionClose handleProviderSessionClose();
    ViewportProviderRequestTokenAllocation allocateProviderRequestToken();
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest();
    ViewportProviderFrameQueueResult queueProviderFrameRequest(
        ViewportProviderFrameQueueRequest request);
    ViewportProviderFrameQueueFlush flushQueuedProviderFrameRequest();
    ViewportProviderFrameRequestStartResult startProviderFrameRequest(
        ViewportProviderFrameRequestStart request);
    ViewportProviderFrameDispatchResult dispatchProviderFrameRequest(
        ViewportProviderFrameRequestStart request);
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
    FramePreparation::ProviderFrameState providerFramePreparationState() const;
    ViewportProviderFrameEventAcceptance acceptProviderFrameEvent(
        ViewportProviderFrameEvent event) const;
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
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetSelection(
        ViewportProviderMetadataTargetSelection selection);
    ImageViewportInternal::ViewportChangeSet handleProviderEndOfSequenceProtocolViolation(
        ViewportProviderEndOfSequenceProtocolViolation violation);
    ViewportProviderEndOfSequenceResult handleProviderPlaybackEndOfSequence();

    ImageViewportPrivate& viewport;
};
