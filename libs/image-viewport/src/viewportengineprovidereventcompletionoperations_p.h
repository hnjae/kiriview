/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportengineproviderfailureoperations_p.h"
#include "viewportengineproviderrequestoperations_p.h"

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
    ImageViewportInternal::InternalObservationBatch observations;
};
struct ViewportEngineProviderWaitingMutation
{
    ImageViewportInternal::RequestState request;
};
using ViewportEngineProviderEndOfSequenceMutation = ViewportEngineProviderRequestMutation;

class ViewportEngineProviderWaitingAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEngineProviderWaitingReduction reduceViewportEngineProviderWaiting(
        ViewportEngineProviderWaitingInput, ViewportEngineProviderWaitingAccess&);
    ViewportEngineProviderWaitingAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::ProviderFactsState& facts,
        const ImageViewportInternal::ProviderSessionState& session,
        const ImageViewportInternal::ProviderRequestLedger& requests)
        : m_request(request)
        , m_facts(facts)
        , m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderWaitingAccess(const ViewportEngineProviderWaitingAccess&) = delete;
    ViewportEngineProviderWaitingAccess(ViewportEngineProviderWaitingAccess&&) noexcept = default;
    ViewportEngineProviderWaitingMutation takeMutation() { return { std::move(m_request) }; }

private:
    ImageViewportInternal::RequestState m_request;
    const ImageViewportInternal::ProviderFactsState& m_facts;
    const ImageViewportInternal::ProviderSessionState& m_session;
    const ImageViewportInternal::ProviderRequestLedger& m_requests;
};

class
    ViewportEngineProviderEndOfSequenceAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEngineProviderEndOfSequenceReduction reduceViewportEngineProviderEndOfSequence(
        ViewportEngineProviderEndOfSequenceInput, ViewportEngineProviderEndOfSequenceAccess&);
    ViewportEngineProviderEndOfSequenceAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64 nextRevision,
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
    ViewportEngineProviderEndOfSequenceMutation takeMutation()
    {
        return { std::move(m_request), m_playback, std::move(m_display), std::move(m_roles),
            m_nextRevision };
    }

private:
    ViewportProviderFrameRequestStartResult startFrame(ImageViewportPageRole,
        ImageViewportInternal::DisplayRequestTarget, const ViewportEngineGeometryInput&);
    ViewportEngineProviderTerminalEventReduction protocolViolation(
        ImageViewportPageRole, ImageSequenceProviderRequestToken);
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::DisplayState m_display;
    std::array<ViewportEngineRoleState, 2> m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    quint64 m_nextRevision;
    quint64 m_presentationRevision;
    quint64 m_targetGeneration;
};

ViewportEngineProviderWaitingReduction reduceViewportEngineProviderWaiting(
    ViewportEngineProviderWaitingInput, ViewportEngineProviderWaitingAccess&);
ViewportEngineProviderEndOfSequenceReduction reduceViewportEngineProviderEndOfSequence(
    ViewportEngineProviderEndOfSequenceInput, ViewportEngineProviderEndOfSequenceAccess&);
