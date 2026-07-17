#pragma once

#include "imagesequencesource_p.h"
#include "imageviewporttoken_p.h"
#include "publicdiagnostic_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportenginetransition_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportprovidercontract_p.h"
#include "viewportrendercontract_p.h"

#include <array>
#include <optional>
#include <utility>

struct ViewportEnginePresentationTargetAssignmentRequest
{
    ImageViewportPresentationTarget presentationTarget = ImageViewportPresentationTarget::clear();
    PresentationTargetTransitionPolicy transitionPolicy;
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

struct ViewportEnginePresentationCommandResult
{
    ViewportEngineCommandResult command;
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerEffects;
};

struct ViewportEnginePresentationTargetAssignmentResult
{
    ViewportEngineCommandResult command;
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
    ViewportPlaybackScheduleEffect schedule;
};

struct ViewportEngineRenderHostFactRequest
{
    ViewportRenderHostFact fact;
};

struct ViewportEngineRenderHostTransition
{
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerEffects;
    ViewportPlaybackScheduleEffect playbackSchedule;
    ImageViewportInternal::RenderFailureDiagnostic diagnostic;
    ImageViewportInternal::InternalObservationBatch observations;
};

struct ViewportEnginePlaybackCommandRequest
{
    ViewportPlaybackCommand command;
};

struct ViewportEnginePlaybackTickRequest
{
    int elapsedMilliseconds = 0;
};

struct ViewportEnginePlaybackProviderEffects
{
    std::array<ViewportProviderFrameTransportEffect, 2> providerFrameTransport;
};

struct ViewportEnginePlaybackCommandResult
{
    ViewportEngineCommandResult command;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportEnginePlaybackProviderEffects effects;
    ViewportPlaybackScheduleEffect schedule;
};

struct ViewportEnginePlaybackTickResult
{
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportEnginePlaybackProviderEffects effects;
    ViewportPlaybackScheduleEffect schedule;
};

struct ViewportEnginePresentationCommandRequest
{
    ImageViewportPresentationCommand command;
};

class ViewportEngineProviderHostEventRequest
{
public:
    static ViewportEngineProviderHostEventRequest admit(ViewportProviderHostEvent event)
    {
        QString diagnostic;
        if (event.kind == ViewportProviderHostEvent::Kind::ProviderEvent) {
            diagnostic = std::exchange(event.providerEvent.diagnostic, {});
        } else {
            diagnostic = std::exchange(event.diagnostic, {});
        }
        event.providerEvent.diagnostic.clear();
        event.diagnostic.clear();
        return ViewportEngineProviderHostEventRequest(std::move(event),
            ImageViewportInternal::PublicDiagnosticText::fromUntrusted(std::move(diagnostic)));
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
    const ImageViewportInternal::PublicDiagnosticText& diagnostic() const { return m_diagnostic; }

private:
    ViewportEngineProviderHostEventRequest(
        ViewportProviderHostEvent event, ImageViewportInternal::PublicDiagnosticText diagnostic)
        : m_event(std::move(event))
        , m_diagnostic(std::move(diagnostic))
    {
    }

    ViewportProviderHostEvent m_event;
    ImageViewportInternal::PublicDiagnosticText m_diagnostic;
};
