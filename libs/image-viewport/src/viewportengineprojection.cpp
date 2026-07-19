// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineprojection_p.h"

#include "imageviewportlimits_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"
#include "viewportenginetargetspreadterminaloperations_p.h"

#include <algorithm>
#include <cmath>

namespace {
using namespace ImageViewportInternal;

const ProviderFactsState& providerFor(
    ViewportEngineProviderFactsView facts, ImageViewportPageRole role)
{
    return facts[role == ImageViewportPageRole::Secondary ? 1U : 0U];
}

bool positive(QSizeF size)
{
    return std::isfinite(size.width()) && std::isfinite(size.height()) && size.width() > 0.0
        && size.height() > 0.0;
}

bool finiteRect(const QRectF& rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y()) && std::isfinite(rect.width())
        && std::isfinite(rect.height()) && std::isfinite(rect.right())
        && std::isfinite(rect.bottom());
}

QRectF finiteRectOrEmpty(QRectF rect) { return finiteRect(rect) ? rect : QRectF {}; }

ImageViewportRevisionToken revision(quint64 value)
{
    return RevisionTokenPrivateAccess::publicRevisionFromValue(value);
}

ImageViewportPresentationTargetGenerationToken generation(quint64 value)
{
    return PresentationTargetGenerationTokenPrivateAccess::fromValue(value);
}

QString renderQualityFallbackWarning(
    const RequestState& request, const DisplayState& display, const PresentationState& presentation)
{
    return display.hasActiveRenderQualityFallback(request.sequenceGeneration, presentation)
        ? QStringLiteral("requested rendering quality is unavailable on the active backend")
        : QString();
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

ImageViewportFailureSnapshot failureSnapshot(const RequestState& request)
{
    const ViewportEngineProjectedTerminal projected = projectViewportEngineTerminal(request);
    if (!projected.terminal) {
        return {};
    }
    return { true, ImageViewportFailureContext::CurrentRequest, projected.terminal->reason,
        QVariant::fromValue(projected.role), projected.scope,
        projected.terminal->providerFailureAvailable, projected.terminal->providerCause,
        projected.terminal->providerReference };
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
    return role == ImageViewportPageRole::Primary
        ? display.roles[0].displayedPayload.sourceLogicalSize
        : display.roles[1].displayedPayload.sourceLogicalSize;
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
    if (!positive(spread) || content.isEmpty() || !finiteRect(content)) {
        return 0.0;
    }
    const int rotation = ((geometry.rotationDegrees % 360) + 360) % 360;
    const QSizeF oriented
        = rotation == 90 || rotation == 270 ? QSizeF(spread.height(), spread.width()) : spread;
    const double result = content.width() / oriented.width() * geometry.devicePixelRatio * 100.0;
    return std::isfinite(result) && result > 0.0 ? result : 0.0;
}

PresentationGeometry::State viewportGeometryState(const ViewportEngineGeometryInput& input,
    const PresentationState& presentation, double effectiveManualZoom)
{
    return { input.primaryPresent, input.itemBounds, input.primarySize, input.secondarySize,
        presentation.pageGap, presentation.spreadDirection, presentation.fitMode,
        presentation.rotationDegrees, presentation.mirrorHorizontally,
        presentation.mirrorVertically, effectiveManualZoom,
        std::isfinite(input.devicePixelRatio) && input.devicePixelRatio > 0.0
            ? input.devicePixelRatio
            : 1.0,
        presentation.contentPosition };
}
}

PresentationGeometry::State projectViewportGeometryState(const ViewportEngineGeometryInput& input,
    const ImageViewportInternal::PresentationState& presentation)
{
    const double maximum = projectViewportMaximumManualZoomPercent(input, presentation);
    const double preferredPercent = presentation.preferredManualZoom * 100.0;
    const double effectivePercent = maximum == 0.0
        ? preferredPercent
        : std::clamp(preferredPercent, ViewportDisplayLimits::minimumManualZoomPercent(), maximum);
    return viewportGeometryState(input, presentation, effectivePercent / 100.0);
}

double projectViewportMaximumManualZoomPercent(const ViewportEngineGeometryInput& input,
    const ImageViewportInternal::PresentationState& presentation)
{
    if (!input.primaryPresent || !finiteRect(input.itemBounds) || input.itemBounds.width() <= 0.0
        || input.itemBounds.height() <= 0.0 || !std::isfinite(input.devicePixelRatio)
        || input.devicePixelRatio <= 0.0) {
        return 0.0;
    }
    const PresentationGeometry::State geometry = viewportGeometryState(input, presentation, 1.0);
    QSizeF spread = PresentationGeometry::spreadSize(geometry);
    if (!positive(spread)) {
        return 0.0;
    }
    const int rotation = ((presentation.rotationDegrees % 360) + 360) % 360;
    if (rotation == 90 || rotation == 270) {
        spread.transpose();
    }

    const double viewportWidth = input.itemBounds.width();
    const double viewportHeight = input.itemBounds.height();
    const double devicePixelRatio = input.devicePixelRatio;
    const double fit = std::min(viewportWidth / spread.width(), viewportHeight / spread.height())
        * devicePixelRatio * 100.0;
    const double maximumViewportSide = std::max(viewportWidth, viewportHeight);
    const double logicalDisplayLimit = std::max(65536.0, 8.0 * maximumViewportSide);
    const double sizeLimit = logicalDisplayLimit * devicePixelRatio * 100.0
        / std::max(spread.width(), spread.height());
    if (!std::isfinite(fit) || !std::isfinite(logicalDisplayLimit) || !std::isfinite(sizeLimit)) {
        return 0.0;
    }
    const double maximum
        = std::max({ ViewportDisplayLimits::minimumManualZoomPercent(), fit, sizeLimit });
    return std::isfinite(maximum) ? maximum : 0.0;
}

ImageViewportRoleSet projectViewportDisplayedRoleSet(
    const ImageViewportInternal::DisplayState& display)
{
    if (display.status == ImageViewportDisplayStatus::Empty) {
        return {};
    }
    return { display.roles[0].displayedPayload.hasPresentableContent(),
        display.roles[1].displayedPayload.hasPresentableContent() };
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
    const ImageViewportRoleSet displayedRoles = projectViewportDisplayedRoleSet(access.display());
    const bool primaryDisplayed = displayedRoles.primary();
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

    const ImageViewportRequestSnapshot requestSnapshot(
        access.request().status, access.request().reason, acceptedGeneration, acceptedRoles);
    const QSizeF displayedSpreadSize = PresentationGeometry::spreadSize(displayedGeometry);
    const bool displayedPresentable = PresentationGeometry::isPresentable(displayedGeometry);
    const ImageViewportDisplaySnapshot displaySnapshot(access.display().status,
        displayPhase(access.display().status, access.request().status),
        generation(displayedGenerationValue), displayedRoles, targetRoles, displayAccepted,
        access.display().status == ImageViewportDisplayStatus::Retained,
        revision(displayedPresentationRevisionValue), revision(targetPresentationRevisionValue),
        displayedPresentable && positive(displayedSpreadSize) ? displayedSpreadSize
                                                              : QSizeF(0.0, 0.0),
        PresentationGeometry::contentRect(displayedGeometry),
        PresentationGeometry::contentSize(displayedGeometry),
        PresentationGeometry::contentPosition(displayedGeometry),
        PresentationGeometry::maximumContentPosition(displayedGeometry),
        PresentationGeometry::visibleSpreadRect(displayedGeometry),
        PresentationGeometry::horizontalPannable(displayedGeometry),
        PresentationGeometry::verticalPannable(displayedGeometry));
    const ImageViewportPresentationSnapshot presentationSnapshot(access.presentation().fitMode,
        effectiveZoomPercent(acceptedGeometry), access.presentation().preferredManualZoom * 100.0,
        ViewportDisplayLimits::minimumManualZoomPercent(),
        projectViewportMaximumManualZoomPercent(input.acceptedGeometry, access.presentation()),
        ViewportDisplayLimits::manualZoomStepFactor(), access.presentation().rotationDegrees,
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
        const QSizeF payloadRaster = displayed ? payload.payloadRasterSize : QSizeF {};
        const QSizeF sourceScale = displayed ? payload.sourceToPayloadScale : QSizeF {};
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
            = PresentationGeometry::isPresentable(acceptedGeometry) && !acceptedPageRect.isEmpty();
        const bool displayedGeometryAvailable
            = PresentationGeometry::isPresentable(displayedGeometry)
            && !displayedPageRect.isEmpty();
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
                role,
                present ? access.playback().forRole(role).phase
                        : ImageViewportPlaybackPhase::Stopped,
                active.target.frame, active.target.position, logicalSize, active.demandRevision),
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
        ImageViewportDiagnosticsSnapshot(access.request().errorString.text(),
            renderQualityFallbackWarning(access.request(), access.display(), access.presentation()),
            failureSnapshot(access.request()), access.commandReason()),
        ImageViewportRevisionsSnapshot(revision(access.request().requestRevision),
            revision(access.display().revision), revision(access.presentationRevision()),
            revision(commandRevisionValue), revision(snapshotRevisionValue)));
}

ViewportEngineGeometryInput projectViewportCurrentGeometry(
    ViewportEngineGeometryQueryInput input, ViewportEngineCurrentGeometryProjectionAccess access)
{
    const bool present = access.request().roles[0].source.facts.present;
    const bool ready = access.display().hasReadyDisplay(present);
    const QSizeF primary
        = ready ? access.display().roles[0].displayedPayload.sourceLogicalSize : QSizeF {};
    const QSizeF secondary
        = ready ? access.display().roles[1].displayedPayload.sourceLogicalSize : QSizeF {};
    return { positive(primary), input.itemBounds, primary, secondary,
        std::isfinite(input.devicePixelRatio) && input.devicePixelRatio > 0.0
            ? input.devicePixelRatio
            : 1.0,
        input.renderAvailable };
}

ViewportEngineGeometryInput projectViewportPendingGeometry(
    ViewportEngineGeometryQueryInput input, ViewportEnginePendingGeometryProjectionAccess access)
{
    const auto& request = access.request();
    QSizeF primary = request.roles[0].source.facts.provider
        ? access.providerFacts()[0].logicalSize
        : sourceLogicalSize(request.roles[0].source);
    QSizeF secondary;
    if (!request.roles[1].sequence || request.roles[1].activeRequest.target.frame < 0)
        secondary = {};
    else if (request.roles[1].provider)
        secondary = access.providerFacts()[1].logicalSize;
    else
        secondary = sourceLogicalSize(request.roles[1].source);
    if (!positive(primary))
        primary = {};
    if (!positive(secondary))
        secondary = {};
    return { positive(primary), input.itemBounds, primary, secondary,
        std::isfinite(input.devicePixelRatio) && input.devicePixelRatio > 0.0
            ? input.devicePixelRatio
            : 1.0,
        input.renderAvailable };
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
        std::isfinite(input.devicePixelRatio) && input.devicePixelRatio > 0.0
            ? input.devicePixelRatio
            : 1.0,
        input.renderAvailable };
}

ViewportRenderSnapshot projectViewportRenderSnapshot(
    ViewportRenderSnapshotInput input, ViewportEngineRenderSnapshotProjectionAccess access)
{
    const auto& presentation = input.useDisplayedPresentation
        ? access.display().displayedPresentation
        : access.presentation();
    const auto target = [&](ImageViewportPageRole role) {
        if (!PresentationGeometry::isPresentable(input.geometryState)) {
            return QRectF {};
        }
        return finiteRectOrEmpty(PresentationGeometry::pageItemRect(input.geometryState, role)
                .intersected(input.geometryState.itemBounds));
    };
    const auto source = [&](ImageViewportPageRole role) {
        return finiteRectOrEmpty(PresentationGeometry::visiblePageRect(input.geometryState, role));
    };
    ViewportRenderSnapshot snapshot;
    snapshot.targetSpread = input.targetSpread;
    snapshot.presentation = input.presentation;
    snapshot.itemSize = positive(input.itemSize) ? input.itemSize : QSizeF {};
    snapshot.backgroundMode = presentation.backgroundMode;
    snapshot.backgroundColor = presentation.backgroundColor;
    snapshot.checkerboardLightColor = presentation.checkerboardLightColor;
    snapshot.checkerboardDarkColor = presentation.checkerboardDarkColor;
    snapshot.checkerboardCellSize = presentation.checkerboardCellSize;
    snapshot.smoothing = presentation.smoothing;
    snapshot.mipmap = presentation.mipmap;
    snapshot.requiredRoleSet = input.requiredRoleSet;
    const auto append = [&](ImageViewportPageRole role, const auto& payload) {
        const QRectF targetRect = target(role);
        const QRectF sourceRect = source(role);
        snapshot.imageLayers.append(
            { role, payload, targetRect, sourceRect, presentation.rotationDegrees,
                presentation.mirrorHorizontally, presentation.mirrorVertically });
    };
    if (input.requiredRoleSet.primary())
        append(ImageViewportPageRole::Primary, input.preparedPayloads[0]);
    if (input.requiredRoleSet.secondary())
        append(ImageViewportPageRole::Secondary, input.preparedPayloads[1]);
    return snapshot;
}
