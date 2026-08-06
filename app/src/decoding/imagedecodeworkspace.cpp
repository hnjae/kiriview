// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodeworkspace.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

namespace {
constexpr qsizetype qtSizeLimit(quint64 byteCount)
{
    constexpr qsizetype maximum = std::numeric_limits<qsizetype>::max();
    return byteCount > static_cast<quint64>(maximum) ? maximum : static_cast<qsizetype>(byteCount);
}

constexpr qsizetype preferredAggregateByteLimit = qtSizeLimit(quint64 { 2 } * 1024 * 1024 * 1024);
constexpr qsizetype preferredPerOperationByteLimit
    = qtSizeLimit(quint64 { 1 } * 1024 * 1024 * 1024);
constexpr qsizetype systemMemoryDivisor = 4;
}

namespace kiriview::ImageDecodeWorkspaceDetail {
struct BudgetState
{
    qsizetype aggregateByteLimit = 0;
    qsizetype perOperationByteLimit = 0;
    qsizetype reservedByteCount = 0;
    mutable std::mutex mutex;
};

struct LeaseState
{
    explicit LeaseState(std::shared_ptr<BudgetState> budget)
        : budget(std::move(budget))
    {
    }

    ~LeaseState()
    {
        if (budget == nullptr) {
            return;
        }
        std::scoped_lock lock(budget->mutex);
        budget->reservedByteCount -= reservedByteCount;
    }

    std::shared_ptr<BudgetState> budget;
    qsizetype reservedByteCount = 0;
    Q_DISABLE_COPY_MOVE(LeaseState)
};
}

namespace kiriview {
ImageDecodeWorkspaceHold::ImageDecodeWorkspaceHold(
    std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> state)
    : m_state(std::move(state))
{
}

qsizetype ImageDecodeWorkspaceHold::reservedByteCount() const
{
    if (m_state == nullptr || m_state->budget == nullptr) {
        return 0;
    }
    std::scoped_lock lock(m_state->budget->mutex);
    return m_state->reservedByteCount;
}

bool ImageDecodeWorkspaceHold::isManaged() const { return m_state != nullptr; }

ImageDecodeWorkspaceLease::ImageDecodeWorkspaceLease() = default;

ImageDecodeWorkspaceLease::ImageDecodeWorkspaceLease(
    std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> state)
    : m_state(std::move(state))
{
}

ImageDecodeWorkspaceLease::~ImageDecodeWorkspaceLease() = default;

ImageDecodeWorkspaceLease::ImageDecodeWorkspaceLease(ImageDecodeWorkspaceLease&& other) noexcept
    = default;

ImageDecodeWorkspaceLease& ImageDecodeWorkspaceLease::operator=(
    ImageDecodeWorkspaceLease&& other) noexcept
    = default;

bool ImageDecodeWorkspaceLease::tryReserve(qsizetype additionalByteCount)
{
    if (additionalByteCount < 0) {
        return false;
    }
    if (additionalByteCount == 0) {
        return true;
    }
    if (m_state == nullptr) {
        return false;
    }

    const std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState>& budget = m_state->budget;
    if (budget == nullptr) {
        return false;
    }
    std::scoped_lock lock(budget->mutex);
    if (additionalByteCount > budget->perOperationByteLimit - m_state->reservedByteCount
        || additionalByteCount > budget->aggregateByteLimit - budget->reservedByteCount) {
        return false;
    }

    m_state->reservedByteCount += additionalByteCount;
    budget->reservedByteCount += additionalByteCount;
    return true;
}

ImageDecodeWorkspaceHold ImageDecodeWorkspaceLease::sharedHold() const
{
    return m_state == nullptr ? ImageDecodeWorkspaceHold {} : ImageDecodeWorkspaceHold(m_state);
}

ImageDecodeWorkspaceHold ImageDecodeWorkspaceLease::retainOnly(qsizetype retainedByteCount)
{
    if (retainedByteCount < 0 || m_state == nullptr || m_state->budget == nullptr
        || m_state.use_count() != 1) {
        return {};
    }

    const std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState>& budget = m_state->budget;
    std::scoped_lock lock(budget->mutex);
    if (retainedByteCount > m_state->reservedByteCount) {
        return {};
    }

    budget->reservedByteCount -= m_state->reservedByteCount - retainedByteCount;
    m_state->reservedByteCount = retainedByteCount;
    return ImageDecodeWorkspaceHold(std::exchange(m_state, {}));
}

qsizetype ImageDecodeWorkspaceLease::reservedByteCount() const
{
    if (m_state == nullptr || m_state->budget == nullptr) {
        return 0;
    }
    std::scoped_lock lock(m_state->budget->mutex);
    return m_state->reservedByteCount;
}

bool ImageDecodeWorkspaceLease::isManaged() const { return m_state != nullptr; }

ImageDecodeWorkspaceBudget::ImageDecodeWorkspaceBudget(
    qsizetype aggregateByteLimit, qsizetype perOperationByteLimit)
    : m_state(std::make_shared<ImageDecodeWorkspaceDetail::BudgetState>())
{
    m_state->aggregateByteLimit = std::max(qsizetype(0), aggregateByteLimit);
    m_state->perOperationByteLimit
        = std::clamp(perOperationByteLimit, qsizetype(0), m_state->aggregateByteLimit);
}

ImageDecodeWorkspaceLease ImageDecodeWorkspaceBudget::startLease() const
{
    return ImageDecodeWorkspaceLease(
        std::make_shared<ImageDecodeWorkspaceDetail::LeaseState>(m_state));
}

qsizetype ImageDecodeWorkspaceBudget::aggregateByteLimit() const
{
    return m_state->aggregateByteLimit;
}

qsizetype ImageDecodeWorkspaceBudget::perOperationByteLimit() const
{
    return m_state->perOperationByteLimit;
}

qsizetype ImageDecodeWorkspaceBudget::reservedByteCount() const
{
    std::scoped_lock lock(m_state->mutex);
    return m_state->reservedByteCount;
}

ImageDecodeWorkspaceBudgetLimits resolvedImageDecodeWorkspaceBudgetLimits(
    ImageDecodeWorkspaceBudgetRequest request, SystemMemorySnapshot systemMemory)
{
    qsizetype aggregateByteLimit = request.aggregateByteLimit;
    if (aggregateByteLimit <= 0) {
        aggregateByteLimit = preferredAggregateByteLimit;
        if (systemMemory.physicalByteSize > 0) {
            aggregateByteLimit
                = std::min(aggregateByteLimit, systemMemory.physicalByteSize / systemMemoryDivisor);
        }
    }
    aggregateByteLimit = std::max(qsizetype(1), aggregateByteLimit);

    const qsizetype perOperationByteLimit
        = std::clamp(request.perOperationByteLimit > 0 ? request.perOperationByteLimit
                                                       : preferredPerOperationByteLimit,
            qsizetype(1), aggregateByteLimit);
    return ImageDecodeWorkspaceBudgetLimits { aggregateByteLimit, perOperationByteLimit };
}

std::shared_ptr<ImageDecodeWorkspaceBudget> defaultImageDecodeWorkspaceBudget(
    ImageDecodeWorkspaceBudgetRequest request, SystemMemorySnapshot systemMemory)
{
    if (systemMemory.physicalByteSize <= 0) {
        systemMemory = systemMemorySnapshot();
    }
    const ImageDecodeWorkspaceBudgetLimits limits
        = resolvedImageDecodeWorkspaceBudgetLimits(request, systemMemory);
    return std::make_shared<ImageDecodeWorkspaceBudget>(
        limits.aggregateByteLimit, limits.perOperationByteLimit);
}

QString imageDecodeWorkspaceResourceLimitDiagnostic()
{
    return QStringLiteral("image decode workspace exceeds the configured resource limit");
}
}
