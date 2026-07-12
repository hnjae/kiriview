#pragma once

#include "viewportcontrollerprovidercontract_p.h"
#include "viewportengineproviderterminaloperations_p.h"

struct ViewportEngineProviderTerminalEventInput
{
    enum class Kind {
        Failure,
        Unsupported,
        Cancellation,
    };

    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ImageSequenceProviderRequestToken token;
    Kind kind = Kind::Failure;
    ImageSequenceProviderSession::UnsupportedCause unsupportedCause
        = ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
    QString diagnostic;
    bool unsupportedCauseExplicit = false;
};

struct ViewportEngineProviderTerminalEventReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportEngineProviderDispatchFailureInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ImageSequenceProviderRequestToken token;
    QString diagnostic;
};

struct ViewportEngineProviderSessionOpenFailureInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    QString diagnostic;
};

struct ViewportEngineProviderSessionOpenFailureReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
};

struct ViewportEngineProviderQueueFailureInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    QString diagnostic;
};

struct ViewportEngineProviderQueueFailureReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::ProviderSchedulerDiagnostic diagnostic;
};

class ViewportEngineProviderTerminalEventAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderTerminalEvent(
        ViewportEngineProviderTerminalEventInput, ViewportEngineProviderTerminalEventAccess);

    ViewportEngineProviderTerminalEventAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::ProviderFactsState& facts,
        ImageViewportInternal::ProviderSessionState& session,
        ImageViewportInternal::ProviderRequestState& requests)
        : m_request(request)
        , m_playback(playback)
        , m_facts(facts)
        , m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderTerminalEventAccess(const ViewportEngineProviderTerminalEventAccess&)
        = delete;
    ViewportEngineProviderTerminalEventAccess(ViewportEngineProviderTerminalEventAccess&&) noexcept
        = default;

private:
    ImageViewportInternal::ViewportChangeSet recordTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    const ImageViewportInternal::ProviderFactsState& m_facts;
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestState& m_requests;
};

class ViewportEngineProviderDispatchFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
        ViewportEngineProviderDispatchFailureInput, ViewportEngineProviderDispatchFailureAccess);

    ViewportEngineProviderDispatchFailureAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::ProviderFactsState& facts,
        ImageViewportInternal::ProviderSessionState& session,
        ImageViewportInternal::ProviderRequestState& requests)
        : m_request(request)
        , m_playback(playback)
        , m_facts(facts)
        , m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderDispatchFailureAccess(const ViewportEngineProviderDispatchFailureAccess&)
        = delete;
    ViewportEngineProviderDispatchFailureAccess(
        ViewportEngineProviderDispatchFailureAccess&&) noexcept
        = default;

private:
    ImageViewportInternal::ViewportChangeSet recordTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    const ImageViewportInternal::ProviderFactsState& m_facts;
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestState& m_requests;
};

class ViewportEngineProviderSessionOpenFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderSessionOpenFailureReduction
        reduceViewportEngineProviderSessionOpenFailure(
            ViewportEngineProviderSessionOpenFailureInput,
            ViewportEngineProviderSessionOpenFailureAccess);

    ViewportEngineProviderSessionOpenFailureAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::ProviderSessionState& session,
        ImageViewportInternal::ProviderRequestState& requests)
        : m_request(request)
        , m_playback(playback)
        , m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderSessionOpenFailureAccess(
        const ViewportEngineProviderSessionOpenFailureAccess&)
        = delete;
    ViewportEngineProviderSessionOpenFailureAccess(
        ViewportEngineProviderSessionOpenFailureAccess&&) noexcept
        = default;

private:
    ImageViewportInternal::ViewportChangeSet recordTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestState& m_requests;
};

class ViewportEngineProviderQueueFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
        ViewportEngineProviderQueueFailureInput, ViewportEngineProviderQueueFailureAccess);

    ViewportEngineProviderQueueFailureAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::ProviderRequestState& requests)
        : m_request(request)
        , m_playback(playback)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderQueueFailureAccess(const ViewportEngineProviderQueueFailureAccess&)
        = delete;
    ViewportEngineProviderQueueFailureAccess(ViewportEngineProviderQueueFailureAccess&&) noexcept
        = default;

private:
    ImageViewportInternal::ViewportChangeSet recordTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::ProviderRequestState& m_requests;
};

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderTerminalEvent(
    ViewportEngineProviderTerminalEventInput, ViewportEngineProviderTerminalEventAccess);
ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
    ViewportEngineProviderDispatchFailureInput, ViewportEngineProviderDispatchFailureAccess);
ViewportEngineProviderSessionOpenFailureReduction reduceViewportEngineProviderSessionOpenFailure(
    ViewportEngineProviderSessionOpenFailureInput, ViewportEngineProviderSessionOpenFailureAccess);
ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
    ViewportEngineProviderQueueFailureInput, ViewportEngineProviderQueueFailureAccess);
