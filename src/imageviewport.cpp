#include "imagesequenceownership_p.h"
#include "imageviewport_p.h"

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

int ImageViewportPrivate::frameCountForSequence(ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return -1;
    }
    if (sequence->isStill() || sequence->isTimedList()) {
        return sequence->frameCount();
    }
    if (!sequence->isProvider()) {
        return -1;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return sequence->m_providerKnownFacts.isTimedFrameList()
            ? sequence->m_providerKnownFrameCount
            : 1;
    }
    if (sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(sequence->m_providerFrameSeekCapability)) {
        return sequence->m_providerKnownFacts.frameCount();
    }

    return -1;
}

int ImageViewportPrivate::totalDurationForSequence(ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return -1;
    }
    if (sequence->isTimedList()) {
        return sequence->totalDuration();
    }
    if (sequence->isProvider() && sequence->m_hasCompleteProviderKnownMetadata
        && sequence->m_providerKnownFacts.isTimedFrameList()
        && sequence->m_providerKnownTimingIntervals) {
        return sequence->m_providerKnownTimingIntervals->totalDuration();
    }

    return -1;
}

ImageViewportRange ImageViewportPrivate::frameSeekBoundsForSequence(ImageSequence* sequence) const
{
    if (frameSeekSupportForSequence(sequence) != TriState::True) {
        return invalidRange();
    }

    const int sequenceFrameCount = frameCountForSequence(sequence);
    if (sequenceFrameCount <= 0) {
        return invalidRange();
    }

    return ImageViewportRange(0, sequenceFrameCount - 1);
}

ImageViewportRange ImageViewportPrivate::positionSeekBoundsForSequence(
    ImageSequence* sequence) const
{
    if (positionSeekSupportForSequence(sequence) != TriState::True) {
        return invalidRange();
    }

    const int sequenceTotalDuration = totalDurationForSequence(sequence);
    if (sequenceTotalDuration < 0) {
        return invalidRange();
    }

    return ImageViewportRange(0, sequenceTotalDuration);
}

ImageViewportPrivate::TriState ImageViewportPrivate::timedPlaybackSupportForSequence(
    ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return TriState::Unavailable;
    }
    if (sequence->isTimedList()) {
        return TriState::True;
    }
    if (sequence->isStill()) {
        return TriState::False;
    }
    if (!sequence->isProvider()) {
        return TriState::Unavailable;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return providerResolvedCapability(sequence->m_providerTimedPlaybackCapability,
                   sequence->m_providerKnownFacts.isTimedFrameList())
            ? TriState::True
            : TriState::False;
    }

    return capabilitySupportToTriState(sequence->m_providerTimedPlaybackCapability);
}

ImageViewportPrivate::TriState ImageViewportPrivate::frameSeekSupportForSequence(
    ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return TriState::Unavailable;
    }
    if (sequence->isStill() || sequence->isTimedList()) {
        return TriState::True;
    }
    if (!sequence->isProvider()) {
        return TriState::Unavailable;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return providerResolvedCapability(sequence->m_providerFrameSeekCapability, true)
            ? TriState::True
            : TriState::False;
    }

    return capabilitySupportToTriState(sequence->m_providerFrameSeekCapability);
}

ImageViewportPrivate::TriState ImageViewportPrivate::positionSeekSupportForSequence(
    ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return TriState::Unavailable;
    }
    if (sequence->isTimedList()) {
        return TriState::True;
    }
    if (sequence->isStill()) {
        return TriState::False;
    }
    if (!sequence->isProvider()) {
        return TriState::Unavailable;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return providerResolvedCapability(sequence->m_providerPositionSeekCapability,
                   sequence->m_providerKnownFacts.isTimedFrameList())
            ? TriState::True
            : TriState::False;
    }

    return capabilitySupportToTriState(sequence->m_providerPositionSeekCapability);
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
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerTimedMetadata() ? controller.providerFrameCount() : 1;
    }
    if (hasProviderSequence() && !controller.providerMetadataReady()
        && controller.requestState().sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(
            controller.requestState().sequence->m_providerFrameSeekCapability)) {
        return controller.requestState().sequence->m_providerKnownFacts.frameCount();
    }
    if (hasDisplayableSequence()) {
        return controller.requestState().sequence->frameCount();
    }

    return -1;
}

int ImageViewportPrivate::totalDuration() const
{
    if (hasProviderSequence() && controller.providerTimedMetadata()) {
        return controller.providerTotalDuration();
    }
    if (hasTimedSequence()) {
        return controller.requestState().sequence->totalDuration();
    }

    return -1;
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
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        if (!controller.providerFrameSeekSupported()) {
            return invalidRange();
        }
        return ImageViewportRange(
            0, controller.providerTimedMetadata() ? controller.providerFrameCount() - 1 : 0);
    }
    if (hasProviderSequence() && !controller.providerMetadataReady()
        && controller.requestState().sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(
            controller.requestState().sequence->m_providerFrameSeekCapability)) {
        return ImageViewportRange(
            0, controller.requestState().sequence->m_providerKnownFacts.frameCount() - 1);
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return ImageViewportRange(0, controller.requestState().sequence->frameCount() - 1);
    }

    return invalidRange();
}

ImageViewportRange ImageViewportPrivate::positionSeekBounds() const
{
    if (hasProviderSequence() && controller.providerTimedMetadata()
        && controller.providerPositionSeekSupported()) {
        return ImageViewportRange(0, controller.providerTotalDuration());
    }
    if (hasTimedSequence()) {
        return ImageViewportRange(0, controller.requestState().sequence->totalDuration());
    }

    return invalidRange();
}

int ImageViewportPrivate::primaryFrameCount() const { return frameCount(); }

int ImageViewportPrivate::secondaryFrameCount() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderTimedMetadata()
            ? controller.secondaryProviderFrameCount()
            : 1;
    }
    return frameCountForSequence(secondarySequence());
}

int ImageViewportPrivate::primaryTotalDuration() const { return totalDuration(); }

int ImageViewportPrivate::secondaryTotalDuration() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderTimedMetadata()) {
        return controller.secondaryProviderTotalDuration();
    }
    return totalDurationForSequence(secondarySequence());
}

ImageViewportRange ImageViewportPrivate::primaryFrameSeekBounds() const
{
    return frameSeekBounds();
}

ImageViewportRange ImageViewportPrivate::secondaryFrameSeekBounds() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        if (!controller.secondaryProviderFrameSeekSupported()) {
            return invalidRange();
        }
        return ImageViewportRange(0,
            controller.secondaryProviderTimedMetadata()
                ? controller.secondaryProviderFrameCount() - 1
                : 0);
    }
    return frameSeekBoundsForSequence(secondarySequence());
}

ImageViewportRange ImageViewportPrivate::primaryPositionSeekBounds() const
{
    return positionSeekBounds();
}

ImageViewportRange ImageViewportPrivate::secondaryPositionSeekBounds() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderTimedMetadata()
        && controller.secondaryProviderPositionSeekSupported()) {
        return ImageViewportRange(0, controller.secondaryProviderTotalDuration());
    }
    return positionSeekBoundsForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::timedPlaybackSupport() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerTimedPlaybackSupported() ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(
            controller.requestState().sequence->m_providerTimedPlaybackCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::frameSeekSupport() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerFrameSeekSupported() ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(
            controller.requestState().sequence->m_providerFrameSeekCapability);
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return TriState::True;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::positionSeekSupport() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerPositionSeekSupported() ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(
            controller.requestState().sequence->m_providerPositionSeekCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryTimedPlaybackSupport() const
{
    return timedPlaybackSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryTimedPlaybackSupport() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderTimedPlaybackSupported() ? TriState::True
                                                                    : TriState::False;
    }
    return timedPlaybackSupportForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryFrameSeekSupport() const
{
    return frameSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryFrameSeekSupport() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderFrameSeekSupported() ? TriState::True : TriState::False;
    }
    return frameSeekSupportForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryPositionSeekSupport() const
{
    return positionSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryPositionSeekSupport() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderPositionSeekSupported() ? TriState::True
                                                                   : TriState::False;
    }
    return positionSeekSupportForSequence(secondarySequence());
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
    const QSizeF primarySize = primaryDisplayedImageSize();
    if (!isPositiveSize(primarySize)) {
        return QSizeF(0.0, 0.0);
    }

    const QSizeF secondarySize = secondaryDisplayedImageSize();
    if (!isPositiveSize(secondarySize)) {
        return primarySize;
    }

    return QSizeF(
        primarySize.width() + controller.presentationState().pageGap + secondarySize.width(),
        std::max(primarySize.height(), secondarySize.height()));
}

QSizeF ImageViewportPrivate::primaryDisplayedImageSize() const { return displayedImageSize(); }

QSizeF ImageViewportPrivate::secondaryDisplayedImageSize() const
{
    if (!hasReadyDisplay()) {
        return QSizeF(0.0, 0.0);
    }

    const QSizeF size = secondaryLogicalSize();
    return isPositiveSize(size) ? size : QSizeF(0.0, 0.0);
}

QSizeF ImageViewportPrivate::secondaryLogicalSize() const
{
    ImageSequence* sequence = secondarySequence();
    if (!sequence || !sequence->isValid()) {
        return {};
    }
    if (sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderLogicalSize();
    }
    if (!sequence->isProvider()) {
        return sequence->logicalSize();
    }

    return {};
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
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}
