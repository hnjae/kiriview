// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_TESTS_IMAGE_ASYNC_TEST_SUPPORT_H
#define KIRIVIEW_TESTS_IMAGE_ASYNC_TEST_SUPPORT_H

#include "async/imageiojob.h"
#include "async/imageworkerscheduler.h"
#include "async/timerscheduler.h"
#include "decoding/imagedecodedependencies.h"
#include "decoding/imagesourcedata.h"
#include "system/filedeletion.h"
#include "system/kiooperationfailure.h"
#include "system/powersaverprovider.h"

#include <KIO/Global>
#include <QByteArray>
#include <QCoreApplication>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace kiriview::TestSupport {
inline ImageDataLoadError backendImageDataLoadFailure(const QUrl& targetUrl, QString detail)
{
    return ImageDataLoadError { kioOperationFailureFromKJob(KioOperationKind::ImageDataRead,
        targetUrl, KIO::ERR_CONNECTION_BROKEN, std::move(detail)) };
}

namespace Detail {
    template <typename Operation>
    ImageIoJob startManualIoJob(QObject* receiver, const std::shared_ptr<Operation>& operation,
        std::function<void()> cancelHook)
    {
        operation->object = new QObject(receiver);

        std::weak_ptr<Operation> weakOperation = operation;
        ImageIoJob job(operation->object,
            [weakOperation, cancelHook = std::move(cancelHook)](QObject* object) {
                const QPointer<QObject> guardedObject(object);
                if (std::shared_ptr<Operation> operation = weakOperation.lock()) {
                    operation->canceled = true;
                    operation->object = nullptr;
                }
                if (cancelHook) {
                    cancelHook();
                }
                if (guardedObject != nullptr) {
                    guardedObject->deleteLater();
                }
            });
        operation->completion = job.completion();
        return job;
    }

    template <typename Operation>
    ImageIoJob startManualIoJob(QObject* receiver, const std::shared_ptr<Operation>& operation)
    {
        return startManualIoJob(receiver, operation, {});
    }

    template <typename Operation, typename Delivery>
    void finishManualIoJob(const std::shared_ptr<Operation>& operation, Delivery&& delivery)
    {
        if (operation == nullptr) {
            return;
        }

        const QPointer<QObject> guardedObject(operation->object);
        operation->completion.claimAndRun([&]() mutable {
            operation->object = nullptr;
            std::forward<Delivery>(delivery)(*operation);
            if (guardedObject != nullptr) {
                guardedObject->deleteLater();
            }
        });
    }
}

class ManualRuntimeTimer final : public RuntimeTimerHandle
{
public:
    ManualRuntimeTimer(TimerDuration interval, RuntimeTimerCallback callback)
        : m_interval(interval)
        , m_callback(std::move(callback))
    {
    }

    int intervalMsec() const { return static_cast<int>(m_interval.count()); }
    bool active() const { return m_active; }

    void start(TimerDuration interval) override
    {
        m_interval = interval;
        m_active = true;
    }
    void stop() override { m_active = false; }

    void fire()
    {
        if (!m_active || !m_callback) {
            return;
        }

        m_active = false;
        m_callback();
    }

private:
    TimerDuration m_interval {};
    RuntimeTimerCallback m_callback;
    bool m_active = false;
};

class ManualTimerScheduler
{
public:
    TimerScheduler scheduler()
    {
        return TimerScheduler {
            [this]() { return TimerDuration(m_currentMsec); },
            [this](QObject*, TimerDuration interval,
                RuntimeTimerCallback callback) -> std::unique_ptr<RuntimeTimerHandle> {
                auto timer = std::make_unique<ManualRuntimeTimer>(interval, std::move(callback));
                m_timers.push_back(timer.get());
                return timer;
            },
        };
    }

    void advanceTo(qint64 monotonicMsec) { m_currentMsec = monotonicMsec; }
    std::size_t timerCount() const { return m_timers.size(); }
    ManualRuntimeTimer& timerAt(std::size_t index) { return *m_timers.at(index); }

private:
    qint64 m_currentMsec = 0;
    std::vector<ManualRuntimeTimer*> m_timers;
};

struct ManualImageWorkerSchedule
{
    ImageWorkerOperation work;
    ImageWorkerCompletion completion;
    ImageWorkerTaskCompletion taskCompletion;
};

class ManualImageWorkerScheduler
{
public:
    ImageWorkerScheduler scheduler()
    {
        return ImageWorkerScheduler(
            [this](QObject*, ImageWorkerOperation work, ImageWorkerCompletion completion) {
                auto schedule = std::make_shared<ManualImageWorkerSchedule>();
                schedule->work = std::move(work);
                schedule->completion = std::move(completion);
                ImageWorkerTask task(
                    [weakSchedule = std::weak_ptr<ManualImageWorkerSchedule>(schedule)]() {
                        if (const auto activeSchedule = weakSchedule.lock()) {
                            activeSchedule->work = {};
                            activeSchedule->completion = {};
                            activeSchedule->taskCompletion.retire();
                        }
                    });
                schedule->taskCompletion = task.completion();
                m_schedules.push_back(std::move(schedule));
                return task;
            });
    }

    std::size_t scheduleCount() const { return m_schedules.size(); }
    bool isActive(std::size_t index) const
    {
        return m_schedules.at(index)->taskCompletion.isActive();
    }

    void runWork(std::size_t index)
    {
        if (m_schedules.at(index)->work) {
            m_schedules.at(index)->work();
        }
    }

    void finish(std::size_t index)
    {
        const auto schedule = m_schedules.at(index);
        schedule->taskCompletion.claimAndRun([&]() {
            if (schedule->completion) {
                schedule->completion();
            }
        });
        schedule->taskCompletion.retire();
    }

private:
    std::vector<std::shared_ptr<ManualImageWorkerSchedule>> m_schedules;
};

inline void runOutstandingImageWorkerSchedules(
    ManualImageWorkerScheduler& workerScheduler, std::size_t& nextSchedule)
{
    while (nextSchedule < workerScheduler.scheduleCount()) {
        if (workerScheduler.isActive(nextSchedule)) {
            workerScheduler.runWork(nextSchedule);
            workerScheduler.finish(nextSchedule);
        }
        ++nextSchedule;
        QCoreApplication::processEvents();
    }
}

struct ManualImageDataLoad
{
    QObject* object = nullptr;
    QUrl url;
    OpenedCollectionScopeLocation openedCollectionScope;
    ImageFirstDisplayDecodeContext firstDisplay;
    ImageDataCallback dataCallback;
    ImageDataLoadErrorCallback errorCallback;
    ImageIoJobCompletion completion;
    bool canceled = false;
};

class ManualImageDataLoader
{
public:
    ImageIoJob start(QObject* receiver, ImageDecodeRequest request, ImageDataCallback callback,
        ImageDataLoadErrorCallback errorCallback)
    {
        auto load = std::make_shared<ManualImageDataLoad>();
        load->url = request.imageUrl();
        load->openedCollectionScope = request.openedCollectionScope();
        load->firstDisplay = request.firstDisplay();
        load->dataCallback = std::move(callback);
        load->errorCallback = std::move(errorCallback);

        ImageIoJob job = Detail::startManualIoJob(receiver, load);
        m_loads.push_back(load);
        return job;
    }

    std::size_t loadCount() const { return m_loads.size(); }

    std::size_t loadCountForUrl(const QUrl& url) const
    {
        return static_cast<std::size_t>(std::ranges::count_if(
            m_loads, [&url](const auto& load) { return load != nullptr && load->url == url; }));
    }

    bool empty() const { return m_loads.empty(); }

    ManualImageDataLoad& frontLoad() { return *m_loads.front(); }

    const ManualImageDataLoad& frontLoad() const { return *m_loads.front(); }

    ManualImageDataLoad& backLoad() { return *m_loads.back(); }

    const ManualImageDataLoad& backLoad() const { return *m_loads.back(); }

    bool hasActiveLoadForUrl(const QUrl& url) const
    {
        return std::any_of(m_loads.cbegin(), m_loads.cend(),
            [&url](const std::shared_ptr<ManualImageDataLoad>& load) {
                return load != nullptr && load->object != nullptr && load->url == url;
            });
    }

    void finishFrontLoad(QByteArray data) { finishDataLoad(m_loads.front(), std::move(data)); }

    void finishFrontLoad(ImageSourceData data) { finishDataLoad(m_loads.front(), std::move(data)); }

    void finishBackLoad(QByteArray data) { finishDataLoad(m_loads.back(), std::move(data)); }

    bool finishOldestActiveLoadForUrl(const QUrl& url, QByteArray data)
    {
        for (const std::shared_ptr<ManualImageDataLoad>& load : m_loads) {
            if (load != nullptr && load->object != nullptr && load->url == url) {
                finishDataLoad(load, std::move(data));
                return true;
            }
        }

        return false;
    }

    bool finishNewestActiveLoadForUrl(const QUrl& url, QByteArray data)
    {
        for (auto load = m_loads.rbegin(); load != m_loads.rend(); ++load) {
            if (*load != nullptr && (*load)->object != nullptr && (*load)->url == url) {
                finishDataLoad(*load, std::move(data));
                return true;
            }
        }

        return false;
    }

    void failFrontLoad(ImageDataLoadError error)
    {
        finishLoadError(m_loads.front(), std::move(error));
    }

    void failBackLoad(ImageDataLoadError error)
    {
        finishLoadError(m_loads.back(), std::move(error));
    }

    void deliverFrontLoadDataIgnoringCancellation(QByteArray data)
    {
        deliverData(*m_loads.front(), std::move(data));
    }

private:
    template <typename Delivery>
    static void finishLoad(const std::shared_ptr<ManualImageDataLoad>& load, Delivery&& delivery)
    {
        Detail::finishManualIoJob(load, std::forward<Delivery>(delivery));
    }

    static void finishDataLoad(const std::shared_ptr<ManualImageDataLoad>& load, QByteArray data)
    {
        finishLoad(load, [data = std::move(data)](ManualImageDataLoad& load) mutable {
            deliverData(load, std::move(data));
        });
    }

    static void finishDataLoad(
        const std::shared_ptr<ManualImageDataLoad>& load, ImageSourceData data)
    {
        finishLoad(load, [data = std::move(data)](ManualImageDataLoad& load) mutable {
            if (load.dataCallback) {
                load.dataCallback(std::move(data));
            }
        });
    }

    static void finishLoadError(
        const std::shared_ptr<ManualImageDataLoad>& load, ImageDataLoadError error)
    {
        finishLoad(load, [error = std::move(error)](ManualImageDataLoad& load) mutable {
            if (load.errorCallback) {
                load.errorCallback(std::move(error));
            }
        });
    }

    static void deliverData(ManualImageDataLoad& load, QByteArray data)
    {
        if (load.dataCallback) {
            load.dataCallback(std::move(data));
        }
    }

    std::vector<std::shared_ptr<ManualImageDataLoad>> m_loads;
};

class ManualImageDataLoaderAdapter
{
public:
    explicit ManualImageDataLoaderAdapter(ManualImageDataLoader& dataLoader)
        : m_dataLoader(&dataLoader)
    {
    }

    ImageIoJob operator()(QObject* receiver, ImageDecodeRequest request, ImageDataCallback callback,
        ImageDataLoadErrorCallback errorCallback) const
    {
        return m_dataLoader->start(
            receiver, std::move(request), std::move(callback), std::move(errorCallback));
    }

private:
    ManualImageDataLoader* m_dataLoader = nullptr;
};

inline ImageDataLoader dataLoaderFor(ManualImageDataLoader& dataLoader)
{
    return ManualImageDataLoaderAdapter(dataLoader);
}

class ManualPowerSaverMonitor final : public PowerSaverStateMonitor
{
public:
    ManualPowerSaverMonitor(bool enabled, PowerSaverChangedCallback callback)
        : m_enabled(enabled)
        , m_callback(std::move(callback))
    {
    }

    bool powerSaverEnabled() const override { return m_enabled; }

    void setPowerSaverEnabled(bool enabled)
    {
        if (m_enabled == enabled) {
            return;
        }

        m_enabled = enabled;
        if (m_callback) {
            m_callback(enabled);
        }
    }

private:
    bool m_enabled = false;
    PowerSaverChangedCallback m_callback;
};

inline PowerSaverProvider powerSaverProviderFor(
    ManualPowerSaverMonitor*& monitor, bool initialEnabled)
{
    return PowerSaverProvider {
        [&monitor, initialEnabled](PowerSaverChangedCallback callback) {
            auto instance
                = std::make_unique<ManualPowerSaverMonitor>(initialEnabled, std::move(callback));
            monitor = instance.get();
            return instance;
        },
    };
}

struct ManualFileDeletionOperation
{
    QObject* object = nullptr;
    FileDeletionRequest request;
    FileDeletionCallback callback;
    ImageIoJobCompletion completion;
    bool canceled = false;
};

inline KioOperationFailure manualFileDeletionFailure(
    const FileDeletionRequest& request, FileDeletionResult result, const QString& errorString)
{
    KioOperationFailure failure;
    failure.operationKind = KioOperationKind::FileDeletion;
    failure.targetUrl = request.targetUrl;
    failure.canceled = result == FileDeletionResult::Canceled;
    failure.userMessage = result == FileDeletionResult::Failed ? errorString : QString();
    failure.diagnosticDetail = errorString;
    failure.retryable = result == FileDeletionResult::Failed;
    return failure;
}

class ManualFileDeletionProvider
{
public:
    void setCancelHook(std::function<void()> cancelHook) { m_cancelHook = std::move(cancelHook); }

    ImageIoJob start(QObject* receiver, FileDeletionRequest request, FileDeletionCallback callback)
    {
        auto operation = std::make_shared<ManualFileDeletionOperation>();
        operation->request = std::move(request);
        operation->callback = std::move(callback);

        ImageIoJob job = Detail::startManualIoJob(receiver, operation, m_cancelHook);
        m_operations.push_back(operation);
        return job;
    }

    std::size_t operationCount() const { return m_operations.size(); }

    ManualFileDeletionOperation& backOperation() { return *m_operations.back(); }

    const ManualFileDeletionOperation& backOperation() const { return *m_operations.back(); }

    ManualFileDeletionOperation& operationAt(std::size_t index) { return *m_operations.at(index); }

    const ManualFileDeletionOperation& operationAt(std::size_t index) const
    {
        return *m_operations.at(index);
    }

    void finishBackOperation(FileDeletionResult result, const QString& errorString = QString())
    {
        finishBackOperation(
            result, manualFileDeletionFailure(m_operations.back()->request, result, errorString));
    }

    void finishBackOperation(FileDeletionResult result, KioOperationFailure failure)
    {
        finishOperation(m_operations.back(), result, std::move(failure));
    }

    void deliverOperationAtIgnoringCancellation(
        std::size_t index, FileDeletionResult result, const QString& errorString = QString())
    {
        deliverOperationAtIgnoringCancellation(index, result,
            manualFileDeletionFailure(m_operations.at(index)->request, result, errorString));
    }

    void deliverOperationAtIgnoringCancellation(
        std::size_t index, FileDeletionResult result, const KioOperationFailure& failure)
    {
        if (m_operations.at(index)->callback) {
            m_operations.at(index)->callback(result, failure);
        }
    }

private:
    static void finishOperation(const std::shared_ptr<ManualFileDeletionOperation>& operation,
        FileDeletionResult result, KioOperationFailure failure)
    {
        Detail::finishManualIoJob(operation,
            [result, failure = std::move(failure)](ManualFileDeletionOperation& operation) {
                if (operation.callback) {
                    operation.callback(result, failure);
                }
            });
    }

    std::vector<std::shared_ptr<ManualFileDeletionOperation>> m_operations;
    std::function<void()> m_cancelHook;
};

class ManualFileDeletionProviderAdapter
{
public:
    explicit ManualFileDeletionProviderAdapter(ManualFileDeletionProvider& provider)
        : m_provider(&provider)
    {
    }

    ImageIoJob operator()(
        QObject* receiver, FileDeletionRequest request, FileDeletionCallback callback) const
    {
        return m_provider->start(receiver, std::move(request), std::move(callback));
    }

private:
    ManualFileDeletionProvider* m_provider = nullptr;
};

inline FileDeletionProvider fileDeletionProviderFor(ManualFileDeletionProvider& provider)
{
    return ManualFileDeletionProviderAdapter(provider);
}
}

#endif
