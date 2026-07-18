/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

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
        && actual.payloadId == expected.payloadId;
}
inline bool targetSpreadMatches(TargetSpreadIdentity actual, const RequestState& request)
{
    const auto expected = request.activeTargetSpreadIdentity();
    return actual.isValid() && expected.isValid() && actual.generation == expected.generation
        && actual.requestId == expected.requestId;
}
inline PreparedPayloadIdentity acknowledgedPayload(
    const ViewportRenderAcknowledgement& acknowledgement, ImageViewportPageRole role)
{
    for (const auto& payload : acknowledgement.rolePayloads)
        if (payload.role == role)
            return payload.preparedPayload;
    return {};
}
inline PreparedPayloadIdentity expectedPayload(
    const DisplayState& display, const RequestState& request, ImageViewportPageRole role)
{
    const auto index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
    if (display.roles[index].pendingRenderPayload.commitPending)
        return display.roles[index].pendingRenderPayload.identity();
    if (role == ImageViewportPageRole::Primary)
        return display.roles[0].displayedPayload.identity();
    if (!hasSecondary(request))
        return {};
    return display.roles[1].displayedPayload.identity();
}
inline bool primaryMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    const auto actual = acknowledgedPayload(acknowledgement, ImageViewportPageRole::Primary);
    return payloadMatches(actual, expectedPayload(display, request, ImageViewportPageRole::Primary))
        && request.activeRequestOwnsPreparedPayload(ImageViewportPageRole::Primary, actual);
}
inline bool completeMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    if (!targetSpreadMatches(acknowledgement.targetSpread, request)
        || !primaryMatches(display, request, acknowledgement)) {
        return false;
    }
    if (!hasSecondary(request))
        return true;
    const auto secondary = acknowledgedPayload(acknowledgement, ImageViewportPageRole::Secondary);
    return payloadMatches(
               secondary, expectedPayload(display, request, ImageViewportPageRole::Secondary))
        && request.activeRequestOwnsPreparedPayload(ImageViewportPageRole::Secondary, secondary);
}
inline bool failureMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    if (!targetSpreadMatches(acknowledgement.targetSpread, request))
        return false;
    const auto index = acknowledgement.failedRole == ImageViewportPageRole::Secondary ? 1U : 0U;
    const auto& pending = display.roles[index].pendingRenderPayload;
    if (!pending.commitPending
        || !payloadMatches(
            acknowledgedPayload(acknowledgement, acknowledgement.failedRole), pending.identity()))
        return false;
    return request.activeRequestOwnsPreparedPayload(acknowledgement.failedRole, pending.identity());
}
}
