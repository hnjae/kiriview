#include "imagesequenceownership_p.h"
#include "imageviewport_p.h"

#include <utility>

using namespace ImageViewportInternal;

namespace {
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

QVariantMap ImageViewportPrivate::frameSeekBoundsForSequence(ImageSequence* sequence) const
{
    if (frameSeekSupportForSequence(sequence) != TriState::True) {
        return invalidRange();
    }

    const int sequenceFrameCount = frameCountForSequence(sequence);
    if (sequenceFrameCount <= 0) {
        return invalidRange();
    }

    return {
        { QStringLiteral("minimum"), 0 },
        { QStringLiteral("maximum"), sequenceFrameCount - 1 },
    };
}

QVariantMap ImageViewportPrivate::positionSeekBoundsForSequence(ImageSequence* sequence) const
{
    if (positionSeekSupportForSequence(sequence) != TriState::True) {
        return invalidRange();
    }

    const int sequenceTotalDuration = totalDurationForSequence(sequence);
    if (sequenceTotalDuration < 0) {
        return invalidRange();
    }

    return {
        { QStringLiteral("minimum"), 0 },
        { QStringLiteral("maximum"), sequenceTotalDuration },
    };
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
    return presentation.spreadDirection;
}

void ImageViewportPrivate::setSpreadDirectionProperty(SpreadDirection direction)
{
    setSpreadDirection(direction);
}

double ImageViewportPrivate::pageGap() const { return presentation.pageGap; }

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

int ImageViewportPrivate::secondaryDisplayedFrame() const { return -1; }

int ImageViewportPrivate::secondaryRequestedFrame() const { return -1; }

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

int ImageViewportPrivate::secondaryDisplayedPosition() const { return -1; }

int ImageViewportPrivate::secondaryRequestedPosition() const { return -1; }

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

QVariantMap ImageViewportPrivate::frameSeekBounds() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        if (!controller.providerFrameSeekSupported()) {
            return invalidRange();
        }
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"),
                controller.providerTimedMetadata() ? controller.providerFrameCount() - 1 : 0 },
        };
    }
    if (hasProviderSequence() && !controller.providerMetadataReady()
        && controller.requestState().sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(
            controller.requestState().sequence->m_providerFrameSeekCapability)) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"),
                controller.requestState().sequence->m_providerKnownFacts.frameCount() - 1 },
        };
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), controller.requestState().sequence->frameCount() - 1 },
        };
    }

    return invalidRange();
}

QVariantMap ImageViewportPrivate::positionSeekBounds() const
{
    if (hasProviderSequence() && controller.providerTimedMetadata()
        && controller.providerPositionSeekSupported()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), controller.providerTotalDuration() },
        };
    }
    if (hasTimedSequence()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), controller.requestState().sequence->totalDuration() },
        };
    }

    return invalidRange();
}

int ImageViewportPrivate::primaryFrameCount() const { return frameCount(); }

int ImageViewportPrivate::secondaryFrameCount() const
{
    return frameCountForSequence(secondarySequence());
}

int ImageViewportPrivate::primaryTotalDuration() const { return totalDuration(); }

int ImageViewportPrivate::secondaryTotalDuration() const
{
    return totalDurationForSequence(secondarySequence());
}

QVariantMap ImageViewportPrivate::primaryFrameSeekBounds() const { return frameSeekBounds(); }

QVariantMap ImageViewportPrivate::secondaryFrameSeekBounds() const
{
    return frameSeekBoundsForSequence(secondarySequence());
}

QVariantMap ImageViewportPrivate::primaryPositionSeekBounds() const { return positionSeekBounds(); }

QVariantMap ImageViewportPrivate::secondaryPositionSeekBounds() const
{
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
    return timedPlaybackSupportForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryFrameSeekSupport() const
{
    return frameSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryFrameSeekSupport() const
{
    return frameSeekSupportForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryPositionSeekSupport() const
{
    return positionSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryPositionSeekSupport() const
{
    return positionSeekSupportForSequence(secondarySequence());
}

QSizeF ImageViewportPrivate::displayedImageSize() const
{
    if (hasReadyDisplay()) {
        return controller.displayState().displayedImageSize;
    }

    return QSizeF(0.0, 0.0);
}

QSizeF ImageViewportPrivate::displayedSpreadSize() const { return displayedImageSize(); }

QSizeF ImageViewportPrivate::primaryDisplayedImageSize() const { return displayedImageSize(); }

QSizeF ImageViewportPrivate::secondaryDisplayedImageSize() const { return QSizeF(0.0, 0.0); }

uint ImageViewportPrivate::displayRevision() const { return controller.displayState().revision; }

uint ImageViewportPrivate::requestRevision() const
{
    return controller.requestState().requestRevision;
}

uint ImageViewportPrivate::commandRevision() const
{
    return controller.requestState().commandRevision;
}

QString ImageViewportPrivate::errorString() const { return controller.requestState().errorString; }

QString ImageViewportPrivate::warningString() const
{
    return controller.requestState().warningString;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageSet(
    const QVariant& primary, const QVariant& secondary, const QVariant&)
{
    bool primaryValid = false;
    bool secondaryValid = false;
    ImageSequence* primarySequence = sequenceFromPageSetValue(primary, primaryValid);
    ImageSequence* secondarySequence = sequenceFromPageSetValue(secondary, secondaryValid);

    if (!primaryValid || !secondaryValid) {
        return CommandOutcome::Invalid;
    }

    if (!primarySequence) {
        return clear();
    }

    std::shared_ptr<ImageSequence> primaryOwner = factorySequenceOwner(primarySequence);
    std::shared_ptr<ImageSequence> secondaryOwner = factorySequenceOwner(secondarySequence);
    ViewportSequenceAssignmentResult result = controller.assignSequence({ primarySequence,
        std::move(primaryOwner), secondarySequence, std::move(secondaryOwner) });
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    if (result.openProviderSession && !openProviderSession()) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return CommandOutcome::Accepted;
}
