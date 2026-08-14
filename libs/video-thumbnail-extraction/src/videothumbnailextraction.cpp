// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <VideoThumbnailExtraction/videothumbnailextraction.h>

#include "videothumbnailextraction_p.h"

#include <QAbstractEventDispatcher>
#include <QObject>
#include <QThread>

#include <memory>
#include <utility>

namespace {

thread_local kiriview::detail::VideoThumbnailRuntimeDependencies* dependencyOverride = nullptr;

class DependencyOverrideGuard final
{
public:
    explicit DependencyOverrideGuard(
        kiriview::detail::VideoThumbnailRuntimeDependencies& dependencies)
        : previous_(std::exchange(dependencyOverride, &dependencies))
    {
        Q_ASSERT(previous_ == nullptr);
    }

    ~DependencyOverrideGuard() { dependencyOverride = previous_; }

    DependencyOverrideGuard(const DependencyOverrideGuard&) = delete;
    auto operator=(const DependencyOverrideGuard&) -> DependencyOverrideGuard& = delete;
    DependencyOverrideGuard(DependencyOverrideGuard&&) = delete;
    auto operator=(DependencyOverrideGuard&&) -> DependencyOverrideGuard& = delete;

private:
    kiriview::detail::VideoThumbnailRuntimeDependencies* previous_ = nullptr;
};

} // namespace

namespace kiriview {

class VideoThumbnailExtractionJob::State
{
public:
    explicit State(std::shared_ptr<detail::VideoThumbnailExtractionJobControl> control)
        : control_(std::move(control))
    {
    }

    void cancel() noexcept
    {
        if (!control_ || !control_->active) {
            return;
        }
        auto cancel = control_->cancel;
        if (cancel) {
            cancel();
        } else {
            control_->active = false;
        }
    }

    void setRetirementCallback(std::function<void()> callback)
    {
        if (control_->retired) {
            if (callback) {
                callback();
            }
            return;
        }
        control_->retirementCallback = std::move(callback);
    }

    [[nodiscard]] auto isActive() const noexcept -> bool { return control_ && control_->active; }

private:
    std::shared_ptr<detail::VideoThumbnailExtractionJobControl> control_;
};

VideoThumbnailExtractionJob::VideoThumbnailExtractionJob(std::shared_ptr<State> state) noexcept
    : state_(std::move(state))
{
}

VideoThumbnailExtractionJob::VideoThumbnailExtractionJob(
    VideoThumbnailExtractionJob&& other) noexcept
    = default;

auto VideoThumbnailExtractionJob::operator=(VideoThumbnailExtractionJob&& other) noexcept
    -> VideoThumbnailExtractionJob&
{
    if (this != &other) {
        cancel();
        state_ = std::move(other.state_);
    }
    return *this;
}

VideoThumbnailExtractionJob::~VideoThumbnailExtractionJob() { cancel(); }

void VideoThumbnailExtractionJob::cancel() noexcept
{
    if (state_) {
        state_->cancel();
    }
}

void VideoThumbnailExtractionJob::setRetirementCallback(std::function<void()> callback)
{
    if (state_) {
        state_->setRetirementCallback(std::move(callback));
    } else if (callback) {
        callback();
    }
}

auto VideoThumbnailExtractionJob::isActive() const noexcept -> bool
{
    return state_ && state_->isActive();
}

auto startVideoThumbnailExtraction(QObject* receiver, VideoThumbnailExtractionRequest request,
    VideoThumbnailExtractionCallback completion) -> VideoThumbnailExtractionJob
{
    Q_ASSERT(receiver != nullptr);
    Q_ASSERT(completion);
    Q_ASSERT(QThread::currentThread() == receiver->thread());
    Q_ASSERT(receiver->thread()->eventDispatcher() != nullptr);

    detail::VideoThumbnailRuntimeDependencies dependencies;
    if (dependencyOverride != nullptr) {
        dependencies = std::move(*dependencyOverride);
    } else {
        dependencies.backendFactory = detail::createQtVideoThumbnailBackend;
        dependencies.deadlineFactory = detail::createQtVideoThumbnailDeadline;
    }

    auto control = std::make_shared<detail::VideoThumbnailExtractionJobControl>();
    auto state = std::make_shared<VideoThumbnailExtractionJob::State>(control);
    detail::startVideoThumbnailExtractionOperation(
        receiver, std::move(request), std::move(completion), std::move(dependencies), control);
    return VideoThumbnailExtractionJob(std::move(state));
}

} // namespace kiriview

namespace kiriview::detail {

auto startVideoThumbnailExtractionWithDependencies(QObject* receiver,
    VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback completion,
    VideoThumbnailRuntimeDependencies dependencies) -> VideoThumbnailExtractionJob
{
    const DependencyOverrideGuard guard(dependencies);
    return kiriview::startVideoThumbnailExtraction(
        receiver, std::move(request), std::move(completion));
}

} // namespace kiriview::detail
