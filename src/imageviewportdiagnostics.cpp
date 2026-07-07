#include "imageviewportdiagnostics_p.h"

namespace ImageViewportInternal {

void InternalDiagnostics::recordProviderCleanupFailure(
    const ProviderTransportDiagnostic& diagnostic)
{
    if (diagnostic.valid) {
        m_lastProviderCleanupFailure = diagnostic;
    }
}

void InternalDiagnostics::recordProviderSchedulerFailure(
    const ProviderSchedulerDiagnostic& diagnostic)
{
    if (diagnostic.valid) {
        m_lastProviderSchedulerFailure = diagnostic;
    }
}

void InternalDiagnostics::recordRenderFailure(const RenderFailureDiagnostic& diagnostic)
{
    if (diagnostic.valid) {
        m_lastRenderFailure = diagnostic;
    }
}

ProviderTransportDiagnostic InternalDiagnostics::lastProviderCleanupFailure() const
{
    return m_lastProviderCleanupFailure;
}

ProviderSchedulerDiagnostic InternalDiagnostics::lastProviderSchedulerFailure() const
{
    return m_lastProviderSchedulerFailure;
}

RenderFailureDiagnostic InternalDiagnostics::lastRenderFailure() const
{
    return m_lastRenderFailure;
}

} // namespace ImageViewportInternal
