#pragma once

#include "viewportcontrollerprovidercontract_p.h"
#include "viewportenginestate_p.h"

struct ViewportEngineProviderSessionOpenInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    quint64 generation = 0;
};

struct ViewportEngineProviderSessionOpenEffect
{
    bool openSession = false;
    ViewportProviderTransportCommand command;
};

class ViewportEngineProviderSessionOpenAccess
{
    friend class ViewportEngine;
    friend ViewportEngineProviderSessionOpenEffect beginViewportEngineProviderSession(
        ViewportEngineProviderSessionOpenInput, ViewportEngineProviderSessionOpenAccess);

    ViewportEngineProviderSessionOpenAccess(
        const ImageViewportInternal::ImageSequenceSource& source,
        ImageViewportInternal::ProviderSessionState& session)
        : m_source(source)
        , m_session(session)
    {
    }

public:
    ViewportEngineProviderSessionOpenAccess(const ViewportEngineProviderSessionOpenAccess&) = delete;
    ViewportEngineProviderSessionOpenAccess(ViewportEngineProviderSessionOpenAccess&&) noexcept
        = default;
    ViewportEngineProviderSessionOpenAccess& operator=(
        const ViewportEngineProviderSessionOpenAccess&) = delete;

private:
    const ImageViewportInternal::ImageSequenceSource& m_source;
    ImageViewportInternal::ProviderSessionState& m_session;
};

class ViewportEngineProviderSessionCloseAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineProviderMetadataReadyAccess;
    friend ViewportProviderFrameTransportEffect closeViewportEngineProviderSession(
        ViewportEngineProviderSessionCloseAccess);

    ViewportEngineProviderSessionCloseAccess(ImageViewportInternal::ProviderSessionState& session,
        ImageViewportInternal::ProviderRequestState& requests)
        : m_session(session)
        , m_requests(requests)
    {
    }

public:
    ViewportEngineProviderSessionCloseAccess(const ViewportEngineProviderSessionCloseAccess&)
        = delete;
    ViewportEngineProviderSessionCloseAccess(ViewportEngineProviderSessionCloseAccess&&) noexcept
        = default;
    ViewportEngineProviderSessionCloseAccess& operator=(
        const ViewportEngineProviderSessionCloseAccess&) = delete;

private:
    ImageViewportInternal::ProviderSessionState& m_session;
    ImageViewportInternal::ProviderRequestState& m_requests;
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
        ViewportEngineProviderSessionAdmissionInput,
        ViewportEngineProviderSessionAdmissionAccess);

    ViewportEngineProviderSessionAdmissionAccess(quint64 currentGeneration,
        const ImageViewportInternal::ProviderSessionState& session)
        : m_currentGeneration(currentGeneration)
        , m_session(session)
    {
    }

public:
    ViewportEngineProviderSessionAdmissionAccess(
        const ViewportEngineProviderSessionAdmissionAccess&)
        = delete;
    ViewportEngineProviderSessionAdmissionAccess(
        ViewportEngineProviderSessionAdmissionAccess&&) noexcept = default;
    ViewportEngineProviderSessionAdmissionAccess& operator=(
        const ViewportEngineProviderSessionAdmissionAccess&) = delete;

private:
    quint64 m_currentGeneration = 0;
    const ImageViewportInternal::ProviderSessionState& m_session;
};

ViewportEngineProviderSessionOpenEffect beginViewportEngineProviderSession(
    ViewportEngineProviderSessionOpenInput, ViewportEngineProviderSessionOpenAccess);
ViewportProviderFrameTransportEffect closeViewportEngineProviderSession(
    ViewportEngineProviderSessionCloseAccess);
bool acceptsViewportEngineProviderSessionEvent(ViewportEngineProviderSessionAdmissionInput,
    ViewportEngineProviderSessionAdmissionAccess);
