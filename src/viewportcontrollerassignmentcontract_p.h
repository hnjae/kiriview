#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"
#include "timingintervals_p.h"
#include "viewportcontrollerprovidercontract_p.h"

#include <optional>

struct ViewportSequenceRoleSource
{
    bool present = false;
    bool provider = false;
    bool timed = false;
    int frameCount = -1;
    int firstFramePosition = -1;
    TimingIntervals timingIntervals;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
};

struct ControllerTransitionPolicy
{
    PageSetTransitionPolicy::DisplayTransition displayTransition
        = PageSetTransitionPolicy::DisplayTransition::RetainPrevious;
    PageSetTransitionPolicy::ZoomTransition magnificationPolicy
        = PageSetTransitionPolicy::ZoomTransition::Preserve;
    PageSetTransitionPolicy::ContentPositionTransition contentPositionTransition
        = PageSetTransitionPolicy::ContentPositionTransition::Clamp;
    PageSetTransitionPolicy::RotationTransition rotationTransition
        = PageSetTransitionPolicy::RotationTransition::Preserve;
    PageSetTransitionPolicy::MirrorTransition mirrorTransition
        = PageSetTransitionPolicy::MirrorTransition::Preserve;
    PageSetTransitionPolicy::ReplacementIntent replacementIntent
        = PageSetTransitionPolicy::ReplacementIntent::NewTarget;
    std::optional<ImageViewport::FitMode> explicitFitMode;
    std::optional<ImageViewport::SpreadDirection> explicitSpreadDirection;
    std::optional<double> explicitPageGap;
};

struct ViewportSequenceAssignment
{
    ImageViewportInternal::ImageSequenceSource source;
    ImageViewportInternal::ImageSequenceSource secondarySourceHandle;
    ImageSequence* sequence = nullptr;
    ImageSequence* secondarySequence = nullptr;
    ViewportSequenceRoleSource secondarySource;
    PageSetTransitionPolicy transitionPolicy;
};

struct ViewportSequenceAssignmentResult
{
    ImageViewport::CommandOutcome outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::ViewportChangeSet changes;
    ViewportProviderFrameTransportEffect providerFrameTransport;
    ViewportProviderFrameTransportEffect secondaryProviderFrameTransport;
    bool openProviderSession = false;
    bool openSecondaryProviderSession = false;
};
