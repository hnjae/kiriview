#include "imageviewportdiagnostics_p.h"

#include <limits>
#include <utility>

namespace ImageViewportInternal {

namespace {
    constexpr qsizetype observationCapacity = 256;
}

void InternalObservability::recordProviderCleanupFailure(
    const ProviderTransportDiagnostic& diagnostic)
{
    if (diagnostic.valid) {
        m_lastProviderCleanupFailure = diagnostic;
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::ProviderHost;
        observation.category = InternalObservationCategory::CleanupFailure;
        switch (diagnostic.operation) {
        case ProviderTransportOperation::Cancel:
            observation.cause = InternalObservationCause::ProviderCancelFailed;
            break;
        case ProviderTransportOperation::Close:
            observation.cause = InternalObservationCause::ProviderCloseFailed;
            break;
        case ProviderTransportOperation::Release:
            observation.cause = InternalObservationCause::ProviderReleaseFailed;
            break;
        case ProviderTransportOperation::Destruction:
            observation.cause = InternalObservationCause::ProviderDestructionFailed;
            break;
        case ProviderTransportOperation::None:
            observation.cause = InternalObservationCause::None;
            break;
        }
        observation.identity.roleValid = true;
        observation.identity.role = diagnostic.role;
        observation.identity.generation = diagnostic.generation;
        observation.identity.sessionSerial = diagnostic.sessionSerial;
        observation.identity.providerToken = diagnostic.frameTokenValid
            ? diagnostic.frameTokenValue
            : diagnostic.metadataTokenValue;
        observation.identity.providerLeaseId = diagnostic.providerLeaseId;
        observation.detail = int(diagnostic.operation);
        record(std::move(observation));
    }
}

void InternalObservability::recordProviderSchedulerFailure(
    const ProviderSchedulerDiagnostic& diagnostic)
{
    if (diagnostic.valid) {
        m_lastProviderSchedulerFailure = diagnostic;
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::PlaybackScheduler;
        observation.category = InternalObservationCategory::BackendFailure;
        observation.cause = InternalObservationCause::ProviderSchedulingFailed;
        observation.identity.roleValid = true;
        observation.identity.role = diagnostic.role;
        observation.identity.generation = diagnostic.generation;
        observation.identity.requestId = diagnostic.activeRequestId;
        observation.detail = int(diagnostic.operation);
        record(std::move(observation));
    }
}

void InternalObservability::recordRenderFailure(const RenderFailureDiagnostic& diagnostic)
{
    if (diagnostic.valid) {
        m_lastRenderFailure = diagnostic;
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::RenderHost;
        observation.category = InternalObservationCategory::BackendFailure;
        observation.cause = InternalObservationCause::RenderBackendFailure;
        observation.identity.roleValid = true;
        observation.identity.role = diagnostic.role;
        observation.identity.generation = diagnostic.generation;
        observation.identity.requestId = diagnostic.requestId;
        observation.identity.payloadId = diagnostic.preparedPayloadId;
        observation.identity.renderAttempt = diagnostic.renderAttempt;
        observation.detail = int(diagnostic.cause);
        record(std::move(observation));
    }
}

void InternalObservability::record(InternalObservation observation)
{
    if (m_nextObservationSequence == std::numeric_limits<quint64>::max()) {
        m_nextObservationSequence = 0;
    }
    observation.sequence = ++m_nextObservationSequence;
    if (m_observations.size() == observationCapacity) {
        m_observations.removeFirst();
    }
    m_observations.append(std::move(observation));
}

void InternalObservability::record(const InternalObservationBatch& observations)
{
    for (const InternalObservation& observation : observations) {
        record(observation);
    }
}

ProviderTransportDiagnostic InternalObservability::lastProviderCleanupFailure() const
{
    return m_lastProviderCleanupFailure;
}

ProviderSchedulerDiagnostic InternalObservability::lastProviderSchedulerFailure() const
{
    return m_lastProviderSchedulerFailure;
}

RenderFailureDiagnostic InternalObservability::lastRenderFailure() const
{
    return m_lastRenderFailure;
}

QVector<InternalObservation> InternalObservability::observations() const { return m_observations; }

} // namespace ImageViewportInternal
