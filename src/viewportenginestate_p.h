#pragma once

#include "viewportengine_p.h"

struct ViewportEngineRoleState
{
    ImageViewportInternal::ProviderGenerationState provider;
};

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

struct ViewportEngineProviderStateAccess
{
    ImageViewportInternal::RequestState& request;
    ImageViewportInternal::DisplayState& display;
    std::array<ViewportEngineRoleState, 2>& roles;
};

struct ViewportEnginePlaybackStateAccess
{
    ImageViewportInternal::RequestState& request;
    ImageViewportInternal::DisplayState& display;
    std::array<ViewportEngineRoleState, 2>& roles;
};

struct ViewportEngineRenderStateAccess
{
    ImageViewportInternal::RequestState& request;
    ImageViewportInternal::DisplayState& display;
    std::array<ViewportEngineRoleState, 2>& roles;
    quint64& nextSynchronizationAttempt;
    ViewportRenderSynchronization& lastSynchronization;
};

struct ViewportEnginePresentationStateAccess
{
    ImageViewportInternal::RequestState& request;
    const ImageViewportInternal::DisplayState& display;
    ImageViewportInternal::PresentationState& presentation;
};

struct ViewportEngineSnapshotStateAccess
{
    const ImageViewportInternal::RequestState& request;
    const ImageViewportInternal::DisplayState& display;
    const std::array<ViewportEngineRoleState, 2>& roles;
    const ImageViewportInternal::PresentationState& presentation;
    const ViewportEngine::PresentationTargetState& presentationTarget;
    const RevisionToken& commandRevision;
    quint64 presentationRevision = 0;
    quint64 snapshotRevision = 0;
};
