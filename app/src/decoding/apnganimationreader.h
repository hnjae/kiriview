// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APNGANIMATIONREADER_H
#define KIRIVIEW_APNGANIMATIONREADER_H

#include "animationframe.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
enum class ApngOpenStatus {
    NotApng,
    Success,
    Error,
    ResourceLimitExceeded,
};

struct ApngAnimationWorkspacePlan
{
    QSize canvasSize;
    qsizetype transientByteCount = 0;
    qsizetype firstFrameOutputByteCount = 0;
    std::size_t decoderInternalByteLimit = 0;
};

struct ApngAnimationWorkspacePlanResult
{
    ApngOpenStatus status = ApngOpenStatus::NotApng;
    ApngAnimationWorkspacePlan plan;
    QString errorString;
};

[[nodiscard]] ApngAnimationWorkspacePlanResult planApngAnimationOpen(const QByteArray& data);

struct ApngOpenResult // NOLINT(cppcoreguidelines-special-member-functions) --
                      // Pass-by-value assignment preserves aggregate initialization and retires
                      // the old first frame before its workspace hold.
{
    ApngOpenStatus status = ApngOpenStatus::NotApng;
    ImageDecodeWorkspaceHold workspaceHold;
    QImage firstFrame;
    int firstFrameDelay = 0;
    int loopCount = 0;
    int frameCount = 0;
    QString errorString;

    ApngOpenResult& operator=(ApngOpenResult other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(ApngOpenResult& left, ApngOpenResult& right) noexcept
    {
        using std::swap;
        swap(left.status, right.status);
        swap(left.workspaceHold, right.workspaceHold);
        swap(left.firstFrame, right.firstFrame);
        swap(left.firstFrameDelay, right.firstFrameDelay);
        swap(left.loopCount, right.loopCount);
        swap(left.frameCount, right.frameCount);
        swap(left.errorString, right.errorString);
    }
};

class ApngAnimationReader final
{
public:
    ApngAnimationReader();
    explicit ApngAnimationReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget);
    ~ApngAnimationReader();
    Q_DISABLE_COPY_MOVE(ApngAnimationReader)

    ApngOpenResult open(const QByteArray& data);
    ApngOpenResult open(const QByteArray& data, const ApngAnimationWorkspacePlan& plan);
    AnimationFrameReadResult readNextFrame();
    AnimationFrameReadResult readNextFrame(
        std::shared_ptr<ImageDecodeWorkspaceBudget> outputWorkspaceBudget);
    [[nodiscard]] bool hasMoreFrames() const;
    [[nodiscard]] bool lastReadResourceLimitExceeded() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};
}

#endif
