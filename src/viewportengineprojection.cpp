#include "viewportengineprojection_p.h"

#include "imageviewportlimits_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"

#include <algorithm>

namespace {
using namespace ImageViewportInternal;

const ProviderFactsState& providerFor(
    ViewportEngineProviderFactsView facts, ImageViewportPageRole role)
{
    return facts[role == ImageViewportPageRole::Secondary ? 1U : 0U];
}

bool positive(QSizeF size) { return size.isValid() && size.width() > 0.0 && size.height() > 0.0; }

ImageViewportRevisionToken revision(quint64 value)
{
    return RevisionTokenPrivateAccess::publicRevisionFromValue(value);
}

ImageViewportPresentationTargetGenerationToken generation(quint64 value)
{
    return RevisionTokenPrivateAccess::generationFromValue(value);
}

ImageViewportDisplayPhase displayPhase(
    ImageViewportDisplayStatus display, ImageViewportRequestStatus request)
{
    if (display == ImageViewportDisplayStatus::Ready) {
        return ImageViewportDisplayPhase::CommittedActive;
    }
    if (display == ImageViewportDisplayStatus::Retained) {
        return ImageViewportDisplayPhase::PreviousActive;
    }
    return request == ImageViewportRequestStatus::NoRequest
        ? ImageViewportDisplayPhase::NoPresentation
        : ImageViewportDisplayPhase::TransitioningPlaceholder;
}

quint64 mix(quint64 seed, quint64 value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

quint64 snapshotRevision(const RequestState& request, const DisplayState& display,
    quint64 presentationRevision, quint64 commandRevision, quint64 generationValue)
{
    if (request.requestRevision == 0 && display.revision == 0 && presentationRevision == 0
        && commandRevision == 0 && generationValue == 0) {
        return 0;
    }
    quint64 seed = 0xcbf29ce484222325ULL;
    seed = mix(seed, request.requestRevision);
    seed = mix(seed, display.revision);
    seed = mix(seed, presentationRevision);
    seed = mix(seed, commandRevision);
    seed = mix(seed, generationValue);
    return seed == 0 ? 1 : seed;
}

const ImageSequenceSource& sourceForRole(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? request.roles[0].source
                                                  : request.roles[1].source;
}

const DisplayRequest& requestForRole(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? request.roles[0].activeRequest
                                                  : request.roles[1].activeRequest;
}

const DisplayRequestSnapshot& displayedRequestForRole(
    const DisplayState& display, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? display.roles[0].displayedRequest
                                                  : display.roles[1].displayedRequest;
}

QSizeF displayedSizeForRole(const DisplayState& display, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? display.roles[0].displayedImageSize
                                                  : display.roles[1].displayedImageSize;
}

const PreparedPayload& displayedPayloadForRole(
    const DisplayState& display, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? display.roles[0].displayedPayload
                                                  : display.roles[1].displayedPayload;
}

struct Metadata
{
    int frameCount = -1;
    int totalDuration = -1;
    ImageViewportRange frameBounds;
    ImageViewportRange positionBounds;
    ImageViewportCapabilitySupport frameSeek = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport positionSeek = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport playback = ImageViewportCapabilitySupport::Unavailable;
};

ImageViewportCapabilitySupport support(bool value)
{
    return value ? ImageViewportCapabilitySupport::True : ImageViewportCapabilitySupport::False;
}

Metadata metadataFor(const RequestState& request, const ProviderFactsState& primaryProvider,
    const ProviderFactsState& secondaryProvider, ImageViewportPageRole role)
{
    const auto& source = sourceForRole(request, role);
    if (!source.facts.present) {
        return {};
    }
    if (!source.facts.provider) {
        Metadata result;
        result.frameCount = source.facts.frameCount;
        result.totalDuration = source.facts.timed ? source.facts.totalDuration : -1;
        result.frameBounds = result.frameCount > 0 ? ImageViewportRange(0, result.frameCount - 1)
                                                   : ImageViewportRange();
        result.positionBounds = source.facts.timed && result.totalDuration >= 0
            ? ImageViewportRange(0, result.totalDuration)
            : ImageViewportRange();
        result.frameSeek = ImageViewportCapabilitySupport::True;
        result.positionSeek = support(source.facts.timed);
        result.playback = support(source.facts.timed);
        return result;
    }
    const auto& provider
        = role == ImageViewportPageRole::Primary ? primaryProvider : secondaryProvider;
    Metadata result;
    if (provider.metadataReady) {
        result.frameCount = provider.timedMetadata ? provider.timingIntervals.frameCount() : 1;
        result.totalDuration
            = provider.timedMetadata ? provider.timingIntervals.totalDuration() : -1;
        result.frameSeek = support(provider.frameSeekSupport);
        result.positionSeek = support(provider.positionSeekSupport);
        result.playback = support(provider.timedPlaybackSupport);
    } else {
        result.frameSeek = providerCapabilitySupport(source.facts.providerFrameSeekCapability);
        result.positionSeek
            = providerCapabilitySupport(source.facts.providerPositionSeekCapability);
        result.playback = providerCapabilitySupport(source.facts.providerTimedPlaybackCapability);
        if (source.facts.providerKnownFacts.isStill()) {
            result.frameCount = 1;
            result.frameSeek = ImageViewportCapabilitySupport::True;
            result.positionSeek = ImageViewportCapabilitySupport::False;
            result.playback = ImageViewportCapabilitySupport::False;
        } else if (source.facts.providerKnownFacts.isTimedFrameList()) {
            const TimingIntervals timing = TimingIntervals::fromFrameDurations(
                source.facts.providerKnownFacts.frameDurations());
            result.frameCount = timing.frameCount();
            result.totalDuration = timing.totalDuration();
            result.frameSeek = ImageViewportCapabilitySupport::True;
            result.positionSeek = ImageViewportCapabilitySupport::True;
            result.playback = ImageViewportCapabilitySupport::True;
        } else if (source.facts.providerKnownFacts.isTimedFrameCount()) {
            result.frameCount = source.facts.providerKnownFacts.frameCount();
        }
    }
    if (result.frameCount > 0 && result.frameSeek == ImageViewportCapabilitySupport::True) {
        result.frameBounds = ImageViewportRange(0, result.frameCount - 1);
    }
    if (result.totalDuration >= 0 && result.positionSeek == ImageViewportCapabilitySupport::True) {
        result.positionBounds = ImageViewportRange(0, result.totalDuration);
    }
    return result;
}

double effectiveZoomPercent(const PresentationGeometry::State& geometry)
{
    const QSizeF spread = PresentationGeometry::spreadSize(geometry);
    const QRectF content = PresentationGeometry::contentRect(geometry);
    if (!positive(spread) || content.isEmpty()) {
        return 0.0;
    }
    const int rotation = ((geometry.rotationDegrees % 360) + 360) % 360;
    const QSizeF oriented
        = rotation == 90 || rotation == 270 ? QSizeF(spread.height(), spread.width()) : spread;
    return content.width() / oriented.width() * geometry.devicePixelRatio * 100.0;
}
}

PresentationGeometry::State projectViewportGeometryState(const ViewportEngineGeometryInput& input,
    const ImageViewportInternal::PresentationState& presentation)
{
    return { input.primaryPresent, input.itemBounds, input.primarySize, input.secondarySize,
        presentation.pageGap, presentation.spreadDirection, presentation.fitMode,
        presentation.rotationDegrees, presentation.mirrorHorizontally,
        presentation.mirrorVertically, presentation.manualZoom,
        input.devicePixelRatio > 0.0 ? input.devicePixelRatio : 1.0, presentation.contentPosition };
}

ImageViewportStateSnapshot projectViewportStateSnapshot(
    ViewportEngineSnapshotInput input, ViewportEngineSnapshotStateAccess access)
{
    const bool primaryPresent = access.request().roles[0].source.facts.present;
    const bool secondaryPresent = access.request().roles[1].source.facts.present;
    const ImageViewportRoleSet acceptedRoles(primaryPresent, secondaryPresent);
    const ImageViewportRoleSet targetRoles = acceptedRoles;
    const quint64 acceptedGenerationValue = access.presentationTarget().generation != 0
        ? access.presentationTarget().generation
        : access.request().sequenceGeneration;
    const auto acceptedGeneration = generation(primaryPresent ? acceptedGenerationValue : 0);
    const bool primaryDisplayed = access.display().status != ImageViewportDisplayStatus::Empty
        && positive(access.display().roles[0].displayedImageSize);
    const bool secondaryDisplayed = access.display().status != ImageViewportDisplayStatus::Empty
        && positive(access.display().roles[1].displayedImageSize);
    const ImageViewportRoleSet displayedRoles(primaryDisplayed, secondaryDisplayed);
    const quint64 displayedGenerationValue
        = primaryDisplayed ? access.display().roles[0].displayedRequest.generation : 0;
    const auto roleTargetMatches = [&](ImageViewportPageRole role) {
        const auto& displayed = displayedRequestForRole(access.display(), role);
        const auto& active = requestForRole(access.request(), role);
        return displayed.generation == acceptedGenerationValue
            && displayed.request.resolvedFrame.frame == active.resolvedFrame.frame
            && displayed.request.resolvedFrame.position == active.resolvedFrame.position;
    };
    const PresentationGeometry::State acceptedGeometry
        = projectViewportGeometryState(input.acceptedGeometry, access.presentation());
    const PresentationGeometry::State displayedGeometry
        = projectViewportGeometryState(input.displayedGeometry,
            access.display().status == ImageViewportDisplayStatus::Retained
                ? access.display().displayedPresentation
                : access.presentation());
    const quint64 targetPresentationRevisionValue
        = primaryPresent ? access.targetPresentationRevision() : 0;
    const quint64 displayedPresentationRevisionValue = primaryDisplayed
        ? (access.display().displayedPresentationRevision != 0
                  ? access.display().displayedPresentationRevision
                  : access.display().revision)
        : 0;
    const bool displayAccepted = access.display().status == ImageViewportDisplayStatus::Ready
        && displayedRoles == acceptedRoles && primaryDisplayed
        && roleTargetMatches(ImageViewportPageRole::Primary)
        && (!acceptedRoles.secondary() || roleTargetMatches(ImageViewportPageRole::Secondary))
        && displayedPresentationRevisionValue != 0
        && displayedPresentationRevisionValue == targetPresentationRevisionValue;
    const quint64 commandRevisionValue = access.publishedCommandRevision() != 0
        ? access.publishedCommandRevision()
        : RevisionTokenPrivateAccess::value(access.commandRevision());
    const quint64 snapshotRevisionValue = access.snapshotRevision() != 0
        ? access.snapshotRevision()
        : snapshotRevision(access.request(), access.display(), access.presentationRevision(),
              commandRevisionValue, acceptedGenerationValue);

    QVariant playbackRole;
    if (access.playback().phase != ImageViewportPlaybackPhase::Stopped && primaryPresent) {
        playbackRole = QVariant::fromValue(access.playback().role);
    }

    const ImageViewportRequestSnapshot requestSnapshot(access.request().status,
        access.request().reason, access.playback().phase, acceptedGeneration, acceptedRoles,
        playbackRole);
    const QSizeF displayedSpreadSize = PresentationGeometry::spreadSize(displayedGeometry);
    const ImageViewportDisplaySnapshot displaySnapshot(access.display().status,
        displayPhase(access.display().status, access.request().status),
        generation(displayedGenerationValue), displayedRoles, targetRoles, displayAccepted,
        access.display().status == ImageViewportDisplayStatus::Retained,
        revision(displayedPresentationRevisionValue), revision(targetPresentationRevisionValue),
        positive(displayedSpreadSize) ? displayedSpreadSize : QSizeF(0.0, 0.0),
        PresentationGeometry::contentRect(displayedGeometry),
        PresentationGeometry::contentSize(displayedGeometry),
        PresentationGeometry::contentPosition(displayedGeometry),
        PresentationGeometry::maximumContentPosition(displayedGeometry),
        PresentationGeometry::visibleSpreadRect(displayedGeometry),
        PresentationGeometry::horizontalPannable(displayedGeometry),
        PresentationGeometry::verticalPannable(displayedGeometry));
    const ImageViewportPresentationSnapshot presentationSnapshot(access.presentation().fitMode,
        effectiveZoomPercent(acceptedGeometry), access.presentation().manualZoom * 100.0,
        ImageViewportDisplayLimits::minimumManualZoomPercent(),
        ImageViewportDisplayLimits::maximumManualZoomPercent(),
        ImageViewportDisplayLimits::manualZoomStepFactor(), access.presentation().rotationDegrees,
        access.presentation().mirrorHorizontally, access.presentation().mirrorVertically,
        access.presentation().spreadDirection, access.presentation().pageGap,
        access.presentation().backgroundMode, access.presentation().backgroundColor,
        access.presentation().checkerboardLightColor, access.presentation().checkerboardDarkColor,
        access.presentation().checkerboardCellSize, access.presentation().smoothing,
        access.presentation().mipmap, access.playback().looping,
        access.presentation().qualityPreference, access.presentation().exactnessPreference);

    const auto roleSnapshot = [&](ImageViewportPageRole role) {
        const auto& source = sourceForRole(access.request(), role);
        const auto& active = requestForRole(access.request(), role);
        const auto& displayedRequest = displayedRequestForRole(access.display(), role);
        const QSizeF displayedSize = displayedSizeForRole(access.display(), role);
        const bool present = source.facts.present;
        const bool displayed = access.display().status != ImageViewportDisplayStatus::Empty
            && positive(displayedSize);
        const bool belongs = displayed && displayAccepted;
        const Metadata metadata = metadataFor(access.request(),
            providerFor(access.providerFacts(), ImageViewportPageRole::Primary),
            providerFor(access.providerFacts(), ImageViewportPageRole::Secondary), role);
        const QSizeF logicalSize = source.facts.provider
            ? (providerFor(access.providerFacts(), role).metadataReady
                      ? providerFor(access.providerFacts(), role).logicalSize
                      : source.facts.providerKnownLogicalSize)
            : sourceLogicalSize(source);
        const auto& payload = displayedPayloadForRole(access.display(), role);
        const QSizeF payloadRaster
            = positive(payload.payloadRasterSize) ? payload.payloadRasterSize : displayedSize;
        const QSizeF sourceScale = positive(payload.sourceToPayloadScale)
            ? payload.sourceToPayloadScale
            : (displayed ? QSizeF(1.0, 1.0) : QSizeF());
        const QRectF acceptedPageRect = role == ImageViewportPageRole::Primary
            ? PresentationGeometry::primaryPageRect(acceptedGeometry)
            : PresentationGeometry::secondaryPageRect(acceptedGeometry);
        const QRectF acceptedItemRect = PresentationGeometry::pageItemRect(acceptedGeometry, role);
        const QRectF acceptedVisibleRect
            = PresentationGeometry::visiblePageRect(acceptedGeometry, role);
        const QRectF displayedPageRect = role == ImageViewportPageRole::Primary
            ? PresentationGeometry::primaryPageRect(displayedGeometry)
            : PresentationGeometry::secondaryPageRect(displayedGeometry);
        const QRectF displayedItemRect
            = PresentationGeometry::pageItemRect(displayedGeometry, role);
        const QRectF displayedVisibleRect
            = PresentationGeometry::visiblePageRect(displayedGeometry, role);
        const bool acceptedGeometryAvailable
            = !input.acceptedGeometry.itemBounds.isEmpty() && !acceptedPageRect.isEmpty();
        const bool displayedGeometryAvailable
            = !input.displayedGeometry.itemBounds.isEmpty() && !displayedPageRect.isEmpty();
        const auto animation
            = source.facts.provider && providerFor(access.providerFacts(), role).metadataReady
            ? providerFor(access.providerFacts(), role).authoredAnimationFacts
            : source.facts.authoredAnimationFacts;
        const bool animationAvailable = present
            && (source.facts.provider && providerFor(access.providerFacts(), role).metadataReady
                    ? providerFor(access.providerFacts(), role).authoredAnimationFactsAvailable
                    : source.facts.authoredAnimationFactsAvailable);
        const ImageViewportCapabilitySupport autoplay = animationAvailable
            ? support(animation.autoplay())
            : ImageViewportCapabilitySupport::Unavailable;
        const ImageSequenceAuthoredAnimationLoopMode loopMode = animationAvailable
            ? animation.loopMode()
            : ImageSequenceAuthoredAnimationLoopMode::Unavailable;
        const int loopCount = loopMode == ImageSequenceAuthoredAnimationLoopMode::PlayOnce ? 1
            : loopMode == ImageSequenceAuthoredAnimationLoopMode::Finite ? animation.loopCount()
                                                                         : -1;
        const bool metadataAvailable = present
            && (!source.facts.provider || providerFor(access.providerFacts(), role).metadataReady
                || source.facts.providerKnownFacts.isSpecified()
                || source.facts.providerTimedPlaybackCapability
                    != ImageSequenceProviderCapabilitySupport::Unavailable
                || source.facts.providerFrameSeekCapability
                    != ImageSequenceProviderCapabilitySupport::Unavailable
                || source.facts.providerPositionSeekCapability
                    != ImageSequenceProviderCapabilitySupport::Unavailable
                || source.facts.authoredAnimationFactsAvailable);
        const bool targetMatches = displayed
            && displayedRequest.generation == acceptedGenerationValue
            && displayedRequest.request.resolvedFrame.frame == active.resolvedFrame.frame
            && displayedRequest.request.resolvedFrame.position == active.resolvedFrame.position;
        const bool currentForDemand = targetMatches
            && (!source.facts.provider
                || (payload.demandRevision.isValid()
                    && payload.demandRevision == active.demandRevision));
        return ImageViewportRoleSnapshot(present, source.sequence,
            ImageViewportRoleRequestSnapshot(present,
                present ? acceptedGeneration : ImageViewportPresentationTargetGenerationToken {},
                role, active.target.frame, active.target.position, logicalSize,
                active.demandRevision),
            ImageViewportRoleDisplaySnapshot(belongs,
                displayed && access.display().status == ImageViewportDisplayStatus::Retained,
                displayed ? displayedRequest.request.resolvedFrame.frame : -1,
                displayed ? displayedRequest.request.resolvedFrame.position : -1,
                displayed ? displayedSize : QSizeF(), displayed ? payloadRaster : QSizeF(),
                displayed ? sourceScale : QSizeF(),
                displayed ? (payload.quality == ImageViewportPayloadQuality::Unknown
                                    ? ImageViewportPayloadQuality::Exact
                                    : payload.quality)
                          : ImageViewportPayloadQuality::Unknown,
                displayed ? (payload.exactness == ImageViewportPayloadExactness::Unknown
                                    ? ImageViewportPayloadExactness::ExactForSource
                                    : payload.exactness)
                          : ImageViewportPayloadExactness::Unknown,
                currentForDemand, payload.demandRevision),
            ImageViewportRoleMetadataSnapshot(metadataAvailable, logicalSize, metadata.frameCount,
                metadata.totalDuration, metadata.frameBounds, metadata.positionBounds,
                metadata.frameSeek, metadata.positionSeek, metadata.playback, autoplay, loopMode,
                loopCount),
            ImageViewportRoleGeometrySnapshot(
                present && acceptedGeometryAvailable ? acceptedPageRect : QRectF(),
                present && acceptedGeometryAvailable ? acceptedItemRect : QRectF(),
                present && acceptedGeometryAvailable ? acceptedVisibleRect : QRectF(),
                displayed && displayedGeometryAvailable ? displayedPageRect : QRectF(),
                displayed && displayedGeometryAvailable ? displayedItemRect : QRectF(),
                displayed && displayedGeometryAvailable ? displayedVisibleRect : QRectF()));
    };

    return ImageViewportStateSnapshot(requestSnapshot, displaySnapshot, presentationSnapshot,
        roleSnapshot(ImageViewportPageRole::Primary),
        roleSnapshot(ImageViewportPageRole::Secondary),
        ImageViewportDiagnosticsSnapshot(
            access.request().errorString, access.request().warningString, access.commandReason()),
        ImageViewportRevisionsSnapshot(revision(access.request().requestRevision),
            revision(access.display().revision), revision(access.presentationRevision()),
            revision(commandRevisionValue), revision(snapshotRevisionValue)));
}

ViewportEngineGeometryInput projectViewportCurrentGeometry(
    ViewportEngineGeometryQueryInput input, ViewportEngineCurrentGeometryProjectionAccess access)
{
    const bool present = access.request().roles[0].source.facts.present;
    const bool ready = access.display().hasReadyDisplay(present);
    const QSizeF primary = ready ? access.display().roles[0].displayedImageSize : QSizeF {};
    const QSizeF secondary = ready ? access.display().roles[1].displayedImageSize : QSizeF {};
    return { positive(primary), input.itemBounds, primary, secondary,
        input.devicePixelRatio > 0.0 ? input.devicePixelRatio : 1.0 };
}

ViewportEngineGeometryInput projectViewportPendingGeometry(
    ViewportEngineGeometryQueryInput input, ViewportEnginePendingGeometryProjectionAccess access)
{
    const auto& request = access.request();
    const auto& display = access.display();
    const bool ready = display.hasReadyDisplay(request.roles[0].source.facts.present);
    QSizeF primary = ready ? display.roles[0].displayedImageSize : QSizeF {};
    QSizeF secondary = ready ? display.roles[1].displayedImageSize : QSizeF {};
    const auto imageSize = [](const QImage& image) {
        return image.isNull() ? QSizeF() : image.deviceIndependentSize();
    };
    if (request.roles[0].source.facts.provider && positive(access.providerFacts()[0].logicalSize))
        primary = access.providerFacts()[0].logicalSize;
    else if (positive(imageSize(display.roles[0].pendingRenderPayload.image)))
        primary = imageSize(display.roles[0].pendingRenderPayload.image);
    if (!request.roles[1].sequence || request.roles[1].activeRequest.target.frame < 0)
        secondary = {};
    else if (request.roles[1].provider && positive(access.providerFacts()[1].logicalSize))
        secondary = access.providerFacts()[1].logicalSize;
    else if (positive(imageSize(display.roles[1].pendingRenderPayload.image)))
        secondary = imageSize(display.roles[1].pendingRenderPayload.image);
    return { positive(primary), input.itemBounds, primary, secondary,
        input.devicePixelRatio > 0.0 ? input.devicePixelRatio : 1.0 };
}

ViewportEngineGeometryInput projectViewportAcceptedGeometry(
    ViewportEngineGeometryQueryInput input, ViewportEngineAcceptedGeometryProjectionAccess access)
{
    const auto& request = access.request();
    QSizeF primary = request.roles[0].source.facts.provider
        ? access.providerFacts()[0].logicalSize
        : sourceLogicalSize(request.roles[0].source);
    QSizeF secondary;
    if (request.roles[1].sequence) {
        if (request.roles[1].provider)
            secondary = access.providerFacts()[1].logicalSize;
        else if (request.roles[1].activeRequest.target.frame >= 0)
            secondary = sourceLogicalSize(request.roles[1].source);
    }
    if (!positive(primary))
        primary = {};
    if (!positive(secondary))
        secondary = {};
    return { positive(primary), input.itemBounds, primary, secondary,
        input.devicePixelRatio > 0.0 ? input.devicePixelRatio : 1.0 };
}

ViewportRenderSnapshot projectViewportRenderSnapshot(
    ViewportRenderSnapshotInput input, ViewportEngineRenderSnapshotProjectionAccess access)
{
    const auto target = [&](ImageViewportPageRole role) {
        return PresentationGeometry::pageItemRect(input.geometryState, role)
            .intersected(input.geometryState.itemBounds);
    };
    const auto source = [&](ImageViewportPageRole role) {
        return PresentationGeometry::visiblePageRect(input.geometryState, role);
    };
    auto primary = input.preparedPayload;
    if (primary.image.isNull()
        && access.display().hasReadyDisplay(access.request().roles[0].source.facts.present)) {
        primary = access.display().roles[0].displayedPayload;
        primary.image = access.display().roles[0].displayedImage;
    }
    auto secondary = primary;
    if (access.display().roles[1].pendingRenderPayload.commitPending
        && !access.display().roles[1].pendingRenderPayload.image.isNull())
        secondary = access.display().roles[1].pendingRenderPayload;
    else
        secondary.image = access.display().roles[1].displayedImage;
    primary.providerFrameLeaseId = 0;
    secondary.providerFrameLeaseId = 0;
    ViewportRenderSnapshot snapshot;
    snapshot.itemSize = input.itemSize;
    snapshot.backgroundMode = access.presentation().backgroundMode;
    snapshot.backgroundColor = access.presentation().backgroundColor;
    snapshot.checkerboardLightColor = access.presentation().checkerboardLightColor;
    snapshot.checkerboardDarkColor = access.presentation().checkerboardDarkColor;
    snapshot.checkerboardCellSize = access.presentation().checkerboardCellSize;
    snapshot.smoothing = access.presentation().smoothing;
    snapshot.mipmap = access.presentation().mipmap;
    const auto append = [&](ImageViewportPageRole role, const auto& payload, bool requireRects) {
        const QRectF targetRect = target(role);
        const QRectF sourceRect = source(role);
        if (!payload.image.isNull()
            && (!requireRects || (!targetRect.isEmpty() && !sourceRect.isEmpty())))
            snapshot.imageLayers.append({ role, payload, targetRect, sourceRect,
                access.presentation().rotationDegrees, access.presentation().mirrorHorizontally,
                access.presentation().mirrorVertically });
    };
    append(ImageViewportPageRole::Primary, primary, true);
    append(ImageViewportPageRole::Secondary, secondary, true);
    return snapshot;
}
