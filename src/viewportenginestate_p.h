#pragma once

#include "imageviewportstate_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportrendercontract_p.h"

#include <array>

struct ViewportEngineRoleState
{
    ImageViewportInternal::ProviderRoleState provider;
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

class ViewportEngineProviderFactsView
{
public:
    ViewportEngineProviderFactsView(const ImageViewportInternal::ProviderFactsState& primary,
        const ImageViewportInternal::ProviderFactsState& secondary)
        : m_primary(primary)
        , m_secondary(secondary)
    {
    }

    const ImageViewportInternal::ProviderFactsState& operator[](std::size_t index) const
    {
        return index == 0 ? m_primary : m_secondary;
    }

private:
    const ImageViewportInternal::ProviderFactsState& m_primary;
    const ImageViewportInternal::ProviderFactsState& m_secondary;
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
