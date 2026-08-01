// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagesourcedata.h"

#include <QIODevice>
#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

namespace {
constexpr qsizetype preferredAggregateByteLimit = qsizetype { 1024 } * 1024 * 1024;
constexpr qsizetype preferredPerSourceByteLimit = qsizetype { 512 } * 1024 * 1024;
constexpr qsizetype systemMemoryDivisor = 8;
constexpr qsizetype readChunkByteCount = qsizetype { 64 } * 1024;
}

namespace kiriview::ImageSourceDataDetail {
struct BudgetState
{
    qsizetype aggregateByteLimit = 0;
    qsizetype perSourceByteLimit = 0;
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
ImageSourceDataLease::ImageSourceDataLease(std::shared_ptr<ImageSourceDataDetail::LeaseState> state)
    : m_state(std::move(state))
{
}

bool ImageSourceDataLease::tryReserve(qsizetype additionalByteCount)
{
    if (additionalByteCount < 0) {
        return false;
    }
    if (additionalByteCount == 0 || m_state == nullptr) {
        return true;
    }

    const std::shared_ptr<ImageSourceDataDetail::BudgetState>& budget = m_state->budget;
    if (budget == nullptr) {
        return true;
    }
    std::scoped_lock lock(budget->mutex);
    if (additionalByteCount > budget->perSourceByteLimit - m_state->reservedByteCount
        || additionalByteCount > budget->aggregateByteLimit - budget->reservedByteCount) {
        return false;
    }

    m_state->reservedByteCount += additionalByteCount;
    budget->reservedByteCount += additionalByteCount;
    return true;
}

qsizetype ImageSourceDataLease::reservedByteCount() const
{
    if (m_state == nullptr || m_state->budget == nullptr) {
        return 0;
    }
    std::scoped_lock lock(m_state->budget->mutex);
    return m_state->reservedByteCount;
}

bool ImageSourceDataLease::isManaged() const { return m_state != nullptr; }

ImageSourceDataBudget::ImageSourceDataBudget(
    qsizetype aggregateByteLimit, qsizetype perSourceByteLimit)
    : m_state(std::make_shared<ImageSourceDataDetail::BudgetState>())
{
    m_state->aggregateByteLimit = std::max(qsizetype(0), aggregateByteLimit);
    m_state->perSourceByteLimit
        = std::clamp(perSourceByteLimit, qsizetype(0), m_state->aggregateByteLimit);
}

ImageSourceDataLease ImageSourceDataBudget::startLease() const
{
    return ImageSourceDataLease(std::make_shared<ImageSourceDataDetail::LeaseState>(m_state));
}

qsizetype ImageSourceDataBudget::aggregateByteLimit() const { return m_state->aggregateByteLimit; }

qsizetype ImageSourceDataBudget::perSourceByteLimit() const { return m_state->perSourceByteLimit; }

qsizetype ImageSourceDataBudget::reservedByteCount() const
{
    std::scoped_lock lock(m_state->mutex);
    return m_state->reservedByteCount;
}

ImageSourceData::ImageSourceData(QByteArray sourceData, ImageSourceDataLease sourceLease)
    : data(std::move(sourceData))
    , lease(std::move(sourceLease))
{
}

bool ImageSourceData::tryReserveExpectedByteCount(qint64 expectedByteCount)
{
    if (expectedByteCount < 0
        || static_cast<quint64>(expectedByteCount)
            > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
        return false;
    }

    const qsizetype expected = static_cast<qsizetype>(expectedByteCount);
    const qsizetype reserved = lease.reservedByteCount();
    return expected <= reserved || lease.tryReserve(expected - reserved);
}

bool ImageSourceData::tryAppend(QByteArrayView chunk)
{
    if (chunk.size() > std::numeric_limits<qsizetype>::max() - data.size()) {
        return false;
    }
    const qsizetype requiredByteCount = data.size() + chunk.size();
    const qsizetype reservedByteCount = lease.reservedByteCount();
    if (requiredByteCount > reservedByteCount
        && !lease.tryReserve(requiredByteCount - reservedByteCount)) {
        return false;
    }

    data.append(chunk.data(), chunk.size());
    return true;
}

ImageSourceDataBudgetLimits resolvedImageSourceDataBudgetLimits(
    ImageSourceDataBudgetRequest request, SystemMemorySnapshot systemMemory)
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

    const qsizetype perSourceByteLimit = std::clamp(
        request.perSourceByteLimit > 0 ? request.perSourceByteLimit : preferredPerSourceByteLimit,
        qsizetype(1), aggregateByteLimit);
    return ImageSourceDataBudgetLimits { aggregateByteLimit, perSourceByteLimit };
}

std::shared_ptr<ImageSourceDataBudget> defaultImageSourceDataBudget(
    ImageSourceDataBudgetRequest request, SystemMemorySnapshot systemMemory)
{
    if (systemMemory.physicalByteSize <= 0) {
        systemMemory = systemMemorySnapshot();
    }
    const ImageSourceDataBudgetLimits limits
        = resolvedImageSourceDataBudgetLimits(request, systemMemory);
    return std::make_shared<ImageSourceDataBudget>(
        limits.aggregateByteLimit, limits.perSourceByteLimit);
}

ImageSourceDataReadResult readImageSourceData(
    QIODevice& device, ImageSourceDataLease lease, qint64 expectedByteCount)
{
    if (!lease.isManaged()) {
        lease = defaultImageSourceDataBudget()->startLease();
    }
    ImageSourceData sourceData({}, std::move(lease));
    if (expectedByteCount >= 0 && !sourceData.tryReserveExpectedByteCount(expectedByteCount)) {
        return ImageSourceDataReadResult { ImageSourceDataReadStatus::ResourceLimitExceeded, {},
            imageSourceDataResourceLimitDiagnostic() };
    }

    QByteArray buffer(readChunkByteCount, Qt::Uninitialized);
    while (true) {
        const qint64 bytesRead = device.read(buffer.data(), buffer.size());
        if (bytesRead == 0) {
            break;
        }
        if (bytesRead < 0) {
            return ImageSourceDataReadResult { ImageSourceDataReadStatus::ReadFailed, {},
                device.errorString() };
        }
        if (!sourceData.tryAppend(
                QByteArrayView(buffer.constData(), static_cast<qsizetype>(bytesRead)))) {
            return ImageSourceDataReadResult { ImageSourceDataReadStatus::ResourceLimitExceeded, {},
                imageSourceDataResourceLimitDiagnostic() };
        }
    }

    if (expectedByteCount >= 0 && sourceData.data.size() != expectedByteCount) {
        return ImageSourceDataReadResult { ImageSourceDataReadStatus::ReadFailed, {},
            QStringLiteral("image source byte count did not match the declared size") };
    }

    return ImageSourceDataReadResult { ImageSourceDataReadStatus::Ready, std::move(sourceData),
        {} };
}

QString imageSourceDataResourceLimitDiagnostic()
{
    return QStringLiteral("image source data exceeds the configured resource limit");
}
}
