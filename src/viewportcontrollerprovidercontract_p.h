#pragma once

#include "framepreparation_p.h"
#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "timingintervals_p.h"
#include "viewportproviderevent_p.h"
#include "viewportplaybackcontract_p.h"

#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QVector>

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
    bool unsupportedCauseExplicit = false;
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

enum class ViewportProviderDeferredControllerEvent {
    None,
    FlushQueuedFrameRequest,
};

struct ViewportProviderFrameQueueResult
{
    ImageSequenceProviderRequestToken cancelToken;
    ViewportProviderDeferredControllerEvent deferredControllerEvent
        = ViewportProviderDeferredControllerEvent::None;
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
    ImageSequenceProviderDisplayDemand demand;
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
    ViewportProviderDeferredControllerEvent deferredControllerEvent
        = ViewportProviderDeferredControllerEvent::None;
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
    bool sendCommand = false;
    ViewportProviderFrameCommand command;
};

struct ViewportProviderTransportCommand
{
    enum class Kind {
        OpenSession,
        SendRequest,
        CloseSession,
        ScheduleDeferredEvent,
    };

    Kind kind = Kind::SendRequest;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ImageSequenceProviderRequest request;
    ViewportProviderSessionClose sessionClose;
    ViewportProviderDeferredControllerEvent deferredEvent
        = ViewportProviderDeferredControllerEvent::None;
    bool reportDispatchFailure = true;
    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory;
    ImageSequenceProviderThreadingContract threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
    quint64 generation = 0;
    quint64 sessionSerial = 0;
};

using ViewportProviderTransportBatch = QVector<ViewportProviderTransportCommand>;

enum class ViewportProviderEventTransportPhase {
    None,
    BeforeChanges,
    AfterChanges,
};

struct ViewportProviderEventResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderEventTransportPhase providerFrameTransportPhase
        = ViewportProviderEventTransportPhase::None;
    ViewportPlaybackScheduleEffect schedule;
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
    ViewportPlaybackScheduleEffect schedule;
};

struct ViewportProviderSchedulerFailureResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::ProviderSchedulerDiagnostic diagnostic;
    ViewportPlaybackScheduleEffect schedule;
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
    ViewportPlaybackScheduleEffect schedule;
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

struct ViewportProviderHostEvent
{
    enum class Kind {
        SessionOpened,
        SessionOpenFailed,
        ProviderEvent,
        DispatchFailed,
        FlushQueuedFrameRequest,
        QueueFlushSchedulingFailed,
    };

    Kind kind = Kind::ProviderEvent;
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ViewportProviderEvent providerEvent;
    ImageSequenceProviderRequestToken token;
    QString diagnostic;
};

struct ViewportProviderHostEventResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderTransportBatch beforeChanges;
    ViewportProviderTransportBatch afterChanges;
    ImageViewportInternal::ProviderSchedulerDiagnostic schedulerDiagnostic;
    ViewportPlaybackScheduleEffect schedule;
};
