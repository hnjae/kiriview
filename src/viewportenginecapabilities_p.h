#pragma once

#include "viewportenginestate_p.h"

class ViewportEngineProviderStateAccess
{
private:
    friend class ViewportEngine;
    ViewportEngineProviderStateAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display, std::array<ViewportEngineRoleState, 2>& roles)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
    {
    }

public:
    ViewportEngineProviderStateAccess(const ViewportEngineProviderStateAccess&) = delete;
    ViewportEngineProviderStateAccess& operator=(const ViewportEngineProviderStateAccess&) = delete;

    ImageViewportInternal::RequestState& request() const { return m_request; }
    ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    ImageViewportInternal::DisplayState& display() const { return m_display; }
    std::array<ViewportEngineRoleState, 2>& roles() const { return m_roles; }

private:
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::DisplayState& m_display;
    std::array<ViewportEngineRoleState, 2>& m_roles;
};

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
        ImageViewport::CommandReason commandReason, const RevisionToken& commandRevision,
        quint64 publishedCommandRevision, quint64 presentationRevision, quint64 snapshotRevision)
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
        , m_snapshotRevision(snapshotRevision)
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
    ImageViewport::CommandReason commandReason() const { return m_commandReason; }
    quint64 publishedCommandRevision() const { return m_publishedCommandRevision; }
    quint64 presentationRevision() const { return m_presentationRevision; }
    quint64 snapshotRevision() const { return m_snapshotRevision; }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::PlaybackState& m_playback;
    const ImageViewportInternal::DisplayState& m_display;
    ViewportEngineProviderFactsView m_providerFacts;
    const ImageViewportInternal::PresentationState& m_presentation;
    const ViewportEnginePresentationTargetState& m_presentationTarget;
    const RevisionToken& m_commandRevision;
    ImageViewport::CommandReason m_commandReason = ImageViewport::CommandReason::NoCommand;
    quint64 m_publishedCommandRevision = 0;
    quint64 m_presentationRevision = 0;
    quint64 m_snapshotRevision = 0;
};
