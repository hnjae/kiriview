#include "viewportengineprovidermetadataoperations_p.h"

#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "viewportengineprovidersessionoperations_p.h"

#include <algorithm>

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
};

struct MetadataTargetRejection
{
    ImageViewport::RequestStatus status = ImageViewport::RequestStatus::Unsupported;
    ImageViewport::RequestReason reason = ImageViewport::RequestReason::UnsupportedRequest;
    int selectedFrame = -1;
    bool updateActiveTarget = false;
    bool selectedFromPosition = false;
    bool clearPlaybackStartPending = false;
};

ProviderRoleState& providerFor(
    std::array<ViewportEngineRoleState, 2>& roles, ImageViewport::PageRole role)
{
    return roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U].provider;
}

DisplayRequest& requestForRole(RequestState& request, ImageViewport::PageRole role)
{
    return role == ImageViewport::PageRole::Secondary ? request.roles[1].activeRequest
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
    PlaybackState& playback, ImageViewport::PlaybackPhase phase, ViewportChangeSet& changes)
{
    if (playback.phase == phase) {
        return;
    }
    playback.phase = phase;
    changes.playbackPhase = true;
}
}

ViewportProviderFrameRequestStartResult
ViewportEngineProviderMetadataReadyAccess::startFrameRequest(ImageViewport::PageRole role,
    ImageViewportInternal::DisplayRequestTarget target, const ViewportEngineGeometryInput& geometry)
{
    ViewportEngineProviderFrameRequestAccess access(m_request, m_playback, m_display, m_roles,
        m_presentation, m_nextRevision, m_presentationRevision, m_presentationTargetGeneration);
    return startViewportEngineProviderFrameRequest({ role, target, geometry }, std::move(access));
}

ViewportProviderFrameTransportEffect ViewportEngineProviderMetadataReadyAccess::closeSession(
    ImageViewport::PageRole role)
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
    ViewportEngineProviderMetadataReadyInput input,
    ViewportEngineProviderMetadataReadyAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEngineProviderMetadataReadyReduction result;
    if (terminalMatchesActiveRequest(access.m_request)) {
        return result;
    }

    auto& provider = providerFor(access.m_roles, input.role);
    const bool providerPresent = input.role == ImageViewport::PageRole::Primary
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
        result.changes = access.recordTerminal({ role, ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::PayloadRejection, FailureScope::Generation, diagnostic,
            result.changes });
        updatePlaybackPhase(
            access.m_playback, ImageViewport::PlaybackPhase::Stopped, result.changes);
        result.providerFrameTransport = access.closeSession(role);
    };

    const auto admission = FramePreparation::admitProviderMetadata(input.metadata);
    if (!admission.accepted()) {
        rejectMetadata(admission.diagnostic);
        return result;
    }
    const auto& source = input.role == ImageViewport::PageRole::Secondary
        ? access.m_request.roles[1].source
        : access.m_request.roles[0].source;
    const auto& sourceFacts = source.facts;
    if (providerCapabilityContradictsMetadata(
            sourceFacts.providerTimedPlaybackCapability, input.metadata.timedPlaybackSupport())
        || providerCapabilityContradictsMetadata(
            sourceFacts.providerFrameSeekCapability, input.metadata.frameSeekSupport())
        || providerCapabilityContradictsMetadata(
            sourceFacts.providerPositionSeekCapability, input.metadata.positionSeekSupport())) {
        rejectMetadata(
            QStringLiteral("provider metadata contradicts construction-time capabilities"));
        return result;
    }
    if (providerFactsContradictMetadata(sourceFacts.providerKnownFacts, input.metadata)) {
        rejectMetadata(QStringLiteral("provider metadata contradicts construction-time facts"));
        return result;
    }

    const AcceptedMetadataFacts facts { admission.timedMetadata,
        input.metadata.timedPlaybackSupport(), input.metadata.frameSeekSupport(),
        input.metadata.positionSeekSupport(), admission.logicalSize, admission.timingIntervals,
        input.metadata.hasAuthoredAnimationFacts() ? input.metadata.authoredAnimationFacts()
                                                   : sourceFacts.authoredAnimationFacts };
    provider.facts.metadataReady = true;
    provider.facts.timedMetadata = facts.timedMetadata;
    provider.facts.timedPlaybackSupport = facts.timedPlaybackSupport;
    provider.facts.frameSeekSupport = facts.frameSeekSupport;
    provider.facts.positionSeekSupport = facts.positionSeekSupport;
    provider.facts.logicalSize = facts.logicalSize;
    provider.facts.timingIntervals = facts.timingIntervals;
    provider.facts.authoredAnimationFacts = facts.authoredAnimationFacts;
    if (input.role == ImageViewport::PageRole::Secondary) {
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
        updatePlaybackPhase(
            access.m_playback, ImageViewport::PlaybackPhase::Stopped, result.changes);
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsChanged;
    };

    ViewportEngineGeometryInput acceptedGeometry = input.geometry;
    if (input.role == ImageViewport::PageRole::Primary) {
        acceptedGeometry.primaryPresent = true;
        acceptedGeometry.primarySize = facts.logicalSize;
    } else {
        acceptedGeometry.secondarySize = facts.logicalSize;
    }

    const int frameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;
    DisplayRequestTarget target;
    if (input.role == ImageViewport::PageRole::Primary) {
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
            rejectTarget({ ImageViewport::RequestStatus::Unsupported,
                ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, true });
            return result;
        }
        if (position) {
            if (!facts.timedMetadata || !provider.facts.positionSeekSupport) {
                rejectTarget({ ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest });
                return result;
            }
            target.frame = facts.timingIntervals.frameIndexForPosition(request.target.position);
        }
        if (target.frame < 0 || target.frame >= frameCount) {
            rejectTarget({ ImageViewport::RequestStatus::Unsupported,
                ImageViewport::RequestReason::InvalidRequest, target.frame, true, position });
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
                    rejectTarget({ ImageViewport::RequestStatus::Unsupported,
                        ImageViewport::RequestReason::UnsupportedRequest });
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
                    rejectTarget({ ImageViewport::RequestStatus::Unsupported,
                        ImageViewport::RequestReason::UnsupportedRequest });
                    return result;
                }
                target.frame = facts.timingIntervals.frameIndexForPosition(target.position);
            } else {
                return result;
            }
            if (target.frame < 0 || target.frame >= frameCount) {
                rejectTarget({ ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::InvalidRequest });
                return result;
            }
        }
    }

    if (input.role == ImageViewport::PageRole::Primary) {
        access.m_display.clearPendingRenderPayload();
        access.m_display.clearRenderFailureRetainedDisplay();
    }
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
