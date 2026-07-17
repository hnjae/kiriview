#pragma once

#include "viewportengineproviderterminaloperations_p.h"
#include "viewportprovidercontract_p.h"

struct ViewportEngineProviderTerminalEventInput
{
    enum class Kind {
        Failure,
        Unsupported,
        Cancellation,
    };

    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
    Kind kind = Kind::Failure;
    ImageSequenceProviderUnsupportedCause unsupportedCause
        = ImageSequenceProviderUnsupportedCause::PayloadRejection;
    ImageViewportInternal::PublicDiagnosticText diagnostic;
};

struct ViewportEngineProviderProtocolViolationInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
};

struct ViewportEngineProviderTerminalEventReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
};

struct ViewportEngineProviderDispatchFailureInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
    ImageViewportInternal::PublicDiagnosticText diagnostic;
};

struct ViewportEngineProviderSessionOpenFailureInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportInternal::PublicDiagnosticText diagnostic;
};

struct ViewportEngineProviderSessionOpenFailureReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
};

struct ViewportEngineProviderQueueFailureInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportInternal::PublicDiagnosticText diagnostic;
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
        ImageViewportInternal::ProviderRequestLedger& requests)
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
    ImageViewportInternal::ViewportChangeSet recordDisplayRequestTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    const ImageViewportInternal::ProviderFactsState& m_facts;
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestLedger& m_requests;
};

class ViewportEngineProviderProtocolViolationAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineProviderEndOfSequenceAccess;
    friend ViewportEngineProviderTerminalEventReduction
        reduceViewportEngineProviderProtocolViolation(ViewportEngineProviderProtocolViolationInput,
            ViewportEngineProviderProtocolViolationAccess);

    ViewportEngineProviderProtocolViolationAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::ProviderSessionState& session,
        ImageViewportInternal::ProviderRequestLedger& requests)
        : m_request(request)
        , m_playback(playback)
        , m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderProtocolViolationAccess(
        const ViewportEngineProviderProtocolViolationAccess&)
        = delete;
    ViewportEngineProviderProtocolViolationAccess(
        ViewportEngineProviderProtocolViolationAccess&&) noexcept
        = default;

private:
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestLedger& m_requests;
};

class ViewportEngineProviderDispatchFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
        ViewportEngineProviderDispatchFailureInput, ViewportEngineProviderDispatchFailureAccess);

    ViewportEngineProviderDispatchFailureAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::ProviderSessionState& session,
        ImageViewportInternal::ProviderRequestLedger& requests)
        : m_request(request)
        , m_playback(playback)
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
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestLedger& m_requests;
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
        ImageViewportInternal::ProviderRequestLedger& requests)
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
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestLedger& m_requests;
};

class ViewportEngineProviderQueueFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
        ViewportEngineProviderQueueFailureInput, ViewportEngineProviderQueueFailureAccess);

    ViewportEngineProviderQueueFailureAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::ProviderRequestLedger& requests)
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
    ImageViewportInternal::ViewportChangeSet recordDisplayRequestTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::ProviderRequestLedger& m_requests;
};

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderTerminalEvent(
    ViewportEngineProviderTerminalEventInput, ViewportEngineProviderTerminalEventAccess);
ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderProtocolViolation(
    ViewportEngineProviderProtocolViolationInput, ViewportEngineProviderProtocolViolationAccess);
ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
    ViewportEngineProviderDispatchFailureInput, ViewportEngineProviderDispatchFailureAccess);
ViewportEngineProviderSessionOpenFailureReduction reduceViewportEngineProviderSessionOpenFailure(
    ViewportEngineProviderSessionOpenFailureInput, ViewportEngineProviderSessionOpenFailureAccess);
ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
    ViewportEngineProviderQueueFailureInput, ViewportEngineProviderQueueFailureAccess);
