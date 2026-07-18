#pragma once

#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QVector>

namespace ImageViewportInternal {

enum class InternalObservationSubsystem {
    Engine,
    ProviderHost,
    Preparation,
    RenderHost,
    PlaybackScheduler,
};

enum class InternalObservationCategory {
    StaleDrop,
    AdmissionFailure,
    CleanupFailure,
    BackendFailure,
};

enum class InternalObservationCause {
    None,
    RetiredProviderSession,
    RetiredProviderCommand,
    ProviderTokenMismatch,
    ProviderMetadataRejected,
    ProviderFrameRejected,
    ProviderCancelFailed,
    ProviderCloseFailed,
    ProviderReleaseFailed,
    ProviderDestructionFailed,
    ProviderSchedulingFailed,
    StaleRenderAcknowledgement,
    StaleRenderQualityFallback,
    RenderBackendFailure,
};

struct InternalObservationIdentity
{
    bool roleValid = false;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 generation = 0;
    quint64 sessionSerial = 0;
    quint64 requestId = 0;
    quint64 providerToken = 0;
    quint64 demandRevision = 0;
    quint64 payloadId = 0;
    quint64 providerLeaseId = 0;
    quint64 renderAttempt = 0;
};

struct InternalObservation
{
    quint64 sequence = 0;
    InternalObservationSubsystem subsystem = InternalObservationSubsystem::Engine;
    InternalObservationCategory category = InternalObservationCategory::StaleDrop;
    InternalObservationCause cause = InternalObservationCause::None;
    InternalObservationIdentity identity;
    int detail = 0;
};

using InternalObservationBatch = QVector<InternalObservation>;

} // namespace ImageViewportInternal
