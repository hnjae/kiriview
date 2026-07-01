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
    if (!controller.requestState().sequence && !sequence) {
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

ImageSequence* ImageViewportPrivate::secondarySequence() const { return nullptr; }

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

int ImageViewportPrivate::secondaryFrameCount() const { return -1; }

int ImageViewportPrivate::primaryTotalDuration() const { return totalDuration(); }

int ImageViewportPrivate::secondaryTotalDuration() const { return -1; }

QVariantMap ImageViewportPrivate::primaryFrameSeekBounds() const { return frameSeekBounds(); }

QVariantMap ImageViewportPrivate::secondaryFrameSeekBounds() const { return invalidRange(); }

QVariantMap ImageViewportPrivate::primaryPositionSeekBounds() const { return positionSeekBounds(); }

QVariantMap ImageViewportPrivate::secondaryPositionSeekBounds() const { return invalidRange(); }

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
    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryFrameSeekSupport() const
{
    return frameSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryFrameSeekSupport() const
{
    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryPositionSeekSupport() const
{
    return positionSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryPositionSeekSupport() const
{
    return TriState::Unavailable;
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
    sequenceFromPageSetValue(secondary, secondaryValid);

    if (!primaryValid || !secondaryValid) {
        return CommandOutcome::Invalid;
    }

    if (!primarySequence) {
        return clear();
    }

    setSequence(primarySequence);
    return CommandOutcome::Accepted;
}
