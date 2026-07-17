#include "imagesequencesource_p.h"
#include "imageviewport_p.h"
#include "presentationgeometry_p.h"
#include "viewportcommandoutcome_p.h"
#include "viewportitemtransaction_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace ImageViewportInternal;

namespace {

bool hasDisplayedSecondaryRole(const ImageViewportInternal::DisplayState& display)
{
    return display.roles[1].displayedPayload.hasPresentableContent();
}

} // namespace

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
bool isPositiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

}

ImageViewportPrivate::SpreadDirection ImageViewportPrivate::spreadDirection() const
{
    return lastStateSnapshot.presentation().spreadDirection();
}

double ImageViewportPrivate::pageGap() const { return lastStateSnapshot.presentation().pageGap(); }

ImageViewportPrivate::CommandReason ImageViewportPrivate::commandReason() const
{
    return lastStateSnapshot.diagnostics().commandReason();
}

ImageViewportPrivate::DisplayStatus ImageViewportPrivate::displayStatus() const
{
    return lastStateSnapshot.display().status();
}

int ImageViewportPrivate::displayedFrame() const
{
    if (hasReadyDisplay()) {
        return lastStateSnapshot.primary().display().frame();
    }

    return -1;
}

int ImageViewportPrivate::requestedFrame() const
{
    if (hasDisplayableSequence()) {
        return lastStateSnapshot.primary().request().frame();
    }

    return -1;
}

int ImageViewportPrivate::primaryDisplayedFrame() const { return displayedFrame(); }

int ImageViewportPrivate::primaryRequestedFrame() const { return requestedFrame(); }

int ImageViewportPrivate::secondaryDisplayedFrame() const
{
    if (hasReadyDisplay()
        && lastStateSnapshot.secondary().display().sourceLogicalSize().isValid()) {
        return lastStateSnapshot.secondary().display().frame();
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedFrame() const
{
    if (lastStateSnapshot.secondary().metadata().available()) {
        return lastStateSnapshot.secondary().request().frame();
    }

    return -1;
}

int ImageViewportPrivate::displayedPosition() const
{
    if (hasReadyDisplay()) {
        return lastStateSnapshot.primary().display().position();
    }

    return -1;
}

int ImageViewportPrivate::requestedPosition() const
{
    if (lastStateSnapshot.primary().metadata().available()
        && (lastStateSnapshot.primary().metadata().totalDuration() >= 0
            || lastStateSnapshot.primary().request().position() >= 0)) {
        return lastStateSnapshot.primary().request().position();
    }

    return -1;
}

int ImageViewportPrivate::primaryDisplayedPosition() const { return displayedPosition(); }

int ImageViewportPrivate::primaryRequestedPosition() const { return requestedPosition(); }

int ImageViewportPrivate::secondaryDisplayedPosition() const
{
    if (hasReadyDisplay()
        && lastStateSnapshot.secondary().display().sourceLogicalSize().isValid()) {
        return lastStateSnapshot.secondary().display().position();
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedPosition() const
{
    if (!lastStateSnapshot.secondary().metadata().available()) {
        return -1;
    }
    if (lastStateSnapshot.secondary().metadata().totalDuration() >= 0
        || lastStateSnapshot.secondary().request().position() >= 0) {
        return lastStateSnapshot.secondary().request().position();
    }

    return -1;
}

QSizeF ImageViewportPrivate::displayedSpreadSize() const
{
    const QSizeF spreadSize = PresentationGeometry::spreadSize(engine.geometryState());
    return isPositiveSize(spreadSize) ? spreadSize : QSizeF(0.0, 0.0);
}

QString ImageViewportPrivate::errorString() const
{
    return lastStateSnapshot.diagnostics().errorString();
}

QString ImageViewportPrivate::warningString() const
{
    return lastStateSnapshot.diagnostics().warningString();
}

ImageViewportCommandResult ImageViewportPrivate::setPresentationTarget(
    const ImageViewportPresentationTarget& presentationTarget,
    PresentationTargetTransitionPolicy policy)
{
    ImageSequenceSource primarySource = factorySequenceSource(presentationTarget.primary());
    ImageSequenceSource secondarySourceHandle
        = factorySequenceSource(presentationTarget.secondary());
    const auto reduced = engine.assignPresentationTarget(
        { presentationTarget, policy, std::move(primarySource), std::move(secondarySourceHandle) });
    ViewportCommandResult result
        = ImageViewportInternal::CommandOutcome::fromEngineCommand(reduced.command);
    mergeChanges(result.transition.changes, reduced.changes);
    appendProviderTransport(
        result.transition.providerAfterPublication, reduced.providerEffects[0], PageRole::Primary);
    appendProviderTransport(result.transition.providerAfterPublication, reduced.providerEffects[1],
        PageRole::Secondary);
    for (const auto& effect : reduced.providerSessionOpenEffects) {
        if (effect.openSession) {
            result.transition.providerAfterPublication.append(effect.command);
        }
    }
    result.transition.playbackSchedule = reduced.schedule;
    const ImageViewportStateSnapshot snapshot = applyEngineTransition(result.transition);
    return commandResult(result.outcome, snapshot);
}
