#pragma once

#include "imageviewportstate_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportrendercontract_p.h"

#include <array>

struct ViewportEngineRoleState
{
    ImageViewportInternal::ProviderGenerationState provider;
};

struct ViewportEngineRequestState
{
    quint64 nextPresentationTargetGeneration = 0;
    ViewportEnginePresentationTargetState presentationTarget;
    ImageViewportInternal::RequestState request;
};

struct ViewportEngineDisplayState
{
    ImageViewportInternal::DisplayState display;
};

struct ViewportEngineProviderRoleState
{
    std::array<ViewportEngineRoleState, 2> roles;
};

struct ViewportEnginePlaybackState
{
    ImageViewportInternal::PlaybackState playback;
};

struct ViewportEnginePresentationState
{
    ImageViewportInternal::PresentationState presentation;
};

struct ViewportEngineRenderCoordinationState
{
    quint64 nextSynchronizationAttempt = 0;
    ViewportRenderSynchronization lastSynchronization;
};

struct ViewportEngineRevisionState
{
    quint64 nextRevision = 0;
    quint64 presentationRevision = 0;
    quint64 snapshotRevision = 0;
};

struct ViewportEngineCommandState
{
    ImageViewport::CommandReason reason = ImageViewport::CommandReason::NoCommand;
    RevisionToken revision;
    quint64 publishedRevision = 0;
};

struct ViewportEngineCanonicalState
{
    ViewportEngineRequestState requestState;
    ViewportEngineDisplayState displayState;
    ViewportEngineProviderRoleState providerState;
    ViewportEnginePlaybackState playbackState;
    ViewportEnginePresentationState presentationState;
    ViewportEngineRenderCoordinationState renderCoordination;
    ViewportEngineCommandState commandState;
    ViewportEngineRevisionState revisions;
};
