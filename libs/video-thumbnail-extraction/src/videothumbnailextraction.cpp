// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <VideoThumbnailExtraction/videothumbnailextraction.h>

#include <utility>

namespace kiriview {

class VideoThumbnailExtractionJob::State
{
public:
    State() = default;
    State(const State&) = delete;
    auto operator=(const State&) -> State& = delete;
    State(State&&) = delete;
    auto operator=(State&&) -> State& = delete;
    virtual ~State() = default;
    virtual void cancel() noexcept = 0;
    [[nodiscard]] virtual auto isActive() const noexcept -> bool = 0;
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
        state_.reset();
    }
}

auto VideoThumbnailExtractionJob::isActive() const noexcept -> bool
{
    return state_ && state_->isActive();
}

} // namespace kiriview
