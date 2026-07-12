#pragma once

#include "viewportenginecapabilities_p.h"

PresentationGeometry::State projectViewportGeometryState(
    const ViewportEngineGeometryInput&, const ImageViewportInternal::PresentationState&);

struct ViewportEngineGeometryQueryInput
{
    QRectF itemBounds;
    double devicePixelRatio = 1.0;
};

class ViewportEngineCurrentGeometryProjectionAccess
{
    friend class ViewportEngine;
    ViewportEngineCurrentGeometryProjectionAccess(
        const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display)
        : m_request(request)
        , m_display(display)
    {
    }

public:
    ViewportEngineCurrentGeometryProjectionAccess(
        const ViewportEngineCurrentGeometryProjectionAccess&)
        = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
};

class ViewportEnginePendingGeometryProjectionAccess
{
    friend class ViewportEngine;
    ViewportEnginePendingGeometryProjectionAccess(
        const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        ViewportEngineProviderFactsView providerFacts)
        : m_request(request)
        , m_display(display)
        , m_providerFacts(providerFacts)
    {
    }

public:
    ViewportEnginePendingGeometryProjectionAccess(
        const ViewportEnginePendingGeometryProjectionAccess&)
        = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    const ViewportEngineProviderFactsView& providerFacts() const { return m_providerFacts; }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    ViewportEngineProviderFactsView m_providerFacts;
};

class ViewportEngineAcceptedGeometryProjectionAccess
{
    friend class ViewportEngine;
    ViewportEngineAcceptedGeometryProjectionAccess(
        const ImageViewportInternal::RequestState& request,
        ViewportEngineProviderFactsView providerFacts)
        : m_request(request)
        , m_providerFacts(providerFacts)
    {
    }

public:
    ViewportEngineAcceptedGeometryProjectionAccess(
        const ViewportEngineAcceptedGeometryProjectionAccess&)
        = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ViewportEngineProviderFactsView& providerFacts() const { return m_providerFacts; }

private:
    const ImageViewportInternal::RequestState& m_request;
    ViewportEngineProviderFactsView m_providerFacts;
};

class ViewportEngineRenderSnapshotProjectionAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineRenderSynchronizationAccess;
    ViewportEngineRenderSnapshotProjectionAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::PresentationState& presentation)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
    {
    }

public:
    ViewportEngineRenderSnapshotProjectionAccess(
        const ViewportEngineRenderSnapshotProjectionAccess&)
        = delete;
    ViewportEngineRenderSnapshotProjectionAccess(ViewportEngineRenderSnapshotProjectionAccess&&)
        = default;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    const ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    const ImageViewportInternal::PresentationState& m_presentation;
};

ViewportEngineGeometryInput projectViewportCurrentGeometry(
    ViewportEngineGeometryQueryInput, ViewportEngineCurrentGeometryProjectionAccess);
ViewportEngineGeometryInput projectViewportPendingGeometry(
    ViewportEngineGeometryQueryInput, ViewportEnginePendingGeometryProjectionAccess);
ViewportEngineGeometryInput projectViewportAcceptedGeometry(
    ViewportEngineGeometryQueryInput, ViewportEngineAcceptedGeometryProjectionAccess);
ViewportRenderSnapshot projectViewportRenderSnapshot(
    ViewportRenderSnapshotInput, ViewportEngineRenderSnapshotProjectionAccess);

ImageViewportStateSnapshot projectViewportStateSnapshot(
    ViewportEngineSnapshotInput input, ViewportEngineSnapshotStateAccess access);
