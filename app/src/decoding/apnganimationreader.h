// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_APNGANIMATIONREADER_H
#define KIRIVIEW_APNGANIMATIONREADER_H

#include "animationframe.h"

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
};

struct ApngOpenResult
{
    ApngOpenStatus status = ApngOpenStatus::NotApng;
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
    ~ApngAnimationReader();
    Q_DISABLE_COPY_MOVE(ApngAnimationReader)

    ApngOpenResult open(const QByteArray& data);
    AnimationFrameReadResult readNextFrame();
    [[nodiscard]] bool hasMoreFrames() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};
}

#endif
