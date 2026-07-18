// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagesequencesource_p.h"
#include "imageviewport_p.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace ImageViewportInternal;

bool PresentationTargetTransitionPolicy::isValid() const
{
    auto displayTransitionValid = [](DisplayTransition transition) {
        switch (transition) {
        case DisplayTransition::RetainPrevious:
        case DisplayTransition::ClearBeforeLoad:
            return true;
        }
        return false;
    };
    auto zoomTransitionValid = [](ZoomTransition transition) {
        switch (transition) {
        case ZoomTransition::Preserve:
        case ZoomTransition::ResetToContain:
            return true;
        }
        return false;
    };
    auto contentPositionTransitionValid = [](ContentPositionTransition transition) {
        switch (transition) {
        case ContentPositionTransition::Clamp:
        case ContentPositionTransition::AnchorStart:
        case ContentPositionTransition::AnchorEnd:
            return true;
        }
        return false;
    };
    auto rotationTransitionValid = [](RotationTransition transition) {
        switch (transition) {
        case RotationTransition::Preserve:
        case RotationTransition::Reset:
            return true;
        }
        return false;
    };
    auto mirrorTransitionValid = [](MirrorTransition transition) {
        switch (transition) {
        case MirrorTransition::Preserve:
        case MirrorTransition::Reset:
            return true;
        }
        return false;
    };
    auto fitModeTransitionValid = [](FitModeTransition transition) {
        switch (transition) {
        case FitModeTransition::Preserve:
        case FitModeTransition::SetExplicit:
            return true;
        }
        return false;
    };
    auto fitModeValid = [](ImageViewportFitMode mode) {
        switch (mode) {
        case ImageViewportFitMode::Contain:
        case ImageViewportFitMode::FitWidth:
        case ImageViewportFitMode::FitHeight:
        case ImageViewportFitMode::Manual:
            return true;
        }
        return false;
    };
    auto spreadDirectionTransitionValid = [](SpreadDirectionTransition transition) {
        switch (transition) {
        case SpreadDirectionTransition::Preserve:
        case SpreadDirectionTransition::SetExplicit:
            return true;
        }
        return false;
    };
    auto spreadDirectionValid = [](ImageViewportSpreadDirection direction) {
        switch (direction) {
        case ImageViewportSpreadDirection::LeftToRight:
        case ImageViewportSpreadDirection::RightToLeft:
            return true;
        }
        return false;
    };
    auto pageGapTransitionValid = [](PageGapTransition transition) {
        switch (transition) {
        case PageGapTransition::Preserve:
        case PageGapTransition::SetExplicit:
            return true;
        }
        return false;
    };
    auto replacementIntentValid = [](ReplacementIntent intent) {
        switch (intent) {
        case ReplacementIntent::NewTarget:
        case ReplacementIntent::SameTargetRefinement:
            return true;
        }
        return false;
    };

    if (!displayTransitionValid(m_displayTransition) || !zoomTransitionValid(m_zoomTransition)
        || !contentPositionTransitionValid(m_contentPositionTransition)
        || !rotationTransitionValid(m_rotationTransition)
        || !mirrorTransitionValid(m_mirrorTransition)
        || !fitModeTransitionValid(m_fitModeTransition)
        || !spreadDirectionTransitionValid(m_spreadDirectionTransition)
        || !pageGapTransitionValid(m_pageGapTransition)
        || !replacementIntentValid(m_replacementIntent)) {
        return false;
    }
    if ((m_fitModeSet && !fitModeValid(m_fitMode))
        || (m_fitModeTransition == FitModeTransition::SetExplicit && !m_fitModeSet)) {
        return false;
    }
    if ((m_spreadDirectionSet && !spreadDirectionValid(m_spreadDirection))
        || (m_spreadDirectionTransition == SpreadDirectionTransition::SetExplicit
            && !m_spreadDirectionSet)) {
        return false;
    }
    if ((m_pageGapSet
            && (!std::isfinite(m_pageGap) || m_pageGap < 0.0
                || m_pageGap > ImageViewportDisplayLimits::maximumPageGap()))
        || (m_pageGapTransition == PageGapTransition::SetExplicit && !m_pageGapSet)) {
        return false;
    }
    return m_zoomTransition != ZoomTransition::ResetToContain
        || m_fitModeTransition != FitModeTransition::SetExplicit;
}

namespace {

ViewportEnginePresentationTarget enginePresentationTarget(
    const ImageViewportPresentationTarget& target)
{
    return { target.primary(), target.secondary(), target.isValid() };
}

ViewportEnginePresentationTargetTransitionPolicy engineTransitionPolicy(
    const PresentationTargetTransitionPolicy& policy)
{
    using EnginePolicy = ViewportEnginePresentationTargetTransitionPolicy;
    EnginePolicy result;
    result.displayTransitionValue
        = static_cast<EnginePolicy::DisplayTransition>(policy.displayTransition());
    result.zoomTransitionValue = static_cast<EnginePolicy::ZoomTransition>(policy.zoomTransition());
    result.contentPositionTransitionValue
        = static_cast<EnginePolicy::ContentPositionTransition>(policy.contentPositionTransition());
    result.rotationTransitionValue
        = static_cast<EnginePolicy::RotationTransition>(policy.rotationTransition());
    result.mirrorTransitionValue
        = static_cast<EnginePolicy::MirrorTransition>(policy.mirrorTransition());
    result.fitModeTransitionValue
        = static_cast<EnginePolicy::FitModeTransition>(policy.fitModeTransition());
    result.fitModeValue = policy.fitMode();
    result.fitModeSet = policy.hasExplicitFitMode();
    result.spreadDirectionTransitionValue
        = static_cast<EnginePolicy::SpreadDirectionTransition>(policy.spreadDirectionTransition());
    result.spreadDirectionValue = policy.spreadDirection();
    result.spreadDirectionSet = policy.hasExplicitSpreadDirection();
    result.pageGapTransitionValue
        = static_cast<EnginePolicy::PageGapTransition>(policy.pageGapTransition());
    result.pageGapValue = policy.pageGap();
    result.pageGapSet = policy.hasExplicitPageGap();
    result.replacementIntentValue
        = static_cast<EnginePolicy::ReplacementIntent>(policy.replacementIntent());
    result.shapeValid = policy.isValid();
    return result;
}

} // namespace

ImageViewportCommandResult ImageViewportPrivate::setPresentationTarget(
    const ImageViewportPresentationTarget& presentationTarget,
    PresentationTargetTransitionPolicy policy)
{
    ImageSequenceSource primarySource = factorySequenceSource(presentationTarget.primary());
    ImageSequenceSource secondarySourceHandle
        = factorySequenceSource(presentationTarget.secondary());
    const ViewportEnginePresentationTargetAssignmentRequest request {
        enginePresentationTarget(presentationTarget), engineTransitionPolicy(policy),
        std::move(primarySource), std::move(secondarySourceHandle)
    };
    const bool flushRefinementElapsed = policy.replacementIntent()
            == PresentationTargetTransitionPolicy::ReplacementIntent::SameTargetRefinement
        && engine.canAssignPresentationTarget(request);
    if (flushRefinementElapsed) {
        ++itemTransactionDepth;
        playbackScheduler.flushElapsed();
    }
    auto reduced = engine.assignPresentationTarget(request);
    const CommandOutcome outcome = reduced.outcome();
    ImageViewportStateSnapshot snapshot = applyEngineTransition(reduced.takeTransition());
    if (flushRefinementElapsed) {
        --itemTransactionDepth;
        snapshot = finalizeItemTransaction();
    }
    return commandResult(outcome, snapshot);
}
