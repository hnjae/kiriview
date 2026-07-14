#pragma once

#include "viewportengineproviderrequestoperations_p.h"
#include "viewportengineproviderterminaloperations_p.h"

struct ViewportEngineProviderWaitingInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
    bool progress = false;
    double progressValue = 0.0;
};
struct ViewportEngineProviderWaitingReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
};
struct ViewportEngineProviderEndOfSequenceInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
    ViewportEngineGeometryInput geometry;
};
struct ViewportEngineProviderEndOfSequenceReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

class ViewportEngineProviderWaitingAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderWaitingReduction reduceViewportEngineProviderWaiting(
        ViewportEngineProviderWaitingInput, ViewportEngineProviderWaitingAccess);
    ViewportEngineProviderWaitingAccess(ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::ProviderFactsState& facts,
        const ImageViewportInternal::ProviderSessionState& session,
        const ImageViewportInternal::ProviderRequestState& requests)
        : m_request(request)
        , m_facts(facts)
        , m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderWaitingAccess(const ViewportEngineProviderWaitingAccess&) = delete;
    ViewportEngineProviderWaitingAccess(ViewportEngineProviderWaitingAccess&&) noexcept = default;

private:
    ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::ProviderFactsState& m_facts;
    const ImageViewportInternal::ProviderSessionState& m_session;
    const ImageViewportInternal::ProviderRequestState& m_requests;
};

class ViewportEngineProviderEndOfSequenceAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderEndOfSequenceReduction reduceViewportEngineProviderEndOfSequence(
        ViewportEngineProviderEndOfSequenceInput, ViewportEngineProviderEndOfSequenceAccess);
    ViewportEngineProviderEndOfSequenceAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display, std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
        quint64 presentationRevision, quint64 targetGeneration)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
        , m_presentation(presentation)
        , m_nextRevision(nextRevision)
        , m_presentationRevision(presentationRevision)
        , m_targetGeneration(targetGeneration)
    {
    }

public:
    ViewportEngineProviderEndOfSequenceAccess(const ViewportEngineProviderEndOfSequenceAccess&)
        = delete;
    ViewportEngineProviderEndOfSequenceAccess(ViewportEngineProviderEndOfSequenceAccess&&) noexcept
        = default;

private:
    ViewportProviderFrameRequestStartResult startFrame(ImageViewportPageRole,
        ImageViewportInternal::DisplayRequestTarget, const ViewportEngineGeometryInput&);
    ViewportProviderFrameTransportEffect closeSession(ImageViewportPageRole);
    ImageViewportInternal::ViewportChangeSet recordTerminal(
        ViewportEngineProviderTerminalProjectionInput);
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::DisplayState& m_display;
    std::array<ViewportEngineRoleState, 2>& m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    quint64& m_nextRevision;
    quint64 m_presentationRevision;
    quint64 m_targetGeneration;
};

ViewportEngineProviderWaitingReduction reduceViewportEngineProviderWaiting(
    ViewportEngineProviderWaitingInput, ViewportEngineProviderWaitingAccess);
ViewportEngineProviderEndOfSequenceReduction reduceViewportEngineProviderEndOfSequence(
    ViewportEngineProviderEndOfSequenceInput, ViewportEngineProviderEndOfSequenceAccess);
