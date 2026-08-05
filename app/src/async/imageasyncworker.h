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
#include <type_traits>
#include <utility>

namespace kiriview {
class ImageWorkerTaskState final
{
public:
    using CancelCallback = std::move_only_function<void()>;

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

private:
    mutable std::mutex m_mutex;
    CancelCallback m_cancelCallback;
    bool m_active = false;
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

private:
    std::shared_ptr<ImageWorkerTaskState> m_state;
};

namespace Detail {
    struct AsyncWorkerDeliveryState final
    {
        QPointer<QObject> guardedContext;
        std::shared_ptr<QObject> relay;
    };

    inline std::shared_ptr<AsyncWorkerDeliveryState> createAsyncWorkerDeliveryState(
        QObject* context)
    {
        auto relay
            = std::shared_ptr<QObject>(new QObject, [](QObject* object) { object->deleteLater(); });
        QThread* ownerThread = context->thread();
        if (ownerThread == nullptr || !relay->moveToThread(ownerThread)) {
            return {};
        }

        return std::make_shared<AsyncWorkerDeliveryState>(
            AsyncWorkerDeliveryState { QPointer<QObject>(context), std::move(relay) });
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

    template <typename Work, typename Finish> class AsyncWorkerRunnable final : public QRunnable
    {
    public:
        using Result = std::invoke_result_t<Work&>;

        AsyncWorkerRunnable(std::shared_ptr<AsyncWorkerQueueState> queueState,
            ImageWorkerTaskCompletion taskCompletion,
            std::shared_ptr<AsyncWorkerDeliveryState> delivery, Work work, Finish finish)
            : m_queueState(std::move(queueState))
            , m_taskCompletion(std::move(taskCompletion))
            , m_delivery(std::move(delivery))
            , m_work(std::move(work))
            , m_finish(std::move(finish))
        {
            setAutoDelete(false);
        }

        void run() override
        {
            m_queueState->markStarted(this);
            if (!m_taskCompletion.isActive()) {
                delete this;
                return;
            }

            Result result = m_work();
            Finish finish = std::move(m_finish);
            if (!m_taskCompletion.isActive()) {
                delete this;
                return;
            }

            const std::shared_ptr<AsyncWorkerDeliveryState> delivery = m_delivery;
            const bool queued = QMetaObject::invokeMethod(
                delivery->relay.get(),
                [delivery, taskCompletion = m_taskCompletion, finish = std::move(finish),
                    result = std::move(result)]() mutable {
                    if (delivery->guardedContext == nullptr) {
                        taskCompletion.cancel();
                        return;
                    }
                    taskCompletion.claimAndRun([&]() mutable { finish(std::move(result)); });
                },
                Qt::QueuedConnection);
            if (!queued) {
                m_taskCompletion.cancel();
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
