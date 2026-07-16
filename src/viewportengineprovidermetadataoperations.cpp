#include "imageviewporttoken_p.h"
#include "viewportengineprovidermetadataoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"

#include <algorithm>
#include <limits>

namespace {
using namespace ImageViewportInternal;

struct AcceptedMetadataFacts
{
    bool timedMetadata = false;
    bool timedPlaybackSupport = false;
    bool frameSeekSupport = false;
    bool positionSeekSupport = false;
    QSizeF logicalSize;
    TimingIntervals timingIntervals;
    ImageSequenceAuthoredAnimationFacts authoredAnimationFacts;
    bool authoredAnimationFactsAvailable = false;
};

bool authoredAnimationFactsEqual(
    ImageSequenceAuthoredAnimationFacts lhs, ImageSequenceAuthoredAnimationFacts rhs)
{
    return lhs.autoplay() == rhs.autoplay() && lhs.loopMode() == rhs.loopMode()
        && lhs.loopCount() == rhs.loopCount();
}

struct MetadataTargetRejection
{
    ImageViewportRequestStatus status = ImageViewportRequestStatus::Unsupported;
    ImageViewportRequestReason reason = ImageViewportRequestReason::UnsupportedRequest;
    int selectedFrame = -1;
    bool updateActiveTarget = false;
    bool selectedFromPosition = false;
    bool clearPlaybackStartPending = false;
};

ProviderRoleState& providerFor(
    std::array<ViewportEngineRoleState, 2>& roles, ImageViewportPageRole role)
{
    return roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider;
}

DisplayRequest& requestForRole(RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? request.roles[1].activeRequest
                                                    : request.roles[0].activeRequest;
}

bool terminalMatchesActiveRequest(const RequestState& request)
{
    const auto& terminal = request.targetSpreadTerminal;
    return terminal.sealed && terminal.generation == request.sequenceGeneration
        && terminal.requestId == request.roles[0].activeRequest.identity.id;
}

bool unknownMetadataInitialRequest(const DisplayRequest& request)
{
    return (request.identity.origin == DisplayRequestOrigin::Initial
               || request.identity.origin == DisplayRequestOrigin::StopRestore
               || request.identity.origin == DisplayRequestOrigin::MetadataBoundSelection)
        && request.target.frame < 0 && request.target.position < 0
        && request.target.providerTargetKind == ProviderRequestTargetKind::Unknown;
}

void updatePlaybackPhase(
    PlaybackState& playback, ImageViewportPlaybackPhase phase, ViewportChangeSet& changes)
{
    if (playback.phase == phase) {
        return;
    }
    playback.phase = phase;
    changes.playbackPhase = true;
}

void mergeChanges(ViewportChangeSet& target, const ViewportChangeSet& source)
{
    target.requestState = target.requestState || source.requestState;
    target.displayState = target.displayState || source.displayState;
    target.geometryState = target.geometryState || source.geometryState;
    target.playbackPhase = target.playbackPhase || source.playbackPhase;
    target.diagnostics = target.diagnostics || source.diagnostics;
    target.displayRevision = target.displayRevision || source.displayRevision;
    target.requestRevision = target.requestRevision || source.requestRevision;
    target.commandRevision = target.commandRevision || source.commandRevision;
    target.presentationRevision = target.presentationRevision || source.presentationRevision;
    target.targetPresentationRevision
        = target.targetPresentationRevision || source.targetPresentationRevision;
    target.adoptTargetPresentationRevision
        = target.adoptTargetPresentationRevision || source.adoptTargetPresentationRevision;
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
}
}

ViewportProviderFrameRequestStartResult
ViewportEngineProviderMetadataReadyAccess::startFrameRequest(ImageViewportPageRole role,
    ImageViewportInternal::DisplayRequestTarget target, const ViewportEngineGeometryInput& geometry)
{
    ViewportEngineProviderFrameRequestAccess access(m_request, m_playback, m_display, m_roles,
        m_presentation, m_nextRevision, m_targetPresentationRevision,
        m_presentationTargetGeneration);
    return startViewportEngineProviderFrameRequest({ role, target, geometry }, std::move(access));
}

void ViewportEngineProviderMetadataReadyAccess::advanceTargetPresentationRevision()
{
    if (m_nextRevision == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport revision token allocator exhausted");
    }
    m_targetPresentationRevision = ++m_nextRevision;
}

ViewportProviderFrameTransportEffect ViewportEngineProviderMetadataReadyAccess::closeSession(
    ImageViewportPageRole role)
{
    auto& provider = providerFor(m_roles, role);
    ViewportEngineProviderSessionCloseAccess access(provider.session, provider.requests);
    return closeViewportEngineProviderSession(std::move(access));
}

ImageViewportInternal::ViewportChangeSet ViewportEngineProviderMetadataReadyAccess::recordTerminal(
    ViewportEngineProviderTerminalProjectionInput input)
{
    ViewportEngineProviderTerminalProjectionAccess access(m_request);
    return reduceViewportEngineProviderTerminalProjection(std::move(input), std::move(access));
}

ViewportEngineProviderMetadataReadyReduction reduceViewportEngineProviderMetadataReady(
    ViewportEngineProviderMetadataReadyInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineProviderMetadataReadyAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEngineProviderMetadataReadyReduction result;
    if (terminalMatchesActiveRequest(access.m_request)) {
        return result;
    }

    auto& provider = providerFor(access.m_roles, input.role);
    const bool providerPresent = input.role == ImageViewportPageRole::Primary
        ? access.m_request.roles[0].source.facts.provider
        : access.m_request.roles[1].sequence && access.m_request.roles[1].provider;
    if (!providerPresent || !provider.session.sessionActive
        || !provider.requests.activeMetadataToken.isValid()
        || input.token != provider.requests.activeMetadataToken) {
        return result;
    }
    provider.requests.activeMetadataToken = {};

    const auto rejectMetadata = [&access, &result, role = input.role](const QString& diagnostic) {
        access.m_playback.providerStartPending = false;
        result.changes = access.recordTerminal(
            { role, ImageViewportRequestStatus::Error, ImageViewportRequestReason::PayloadRejection,
                FailureScope::Generation, diagnostic, result.changes });
        updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
        result.providerFrameTransport = access.closeSession(role);
    };

    const auto admission = FramePreparation::admitProviderMetadata(input.metadata);
    if (!admission.accepted()) {
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::Preparation;
        observation.category = InternalObservationCategory::AdmissionFailure;
        observation.cause = InternalObservationCause::ProviderMetadataRejected;
        observation.identity.roleValid = true;
        observation.identity.role = input.role;
        observation.identity.generation = access.m_request.sequenceGeneration;
        observation.identity.requestId
            = access.m_request.roles[input.role == ImageViewportPageRole::Secondary ? 1 : 0]
                  .activeRequest.identity.id;
        observation.identity.providerToken = ProviderRequestTokenPrivateAccess::value(input.token);
        observation.detail = int(admission.cause);
        result.observations.append(observation);
        rejectMetadata(admission.diagnostic);
        return result;
    }
    const auto& source = input.role == ImageViewportPageRole::Secondary
        ? access.m_request.roles[1].source
        : access.m_request.roles[0].source;
    const auto& sourceFacts = source.facts;
    if (providerCapabilityContradictsMetadata(sourceFacts.providerTimedPlaybackCapability,
            input.metadata.timedPlaybackSupport() == ImageViewportCapabilitySupport::True)
        || providerCapabilityContradictsMetadata(sourceFacts.providerFrameSeekCapability,
            input.metadata.frameSeekSupport() == ImageViewportCapabilitySupport::True)
        || providerCapabilityContradictsMetadata(sourceFacts.providerPositionSeekCapability,
            input.metadata.positionSeekSupport() == ImageViewportCapabilitySupport::True)) {
        rejectMetadata(
            QStringLiteral("provider metadata contradicts construction-time capabilities"));
        return result;
    }
    if (providerFactsContradictMetadata(sourceFacts.providerKnownFacts, input.metadata)) {
        rejectMetadata(QStringLiteral("provider metadata contradicts construction-time facts"));
        return result;
    }
    if (sourceFacts.authoredAnimationFactsAvailable && input.metadata.hasAuthoredAnimationFacts()
        && !authoredAnimationFactsEqual(
            sourceFacts.authoredAnimationFacts, input.metadata.authoredAnimationFacts())) {
        rejectMetadata(QStringLiteral(
            "provider metadata contradicts construction-time authored animation facts"));
        return result;
    }

    const AcceptedMetadataFacts facts { admission.timedMetadata,
        input.metadata.timedPlaybackSupport() == ImageViewportCapabilitySupport::True,
        input.metadata.frameSeekSupport() == ImageViewportCapabilitySupport::True,
        input.metadata.positionSeekSupport() == ImageViewportCapabilitySupport::True,
        admission.logicalSize, admission.timingIntervals,
        input.metadata.hasAuthoredAnimationFacts() ? input.metadata.authoredAnimationFacts()
                                                   : sourceFacts.authoredAnimationFacts,
        input.metadata.hasAuthoredAnimationFacts() || sourceFacts.authoredAnimationFactsAvailable };
    provider.facts.metadataReady = true;
    provider.facts.timedMetadata = facts.timedMetadata;
    provider.facts.timedPlaybackSupport = facts.timedPlaybackSupport;
    provider.facts.frameSeekSupport = facts.frameSeekSupport;
    provider.facts.positionSeekSupport = facts.positionSeekSupport;
    provider.facts.logicalSize = facts.logicalSize;
    provider.facts.timingIntervals = facts.timingIntervals;
    provider.facts.authoredAnimationFacts = facts.authoredAnimationFacts;
    provider.facts.authoredAnimationFactsAvailable = facts.authoredAnimationFactsAvailable;
    if (input.role == ImageViewportPageRole::Secondary) {
        result.changes.requestState = true;
        result.changes.requestRevision = true;
    }

    auto rejectTarget = [&access, &result, role = input.role](MetadataTargetRejection rejection) {
        if (rejection.updateActiveTarget) {
            auto& request = requestForRole(access.m_request, role);
            request.target.frame = rejection.selectedFrame;
            request.resolvedFrame = { rejection.selectedFrame, -1 };
            if (!rejection.selectedFromPosition) {
                request.target.position = -1;
            }
            access.m_playback.position = -1;
        }
        const bool diagnosticsChanged = access.m_request.clearDiagnostics();
        if (rejection.clearPlaybackStartPending) {
            access.m_playback.providerStartPending = false;
        }
        result.changes = access.recordTerminal({ role, rejection.status, rejection.reason,
            FailureScope::DisplayRequest, {}, result.changes });
        updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsChanged;
    };

    ViewportEngineGeometryInput acceptedGeometry = input.geometry;
    if (input.role == ImageViewportPageRole::Primary) {
        acceptedGeometry.primaryPresent = true;
        acceptedGeometry.primarySize = facts.logicalSize;
    } else {
        acceptedGeometry.secondarySize = facts.logicalSize;
    }

    const int frameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;
    DisplayRequestTarget target;
    if (input.role == ImageViewportPageRole::Primary) {
        const auto request = access.m_request.roles[0].activeRequest;
        const bool playback = access.m_playback.providerStartPending
            && request.target.providerTargetKind == ProviderRequestTargetKind::Playback;
        const bool position
            = request.target.providerTargetKind == ProviderRequestTargetKind::Position;
        target.providerTargetKind = playback ? ProviderRequestTargetKind::Playback
            : position                       ? ProviderRequestTargetKind::Position
                                             : ProviderRequestTargetKind::Frame;
        target.frame = request.target.frame >= 0 ? request.target.frame : 0;
        target.position = position ? request.target.position : -1;
        if (playback && (!facts.timedMetadata || !provider.facts.timedPlaybackSupport)) {
            rejectTarget({ ImageViewportRequestStatus::Unsupported,
                ImageViewportRequestReason::UnsupportedRequest, -1, false, false, true });
            return result;
        }
        if (position) {
            if (!facts.timedMetadata || !provider.facts.positionSeekSupport) {
                rejectTarget({ ImageViewportRequestStatus::Unsupported,
                    ImageViewportRequestReason::UnsupportedRequest });
                return result;
            }
            target.frame = facts.timingIntervals.frameIndexForPosition(request.target.position);
        }
        if (target.frame < 0 || target.frame >= frameCount) {
            rejectTarget({ ImageViewportRequestStatus::Unsupported,
                ImageViewportRequestReason::InvalidRequest, target.frame, true, position });
            return result;
        }
        const int resolvedPosition
            = facts.timedMetadata ? facts.timingIntervals.frameStartPosition(target.frame) : -1;
        if (!position) {
            target.position = resolvedPosition;
        }
        const bool carrySecondary = access.m_request.roles[1].sequence
            && access.m_request.roles[1].provider
            && access.m_request.roles[1].activeRequest.identity.id == request.identity.id
            && unknownMetadataInitialRequest(request)
            && unknownMetadataInitialRequest(access.m_request.roles[1].activeRequest);
        access.m_request.beginDisplayRequest(DisplayRequestOrigin::MetadataBoundSelection, target,
            { target.frame, resolvedPosition },
            target.providerTargetKind != ProviderRequestTargetKind::Playback);
        if (carrySecondary) {
            access.m_request.roles[1].activeRequest.identity
                = access.m_request.roles[0].activeRequest.identity;
            access.m_request.roles[1].activeRequest.preparedPayloadId
                = access.m_request.roles[0].activeRequest.preparedPayloadId;
        }
        access.m_playback.position = target.position;
        access.m_playback.providerStartPending = false;
    } else {
        const auto request = access.m_request.roles[1].activeRequest;
        if (request.identity.id == 0
            || request.identity.id != access.m_request.roles[0].activeRequest.identity.id) {
            return result;
        }
        if (unknownMetadataInitialRequest(request)) {
            target = { 0, facts.timedMetadata ? 0 : -1, ProviderRequestTargetKind::Frame };
        } else {
            target = request.target;
            if (target.providerTargetKind == ProviderRequestTargetKind::Playback) {
                if (!facts.timedMetadata || !provider.facts.timedPlaybackSupport) {
                    rejectTarget({ ImageViewportRequestStatus::Unsupported,
                        ImageViewportRequestReason::UnsupportedRequest });
                    return result;
                }
                target.frame = std::max(target.frame, 0);
                target.position = target.frame < frameCount
                    ? facts.timingIntervals.frameStartPosition(target.frame)
                    : -1;
            } else if (target.providerTargetKind == ProviderRequestTargetKind::Frame) {
                target.position
                    = facts.timedMetadata && target.frame >= 0 && target.frame < frameCount
                    ? facts.timingIntervals.frameStartPosition(target.frame)
                    : -1;
            } else if (target.providerTargetKind == ProviderRequestTargetKind::Position) {
                if (!facts.timedMetadata || !provider.facts.positionSeekSupport) {
                    rejectTarget({ ImageViewportRequestStatus::Unsupported,
                        ImageViewportRequestReason::UnsupportedRequest });
                    return result;
                }
                target.frame = facts.timingIntervals.frameIndexForPosition(target.position);
            } else {
                return result;
            }
            if (target.frame < 0 || target.frame >= frameCount) {
                rejectTarget({ ImageViewportRequestStatus::Unsupported,
                    ImageViewportRequestReason::InvalidRequest });
                return result;
            }
        }
    }

    if (input.role == ImageViewportPageRole::Primary) {
        access.m_display.clearPendingRenderPayload();
        access.m_display.clearRenderFailureRetainedDisplay();
    }
    const auto positiveSize
        = [](QSizeF size) { return size.isValid() && size.width() > 0.0 && size.height() > 0.0; };
    const bool completeTargetGeometry = acceptedGeometry.primaryPresent
        && positiveSize(acceptedGeometry.primarySize)
        && (!access.m_presentationTarget.acceptedRoleSet.secondary()
            || positiveSize(acceptedGeometry.secondarySize));
    const bool targetGeometryChanged
        = acceptedGeometry.primaryPresent != input.geometry.primaryPresent
        || acceptedGeometry.primarySize != input.geometry.primarySize
        || acceptedGeometry.secondarySize != input.geometry.secondarySize;
    if (completeTargetGeometry && targetGeometryChanged) {
        access.advanceTargetPresentationRevision();
        result.changes.targetPresentationRevision = true;
        result.changes.displayRevision = true;
        result.changes.geometryState = true;
    }
    mergeChanges(result.changes,
        resolveViewportEnginePendingPresentationTargetTransition(acceptedGeometry,
            access.m_presentationTarget, access.m_presentation,
            access.m_display.hasReadyDisplay(access.m_request.roles[0].source.facts.present)));
    const auto start = access.startFrameRequest(input.role, target, acceptedGeometry);
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    if (!start.accepted) {
        result.changes.diagnostics = true;
    }
    return result;
}
