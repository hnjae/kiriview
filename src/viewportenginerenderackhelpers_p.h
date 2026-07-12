#pragma once

#include "viewportenginestate_p.h"

namespace ViewportEngineRenderAcknowledgement {
using namespace ImageViewportInternal;

inline bool hasSecondary(const RequestState& request)
{
    return request.roles[1].sequence && request.roles[1].activeRequest.target.frame >= 0;
}
inline bool payloadMatches(PreparedPayloadIdentity actual, PreparedPayloadIdentity expected)
{
    return actual.isValid() && expected.isValid() && actual.generation == expected.generation
        && actual.requestId == expected.requestId && actual.payloadId == expected.payloadId;
}
inline PreparedPayloadIdentity acknowledgedPayload(
    const ViewportRenderAcknowledgement& acknowledgement, ImageViewport::PageRole role)
{
    for (const auto& payload : acknowledgement.rolePayloads)
        if (payload.role == role) return payload.preparedPayload;
    return role == ImageViewport::PageRole::Primary ? acknowledgement.preparedPayload
                                                    : PreparedPayloadIdentity {};
}
inline PreparedPayloadIdentity expectedPayload(
    const DisplayState& display, const RequestState& request, ImageViewport::PageRole role)
{
    if (role == ImageViewport::PageRole::Primary) return display.roles[0].pendingRenderPayload.identity();
    if (!hasSecondary(request)) return {};
    const auto secondary = display.roles[1].pendingRenderPayload.identity();
    return secondary.isValid() ? secondary : display.roles[0].pendingRenderPayload.identity();
}
inline bool primaryMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    const auto actual = acknowledgedPayload(acknowledgement, ImageViewport::PageRole::Primary);
    return display.roles[0].pendingRenderPayload.commitPending
        && payloadMatches(actual, display.roles[0].pendingRenderPayload.identity())
        && request.activeRequestOwnsPreparedPayload(actual);
}
inline bool completeMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    return primaryMatches(display, request, acknowledgement)
        && (!hasSecondary(request) || payloadMatches(
            acknowledgedPayload(acknowledgement, ImageViewport::PageRole::Secondary),
            expectedPayload(display, request, ImageViewport::PageRole::Secondary)));
}
inline bool failureMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    if (acknowledgement.failedRole == ImageViewport::PageRole::Primary)
        return primaryMatches(display, request, acknowledgement);
    return hasSecondary(request)
        && payloadMatches(acknowledgedPayload(acknowledgement, acknowledgement.failedRole),
            expectedPayload(display, request, acknowledgement.failedRole));
}
}
