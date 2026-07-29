// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "animationsourceruntime.h"

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
    ImageAnimationPlaybackSourceFactory sourceFactory)
    : m_firstFrame(std::move(retainedFirstFrame))
    , m_frameCount(authoredFrameCount)
    , m_sourceFactory(std::move(sourceFactory))
{
}

AnimationSourceRuntime::~AnimationSourceRuntime() = default;

AnimationSourceFrameResult AnimationSourceRuntime::frame(int authoredFrameIndex)
{
    if (m_closed.load(std::memory_order_acquire)) {
        return failedFrame(closedSourceError());
    }

    const std::scoped_lock lock(m_mutex);
    if (m_closed.load(std::memory_order_relaxed)) {
        return failedFrame(closedSourceError());
    }
    if (m_firstFrame.isNull() || authoredFrameIndex < 0 || authoredFrameIndex >= m_frameCount) {
        return failedFrame(unavailableFrameError());
    }
    if (authoredFrameIndex == 0) {
        m_source.reset();
        m_sourceFrame = 0;
        return m_firstFrame;
    }

    if (m_source == nullptr || authoredFrameIndex <= m_sourceFrame) {
        AnimationSourceFrameResult opened = openSource();
        if (!opened.has_value()) {
            return opened;
        }
    }

    QImage decodedFrame;
    while (m_sourceFrame < authoredFrameIndex) {
        if (m_closed.load(std::memory_order_relaxed)) {
            return failedFrame(closedSourceError());
        }
        ImageAnimationPlaybackReadResult read = m_source->readNextFrame();
        if (read.status == ImageAnimationPlaybackReadStatus::Error) {
            m_source.reset();
            return failedFrame(
                read.errorString.isEmpty() ? unavailableFrameError() : std::move(read.errorString));
        }
        if (read.status != ImageAnimationPlaybackReadStatus::Frame || read.frame.image.isNull()
            || read.frame.image.size() != m_firstFrame.size()) {
            m_source.reset();
            return failedFrame(unavailableFrameError());
        }
        ++m_sourceFrame;
        decodedFrame = std::move(read.frame.image);
    }
    return decodedFrame;
}

void AnimationSourceRuntime::close() { m_closed.store(true, std::memory_order_release); }

AnimationSourceFrameResult AnimationSourceRuntime::failedFrame(QString errorString) const
{
    return std::unexpected(std::move(errorString));
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
        || opened.firstFrame.size() != m_firstFrame.size()) {
        m_source.reset();
        return failedFrame(
            opened.errorString.isEmpty() ? unavailableFrameError() : std::move(opened.errorString));
    }
    return opened.firstFrame;
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
