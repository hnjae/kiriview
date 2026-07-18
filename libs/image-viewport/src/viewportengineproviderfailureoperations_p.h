/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

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
};

struct ViewportEngineProviderProtocolViolationInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
    ImageViewportInternal::InternalObservationCause cause
        = ImageViewportInternal::InternalObservationCause::None;
    ImageSequenceProviderEventKind eventKind = ImageSequenceProviderEventKind::Waiting;
};

struct ViewportEngineProviderTerminalEventReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ImageViewportInternal::InternalObservationBatch observations;
};

struct ViewportEngineProviderDispatchFailureInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
};

struct ViewportEngineProviderSessionOpenFailureInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
};

struct ViewportEngineProviderSessionOpenFailureReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
};

struct ViewportEngineProviderQueueFailureInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
};

struct ViewportEngineProviderQueueFailureReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::ProviderSchedulerDiagnostic diagnostic;
};

struct ViewportEngineProviderFailureMutation
{
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::PlaybackState playback;
    ImageViewportInternal::ProviderSessionState session;
    ImageViewportInternal::ProviderRequestLedger requests;
};

#define VIEWPORT_PROVIDER_FAILURE_TAKE_MUTATION                                                    \
    ViewportEngineProviderFailureMutation takeMutation()                                           \
    {                                                                                              \
        return { std::move(m_request), std::move(m_playback), std::move(m_session),                \
            std::move(m_requests) };                                                               \
    }

class ViewportEngineProviderTerminalEventAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderTerminalEvent(
        ViewportEngineProviderTerminalEventInput, ViewportEngineProviderTerminalEventAccess&);

    ViewportEngineProviderTerminalEventAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::ProviderFactsState& facts,
        const ImageViewportInternal::ProviderSessionState& session,
        const ImageViewportInternal::ProviderRequestLedger& requests)
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
    VIEWPORT_PROVIDER_FAILURE_TAKE_MUTATION;

private:
    ImageViewportInternal::ViewportChangeSet recordDisplayRequestTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    const ImageViewportInternal::ProviderFactsState& m_facts;
    ImageViewportInternal::ProviderSessionState m_session;
    ImageViewportInternal::ProviderRequestLedger m_requests;
};

class ViewportEngineProviderProtocolViolationAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineProviderEndOfSequenceAccess;
    friend ViewportEngineProviderTerminalEventReduction
    reduceViewportEngineProviderProtocolViolation(ViewportEngineProviderProtocolViolationInput,
        ViewportEngineProviderProtocolViolationAccess&);

    ViewportEngineProviderProtocolViolationAccess(
        const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::ProviderSessionState& session,
        const ImageViewportInternal::ProviderRequestLedger& requests)
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
    VIEWPORT_PROVIDER_FAILURE_TAKE_MUTATION;

private:
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::ProviderSessionState m_session;
    ImageViewportInternal::ProviderRequestLedger m_requests;
};

class ViewportEngineProviderDispatchFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
        ViewportEngineProviderDispatchFailureInput, ViewportEngineProviderDispatchFailureAccess&);

    ViewportEngineProviderDispatchFailureAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::ProviderSessionState& session,
        const ImageViewportInternal::ProviderRequestLedger& requests)
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
    VIEWPORT_PROVIDER_FAILURE_TAKE_MUTATION;

private:
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ViewportProviderFrameTransportEffect closeSession();
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::ProviderSessionState m_session;
    ImageViewportInternal::ProviderRequestLedger m_requests;
};

class ViewportEngineProviderSessionOpenFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderSessionOpenFailureReduction
    reduceViewportEngineProviderSessionOpenFailure(ViewportEngineProviderSessionOpenFailureInput,
        ViewportEngineProviderSessionOpenFailureAccess&);

    ViewportEngineProviderSessionOpenFailureAccess(
        const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::ProviderSessionState& session,
        const ImageViewportInternal::ProviderRequestLedger& requests)
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
    VIEWPORT_PROVIDER_FAILURE_TAKE_MUTATION;

private:
    ImageViewportInternal::ViewportChangeSet recordGenerationTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::ProviderSessionState m_session;
    ImageViewportInternal::ProviderRequestLedger m_requests;
};

class ViewportEngineProviderQueueFailureAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
        ViewportEngineProviderQueueFailureInput, ViewportEngineProviderQueueFailureAccess&);

    ViewportEngineProviderQueueFailureAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::ProviderRequestLedger& requests)
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
    ViewportEngineProviderFailureMutation takeMutation()
    {
        return { std::move(m_request), std::move(m_playback), {}, std::move(m_requests) };
    }

private:
    ImageViewportInternal::ViewportChangeSet recordDisplayRequestTerminal(
        ViewportEngineProviderTerminalProjectionInput input);
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::ProviderRequestLedger m_requests;
};

ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderTerminalEvent(
    ViewportEngineProviderTerminalEventInput, ViewportEngineProviderTerminalEventAccess&);
ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderProtocolViolation(
    ViewportEngineProviderProtocolViolationInput, ViewportEngineProviderProtocolViolationAccess&);
ViewportEngineProviderTerminalEventReduction reduceViewportEngineProviderDispatchFailure(
    ViewportEngineProviderDispatchFailureInput, ViewportEngineProviderDispatchFailureAccess&);
ViewportEngineProviderSessionOpenFailureReduction reduceViewportEngineProviderSessionOpenFailure(
    ViewportEngineProviderSessionOpenFailureInput, ViewportEngineProviderSessionOpenFailureAccess&);
ViewportEngineProviderQueueFailureReduction reduceViewportEngineProviderQueueFailure(
    ViewportEngineProviderQueueFailureInput, ViewportEngineProviderQueueFailureAccess&);

#undef VIEWPORT_PROVIDER_FAILURE_TAKE_MUTATION
