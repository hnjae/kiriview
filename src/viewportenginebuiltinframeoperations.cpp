#include "viewportenginebuiltinframeoperations_p.h"

#include "viewportenginetargetspreadterminaloperations_p.h"

namespace {
using namespace ImageViewportInternal;

bool displayedPayloadAvailable(const DisplayState& display)
{
    return (display.status == ImageViewportDisplayStatus::Ready
               || display.status == ImageViewportDisplayStatus::Retained)
        && display.roles[0].displayedPayload.hasPresentableContent();
}

void reuseDisplayedProviderRoles(RequestState& request, DisplayState& display)
{
    for (const ImageViewportPageRole role :
        { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
        const std::size_t index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
        auto& requestRole = request.roles[index];
        auto& displayRole = display.roles[index];
        if (!requestRole.source.facts.present || !requestRole.source.facts.provider
            || !displayRole.displayedPayload.hasPresentableContent()
            || displayRole.displayedRequest.generation != request.sequenceGeneration
            || displayRole.displayedRequest.request.resolvedFrame.frame
                != requestRole.activeRequest.resolvedFrame.frame
            || displayRole.displayedRequest.request.resolvedFrame.position
                != requestRole.activeRequest.resolvedFrame.position) {
            continue;
        }
        displayRole.pendingRenderPayload = displayRole.displayedPayload;
        displayRole.pendingRenderPayload.commitPending = true;
        requestRole.activeRequest.preparedPayloadId = displayRole.pendingRenderPayload.payloadId;
    }
}

void projectFailure(RequestState& request, DisplayState& display, PlaybackState* playback,
    ImageViewportPageRole role, const FramePreparation::BuiltInFrameAdmissionResult& admission,
    ViewportEngineBuiltInFrameStageResult& result)
{
    const QString diagnostic = FramePreparation::boundedDiagnostic(
        admission.diagnostic, QStringLiteral("in-memory frame payload rejected"));
    recordViewportEngineTargetSpreadTerminal(
        { role, admission.status, admission.reason, FailureScope::DisplayRequest, diagnostic, {} },
        request);

    const bool retainDisplay = displayedPayloadAvailable(display);
    display.clearPendingRenderPayload();
    request.roles[0].activeRequest.preparedPayloadId = 0;
    request.roles[1].activeRequest.preparedPayloadId = 0;
    display.status
        = retainDisplay ? ImageViewportDisplayStatus::Retained : ImageViewportDisplayStatus::Empty;
    if (playback && playback->phase != ImageViewportPlaybackPhase::Stopped) {
        playback->phase = ImageViewportPlaybackPhase::Stopped;
        playback->stopWhenRequestReady = false;
        result.playbackStopped = true;
    }

    result.accepted = false;
    result.failedRole = role;
    result.status = admission.status;
    result.reason = admission.reason;
    result.diagnostic = diagnostic;
}

FramePreparation::BuiltInFrameAdmissionResult stageRole(RequestState& request,
    DisplayState& display, ImageViewportExactnessPreference exactnessPreference,
    ImageViewportPageRole role)
{
    const std::size_t index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
    auto& requestRole = request.roles[index];
    PreparedPayload seed;
    if (role == ImageViewportPageRole::Primary) {
        display.roles[0].pendingRenderPayload.commitPending = true;
        display.beginPreparedPayloadIdentity(request.sequenceGeneration, requestRole.activeRequest);
        seed = display.roles[0].pendingRenderPayload;
    } else {
        seed.commitPending = true;
        seed.generation = request.sequenceGeneration;
        seed.payloadId = ++display.nextPreparedPayloadId;
        requestRole.activeRequest.preparedPayloadId = seed.payloadId;
    }
    return FramePreparation::admitBuiltInFrame(requestRole.source,
        requestRole.activeRequest.target.frame, seed, exactnessPreference, role);
}
}

ViewportEngineBuiltInFrameStageResult stageViewportEngineBuiltInTargetSpread(
    ImageViewportInternal::RequestState& request, ImageViewportInternal::DisplayState& display,
    ImageViewportExactnessPreference exactnessPreference,
    ImageViewportInternal::PlaybackState* playback)
{
    ViewportEngineBuiltInFrameStageResult result;
    request.targetSpreadTerminal.clear();
    request.lastAcceptedRenderFailure = {};
    reuseDisplayedProviderRoles(request, display);

    for (ImageViewportPageRole role :
        { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
        const std::size_t index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
        const auto& requestRole = request.roles[index];
        if (!requestRole.source.facts.present || requestRole.source.facts.provider
            || requestRole.activeRequest.target.frame < 0) {
            continue;
        }
        const auto admission = stageRole(request, display, exactnessPreference, role);
        if (!admission.accepted()) {
            projectFailure(request, display, playback, role, admission, result);
            return result;
        }
        display.roles[index].pendingRenderPayload = admission.preparedPayload;
    }
    return result;
}
