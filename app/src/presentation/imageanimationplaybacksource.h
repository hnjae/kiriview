// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEANIMATIONPLAYBACKSOURCE_H
#define KIRIVIEW_IMAGEANIMATIONPLAYBACKSOURCE_H

#include "decoding/animationframe.h"
#include "decoding/imageanimationrequest.h"

#include <QImage>
#include <QString>
#include <QtGlobal>
#include <memory>

namespace kiriview {
enum class ImageAnimationPlaybackOpenStatus {
    Success,
    Error,
    ResourceLimitExceeded,
};

struct ImageAnimationPlaybackOpenResult
{
    ImageAnimationPlaybackOpenStatus status = ImageAnimationPlaybackOpenStatus::Error;
    ImageDecodeWorkspaceHold workspaceHold;
    QImage firstFrame;
    int firstFrameDelay = 0;
    int loopCount = 0;
    bool sourceHasMoreFrames = false;
    QString errorString;
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

class ImageAnimationPlaybackSource
{
public:
    ImageAnimationPlaybackSource() = default;
    virtual ~ImageAnimationPlaybackSource() = default;

    virtual ImageAnimationPlaybackOpenResult open() = 0;
    virtual ImageAnimationPlaybackReadResult readNextFrame() = 0;
    [[nodiscard]] virtual bool restartable() const = 0;
    void retainSourceDataLease(ImageSourceDataLease sourceDataLease);
    Q_DISABLE_COPY_MOVE(ImageAnimationPlaybackSource)

private:
    ImageSourceDataLease m_sourceDataLease;
};

std::unique_ptr<ImageAnimationPlaybackSource> makeImageAnimationPlaybackSource(
    ImageAnimationPlaybackRequest request);
}

#endif
