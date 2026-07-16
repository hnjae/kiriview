#pragma once

#include "viewportengineproviderrequestoperations_p.h"
#include "viewportengineproviderterminaloperations_p.h"

struct ViewportEngineProviderMetadataReadyInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
    ImageSequenceProviderMetadata metadata;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderMetadataReadyReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ImageViewportInternal::InternalObservationBatch observations;
};

class ViewportEngineProviderMetadataReadyAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderMetadataReadyReduction reduceViewportEngineProviderMetadataReady(
        ViewportEngineProviderMetadataReadyInput, ViewportEngineProviderMetadataReadyAccess);

    ViewportEngineProviderMetadataReadyAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display, std::array<ViewportEngineRoleState, 2>& roles,
        ImageViewportInternal::PresentationState& presentation,
        ViewportEnginePresentationTargetState& presentationTarget, quint64& nextRevision,
        quint64& targetPresentationRevision)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
        , m_presentation(presentation)
        , m_presentationTarget(presentationTarget)
        , m_nextRevision(nextRevision)
        , m_targetPresentationRevision(targetPresentationRevision)
        , m_presentationTargetGeneration(presentationTarget.generation)
    {
    }

public:
    ViewportEngineProviderMetadataReadyAccess(const ViewportEngineProviderMetadataReadyAccess&)
        = delete;
    ViewportEngineProviderMetadataReadyAccess(ViewportEngineProviderMetadataReadyAccess&&) noexcept
        = default;

private:
    ViewportProviderFrameRequestStartResult startFrameRequest(ImageViewportPageRole role,
        ImageViewportInternal::DisplayRequestTarget target,
        const ViewportEngineGeometryInput& geometry);
    ViewportProviderFrameTransportEffect closeSession(ImageViewportPageRole role);
    ImageViewportInternal::ViewportChangeSet recordTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    void advanceTargetPresentationRevision();
    bool applyAutoplay();

    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::DisplayState& m_display;
    std::array<ViewportEngineRoleState, 2>& m_roles;
    ImageViewportInternal::PresentationState& m_presentation;
    ViewportEnginePresentationTargetState& m_presentationTarget;
    quint64& m_nextRevision;
    quint64& m_targetPresentationRevision;
    quint64 m_presentationTargetGeneration = 0;
};

ViewportEngineProviderMetadataReadyReduction reduceViewportEngineProviderMetadataReady(
    ViewportEngineProviderMetadataReadyInput, ViewportEngineProviderMetadataReadyAccess);
