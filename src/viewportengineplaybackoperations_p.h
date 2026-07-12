#pragma once

#include "viewportenginestate_p.h"
#include "viewportengineproviderrequesttokenoperations_p.h"
#include "viewportengineproviderprojection_p.h"
#include "viewportplaybackcontract_p.h"

class ViewportEngineTestAccess;

struct ViewportEngineAuthoredAutoplayInput
{
};

struct ViewportEngineAuthoredAutoplayReduction
{
    bool armed = false;
    bool activeRequestChanged = false;
    bool playbackChanged = false;
    bool playbackPhaseChanged = false;
};

class ViewportEngineAuthoredAutoplayAccess
{
    friend class ViewportEngine;
    friend class ViewportEngineTestAccess;
    ViewportEngineAuthoredAutoplayAccess(const ImageViewportInternal::ImageSequenceSource& source,
        const ImageViewportInternal::ProviderFactsState& providerFacts,
        ImageViewportInternal::DisplayRequest& activeRequest,
        ImageViewportInternal::PlaybackState& playback, ImageViewport::RequestStatus requestStatus)
        : m_source(source)
        , m_providerFacts(providerFacts)
        , m_activeRequest(activeRequest)
        , m_playback(playback)
        , m_requestStatus(requestStatus)
    {
    }

public:
    ViewportEngineAuthoredAutoplayAccess(const ViewportEngineAuthoredAutoplayAccess&) = delete;
    ViewportEngineAuthoredAutoplayAccess(ViewportEngineAuthoredAutoplayAccess&&) noexcept = default;
    ViewportEngineAuthoredAutoplayAccess& operator=(const ViewportEngineAuthoredAutoplayAccess&)
        = delete;

    const ImageViewportInternal::ImageSequenceSource& source() const { return m_source; }
    const ImageViewportInternal::ProviderFactsState& providerFacts() const
    {
        return m_providerFacts;
    }
    ImageViewportInternal::DisplayRequest& activeRequest() const { return m_activeRequest; }
    ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    ImageViewport::RequestStatus requestStatus() const { return m_requestStatus; }

private:
    const ImageViewportInternal::ImageSequenceSource& m_source;
    const ImageViewportInternal::ProviderFactsState& m_providerFacts;
    ImageViewportInternal::DisplayRequest& m_activeRequest;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewport::RequestStatus m_requestStatus = ImageViewport::RequestStatus::NoRequest;
};

struct ViewportEnginePlaybackPauseInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
};

struct ViewportEnginePlaybackPauseReduction
{
    bool playbackPhaseChanged = false;
};

struct ViewportEnginePlaybackStopInput
{
    ImageViewport::PageRole role = ImageViewport::PageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEnginePlaybackStopReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerFrameTransport;
};

class ViewportEnginePlaybackStopAccess
{
    friend class ViewportEngine;
    friend ViewportEnginePlaybackStopReduction reduceViewportEnginePlaybackStop(
        ViewportEnginePlaybackStopInput, ViewportEnginePlaybackStopAccess);

    ViewportEnginePlaybackStopAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display,
        std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation,
        quint64& nextRevision, quint64 presentationRevision, quint64 presentationTargetGeneration)
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
    ViewportEnginePlaybackStopAccess(const ViewportEnginePlaybackStopAccess&) = delete;
    ViewportEnginePlaybackStopAccess(ViewportEnginePlaybackStopAccess&&) noexcept = default;
    ViewportEnginePlaybackStopAccess& operator=(const ViewportEnginePlaybackStopAccess&) = delete;

private:
    ImageSequenceProviderDisplayDemand providerDemand(
        ImageViewport::PageRole role, const ViewportEngineGeometryInput& geometry) const;
    ViewportProviderRequestTokenAllocationResult allocateProviderRequestToken(
        ImageViewport::PageRole role);

    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::DisplayState& m_display;
    std::array<ViewportEngineRoleState, 2>& m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    quint64& m_nextRevision;
    quint64 m_presentationRevision = 0;
    quint64 m_presentationTargetGeneration = 0;
};

class ViewportEnginePlaybackPauseAccess
{
    friend class ViewportEngine;
    explicit ViewportEnginePlaybackPauseAccess(ImageViewportInternal::PlaybackState& playback)
        : m_playback(playback)
    {
    }

public:
    ViewportEnginePlaybackPauseAccess(const ViewportEnginePlaybackPauseAccess&) = delete;
    ViewportEnginePlaybackPauseAccess(ViewportEnginePlaybackPauseAccess&&) noexcept = default;
    ViewportEnginePlaybackPauseAccess& operator=(const ViewportEnginePlaybackPauseAccess&) = delete;

    ImageViewportInternal::PlaybackState& playback() const { return m_playback; }

private:
    ImageViewportInternal::PlaybackState& m_playback;
};

class ViewportEnginePlaybackScheduleAccess
{
    friend class ViewportEngine;
    ViewportEnginePlaybackScheduleAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        ViewportEngineProviderFactsView providerFacts)
        : m_request(request)
        , m_playback(playback)
        , m_providerFacts(providerFacts)
    {
    }

public:
    ViewportEnginePlaybackScheduleAccess(const ViewportEnginePlaybackScheduleAccess&) = delete;
    ViewportEnginePlaybackScheduleAccess(ViewportEnginePlaybackScheduleAccess&&) noexcept = default;
    ViewportEnginePlaybackScheduleAccess& operator=(const ViewportEnginePlaybackScheduleAccess&)
        = delete;

    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    const ViewportEngineProviderFactsView& providerFacts() const { return m_providerFacts; }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::PlaybackState& m_playback;
    ViewportEngineProviderFactsView m_providerFacts;
};

ViewportPlaybackScheduleEffect projectViewportPlaybackSchedule(
    ViewportEnginePlaybackScheduleAccess);
bool validateViewportPlaybackCommand(ViewportPlaybackCommand);
ViewportEnginePlaybackPauseReduction reduceViewportEnginePlaybackPause(
    ViewportEnginePlaybackPauseInput, ViewportEnginePlaybackPauseAccess);
ViewportEnginePlaybackStopReduction reduceViewportEnginePlaybackStop(
    ViewportEnginePlaybackStopInput, ViewportEnginePlaybackStopAccess);
ViewportEngineAuthoredAutoplayReduction reduceViewportEngineAuthoredAutoplay(
    ViewportEngineAuthoredAutoplayInput, ViewportEngineAuthoredAutoplayAccess);
