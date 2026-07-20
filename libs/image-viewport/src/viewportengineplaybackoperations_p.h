/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportengineproviderprojection_p.h"
#include "viewportengineproviderrequesttokenoperations_p.h"
#include "viewportenginestate_p.h"
#include "viewportplaybackcontract_p.h"

class ViewportEngineTestAccess;

struct ViewportEnginePlaybackMutation
{
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::PlaybackState playback;
    ImageViewportInternal::DisplayState display;
    std::array<ViewportEngineRoleState, 2> roles;
    quint64 nextRevision = 0;
};

struct ViewportEngineAuthoredAutoplayInput
{
};

struct ViewportEngineAuthoredAutoplayReduction
{
    bool armed = false;
    bool resolved = false;
    bool playbackPhaseChanged = false;
};
struct ViewportEngineAuthoredAutoplayMutation
{
    ImageViewportInternal::PlaybackState playback;
};

class ViewportEngineAuthoredAutoplayAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend class ViewportEnginePresentationTargetAssignmentAccess;
    friend class ViewportEngineProviderMetadataReadyAccess;
    friend class ViewportEngineTestAccess;
    friend ViewportEngineAuthoredAutoplayReduction reduceViewportEngineAuthoredAutoplay(
        ViewportEngineAuthoredAutoplayInput, ViewportEngineAuthoredAutoplayAccess&);
    ViewportEngineAuthoredAutoplayAccess(const ImageViewportInternal::RequestState& request,
        ViewportEngineProviderFactsView providerFacts,
        const ImageViewportInternal::PlaybackState& playback)
        : m_request(request)
        , m_providerFacts(providerFacts)
        , m_playback(playback)
    {
    }

public:
    ViewportEngineAuthoredAutoplayAccess(const ViewportEngineAuthoredAutoplayAccess&) = delete;
    ViewportEngineAuthoredAutoplayAccess(ViewportEngineAuthoredAutoplayAccess&&) noexcept = default;
    ViewportEngineAuthoredAutoplayAccess& operator=(const ViewportEngineAuthoredAutoplayAccess&)
        = delete;
    ViewportEngineAuthoredAutoplayMutation takeMutation() { return { m_playback }; }

    const ImageViewportInternal::ImageSequenceSource& source(ImageViewportPageRole role) const
    {
        return m_request.roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].source;
    }
    const ImageViewportInternal::ProviderFactsState& providerFacts(ImageViewportPageRole role) const
    {
        return m_providerFacts[role == ImageViewportPageRole::Secondary ? 1U : 0U];
    }
    const ImageViewportInternal::DisplayRequest& activeRequest(ImageViewportPageRole role) const
    {
        return m_request.roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].activeRequest;
    }
    ImageViewportRequestStatus requestStatus() const { return m_request.status; }

private:
    ImageViewportInternal::PlaybackState& playback() { return m_playback; }
    const ImageViewportInternal::RequestState& m_request;
    ViewportEngineProviderFactsView m_providerFacts;
    ImageViewportInternal::PlaybackState m_playback;
};

struct ViewportEnginePlaybackPauseInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
};

struct ViewportEnginePlaybackPauseReduction
{
    bool playbackPhaseChanged = false;
};
struct ViewportEnginePlaybackPauseMutation
{
    ImageViewportInternal::PlaybackState playback;
};

struct ViewportEnginePlaybackStopInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEnginePlaybackStopReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerFrameTransport;
};

class ViewportEnginePlaybackStopAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEnginePlaybackStopReduction reduceViewportEnginePlaybackStop(
        ViewportEnginePlaybackStopInput, ViewportEnginePlaybackStopAccess&);

    ViewportEnginePlaybackStopAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
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
    ViewportEnginePlaybackStopAccess(const ViewportEnginePlaybackStopAccess&) = delete;
    ViewportEnginePlaybackStopAccess(ViewportEnginePlaybackStopAccess&&) noexcept = default;
    ViewportEnginePlaybackStopAccess& operator=(const ViewportEnginePlaybackStopAccess&) = delete;
    ViewportEnginePlaybackMutation takeMutation()
    {
        return { std::move(m_request), m_playback, std::move(m_display), std::move(m_roles),
            m_nextRevision };
    }

private:
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::DisplayState m_display;
    std::array<ViewportEngineRoleState, 2> m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    quint64 m_nextRevision;
    quint64 m_presentationRevision = 0;
    quint64 m_presentationTargetGeneration = 0;
};

struct ViewportEnginePlaybackSeekInput
{
    ViewportPlaybackCommand::Kind kind = ViewportPlaybackCommand::Kind::SeekFrame;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    int value = -1;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEnginePlaybackSeekReduction
{
    ImageViewportCommandOutcome outcome = ImageViewportCommandOutcome::Accepted;
    ImageViewportCommandReason reason = ImageViewportCommandReason::NoCommand;
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerFrameTransport;
};

class ViewportEnginePlaybackSeekAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEnginePlaybackSeekReduction reduceViewportEnginePlaybackSeek(
        ViewportEnginePlaybackSeekInput, ViewportEnginePlaybackSeekAccess&);

    ViewportEnginePlaybackSeekAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
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
    ViewportEnginePlaybackSeekAccess(const ViewportEnginePlaybackSeekAccess&) = delete;
    ViewportEnginePlaybackSeekAccess(ViewportEnginePlaybackSeekAccess&&) noexcept = default;
    ViewportEnginePlaybackSeekAccess& operator=(const ViewportEnginePlaybackSeekAccess&) = delete;
    ViewportEnginePlaybackMutation takeMutation()
    {
        return { std::move(m_request), m_playback, std::move(m_display), std::move(m_roles),
            m_nextRevision };
    }

private:
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::DisplayState m_display;
    std::array<ViewportEngineRoleState, 2> m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    quint64 m_nextRevision;
    quint64 m_presentationRevision = 0;
    quint64 m_presentationTargetGeneration = 0;
};

struct ViewportEnginePlaybackPlayInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEnginePlaybackPlayReduction
{
    ImageViewportCommandOutcome outcome = ImageViewportCommandOutcome::Accepted;
    ImageViewportCommandReason reason = ImageViewportCommandReason::NoCommand;
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerFrameTransport;
};

class ViewportEnginePlaybackPlayAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEnginePlaybackPlayReduction reduceViewportEnginePlaybackPlay(
        ViewportEnginePlaybackPlayInput, ViewportEnginePlaybackPlayAccess&);

    ViewportEnginePlaybackPlayAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
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
    ViewportEnginePlaybackPlayAccess(const ViewportEnginePlaybackPlayAccess&) = delete;
    ViewportEnginePlaybackPlayAccess(ViewportEnginePlaybackPlayAccess&&) noexcept = default;
    ViewportEnginePlaybackPlayAccess& operator=(const ViewportEnginePlaybackPlayAccess&) = delete;
    ViewportEnginePlaybackMutation takeMutation()
    {
        return { std::move(m_request), m_playback, std::move(m_display), std::move(m_roles),
            m_nextRevision };
    }

private:
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::DisplayState m_display;
    std::array<ViewportEngineRoleState, 2> m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    quint64 m_nextRevision;
    quint64 m_presentationRevision = 0;
    quint64 m_presentationTargetGeneration = 0;
};

struct ViewportEnginePlaybackTickInput
{
    int elapsedMilliseconds = 0;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEnginePlaybackTickReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerFrameTransport;
    bool projectSchedule = false;
};

class ViewportEnginePlaybackTickAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEnginePlaybackTickReduction reduceViewportEnginePlaybackTick(
        ViewportEnginePlaybackTickInput, ViewportEnginePlaybackTickAccess&);

    ViewportEnginePlaybackTickAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::PlaybackState& playback,
        const ImageViewportInternal::DisplayState& display,
        const std::array<ViewportEngineRoleState, 2>& roles,
        const ImageViewportInternal::PresentationState& presentation, quint64& nextRevision,
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
    ViewportEnginePlaybackTickAccess(const ViewportEnginePlaybackTickAccess&) = delete;
    ViewportEnginePlaybackTickAccess(ViewportEnginePlaybackTickAccess&&) noexcept = default;
    ViewportEnginePlaybackTickAccess& operator=(const ViewportEnginePlaybackTickAccess&) = delete;
    ViewportEnginePlaybackMutation takeMutation()
    {
        return { std::move(m_request), m_playback, std::move(m_display), std::move(m_roles),
            m_nextRevision };
    }

private:
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::PlaybackState m_playback;
    ImageViewportInternal::DisplayState m_display;
    std::array<ViewportEngineRoleState, 2> m_roles;
    const ImageViewportInternal::PresentationState& m_presentation;
    quint64 m_nextRevision;
    quint64 m_presentationRevision = 0;
    quint64 m_presentationTargetGeneration = 0;
};

class ViewportEnginePlaybackPauseAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEnginePlaybackPauseReduction reduceViewportEnginePlaybackPause(
        ViewportEnginePlaybackPauseInput, ViewportEnginePlaybackPauseAccess&);
    explicit ViewportEnginePlaybackPauseAccess(const ImageViewportInternal::PlaybackState& playback)
        : m_playback(playback)
    {
    }

public:
    ViewportEnginePlaybackPauseAccess(const ViewportEnginePlaybackPauseAccess&) = delete;
    ViewportEnginePlaybackPauseAccess(ViewportEnginePlaybackPauseAccess&&) noexcept = default;
    ViewportEnginePlaybackPauseAccess& operator=(const ViewportEnginePlaybackPauseAccess&) = delete;
    ViewportEnginePlaybackPauseMutation takeMutation() { return { m_playback }; }

private:
    ImageViewportInternal::PlaybackState& playback() { return m_playback; }
    ImageViewportInternal::PlaybackState m_playback;
};

class ViewportEnginePlaybackScheduleAccess // NOLINT(cppcoreguidelines-special-member-functions)
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
    ViewportEnginePlaybackScheduleAccess, ImageViewportPageRole);
bool validateViewportPlaybackCommand(ViewportPlaybackCommand);
ViewportEnginePlaybackPauseReduction reduceViewportEnginePlaybackPause(
    ViewportEnginePlaybackPauseInput, ViewportEnginePlaybackPauseAccess&);
ViewportEnginePlaybackStopReduction reduceViewportEnginePlaybackStop(
    ViewportEnginePlaybackStopInput, ViewportEnginePlaybackStopAccess&);
ViewportEnginePlaybackSeekReduction reduceViewportEnginePlaybackSeek(
    ViewportEnginePlaybackSeekInput, ViewportEnginePlaybackSeekAccess&);
ViewportEnginePlaybackPlayReduction reduceViewportEnginePlaybackPlay(
    ViewportEnginePlaybackPlayInput, ViewportEnginePlaybackPlayAccess&);
ViewportEnginePlaybackTickReduction reduceViewportEnginePlaybackTick(
    ViewportEnginePlaybackTickInput, ViewportEnginePlaybackTickAccess&);
ViewportEngineAuthoredAutoplayReduction reduceViewportEngineAuthoredAutoplay(
    ViewportEngineAuthoredAutoplayInput, ViewportEngineAuthoredAutoplayAccess&);
