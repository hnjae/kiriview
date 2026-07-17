#include "viewportengineproviderrequesttokenoperations_p.h"

#include "imageviewporttoken_p.h"

#include <limits>

ViewportProviderRequestTokenAllocationResult allocateViewportProviderRequestToken(
    ViewportProviderRequestTokenAllocationInput input,
    ViewportProviderRequestTokenAllocationAccess access)
{
    ViewportProviderRequestTokenAllocationResult result;
    auto& session = access.session(input.role);
    auto& requests = access.requests(input.role);
    if (requests.nextRequestToken != std::numeric_limits<quint64>::max()) {
        result.token = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(
            ++requests.nextRequestToken);
        return result;
    }

    result.exhausted = true;
    result.closeSession = session.sessionActive;
    result.sessionClose = { requests.metadataToken(), requests.frameToken() };
    session.sessionActive = false;
    requests = {};
    auto& request = access.request();
    request.status = ImageViewportRequestStatus::Error;
    request.reason = ImageViewportRequestReason::ProviderFailure;
    request.errorString = QStringLiteral("provider request token exhausted");
    auto& playback = access.playback();
    playback.providerStartPending = false;
    playback.stopWhenRequestReady = false;
    playback.phase = ImageViewportPlaybackPhase::Stopped;
    auto& display = access.display();
    display.status = display.roles[0].displayedPayload.hasPresentableContent()
        ? ImageViewportDisplayStatus::Retained
        : ImageViewportDisplayStatus::Empty;
    display.clearPendingRenderPayload();
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.playbackPhase = true;
    result.changes.diagnostics = true;
    result.changes.scheduleUpdate = true;
    return result;
}
