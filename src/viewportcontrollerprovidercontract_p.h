#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "timingintervals_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportproviderevent_p.h"

#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QVector>

struct ViewportProviderDispatchFailureEvent
{
    ImageSequenceProviderRequestToken token;
    QString diagnostic;
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

struct ViewportProviderMetadataTransportEffect
{
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
    bool sendCommand = false;
    ImageSequenceProviderRequestToken token;
};

enum class ViewportProviderDeferredControllerEvent {
    None,
    FlushQueuedFrameRequest,
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

struct ViewportProviderSessionOpenFailureResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportPlaybackScheduleEffect schedule;
};

struct ViewportProviderTerminalEventResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportPlaybackScheduleEffect schedule;
};

struct ViewportProviderEndOfSequenceResult
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
