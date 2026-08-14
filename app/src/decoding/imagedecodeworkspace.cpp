// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedecodeworkspace.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <Qt>
#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

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
enum class AdmissionStatus : quint8 {
    Pending,
    Delivering,
    Completed,
    Canceled,
};

constexpr std::size_t admissionPriorityCount = 3;

class AdmissionDeliveryRelay final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    void queueGrant() { Q_EMIT grantReady(); }

Q_SIGNALS:
    void grantReady();
};

std::optional<std::size_t> admissionPriorityIndex(ImageDecodeWorkspacePriority priority)
{
    switch (priority) {
    case ImageDecodeWorkspacePriority::Interactive:
        return 0;
    case ImageDecodeWorkspacePriority::Demanded:
        return 1;
    case ImageDecodeWorkspacePriority::Speculative:
        return 2;
    }
    return std::nullopt;
}

struct AdmissionState;

struct BudgetState
{
    qsizetype aggregateByteLimit = 0;
    qsizetype perOperationByteLimit = 0;
    qsizetype reservedByteCount = 0;
    std::optional<ImageDecodeWorkspaceLease> upstreamGrant;
    bool precharged = false;
    bool prechargedFinalized = false;
    std::array<std::deque<std::shared_ptr<AdmissionState>>, admissionPriorityCount> admissions;
    mutable std::mutex mutex;
};

void pumpAdmissions(const std::shared_ptr<BudgetState>& budget);

void releaseBudgetReservation(
    const std::shared_ptr<BudgetState>& budget, qsizetype releasedByteCount)
{
    if (budget == nullptr || releasedByteCount <= 0) {
        return;
    }

    bool shouldRepump = false;
    {
        const std::scoped_lock lock(budget->mutex);
        budget->reservedByteCount -= releasedByteCount;
        if (budget->precharged) {
            if (budget->prechargedFinalized && budget->upstreamGrant.has_value()) {
                (void)budget->upstreamGrant->release(releasedByteCount);
            }
        } else {
            shouldRepump = true;
        }
    }
    if (shouldRepump) {
        pumpAdmissions(budget);
    }
}

struct LeaseState
{
    explicit LeaseState(
        std::shared_ptr<BudgetState> budget, qsizetype perOperationBaselineByteCount = 0)
        : budget(std::move(budget))
        , perOperationBaselineByteCount(perOperationBaselineByteCount)
    {
    }

    ~LeaseState()
    {
        if (budget == nullptr) {
            return;
        }
        const std::shared_ptr<BudgetState> activeBudget = budget;
        const qsizetype releasedByteCount = reservedByteCount;
        releaseBudgetReservation(activeBudget, releasedByteCount);
    }

    std::shared_ptr<BudgetState> budget;
    qsizetype perOperationBaselineByteCount = 0;
    qsizetype reservedByteCount = 0;
    Q_DISABLE_COPY_MOVE(LeaseState)
};

void releaseAdmissionRelay(QObject* relay)
{
    if (relay != nullptr && !QCoreApplication::closingDown()) {
        relay->deleteLater();
    }
}

struct AdmissionState final : std::enable_shared_from_this<AdmissionState>
{
    AdmissionState(const std::shared_ptr<BudgetState>& activeBudget, QObject* activeReceiver,
        AdmissionDeliveryRelay* deliveryRelay,
        ImageDecodeWorkspaceAdmissionRequest admissionRequest,
        ImageDecodeWorkspaceGranted grantCallback)
        : budget(activeBudget)
        , receiver(activeReceiver)
        , relay(deliveryRelay)
        , request(admissionRequest)
        , granted(std::move(grantCallback))
    {
    }

    ~AdmissionState()
    {
        AdmissionDeliveryRelay* deliveryRelay = nullptr;
        {
            const std::scoped_lock lock(mutex);
            deliveryRelay = std::exchange(relay, nullptr);
        }
        releaseAdmissionRelay(deliveryRelay);
    }

    Q_DISABLE_COPY_MOVE(AdmissionState)

    [[nodiscard]] bool isPending() const
    {
        const std::scoped_lock lock(mutex);
        return status == AdmissionStatus::Pending;
    }

    void cancel()
    {
        ImageDecodeWorkspaceGranted abandonedCallback;
        std::optional<ImageDecodeWorkspaceLease> abandonedLease;
        AdmissionDeliveryRelay* abandonedRelay = nullptr;
        std::shared_ptr<BudgetState> activeBudget;
        bool shouldRepump = false;
        {
            const std::scoped_lock lock(mutex);
            if (status == AdmissionStatus::Completed || status == AdmissionStatus::Canceled) {
                return;
            }

            shouldRepump = status == AdmissionStatus::Pending;
            status = AdmissionStatus::Canceled;
            activeBudget = budget.lock();
            abandonedCallback = std::move(granted);
            abandonedLease = std::move(grantedLease);
            grantedLease.reset();
            abandonedRelay = std::exchange(relay, nullptr);
        }

        releaseAdmissionRelay(abandonedRelay);
        abandonedCallback = {};
        abandonedLease.reset();
        if (shouldRepump && activeBudget != nullptr) {
            pumpAdmissions(activeBudget);
        }
    }

    void queueGrant()
    {
        const std::scoped_lock lock(mutex);
        if (status == AdmissionStatus::Delivering && relay != nullptr) {
            relay->queueGrant();
        }
    }

    void deliver()
    {
        ImageDecodeWorkspaceGranted callback;
        std::optional<ImageDecodeWorkspaceLease> lease;
        AdmissionDeliveryRelay* deliveredRelay = nullptr;
        {
            const std::scoped_lock lock(mutex);
            if (status != AdmissionStatus::Delivering) {
                return;
            }

            deliveredRelay = std::exchange(relay, nullptr);
            if (receiver == nullptr) {
                status = AdmissionStatus::Canceled;
                granted = {};
                lease = std::move(grantedLease);
                grantedLease.reset();
            } else {
                status = AdmissionStatus::Completed;
                callback = std::move(granted);
                lease = std::move(grantedLease);
                grantedLease.reset();
            }
        }

        releaseAdmissionRelay(deliveredRelay);
        if (callback && lease.has_value()) {
            callback(std::move(*lease));
        }
    }

    void installLifetimeConnections(QObject* activeReceiver, QThread* ownerThread)
    {
        const std::weak_ptr<AdmissionState> guardedState(shared_from_this());
        QObject::connect(
            relay, &AdmissionDeliveryRelay::grantReady, activeReceiver,
            [guardedState]() {
                if (const std::shared_ptr<AdmissionState> state = guardedState.lock()) {
                    state->deliver();
                }
            },
            Qt::QueuedConnection);
        QObject::connect(
            activeReceiver, &QObject::destroyed, relay,
            [guardedState]() {
                if (const std::shared_ptr<AdmissionState> state = guardedState.lock()) {
                    state->cancel();
                }
            },
            Qt::DirectConnection);
        QObject::connect(
            ownerThread, &QThread::finished, relay,
            [guardedState, ownerThread]() {
                if (const std::shared_ptr<AdmissionState> state = guardedState.lock()) {
                    state->handleFinishedOwnerThread(ownerThread);
                }
            },
            Qt::DirectConnection);
    }

    void handleFinishedOwnerThread(QThread* finishedThread)
    {
        QThread* nextOwnerThread = nullptr;
        AdmissionDeliveryRelay* deliveryRelay = nullptr;
        {
            const std::scoped_lock lock(mutex);
            if (status == AdmissionStatus::Completed || status == AdmissionStatus::Canceled) {
                return;
            }
            if (receiver == nullptr || receiver->thread() == finishedThread) {
                nextOwnerThread = finishedThread;
            } else {
                nextOwnerThread = receiver->thread();
                deliveryRelay = relay;
            }
        }

        if (deliveryRelay == nullptr || nextOwnerThread == nullptr || !nextOwnerThread->isRunning()
            || !deliveryRelay->moveToThread(nextOwnerThread)) {
            cancel();
            return;
        }

        const std::weak_ptr<AdmissionState> guardedState(shared_from_this());
        QObject::connect(
            nextOwnerThread, &QThread::finished, deliveryRelay,
            [guardedState, nextOwnerThread]() {
                if (const std::shared_ptr<AdmissionState> state = guardedState.lock()) {
                    state->handleFinishedOwnerThread(nextOwnerThread);
                }
            },
            Qt::DirectConnection);
    }

    void setGrantedLease(std::shared_ptr<LeaseState> lease)
    {
        grantedLease.emplace(ImageDecodeWorkspaceLease(std::move(lease)));
        status = AdmissionStatus::Delivering;
    }

    std::weak_ptr<BudgetState> budget;
    QPointer<QObject> receiver;
    AdmissionDeliveryRelay* relay = nullptr;
    ImageDecodeWorkspaceAdmissionRequest request;
    ImageDecodeWorkspaceGranted granted;
    std::optional<ImageDecodeWorkspaceLease> grantedLease;
    AdmissionStatus status = AdmissionStatus::Pending;
    mutable std::mutex mutex;
};

std::optional<ImageDecodeWorkspaceAdmissionFailure> admissionRequestFailure(
    const BudgetState& budget, ImageDecodeWorkspaceAdmissionRequest request)
{
    if (!admissionPriorityIndex(request.priority).has_value() || request.additionalPeakByteCount < 0
        || request.perOperationBaselineByteCount < 0
        || request.additionalPeakByteCount
            > std::numeric_limits<qsizetype>::max() - request.perOperationBaselineByteCount) {
        return ImageDecodeWorkspaceAdmissionFailure::InvalidRequest;
    }

    if (request.additionalPeakByteCount > budget.aggregateByteLimit) {
        return ImageDecodeWorkspaceAdmissionFailure::AggregateLimitExceeded;
    }
    if (request.additionalPeakByteCount + request.perOperationBaselineByteCount
        > budget.perOperationByteLimit) {
        return ImageDecodeWorkspaceAdmissionFailure::PerOperationLimitExceeded;
    }
    return std::nullopt;
}

bool hasPendingAdmissionAtOrAbovePriority(
    const BudgetState& budget, ImageDecodeWorkspacePriority priority)
{
    const std::optional<std::size_t> requestedPriority = admissionPriorityIndex(priority);
    if (!requestedPriority.has_value()) {
        return true;
    }

    for (std::size_t index = 0; index <= *requestedPriority; ++index) {
        for (const std::shared_ptr<AdmissionState>& admission : budget.admissions.at(index)) {
            if (admission->isPending()) {
                return true;
            }
        }
    }
    return false;
}

bool hasAnyPendingAdmission(const BudgetState& budget)
{
    return hasPendingAdmissionAtOrAbovePriority(budget, ImageDecodeWorkspacePriority::Speculative);
}

void pumpAdmissions(const std::shared_ptr<BudgetState>& budget)
{
    if (budget == nullptr) {
        return;
    }

    std::vector<std::shared_ptr<AdmissionState>> deliveries;
    {
        const std::scoped_lock budgetLock(budget->mutex);
        while (true) {
            std::shared_ptr<AdmissionState> admission;
            std::deque<std::shared_ptr<AdmissionState>>* selectedQueue = nullptr;
            for (std::deque<std::shared_ptr<AdmissionState>>& queue : budget->admissions) {
                while (!queue.empty() && !queue.front()->isPending()) {
                    queue.pop_front();
                }
                if (!queue.empty()) {
                    admission = queue.front();
                    selectedQueue = &queue;
                    break;
                }
            }
            if (admission == nullptr || selectedQueue == nullptr) {
                break;
            }

            const qsizetype additionalByteCount = admission->request.additionalPeakByteCount;
            if (additionalByteCount > budget->aggregateByteLimit - budget->reservedByteCount) {
                break;
            }

            const auto lease = std::make_shared<LeaseState>(
                budget, admission->request.perOperationBaselineByteCount);
            {
                const std::scoped_lock admissionLock(admission->mutex);
                if (admission->status != AdmissionStatus::Pending) {
                    selectedQueue->pop_front();
                    continue;
                }
                lease->reservedByteCount = additionalByteCount;
                budget->reservedByteCount += additionalByteCount;
                admission->setGrantedLease(lease);
            }
            selectedQueue->pop_front();
            deliveries.push_back(std::move(admission));
        }
    }

    for (const std::shared_ptr<AdmissionState>& admission : deliveries) {
        admission->queueGrant();
    }
}
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

ImageDecodeWorkspaceLease ImageDecodeWorkspaceDetail::startLease(
    const ImageDecodeWorkspaceBudget& budget)
{
    return budget.startLease();
}

ImageDecodeWorkspaceLease ImageDecodeWorkspaceDetail::startLeaseForOperation(
    const ImageDecodeWorkspaceBudget& budget, qsizetype alreadyReservedByteCount)
{
    return budget.startLeaseForOperation(alreadyReservedByteCount);
}

bool ImageDecodeWorkspaceDetail::tryReserve(
    ImageDecodeWorkspaceLease& lease, qsizetype additionalByteCount)
{
    return lease.tryReserve(additionalByteCount);
}

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
    if ((budget->precharged && budget->prechargedFinalized)
        || ImageDecodeWorkspaceDetail::hasAnyPendingAdmission(*budget)
        || m_state->perOperationBaselineByteCount > budget->perOperationByteLimit
        || m_state->reservedByteCount
            > budget->perOperationByteLimit - m_state->perOperationBaselineByteCount
        || additionalByteCount > budget->perOperationByteLimit
                - m_state->perOperationBaselineByteCount - m_state->reservedByteCount
        || additionalByteCount > budget->aggregateByteLimit - budget->reservedByteCount) {
        return false;
    }

    m_state->reservedByteCount += additionalByteCount;
    budget->reservedByteCount += additionalByteCount;
    return true;
}

bool ImageDecodeWorkspaceLease::release(qsizetype byteCount)
{
    if (byteCount < 0 || m_state == nullptr || m_state->budget == nullptr) {
        return false;
    }
    if (byteCount == 0) {
        return true;
    }

    const std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState>& budget = m_state->budget;
    {
        std::scoped_lock lock(budget->mutex);
        if (byteCount > m_state->reservedByteCount) {
            return false;
        }

        m_state->reservedByteCount -= byteCount;
    }
    ImageDecodeWorkspaceDetail::releaseBudgetReservation(budget, byteCount);
    return true;
}

ImageDecodeWorkspaceHold ImageDecodeWorkspaceLease::sharedHold() const
{
    return m_state == nullptr ? ImageDecodeWorkspaceHold {} : ImageDecodeWorkspaceHold(m_state);
}

ImageDecodeWorkspaceHold ImageDecodeWorkspaceLease::splitRetained(qsizetype retainedByteCount)
{
    if (retainedByteCount <= 0 || m_state == nullptr || m_state->budget == nullptr
        || m_state.use_count() != 1) {
        return {};
    }

    const std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState>& budget = m_state->budget;
    std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> retainedState;
    try {
        retainedState = std::make_shared<ImageDecodeWorkspaceDetail::LeaseState>(budget);
    } catch (const std::bad_alloc&) {
        return {};
    }
    std::scoped_lock lock(budget->mutex);
    if (retainedByteCount > m_state->reservedByteCount) {
        return {};
    }

    m_state->reservedByteCount -= retainedByteCount;
    retainedState->reservedByteCount = retainedByteCount;
    return ImageDecodeWorkspaceHold(std::move(retainedState));
}

ImageDecodeWorkspaceHold ImageDecodeWorkspaceLease::retainOnly(qsizetype retainedByteCount)
{
    if (retainedByteCount < 0 || m_state == nullptr || m_state->budget == nullptr
        || m_state.use_count() != 1) {
        return {};
    }

    const std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState>& budget = m_state->budget;
    std::shared_ptr<ImageDecodeWorkspaceDetail::LeaseState> retainedState;
    qsizetype releasedByteCount = 0;
    {
        std::scoped_lock lock(budget->mutex);
        if (retainedByteCount > m_state->reservedByteCount) {
            return {};
        }

        releasedByteCount = m_state->reservedByteCount - retainedByteCount;
        m_state->reservedByteCount = retainedByteCount;
        retainedState = std::exchange(m_state, {});
    }
    ImageDecodeWorkspaceDetail::releaseBudgetReservation(budget, releasedByteCount);
    return ImageDecodeWorkspaceHold(std::move(retainedState));
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

ImageDecodeWorkspaceAdmission::ImageDecodeWorkspaceAdmission() = default;

ImageDecodeWorkspaceAdmission::ImageDecodeWorkspaceAdmission(
    std::shared_ptr<ImageDecodeWorkspaceDetail::AdmissionState> state)
    : m_state(std::move(state))
{
}

ImageDecodeWorkspaceAdmission::~ImageDecodeWorkspaceAdmission() { cancel(); }

ImageDecodeWorkspaceAdmission::ImageDecodeWorkspaceAdmission(
    ImageDecodeWorkspaceAdmission&& other) noexcept
    = default;

ImageDecodeWorkspaceAdmission& ImageDecodeWorkspaceAdmission::operator=(
    ImageDecodeWorkspaceAdmission&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    cancel();
    m_state = std::move(other.m_state);
    return *this;
}

void ImageDecodeWorkspaceAdmission::cancel()
{
    if (m_state != nullptr) {
        m_state->cancel();
    }
}

bool ImageDecodeWorkspaceAdmission::isPending() const
{
    return m_state != nullptr && m_state->isPending();
}

ImageDecodeWorkspaceBudget::ImageDecodeWorkspaceBudget(
    qsizetype aggregateByteLimit, qsizetype perOperationByteLimit)
    : m_state(std::make_shared<ImageDecodeWorkspaceDetail::BudgetState>())
{
    m_state->aggregateByteLimit = std::max(qsizetype(0), aggregateByteLimit);
    m_state->perOperationByteLimit
        = std::clamp(perOperationByteLimit, qsizetype(0), m_state->aggregateByteLimit);
}

ImageDecodeWorkspaceBudget::ImageDecodeWorkspaceBudget(
    std::shared_ptr<ImageDecodeWorkspaceDetail::BudgetState> state)
    : m_state(std::move(state))
{
}

ImageDecodeWorkspaceLease ImageDecodeWorkspaceBudget::startLease() const
{
    return ImageDecodeWorkspaceLease(
        std::make_shared<ImageDecodeWorkspaceDetail::LeaseState>(m_state));
}

ImageDecodeWorkspaceLease ImageDecodeWorkspaceBudget::startLeaseForOperation(
    qsizetype alreadyReservedByteCount) const
{
    if (alreadyReservedByteCount < 0) {
        return {};
    }
    return ImageDecodeWorkspaceLease(std::make_shared<ImageDecodeWorkspaceDetail::LeaseState>(
        m_state, alreadyReservedByteCount));
}

std::expected<ImageDecodeWorkspaceAdmission, ImageDecodeWorkspaceAdmissionFailure>
ImageDecodeWorkspaceBudget::requestAdmission(QObject* receiver,
    ImageDecodeWorkspaceAdmissionRequest request, ImageDecodeWorkspaceGranted granted) const
{
    if (receiver == nullptr || !granted) {
        return std::unexpected(ImageDecodeWorkspaceAdmissionFailure::InvalidRequest);
    }
    if (const std::optional<ImageDecodeWorkspaceAdmissionFailure> failure
        = ImageDecodeWorkspaceDetail::admissionRequestFailure(*m_state, request)) {
        return std::unexpected(*failure);
    }
    {
        const std::scoped_lock lock(m_state->mutex);
        if (m_state->precharged) {
            return std::unexpected(ImageDecodeWorkspaceAdmissionFailure::InvalidRequest);
        }
    }

    QThread* ownerThread = receiver->thread();
    auto relayOwner = std::make_unique<ImageDecodeWorkspaceDetail::AdmissionDeliveryRelay>();
    if (ownerThread == nullptr || !relayOwner->moveToThread(ownerThread)) {
        return std::unexpected(ImageDecodeWorkspaceAdmissionFailure::InvalidRequest);
    }

    ImageDecodeWorkspaceDetail::AdmissionDeliveryRelay* relay = relayOwner.release();
    auto admission = std::make_shared<ImageDecodeWorkspaceDetail::AdmissionState>(
        m_state, receiver, relay, request, std::move(granted));
    admission->installLifetimeConnections(receiver, ownerThread);
    {
        const std::scoped_lock lock(m_state->mutex);
        const std::optional<std::size_t> priority
            = ImageDecodeWorkspaceDetail::admissionPriorityIndex(request.priority);
        m_state->admissions.at(*priority).push_back(admission);
    }
    ImageDecodeWorkspaceDetail::pumpAdmissions(m_state);
    return ImageDecodeWorkspaceAdmission(std::move(admission));
}

std::optional<ImageDecodeWorkspaceLease> ImageDecodeWorkspaceBudget::tryBestEffortAdmission(
    ImageDecodeWorkspaceAdmissionRequest request) const
{
    if (ImageDecodeWorkspaceDetail::admissionRequestFailure(*m_state, request).has_value()) {
        return std::nullopt;
    }

    const std::scoped_lock lock(m_state->mutex);
    if (m_state->precharged
        || ImageDecodeWorkspaceDetail::hasPendingAdmissionAtOrAbovePriority(
            *m_state, request.priority)
        || request.additionalPeakByteCount
            > m_state->aggregateByteLimit - m_state->reservedByteCount) {
        return std::nullopt;
    }

    auto lease = std::make_shared<ImageDecodeWorkspaceDetail::LeaseState>(
        m_state, request.perOperationBaselineByteCount);
    lease->reservedByteCount = request.additionalPeakByteCount;
    m_state->reservedByteCount += request.additionalPeakByteCount;
    return ImageDecodeWorkspaceLease(std::move(lease));
}

std::optional<ImageDecodeWorkspaceLease> ImageDecodeWorkspaceDetail::tryBestEffortAdmission(
    const ImageDecodeWorkspaceBudget& budget, ImageDecodeWorkspaceAdmissionRequest request)
{
    return budget.tryBestEffortAdmission(request);
}

void ImageDecodeWorkspaceBudget::finalizePrechargedAdmission() const
{
    const std::scoped_lock lock(m_state->mutex);
    if (!m_state->precharged || m_state->prechargedFinalized) {
        return;
    }

    m_state->prechargedFinalized = true;
    if (!m_state->upstreamGrant.has_value()) {
        return;
    }
    const qsizetype upstreamReservedByteCount = m_state->upstreamGrant->reservedByteCount();
    if (upstreamReservedByteCount > m_state->reservedByteCount) {
        (void)m_state->upstreamGrant->release(
            upstreamReservedByteCount - m_state->reservedByteCount);
    }
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

std::shared_ptr<ImageDecodeWorkspaceBudget> imageDecodeWorkspaceBudgetForSystemMemory(
    ImageDecodeWorkspaceBudgetRequest request, SystemMemorySnapshot systemMemory)
{
    const ImageDecodeWorkspaceBudgetLimits limits
        = resolvedImageDecodeWorkspaceBudgetLimits(request, systemMemory);
    return std::make_shared<ImageDecodeWorkspaceBudget>(
        limits.aggregateByteLimit, limits.perOperationByteLimit);
}

std::shared_ptr<ImageDecodeWorkspaceBudget> defaultImageDecodeWorkspaceBudget(
    ImageDecodeWorkspaceBudgetRequest request, SystemMemoryRuntime runtime)
{
    return imageDecodeWorkspaceBudgetForSystemMemory(
        request, systemMemorySnapshot(std::move(runtime)));
}

std::shared_ptr<ImageDecodeWorkspaceBudget> prechargedImageDecodeWorkspaceBudget(
    ImageDecodeWorkspaceLease grant, qsizetype perOperationBaselineByteCount)
{
    if (!grant.isManaged() || perOperationBaselineByteCount < 0) {
        return {};
    }

    const qsizetype admittedByteCount = grant.reservedByteCount();
    if (admittedByteCount > std::numeric_limits<qsizetype>::max() - perOperationBaselineByteCount) {
        return {};
    }

    auto state = std::make_shared<ImageDecodeWorkspaceDetail::BudgetState>();
    state->aggregateByteLimit = admittedByteCount;
    state->perOperationByteLimit = admittedByteCount + perOperationBaselineByteCount;
    state->precharged = true;
    state->upstreamGrant.emplace(std::move(grant));
    return std::shared_ptr<ImageDecodeWorkspaceBudget>(
        new ImageDecodeWorkspaceBudget(std::move(state)));
}

QString imageDecodeWorkspaceResourceLimitDiagnostic()
{
    return QStringLiteral("image decode workspace exceeds the configured resource limit");
}

std::optional<qsizetype> checkedImageDecodeWorkspaceByteCount(
    QSize imageSize, qsizetype bytesPerPixel, qsizetype bufferCount)
{
    if (imageSize.isEmpty() || bytesPerPixel <= 0 || bufferCount <= 0) {
        return std::nullopt;
    }

    constexpr qsizetype maximum = std::numeric_limits<qsizetype>::max();
    const qsizetype width = imageSize.width();
    const qsizetype height = imageSize.height();
    if (width > maximum / height) {
        return std::nullopt;
    }
    const qsizetype pixelCount = width * height;
    if (pixelCount > maximum / bytesPerPixel) {
        return std::nullopt;
    }
    const qsizetype frameByteCount = pixelCount * bytesPerPixel;
    if (frameByteCount > maximum / bufferCount) {
        return std::nullopt;
    }
    return frameByteCount * bufferCount;
}
}

#include "imagedecodeworkspace.moc"
