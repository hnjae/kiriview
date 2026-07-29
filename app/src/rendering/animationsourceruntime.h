// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ANIMATIONSOURCERUNTIME_H
#define KIRIVIEW_ANIMATIONSOURCERUNTIME_H

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
#include <thread>

namespace kiriview {
using AnimationSourceFrameResult = std::expected<QImage, QString>;
using ImageAnimationPlaybackSourceFactory
    = std::function<std::unique_ptr<ImageAnimationPlaybackSource>()>;

class AnimationSourceRuntime final
{
public:
    AnimationSourceRuntime(QImage retainedFirstFrame, int authoredFrameCount,
        ImageAnimationPlaybackSourceFactory sourceFactory);
    ~AnimationSourceRuntime();
    Q_DISABLE_COPY_MOVE(AnimationSourceRuntime)

    AnimationSourceFrameResult frame(int authoredFrameIndex);
    void close();

private:
    using FrameTask = std::packaged_task<AnimationSourceFrameResult()>;

    [[nodiscard]] AnimationSourceFrameResult failedFrame(QString errorString) const;
    [[nodiscard]] AnimationSourceFrameResult decodeFrame(int authoredFrameIndex);
    [[nodiscard]] AnimationSourceFrameResult openSource();
    void runSourceOwner();

    QImage m_firstFrame;
    int m_frameCount = 0;
    ImageAnimationPlaybackSourceFactory m_sourceFactory;
    std::unique_ptr<ImageAnimationPlaybackSource> m_source;
    int m_sourceFrame = 0;
    std::atomic_bool m_closed = false;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::deque<FrameTask> m_frameTasks;
    std::jthread m_sourceOwner;
};

ImageAnimationPlaybackSourceFactory imageAnimationPlaybackSourceFactory(
    ImageAnimationPlaybackRequest request);
}

#endif
