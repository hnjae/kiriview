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
    PresentationTargetTransitionPolicy::DisplayTransition displayTransition
        = PresentationTargetTransitionPolicy::DisplayTransition::RetainPrevious;
    PresentationTargetTransitionPolicy::ZoomTransition magnificationPolicy
        = PresentationTargetTransitionPolicy::ZoomTransition::Preserve;
    PresentationTargetTransitionPolicy::ContentPositionTransition contentPositionTransition
        = PresentationTargetTransitionPolicy::ContentPositionTransition::Clamp;
    PresentationTargetTransitionPolicy::RotationTransition rotationTransition
        = PresentationTargetTransitionPolicy::RotationTransition::Preserve;
    PresentationTargetTransitionPolicy::MirrorTransition mirrorTransition
        = PresentationTargetTransitionPolicy::MirrorTransition::Preserve;
    PresentationTargetTransitionPolicy::ReplacementIntent replacementIntent
        = PresentationTargetTransitionPolicy::ReplacementIntent::NewTarget;
    std::optional<ImageViewport::FitMode> explicitFitMode;
    std::optional<ImageViewport::SpreadDirection> explicitSpreadDirection;
    std::optional<double> explicitPageGap;
};

struct ViewportSequenceAssignment
{
    ViewportSequenceAssignment() = default;
    ViewportSequenceAssignment(ImageSequence* primarySequence)
        : presentationTarget(primarySequence ? ImageViewportPresentationTarget(primarySequence)
                                             : ImageViewportPresentationTarget::clear())
        , sequence(primarySequence)
    {
        source.sequence = primarySequence;
    }

    ImageViewportPresentationTarget presentationTarget = ImageViewportPresentationTarget::clear();
    ImageViewportInternal::ImageSequenceSource source;
    ImageViewportInternal::ImageSequenceSource secondarySourceHandle;
    ImageSequence* sequence = nullptr;
    ImageSequence* secondarySequence = nullptr;
    ViewportSequenceRoleSource secondarySource;
    PresentationTargetTransitionPolicy transitionPolicy;
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
