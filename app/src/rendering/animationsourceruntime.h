// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ANIMATIONSOURCERUNTIME_H
#define KIRIVIEW_ANIMATIONSOURCERUNTIME_H

#include "decoding/imagedecodeworkspace.h"
#include "presentation/imageanimationplaybacksource.h"

#include <QImage>
#include <QString>
#include <QtGlobal>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>

namespace kiriview {
enum class AnimationSourceFrameFailureCause {
    Unavailable,
    ResourceLimitExceeded,
};

struct AnimationSourceFrameFailure
{
    AnimationSourceFrameFailureCause cause = AnimationSourceFrameFailureCause::Unavailable;
    QString errorString;
};

struct AnimationSourceFrame
{
    AnimationSourceFrame() = default;
    AnimationSourceFrame(ImageDecodeWorkspaceHold retainedWorkspace, QImage retainedImage)
        : workspaceHold(std::move(retainedWorkspace))
        , image(std::move(retainedImage))
    {
    }
    AnimationSourceFrame(const AnimationSourceFrame&) = default;
    AnimationSourceFrame(AnimationSourceFrame&&) noexcept = default;
    ~AnimationSourceFrame() = default;
    AnimationSourceFrame& operator=(const AnimationSourceFrame& other)
    {
        if (this == &other) {
            return *this;
        }
        QImage nextImage = other.image;
        ImageDecodeWorkspaceHold nextWorkspaceHold = other.workspaceHold;
        image = {};
        workspaceHold = {};
        workspaceHold = std::move(nextWorkspaceHold);
        image = std::move(nextImage);
        return *this;
    }
    AnimationSourceFrame& operator=(AnimationSourceFrame&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        QImage nextImage = std::move(other.image);
        ImageDecodeWorkspaceHold nextWorkspaceHold = std::move(other.workspaceHold);
        image = {};
        workspaceHold = {};
        workspaceHold = std::move(nextWorkspaceHold);
        image = std::move(nextImage);
        return *this;
    }

    ImageDecodeWorkspaceHold workspaceHold;
    QImage image;
};

using AnimationSourceFrameResult = std::expected<AnimationSourceFrame, AnimationSourceFrameFailure>;
using ImageAnimationPlaybackSourceFactory
    = std::function<std::unique_ptr<ImageAnimationPlaybackSource>()>;

struct AnimationSourceFramePreparation
{
    std::optional<ImageDecodeWorkspaceAdmissionRequest> workspaceAdmission;
    qsizetype outputByteCount = 0;
    bool retirePlaybackSourceBeforeProduction = false;
};

using AnimationSourceFramePreparationResult
    = std::expected<AnimationSourceFramePreparation, AnimationSourceFrameFailure>;

class AnimationSourceRuntime final
{
public:
    AnimationSourceRuntime(QImage retainedFirstFrame, int authoredFrameCount,
        ImageAnimationPlaybackSourceFactory sourceFactory,
        ImageDecodeWorkspaceHold firstFrameWorkspaceHold = {},
        ImageAnimationPlaybackWorkspacePlan workspacePlan = {});
    ~AnimationSourceRuntime();
    Q_DISABLE_COPY_MOVE(AnimationSourceRuntime)

    AnimationSourceFrameResult frame(int authoredFrameIndex);
    AnimationSourceFrameResult frame(int authoredFrameIndex, ImageDecodeWorkspaceLease grant,
        qsizetype perOperationBaselineByteCount);
    [[nodiscard]] AnimationSourceFramePreparationResult prepareFrame(int authoredFrameIndex);
    bool retirePreparedPlaybackSource();
    void releaseRetainedFirstFrameWorkspace();
    void close();

private:
    using FrameTask = std::packaged_task<AnimationSourceFrameResult()>;
    using PreparationTask = std::packaged_task<AnimationSourceFramePreparationResult()>;
    using RetirementTask = std::packaged_task<bool()>;
    using SourceTask = std::variant<FrameTask, PreparationTask, RetirementTask>;

    [[nodiscard]] AnimationSourceFrameResult failedFrame(QString errorString,
        AnimationSourceFrameFailureCause cause
        = AnimationSourceFrameFailureCause::Unavailable) const;
    [[nodiscard]] AnimationSourceFrameResult decodeFrame(int authoredFrameIndex);
    [[nodiscard]] AnimationSourceFrameResult decodeFrame(
        int authoredFrameIndex, const std::shared_ptr<ImageDecodeWorkspaceBudget>& operationBudget);
    [[nodiscard]] AnimationSourceFrameResult openSource(
        const std::shared_ptr<ImageDecodeWorkspaceBudget>& operationBudget = {});
    [[nodiscard]] AnimationSourceFramePreparationResult prepareFrameOnSourceOwner(
        int authoredFrameIndex) const;
    bool enqueueSourceTask(SourceTask task);
    void runSourceOwner();

    ImageDecodeWorkspaceHold m_firstFrameWorkspaceHold;
    QImage m_firstFrame;
    std::mutex m_firstFrameWorkspaceMutex;
    QSize m_frameSize;
    int m_frameCount = 0;
    ImageAnimationPlaybackSourceFactory m_sourceFactory;
    ImageAnimationPlaybackWorkspacePlan m_workspacePlan;
    std::unique_ptr<ImageAnimationPlaybackSource> m_source;
    int m_sourceFrame = 0;
    std::atomic_bool m_closed = false;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::deque<SourceTask> m_sourceTasks;
    std::jthread m_sourceOwner;
};

ImageAnimationPlaybackSourceFactory imageAnimationPlaybackSourceFactory(
    ImageAnimationPlaybackRequest request);
}

#endif
