#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "presentationgeometry_p.h"
#include "renderfailurecause_p.h"

#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QImage>

#include <memory>

struct ViewportRenderRolePayload
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
};

struct ViewportRenderAcknowledgement
{
    ImageViewportInternal::PreparedPayloadIdentity preparedPayload;
    QVector<ViewportRenderRolePayload> rolePayloads;
    ImageViewport::PageRole failedRole = ImageViewport::PageRole::Primary;
    RenderFailureCause failureCause = RenderFailureCause::None;
};

struct ViewportRenderLayer
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
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

struct ViewportRenderSynchronization
{
    bool pendingTargetCommit = false;
    bool pendingSecondaryProviderCommit = false;
    ImageViewportInternal::PreparedPayload preparedPayload;
    ImageViewport::DisplayStatus oldDisplayStatus = ImageViewport::DisplayStatus::Empty;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
    PresentationGeometry::State geometryState;
    ViewportRenderSnapshot renderSnapshot;
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

struct ViewportProviderMetadataReadyEvent
{
    ImageSequenceProviderRequestToken token;
    ImageSequenceProviderMetadata metadata;
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

struct ViewportProviderDispatchFailureEvent
{
    ImageSequenceProviderRequestToken token;
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
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
};

struct ViewportMetadataProjection
{
    int frameCount = -1;
    int totalDuration = -1;
    ImageViewportRange frameSeekBounds;
    ImageViewportRange positionSeekBounds;
    ImageViewport::TriState timedPlaybackSupport = ImageViewport::TriState::Unavailable;
    ImageViewport::TriState frameSeekSupport = ImageViewport::TriState::Unavailable;
    ImageViewport::TriState positionSeekSupport = ImageViewport::TriState::Unavailable;
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

struct ViewportProviderFrameQueueFlushResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
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

struct ViewportProviderMetadataReadyResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportCommandResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderFrameTransportEffect secondaryProviderFrameTransport;
};

struct ViewportPlaybackAdvanceResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderFrameTransportEffect secondaryProviderFrameTransport;
};

struct ViewportSequenceRoleSource
{
    bool present = false;
    bool provider = false;
    bool timed = false;
    int frameCount = -1;
    int firstFramePosition = -1;
    TimingIntervals timingIntervals;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
};

struct ViewportSequenceAssignment
{
    ImageSequence* sequence = nullptr;
    std::shared_ptr<ImageSequence> sequenceOwner;
    ImageSequence* secondarySequence = nullptr;
    std::shared_ptr<ImageSequence> secondarySequenceOwner;
    ViewportSequenceRoleSource secondarySource;
    bool retainPreviousDisplay = true;
    PageSetTransitionPolicy transitionPolicy;
};

struct ViewportSequenceAssignmentResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderFrameTransportEffect secondaryProviderFrameTransport;
    bool openProviderSession = false;
    bool openSecondaryProviderSession = false;
};

struct ViewportControllerState
{
    ImageViewportInternal::PresentationState presentation;
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::ProviderGenerationState provider;
    ImageViewportInternal::ProviderGenerationState secondaryProvider;
    ViewportSequenceRoleSource secondarySource;
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

    // Viewport geometry inputs.
    virtual QRectF contentRect() const;
    virtual QRectF visibleImageRect() const;
    virtual QRectF itemBounds() const;
    virtual double width() const;
    virtual double height() const;

    // Page-set assignment and active request facts.
    virtual bool hasActiveRequest() const;
    virtual bool hasReadyDisplay() const;
    virtual bool hasDisplayableSequence() const;
    virtual bool hasTimedSequence() const;
    virtual bool hasProviderSequence() const;
    virtual bool hasGenerationTerminalProviderFailure() const;

    // Primary provider construction/runtime facts.
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
    virtual ImageSequenceAuthoredAnimationFacts providerAuthoredAnimationFacts() const;

    // Primary built-in sequence facts and payload data.
    virtual int frameCount() const;
    virtual int totalDuration() const;
    virtual int sequenceFrameCount() const;
    virtual int sequenceTotalDuration() const;
    virtual int sequenceFrameIndexForPosition(int position) const;
    virtual int sequenceFrameStartPosition(int frame) const;
    virtual ImageSequenceAuthoredAnimationFacts sequenceAuthoredAnimationFacts() const;
    virtual QSizeF sequenceLogicalSize() const;
    virtual QImage sequenceFrameImage(int frame) const;

    // Secondary built-in sequence facts and payload data.
    virtual bool hasSecondaryTimedSequence() const;
    virtual int secondarySequenceFrameCount() const;
    virtual int secondarySequenceTotalDuration() const;
    virtual int secondaryTotalDuration() const;
    virtual int secondarySequenceFrameIndexForPosition(int position) const;
    virtual int secondarySequenceFrameStartPosition(int frame) const;
    virtual ImageSequenceAuthoredAnimationFacts secondarySequenceAuthoredAnimationFacts() const;
    virtual QSizeF secondarySequenceLogicalSize() const;
    virtual QImage secondarySequenceFrameImage(int frame) const;

    // Secondary provider construction/runtime facts.
    virtual ImageSequenceProviderKnownFacts secondaryProviderKnownFacts() const;
    virtual QSizeF secondaryProviderKnownLogicalSize() const;
    virtual TimingIntervals secondaryProviderKnownTimingIntervals() const;
    virtual ImageSequenceProviderCapabilitySupport secondaryProviderTimedPlaybackCapability() const;
    virtual ImageSequenceProviderCapabilitySupport secondaryProviderFrameSeekCapability() const;
    virtual ImageSequenceProviderCapabilitySupport secondaryProviderPositionSeekCapability() const;
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
    ImageViewportInternal::ProviderGenerationState& secondaryProviderState();
    const ImageViewportInternal::ProviderGenerationState& secondaryProviderState() const;

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
    ImageSequenceAuthoredAnimationFacts providerAuthoredAnimationFacts() const;
    int frameCount() const;
    int totalDuration() const;
    int sequenceFrameCount() const;
    int sequenceTotalDuration() const;
    int sequenceFrameIndexForPosition(int position) const;
    int sequenceFrameStartPosition(int frame) const;
    ImageSequenceAuthoredAnimationFacts sequenceAuthoredAnimationFacts() const;
    bool hasSecondaryTimedSequence() const;
    int secondarySequenceFrameCount() const;
    int secondarySequenceTotalDuration() const;
    int secondaryTotalDuration() const;
    int secondarySequenceFrameIndexForPosition(int position) const;
    int secondarySequenceFrameStartPosition(int frame) const;
    ImageSequenceAuthoredAnimationFacts secondarySequenceAuthoredAnimationFacts() const;
    ImageSequenceProviderKnownFacts secondaryProviderKnownFacts() const;
    QSizeF secondaryProviderKnownLogicalSize() const;
    TimingIntervals secondaryProviderKnownTimingIntervals() const;
    ImageSequenceProviderCapabilitySupport secondaryProviderTimedPlaybackCapability() const;
    ImageSequenceProviderCapabilitySupport secondaryProviderFrameSeekCapability() const;
    ImageSequenceProviderCapabilitySupport secondaryProviderPositionSeekCapability() const;
    QSizeF sequenceLogicalSize() const;
    QSizeF secondarySequenceLogicalSize() const;
    QImage sequenceFrameImage(int frame) const;
    QImage secondarySequenceFrameImage(int frame) const;
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

    const ImageViewportInternal::PresentationState& presentationState() const;
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
    ImageSequenceAuthoredAnimationFacts providerAuthoredAnimationFacts() const;
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
    ViewportMetadataProjection metadataProjection(ImageViewport::PageRole role) const;
    bool looping() const;
    PresentationGeometry::State geometryState(double devicePixelRatio = 1.0) const;
    PresentationGeometry::State geometryStateForItemBounds(
        const QRectF& itemBounds, double devicePixelRatio = 1.0) const;
    ImageViewportInternal::ViewportChangeSet setLooping(bool looping);
    void incrementDisplayRevision();
    void incrementRequestRevision();
    void incrementCommandRevision();

    ViewportSequenceAssignmentResult assignSequence(ViewportSequenceAssignment assignment);
    ViewportCommandResult rejectInvalidCommand();
    ViewportCommandResult rejectUnsupportedCommand();
    ViewportCommandResult rejectIgnoredNoRequestCommand();
    ViewportProviderFrameTransportEffect closeProviderSession(ImageViewport::PageRole role);
    ViewportCommandResult acceptNoopCommand();
    ViewportCommandResult clear();
    ViewportCommandResult play();
    ViewportCommandResult play(ImageViewport::PageRole role);
    ViewportCommandResult playSecondaryBuiltIn();
    ViewportCommandResult playSecondaryProvider();
    ViewportCommandResult pause();
    ViewportCommandResult pause(ImageViewport::PageRole role);
    ViewportCommandResult stop();
    ViewportCommandResult stop(ImageViewport::PageRole role);
    ViewportCommandResult seek(int frame);
    ViewportCommandResult seek(ImageViewport::PageRole role, int frame);
    ViewportCommandResult seekToPosition(int milliseconds);
    ViewportCommandResult seekToPosition(ImageViewport::PageRole role, int milliseconds);
    ViewportCommandResult seekSecondaryBuiltIn(ImageViewportInternal::DisplayRequestTarget target,
        ImageViewportInternal::ResolvedFrameIdentity resolvedFrame);
    ViewportCommandResult seekSecondaryProvider(int frame);
    ViewportCommandResult seekSecondaryProviderToPosition(int milliseconds);
    ImageViewportInternal::ViewportChangeSet setSmoothing(bool smoothing);
    ImageViewportInternal::ViewportChangeSet setMipmap(bool mipmap);
    ImageViewportInternal::ViewportChangeSet setMirrorHorizontally(bool enabled);
    ImageViewportInternal::ViewportChangeSet setMirrorVertically(bool enabled);
    ImageViewportInternal::ViewportChangeSet setBackgroundMode(ImageViewport::BackgroundMode mode);
    ImageViewportInternal::ViewportChangeSet setBackgroundColor(const QColor& color);
    ViewportCommandResult setSpreadDirection(ImageViewport::SpreadDirection direction);
    ViewportCommandResult setPageGap(double gap);
    ViewportCommandResult setFitMode(ImageViewport::FitMode mode, QPointF anchor);
    ViewportCommandResult setZoomPercent(double percent, QPointF anchor);
    ViewportCommandResult panBy(QPointF delta);
    ViewportCommandResult panToStart();
    ViewportCommandResult panToEnd();
    ViewportCommandResult scanNext();
    ViewportCommandResult scanPrevious();
    ViewportCommandResult rotateClockwise(QPointF anchor);
    ViewportCommandResult rotateCounterClockwise(QPointF anchor);
    ViewportCommandResult setMirrorHorizontally(bool enabled, QPointF anchor);
    ViewportCommandResult setMirrorVertically(bool enabled, QPointF anchor);
    ViewportCommandResult resetView();
    ImageViewportInternal::ViewportChangeSet handleProviderFrameEvent(
        ImageViewport::PageRole role, ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameEvent(
        ViewportProviderFrameEvent event, ImageFrame* frame,
        ImageSequenceProviderFrameMetadata metadata);
    ViewportProviderMetadataEventAcceptance acceptProviderMetadataEvent(
        ViewportProviderMetadataEvent event);
    ViewportProviderMetadataReadyResult handleProviderMetadataReadyEvent(
        ImageViewport::PageRole role, const ViewportProviderMetadataReadyEvent& event);
    ImageViewportInternal::ViewportChangeSet handleProviderSessionOpenFailure(
        const QString& diagnostic);
    ImageViewportInternal::ViewportChangeSet handleProviderSessionOpenFailure(
        ImageViewport::PageRole role, const QString& diagnostic);
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
    ViewportProviderMetadataAdmissionResult handleProviderMetadataAdmission(
        ImageViewport::PageRole role, const ImageSequenceProviderMetadata& metadata);
    ViewportProviderMetadataAdmissionResult handleSecondaryProviderMetadataAdmission(
        const ImageSequenceProviderMetadata& metadata);
    ViewportProviderTerminalEventResult handleProviderTerminalEvent(
        const ViewportProviderTerminalEvent& event);
    ViewportProviderTerminalEventResult handleProviderTerminalEvent(
        ImageViewport::PageRole role, const ViewportProviderTerminalEvent& event);
    ViewportProviderTerminalEventResult handleProviderDispatchFailure(
        ImageViewport::PageRole role, const ViewportProviderDispatchFailureEvent& event);
    ImageViewportInternal::ViewportChangeSet handleProviderAcceptedMetadataFacts(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataEventAcceptance acceptSecondaryProviderMetadataEvent(
        ViewportProviderMetadataEvent event);
    ImageViewportInternal::ViewportChangeSet handleSecondaryProviderAcceptedMetadataFacts(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetPolicy(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ViewportProviderMetadataTargetPolicyResult handleSecondaryProviderMetadataTargetPolicy(
        const ViewportProviderAcceptedMetadataFacts& facts);
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ViewportProviderWaitingEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderWaitingEvent(
        ImageViewport::PageRole role, ViewportProviderWaitingEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderWaiting();
    ViewportProviderEndOfSequenceResult handleProviderEndOfSequenceEvent(
        ViewportProviderEndOfSequenceEvent event);
    ViewportProviderEndOfSequenceResult handleProviderEndOfSequenceEvent(
        ImageViewport::PageRole role, ViewportProviderEndOfSequenceEvent event);
    ViewportProviderFrameTransportEffect closeProviderSession();
    ViewportProviderFrameTransportEffect closeSecondaryProviderSession();
    ViewportProviderSessionClose handleProviderSessionClose();
    ViewportProviderSessionClose handleProviderSessionClose(ImageViewport::PageRole role);
    ViewportProviderSessionClose handleSecondaryProviderSessionClose();
    ViewportProviderRequestTokenAllocation allocateProviderRequestToken();
    ViewportProviderRequestTokenAllocation allocateProviderRequestToken(ImageViewport::PageRole role);
    ViewportProviderRequestTokenAllocation allocateSecondaryProviderRequestToken();
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest();
    ViewportProviderMetadataRequestStartResult startProviderMetadataRequest(
        ImageViewport::PageRole role);
    ViewportProviderMetadataRequestStartResult startSecondaryProviderMetadataRequest();
    ViewportProviderFrameRequestStartResult startSecondaryProviderFrameRequest(int frame);
    ViewportProviderFrameRequestStartResult startSecondaryProviderFrameRequest(
        ImageViewportInternal::DisplayRequestTarget target);
    ViewportProviderFrameQueueResult queueProviderFrameRequest(
        ViewportProviderFrameQueueRequest request);
    ViewportProviderFrameQueueFlush flushQueuedProviderFrameRequest();
    ViewportProviderFrameQueueFlushResult flushQueuedProviderFrameRequestEvent();
    ViewportProviderFrameRequestStartResult startProviderFrameRequest(
        ViewportProviderFrameRequestStart request);
    ViewportProviderFrameDispatchResult dispatchProviderFrameRequest(
        ViewportProviderFrameRequestStart request);
    ImageViewportInternal::ViewportChangeSet handleGeometryChanged(
        const QRectF& oldContentRect, const QRectF& oldVisibleImageRect);
    ViewportRenderSynchronization beginRenderSynchronization(double devicePixelRatio = 1.0);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderCommit(
        const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
        const ViewportRenderSynchronization& synchronization);
    ImageViewportInternal::ViewportChangeSet acknowledgeRenderFailure(
        const ViewportRenderAcknowledgement& acknowledgement);
    int playbackTimerInterval() const;
    ViewportPlaybackAdvanceResult advancePlayback(int elapsedMilliseconds);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    void setNextProviderRequestTokenForTest(quint64 token);
    void setNextProviderRequestTokenForTest(ImageViewport::PageRole role, quint64 token);
    bool hasPendingRenderCommitForTest() const;
    quint64 activeRequestIdForTest() const;
    quint64 displayedRequestIdForTest() const;
    quint64 pendingRenderGenerationForTest() const;
    quint64 pendingRenderPayloadIdForTest() const;
    quint64 secondaryPendingRenderPayloadIdForTest() const;
#endif

private:
    FramePreparation::ProviderFrameState providerFramePreparationState() const;
    FramePreparation::ProviderFrameState providerFramePreparationState(
        ImageViewport::PageRole role) const;
    ViewportProviderFrameEventAcceptance acceptProviderFrameEvent(
        ImageViewport::PageRole role, ViewportProviderFrameEvent event);
    ViewportProviderFrameEventAcceptance acceptProviderFrameEvent(
        ViewportProviderFrameEvent event);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameAdmission(
        const FramePreparation::ProviderFrameAdmissionResult& admission);
    ImageViewportInternal::ViewportChangeSet handleSecondaryProviderFrameAdmission(
        const FramePreparation::ProviderFrameAdmissionResult& admission);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameTerminalResult(
        ImageViewport::PageRole role, const ViewportProviderFrameTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderFrameTerminalResult(
        const ViewportProviderFrameTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTerminalResult(
        ImageViewport::PageRole role, const ViewportProviderMetadataTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTerminalResult(
        const ViewportProviderMetadataTerminalResult& result);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataContradiction(
        const ViewportProviderMetadataContradiction& contradiction);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataContradiction(
        ImageViewport::PageRole role, const ViewportProviderMetadataContradiction& contradiction);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataAdmissionRejection(
        const ViewportProviderMetadataAdmissionRejection& rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataAdmissionRejection(
        ImageViewport::PageRole role, const ViewportProviderMetadataAdmissionRejection& rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTargetRejection(
        ViewportProviderMetadataTargetRejection rejection);
    ImageViewportInternal::ViewportChangeSet handleProviderMetadataTargetRejection(
        ImageViewport::PageRole role, ViewportProviderMetadataTargetRejection rejection);
    ViewportProviderMetadataTargetPolicyResult handleProviderMetadataTargetSelection(
        ViewportProviderMetadataTargetSelection selection);
    ImageViewportInternal::ViewportChangeSet handleProviderEndOfSequenceProtocolViolation(
        ViewportProviderEndOfSequenceProtocolViolation violation);
    ImageViewportInternal::ViewportChangeSet handleSecondaryProviderEndOfSequenceProtocolViolation(
        ViewportProviderEndOfSequenceProtocolViolation violation);
    ViewportProviderEndOfSequenceResult handleProviderPlaybackEndOfSequence();
    ViewportProviderEndOfSequenceResult handleSecondaryProviderPlaybackEndOfSequence();

    ViewportControllerState state;
    ViewportControllerPort viewport;
};
