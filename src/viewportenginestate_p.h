#pragma once

#include "viewportenginecapabilities_p.h"

struct ViewportEngineCanonicalState
{
    quint64 nextRevision = 0;
    quint64 nextPresentationTargetGeneration = 0;
    quint64 nextRenderSynchronizationAttempt = 0;
    quint64 presentationRevision = 0;
    quint64 snapshotRevision = 0;
    ViewportRenderSynchronization lastRenderSynchronization;
    RevisionToken commandRevision;
    ViewportEngine::PresentationTargetState presentationTarget;
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::RequestState request;
    std::array<ViewportEngineRoleState, 2> roles;
    ImageViewportInternal::PresentationState presentation;
};
