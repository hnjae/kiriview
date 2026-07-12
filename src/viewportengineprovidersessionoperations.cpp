#include "viewportengineprovidersessionoperations_p.h"

ViewportEngineProviderSessionOpenEffect beginViewportEngineProviderSession(
    ViewportEngineProviderSessionOpenInput input, ViewportEngineProviderSessionOpenAccess access)
{
    ViewportEngineProviderSessionOpenEffect result;
    access.m_session.sessionActive = true;
    ++access.m_session.sessionSerial;
    result.openSession = true;
    result.command.kind = ViewportProviderTransportCommand::Kind::OpenSession;
    result.command.role = input.role;
    result.command.sessionFactory = access.m_source.providerSessionFactory;
    result.command.threadingContract = access.m_source.facts.providerThreadingContract;
    result.command.generation = input.generation;
    result.command.sessionSerial = access.m_session.sessionSerial;
    return result;
}

ViewportProviderFrameTransportEffect closeViewportEngineProviderSession(
    ViewportEngineProviderSessionCloseAccess access)
{
    ViewportProviderFrameTransportEffect effect;
    effect.closeSession = access.m_session.sessionActive;
    access.m_requests.queuedFrameRequest = false;
    access.m_requests.queuedFrameGeneration = 0;
    access.m_requests.queuedFrameRequestId = 0;
    access.m_requests.queuedFrame = -1;
    access.m_requests.queuedPosition = -1;
    access.m_requests.queuedResolvedFrame = {};
    access.m_requests.queuedFrameFromPlayback = false;
    access.m_requests.queuedFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    if (!access.m_session.sessionActive) {
        return effect;
    }
    effect.sessionClose.metadataToken = access.m_requests.activeMetadataToken;
    effect.sessionClose.frameToken = access.m_requests.activeFrameToken;
    access.m_session.sessionActive = false;
    access.m_requests.activeMetadataToken = {};
    access.m_requests.activeFrameToken = {};
    access.m_requests.nextRequestToken = 0;
    return effect;
}

bool acceptsViewportEngineProviderSessionEvent(ViewportEngineProviderSessionAdmissionInput input,
    ViewportEngineProviderSessionAdmissionAccess access)
{
    return input.generation != 0 && input.generation == access.m_currentGeneration
        && access.m_session.sessionActive
        && access.m_session.sessionSerial == input.sessionSerial;
}
