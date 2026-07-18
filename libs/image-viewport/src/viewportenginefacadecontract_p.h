/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imagesequencesource_p.h"
#include "imageviewporttoken_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportenginetransition_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportprovidercontract_p.h"
#include "viewportrendercontract_p.h"

#include <optional>
#include <utility>

struct ViewportEnginePresentationTargetAssignmentRequest
{
    ViewportEnginePresentationTarget presentationTarget = ViewportEnginePresentationTarget::clear();
    ViewportEnginePresentationTargetTransitionPolicy transitionPolicy;
    ImageViewportInternal::ImageSequenceSource primarySource;
    ImageViewportInternal::ImageSequenceSource secondarySource;
};

struct ViewportEngineCommandDiagnostics
{
    ImageViewportCommandReason reason = ImageViewportCommandReason::NoCommand;
    RevisionToken revision;
};

enum class ViewportEngineCoordinateRoleKind {
    Null,
    Value,
    Invalid,
};

struct ViewportEngineCoordinateQueryRequest
{
    ImageViewportCoordinateSpace sourceSpace = ImageViewportCoordinateSpace::Item;
    ImageViewportCoordinateSpace targetSpace = ImageViewportCoordinateSpace::Item;
    ViewportEngineCoordinateRoleKind roleKind = ViewportEngineCoordinateRoleKind::Null;
    ImageViewportPageRole role = ImageViewportPageRole::Primary;
    QPointF point;
};

struct ViewportEngineCoordinateQueryResult
{
    bool valid = false;
    ImageViewportCoordinateSpace space = ImageViewportCoordinateSpace::Item;
    std::optional<ImageViewportPageRole> role;
    QPointF point;
};

struct ViewportEngineCommandResult
{
    ImageViewportCommandOutcome outcome = ImageViewportCommandOutcome::Accepted;
    ImageViewportCommandReason reason = ImageViewportCommandReason::NoCommand;
    RevisionToken commandRevision;
    bool commandRevisionChanged = false;
};

struct ViewportEngineRenderHostFactRequest
{
    ViewportRenderHostFact fact;
};

struct ViewportEnginePlaybackCommandRequest
{
    ViewportPlaybackCommand command;
};

struct ViewportEnginePlaybackTickRequest
{
    int elapsedMilliseconds = 0;
};

struct ViewportEnginePresentationCommandRequest
{
    ViewportEnginePresentationCommand command;
};

class ViewportEngineProviderHostEventRequest
{
public:
    static ViewportEngineProviderHostEventRequest admit(ViewportProviderHostEvent event)
    {
        return ViewportEngineProviderHostEventRequest(std::move(event));
    }

    ViewportEngineProviderHostEventRequest(const ViewportEngineProviderHostEventRequest&) = default;
    ViewportEngineProviderHostEventRequest(ViewportEngineProviderHostEventRequest&&) noexcept
        = default;
    ViewportEngineProviderHostEventRequest& operator=(const ViewportEngineProviderHostEventRequest&)
        = default;
    ViewportEngineProviderHostEventRequest& operator=(
        ViewportEngineProviderHostEventRequest&&) noexcept
        = default;

    const ViewportProviderHostEvent& event() const { return m_event; }

private:
    explicit ViewportEngineProviderHostEventRequest(ViewportProviderHostEvent event)
        : m_event(std::move(event))
    {
    }

    ViewportProviderHostEvent m_event;
};
