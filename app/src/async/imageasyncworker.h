// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEASYNCWORKER_H
#define KIRIVIEW_IMAGEASYNCWORKER_H

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <Qt>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace kiriview {
class ImageWorkerTaskState final
{
public:
    using CancelCallback = std::move_only_function<void()>;
    using RetirementCallback = std::function<void()>;

    explicit ImageWorkerTaskState(CancelCallback cancelCallback)
        : m_cancelCallback(std::move(cancelCallback))
        , m_active(true)
    {
    }

    bool isActive() const
    {
        const std::scoped_lock lock(m_mutex);
        return m_active;
    }

    void cancel()
    {
        CancelCallback cancelCallback;
        {
            const std::scoped_lock lock(m_mutex);
            if (!m_active) {
                return;
            }
            m_active = false;
            cancelCallback = std::move(m_cancelCallback);
        }
        if (cancelCallback) {
            cancelCallback();
        }
    }

    template <typename Finish> bool claimAndRun(Finish&& finish)
    {
        {
            const std::scoped_lock lock(m_mutex);
            if (!m_active) {
                return false;
            }
            m_active = false;
            m_cancelCallback = {};
        }
        std::forward<Finish>(finish)();
        return true;
    }

    void setRetirementCallback(RetirementCallback callback)
    {
        RetirementCallback immediateCallback;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_retired) {
                immediateCallback = std::move(callback);
            } else {
                m_retirementCallback = std::move(callback);
            }
        }
        if (immediateCallback) {
            immediateCallback();
        }
    }

    void retire()
    {
        RetirementCallback callback;
        {
            const std::scoped_lock lock(m_mutex);
            if (m_retired) {
                return;
            }
            m_retired = true;
            callback = std::move(m_retirementCallback);
        }
        if (callback) {
            callback();
        }
    }

private:
    mutable std::mutex m_mutex;
    CancelCallback m_cancelCallback;
    RetirementCallback m_retirementCallback;
    bool m_active = false;
    bool m_retired = false;
};

class ImageWorkerTaskCompletion final
{
public:
    ImageWorkerTaskCompletion() = default;
    explicit ImageWorkerTaskCompletion(std::shared_ptr<ImageWorkerTaskState> state)
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] bool isActive() const { return m_state != nullptr && m_state->isActive(); }

    void cancel() const
    {
        if (m_state != nullptr) {
            m_state->cancel();
        }
    }

    template <typename Finish> bool claimAndRun(Finish&& finish) const
    {
        return m_state != nullptr && m_state->claimAndRun(std::forward<Finish>(finish));
    }

    void retire() const
    {
        if (m_state != nullptr) {
            m_state->retire();
        }
    }

private:
    std::shared_ptr<ImageWorkerTaskState> m_state;
};

class ImageWorkerTask final
{
public:
    ImageWorkerTask() = default;
    explicit ImageWorkerTask(ImageWorkerTaskState::CancelCallback cancelCallback)
        : m_state(std::make_shared<ImageWorkerTaskState>(std::move(cancelCallback)))
    {
    }
    ~ImageWorkerTask() { cancel(); }

    ImageWorkerTask(const ImageWorkerTask&) = delete;
    ImageWorkerTask& operator=(const ImageWorkerTask&) = delete;
    ImageWorkerTask(ImageWorkerTask&&) noexcept = default;
    ImageWorkerTask& operator=(ImageWorkerTask&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        cancel();
        m_state = std::move(other.m_state);
        return *this;
    }

    void cancel()
    {
        if (m_state != nullptr) {
            m_state->cancel();
        }
    }

    [[nodiscard]] bool isActive() const { return m_state != nullptr && m_state->isActive(); }

    [[nodiscard]] ImageWorkerTaskCompletion completion() const
    {
        return ImageWorkerTaskCompletion(m_state);
    }

    void setRetirementCallback(ImageWorkerTaskState::RetirementCallback callback)
    {
        if (m_state == nullptr) {
            if (callback) {
                callback();
            }
            return;
        }
        m_state->setRetirementCallback(std::move(callback));
    }

private:
    std::shared_ptr<ImageWorkerTaskState> m_state;
};

namespace Detail {
    struct DeleteAsyncWorkerRelayLater final
    {
        void operator()(QObject* relay) const
        {
            if (relay != nullptr && !QCoreApplication::closingDown()) {
                relay->deleteLater();
            }
        }
    };

    using AsyncWorkerRelayOwner = std::unique_ptr<QObject, DeleteAsyncWorkerRelayLater>;

    class AsyncWorkerDeliveryState final
    {
    public:
        AsyncWorkerDeliveryState(QPointer<QObject> guardedContext, QObject* relay)
            : m_guardedContext(std::move(guardedContext))
            , m_relay(relay)
        {
        }

        ~AsyncWorkerDeliveryState() { releaseRelay(); }

        AsyncWorkerDeliveryState(const AsyncWorkerDeliveryState&) = delete;
        AsyncWorkerDeliveryState& operator=(const AsyncWorkerDeliveryState&) = delete;
        AsyncWorkerDeliveryState(AsyncWorkerDeliveryState&&) = delete;
        AsyncWorkerDeliveryState& operator=(AsyncWorkerDeliveryState&&) = delete;

        template <typename Queue> bool queue(Queue&& queue)
        {
            const std::scoped_lock lock(m_mutex);
            return m_relay != nullptr && std::forward<Queue>(queue)(m_relay);
        }

        QPointer<QObject> guardedContext() const { return m_guardedContext; }

        AsyncWorkerRelayOwner takeRelay()
        {
            const std::scoped_lock lock(m_mutex);
            return AsyncWorkerRelayOwner(std::exchange(m_relay, nullptr));
        }

        void releaseRelay() { takeRelay().reset(); }

    private:
        mutable std::mutex m_mutex;
        QPointer<QObject> m_guardedContext;
        QObject* m_relay = nullptr;
    };

    inline std::shared_ptr<AsyncWorkerDeliveryState> createAsyncWorkerDeliveryState(
        QObject* context)
    {
        auto relay = AsyncWorkerRelayOwner(new QObject);
        QThread* ownerThread = context->thread();
        if (ownerThread == nullptr || !relay->moveToThread(ownerThread)) {
            return {};
        }

        QObject* relayObject = relay.get();
        auto delivery = std::make_shared<AsyncWorkerDeliveryState>(
            QPointer<QObject>(context), relay.release());
        const std::weak_ptr<AsyncWorkerDeliveryState> guardedDelivery(delivery);
        QObject::connect(
            ownerThread, &QThread::finished, relayObject,
            [guardedDelivery]() {
                if (const auto activeDelivery = guardedDelivery.lock()) {
                    activeDelivery->releaseRelay();
                }
            },
            Qt::DirectConnection);
        return delivery;
    }

    class AsyncWorkerQueueState final
    {
    public:
        explicit AsyncWorkerQueueState(QThreadPool* pool)
            : m_pool(pool)
        {
        }

        void setRunnable(QRunnable* runnable)
        {
            const std::scoped_lock lock(m_mutex);
            m_runnable = runnable;
        }

        void markStarted(QRunnable* runnable)
        {
            const std::scoped_lock lock(m_mutex);
            if (m_runnable == runnable) {
                m_runnable = nullptr;
            }
        }

        void withdrawQueued()
        {
            QRunnable* withdrawn = nullptr;
            {
                const std::scoped_lock lock(m_mutex);
                if (m_pool != nullptr && m_runnable != nullptr && m_pool->tryTake(m_runnable)) {
                    withdrawn = m_runnable;
                    m_runnable = nullptr;
                }
            }
            delete withdrawn;
        }

    private:
        std::mutex m_mutex;
        QPointer<QThreadPool> m_pool;
        QRunnable* m_runnable = nullptr;
    };

    class AsyncWorkerRetirement
    {
    public:
        explicit AsyncWorkerRetirement(ImageWorkerTaskCompletion completion)
            : m_completion(std::move(completion))
        {
        }

        ~AsyncWorkerRetirement() { m_completion.retire(); }

        AsyncWorkerRetirement(const AsyncWorkerRetirement&) = delete;
        AsyncWorkerRetirement& operator=(const AsyncWorkerRetirement&) = delete;
        AsyncWorkerRetirement(AsyncWorkerRetirement&&) = delete;
        AsyncWorkerRetirement& operator=(AsyncWorkerRetirement&&) = delete;

    protected:
        void disarm() { m_completion = {}; }

    private:
        ImageWorkerTaskCompletion m_completion;
    };

    template <typename Result, typename Finish> class AsyncWorkerDeliveryPayload final
    {
    public:
        AsyncWorkerDeliveryPayload(
            ImageWorkerTaskCompletion completion, Result resultValue, Finish finishValue)
            : m_completion(std::move(completion))
            , m_result(std::move(resultValue))
            , m_finish(std::move(finishValue))
        {
        }

        ~AsyncWorkerDeliveryPayload()
        {
            m_finish.reset();
            m_result.reset();
            m_completion.retire();
        }

        Q_DISABLE_COPY_MOVE(AsyncWorkerDeliveryPayload)

        void deliver()
        {
            if (!m_result.has_value() || !m_finish.has_value()) {
                return;
            }
            Finish finish = std::move(*m_finish);
            Result result = std::move(*m_result);
            m_finish.reset();
            m_result.reset();
            finish(std::move(result));
        }

    private:
        ImageWorkerTaskCompletion m_completion;
        std::optional<Result> m_result;
        std::optional<Finish> m_finish;
    };

    template <typename Work, typename Finish>
    class AsyncWorkerRunnable final : public QRunnable, private AsyncWorkerRetirement
    {
    public:
        using Result = std::invoke_result_t<Work&>;

        AsyncWorkerRunnable(std::shared_ptr<AsyncWorkerQueueState> queueState,
            ImageWorkerTaskCompletion taskCompletion,
            std::shared_ptr<AsyncWorkerDeliveryState> delivery, Work work, Finish finish)
            : AsyncWorkerRetirement(taskCompletion)
            , m_queueState(std::move(queueState))
            , m_taskCompletion(std::move(taskCompletion))
            , m_delivery(std::move(delivery))
            , m_work(std::move(work))
            , m_finish(std::move(finish))
        {
            setAutoDelete(false);
        }

        ~AsyncWorkerRunnable() override = default;
        Q_DISABLE_COPY_MOVE(AsyncWorkerRunnable)

        void run() override
        {
            m_queueState->markStarted(this);
            if (!m_taskCompletion.isActive()) {
                delete this;
                return;
            }

            {
                Result result = m_work();
                if (m_taskCompletion.isActive()) {
                    using DeliveryPayload
                        = AsyncWorkerDeliveryPayload<std::decay_t<Result>, std::decay_t<Finish>>;
                    auto payload = std::make_shared<DeliveryPayload>(
                        m_taskCompletion, std::move(result), std::move(m_finish));
                    disarm();

                    const std::shared_ptr<AsyncWorkerDeliveryState> delivery = m_delivery;
                    ImageWorkerTaskCompletion taskCompletion = m_taskCompletion;
                    const bool queued = delivery->queue([&](QObject* relay) {
                        return QMetaObject::invokeMethod(
                            relay,
                            [delivery, taskCompletion, payload]() mutable {
                                AsyncWorkerRelayOwner relayOwner = delivery->takeRelay();
                                if (relayOwner == nullptr
                                    || delivery->guardedContext() == nullptr) {
                                    taskCompletion.cancel();
                                    return;
                                }
                                taskCompletion.claimAndRun([&]() mutable { payload->deliver(); });
                            },
                            Qt::QueuedConnection);
                    });
                    if (!queued) {
                        delivery->releaseRelay();
                        m_taskCompletion.cancel();
                    }
                }
            }
            delete this;
        }

    private:
        std::shared_ptr<AsyncWorkerQueueState> m_queueState;
        ImageWorkerTaskCompletion m_taskCompletion;
        std::shared_ptr<AsyncWorkerDeliveryState> m_delivery;
        Work m_work;
        Finish m_finish;
    };
}

template <typename Work, typename Finish>
ImageWorkerTask runAsyncWorker(QThreadPool* pool, QObject* context, Work work, Finish finish)
{
    using Result = std::invoke_result_t<Work&>;
    static_assert(!std::is_void_v<Result>, "runAsyncWorker requires a non-void work result");

    if (pool == nullptr || context == nullptr) {
        finish(work());
        return {};
    }

    if (QCoreApplication::instance() == nullptr) {
        finish(work());
        return {};
    }

    std::shared_ptr<Detail::AsyncWorkerDeliveryState> delivery
        = Detail::createAsyncWorkerDeliveryState(context);
    if (delivery == nullptr) {
        return {};
    }

    auto queueState = std::make_shared<Detail::AsyncWorkerQueueState>(pool);
    ImageWorkerTask task([queueState]() { queueState->withdrawQueued(); });
    auto* runnable = new Detail::AsyncWorkerRunnable<Work, Finish>(
        queueState, task.completion(), std::move(delivery), std::move(work), std::move(finish));
    queueState->setRunnable(runnable);
    pool->start(runnable);
    return task;
}

template <typename Work, typename Finish>
ImageWorkerTask runAsyncWorker(QObject* context, Work work, Finish finish)
{
    return runAsyncWorker(
        QThreadPool::globalInstance(), context, std::move(work), std::move(finish));
}
}

#endif
