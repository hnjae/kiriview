/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportenginestate_p.h"

class ViewportEngineSnapshotStateAccess
{
private:
    friend class ViewportEngine;
    ViewportEngineSnapshotStateAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        ViewportEngineProviderFactsView providerFacts,
        const ImageViewportInternal::PresentationState& presentation,
        const ViewportEnginePresentationTargetState& presentationTarget,
        ImageViewportCommandReason commandReason, const RevisionToken& commandRevision,
        quint64 publishedCommandRevision, quint64 presentationRevision,
        quint64 targetPresentationRevision, quint64 snapshotRevision,
        const std::optional<ViewportEngineRecoveredTransitionFailure>& recoveredTransitionFailure)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_providerFacts(providerFacts)
        , m_presentation(presentation)
        , m_presentationTarget(presentationTarget)
        , m_commandRevision(commandRevision)
        , m_commandReason(commandReason)
        , m_publishedCommandRevision(publishedCommandRevision)
        , m_presentationRevision(presentationRevision)
        , m_targetPresentationRevision(targetPresentationRevision)
        , m_snapshotRevision(snapshotRevision)
        , m_recoveredTransitionFailure(recoveredTransitionFailure)
    {
    }

public:
    ViewportEngineSnapshotStateAccess(const ViewportEngineSnapshotStateAccess&) = delete;
    ViewportEngineSnapshotStateAccess& operator=(const ViewportEngineSnapshotStateAccess&) = delete;

    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    const ViewportEngineProviderFactsView& providerFacts() const { return m_providerFacts; }
    const ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }
    const ViewportEnginePresentationTargetState& presentationTarget() const
    {
        return m_presentationTarget;
    }
    const RevisionToken& commandRevision() const { return m_commandRevision; }
    ImageViewportCommandReason commandReason() const { return m_commandReason; }
    quint64 publishedCommandRevision() const { return m_publishedCommandRevision; }
    quint64 presentationRevision() const { return m_presentationRevision; }
    quint64 targetPresentationRevision() const { return m_targetPresentationRevision; }
    quint64 snapshotRevision() const { return m_snapshotRevision; }
    const std::optional<ViewportEngineRecoveredTransitionFailure>&
    recoveredTransitionFailure() const
    {
        return m_recoveredTransitionFailure;
    }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::PlaybackState& m_playback;
    const ImageViewportInternal::DisplayState& m_display;
    ViewportEngineProviderFactsView m_providerFacts;
    const ImageViewportInternal::PresentationState& m_presentation;
    const ViewportEnginePresentationTargetState& m_presentationTarget;
    const RevisionToken& m_commandRevision;
    ImageViewportCommandReason m_commandReason = ImageViewportCommandReason::NoCommand;
    quint64 m_publishedCommandRevision = 0;
    quint64 m_presentationRevision = 0;
    quint64 m_targetPresentationRevision = 0;
    quint64 m_snapshotRevision = 0;
    const std::optional<ViewportEngineRecoveredTransitionFailure>& m_recoveredTransitionFailure;
};
