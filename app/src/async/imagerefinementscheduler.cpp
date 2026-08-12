// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagerefinementscheduler.h"

#include <QMetaObject>
#include <QThreadPool>
#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>
#include <ranges>
#include <utility>

namespace kiriview {
namespace {
    class RefinementExecutionQueue;
    class RefinementDeliveryRetirement;

    struct RefinementExecution
    {
        quint64 id = 0;
        ImageWorkerOperation operation;
        ImageWorkerCompletion completion;
        ImageWorkerTaskCompletion taskCompletion;
        std::shared_ptr<Detail::AsyncWorkerDeliveryState> delivery;
    };

    class RefinementDeliveryRetirement final
    {
    public:
        RefinementDeliveryRetirement(std::shared_ptr<RefinementExecutionQueue> queue,
            std::shared_ptr<RefinementExecution> execution)
            : m_queue(std::move(queue))
            , m_execution(std::move(execution))
        {
        }

        ~RefinementDeliveryRetirement();

        Q_DISABLE_COPY_MOVE(RefinementDeliveryRetirement)

        void deliver();

    private:
        std::shared_ptr<RefinementExecutionQueue> m_queue;
        std::shared_ptr<RefinementExecution> m_execution;
    };

    class RefinementRunnable final : public QRunnable
    {
    public:
        RefinementRunnable(std::shared_ptr<RefinementExecutionQueue> queue,
            std::shared_ptr<RefinementExecution> execution)
            : m_queue(std::move(queue))
            , m_execution(std::move(execution))
        {
        }

        void run() override;

    private:
        std::shared_ptr<RefinementExecutionQueue> m_queue;
        std::shared_ptr<RefinementExecution> m_execution;
    };

    class RefinementExecutionQueue final
        : public std::enable_shared_from_this<RefinementExecutionQueue>
    {
    public:
        RefinementExecutionQueue() { m_pool.setMaxThreadCount(1); }

        ImageWorkerTask schedule(
            QObject* context, ImageWorkerOperation operation, ImageWorkerCompletion completion)
        {
            if (context == nullptr || !operation) {
                return {};
            }
            std::shared_ptr<Detail::AsyncWorkerDeliveryState> delivery
                = Detail::createAsyncWorkerDeliveryState(context);
            if (delivery == nullptr) {
                return {};
            }

            const auto execution = std::make_shared<RefinementExecution>();
            execution->operation = std::move(operation);
            execution->completion = std::move(completion);
            execution->delivery = std::move(delivery);
            {
                const std::scoped_lock lock(m_mutex);
                execution->id = reserveIdLocked();
            }
            const std::weak_ptr<RefinementExecutionQueue> weakQueue = weak_from_this();
            ImageWorkerTask task([weakQueue, id = execution->id]() {
                if (const std::shared_ptr<RefinementExecutionQueue> queue = weakQueue.lock()) {
                    queue->cancel(id);
                }
            });
            execution->taskCompletion = task.completion();

            std::shared_ptr<RefinementExecution> next;
            {
                const std::scoped_lock lock(m_mutex);
                m_queued.push_back(execution);
                next = takeNextLocked();
            }
            start(std::move(next));
            return task;
        }

        void workReturned(const std::shared_ptr<RefinementExecution>& execution)
        {
            execution->operation = {};
            const std::shared_ptr<RefinementExecutionQueue> queue = shared_from_this();
            const std::shared_ptr<Detail::AsyncWorkerDeliveryState> delivery = execution->delivery;
            const auto retirement
                = std::make_shared<RefinementDeliveryRetirement>(queue, execution);
            const bool queued = delivery->queue([&](QObject* relay) {
                return QMetaObject::invokeMethod(
                    relay, [retirement]() { retirement->deliver(); }, Qt::QueuedConnection);
            });
            if (!queued) {
                delivery->releaseRelay();
                execution->taskCompletion.cancel();
                execution->completion = {};
            }
        }

        void retire(const std::shared_ptr<RefinementExecution>& execution)
        {
            execution->operation = {};
            execution->completion = {};
            execution->delivery.reset();
            execution->taskCompletion.cancel();

            std::shared_ptr<RefinementExecution> next;
            {
                const std::scoped_lock lock(m_mutex);
                if (m_active != execution) {
                    return;
                }
                m_active.reset();
                next = takeNextLocked();
            }

            execution->taskCompletion.retire();
            start(std::move(next));
        }

    private:
        quint64 reserveIdLocked()
        {
            if (m_nextId == 0) {
                m_nextId = 1;
            }
            quint64 candidate = m_nextId;
            const auto idIsLive = [this](quint64 id) {
                return (m_active != nullptr && m_active->id == id)
                    || std::ranges::any_of(
                        m_queued, [id](const auto& queued) { return queued->id == id; });
            };
            while (idIsLive(candidate)) {
                ++candidate;
                if (candidate == 0) {
                    candidate = 1;
                }
            }
            m_nextId = candidate + 1;
            return candidate;
        }

        void cancel(quint64 id)
        {
            std::shared_ptr<RefinementExecution> canceled;
            {
                const std::scoped_lock lock(m_mutex);
                const auto queued = std::ranges::find_if(
                    m_queued, [id](const auto& execution) { return execution->id == id; });
                if (queued != m_queued.end()) {
                    canceled = std::move(*queued);
                    m_queued.erase(queued);
                }
            }

            if (canceled == nullptr) {
                return;
            }
            canceled->operation = {};
            canceled->completion = {};
            canceled->delivery.reset();
            canceled->taskCompletion.retire();
        }

        std::shared_ptr<RefinementExecution> takeNextLocked()
        {
            if (m_active != nullptr || m_queued.empty()) {
                return {};
            }
            m_active = std::move(m_queued.front());
            m_queued.pop_front();
            return m_active;
        }

        void start(std::shared_ptr<RefinementExecution> execution)
        {
            if (execution != nullptr) {
                m_pool.start(new RefinementRunnable(shared_from_this(), std::move(execution)));
            }
        }

        std::mutex m_mutex;
        QThreadPool m_pool;
        std::deque<std::shared_ptr<RefinementExecution>> m_queued;
        std::shared_ptr<RefinementExecution> m_active;
        quint64 m_nextId = 1;
    };

    RefinementDeliveryRetirement::~RefinementDeliveryRetirement() { m_queue->retire(m_execution); }

    void RefinementDeliveryRetirement::deliver()
    {
        const std::shared_ptr<Detail::AsyncWorkerDeliveryState> delivery = m_execution->delivery;
        Detail::AsyncWorkerRelayOwner relayOwner = delivery->takeRelay();
        if (relayOwner != nullptr && delivery->guardedContext() != nullptr) {
            m_execution->taskCompletion.claimAndRun([&]() mutable {
                if (m_execution->completion) {
                    m_execution->completion();
                }
            });
        } else {
            m_execution->taskCompletion.cancel();
        }
        m_execution->completion = {};
        m_execution->delivery.reset();
    }

    void RefinementRunnable::run()
    {
        if (m_execution->taskCompletion.isActive() && m_execution->operation) {
            m_execution->operation();
        }
        m_queue->workReturned(m_execution);
    }

    std::shared_ptr<RefinementExecutionQueue> sharedRefinementExecutionQueue()
    {
        static const std::shared_ptr<RefinementExecutionQueue> queue
            = std::make_shared<RefinementExecutionQueue>();
        return queue;
    }
}

ImageWorkerScheduler defaultImageRefinementScheduler()
{
    const std::shared_ptr<RefinementExecutionQueue> queue = sharedRefinementExecutionQueue();
    return ImageWorkerScheduler([queue](QObject* context, ImageWorkerOperation operation,
                                    ImageWorkerCompletion completion) {
        return queue->schedule(context, std::move(operation), std::move(completion));
    });
}
}
