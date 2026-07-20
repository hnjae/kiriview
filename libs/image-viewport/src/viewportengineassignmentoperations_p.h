/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportenginecontracts_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"
#include "viewportplaybackcontract_p.h"

struct ViewportEnginePresentationTargetAssignmentInput
{
    ViewportEnginePresentationTarget presentationTarget = ViewportEnginePresentationTarget::clear();
    ViewportEnginePresentationTargetTransitionPolicy transitionPolicy;
    ImageViewportInternal::ImageSequenceSource primarySource;
    ImageViewportInternal::ImageSequenceSource secondarySource;
    ViewportEngineGeometryInput geometry;
};

struct ViewportEnginePresentationTargetAssignmentMutation
{
    ViewportEnginePresentationTargetState target;
    quint64 nextTargetGeneration = 0;
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::PlaybackState playback;
    ImageViewportInternal::DisplayState display;
    std::array<ViewportEngineRoleState, 2> roles;
    ImageViewportInternal::PresentationState presentation;
};

struct ViewportEnginePresentationTargetAssignmentReduction
{
    ViewportEnginePresentationTargetAssignmentMutation mutation;
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

class
    ViewportEnginePresentationTargetAssignmentAccess // NOLINT(cppcoreguidelines-special-member-functions)
{
    friend class ViewportEngine;
    friend ViewportEnginePresentationTargetAssignmentReduction
        reduceViewportEnginePresentationTargetAssignment(
            ViewportEnginePresentationTargetAssignmentInput,
            ViewportEnginePresentationTargetAssignmentAccess);

    explicit ViewportEnginePresentationTargetAssignmentAccess(
        ViewportEnginePresentationTargetAssignmentMutation mutation)
        : m_mutation(std::move(mutation))
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
    ViewportEnginePresentationTargetAssignmentMutation m_mutation;
};

ViewportEnginePresentationTargetAssignmentReduction
    reduceViewportEnginePresentationTargetAssignment(
        ViewportEnginePresentationTargetAssignmentInput,
        ViewportEnginePresentationTargetAssignmentAccess);
bool validateViewportEnginePresentationTargetAssignment(
    const ViewportEnginePresentationTargetAssignmentInput&,
    const ViewportEnginePresentationTargetState&, const ImageViewportInternal::RequestState&,
    const std::array<ViewportEngineRoleState, 2>&);
