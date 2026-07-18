// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineprovidersessionoperations_p.h"

ViewportEngineProviderSessionOpenEffect beginViewportEngineProviderSession(
    ViewportEngineProviderSessionOpenInput input, ViewportEngineProviderSessionOpenAccess& access)
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
    ViewportEngineProviderSessionCloseAccess& access)
{
    ViewportProviderFrameTransportEffect effect;
    effect.closeSession = access.m_session.sessionActive;
    access.m_requests.clearQueue();
    if (!access.m_session.sessionActive) {
        return effect;
    }
    effect.sessionClose.metadataToken = access.m_requests.metadataToken();
    effect.sessionClose.frameToken = access.m_requests.frameToken();
    access.m_session.sessionActive = false;
    access.m_requests.resetSession();
    return effect;
}

bool acceptsViewportEngineProviderSessionEvent(ViewportEngineProviderSessionAdmissionInput input,
    ViewportEngineProviderSessionAdmissionAccess access)
{
    return input.generation != 0 && input.generation == access.m_currentGeneration
        && access.m_session.sessionActive && access.m_session.sessionSerial == input.sessionSerial;
}
