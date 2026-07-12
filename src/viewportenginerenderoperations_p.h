#pragma once

#include "viewportengineprojection_p.h"
#include "viewportplaybackcontract_p.h"

struct ViewportEngineRenderSynchronizationInput
{
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
    quint64 synchronizationAttempt = 0;
    bool pendingTargetCommit = false;
    bool pendingSecondaryProviderCommit = false;
    ImageViewportInternal::PreparedPayload preparedPayload;
    ImageViewport::DisplayStatus oldDisplayStatus = ImageViewport::DisplayStatus::Empty;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
    PresentationGeometry::State geometryState;
};

struct ViewportEngineRenderCommitReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
};

struct ViewportEngineRenderCommitTransition
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportPlaybackScheduleEffect playbackSchedule;
};

struct ViewportEngineRenderFailureReduction
{
    ImageViewportInternal::ViewportChangeSet changes;
    ImageViewportInternal::RenderFailureDiagnostic diagnostic;
};

struct ViewportEngineRenderFailureTransition
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportPlaybackScheduleEffect playbackSchedule;
    ImageViewportInternal::RenderFailureDiagnostic diagnostic;
};

class ViewportEngineRenderSynchronizationAccess
{
    friend class ViewportEngine;
    ViewportEngineRenderSynchronizationAccess(const ImageViewportInternal::RequestState& request,
        const ImageViewportInternal::DisplayState& display,
        const ImageViewportInternal::PresentationState& presentation,
        ViewportEngineRenderCoordinationState& render)
        : m_request(request)
        , m_display(display)
        , m_presentation(presentation)
        , m_render(render)
    {
    }

public:
    ViewportEngineRenderSynchronizationAccess(const ViewportEngineRenderSynchronizationAccess&)
        = delete;
    const ImageViewportInternal::RequestState& request() const { return m_request; }
    const ImageViewportInternal::DisplayState& display() const { return m_display; }
    const ImageViewportInternal::PresentationState& presentation() const { return m_presentation; }
    ViewportEngineRenderCoordinationState& render() const { return m_render; }
    ViewportEngineRenderSnapshotProjectionAccess renderSnapshot() const
    {
        return { m_request, m_display, m_presentation };
    }

private:
    const ImageViewportInternal::RequestState& m_request;
    const ImageViewportInternal::DisplayState& m_display;
    const ImageViewportInternal::PresentationState& m_presentation;
    ViewportEngineRenderCoordinationState& m_render;
};

class ViewportEngineRenderCommitAccess
{
    friend class ViewportEngine;
    ViewportEngineRenderCommitAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::DisplayState& display,
        ImageViewportInternal::PlaybackState& playback,
        ViewportEngineProviderFactsView providerFacts,
        const ViewportEngineRenderCoordinationState& render)
        : m_request(request)
        , m_display(display)
        , m_playback(playback)
        , m_providerFacts(providerFacts)
        , m_render(render)
    {
    }

public:
    ViewportEngineRenderCommitAccess(const ViewportEngineRenderCommitAccess&) = delete;
    ImageViewportInternal::RequestState& request() const { return m_request; }
    ImageViewportInternal::DisplayState& display() const { return m_display; }
    ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    const ViewportEngineProviderFactsView& providerFacts() const { return m_providerFacts; }
    const ViewportEngineRenderCoordinationState& render() const { return m_render; }

private:
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::DisplayState& m_display;
    ImageViewportInternal::PlaybackState& m_playback;
    ViewportEngineProviderFactsView m_providerFacts;
    const ViewportEngineRenderCoordinationState& m_render;
};

class ViewportEngineRenderFailureAccess
{
    friend class ViewportEngine;
    ViewportEngineRenderFailureAccess(ImageViewportInternal::RequestState& request,
        ImageViewportInternal::DisplayState& display,
        ImageViewportInternal::PlaybackState& playback,
        const ViewportEngineRenderCoordinationState& render)
        : m_request(request)
        , m_display(display)
        , m_playback(playback)
        , m_render(render)
    {
    }

public:
    ViewportEngineRenderFailureAccess(const ViewportEngineRenderFailureAccess&) = delete;
    ImageViewportInternal::RequestState& request() const { return m_request; }
    ImageViewportInternal::DisplayState& display() const { return m_display; }
    ImageViewportInternal::PlaybackState& playback() const { return m_playback; }
    const ViewportEngineRenderCoordinationState& render() const { return m_render; }

private:
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::DisplayState& m_display;
    ImageViewportInternal::PlaybackState& m_playback;
    const ViewportEngineRenderCoordinationState& m_render;
};

ViewportRenderSynchronization synchronizeViewportEngineRender(
    ViewportEngineRenderSynchronizationInput, ViewportEngineRenderSynchronizationAccess);
ViewportEngineRenderCommitReduction reduceViewportEngineRenderCommit(
    ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderCommitAccess);
ViewportEngineRenderFailureReduction reduceViewportEngineRenderFailure(
    ViewportEngineRenderAcknowledgementInput, ViewportEngineRenderFailureAccess);
