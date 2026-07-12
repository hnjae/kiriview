#pragma once

#include "viewportenginestate_p.h"

class ViewportEngineProviderStateAccess
{
private:
    friend class ViewportEngine;
    ViewportEngineProviderStateAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback, ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles)
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

class ViewportEnginePlaybackStateAccess
{
private:
    friend class ViewportEngine;
    ViewportEnginePlaybackStateAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback, ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
    {
    }
public:
    ViewportEnginePlaybackStateAccess(const ViewportEnginePlaybackStateAccess&) = delete;
    ViewportEnginePlaybackStateAccess& operator=(const ViewportEnginePlaybackStateAccess&) = delete;

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

class ViewportEngineGeometryTransitionAccess
{
private:
    friend class ViewportEngine;
    ViewportEngineGeometryTransitionAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles)
        : m_request(request)
        , m_display(display)
        , m_roles(roles)
    {
    }
public:
    ViewportEngineGeometryTransitionAccess(const ViewportEngineGeometryTransitionAccess&) = delete;
    ViewportEngineGeometryTransitionAccess& operator=(const ViewportEngineGeometryTransitionAccess&) = delete;

    ImageViewportInternal::RequestState& request() const { return m_request; }
    ImageViewportInternal::DisplayState& display() const { return m_display; }
    std::array<ViewportEngineRoleState, 2>& roles() const { return m_roles; }

private:
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::DisplayState& m_display;
    std::array<ViewportEngineRoleState, 2>& m_roles;
};

class ViewportEnginePresentationStateAccess
{
private:
    friend class ViewportEngine;
    ViewportEnginePresentationStateAccess(ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        ImageViewportInternal::PresentationState& presentation)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
    {
    }
public:
    ViewportEnginePresentationStateAccess(const ViewportEnginePresentationStateAccess&) = delete;
    ViewportEnginePresentationStateAccess& operator=(const ViewportEnginePresentationStateAccess&)
        = delete;

    ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }

private:
    ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    ImageViewportInternal::PresentationState& m_presentation;
};

class ViewportEnginePresentationLoopingStateAccess
{
private:
    friend class ViewportEngine;
    explicit ViewportEnginePresentationLoopingStateAccess(
        ImageViewportInternal::PlaybackState& playback)
        : m_playback(playback)
    {
    }
public:
    ViewportEnginePresentationLoopingStateAccess(
        const ViewportEnginePresentationLoopingStateAccess&) = delete;
    ViewportEnginePresentationLoopingStateAccess& operator=(
        const ViewportEnginePresentationLoopingStateAccess&) = delete;

    ImageViewportInternal::PlaybackState& playback() const { return m_playback; }

private:
    ImageViewportInternal::PlaybackState& m_playback;
};

class ViewportEngineSnapshotStateAccess
{
private:
    friend class ViewportEngine;
    ViewportEngineSnapshotStateAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation,
        const ViewportEnginePresentationTargetState& presentationTarget,
        ImageViewport::CommandReason commandReason, const RevisionToken& commandRevision,
        quint64 publishedCommandRevision, quint64 presentationRevision,
        quint64 snapshotRevision)
        : m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
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
    const std::array<ViewportEngineRoleState, 2>& roles() const { return m_roles; }
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
    const std::array<ViewportEngineRoleState, 2>& m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    const ViewportEnginePresentationTargetState& m_presentationTarget;
    const RevisionToken& m_commandRevision;
    ImageViewport::CommandReason m_commandReason = ImageViewport::CommandReason::NoCommand;
    quint64 m_publishedCommandRevision = 0;
    quint64 m_presentationRevision = 0;
    quint64 m_snapshotRevision = 0;
};
