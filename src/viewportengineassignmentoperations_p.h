#pragma once

#include "viewportenginecontracts_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"
#include "viewportplaybackcontract_p.h"

struct ViewportEnginePresentationTargetAssignmentInput
{
    ImageViewportPresentationTarget presentationTarget = ImageViewportPresentationTarget::clear();
    PresentationTargetTransitionPolicy transitionPolicy;
    ImageViewportInternal::ImageSequenceSource primarySource;
    ImageViewportInternal::ImageSequenceSource secondarySource;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEnginePresentationTargetAssignmentReduction
{
    ViewportEnginePresentationTargetState presentationTargetState;
    bool presentationTargetChanged = false;
    bool clear = true;
    bool retainPreviousDisplay = true;
    bool releaseDisplayedState = false;
    bool resetDisplayRequests = false;
    bool stopPlayback = false;
    bool closeProviderSessions = false;
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerEffects;
    std::array<ViewportEngineProviderSessionOpenEffect, 2> providerSessionOpenEffects;
};

class ViewportEnginePresentationTargetAssignmentAccess
{
    friend class ViewportEngine;
    friend ViewportEnginePresentationTargetAssignmentReduction
        reduceViewportEnginePresentationTargetAssignment(
            ViewportEnginePresentationTargetAssignmentInput,
            ViewportEnginePresentationTargetAssignmentAccess);

    ViewportEnginePresentationTargetAssignmentAccess(ViewportEnginePresentationTargetState& target,
        quint64& nextTargetGeneration, ImageViewportInternal::RequestState& request,
        ImageViewportInternal::PlaybackState& playback,
        ImageViewportInternal::DisplayState& display, std::array<ViewportEngineRoleState, 2>& roles,
        ImageViewportInternal::PresentationState& presentation)
        : m_target(target)
        , m_nextTargetGeneration(nextTargetGeneration)
        , m_request(request)
        , m_playback(playback)
        , m_display(display)
        , m_roles(roles)
        , m_presentation(presentation)
    {
    }

public:
    ViewportEnginePresentationTargetAssignmentAccess(
        const ViewportEnginePresentationTargetAssignmentAccess&)
        = delete;
    ViewportEnginePresentationTargetAssignmentAccess(
        ViewportEnginePresentationTargetAssignmentAccess&&) noexcept
        = default;

private:
    ViewportProviderFrameTransportEffect closeSession(ImageViewportPageRole);
    ViewportEngineProviderSessionOpenEffect openSession(
        ImageViewportPageRole, const ImageViewportInternal::ImageSequenceSource&, quint64);
    ViewportEnginePresentationTargetTransitionReduction transition(
        ViewportEnginePresentationTargetTransitionInput);
    void applyAutoplay();
    ViewportEnginePresentationTargetState& m_target;
    quint64& m_nextTargetGeneration;
    ImageViewportInternal::RequestState& m_request;
    ImageViewportInternal::PlaybackState& m_playback;
    ImageViewportInternal::DisplayState& m_display;
    std::array<ViewportEngineRoleState, 2>& m_roles;
    ImageViewportInternal::PresentationState& m_presentation;
};

ViewportEnginePresentationTargetAssignmentReduction
    reduceViewportEnginePresentationTargetAssignment(
        ViewportEnginePresentationTargetAssignmentInput,
        ViewportEnginePresentationTargetAssignmentAccess);
bool validateViewportEnginePresentationTargetAssignment(
    const ViewportEnginePresentationTargetAssignmentInput&,
    const ViewportEnginePresentationTargetState&, const ImageViewportInternal::RequestState&,
    const std::array<ViewportEngineRoleState, 2>&);
