#pragma once

#include "imageviewportstate_p.h"
#include "internalobservation_p.h"

namespace ImageViewportInternal {

class InternalObservability
{
public:
    void recordProviderCleanupFailure(const ProviderTransportDiagnostic& diagnostic);
    void recordProviderSchedulerFailure(const ProviderSchedulerDiagnostic& diagnostic);
    void recordRenderFailure(const RenderFailureDiagnostic& diagnostic);
    void record(InternalObservation observation);
    void record(const InternalObservationBatch& observations);

    ProviderTransportDiagnostic lastProviderCleanupFailure() const;
    ProviderSchedulerDiagnostic lastProviderSchedulerFailure() const;
    RenderFailureDiagnostic lastRenderFailure() const;
    QVector<InternalObservation> observations() const;

private:
    ProviderTransportDiagnostic m_lastProviderCleanupFailure;
    ProviderSchedulerDiagnostic m_lastProviderSchedulerFailure;
    RenderFailureDiagnostic m_lastRenderFailure;
    QVector<InternalObservation> m_observations;
    quint64 m_nextObservationSequence = 0;
};

} // namespace ImageViewportInternal
