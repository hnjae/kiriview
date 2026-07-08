#include "imageviewport_p.h"
#include "viewportcontrollermetadatacontract_p.h"

namespace {

bool positiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

ImageViewport::CapabilitySupport capabilitySupport(ImageViewport::TriState support)
{
    switch (support) {
    case ImageViewport::TriState::False:
        return ImageViewport::CapabilitySupport::False;
    case ImageViewport::TriState::True:
        return ImageViewport::CapabilitySupport::True;
    case ImageViewport::TriState::Unavailable:
        break;
    }
    return ImageViewport::CapabilitySupport::Unavailable;
}

QVariant roleVariant(ImageViewport::PageRole role) { return QVariant::fromValue(role); }

ImageViewport::DisplayPhase displayPhase(
    ImageViewport::DisplayStatus displayStatus, ImageViewport::RequestStatus requestStatus)
{
    switch (displayStatus) {
    case ImageViewport::DisplayStatus::Ready:
        return ImageViewport::DisplayPhase::CommittedActive;
    case ImageViewport::DisplayStatus::Retained:
        return ImageViewport::DisplayPhase::PreviousActive;
    case ImageViewport::DisplayStatus::Empty:
        break;
    }

    return requestStatus == ImageViewport::RequestStatus::NoRequest
        ? ImageViewport::DisplayPhase::NoPresentation
        : ImageViewport::DisplayPhase::Placeholder;
}

quint64 mixRevision(quint64 seed, quint64 value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

quint64 snapshotRevisionValue(
    quint64 requestRevision, quint64 displayRevision, quint64 commandRevision, quint64 generation)
{
    if (requestRevision == 0 && displayRevision == 0 && commandRevision == 0 && generation == 0) {
        return 0;
    }

    quint64 seed = 0xcbf29ce484222325ULL;
    seed = mixRevision(seed, requestRevision);
    seed = mixRevision(seed, displayRevision);
    seed = mixRevision(seed, commandRevision);
    seed = mixRevision(seed, generation);
    return seed == 0 ? 1 : seed;
}

} // namespace

ImageViewportStateSnapshot ImageViewportPrivate::state() const
{
    const auto& request = controller.requestState();
    const auto& display = controller.displayState();
    const ViewportEngine::PageSetState pageSetState = controller.pageSetState();
    const bool primaryPresent
        = pageSetState.acceptedRoleSet.primary() && pageSetState.pageSet.primary();
    const bool secondaryPresent
        = pageSetState.acceptedRoleSet.secondary() && pageSetState.pageSet.secondary();
    const ImageViewportRoleSet acceptedRoleSet(primaryPresent, secondaryPresent);
    const ImageViewportRoleSet targetRoleSet(primaryPresent && pageSetState.targetRoleSet.primary(),
        secondaryPresent && pageSetState.targetRoleSet.secondary());
    const ImageViewportPageSetGenerationToken acceptedGeneration(
        primaryPresent ? pageSetState.generation : 0);
    const bool primaryDisplayed
        = positiveSize(display.displayedImageSize) && display.status != DisplayStatus::Empty;
    const bool secondaryDisplayed = positiveSize(display.secondaryDisplayedImageSize)
        && display.status != DisplayStatus::Empty;
    const ImageViewportRoleSet displayedRoleSet(primaryDisplayed, secondaryDisplayed);
    const ImageViewportPageSetGenerationToken displayedGeneration(
        primaryDisplayed ? display.displayedRequest.generation : 0);
    const bool displayedBelongsToAcceptedPageSet
        = primaryDisplayed && display.displayedRequest.generation == pageSetState.generation;
    const ImageViewportRevisionToken requestRevision(request.requestRevision);
    const ImageViewportRevisionToken displayRevision(display.revision);
    const ImageViewportRevisionToken commandRevision(request.commandRevision);
    const ImageViewportRevisionToken presentationRevision(display.revision);
    const ImageViewportRevisionToken snapshotRevision(snapshotRevisionValue(request.requestRevision,
        display.revision, request.commandRevision, pageSetState.generation));

    QVariant activeRole;
    if (primaryPresent) {
        activeRole = roleVariant(PageRole::Primary);
    }
    QVariant playbackRole;
    if (request.playbackPhase != PlaybackPhase::Stopped && primaryPresent) {
        playbackRole = roleVariant(request.playbackRole);
    }

    const ImageViewportRequestSnapshot requestSnapshot(request.status, request.reason,
        request.playbackPhase, acceptedGeneration, acceptedRoleSet, targetRoleSet, activeRole,
        playbackRole);
    const ImageViewportDisplaySnapshot displaySnapshot(display.status,
        displayPhase(display.status, request.status), displayedGeneration, displayedRoleSet,
        targetRoleSet, displayedBelongsToAcceptedPageSet, display.status == DisplayStatus::Retained,
        primaryDisplayed ? displayRevision : ImageViewportRevisionToken {}, presentationRevision,
        displayedSpreadSize(), contentRect(), contentSize(), contentPosition(),
        maximumContentPosition(), visibleSpreadRect(), horizontalPannable(), verticalPannable());
    const ImageViewportPresentationSnapshot presentationSnapshot(fitMode(), zoomPercent(),
        minimumManualZoomPercent(), maximumManualZoomPercent(), manualZoomStepFactor(),
        rotationDegrees(), mirrorHorizontally(), mirrorVertically(), spreadDirection(), pageGap(),
        backgroundMode(), backgroundColor(), smoothing(), mipmap(), looping(),
        controller.presentationState().qualityPreference,
        controller.presentationState().exactnessPreference);

    const auto roleSnapshot = [this, &request, &display, acceptedGeneration, &pageSetState](
                                  PageRole role) -> ImageViewportRoleSnapshot {
        const bool primary = role == PageRole::Primary;
        ImageSequence* sequence
            = primary ? pageSetState.pageSet.primary() : pageSetState.pageSet.secondary();
        const bool present = sequence != nullptr;
        const bool roleDisplayed = display.status != DisplayStatus::Empty
            && positiveSize(
                primary ? display.displayedImageSize : display.secondaryDisplayedImageSize);
        const bool roleDisplayBelongsToAccepted
            = roleDisplayed && display.displayedRequest.generation == pageSetState.generation;
        const QSizeF requestLogicalSize = present
            ? (primary ? sequenceLogicalSize() : secondarySequenceLogicalSize())
            : QSizeF();
        const QSizeF displayedLogicalSize = roleDisplayed
            ? (primary ? display.displayedImageSize : display.secondaryDisplayedImageSize)
            : QSizeF();
        const int requestedFrame
            = primary ? this->primaryRequestedFrame() : this->secondaryRequestedFrame();
        const int requestedPosition
            = primary ? this->primaryRequestedPosition() : this->secondaryRequestedPosition();
        const int displayedFrame
            = primary ? this->primaryDisplayedFrame() : this->secondaryDisplayedFrame();
        const int displayedPosition
            = primary ? this->primaryDisplayedPosition() : this->secondaryDisplayedPosition();
        const auto metadata = controller.metadataProjection(role);
        const ImageSequenceAuthoredAnimationFacts animationFacts = primary
            ? sequenceAuthoredAnimationFacts()
            : secondarySequenceAuthoredAnimationFacts();
        const ImageViewport::CapabilitySupport frameSeekSupport
            = capabilitySupport(metadata.frameSeekSupport);
        const ImageViewport::CapabilitySupport positionSeekSupport
            = capabilitySupport(metadata.positionSeekSupport);
        const ImageViewport::CapabilitySupport timedPlaybackSupport
            = capabilitySupport(metadata.timedPlaybackSupport);
        const bool metadataAvailable
            = present && metadata.frameCount >= 0 && positiveSize(requestLogicalSize);
        const int loopCount
            = animationFacts.loopMode() == ImageSequenceAuthoredAnimationFacts::LoopMode::Finite
            ? animationFacts.loopCount()
            : -1;
        const QRectF rolePageRect = primary ? this->primaryPageRect() : this->secondaryPageRect();
        const QRectF roleItemRect = primary ? this->primaryItemRect() : this->secondaryItemRect();
        const QRectF roleVisiblePageRect
            = primary ? this->visiblePrimaryPageRect() : this->visibleSecondaryPageRect();
        const bool geometryAvailable = !rolePageRect.isEmpty();
        const QRectF acceptedPageRect = present && geometryAvailable ? rolePageRect : QRectF();
        const QRectF acceptedItemRect = present && geometryAvailable ? roleItemRect : QRectF();
        const QRectF acceptedVisiblePageRect
            = present && geometryAvailable ? roleVisiblePageRect : QRectF();
        const QRectF displayedPageRect
            = roleDisplayed && geometryAvailable ? rolePageRect : QRectF();
        const QRectF displayedItemRect
            = roleDisplayed && geometryAvailable ? roleItemRect : QRectF();
        const QRectF displayedVisiblePageRect
            = roleDisplayed && geometryAvailable ? roleVisiblePageRect : QRectF();

        return ImageViewportRoleSnapshot(present, sequence,
            ImageViewportRoleRequestSnapshot(present,
                present ? acceptedGeneration : ImageViewportPageSetGenerationToken {}, role,
                requestedFrame, requestedPosition, requestLogicalSize,
                ImageViewportDemandRevisionToken {}),
            ImageViewportRoleDisplaySnapshot(roleDisplayBelongsToAccepted,
                roleDisplayed && display.status == DisplayStatus::Retained, displayedFrame,
                displayedPosition, displayedLogicalSize, displayedLogicalSize,
                roleDisplayed ? QSizeF(1.0, 1.0) : QSizeF(),
                roleDisplayed ? ImageViewport::PayloadQuality::Exact
                              : ImageViewport::PayloadQuality::Unknown,
                roleDisplayed ? ImageViewport::PayloadExactness::ExactForSource
                              : ImageViewport::PayloadExactness::Unknown,
                false, ImageViewportDemandRevisionToken {}),
            ImageViewportRoleMetadataSnapshot(metadataAvailable, requestLogicalSize,
                metadata.frameCount, metadata.totalDuration, metadata.frameSeekBounds,
                metadata.positionSeekBounds, frameSeekSupport, positionSeekSupport,
                timedPlaybackSupport, animationFacts.autoplay(),
                animationFacts.progressiveAnimationReadiness(), animationFacts.loopMode(),
                loopCount),
            ImageViewportRoleGeometrySnapshot(acceptedPageRect, acceptedItemRect,
                acceptedVisiblePageRect, displayedPageRect, displayedItemRect,
                displayedVisiblePageRect));
    };

    return ImageViewportStateSnapshot(requestSnapshot, displaySnapshot, presentationSnapshot,
        roleSnapshot(PageRole::Primary), roleSnapshot(PageRole::Secondary),
        ImageViewportDiagnosticsSnapshot(errorString(), warningString(), commandReason()),
        ImageViewportRevisionsSnapshot(requestRevision, displayRevision, presentationRevision,
            commandRevision, snapshotRevision));
}

void ImageViewportPrivate::refreshStateSnapshot()
{
    const ImageViewportStateSnapshot currentSnapshot = state();
    if (currentSnapshot == lastStateSnapshot) {
        return;
    }
    lastStateSnapshot = currentSnapshot;
    emit q->stateChanged();
}
