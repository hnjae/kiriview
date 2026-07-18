/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"
#include "internalobservation_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportprovidercontract_p.h"

#include <utility>

class ViewportEngine;

class ViewportEngineTransitionDraft
{
private:
    friend class ViewportEngine;
    friend class ViewportEngineTransition;

    ViewportEngineTransitionDraft() = default;
    ViewportEngineTransitionDraft(const ViewportEngineTransitionDraft&) = delete;
    ViewportEngineTransitionDraft& operator=(const ViewportEngineTransitionDraft&) = delete;
    ViewportEngineTransitionDraft(ViewportEngineTransitionDraft&&) noexcept = default;
    ViewportEngineTransitionDraft& operator=(ViewportEngineTransitionDraft&&) noexcept = default;

    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderTransportBatch providerTransport;
    ViewportPlaybackScheduleEffect playbackSchedule;
    ImageViewportInternal::ProviderSchedulerDiagnostic providerSchedulerDiagnostic;
    ImageViewportInternal::InternalObservationBatch observations;
};

class ViewportEngineTransition
{
public:
    ViewportEngineTransition() = delete;
    ViewportEngineTransition(const ViewportEngineTransition&) = delete;
    ViewportEngineTransition& operator=(const ViewportEngineTransition&) = delete;
    ViewportEngineTransition(ViewportEngineTransition&&) noexcept = default;
    ViewportEngineTransition& operator=(ViewportEngineTransition&&) noexcept = default;

    bool schedulesRenderUpdate() const { return m_draft.changes.scheduleUpdate; }
    const ViewportProviderTransportBatch& providerTransport() const
    {
        return m_draft.providerTransport;
    }
    ViewportProviderTransportBatch takeProviderTransport()
    {
        return std::move(m_draft.providerTransport);
    }
    const ViewportPlaybackScheduleEffect& playbackSchedule() const
    {
        return m_draft.playbackSchedule;
    }
    const ImageViewportInternal::ProviderSchedulerDiagnostic& providerSchedulerDiagnostic() const
    {
        return m_draft.providerSchedulerDiagnostic;
    }
    const ImageViewportInternal::RenderFailureDiagnostic& renderFailureDiagnostic() const
    {
        return m_draft.changes.renderFailureDiagnostic;
    }
    const ImageViewportInternal::InternalObservationBatch& observations() const
    {
        return m_draft.observations;
    }

private:
    friend class ViewportEngine;
    explicit ViewportEngineTransition(ViewportEngineTransitionDraft draft)
        : m_draft(std::move(draft))
    {
    }

    ViewportEngineTransitionDraft m_draft;
};

class ViewportEngineCommandTransition
{
public:
    ViewportEngineCommandTransition() = delete;
    ViewportEngineCommandTransition(const ViewportEngineCommandTransition&) = delete;
    ViewportEngineCommandTransition& operator=(const ViewportEngineCommandTransition&) = delete;
    ViewportEngineCommandTransition(ViewportEngineCommandTransition&&) noexcept = default;
    ViewportEngineCommandTransition& operator=(ViewportEngineCommandTransition&&) noexcept
        = default;

    ImageViewportCommandOutcome outcome() const { return m_outcome; }
    const ViewportEngineTransition& transition() const { return m_transition; }
    ViewportEngineTransition takeTransition() { return std::move(m_transition); }

private:
    friend class ViewportEngine;
    ViewportEngineCommandTransition(
        ImageViewportCommandOutcome outcome, ViewportEngineTransition transition)
        : m_outcome(outcome)
        , m_transition(std::move(transition))
    {
    }

    ImageViewportCommandOutcome m_outcome = ImageViewportCommandOutcome::Accepted;
    ViewportEngineTransition m_transition;
};
