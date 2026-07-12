#pragma once

#include "viewportenginecapabilities_p.h"

PresentationGeometry::State projectViewportGeometryState(
    const ViewportEngineGeometryInput&, const ImageViewportInternal::PresentationState&);

struct ViewportEngineGeometryQueryInput { QRectF itemBounds; double devicePixelRatio = 1.0; };

class ViewportEngineCurrentGeometryProjectionAccess
{
    friend class ViewportEngine;
    ViewportEngineCurrentGeometryProjectionAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display) : m_request(request), m_display(display) { }
public:
    ViewportEngineCurrentGeometryProjectionAccess(const ViewportEngineCurrentGeometryProjectionAccess&) = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
};

class ViewportEnginePendingGeometryProjectionAccess
{
    friend class ViewportEngine;
    ViewportEnginePendingGeometryProjectionAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles)
        : m_request(request), m_display(display), m_roles(roles) { }
public:
    ViewportEnginePendingGeometryProjectionAccess(const ViewportEnginePendingGeometryProjectionAccess&) = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    const std::array<ViewportEngineRoleState, 2>& roles() const { return m_roles; }
private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    const std::array<ViewportEngineRoleState, 2>& m_roles;
};

class ViewportEngineAcceptedGeometryProjectionAccess
{
    friend class ViewportEngine;
    ViewportEngineAcceptedGeometryProjectionAccess(const ImageViewportInternal::RequestState& request,
        const std::array<ViewportEngineRoleState, 2>& roles) : m_request(request), m_roles(roles) { }
public:
    ViewportEngineAcceptedGeometryProjectionAccess(const ViewportEngineAcceptedGeometryProjectionAccess&) = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const std::array<ViewportEngineRoleState, 2>& roles() const { return m_roles; }
private:
    const ImageViewportInternal::RequestState& m_request;
    const std::array<ViewportEngineRoleState, 2>& m_roles;
};

class ViewportEngineRenderSnapshotProjectionAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineRenderSynchronizationAccess;
    ViewportEngineRenderSnapshotProjectionAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::PresentationState& presentation)
        : m_request(request), m_display(display), m_presentation(presentation) { }
public:
    ViewportEngineRenderSnapshotProjectionAccess(const ViewportEngineRenderSnapshotProjectionAccess&) = delete;
    ViewportEngineRenderSnapshotProjectionAccess(ViewportEngineRenderSnapshotProjectionAccess&&) = default;
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
