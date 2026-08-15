// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "animationsourceruntime.h"

#include <limits>
#include <optional>
#include <utility>

namespace {
QString unavailableFrameError()
{
    return QStringLiteral("requested animation frame is unavailable");
}

QString closedSourceError() { return QStringLiteral("animation source is closed"); }

std::optional<qsizetype> checkedByteSum(qsizetype left, qsizetype right)
{
    if (left < 0 || right < 0 || left > std::numeric_limits<qsizetype>::max() - right) {
        return std::nullopt;
    }
    return left + right;
}
}

namespace kiriview {
AnimationSourceRuntime::AnimationSourceRuntime(QImage retainedFirstFrame, int authoredFrameCount,
    ImageAnimationPlaybackSourceFactory sourceFactory,
    ImageDecodeWorkspaceHold firstFrameWorkspaceHold,
    ImageAnimationPlaybackWorkspacePlan workspacePlan)
    : m_firstFrameWorkspaceHold(std::move(firstFrameWorkspaceHold))
    , m_firstFrame(std::move(retainedFirstFrame))
    , m_frameSize(m_firstFrame.size())
    , m_frameCount(authoredFrameCount)
    , m_sourceFactory(std::move(sourceFactory))
    , m_workspacePlan(workspacePlan)
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
        if (!enqueueSourceTask(SourceTask(std::move(task)))) {
            return failedFrame(closedSourceError());
        }
        return result.get();
    } catch (...) {
        return failedFrame(unavailableFrameError());
    }
}

AnimationSourceFrameResult AnimationSourceRuntime::frame(int authoredFrameIndex,
    ImageDecodeWorkspaceLease grant, qsizetype perOperationBaselineByteCount)
{
    if (!grant.isManaged() || perOperationBaselineByteCount < 0) {
        return failedFrame(imageDecodeWorkspaceResourceLimitDiagnostic(),
            AnimationSourceFrameFailureCause::ResourceLimitExceeded);
    }
    std::shared_ptr<ImageDecodeWorkspaceBudget> operationBudget
        = prechargedImageDecodeWorkspaceBudget(std::move(grant), perOperationBaselineByteCount);
    if (operationBudget == nullptr) {
        return failedFrame(imageDecodeWorkspaceResourceLimitDiagnostic(),
            AnimationSourceFrameFailureCause::ResourceLimitExceeded);
    }

    AnimationSourceFrameResult result = failedFrame(closedSourceError());
    try {
        FrameTask task([this, authoredFrameIndex, operationBudget]() {
            return decodeFrame(authoredFrameIndex, operationBudget);
        });
        std::future<AnimationSourceFrameResult> future = task.get_future();
        if (enqueueSourceTask(SourceTask(std::move(task)))) {
            result = future.get();
        }
    } catch (...) {
        result = failedFrame(unavailableFrameError());
    }
    operationBudget->finalizePrechargedAdmission();
    return result;
}

AnimationSourceFramePreparationResult AnimationSourceRuntime::prepareFrame(int authoredFrameIndex)
{
    if (m_closed.load(std::memory_order_acquire)) {
        return std::unexpected(AnimationSourceFrameFailure {
            AnimationSourceFrameFailureCause::Unavailable, closedSourceError() });
    }
    try {
        PreparationTask task(
            [this, authoredFrameIndex]() { return prepareFrameOnSourceOwner(authoredFrameIndex); });
        std::future<AnimationSourceFramePreparationResult> result = task.get_future();
        if (!enqueueSourceTask(SourceTask(std::move(task)))) {
            return std::unexpected(AnimationSourceFrameFailure {
                AnimationSourceFrameFailureCause::Unavailable, closedSourceError() });
        }
        return result.get();
    } catch (...) {
        return std::unexpected(AnimationSourceFrameFailure {
            AnimationSourceFrameFailureCause::Unavailable, unavailableFrameError() });
    }
}

bool AnimationSourceRuntime::retirePreparedPlaybackSource()
{
    if (m_closed.load(std::memory_order_acquire)) {
        return false;
    }
    try {
        RetirementTask task([this]() {
            m_source.reset();
            m_sourceFrame = 0;
            return true;
        });
        std::future<bool> result = task.get_future();
        return enqueueSourceTask(SourceTask(std::move(task))) && result.get();
    } catch (...) {
        return false;
    }
}

AnimationSourceFrameResult AnimationSourceRuntime::decodeFrame(int authoredFrameIndex)
{
    return decodeFrame(authoredFrameIndex, {});
}

AnimationSourceFrameResult AnimationSourceRuntime::decodeFrame(
    int authoredFrameIndex, const std::shared_ptr<ImageDecodeWorkspaceBudget>& operationBudget)
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
        return openSource(operationBudget);
    }
    if (m_source == nullptr || authoredFrameIndex <= m_sourceFrame) {
        AnimationSourceFrameResult opened = openSource(operationBudget);
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
        ImageAnimationPlaybackReadResult read = operationBudget == nullptr
            ? m_source->readNextFrame()
            : m_source->readNextFrameAdmitted(operationBudget);
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

AnimationSourceFramePreparationResult AnimationSourceRuntime::prepareFrameOnSourceOwner(
    int authoredFrameIndex) const
{
    if (m_closed.load(std::memory_order_acquire)) {
        return std::unexpected(AnimationSourceFrameFailure {
            AnimationSourceFrameFailureCause::Unavailable, closedSourceError() });
    }
    if (m_frameSize.isEmpty() || authoredFrameIndex < 0 || authoredFrameIndex >= m_frameCount) {
        return std::unexpected(AnimationSourceFrameFailure {
            AnimationSourceFrameFailureCause::Unavailable, unavailableFrameError() });
    }
    if (authoredFrameIndex == 0 && !m_firstFrame.isNull()) {
        return AnimationSourceFramePreparation {};
    }
    if (!m_workspacePlan.isValid()) {
        return std::unexpected(AnimationSourceFrameFailure {
            AnimationSourceFrameFailureCause::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic(),
        });
    }

    const bool needsOpen = m_source == nullptr || authoredFrameIndex <= m_sourceFrame;
    if (needsOpen) {
        const std::optional<qsizetype> openPeakByteCount = m_workspacePlan.openPeakByteCount();
        if (!openPeakByteCount.has_value()) {
            return std::unexpected(AnimationSourceFrameFailure {
                AnimationSourceFrameFailureCause::ResourceLimitExceeded,
                imageDecodeWorkspaceResourceLimitDiagnostic(),
            });
        }
        return AnimationSourceFramePreparation {
            ImageDecodeWorkspaceAdmissionRequest {
                *openPeakByteCount,
                m_workspacePlan.retainedInputByteCount,
                ImageDecodeWorkspacePriority::Interactive,
            },
            m_workspacePlan.frameOutputByteCount,
            m_source != nullptr,
        };
    }

    const std::optional<qsizetype> baselineByteCount = checkedByteSum(
        m_workspacePlan.retainedInputByteCount, m_workspacePlan.persistentDecoderByteCount);
    if (!baselineByteCount.has_value()) {
        return std::unexpected(AnimationSourceFrameFailure {
            AnimationSourceFrameFailureCause::ResourceLimitExceeded,
            imageDecodeWorkspaceResourceLimitDiagnostic(),
        });
    }
    return AnimationSourceFramePreparation {
        ImageDecodeWorkspaceAdmissionRequest {
            m_workspacePlan.frameOutputByteCount,
            *baselineByteCount,
            ImageDecodeWorkspacePriority::Interactive,
        },
        m_workspacePlan.frameOutputByteCount,
        false,
    };
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

AnimationSourceFrameResult AnimationSourceRuntime::openSource(
    const std::shared_ptr<ImageDecodeWorkspaceBudget>& operationBudget)
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

    ImageAnimationPlaybackOpenResult opened
        = operationBudget == nullptr ? m_source->open() : m_source->openAdmitted(operationBudget);
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

bool AnimationSourceRuntime::enqueueSourceTask(SourceTask task)
{
    {
        const std::scoped_lock lock(m_queueMutex);
        if (m_closed.load(std::memory_order_relaxed)) {
            return false;
        }
        if (!m_sourceOwner.joinable()) {
            m_sourceOwner = std::jthread([this]() { runSourceOwner(); });
        }
        m_sourceTasks.push_back(std::move(task));
    }
    m_queueCondition.notify_one();
    return true;
}

void AnimationSourceRuntime::runSourceOwner()
{
    while (true) {
        SourceTask task;
        {
            std::unique_lock lock(m_queueMutex);
            m_queueCondition.wait(lock, [this]() {
                return m_closed.load(std::memory_order_acquire) || !m_sourceTasks.empty();
            });
            if (m_sourceTasks.empty()) {
                break;
            }
            task = std::move(m_sourceTasks.front());
            m_sourceTasks.pop_front();
        }
        std::visit([](auto& sourceTask) { sourceTask(); }, task);
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
