/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportengineproviderprojection_p.h"
#include "viewportengineproviderrequesttokenoperations_p.h"
#include "viewportplaybackcontract_p.h"

struct ViewportEngineProviderSessionOpenedInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderQueueFlushInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderDemandRestageInput
{
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderFrameRequestInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageViewportInternal::DisplayRequestTarget target;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderRequestMutation
{
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::PlaybackState playback;
    ImageViewportInternal::DisplayState display;
    std::array<ViewportEngineRoleState, 2> roles;
    quint64 nextRevision = 0;
};

#define VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS                                                   \
    ImageViewportInternal::RequestState m_request;                                                 \
    ImageViewportInternal::PlaybackState m_playback;                                               \
    ImageViewportInternal::DisplayState m_display;                                                 \
    std::array<ViewportEngineRoleState, 2> m_roles;                                                \
    const ImageViewportInternal::PresentationState& m_presentation;                                \
    quint64 m_nextRevision;                                                                        \
    quint64 m_presentationRevision = 0;                                                            \
    quint64 m_presentationTargetGeneration = 0

#define VIEWPORT_PROVIDER_REQUEST_TAKE_MUTATION                                                    \
    ViewportEngineProviderRequestMutation takeMutation()                                           \
    {                                                                                              \
        return { std::move(m_request), m_playback, std::move(m_display), std::move(m_roles),       \
            m_nextRevision };                                                                      \
    }

class
    ViewportEngineProviderSessionOpenedAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportProviderSessionOpenResult reduceViewportEngineProviderSessionOpened(
        ViewportEngineProviderSessionOpenedInput, ViewportEngineProviderSessionOpenedAccess&);

    ViewportEngineProviderSessionOpenedAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64 nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
        , m_presentation(presentation)
        , m_nextRevision(nextRevision)
        , m_presentationRevision(presentationRevision)
        , m_presentationTargetGeneration(presentationTargetGeneration)
    {
    }

public:
    ViewportEngineProviderSessionOpenedAccess(const ViewportEngineProviderSessionOpenedAccess&)
        = delete;
    ViewportEngineProviderSessionOpenedAccess(ViewportEngineProviderSessionOpenedAccess&&) noexcept
        = default;
    VIEWPORT_PROVIDER_REQUEST_TAKE_MUTATION;

private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewportPageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

class ViewportEngineProviderQueueFlushAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportProviderFrameQueueFlushResult reduceViewportEngineProviderQueueFlush(
        ViewportEngineProviderQueueFlushInput, ViewportEngineProviderQueueFlushAccess&);
    ViewportEngineProviderQueueFlushAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64 nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
        , m_presentation(presentation)
        , m_nextRevision(nextRevision)
        , m_presentationRevision(presentationRevision)
        , m_presentationTargetGeneration(presentationTargetGeneration)
    {
    }

public:
    ViewportEngineProviderQueueFlushAccess(const ViewportEngineProviderQueueFlushAccess&) = delete;
    ViewportEngineProviderQueueFlushAccess(ViewportEngineProviderQueueFlushAccess&&) noexcept
        = default;
    VIEWPORT_PROVIDER_REQUEST_TAKE_MUTATION;

private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewportPageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

class
    ViewportEngineProviderDemandRestageAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend std::array<ViewportProviderFrameTransportEffect, 2>
    reduceViewportEngineProviderDemandRestage(
        ViewportEngineProviderDemandRestageInput, ViewportEngineProviderDemandRestageAccess&);
    ViewportEngineProviderDemandRestageAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64 nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
        , m_presentation(presentation)
        , m_nextRevision(nextRevision)
        , m_presentationRevision(presentationRevision)
        , m_presentationTargetGeneration(presentationTargetGeneration)
    {
    }

public:
    ViewportEngineProviderDemandRestageAccess(const ViewportEngineProviderDemandRestageAccess&)
        = delete;
    ViewportEngineProviderDemandRestageAccess(ViewportEngineProviderDemandRestageAccess&&) noexcept
        = default;
    VIEWPORT_PROVIDER_REQUEST_TAKE_MUTATION;

private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewportPageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

class ViewportEngineProviderFrameRequestAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend class ViewportEngineProviderMetadataReadyAccess;
    friend class ViewportEngineProviderEndOfSequenceAccess;
    friend ViewportProviderFrameRequestStartResult startViewportEngineProviderFrameRequest(
        ViewportEngineProviderFrameRequestInput, ViewportEngineProviderFrameRequestAccess&);
    ViewportEngineProviderFrameRequestAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64 nextRevision,
        quint64 presentationRevision, quint64 presentationTargetGeneration)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
        , m_presentation(presentation)
        , m_nextRevision(nextRevision)
        , m_presentationRevision(presentationRevision)
        , m_presentationTargetGeneration(presentationTargetGeneration)
    {
    }

public:
    ViewportEngineProviderFrameRequestAccess(const ViewportEngineProviderFrameRequestAccess&)
        = delete;
    ViewportEngineProviderFrameRequestAccess(ViewportEngineProviderFrameRequestAccess&&) noexcept
        = default;
    VIEWPORT_PROVIDER_REQUEST_TAKE_MUTATION;

private:
    ViewportProviderRequestTokenAllocationResult allocate(ImageViewportPageRole role);
    ImageSequenceProviderDisplayDemand demand(
        ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry);
    VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS;
};

#undef VIEWPORT_PROVIDER_REQUEST_ACCESS_MEMBERS
#undef VIEWPORT_PROVIDER_REQUEST_TAKE_MUTATION

ViewportProviderSessionOpenResult reduceViewportEngineProviderSessionOpened(
    ViewportEngineProviderSessionOpenedInput, ViewportEngineProviderSessionOpenedAccess&);
ViewportProviderFrameQueueFlushResult reduceViewportEngineProviderQueueFlush(
    ViewportEngineProviderQueueFlushInput, ViewportEngineProviderQueueFlushAccess&);
std::array<ViewportProviderFrameTransportEffect, 2> reduceViewportEngineProviderDemandRestage(
    ViewportEngineProviderDemandRestageInput, ViewportEngineProviderDemandRestageAccess&);
ViewportProviderFrameRequestStartResult startViewportEngineProviderFrameRequest(
    ViewportEngineProviderFrameRequestInput, ViewportEngineProviderFrameRequestAccess&);
