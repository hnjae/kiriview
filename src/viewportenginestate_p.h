#pragma once

#include "imageviewportstate_p.h"
#include "imageviewporttoken_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportrendercontract_p.h"

#include <array>
#include <optional>

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
    struct AttemptContext
    {
        ViewportRenderAttempt attempt;
        bool pendingTargetCommit = false;
        bool pendingRefinementCommit = false;
        bool pendingPrimaryRefinementCommit = false;
        bool pendingSecondaryRefinementCommit = false;
        ImageViewportInternal::PreparedPayload preparedPayload;
        ImageViewportDisplayStatus oldDisplayStatus = ImageViewportDisplayStatus::Empty;
        QRectF oldContentRect;
        QRectF oldVisibleImageRect;
        PresentationGeometry::State geometryState;
    };

    quint64 nextSynchronizationAttempt = 0;
    std::optional<AttemptContext> activeAttempt;
};

struct ViewportEngineRevisionState
{
    quint64 nextRevision = 0;
    quint64 presentationRevision = 0;
    quint64 targetPresentationRevision = 0;
    quint64 snapshotRevision = 0;
};

struct ViewportEngineCommandState
{
    ImageViewportCommandReason reason = ImageViewportCommandReason::NoCommand;
    RevisionToken revision;
    quint64 publishedRevision = 0;
};

struct ViewportEngineCanonicalState
{
    ViewportEngineViewportState viewport;
    ViewportEngineRequestState requestState;
    ViewportEngineDisplayState displayState;
    ViewportEngineProviderRoleState providerState;
    ViewportEnginePlaybackState playbackState;
    ViewportEnginePresentationState presentationState;
    ViewportEngineRenderCoordinationState renderCoordination;
    ViewportEngineCommandState commandState;
    ViewportEngineRevisionState revisions;
};
