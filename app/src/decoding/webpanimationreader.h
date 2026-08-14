// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_WEBPANIMATIONREADER_H
#define KIRIVIEW_WEBPANIMATIONREADER_H

#include "animationframe.h"
#include "imagedecodeworkspace.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
struct WebPAnimationLibraryVersions
{
    int decoder = 0;
    int demux = 0;
};

inline constexpr int auditedWebPAnimationLibraryVersion = 0x010600;

[[nodiscard]] bool webPAnimationWorkspaceModelSupports(WebPAnimationLibraryVersions versions);
[[nodiscard]] WebPAnimationLibraryVersions currentWebPAnimationLibraryVersions();

enum class WebPAnimationOpenStatus {
    NotWebP,
    NotAnimation,
    Success,
    Error,
    ResourceLimitExceeded,
};

struct WebPAnimationWorkspacePlan
{
    QSize canvasSize;
    qsizetype transientByteCount = 0;
    qsizetype firstFrameOutputByteCount = 0;
};

struct WebPAnimationWorkspacePlanResult
{
    WebPAnimationOpenStatus status = WebPAnimationOpenStatus::NotWebP;
    WebPAnimationWorkspacePlan plan;
    QString errorString;
};

[[nodiscard]] WebPAnimationWorkspacePlanResult planWebPAnimationOpen(const QByteArray& data,
    WebPAnimationLibraryVersions versions = currentWebPAnimationLibraryVersions());

struct WebPAnimationOpenResult // NOLINT(cppcoreguidelines-special-member-functions) --
                               // Pass-by-value assignment preserves aggregate initialization and
                               // retires the old first frame before its workspace hold.
{
    WebPAnimationOpenStatus status = WebPAnimationOpenStatus::NotWebP;
    ImageDecodeWorkspaceHold workspaceHold;
    QImage firstFrame;
    int firstFrameDelay = 0;
    int loopCount = 0;
    bool sourceHasMoreFrames = false;
    QString errorString;

    WebPAnimationOpenResult& operator=(WebPAnimationOpenResult other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(WebPAnimationOpenResult& left, WebPAnimationOpenResult& right) noexcept
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

class WebPAnimationReader final
{
public:
    WebPAnimationReader();
    explicit WebPAnimationReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget);
    WebPAnimationReader(std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
        WebPAnimationLibraryVersions libraryVersions);
    ~WebPAnimationReader();

    WebPAnimationReader(const WebPAnimationReader&) = delete;
    WebPAnimationReader& operator=(const WebPAnimationReader&) = delete;
    WebPAnimationReader(WebPAnimationReader&&) noexcept;
    WebPAnimationReader& operator=(WebPAnimationReader&&) noexcept;

    WebPAnimationOpenResult open(QByteArray data);
    WebPAnimationOpenResult open(QByteArray data, const WebPAnimationWorkspacePlan& plan);
    AnimationFrameReadResult readNextFrame();
    AnimationFrameReadResult readNextFrame(
        const std::shared_ptr<ImageDecodeWorkspaceBudget>& outputWorkspaceBudget);
    [[nodiscard]] bool hasMoreFrames() const;
    [[nodiscard]] bool lastReadResourceLimitExceeded() const;
    void close();

private:
    class Private;
    std::unique_ptr<Private> d;
};
}

#endif
