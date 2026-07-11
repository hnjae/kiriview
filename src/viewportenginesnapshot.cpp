#include "viewportengine_p.h"

#include "imageviewportlimits_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"

#include <algorithm>

namespace {
using namespace ImageViewportInternal;

bool positive(QSizeF size) { return size.isValid() && size.width() > 0.0 && size.height() > 0.0; }

ImageViewportRevisionToken revision(quint64 value)
{
    return RevisionTokenPrivateAccess::publicRevisionFromValue(value);
}

ImageViewportPresentationTargetGenerationToken generation(quint64 value)
{
    return RevisionTokenPrivateAccess::generationFromValue(value);
}

ImageViewport::DisplayPhase displayPhase(
    ImageViewport::DisplayStatus display, ImageViewport::RequestStatus request)
{
    if (display == ImageViewport::DisplayStatus::Ready) {
        return ImageViewport::DisplayPhase::CommittedActive;
    }
    if (display == ImageViewport::DisplayStatus::Retained) {
        return ImageViewport::DisplayPhase::PreviousActive;
    }
    return request == ImageViewport::RequestStatus::NoRequest
        ? ImageViewport::DisplayPhase::NoPresentation
        : ImageViewport::DisplayPhase::TransitioningPlaceholder;
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

const ImageSequenceSource& sourceForRole(const RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? request.roles[0].source
                                                    : request.roles[1].source;
}

const DisplayRequest& requestForRole(const RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? request.roles[0].activeRequest
                                                    : request.roles[1].activeRequest;
}

const DisplayRequestSnapshot& displayedRequestForRole(
    const DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? display.roles[0].displayedRequest
                                                    : display.roles[1].displayedRequest;
}

QSizeF displayedSizeForRole(const DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? display.roles[0].displayedImageSize
                                                    : display.roles[1].displayedImageSize;
}

const PreparedPayload& displayedPayloadForRole(
    const DisplayState& display, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? display.roles[0].displayedPayload
                                                    : display.roles[1].displayedPayload;
}

const ProviderGenerationState& providerForRole(
    const ViewportEngine& engine, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Primary ? engine.providerState()
                                                    : engine.secondaryProviderState();
}

struct Metadata
{
    int frameCount = -1;
    int totalDuration = -1;
    ImageViewportRange frameBounds;
    ImageViewportRange positionBounds;
    ImageViewport::CapabilitySupport frameSeek = ImageViewport::CapabilitySupport::Unavailable;
    ImageViewport::CapabilitySupport positionSeek = ImageViewport::CapabilitySupport::Unavailable;
    ImageViewport::CapabilitySupport playback = ImageViewport::CapabilitySupport::Unavailable;
};

ImageViewport::CapabilitySupport support(bool value)
{
    return value ? ImageViewport::CapabilitySupport::True : ImageViewport::CapabilitySupport::False;
}

Metadata metadataFor(const ViewportEngine& engine, ImageViewport::PageRole role)
{
    const auto& source = sourceForRole(engine.requestState(), role);
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
        result.frameSeek = ImageViewport::CapabilitySupport::True;
        result.positionSeek = support(source.facts.timed);
        result.playback = support(source.facts.timed);
        return result;
    }
    const auto& provider = providerForRole(engine, role);
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
            result.frameSeek = ImageViewport::CapabilitySupport::True;
            result.positionSeek = ImageViewport::CapabilitySupport::False;
            result.playback = ImageViewport::CapabilitySupport::False;
        } else if (source.facts.providerKnownFacts.isTimedFrameList()) {
            const TimingIntervals timing = TimingIntervals::fromFrameDurations(
                source.facts.providerKnownFacts.frameDurations());
            result.frameCount = timing.frameCount();
            result.totalDuration = timing.totalDuration();
            result.frameSeek = ImageViewport::CapabilitySupport::True;
            result.positionSeek = ImageViewport::CapabilitySupport::True;
            result.playback = ImageViewport::CapabilitySupport::True;
        } else if (source.facts.providerKnownFacts.isTimedFrameCount()) {
            result.frameCount = source.facts.providerKnownFacts.frameCount();
        }
    }
    if (result.frameCount > 0 && result.frameSeek == ImageViewport::CapabilitySupport::True) {
        result.frameBounds = ImageViewportRange(0, result.frameCount - 1);
    }
    if (result.totalDuration >= 0
        && result.positionSeek == ImageViewport::CapabilitySupport::True) {
        result.positionBounds = ImageViewportRange(0, result.totalDuration);
    }
    return result;
}

double effectiveZoomPercent(const PresentationGeometry::State& geometry)
{
    const QSizeF spread = PresentationGeometry::spreadSize(geometry);
    const QRectF content = PresentationGeometry::contentRect(geometry);
    if (!positive(spread) || content.isEmpty()) {
        return geometry.manualZoom * 100.0;
    }
    const int rotation = ((geometry.rotationDegrees % 360) + 360) % 360;
    const QSizeF oriented
        = rotation == 90 || rotation == 270 ? QSizeF(spread.height(), spread.width()) : spread;
    return content.width() / oriented.width() * geometry.devicePixelRatio * 100.0;
}
}

ImageViewportStateSnapshot ViewportEngine::snapshot() const
{
    GeometryInput input;
    input.primaryPresent = positive(m_displayState.roles[0].displayedImageSize);
    input.primarySize = m_displayState.roles[0].displayedImageSize;
    input.secondarySize = m_displayState.roles[1].displayedImageSize;
    return snapshot(input);
}

ImageViewportStateSnapshot ViewportEngine::snapshot(const GeometryInput& input) const
{
    return snapshot({ input, input });
}

ImageViewportStateSnapshot ViewportEngine::snapshot(const SnapshotInput& input) const
{
    const bool primaryPresent = m_requestState.roles[0].source.facts.present;
    const bool secondaryPresent = m_requestState.roles[1].source.facts.present;
    const ImageViewportRoleSet acceptedRoles(primaryPresent, secondaryPresent);
    const ImageViewportRoleSet targetRoles(primaryPresent,
        secondaryPresent && m_requestState.roles[1].activeRequest.target.frame >= 0);
    const quint64 acceptedGenerationValue = m_presentationTargetState.generation != 0
        ? m_presentationTargetState.generation
        : m_requestState.sequenceGeneration;
    const auto acceptedGeneration = generation(primaryPresent ? acceptedGenerationValue : 0);
    const bool primaryDisplayed = m_displayState.status != ImageViewport::DisplayStatus::Empty
        && positive(m_displayState.roles[0].displayedImageSize);
    const bool secondaryDisplayed = m_displayState.status != ImageViewport::DisplayStatus::Empty
        && positive(m_displayState.roles[1].displayedImageSize);
    const ImageViewportRoleSet displayedRoles(primaryDisplayed, secondaryDisplayed);
    const quint64 displayedGenerationValue
        = primaryDisplayed ? m_displayState.roles[0].displayedRequest.generation : 0;
    const bool displayAccepted
        = primaryDisplayed && displayedGenerationValue == acceptedGenerationValue;
    const PresentationGeometry::State acceptedGeometry = geometryState(input.acceptedGeometry);
    const PresentationGeometry::State displayedGeometry = geometryState(input.displayedGeometry,
        m_displayState.status == ImageViewport::DisplayStatus::Retained
            ? m_displayState.displayedPresentation
            : m_presentationState);
    const quint64 presentationRevisionValue
        = m_presentationRevision != 0 ? m_presentationRevision : m_displayState.revision;
    const quint64 commandRevisionValue = m_requestState.commandRevision != 0
        ? m_requestState.commandRevision
        : RevisionTokenPrivateAccess::value(m_commandRevision);
    const quint64 snapshotRevisionValue = m_snapshotRevision != 0
        ? m_snapshotRevision
        : snapshotRevision(m_requestState, m_displayState, presentationRevisionValue,
              commandRevisionValue, acceptedGenerationValue);

    QVariant activeRole;
    if (primaryPresent) {
        activeRole = QVariant::fromValue(ImageViewport::PageRole::Primary);
    }
    QVariant playbackRole;
    if (m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Stopped && primaryPresent) {
        playbackRole = QVariant::fromValue(m_requestState.playbackRole);
    }

    const ImageViewportRequestSnapshot requestSnapshot(m_requestState.status, m_requestState.reason,
        m_requestState.playbackPhase, acceptedGeneration, acceptedRoles, targetRoles, activeRole,
        playbackRole);
    const QSizeF displayedSpreadSize = PresentationGeometry::spreadSize(displayedGeometry);
    const ImageViewportDisplaySnapshot displaySnapshot(m_displayState.status,
        displayPhase(m_displayState.status, m_requestState.status),
        generation(displayedGenerationValue), displayedRoles, targetRoles, displayAccepted,
        m_displayState.status == ImageViewport::DisplayStatus::Retained,
        revision(primaryDisplayed ? (m_displayState.displayedPresentationRevision != 0
                                            ? m_displayState.displayedPresentationRevision
                                            : m_displayState.revision)
                                  : 0),
        revision(presentationRevisionValue),
        positive(displayedSpreadSize) ? displayedSpreadSize : QSizeF(0.0, 0.0),
        PresentationGeometry::contentRect(displayedGeometry),
        PresentationGeometry::contentSize(displayedGeometry),
        PresentationGeometry::contentPosition(displayedGeometry),
        PresentationGeometry::maximumContentPosition(displayedGeometry),
        PresentationGeometry::visibleSpreadRect(displayedGeometry),
        PresentationGeometry::horizontalPannable(displayedGeometry),
        PresentationGeometry::verticalPannable(displayedGeometry));
    const ImageViewportPresentationSnapshot presentationSnapshot(m_presentationState.fitMode,
        effectiveZoomPercent(acceptedGeometry), std::numeric_limits<double>::denorm_min(),
        ImageViewportDisplayLimits::maximumManualZoomPercent(), 1.25,
        m_presentationState.rotationDegrees, m_presentationState.mirrorHorizontally,
        m_presentationState.mirrorVertically, m_presentationState.spreadDirection,
        m_presentationState.pageGap, m_presentationState.backgroundMode,
        m_presentationState.backgroundColor, m_presentationState.smoothing,
        m_presentationState.mipmap, m_requestState.looping, m_presentationState.qualityPreference,
        m_presentationState.exactnessPreference);

    const auto roleSnapshot = [&](ImageViewport::PageRole role) {
        const auto& source = sourceForRole(m_requestState, role);
        const auto& active = requestForRole(m_requestState, role);
        const auto& displayedRequest = displayedRequestForRole(m_displayState, role);
        const QSizeF displayedSize = displayedSizeForRole(m_displayState, role);
        const bool present = source.facts.present;
        const bool displayed = m_displayState.status != ImageViewport::DisplayStatus::Empty
            && positive(displayedSize);
        const bool belongs = displayed && displayedRequest.generation == acceptedGenerationValue;
        const Metadata metadata = metadataFor(*this, role);
        const QSizeF logicalSize = source.facts.provider
            ? (providerForRole(*this, role).metadataReady ? providerForRole(*this, role).logicalSize
                                                          : source.facts.providerKnownLogicalSize)
            : sourceLogicalSize(source);
        const auto& payload = displayedPayloadForRole(m_displayState, role);
        const QSizeF payloadRaster
            = positive(payload.payloadRasterSize) ? payload.payloadRasterSize : displayedSize;
        const QSizeF sourceScale = positive(payload.sourceToPayloadScale)
            ? payload.sourceToPayloadScale
            : (displayed ? QSizeF(1.0, 1.0) : QSizeF());
        const QRectF acceptedPageRect = role == ImageViewport::PageRole::Primary
            ? PresentationGeometry::primaryPageRect(acceptedGeometry)
            : PresentationGeometry::secondaryPageRect(acceptedGeometry);
        const QRectF acceptedItemRect = PresentationGeometry::pageItemRect(acceptedGeometry, role);
        const QRectF acceptedVisibleRect
            = PresentationGeometry::visiblePageRect(acceptedGeometry, role);
        const QRectF displayedPageRect = role == ImageViewport::PageRole::Primary
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
        const auto animation = source.facts.provider && providerForRole(*this, role).metadataReady
            ? providerForRole(*this, role).authoredAnimationFacts
            : source.facts.authoredAnimationFacts;
        const int loopCount
            = animation.loopMode() == ImageSequenceAuthoredAnimationFacts::LoopMode::Finite
            ? animation.loopCount()
            : -1;
        return ImageViewportRoleSnapshot(present, source.sequence,
            ImageViewportRoleRequestSnapshot(present,
                present ? acceptedGeneration : ImageViewportPresentationTargetGenerationToken {},
                role, active.target.frame, active.target.position, logicalSize,
                active.demandRevision),
            ImageViewportRoleDisplaySnapshot(belongs,
                displayed && m_displayState.status == ImageViewport::DisplayStatus::Retained,
                displayed ? displayedRequest.request.resolvedFrame.frame : -1,
                displayed ? displayedRequest.request.resolvedFrame.position : -1,
                displayed ? displayedSize : QSizeF(), displayed ? payloadRaster : QSizeF(),
                displayed ? sourceScale : QSizeF(),
                displayed ? (payload.quality == ImageViewport::PayloadQuality::Unknown
                                    ? ImageViewport::PayloadQuality::Exact
                                    : payload.quality)
                          : ImageViewport::PayloadQuality::Unknown,
                displayed ? (payload.exactness == ImageViewport::PayloadExactness::Unknown
                                    ? ImageViewport::PayloadExactness::ExactForSource
                                    : payload.exactness)
                          : ImageViewport::PayloadExactness::Unknown,
                displayed && payload.demandRevision.isValid(), payload.demandRevision),
            ImageViewportRoleMetadataSnapshot(
                present && metadata.frameCount >= 0 && positive(logicalSize), logicalSize,
                metadata.frameCount, metadata.totalDuration, metadata.frameBounds,
                metadata.positionBounds, metadata.frameSeek, metadata.positionSeek,
                metadata.playback, animation.autoplay(), animation.progressiveAnimationReadiness(),
                animation.loopMode(), loopCount),
            ImageViewportRoleGeometrySnapshot(
                present && acceptedGeometryAvailable ? acceptedPageRect : QRectF(),
                present && acceptedGeometryAvailable ? acceptedItemRect : QRectF(),
                present && acceptedGeometryAvailable ? acceptedVisibleRect : QRectF(),
                displayed && displayedGeometryAvailable ? displayedPageRect : QRectF(),
                displayed && displayedGeometryAvailable ? displayedItemRect : QRectF(),
                displayed && displayedGeometryAvailable ? displayedVisibleRect : QRectF()));
    };

    return ImageViewportStateSnapshot(requestSnapshot, displaySnapshot, presentationSnapshot,
        roleSnapshot(ImageViewport::PageRole::Primary),
        roleSnapshot(ImageViewport::PageRole::Secondary),
        ImageViewportDiagnosticsSnapshot(
            m_requestState.errorString, m_requestState.warningString, m_requestState.commandReason),
        ImageViewportRevisionsSnapshot(revision(m_requestState.requestRevision),
            revision(m_displayState.revision), revision(presentationRevisionValue),
            revision(commandRevisionValue), revision(snapshotRevisionValue)));
}
