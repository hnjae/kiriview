/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportengineprojection_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportprovidercontract_p.h"

#include <array>
#include <optional>

struct ViewportEngineGeometryChangeInput
{
    QRectF itemBounds;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
    PresentationGeometry::State geometryState;
    ImageViewportExactnessPreference exactnessPreference
        = ImageViewportExactnessPreference::Default;
};

struct ViewportEngineGeometryChangeReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    std::optional<ViewportEngineGeometryInput> providerDemandGeometry;
};

struct ViewportEngineRenderSynchronizationInput
{
    quint64 targetPresentationRevision = 0;
    quint64 displayedPresentationRevision = 0;
    QSizeF itemSize;
    QRectF itemBounds;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
    ViewportEngineGeometryInput currentGeometry;
    ViewportEngineGeometryInput pendingGeometry;
};

struct ViewportEngineRenderAcknowledgementInput
{
    ViewportRenderAcknowledgement acknowledgement;
    bool renderedImagePresent = false;
    quint64 attempt = 0;
    bool pendingTargetCommit = false;
    bool pendingRefinementCommit = false;
    bool pendingPrimaryRefinementCommit = false;
    bool pendingSecondaryRefinementCommit = false;
    bool committedDisplayAttempt = false;
    ImageViewportInternal::PreparedPayload preparedPayload;
    ImageViewportDisplayStatus oldDisplayStatus = ImageViewportDisplayStatus::Empty;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
    PresentationGeometry::State geometryState;
};

struct ViewportEngineRenderCommitReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::InternalObservationBatch observations;
};

struct ViewportEngineRenderFailureReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::RenderFailureDiagnostic diagnostic;
    ImageViewportInternal::InternalObservationBatch observations;
};
struct ViewportEngineGeometryChangeMutation
{
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::PlaybackState playback;
};
struct ViewportEngineRenderSynchronizationMutation
{
    ViewportEngineRenderCoordinationState render;
};
struct ViewportEngineRenderMutation
{
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::DisplayState display;
    ImageViewportInternal::PlaybackState playback;
};

class ViewportEngineGeometryChangeAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEngineGeometryChangeReduction reduceViewportEngineGeometryChange(
        ViewportEngineGeometryChangeInput, ViewportEngineGeometryChangeAccess&);
    ViewportEngineGeometryChangeAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::PlaybackState& playback)
        : m_request(request)
        , m_display(display)
        , m_playback(playback)
    {
    }

public:
    ViewportEngineGeometryChangeAccess(const ViewportEngineGeometryChangeAccess&) = delete;
    ViewportEngineGeometryChangeAccess(ViewportEngineGeometryChangeAccess&&) noexcept = default;
    ViewportEngineGeometryChangeAccess& operator=(const ViewportEngineGeometryChangeAccess&)
        = delete;
    ViewportEngineGeometryChangeMutation takeMutation()
    {
        return { std::move(m_request), std::move(m_display), m_playback };
    }

private:
    ImageViewportInternal::RequestState& request() { return m_request; }
    ImageViewportInternal::DisplayState& display() { return m_display; }
    ImageViewportInternal::PlaybackState& playback() { return m_playback; }
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::DisplayState m_display;
    ImageViewportInternal::PlaybackState m_playback;
};

class
    ViewportEngineRenderSynchronizationAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEngineRenderCoordinationState::AttemptContext synchronizeViewportEngineRender(
        ViewportEngineRenderSynchronizationInput, ViewportEngineRenderSynchronizationAccess&);
    ViewportEngineRenderSynchronizationAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::PresentationState& presentation,
        const ViewportEngineRenderCoordinationState& render)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
        , m_render(render)
    {
    }

public:
    ViewportEngineRenderSynchronizationAccess(const ViewportEngineRenderSynchronizationAccess&)
        = delete;
    ViewportEngineRenderSynchronizationMutation takeMutation() { return { std::move(m_render) }; }

private:
    [[nodiscard]] const ImageViewportInternal::RequestState& request() const { return m_request; }
    [[nodiscard]] const ImageViewportInternal::DisplayState& display() const { return m_display; }
    [[nodiscard]] const ImageViewportInternal::PresentationState& presentation() const
    {
        return m_presentation;
    }
    ViewportEngineRenderCoordinationState& render() { return m_render; }
    [[nodiscard]] ViewportEngineRenderSnapshotProjectionAccess renderSnapshot() const
    {
        return { m_request, m_display, m_presentation };
    }

    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    const ImageViewportInternal::PresentationState& m_presentation;
    ViewportEngineRenderCoordinationState m_render;
};

class ViewportEngineRenderCommitAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEngineRenderCommitReduction reduceViewportEngineRenderCommit(
        ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderCommitAccess&);
    ViewportEngineRenderCommitAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::PlaybackState& playback,
        ViewportEngineProviderFactsView providerFacts)
        : m_request(request)
        , m_display(display)
        , m_playback(playback)
        , m_providerFacts(providerFacts)
    {
    }

public:
    ViewportEngineRenderCommitAccess(const ViewportEngineRenderCommitAccess&) = delete;
    ViewportEngineRenderMutation takeMutation()
    {
        return { std::move(m_request), std::move(m_display), m_playback };
    }
    [[nodiscard]] const ViewportEngineProviderFactsView& providerFacts() const
    {
        return m_providerFacts;
    }

private:
    ImageViewportInternal::RequestState& request() { return m_request; }
    ImageViewportInternal::DisplayState& display() { return m_display; }
    ImageViewportInternal::PlaybackState& playback() { return m_playback; }
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::DisplayState m_display;
    ImageViewportInternal::PlaybackState m_playback;
    ViewportEngineProviderFactsView m_providerFacts;
};

class ViewportEngineRenderFailureAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEngineRenderFailureReduction reduceViewportEngineRenderFailure(
        ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderFailureAccess&);
    ViewportEngineRenderFailureAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::PlaybackState& playback)
        : m_request(request)
        , m_display(display)
        , m_playback(playback)
    {
    }

public:
    ViewportEngineRenderFailureAccess(const ViewportEngineRenderFailureAccess&) = delete;
    ViewportEngineRenderMutation takeMutation()
    {
        return { std::move(m_request), std::move(m_display), m_playback };
    }

private:
    ImageViewportInternal::RequestState& request() { return m_request; }
    ImageViewportInternal::DisplayState& display() { return m_display; }
    ImageViewportInternal::PlaybackState& playback() { return m_playback; }
    ImageViewportInternal::RequestState m_request;
    ImageViewportInternal::DisplayState m_display;
    ImageViewportInternal::PlaybackState m_playback;
};

ViewportEngineRenderCoordinationState::AttemptContext synchronizeViewportEngineRender(
    ViewportEngineRenderSynchronizationInput, ViewportEngineRenderSynchronizationAccess&);
ViewportEngineRenderCommitReduction reduceViewportEngineRenderCommit(
    ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderCommitAccess&);
ViewportEngineRenderFailureReduction reduceViewportEngineRenderFailure(
    ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderFailureAccess&);
ViewportEngineGeometryChangeReduction reduceViewportEngineGeometryChange(
    ViewportEngineGeometryChangeInput, ViewportEngineGeometryChangeAccess&);
