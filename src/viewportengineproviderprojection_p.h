#pragma once

#include "viewportengineprojection_p.h"

struct ViewportEngineProviderDemandInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ViewportEngineGeometryInput geometry;
    ImageViewportDemandRevisionToken demandRevision;
    ImageViewportRevisionToken requestRevision;
    ImageViewportRevisionToken presentationRevision;
    ImageViewportPresentationTargetGenerationToken allocationGeneration;
};

class ViewportEngineProviderDemandProjectionAccess
{
    friend class ViewportEngine;
    ViewportEngineProviderDemandProjectionAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation)
        : m_request(request), m_display(display), m_roles(roles), m_presentation(presentation) { }
public:
    ViewportEngineProviderDemandProjectionAccess(const ViewportEngineProviderDemandProjectionAccess&) = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    const std::array<ViewportEngineRoleState, 2>& roles() const { return m_roles; }
    const ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }
private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    const std::array<ViewportEngineRoleState, 2>& m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
};

ImageSequenceProviderDisplayDemand projectViewportProviderDemand(
    ViewportEngineProviderDemandInput, ViewportEngineProviderDemandProjectionAccess);
