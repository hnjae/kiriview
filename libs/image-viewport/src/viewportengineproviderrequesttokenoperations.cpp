// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineproviderrequesttokenoperations_p.h"

#include "imageviewporttoken_p.h"
#include "viewportengineprovidersessionoperations_p.h"
#include "viewportenginetargetspreadterminaloperations_p.h"

#include <limits>

ViewportProviderFrameTransportEffect ViewportProviderRequestTokenAllocationAccess::closeSession(
    ImageViewportPageRole role)
{
    ViewportEngineProviderSessionCloseAccess access(session(role), requests(role));
    auto effect = closeViewportEngineProviderSession(access);
    auto mutation = access.takeMutation();
    session(role) = std::move(mutation.session);
    requests(role) = std::move(mutation.requests);
    return effect;
}

ViewportProviderRequestTokenAllocationResult allocateViewportProviderRequestToken(
    ViewportProviderRequestTokenAllocationInput input,
    ViewportProviderRequestTokenAllocationAccess& access)
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
    auto& request = access.request();
    result.changes = recordViewportEngineGenerationTerminal(
        { input.role, ImageViewportRequestStatus::Error,
            ImageViewportRequestReason::ProviderFailure,
            ImageViewportInternal::PublicDiagnosticText::fromTrusted(
                QStringLiteral("provider request token exhausted")),
            result.changes },
        request);
    auto& playback = access.playback();
    playback.providerStartPending = false;
    playback.stopWhenRequestReady = false;
    if (playback.phase != ImageViewportPlaybackPhase::Stopped) {
        playback.phase = ImageViewportPlaybackPhase::Stopped;
        result.changes.playbackPhase = true;
    }
    const auto close = access.closeSession(input.role);
    result.closeSession = close.closeSession;
    result.sessionClose = close.sessionClose;
    auto& display = access.display();
    display.status = display.roles[0].displayedPayload.hasPresentableContent()
        ? ImageViewportDisplayStatus::Retained
        : ImageViewportDisplayStatus::Empty;
    display.clearPendingRenderPayload();
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.scheduleUpdate = true;
    return result;
}
