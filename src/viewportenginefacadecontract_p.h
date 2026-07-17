#pragma once

#include "imagesequencesource_p.h"
#include "imageviewporttoken_p.h"
#include "viewportenginecontracts_p.h"
#include "viewportenginetransition_p.h"
#include "viewportplaybackcontract_p.h"
#include "viewportprovidercontract_p.h"
#include "viewportrendercontract_p.h"

#include <array>

struct ViewportEnginePresentationTargetAssignmentRequest
{
    ImageViewportPresentationTarget presentationTarget = ImageViewportPresentationTarget::clear();
    PresentationTargetTransitionPolicy transitionPolicy;
    ImageViewportInternal::ImageSequenceSource primarySource;
    ImageViewportInternal::ImageSequenceSource secondarySource;
    ViewportEngineViewportInput viewport;
};

struct ViewportEngineCommandDiagnostics
{
    ImageViewportCommandReason reason = ImageViewportCommandReason::NoCommand;
    RevisionToken revision;
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

struct ViewportEngineRenderSynchronizationRequest
{
    ViewportEngineViewportInput viewport;
};

struct ViewportEngineGeometryChangeRequest
{
    ViewportEngineViewportInput viewport;
    QRectF oldContentRect;
    QRectF oldVisibleImageRect;
};

struct ViewportEngineRenderHostFactRequest
{
    ViewportRenderHostFact fact;
};

struct ViewportEngineGeometryChangeTransition
{
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerEffects;
};

struct ViewportEngineRenderHostTransition
{
    ImageViewportInternal::ViewportChangeSet changes;
    std::array<ViewportProviderFrameTransportEffect, 2> providerEffects;
    ViewportPlaybackScheduleEffect playbackSchedule;
    ImageViewportInternal::RenderFailureDiagnostic diagnostic;
    ImageViewportInternal::InternalObservationBatch observations;
};

struct ViewportEngineResourcePressureFact
{
    ViewportEngineViewportInput viewport;
};

struct ViewportEnginePlaybackCommandRequest
{
    ViewportPlaybackCommand command;
    ViewportEngineViewportInput viewport;
};

struct ViewportEnginePlaybackTickRequest
{
    int elapsedMilliseconds = 0;
    ViewportEngineViewportInput viewport;
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
    ViewportEngineViewportInput viewport;
    QPointF anchor;
    int quarterTurnDelta = 0;
};

struct ViewportEngineProviderHostEventRequest
{
    ViewportProviderHostEvent event;
    ViewportEngineViewportInput viewport;
};
