/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "framepreparation_p.h"
#include "internalobservation_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportengineproviderterminaloperations_p.h"

struct ViewportEngineProviderFrameReadyInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ImageSequenceProviderRequestToken token;
    ImageFrame* frame = nullptr;
    QImage anchoredFrameImage;
    quint64 providerFrameLeaseId = 0;
    ImageSequenceProviderFrameEnvelope envelope;
    bool provisional = false;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEngineProviderFrameReadyReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::InternalObservationBatch observations;
};

struct ViewportEngineProviderFrameReadyMutation
{
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::PlaybackState playback;
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::ProviderRoleState provider;
};

class ViewportEngineProviderFrameReadyAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEngineProviderFrameReadyReduction reduceViewportEngineProviderFrameReady(
        const ViewportEngineProviderFrameReadyInput&, ViewportEngineProviderFrameReadyAccess&);

    ViewportEngineProviderFrameReadyAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::ProviderRoleState& provider,
        const ImageViewportInternal::PresentationState& presentation)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_provider(provider)
        , m_presentation(presentation)
    {
    }

public:
    ViewportEngineProviderFrameReadyAccess(const ViewportEngineProviderFrameReadyAccess&) = delete;
    ViewportEngineProviderFrameReadyAccess(ViewportEngineProviderFrameReadyAccess&&) noexcept
        = default;
    ViewportEngineProviderFrameReadyMutation takeMutation()
    {
        return { std::move(m_request), m_playback, std::move(m_display), std::move(m_provider) };
    }

private:
    ImageViewportInternal::ViewportChangeSet recordTerminal(
        ViewportEngineProviderTerminalProjectionInput input);

    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::DisplayState m_display;
    ImageViewportInternal::ProviderRoleState m_provider;
    const ImageViewportInternal::PresentationState& m_presentation;
};

ViewportEngineProviderFrameReadyReduction reduceViewportEngineProviderFrameReady(
    const ViewportEngineProviderFrameReadyInput&, ViewportEngineProviderFrameReadyAccess&);
