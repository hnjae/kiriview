/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportenginestate_p.h"
#include "viewportprovidercontract_p.h"

struct ViewportEngineProviderSessionOpenInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    quint64 generation = 0;
};

struct ViewportEngineProviderSessionMutation
{
    ImageViewportInternal::ProviderSessionState session;
    ImageViewportInternal::ProviderRequestLedger requests;
};

class ViewportEngineProviderSessionOpenAccess
{
    friend class ViewportEngine;
    friend class ViewportEnginePresentationTargetAssignmentAccess;
    friend ViewportEngineProviderSessionOpenEffect beginViewportEngineProviderSession(
        ViewportEngineProviderSessionOpenInput, ViewportEngineProviderSessionOpenAccess&);

    ViewportEngineProviderSessionOpenAccess(
        const ImageViewportInternal::ImageSequenceSource& source,
        ImageViewportInternal::ProviderSessionState session)
        : m_source(source)
        , m_session(session)
    {
    }

public:
    ViewportEngineProviderSessionOpenAccess(const ViewportEngineProviderSessionOpenAccess&)
        = delete;
    ImageViewportInternal::ProviderSessionState takeSession() { return std::move(m_session); }
    ViewportEngineProviderSessionOpenAccess(ViewportEngineProviderSessionOpenAccess&&) noexcept
        = default;
    ViewportEngineProviderSessionOpenAccess& operator=(
        const ViewportEngineProviderSessionOpenAccess&)
        = delete;

private:
    const ImageViewportInternal::ImageSequenceSource& m_source;
    ImageViewportInternal::ProviderSessionState m_session;
};

class ViewportEngineProviderSessionCloseAccess
{
    friend class ViewportEngine;
    friend class ViewportEnginePresentationTargetAssignmentAccess;
    friend class ViewportEngineProviderMetadataReadyAccess;
    friend class ViewportEngineProviderTerminalEventAccess;
    friend class ViewportEngineProviderProtocolViolationAccess;
    friend class ViewportEngineProviderDispatchFailureAccess;
    friend class ViewportProviderRequestTokenAllocationAccess;
    friend ViewportProviderFrameTransportEffect closeViewportEngineProviderSession(
        ViewportEngineProviderSessionCloseAccess&);

    ViewportEngineProviderSessionCloseAccess(ImageViewportInternal::ProviderSessionState session,
        const ImageViewportInternal::ProviderRequestLedger& requests)
        : m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderSessionCloseAccess(const ViewportEngineProviderSessionCloseAccess&)
        = delete;
    ViewportEngineProviderSessionMutation takeMutation()
    {
        return { std::move(m_session), std::move(m_requests) };
    }
    ViewportEngineProviderSessionCloseAccess(ViewportEngineProviderSessionCloseAccess&&) noexcept
        = default;
    ViewportEngineProviderSessionCloseAccess& operator=(
        const ViewportEngineProviderSessionCloseAccess&)
        = delete;

private:
    ImageViewportInternal::ProviderSessionState m_session;
    ImageViewportInternal::ProviderRequestLedger m_requests;
};

struct ViewportEngineProviderSessionAdmissionInput
{
    quint64 generation = 0;
    quint64 sessionSerial = 0;
};

class ViewportEngineProviderSessionAdmissionAccess
{
    friend class ViewportEngine;
    friend bool acceptsViewportEngineProviderSessionEvent(
        ViewportEngineProviderSessionAdmissionInput, ViewportEngineProviderSessionAdmissionAccess);

    ViewportEngineProviderSessionAdmissionAccess(
        quint64 currentGeneration, const ImageViewportInternal::ProviderSessionState& session)
        : m_currentGeneration(currentGeneration)
        , m_session(session)
    {
    }

public:
    ViewportEngineProviderSessionAdmissionAccess(
        const ViewportEngineProviderSessionAdmissionAccess&)
        = delete;
    ViewportEngineProviderSessionAdmissionAccess(
        ViewportEngineProviderSessionAdmissionAccess&&) noexcept
        = default;
    ViewportEngineProviderSessionAdmissionAccess& operator=(
        const ViewportEngineProviderSessionAdmissionAccess&)
        = delete;

private:
    quint64 m_currentGeneration = 0;
    const ImageViewportInternal::ProviderSessionState& m_session;
};

ViewportEngineProviderSessionOpenEffect beginViewportEngineProviderSession(
    ViewportEngineProviderSessionOpenInput, ViewportEngineProviderSessionOpenAccess&);
ViewportProviderFrameTransportEffect closeViewportEngineProviderSession(
    ViewportEngineProviderSessionCloseAccess&);
bool acceptsViewportEngineProviderSessionEvent(
    ViewportEngineProviderSessionAdmissionInput, ViewportEngineProviderSessionAdmissionAccess);
