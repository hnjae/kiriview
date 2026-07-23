/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"
#include "internalobservation_p.h"
#include "timingintervals_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportproviderevent_p.h"

#include <QtCore/QSizeF>
#include <QtCore/QVector>

struct ViewportProviderDispatchFailureEvent
{
    ImageSequenceProviderRequestToken token;
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

enum class ViewportProviderDeferredEngineEvent {
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
    ViewportProviderDeferredEngineEvent deferredEngineEvent
        = ViewportProviderDeferredEngineEvent::None;
    bool closeSession = false;
    ViewportProviderSessionClose sessionClose;
    bool sendCommand = false;
    ViewportProviderFrameCommand command;
};

struct ViewportProviderTransportCommand
{
    enum class Kind {
        OpenSession,
        ActivateSession,
        SendRequest,
        CloseSession,
        ScheduleDeferredEvent,
    };

    Kind kind = Kind::SendRequest;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequest request;
    ViewportProviderSessionClose sessionClose;
    ViewportProviderDeferredEngineEvent deferredEvent = ViewportProviderDeferredEngineEvent::None;
    bool reportDispatchFailure = true;
    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory;
    ImageSequenceProviderThreadingContract threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
    quint64 generation = 0;
    quint64 sessionSerial = 0;
};

struct ViewportEngineProviderSessionOpenEffect
{
    bool openSession = false;
    ViewportProviderTransportCommand command;
};

using ViewportProviderTransportBatch = QVector<ViewportProviderTransportCommand>;

struct ViewportProviderEventResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportPlaybackScheduleBatch schedules;
    ImageViewportInternal::InternalObservationBatch observations;
};

struct ViewportProviderFrameQueueFlushResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportPlaybackScheduleBatch schedules;
};

struct ViewportProviderSchedulerFailureResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::ProviderSchedulerDiagnostic diagnostic;
    ViewportPlaybackScheduleBatch schedules;
};

struct ViewportProviderSessionOpenResult
{
    ViewportProviderMetadataTransportEffect providerMetadataTransport;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportProviderSessionOpenFailureResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportPlaybackScheduleBatch schedules;
};

struct ViewportProviderTerminalEventResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportPlaybackScheduleBatch schedules;
    ImageViewportInternal::InternalObservationBatch observations;
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
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportProviderEvent providerEvent;
    ImageSequenceProviderRequestToken token;
    bool providerFailureAvailable = false;
    ImageSequenceProviderFailureCause providerCause
        = ImageSequenceProviderFailureCause::Unavailable;
    ImageSequenceProviderFailureReference providerReference;
    quint64 providerFailureLeaseId = 0;
    quint64 generation = 0;
    quint64 sessionSerial = 0;
};
