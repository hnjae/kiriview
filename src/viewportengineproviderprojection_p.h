#pragma once

#include "viewportengineprojection_p.h"

struct ViewportEngineProviderDemandInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportEngineGeometryInput geometry;
    ImageViewportDemandRevisionToken demandRevision;
    ImageViewportRevisionToken requestRevision;
    ImageViewportRevisionToken presentationRevision;
    ImageViewportPresentationTargetGenerationToken allocationGeneration;
};

class ViewportEngineProviderDemandProjectionAccess
{
    friend class ViewportEngine;
    friend class ViewportEnginePlaybackStopAccess;
    friend class ViewportEnginePlaybackSeekAccess;
    friend class ViewportEnginePlaybackPlayAccess;
    friend class ViewportEnginePlaybackTickAccess;
    friend class ViewportEngineProviderSessionOpenedAccess;
    friend class ViewportEngineProviderQueueFlushAccess;
    friend class ViewportEngineProviderDemandRestageAccess;
    friend class ViewportEngineProviderFrameRequestAccess;
    ViewportEngineProviderDemandProjectionAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        ViewportEngineProviderFactsView providerFacts,
        const ImageViewportInternal::PresentationState& presentation)
        : m_request(request)
        , m_display(display)
        , m_providerFacts(providerFacts)
        , m_presentation(presentation)
    {
    }

public:
    ViewportEngineProviderDemandProjectionAccess(
        const ViewportEngineProviderDemandProjectionAccess&)
        = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    const ViewportEngineProviderFactsView& providerFacts() const { return m_providerFacts; }
    const ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    ViewportEngineProviderFactsView m_providerFacts;
    const ImageViewportInternal::PresentationState& m_presentation;
};

ImageSequenceProviderDisplayDemand projectViewportProviderDemand(
    ViewportEngineProviderDemandInput, ViewportEngineProviderDemandProjectionAccess);
