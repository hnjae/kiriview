#include "imagesequencesource_p.h"
#include "imageviewport_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollermetadatacontract_p.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace ImageViewportInternal;

namespace {

bool hasDisplayedSecondaryRole(const ImageViewportInternal::DisplayState& display)
{
    const QSizeF size = display.secondaryDisplayedImageSize;
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
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
        case ZoomTransition::PreserveManualPercent:
            return true;
        }
        return false;
    };
    auto contentPositionTransitionValid = [](ContentPositionTransition transition) {
        switch (transition) {
        case ContentPositionTransition::Preserve:
        case ContentPositionTransition::Clamp:
        case ContentPositionTransition::ScanStart:
        case ContentPositionTransition::ScanEnd:
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
    auto fitModeValid = [](ImageViewport::FitMode mode) {
        switch (mode) {
        case ImageViewport::FitMode::Contain:
        case ImageViewport::FitMode::FitWidth:
        case ImageViewport::FitMode::FitHeight:
        case ImageViewport::FitMode::Manual:
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
    auto spreadDirectionValid = [](ImageViewport::SpreadDirection direction) {
        switch (direction) {
        case ImageViewport::SpreadDirection::LeftToRight:
        case ImageViewport::SpreadDirection::RightToLeft:
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
    if (m_fitModeTransition == FitModeTransition::SetExplicit
        && (!m_fitModeSet || !fitModeValid(m_fitMode))) {
        return false;
    }
    if (m_spreadDirectionTransition == SpreadDirectionTransition::SetExplicit
        && (!m_spreadDirectionSet || !spreadDirectionValid(m_spreadDirection))) {
        return false;
    }
    if (m_pageGapTransition == PageGapTransition::SetExplicit
        && (!m_pageGapSet || !std::isfinite(m_pageGap) || m_pageGap < 0.0)) {
        return false;
    }
    return m_zoomTransition != ZoomTransition::ResetToContain
        || m_fitModeTransition != FitModeTransition::SetExplicit
        || m_fitMode == ImageViewport::FitMode::Contain;
}

namespace {
bool isPositiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

}

ImageViewportPrivate::SpreadDirection ImageViewportPrivate::spreadDirection() const
{
    return controller.presentationState().spreadDirection;
}

double ImageViewportPrivate::pageGap() const { return controller.presentationState().pageGap; }

ImageViewportPrivate::CommandReason ImageViewportPrivate::commandReason() const
{
    return controller.requestState().commandReason;
}

ImageViewportPrivate::DisplayStatus ImageViewportPrivate::displayStatus() const
{
    return controller.displayState().status;
}

int ImageViewportPrivate::displayedFrame() const
{
    if (hasReadyDisplay()) {
        return controller.displayState().displayedRequest.request.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::requestedFrame() const
{
    if (hasDisplayableSequence()) {
        return controller.requestState().activeRequest.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::primaryDisplayedFrame() const { return displayedFrame(); }

int ImageViewportPrivate::primaryRequestedFrame() const { return requestedFrame(); }

int ImageViewportPrivate::secondaryDisplayedFrame() const
{
    if (hasReadyDisplay() && hasDisplayedSecondaryRole(controller.displayState())) {
        return controller.displayState().secondaryDisplayedRequest.request.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedFrame() const
{
    if (controller.requestState().secondarySequence) {
        return controller.requestState().secondaryActiveRequest.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::displayedPosition() const
{
    if (hasReadyDisplay()) {
        return controller.displayState().displayedRequest.request.target.position;
    }

    return -1;
}

int ImageViewportPrivate::requestedPosition() const
{
    if (hasProviderSequence()
        && (controller.providerTimedMetadata()
            || controller.requestState().activeRequest.target.position >= 0)) {
        return controller.requestState().activeRequest.target.position;
    }
    if (hasTimedSequence()) {
        return controller.requestState().activeRequest.target.position;
    }

    return -1;
}

int ImageViewportPrivate::primaryDisplayedPosition() const { return displayedPosition(); }

int ImageViewportPrivate::primaryRequestedPosition() const { return requestedPosition(); }

int ImageViewportPrivate::secondaryDisplayedPosition() const
{
    if (hasReadyDisplay() && hasDisplayedSecondaryRole(controller.displayState())) {
        return controller.displayState().secondaryDisplayedRequest.request.target.position;
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedPosition() const
{
    if (!controller.requestState().secondarySequence) {
        return -1;
    }
    const ImageSequenceSource& source = controller.requestState().secondarySequenceSource;
    if (source.facts.provider
        && (controller.secondaryProviderTimedMetadata()
            || controller.requestState().secondaryActiveRequest.target.position >= 0)) {
        return controller.requestState().secondaryActiveRequest.target.position;
    }
    if (source.facts.timed) {
        return controller.requestState().secondaryActiveRequest.target.position;
    }

    return -1;
}

bool ImageViewportPrivate::hasSecondaryTimedSequence() const
{
    return controller.requestState().secondarySequenceSource.facts.timed;
}

int ImageViewportPrivate::secondarySequenceFrameCount() const
{
    const ImageSequenceSource& source = controller.requestState().secondarySequenceSource;
    return source.facts.present && !source.facts.provider ? source.facts.frameCount : -1;
}

int ImageViewportPrivate::secondarySequenceTotalDuration() const
{
    const ImageSequenceSource& source = controller.requestState().secondarySequenceSource;
    return source.facts.present && !source.facts.provider ? source.facts.totalDuration : -1;
}

int ImageViewportPrivate::secondarySequenceFrameIndexForPosition(int position) const
{
    return sourceFrameIndexForPosition(controller.requestState().secondarySequenceSource, position);
}

int ImageViewportPrivate::secondarySequenceFrameStartPosition(int frame) const
{
    return sourceFrameStartPosition(controller.requestState().secondarySequenceSource, frame);
}

ImageSequenceAuthoredAnimationFacts ImageViewportPrivate::sequenceAuthoredAnimationFacts() const
{
    return controller.requestState().sequenceSource.facts.authoredAnimationFacts;
}

ImageSequenceAuthoredAnimationFacts
ImageViewportPrivate::secondarySequenceAuthoredAnimationFacts() const
{
    return controller.requestState().secondarySequenceSource.facts.authoredAnimationFacts;
}

QSizeF ImageViewportPrivate::displayedSpreadSize() const
{
    const QSizeF spreadSize = PresentationGeometry::spreadSize(controller.geometryState());
    return isPositiveSize(spreadSize) ? spreadSize : QSizeF(0.0, 0.0);
}

QString ImageViewportPrivate::errorString() const { return controller.requestState().errorString; }

QString ImageViewportPrivate::warningString() const
{
    return controller.requestState().warningString;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPresentationTarget(
    ImageViewportPresentationTarget presentationTarget, PresentationTargetTransitionPolicy policy)
{
    ImageSequenceSource primarySource = factorySequenceSource(presentationTarget.primary());
    ImageSequenceSource secondarySourceHandle
        = factorySequenceSource(presentationTarget.secondary());
    ViewportSequenceAssignment assignment;
    assignment.presentationTarget = std::move(presentationTarget);
    assignment.source = std::move(primarySource);
    assignment.secondarySourceHandle = std::move(secondarySourceHandle);
    assignment.transitionPolicy = policy;
    ViewportSequenceAssignmentResult result = controller.assignSequence(std::move(assignment));
    if (result.outcome != CommandOutcome::Accepted) {
        applyControllerChanges(result.changes);
        return result.outcome;
    }
    applyControllerChanges(result.changes);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    if (result.openProviderSession && !providerHost.openSession()) {
        applyControllerChanges(controller.handleProviderSessionOpenFailure(
            QStringLiteral("provider session creation failed")));
        playbackScheduler.apply(controller.playbackScheduleEffect());
        return result.outcome;
    }
    if (result.openSecondaryProviderSession && !providerHost.openSession(PageRole::Secondary)) {
        applyControllerChanges(controller.handleProviderSessionOpenFailure(
            PageRole::Secondary, QStringLiteral("provider session creation failed")));
    }
    playbackScheduler.apply(controller.playbackScheduleEffect());
    return result.outcome;
}
