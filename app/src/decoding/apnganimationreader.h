// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APNGANIMATIONREADER_H
#define KIRIVIEW_APNGANIMATIONREADER_H

#include "animationframe.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <memory>
#include <optional>

namespace kiriview {
enum class ApngOpenStatus {
    NotApng,
    Success,
    Error,
    ResourceLimitExceeded,
};

struct ApngOpenResult
{
    ApngOpenStatus status = ApngOpenStatus::NotApng;
    ImageDecodeWorkspaceHold workspaceHold;
    QImage firstFrame;
    int firstFrameDelay = 0;
    int loopCount = 0;
    int frameCount = 0;
    QString errorString;
};

class ApngAnimationReader final
{
public:
    ApngAnimationReader();
    explicit ApngAnimationReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget);
    ~ApngAnimationReader();
    Q_DISABLE_COPY_MOVE(ApngAnimationReader)

    ApngOpenResult open(const QByteArray& data);
    AnimationFrameReadResult readNextFrame();
    [[nodiscard]] bool hasMoreFrames() const;
    [[nodiscard]] bool lastReadResourceLimitExceeded() const;
    [[nodiscard]] ImageDecodeWorkspaceHold takeFirstFrameWorkspaceHold();

private:
    class Private;
    std::unique_ptr<Private> d;
};
}

#endif
