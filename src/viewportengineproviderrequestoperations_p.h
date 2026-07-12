#pragma once

#include "viewportengineproviderprojection_p.h"
#include "viewportengineproviderrequesttokenoperations_p.h"
#include "viewportplaybackcontract_p.h"

struct ViewportEngineProviderSessionOpenedInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderQueueFlushInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderDemandRestageInput
{
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderFrameRequestInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ImageViewportInternal::DisplayRequestTarget target;
    ViewportEngineGeometryInput geometry;
};

#define VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS                                                \
    ImageViewportInternal::RequestState& m_request;                                              \
    ImageViewportInternal::PlaybackState& m_playback;                                            \
    ImageViewportInternal::DisplayState& m_display;                                              \
    std::array<ViewportEngineRoleState, 2>& m_roles;                                              \
    const ImageViewportInternal::PresentationState& m_presentation;                              \
    quint64& m_nextRevision;                                                                     \
    quint64 m_presentationRevision = 0;                                                          \
    quint64 m_presentationTargetGeneration = 0

class ViewportEngineProviderSessionOpenedAccess
{
    friend class ViewportEngine;
    friend ViewportProviderSessionOpenResult reduceViewportEngineProviderSessionOpened(
        ViewportEngineProviderSessionOpenedInput, ViewportEngineProviderSessionOpenedAccess);

    ViewportEngineProviderSessionOpenedAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request), m_playback(playback), m_display(display), m_roles(roles),
          m_presentation(presentation), m_nextRevision(nextRevision),
          m_presentationRevision(presentationRevision),
          m_presentationTargetGeneration(presentationTargetGeneration) {}
public:
    ViewportEngineProviderSessionOpenedAccess(const ViewportEngineProviderSessionOpenedAccess&) = delete;
    ViewportEngineProviderSessionOpenedAccess(ViewportEngineProviderSessionOpenedAccess&&) noexcept = default;
private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewport::PageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewport::PageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

class ViewportEngineProviderQueueFlushAccess
{
    friend class ViewportEngine;
    friend ViewportProviderFrameQueueFlushResult reduceViewportEngineProviderQueueFlush(
        ViewportEngineProviderQueueFlushInput, ViewportEngineProviderQueueFlushAccess);
    ViewportEngineProviderQueueFlushAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request), m_playback(playback), m_display(display), m_roles(roles),
          m_presentation(presentation), m_nextRevision(nextRevision),
          m_presentationRevision(presentationRevision),
          m_presentationTargetGeneration(presentationTargetGeneration) {}
public:
    ViewportEngineProviderQueueFlushAccess(const ViewportEngineProviderQueueFlushAccess&) = delete;
    ViewportEngineProviderQueueFlushAccess(ViewportEngineProviderQueueFlushAccess&&) noexcept = default;
private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewport::PageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewport::PageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

class ViewportEngineProviderDemandRestageAccess
{
    friend class ViewportEngine;
    friend std::array<ViewportProviderFrameTransportEffect, 2>
    reduceViewportEngineProviderDemandRestage(
        ViewportEngineProviderDemandRestageInput, ViewportEngineProviderDemandRestageAccess);
    ViewportEngineProviderDemandRestageAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request), m_playback(playback), m_display(display), m_roles(roles),
          m_presentation(presentation), m_nextRevision(nextRevision),
          m_presentationRevision(presentationRevision),
          m_presentationTargetGeneration(presentationTargetGeneration) {}
public:
    ViewportEngineProviderDemandRestageAccess(const ViewportEngineProviderDemandRestageAccess&) = delete;
    ViewportEngineProviderDemandRestageAccess(ViewportEngineProviderDemandRestageAccess&&) noexcept = default;
private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewport::PageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewport::PageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

class ViewportEngineProviderFrameRequestAccess
{
    friend class ViewportEngine;
    friend ViewportProviderFrameRequestStartResult startViewportEngineProviderFrameRequest(
        ViewportEngineProviderFrameRequestInput, ViewportEngineProviderFrameRequestAccess);
    ViewportEngineProviderFrameRequestAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request), m_playback(playback), m_display(display), m_roles(roles),
          m_presentation(presentation), m_nextRevision(nextRevision),
          m_presentationRevision(presentationRevision),
          m_presentationTargetGeneration(presentationTargetGeneration) {}
public:
    ViewportEngineProviderFrameRequestAccess(const ViewportEngineProviderFrameRequestAccess&) = delete;
    ViewportEngineProviderFrameRequestAccess(ViewportEngineProviderFrameRequestAccess&&) noexcept = default;
private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewport::PageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewport::PageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

#undef VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS

ViewportProviderSessionOpenResult reduceViewportEngineProviderSessionOpened(
    ViewportEngineProviderSessionOpenedInput, ViewportEngineProviderSessionOpenedAccess);
ViewportProviderFrameQueueFlushResult reduceViewportEngineProviderQueueFlush(
    ViewportEngineProviderQueueFlushInput, ViewportEngineProviderQueueFlushAccess);
std::array<ViewportProviderFrameTransportEffect, 2> reduceViewportEngineProviderDemandRestage(
    ViewportEngineProviderDemandRestageInput, ViewportEngineProviderDemandRestageAccess);
ViewportProviderFrameRequestStartResult startViewportEngineProviderFrameRequest(
    ViewportEngineProviderFrameRequestInput, ViewportEngineProviderFrameRequestAccess);
