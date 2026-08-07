// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_JXLANIMATIONREADER_H
#define KIRIVIEW_JXLANIMATIONREADER_H

#include "animationframe.h"
#include "imageanimationsourcecatalog.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
enum class JxlAnimationOpenStatus {
    NotJxl,
    NotAnimation,
    Success,
    Error,
    ResourceLimitExceeded,
};

struct JxlAnimationOpenResult // NOLINT(cppcoreguidelines-special-member-functions) --
                              // Pass-by-value assignment preserves aggregate initialization and
                              // retires the old first frame before its workspace hold.
{
    JxlAnimationOpenStatus status = JxlAnimationOpenStatus::NotJxl;
    ImageDecodeWorkspaceHold workspaceHold;
    QImage firstFrame;
    int firstFrameDelay = 0;
    int loopCount = 0;
    bool sourceHasMoreFrames = false;
    QString errorString;

    JxlAnimationOpenResult& operator=(JxlAnimationOpenResult other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(JxlAnimationOpenResult& left, JxlAnimationOpenResult& right) noexcept
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

class JxlAnimationReader final
{
public:
    JxlAnimationReader();
    explicit JxlAnimationReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget);
    ~JxlAnimationReader();

    JxlAnimationReader(const JxlAnimationReader&) = delete;
    JxlAnimationReader& operator=(const JxlAnimationReader&) = delete;
    JxlAnimationReader(JxlAnimationReader&&) noexcept;
    JxlAnimationReader& operator=(JxlAnimationReader&&) noexcept;

    JxlAnimationOpenResult open(QByteArray data);
    ImageAnimationSourceCatalogResult readSourceCatalog(QByteArray data);
    AnimationFrameReadResult readNextFrame();
    [[nodiscard]] bool lastReadResourceLimitExceeded() const;
    void close();

private:
    class Private;
    std::unique_ptr<Private> d;
};
}

#endif
