#include "imagesequenceownership_p.h"
#include "imageviewport_p.h"
#include "presentationgeometry_p.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace ImageViewportInternal;

bool PageSetTransitionPolicy::isValid() const
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

void mergeControllerChanges(ViewportChangeSet& target, ViewportChangeSet source)
{
    target.requestState = target.requestState || source.requestState;
    target.displayState = target.displayState || source.displayState;
    target.geometryState = target.geometryState || source.geometryState;
    target.playbackPhase = target.playbackPhase || source.playbackPhase;
    target.diagnostics = target.diagnostics || source.diagnostics;
    target.presentation = target.presentation || source.presentation;
    target.sequence = target.sequence || source.sequence;
    target.looping = target.looping || source.looping;
    target.displayRevision = target.displayRevision || source.displayRevision;
    target.requestRevision = target.requestRevision || source.requestRevision;
    target.commandRevision = target.commandRevision || source.commandRevision;
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
}

ImageSequence* sequenceFromPageSetValue(const QVariant& value, bool& ok)
{
    if (!value.isValid() || value.isNull()) {
        ok = true;
        return nullptr;
    }

    if (value.canConvert<ImageSequence*>()) {
        if (ImageSequence* sequence = value.value<ImageSequence*>()) {
            ok = true;
            return sequence;
        }
    }

    if (value.canConvert<QObject*>()) {
        QObject* object = value.value<QObject*>();
        if (!object && value.isNull()) {
            ok = true;
            return nullptr;
        }
        if (ImageSequence* sequence = qobject_cast<ImageSequence*>(object)) {
            ok = true;
            return sequence;
        }
    }

    ok = false;
    return nullptr;
}

}

ImageSequence* ImageViewportPrivate::sequence() const { return controller.requestState().sequence; }

void ImageViewportPrivate::setSequence(ImageSequence* sequence)
{
    if (!controller.requestState().sequence && !controller.requestState().secondarySequence
        && !sequence) {
        return;
    }

    std::shared_ptr<ImageSequence> sequenceOwner = factorySequenceOwner(sequence);
    ViewportSequenceAssignmentResult result
        = controller.assignSequence({ sequence, std::move(sequenceOwner) });
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    if (result.openProviderSession && !openProviderSession()) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
}

ImageSequence* ImageViewportPrivate::primarySequence() const { return sequence(); }

ImageSequence* ImageViewportPrivate::secondarySequence() const
{
    return controller.requestState().secondarySequence;
}

ImageViewportPrivate::SpreadDirection ImageViewportPrivate::spreadDirection() const
{
    return controller.presentationState().spreadDirection;
}

void ImageViewportPrivate::setSpreadDirectionProperty(SpreadDirection direction)
{
    setSpreadDirection(direction);
}

double ImageViewportPrivate::pageGap() const { return controller.presentationState().pageGap; }

void ImageViewportPrivate::setPageGapProperty(double gap) { setPageGap(gap); }

ImageViewportPrivate::RequestStatus ImageViewportPrivate::requestStatus() const
{
    return controller.requestState().status;
}

ImageViewportPrivate::RequestReason ImageViewportPrivate::requestReason() const
{
    return controller.requestState().reason;
}

ImageViewportPrivate::CommandReason ImageViewportPrivate::commandReason() const
{
    return controller.requestState().commandReason;
}

ImageViewportPrivate::DisplayStatus ImageViewportPrivate::displayStatus() const
{
    return controller.displayState().status;
}

ImageViewportPrivate::PlaybackPhase ImageViewportPrivate::playbackPhase() const
{
    return controller.requestState().playbackPhase;
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
    if (hasReadyDisplay() && secondarySequence()) {
        return controller.displayState().secondaryDisplayedRequest.request.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedFrame() const
{
    if (secondarySequence()) {
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
    if (hasReadyDisplay() && secondarySequence()) {
        return controller.displayState().secondaryDisplayedRequest.request.target.position;
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedPosition() const
{
    ImageSequence* sequence = secondarySequence();
    if (!sequence) {
        return -1;
    }
    if (sequence->isProvider()
        && (controller.secondaryProviderTimedMetadata()
            || controller.requestState().secondaryActiveRequest.target.position >= 0)) {
        return controller.requestState().secondaryActiveRequest.target.position;
    }
    if (sequence->isTimedList()) {
        return controller.requestState().secondaryActiveRequest.target.position;
    }

    return -1;
}

int ImageViewportPrivate::frameCount() const
{
    return controller.metadataProjection(PageRole::Primary).frameCount;
}

int ImageViewportPrivate::totalDuration() const
{
    return controller.metadataProjection(PageRole::Primary).totalDuration;
}

bool ImageViewportPrivate::hasSecondaryTimedSequence() const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && sequence->isTimedList();
}

int ImageViewportPrivate::secondarySequenceFrameCount() const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && !sequence->isProvider() ? sequence->frameCount() : -1;
}

int ImageViewportPrivate::secondarySequenceTotalDuration() const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && !sequence->isProvider() ? sequence->totalDuration() : -1;
}

int ImageViewportPrivate::secondarySequenceFrameIndexForPosition(int position) const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && sequence->isTimedList() ? sequence->frameIndexForPosition(position) : -1;
}

int ImageViewportPrivate::secondarySequenceFrameStartPosition(int frame) const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && sequence->isTimedList() ? sequence->frameStartPosition(frame) : -1;
}

ImageSequenceAuthoredAnimationFacts ImageViewportPrivate::sequenceAuthoredAnimationFacts() const
{
    ImageSequence* sequence = controller.requestState().sequence;
    return sequence ? sequence->m_authoredAnimationFacts : ImageSequenceAuthoredAnimationFacts {};
}

ImageSequenceAuthoredAnimationFacts
ImageViewportPrivate::secondarySequenceAuthoredAnimationFacts() const
{
    ImageSequence* sequence = secondarySequence();
    return sequence ? sequence->m_authoredAnimationFacts : ImageSequenceAuthoredAnimationFacts {};
}

ImageViewportRange ImageViewportPrivate::frameSeekBounds() const
{
    return controller.metadataProjection(PageRole::Primary).frameSeekBounds;
}

ImageViewportRange ImageViewportPrivate::positionSeekBounds() const
{
    return controller.metadataProjection(PageRole::Primary).positionSeekBounds;
}

int ImageViewportPrivate::primaryFrameCount() const
{
    return controller.metadataProjection(PageRole::Primary).frameCount;
}

int ImageViewportPrivate::secondaryFrameCount() const
{
    return controller.metadataProjection(PageRole::Secondary).frameCount;
}

int ImageViewportPrivate::primaryTotalDuration() const
{
    return controller.metadataProjection(PageRole::Primary).totalDuration;
}

int ImageViewportPrivate::secondaryTotalDuration() const
{
    return controller.metadataProjection(PageRole::Secondary).totalDuration;
}

ImageViewportRange ImageViewportPrivate::primaryFrameSeekBounds() const
{
    return controller.metadataProjection(PageRole::Primary).frameSeekBounds;
}

ImageViewportRange ImageViewportPrivate::secondaryFrameSeekBounds() const
{
    return controller.metadataProjection(PageRole::Secondary).frameSeekBounds;
}

ImageViewportRange ImageViewportPrivate::primaryPositionSeekBounds() const
{
    return controller.metadataProjection(PageRole::Primary).positionSeekBounds;
}

ImageViewportRange ImageViewportPrivate::secondaryPositionSeekBounds() const
{
    return controller.metadataProjection(PageRole::Secondary).positionSeekBounds;
}

ImageViewportPrivate::TriState ImageViewportPrivate::timedPlaybackSupport() const
{
    return controller.metadataProjection(PageRole::Primary).timedPlaybackSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::frameSeekSupport() const
{
    return controller.metadataProjection(PageRole::Primary).frameSeekSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::positionSeekSupport() const
{
    return controller.metadataProjection(PageRole::Primary).positionSeekSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryTimedPlaybackSupport() const
{
    return controller.metadataProjection(PageRole::Primary).timedPlaybackSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryTimedPlaybackSupport() const
{
    return controller.metadataProjection(PageRole::Secondary).timedPlaybackSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryFrameSeekSupport() const
{
    return controller.metadataProjection(PageRole::Primary).frameSeekSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryFrameSeekSupport() const
{
    return controller.metadataProjection(PageRole::Secondary).frameSeekSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryPositionSeekSupport() const
{
    return controller.metadataProjection(PageRole::Primary).positionSeekSupport;
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryPositionSeekSupport() const
{
    return controller.metadataProjection(PageRole::Secondary).positionSeekSupport;
}

QSizeF ImageViewportPrivate::displayedImageSize() const
{
    if (hasReadyDisplay()) {
        return controller.displayState().displayedImageSize;
    }

    return QSizeF(0.0, 0.0);
}

QSizeF ImageViewportPrivate::displayedSpreadSize() const
{
    const QSizeF spreadSize = PresentationGeometry::spreadSize(controller.geometryState());
    return isPositiveSize(spreadSize) ? spreadSize : QSizeF(0.0, 0.0);
}

QSizeF ImageViewportPrivate::primaryDisplayedImageSize() const { return displayedImageSize(); }

QSizeF ImageViewportPrivate::secondaryDisplayedImageSize() const
{
    if (!hasReadyDisplay()) {
        return QSizeF(0.0, 0.0);
    }

    const QSizeF size = controller.displayState().secondaryDisplayedImageSize;
    return isPositiveSize(size) ? size : QSizeF(0.0, 0.0);
}

RevisionToken ImageViewportPrivate::displayRevision() const
{
    return RevisionToken(controller.displayState().revision);
}

RevisionToken ImageViewportPrivate::requestRevision() const
{
    return RevisionToken(controller.requestState().requestRevision);
}

RevisionToken ImageViewportPrivate::commandRevision() const
{
    return RevisionToken(controller.requestState().commandRevision);
}

QString ImageViewportPrivate::errorString() const { return controller.requestState().errorString; }

QString ImageViewportPrivate::warningString() const
{
    return controller.requestState().warningString;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageSet(
    const QVariant& primary, const QVariant& secondary)
{
    return setPageSet(primary, secondary, PageSetTransitionPolicy {});
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageSet(
    const QVariant& primary, const QVariant& secondary, PageSetTransitionPolicy policy)
{
    bool primaryValid = false;
    bool secondaryValid = false;
    ImageSequence* primarySequence = sequenceFromPageSetValue(primary, primaryValid);
    ImageSequence* secondarySequence = sequenceFromPageSetValue(secondary, secondaryValid);

    if (!primaryValid || !secondaryValid) {
        return CommandOutcome::Invalid;
    }

    return setPageSet(primarySequence, secondarySequence, policy);
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageSet(
    ImageSequence* primary, ImageSequence* secondary)
{
    return setPageSet(primary, secondary, PageSetTransitionPolicy {});
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageSet(
    ImageSequence* primarySequence,
    ImageSequence* secondarySequence,
    PageSetTransitionPolicy policy)
{
    std::shared_ptr<ImageSequence> primaryOwner = factorySequenceOwner(primarySequence);
    std::shared_ptr<ImageSequence> secondaryOwner = factorySequenceOwner(secondarySequence);
    ImageViewportInternal::DisplayRequestTarget secondaryInitialTarget;
    ImageViewportInternal::ResolvedFrameIdentity secondaryInitialResolvedFrame;
    if (secondarySequence && secondarySequence->isValid() && !secondarySequence->isProvider()) {
        const int position
            = secondarySequence->isTimedList() ? secondarySequence->frameStartPosition(0) : -1;
        secondaryInitialTarget
            = { 0, position, ImageViewportInternal::ProviderRequestTargetKind::Unknown };
        secondaryInitialResolvedFrame = { 0, position };
    }
    if (!primarySequence) {
        secondarySequence = nullptr;
        secondaryOwner.reset();
        secondaryInitialTarget = {};
        secondaryInitialResolvedFrame = {};
    }
    ViewportSequenceAssignment assignment;
    assignment.sequence = primarySequence;
    assignment.sequenceOwner = std::move(primaryOwner);
    assignment.secondarySequence = secondarySequence;
    assignment.secondarySequenceOwner = std::move(secondaryOwner);
    assignment.secondaryInitialTarget = secondaryInitialTarget;
    assignment.secondaryInitialResolvedFrame = secondaryInitialResolvedFrame;
    assignment.retainPreviousDisplay
        = policy.displayTransition() == PageSetTransitionPolicy::DisplayTransition::RetainPrevious;
    assignment.secondaryIsProvider = primarySequence && secondarySequence
        && secondarySequence->isProvider();
    assignment.transitionPolicy = policy;
    ViewportSequenceAssignmentResult result = controller.assignSequence(std::move(assignment));
    if (result.outcome != CommandOutcome::Accepted) {
        applyControllerChanges(result.changes);
        return result.outcome;
    }
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    if (result.openProviderSession && !openProviderSession()) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    if (result.openSecondaryProviderSession && !openProviderSession(PageRole::Secondary)) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(PageRole::Secondary,
                QStringLiteral("provider session creation failed")));
    }
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}
