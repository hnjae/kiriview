/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "viewportenginebuiltinframeoperations_p.h"
#include "viewportengineproviderprojection_p.h"
#include "viewportengineproviderrequesttokenoperations_p.h"

struct ViewportEngineProviderRoleMaterializationInput
{
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    ViewportEngineGeometryInput geometry;
    bool fromPlayback = false;
};

struct ViewportEngineProviderRoleMaterializationMutation
{
    ImageViewportInternal::RequestState request;
    ImageViewportInternal::PlaybackState playback;
    ImageViewportInternal::DisplayState display;
    std::array<ViewportEngineRoleState, 2> roles;
    const ImageViewportInternal::PresentationState& presentation;
    quint64 nextRevision = 0;
    quint64 presentationRevision = 0;
    quint64 presentationTargetGeneration = 0;
};

using ViewportEngineProviderRoleMaterializationAccess
    = ViewportEngineProviderRoleMaterializationMutation;

struct ViewportEngineProviderRoleMaterializationResult
{
    bool accepted = false;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect effect;
};

void invalidateViewportEngineTargetSpreadRole(ImageViewportInternal::RequestState& request,
    ImageViewportInternal::DisplayState& display, ImageViewportPageRole role);

void coalesceViewportEngineTargetSpreadCandidates(
    ImageViewportInternal::RequestState& request, ImageViewportInternal::DisplayState& display);

ViewportEngineProviderRoleMaterializationResult materializeViewportEngineProviderRole(
    ViewportEngineProviderRoleMaterializationInput,
    ViewportEngineProviderRoleMaterializationAccess&);

ViewportEngineBuiltInFrameStageResult materializeViewportEngineBuiltInTargetSpread(
    ImageViewportInternal::RequestState&, ImageViewportInternal::PlaybackState&,
    ImageViewportInternal::DisplayState&, const ImageViewportInternal::PresentationState&,
    const ViewportEngineGeometryInput&);
