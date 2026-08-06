// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "animationsourceruntime.h"

#include <optional>
#include <utility>

namespace {
QString unavailableFrameError()
{
    return QStringLiteral("requested animation frame is unavailable");
}

QString closedSourceError() { return QStringLiteral("animation source is closed"); }
}

namespace kiriview {
AnimationSourceRuntime::AnimationSourceRuntime(QImage retainedFirstFrame, int authoredFrameCount,
    ImageAnimationPlaybackSourceFactory sourceFactory,
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold)
    : m_firstFrameWorkspaceHold(std::move(firstFrameWorkspaceHold))
    , m_firstFrame(std::move(retainedFirstFrame))
    , m_frameSize(m_firstFrame.size())
    , m_frameCount(authoredFrameCount)
    , m_sourceFactory(std::move(sourceFactory))
{
}

AnimationSourceRuntime::~AnimationSourceRuntime() { close(); }

AnimationSourceFrameResult AnimationSourceRuntime::frame(int authoredFrameIndex)
{
    if (m_closed.load(std::memory_order_acquire)) {
        return failedFrame(closedSourceError());
    }
    if (m_frameSize.isEmpty() || authoredFrameIndex < 0 || authoredFrameIndex >= m_frameCount) {
        return failedFrame(unavailableFrameError());
    }
    try {
        FrameTask task([this, authoredFrameIndex]() { return decodeFrame(authoredFrameIndex); });
        std::future<AnimationSourceFrameResult> result = task.get_future();
        {
            const std::scoped_lock lock(m_queueMutex);
            if (m_closed.load(std::memory_order_relaxed)) {
                return failedFrame(closedSourceError());
            }
            if (!m_sourceOwner.joinable()) {
                m_sourceOwner = std::jthread([this]() { runSourceOwner(); });
            }
            m_frameTasks.push_back(std::move(task));
        }
        m_queueCondition.notify_one();
        return result.get();
    } catch (...) {
        return failedFrame(unavailableFrameError());
    }
}

AnimationSourceFrameResult AnimationSourceRuntime::decodeFrame(int authoredFrameIndex)
{
    if (m_closed.load(std::memory_order_acquire)) {
        return failedFrame(closedSourceError());
    }
    if (authoredFrameIndex == 0 && !m_firstFrame.isNull()) {
        return AnimationSourceFrame {
            m_firstFrameWorkspaceHold,
            std::exchange(m_firstFrame, {}),
        };
    }
    m_firstFrame = {};
    releaseRetainedFirstFrameWorkspace();
    if (authoredFrameIndex == 0) {
        return openSource();
    }
    if (m_source == nullptr || authoredFrameIndex <= m_sourceFrame) {
        AnimationSourceFrameResult opened = openSource();
        if (!opened.has_value()) {
            return opened;
        }
    }

    std::optional<AnimationSourceFrame> decodedFrame;
    while (m_sourceFrame < authoredFrameIndex) {
        if (m_closed.load(std::memory_order_relaxed)) {
            return failedFrame(closedSourceError());
        }
        decodedFrame.reset();
        ImageAnimationPlaybackReadResult read = m_source->readNextFrame();
        if (read.status == ImageAnimationPlaybackReadStatus::Error
            || read.status == ImageAnimationPlaybackReadStatus::ResourceLimitExceeded) {
            m_source.reset();
            return failedFrame(
                read.errorString.isEmpty() ? unavailableFrameError() : std::move(read.errorString),
                read.status == ImageAnimationPlaybackReadStatus::ResourceLimitExceeded
                    ? AnimationSourceFrameFailureCause::ResourceLimitExceeded
                    : AnimationSourceFrameFailureCause::Unavailable);
        }
        if (read.status != ImageAnimationPlaybackReadStatus::Frame || read.frame.image.isNull()
            || read.frame.image.size() != m_frameSize) {
            m_source.reset();
            return failedFrame(unavailableFrameError());
        }
        ++m_sourceFrame;
        decodedFrame.emplace(AnimationSourceFrame {
            std::move(read.frame.workspaceHold),
            std::move(read.frame.image),
        });
    }
    return std::move(*decodedFrame);
}

void AnimationSourceRuntime::releaseRetainedFirstFrameWorkspace()
{
    const std::scoped_lock lock(m_firstFrameWorkspaceMutex);
    m_firstFrameWorkspaceHold = {};
}

void AnimationSourceRuntime::close()
{
    {
        const std::scoped_lock lock(m_queueMutex);
        m_closed.store(true, std::memory_order_release);
    }
    m_queueCondition.notify_all();
}

AnimationSourceFrameResult AnimationSourceRuntime::failedFrame(
    QString errorString, AnimationSourceFrameFailureCause cause) const
{
    return std::unexpected(AnimationSourceFrameFailure { cause, std::move(errorString) });
}

AnimationSourceFrameResult AnimationSourceRuntime::openSource()
{
    m_source.reset();
    m_sourceFrame = 0;
    if (!m_sourceFactory) {
        return failedFrame(unavailableFrameError());
    }
    m_source = m_sourceFactory();
    if (m_source == nullptr) {
        return failedFrame(unavailableFrameError());
    }

    ImageAnimationPlaybackOpenResult opened = m_source->open();
    if (opened.status != ImageAnimationPlaybackOpenStatus::Success || opened.firstFrame.isNull()
        || opened.firstFrame.size() != m_frameSize) {
        m_source.reset();
        return failedFrame(
            opened.errorString.isEmpty() ? unavailableFrameError() : std::move(opened.errorString),
            opened.status == ImageAnimationPlaybackOpenStatus::ResourceLimitExceeded
                ? AnimationSourceFrameFailureCause::ResourceLimitExceeded
                : AnimationSourceFrameFailureCause::Unavailable);
    }
    return AnimationSourceFrame {
        std::move(opened.workspaceHold),
        std::move(opened.firstFrame),
    };
}

void AnimationSourceRuntime::runSourceOwner()
{
    while (true) {
        FrameTask task;
        {
            std::unique_lock lock(m_queueMutex);
            m_queueCondition.wait(lock, [this]() {
                return m_closed.load(std::memory_order_acquire) || !m_frameTasks.empty();
            });
            if (m_frameTasks.empty()) {
                break;
            }
            task = std::move(m_frameTasks.front());
            m_frameTasks.pop_front();
        }
        task();
    }
    m_source.reset();
    m_sourceFrame = 0;
}

ImageAnimationPlaybackSourceFactory imageAnimationPlaybackSourceFactory(
    ImageAnimationPlaybackRequest request)
{
    if (!request.isValid()) {
        return {};
    }
    return [request = std::move(request)]() mutable {
        return makeImageAnimationPlaybackSource(request);
    };
}
}
