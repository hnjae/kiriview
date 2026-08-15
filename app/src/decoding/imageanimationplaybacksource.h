// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEANIMATIONPLAYBACKSOURCE_H
#define KIRIVIEW_IMAGEANIMATIONPLAYBACKSOURCE_H

#include "decoding/animationframe.h"
#include "decoding/imageanimationrequest.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
enum class ImageAnimationPlaybackOpenStatus {
    Success,
    Error,
    ResourceLimitExceeded,
};

struct ImageAnimationPlaybackOpenResult
{
    ImageAnimationPlaybackOpenResult() = default;
    ImageAnimationPlaybackOpenResult(ImageAnimationPlaybackOpenStatus openStatus,
        ImageDecodeWorkspaceHold retainedWorkspace, QImage initialFrame, int initialFrameDelay,
        int animationLoopCount, bool hasMoreFrames, QString diagnostic)
        : status(openStatus)
        , workspaceHold(std::move(retainedWorkspace))
        , firstFrame(std::move(initialFrame))
        , firstFrameDelay(initialFrameDelay)
        , loopCount(animationLoopCount)
        , sourceHasMoreFrames(hasMoreFrames)
        , errorString(std::move(diagnostic))
    {
    }
    ImageAnimationPlaybackOpenResult(const ImageAnimationPlaybackOpenResult&) = default;
    ImageAnimationPlaybackOpenResult(ImageAnimationPlaybackOpenResult&&) noexcept = default;
    ~ImageAnimationPlaybackOpenResult() = default;

    ImageAnimationPlaybackOpenStatus status = ImageAnimationPlaybackOpenStatus::Error;
    ImageDecodeWorkspaceHold workspaceHold;
    QImage firstFrame;
    int firstFrameDelay = 0;
    int loopCount = 0;
    bool sourceHasMoreFrames = false;
    QString errorString;

    ImageAnimationPlaybackOpenResult& operator=(const ImageAnimationPlaybackOpenResult& other)
    {
        if (this == &other) {
            return *this;
        }
        ImageAnimationPlaybackOpenResult copy(other);
        swap(*this, copy);
        return *this;
    }

    ImageAnimationPlaybackOpenResult& operator=(ImageAnimationPlaybackOpenResult&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        ImageAnimationPlaybackOpenResult moved(std::move(other));
        swap(*this, moved);
        return *this;
    }

    friend void swap(
        ImageAnimationPlaybackOpenResult& left, ImageAnimationPlaybackOpenResult& right) noexcept
    {
        using std::swap;
        swap(left.status, right.status);
        swap(left.workspaceHold, right.workspaceHold);
        swap(left.firstFrame, right.firstFrame);
        swap(left.firstFrameDelay, right.firstFrameDelay);
        swap(left.loopCount, right.loopCount);
        swap(left.sourceHasMoreFrames, right.sourceHasMoreFrames);
        swap(left.errorString, right.errorString);
    }
};

enum class ImageAnimationPlaybackReadStatus {
    Frame,
    End,
    Error,
    ResourceLimitExceeded,
};

struct ImageAnimationPlaybackReadResult
{
    ImageAnimationPlaybackReadStatus status = ImageAnimationPlaybackReadStatus::End;
    AnimationFrame frame;
    bool sourceHasMoreFrames = false;
    QString errorString;
};

struct ImageAnimationPlaybackWorkspacePlan
{
    qsizetype retainedInputByteCount = 0;
    qsizetype persistentDecoderByteCount = 0;
    qsizetype frameOutputByteCount = 0;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] std::optional<qsizetype> openPeakByteCount() const;
};

class ImageAnimationPlaybackSource
{
public:
    ImageAnimationPlaybackSource() = default;
    virtual ~ImageAnimationPlaybackSource() = default;

    virtual ImageAnimationPlaybackOpenResult open() = 0;
    virtual ImageAnimationPlaybackReadResult readNextFrame() = 0;
    virtual ImageAnimationPlaybackOpenResult openAdmitted(
        std::shared_ptr<ImageDecodeWorkspaceBudget> operationBudget);
    virtual ImageAnimationPlaybackReadResult readNextFrameAdmitted(
        std::shared_ptr<ImageDecodeWorkspaceBudget> operationBudget);
    [[nodiscard]] virtual bool restartable() const = 0;
    void retainSourceDataLease(ImageSourceDataLease sourceDataLease);
    void retainInputWorkspace(ImageDecodeWorkspaceHold inputWorkspaceHold);
    Q_DISABLE_COPY_MOVE(ImageAnimationPlaybackSource)

private:
    ImageSourceDataLease m_sourceDataLease;
    ImageDecodeWorkspaceHold m_inputWorkspaceHold;
};

std::unique_ptr<ImageAnimationPlaybackSource> makeImageAnimationPlaybackSource(
    ImageAnimationPlaybackRequest request);
[[nodiscard]] std::optional<ImageAnimationPlaybackWorkspacePlan>
imageAnimationPlaybackWorkspacePlan(
    const ImageAnimationPlaybackRequest& request, QSize logicalSize);
}

#endif
