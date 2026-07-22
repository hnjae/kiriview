// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnail/videothumbnailextractor.h"

#include <QObject>
#include <deque>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
template <typename> inline constexpr bool alwaysFalse = false;

kiriview::VideoThumbnailExtractionDependencies resolvedDependencies(
    kiriview::VideoThumbnailExtractionDependencies dependencies)
{
    if (!dependencies.backendFactory) {
        dependencies.backendFactory = kiriview::createDefaultVideoThumbnailBackend;
    }
    dependencies.timerScheduler
        = kiriview::timerSchedulerWithDefaults(std::move(dependencies.timerScheduler));
    return dependencies;
}

class VideoThumbnailExtractionJob final : public QObject
{
public:
    VideoThumbnailExtractionJob(kiriview::VideoThumbnailExtractionRequest request,
        kiriview::VideoThumbnailExtractionCallback callback,
        kiriview::VideoThumbnailExtractionDependencies dependencies, QObject* parent)
        : QObject(parent)
        , m_request(std::move(request))
        , m_callback(std::move(callback))
        , m_dependencies(resolvedDependencies(std::move(dependencies)))
    {
        m_backend = m_dependencies.backendFactory();
        m_timeout = m_dependencies.timerScheduler.singleShotTimer(
            this, 10000, [this]() { enqueue(m_workflow.handleTimeout()); });
        if (m_backend != nullptr) {
            m_backend->setCallbacks(kiriview::VideoThumbnailBackendCallbacks {
                [this](kiriview::VideoThumbnailBackendMediaFacts facts) {
                    enqueue(m_workflow.handleMediaFacts(facts));
                },
                [this](qint64 position) { enqueue(m_workflow.handlePositionChanged(position)); },
                [this](QImage image) { enqueue(m_workflow.handleFrame(std::move(image))); },
                [this](kiriview::VideoThumbnailEmbeddedImages images) {
                    enqueue(m_workflow.handleMetadata(std::move(images)));
                },
                [this](QString errorString) {
                    enqueue(m_workflow.handleBackendError(std::move(errorString)));
                },
            });
        }
    }

    void setCompletion(kiriview::ImageIoJobCompletion completion)
    {
        m_completion = std::move(completion);
    }

    void start()
    {
        if (m_backend == nullptr || m_timeout == nullptr) {
            enqueue(m_workflow.start({}));
            return;
        }
        enqueue(m_workflow.start(std::move(m_request)));
    }

    void cancel() { enqueue(m_workflow.cancel()); }

private:
    void enqueue(kiriview::VideoThumbnailExtractionPlan plan)
    {
        for (kiriview::VideoThumbnailExtractionOperation& operation : plan) {
            m_pendingOperations.push_back(std::move(operation));
        }
        if (m_dispatching) {
            return;
        }
        m_dispatching = true;
        while (!m_pendingOperations.empty()) {
            kiriview::VideoThumbnailExtractionOperation operation
                = std::move(m_pendingOperations.front());
            m_pendingOperations.pop_front();
            dispatch(std::move(operation));
        }
        m_dispatching = false;
    }

    void dispatch(kiriview::VideoThumbnailExtractionOperation operation)
    {
        std::visit(
            [this](auto payload) {
                using Operation = decltype(payload);
                if constexpr (std::is_same_v<Operation, kiriview::StartVideoThumbnailTimeout>) {
                    if (m_timeout != nullptr) {
                        m_timeout->start(payload.intervalMsec);
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         kiriview::StopVideoThumbnailTimeout>) {
                    if (m_timeout != nullptr) {
                        m_timeout->stop();
                    }
                } else if constexpr (std::is_same_v<Operation, kiriview::SetVideoThumbnailSource>) {
                    if (m_backend != nullptr) {
                        m_backend->setSource(payload.sourceUrl);
                    }
                } else if constexpr (std::is_same_v<Operation, kiriview::SeekVideoThumbnail>) {
                    if (m_backend != nullptr) {
                        m_backend->setPosition(payload.positionMsec);
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         kiriview::PlayVideoThumbnailBackend>) {
                    if (m_backend != nullptr) {
                        m_backend->play();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         kiriview::PauseVideoThumbnailBackend>) {
                    if (m_backend != nullptr) {
                        m_backend->pause();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         kiriview::StopVideoThumbnailBackend>) {
                    if (m_backend != nullptr) {
                        m_backend->stop();
                    }
                } else if constexpr (std::is_same_v<Operation,
                                         kiriview::CompleteVideoThumbnailExtraction>) {
                    complete(std::move(payload.result));
                } else {
                    static_assert(alwaysFalse<Operation>, "Unhandled video thumbnail operation");
                }
            },
            std::move(operation));
    }

    void complete(kiriview::VideoThumbnailExtractionResult result)
    {
        kiriview::ImageIoJobCompletion completion = m_completion;
        kiriview::VideoThumbnailExtractionCallback callback = m_callback;
        m_callback = {};
        completion.claimAndDelete(
            [callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) {
                    callback(std::move(result));
                }
            });
    }

    kiriview::VideoThumbnailExtractionRequest m_request;
    kiriview::VideoThumbnailExtractionCallback m_callback;
    kiriview::VideoThumbnailExtractionDependencies m_dependencies;
    kiriview::ImageIoJobCompletion m_completion;
    kiriview::VideoThumbnailExtractionWorkflow m_workflow;
    std::unique_ptr<kiriview::VideoThumbnailBackend> m_backend;
    std::unique_ptr<kiriview::RuntimeTimerHandle> m_timeout;
    std::deque<kiriview::VideoThumbnailExtractionOperation> m_pendingOperations;
    bool m_dispatching = false;
};
}

namespace kiriview {
ImageIoJob startVideoThumbnailExtraction(QObject* receiver, VideoThumbnailExtractionRequest request,
    VideoThumbnailExtractionCallback callback, VideoThumbnailExtractionDependencies dependencies)
{
    auto* jobObject = new VideoThumbnailExtractionJob(
        std::move(request), std::move(callback), std::move(dependencies), receiver);
    ImageIoJob job(jobObject, [](QObject* object) {
        if (auto* extractionJob = dynamic_cast<VideoThumbnailExtractionJob*>(object)) {
            extractionJob->cancel();
            extractionJob->deleteLater();
        } else if (object != nullptr) {
            object->deleteLater();
        }
    });
    jobObject->setCompletion(job.completion());
    jobObject->start();
    return job;
}

VideoThumbnailExtractionProvider videoThumbnailExtractionProvider(
    VideoThumbnailExtractionDependencies dependencies)
{
    return [dependencies = std::move(dependencies)](QObject* receiver,
               VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback callback) {
        return startVideoThumbnailExtraction(
            receiver, std::move(request), std::move(callback), dependencies);
    };
}
}
