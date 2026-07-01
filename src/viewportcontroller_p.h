#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtGui/QImage>

#include <memory>

class ImageViewportPrivate;

struct ViewportRenderAcknowledgement
{
    ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
};

struct ViewportRenderSynchronization
{
    bool pendingProviderCommit = false;
    bool pendingSecondaryCommit = false;
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

struct ViewportProviderMetadataTransportEffect
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
    ImageViewportInternal::DisplayRequestTarget target;
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

struct ViewportProviderSessionOpenResult
{
    ViewportProviderMetadataTransportEffect providerMetadataTransport;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportProviderTerminalEventResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportProviderMetadataAdmissionResult
{
    bool accepted = false;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderAcceptedMetadataFacts facts;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportProviderEndOfSequenceResult
{
    ImageViewportInternal::ViewportChangeSet changes;
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

struct ViewportPlaybackAdvanceResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderFrameTransportEffect secondaryProviderFrameTransport;
};

struct ViewportSequenceAssignment
{
    ImageSequence* sequence = nullptr;
    std::shared_ptr<ImageSequence> sequenceOwner;
    ImageSequence* secondarySequence = nullptr;
    std::shared_ptr<ImageSequence> secondarySequenceOwner;
    ImageViewportInternal::DisplayRequestTarget secondaryInitialTarget;
    ImageViewportInternal::ResolvedFrameIdentity secondaryInitialResolvedFrame;
    bool retainPreviousDisplay = true;
    bool secondaryIsProvider = false;
};

struct ViewportSequenceAssignmentResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderFrameTransportEffect secondaryProviderFrameTransport;
    bool openProviderSession = false;
    bool openSecondaryProviderSession = false;
};

struct ViewportPresentationReset
{
    bool changed = false;
    bool geometryState = false;
};

struct ViewportControllerState
{
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::ProviderGenerationState provider;
    ImageViewportInternal::ProviderGenerationState secondaryProvider;
};

class ViewportControllerContext
{
public:
    ViewportControllerContext() = default;
    ViewportControllerContext(const ViewportControllerContext&) = delete;
    ViewportControllerContext& operator=(const ViewportControllerContext&) = delete;
    ViewportControllerContext(ViewportControllerContext&&) = delete;
    ViewportControllerContext& operator=(ViewportControllerContext&&) = delete;
    virtual ~ViewportControllerContext() = default;

    virtual QRectF contentRect() const;
    virtual QRectF visibleImageRect() const;
    virtual QRectF itemBounds() const;
    virtual bool hasActiveRequest() const;
    virtual bool hasReadyDisplay() const;
    virtual bool hasDisplayableSequence() const;
    virtual bool hasTimedSequence() const;
    virtual bool hasProviderSequence() const;
    virtual bool hasGenerationTerminalProviderFailure() const;
    virtual bool providerHasCompleteKnownMetadata() const;
    virtual ImageSequenceProviderKnownFacts providerKnownFacts() const;
    virtual QSizeF providerKnownLogicalSize() const;
    virtual TimingIntervals providerKnownTimingIntervals() const;
    virtual ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const;
    virtual ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const;
    virtual ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const;
    virtual bool providerTimedPlaybackCapabilityKnownFalse() const;
    virtual bool providerFrameSeekCapabilityKnownFalse() const;
    virtual bool providerFrameSeekCapabilityKnownTrue() const;
    virtual bool providerPositionSeekCapabilityKnownFalse() const;
    virtual bool providerKnownFactsTimedFrameCount() const;
    virtual int providerKnownFactsFrameCount() const;
    virtual int providerFrameStartPosition(int frame) const;
    virtual int providerFrameIndexForPosition(int position) const;
    virtual int frameCount() const;
    virtual int totalDuration() const;
    virtual int sequenceFrameCount() const;
    virtual int sequenceFrameIndexForPosition(int position) const;
    virtual int sequenceFrameStartPosition(int frame) const;
    virtual bool hasSecondaryTimedSequence() const;
    virtual int secondarySequenceFrameCount() const;
    virtual int secondaryTotalDuration() const;
    virtual int secondarySequenceFrameIndexForPosition(int position) const;
    virtual int secondarySequenceFrameStartPosition(int frame) const;
    virtual QSizeF sequenceLogicalSize() const;
    virtual QImage sequenceFrameImage(int frame) const;
    virtual double width() const;
    virtual double height() const;
};

class ViewportControllerPort
{
public:
    ViewportControllerPort(
        const ViewportControllerContext& context, ViewportControllerState& state);

    ImageViewportInternal::DisplayState& displayState();
    const ImageViewportInternal::DisplayState& displayState() const;
    ImageViewportInternal::RequestState& requestState();
    const ImageViewportInternal::RequestState& requestState() const;
    ImageViewportInternal::ProviderGenerationState& providerState();
    const ImageViewportInternal::ProviderGenerationState& providerState() const;

    QRectF contentRect() const;
    QRectF visibleImageRect() const;
    QRectF itemBounds() const;
    bool hasActiveRequest() const;
    bool hasReadyDisplay() const;
    bool hasDisplayableSequence() const;
    bool hasTimedSequence() const;
    bool hasProviderSequence() const;
    bool hasGenerationTerminalProviderFailure() const;
    bool providerHasCompleteKnownMetadata() const;
    ImageSequenceProviderKnownFacts providerKnownFacts() const;
    QSizeF providerKnownLogicalSize() const;
    TimingIntervals providerKnownTimingIntervals() const;
    ImageSequenceProviderCapabilitySupport providerTimedPlaybackCapability() const;
    ImageSequenceProviderCapabilitySupport providerFrameSeekCapability() const;
    ImageSequenceProviderCapabilitySupport providerPositionSeekCapability() const;
    bool providerTimedPlaybackCapabilityKnownFalse() const;
    bool providerFrameSeekCapabilityKnownFalse() const;
    bool providerFrameSeekCapabilityKnownTrue() const;
    bool providerPositionSeekCapabilityKnownFalse() const;
    bool providerKnownFactsTimedFrameCount() const;
    int providerKnownFactsFrameCount() const;
    int providerFrameStartPosition(int frame) const;
    int providerFrameIndexForPosition(int position) const;
    int frameCount() const;
    int totalDuration() const;
    int sequenceFrameCount() const;
    int sequenceFrameIndexForPosition(int position) const;
    int sequenceFrameStartPosition(int frame) const;
    bool hasSecondaryTimedSequence() const;
    int secondarySequenceFrameCount() const;
    int secondaryTotalDuration() const;
    int secondarySequenceFrameIndexForPosition(int position) const;
    int secondarySequenceFrameStartPosition(int frame) const;
    QSizeF sequenceLogicalSize() const;
    QImage sequenceFrameImage(int frame) const;
    double width() const;
    double height() const;

private:
    const ViewportControllerContext& context;
    ViewportControllerState& state;
};

class ViewportController
{
public:
    explicit ViewportController(const ViewportControllerContext& context);

    const ImageViewportInternal::DisplayState& displayState() const;
    const ImageViewportInternal::RequestState& requestState() const;
    bool hasProviderSession() const;
    bool hasProviderSession(ImageViewport::PageRole role) const;
    bool providerMetadataReady() const;
    bool secondaryProviderMetadataReady() const;
    bool secondaryProviderTimedMetadata() const;
    bool secondaryProviderTimedPlaybackSupported() const;
    bool secondaryProviderFrameSeekSupported() const;
    bool secondaryProviderPositionSeekSupported() const;
    QSizeF secondaryProviderLogicalSize() const;
    int secondaryProviderFrameCount() const;
    int secondaryProviderTotalDuration() const;
    bool providerTimedMetadata() const;
    bool providerTimedPlaybackSupported() const;
    bool providerFrameSeekSupported() const;
    bool providerPositionSeekSupported() const;
    QSizeF providerLogicalSize() const;
    int providerFrameCount() const;
    int providerTotalDuration() const;
    int providerFrameDuration(int frame) const;
    int providerFrameStartPosition(int frame) const;
    int providerFrameIndexForPosition(int position) const;
    bool looping() const;
    ImageViewportInternal::ViewportChangeSet setLooping(bool looping);
    void incrementDisplayRevision();
    void incrementRequestRevision();
    void incrementCommandRevision();

    ViewportSequenceAssignmentResult assignSequence(ViewportSequenceAssignment assignment);
    ViewportCommandResult rejectInvalidCommand();
    ViewportCommandResult rejectUnsupportedCommand();
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewport::PageRole role);
    ViewportCommandResult acceptNoopCommand();
    ViewportCommandResult clear();
    ViewportCommandResult play();
    ViewportCommandResult playSecondaryBuiltIn();
    ViewportCommandResult playSecondaryProvider();
    ViewportCommandResult pause();
    ViewportCommandResult pause(ImageViewport::PageRole role);
    ViewportCommandResult stop();
    ViewportCommandResult stop(ImageViewport::PageRole role);
    ViewportCommandResult seek(int frame);
    ViewportCommandResult seekToPosition(int milliseconds);
    ViewportCommandResult seekSecondaryBuiltIn(
        ImageViewportInternal::DisplayRequestTarget target,
        ImageViewportInternal::ResolvedFrameIdentity resolvedFrame);
    ViewportCommandResult resetView(ViewportPresentationReset reset);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameEvent(
        ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata);
    ImageViewportInternal::ViewportChangeSet handleSecondaryProviderFrameEvent(
        ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata);
    ViewportProviderMetadataEventAcceptance acceptProviderMetadataEvent(
        ViewportProviderMetadataEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderSessionOpenFailure(
        const QString& diagnostic);
    ViewportProviderSessionOpenResult handleProviderSessionOpened();
    ViewportProviderSessionOpenResult handleProviderSessionOpened(ImageViewport::PageRole role);
    ViewportProviderMetadataRequestStartResult handleSecondaryProviderSessionOpened();
    quint64 installProviderSession(ImageSequenceProviderSession* session);
    quint64 installProviderSession(
        ImageViewport::PageRole role, ImageSequenceProviderSession* session);
    ImageSequenceProviderSession* takeProviderSession();
    ImageSequenceProviderSession* takeProviderSession(ImageViewport::PageRole role);
    ImageSequenceProviderSession* currentProviderSession() const;
    ImageSequenceProviderSession* currentProviderSession(ImageViewport::PageRole role) const;
    bool acceptsProviderSessionResult(quint64 sessionSerial) const;
    bool acceptsProviderSessionResult(ImageViewport::PageRole role, quint64 sessionSerial) const;
    ViewportProviderMetadataAdmissionResult handleProviderMetadataAdmission(
        const ImageSequenceProviderMetadata& metadata);
    ViewportProviderTerminalEventResult handleProviderTerminalEvent(
        const ViewportProviderTerminalEvent& event);
    ImageViewportInternal::ViewportChangeSet handleProviderAcceptedMetadataFacts(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataEventAcceptance acceptSecondaryProviderMetadataEvent(
        ViewportProviderMetadataEvent event);
    ImageViewportInternal::ViewportChangeSet handleSecondaryProviderAcceptedMetadataFacts(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetPolicy(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ViewportProviderWaitingEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderWaiting();
    ViewportProviderEndOfSequenceResult handleProviderEndOfSequenceEvent(
        ViewportProviderEndOfSequenceEvent event);
    ViewportProviderFrameTransportEffect closeProviderSession();
    ViewportProviderFrameTransportEffect closeSecondaryProviderSession();
    ViewportProviderSessionClose handleProviderSessionClose();
    ViewportProviderSessionClose handleSecondaryProviderSessionClose();
    ViewportProviderRequestTokenAllocation allocateProviderRequestToken();
    ViewportProviderRequestTokenAllocation allocateSecondaryProviderRequestToken();
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest();
    ViewportProviderMetadataRequestStartResult startSecondaryProviderMetadataRequest();
    ViewportProviderFrameRequestStartResult startSecondaryProviderFrameRequest(int frame);
    ViewportProviderFrameRequestStartResult startSecondaryProviderFrameRequest(
        ImageViewportInternal::DisplayRequestTarget target);
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
    int playbackTimerInterval() const;
    ViewportPlaybackAdvanceResult advancePlayback(int elapsedMilliseconds);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void setNextProviderRequestTokenForTest(quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
#endif

private:
    FramePreparation::ProviderFrameState providerFramePreparationState() const;
    ViewportProviderFrameEventAcceptance acceptProviderFrameEvent(
        ViewportProviderFrameEvent event) const;
    ViewportProviderFrameEventAcceptance acceptSecondaryProviderFrameEvent(
        ViewportProviderFrameEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameAdmission(
        const FramePreparation::ProviderFrameAdmissionResult& admission);
    ImageViewportInternal::ViewportChangeSet handleSecondaryProviderFrameAdmission(
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

    ViewportControllerState state;
    ViewportControllerPort viewport;
};
