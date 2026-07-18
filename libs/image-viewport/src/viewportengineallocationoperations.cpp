// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineallocationoperations_p.h"

#include <ImageViewport/imagesequence.h>

#include <algorithm>
#include <limits>

namespace {
using namespace ImageViewportInternal;

bool sameIdentity(PreparedPayloadIdentity lhs, PreparedPayloadIdentity rhs)
{
    return lhs.isValid() && rhs.isValid() && lhs.generation == rhs.generation
        && lhs.payloadId == rhs.payloadId;
}

qint64 accountedPayloadBytes(const DisplayState& display)
{
    std::array<PreparedPayloadIdentity, 4> identities;
    std::size_t identityCount = 0;
    qint64 total = 0;
    const auto account = [&](const PreparedPayload& payload) {
        if (payload.image.isNull() || payload.payloadByteSize <= 0) {
            return;
        }
        const auto identity = payload.identity();
        for (std::size_t index = 0; index < identityCount; ++index) {
            if (sameIdentity(identity, identities[index])) {
                return;
            }
        }
        if (identity.isValid() && identityCount < identities.size()) {
            identities[identityCount++] = identity;
        }
        const qint64 remaining = std::numeric_limits<qint64>::max() - total;
        total += std::min(payload.payloadByteSize, remaining);
    };

    if (display.status == ImageViewportDisplayStatus::Ready
        || display.status == ImageViewportDisplayStatus::Retained) {
        for (const auto& role : display.roles) {
            account(role.displayedPayload);
        }
    }
    for (const auto& role : display.roles) {
        account(role.pendingRenderPayload);
    }
    return total;
}

int providerRoleCount(const RequestState& request)
{
    return int(request.roles[0].source.facts.present && request.roles[0].source.facts.provider)
        + int(request.roles[1].source.facts.present && request.roles[1].source.facts.provider);
}
}

qint64 viewportEngineDisplayPayloadByteBudget()
{
    constexpr qint64 roleCapacity = 2;
    const qint64 perPayload = ImageSequenceLimits::maximumPayloadBytes();
    return perPayload > std::numeric_limits<qint64>::max() / roleCapacity
        ? std::numeric_limits<qint64>::max()
        : perPayload * roleCapacity;
}

ViewportEnginePayloadAllocationRebuildResult rebuildViewportEnginePayloadAllocation(
    const ImageViewportInternal::RequestState& request,
    ImageViewportInternal::DisplayState& display)
{
    using namespace ImageViewportInternal;
    ViewportEnginePayloadAllocationRebuildResult result;
    const qint64 totalBudget = viewportEngineDisplayPayloadByteBudget();
    const int providers = providerRoleCount(request);
    qint64 accounted = accountedPayloadBytes(display);
    if (display.status == ImageViewportDisplayStatus::Retained
        && (accounted > totalBudget || (providers > 0 && accounted >= totalBudget))) {
        display.discardRetainedDisplay();
        result.retainedDisplayDiscarded = true;
        accounted = accountedPayloadBytes(display);
    }

    auto& allocation = display.payloadAllocation;
    const auto previousRoleBudgets = allocation.roleBudgets;
    if (allocation.nextGeneration == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport payload allocation generation exhausted");
    }
    allocation.generation = ++allocation.nextGeneration;
    allocation.roleBudgets = { -1, -1 };
    if (providers == 0) {
        return result;
    }

    const qint64 available = std::max<qint64>(0, totalBudget - std::min(accounted, totalBudget));
    const qint64 sharedBudget
        = std::min(ImageSequenceLimits::maximumPayloadBytes(), available / providers);
    for (std::size_t index = 0; index < request.roles.size(); ++index) {
        if (request.roles[index].source.facts.present
            && request.roles[index].source.facts.provider) {
            allocation.roleBudgets[index] = sharedBudget;
        }
    }
    for (std::size_t index = 0; index < allocation.roleBudgets.size(); ++index) {
        result.roleBudgetsIncreased = result.roleBudgetsIncreased
            || allocation.roleBudgets[index] > previousRoleBudgets[index];
    }
    return result;
}
