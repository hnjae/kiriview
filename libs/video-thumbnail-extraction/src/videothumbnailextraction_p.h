// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_P_H
#define KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_P_H

#include <VideoThumbnailExtraction/videothumbnailextraction.h>

#include <QImage>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QtTypes>

#include <chrono>
#include <functional>
#include <memory>

namespace kiriview::detail {

enum class VideoThumbnailBackendMediaStatus : std::uint8_t {
    Pending,
    Ready,
    EndOfMedia,
    Invalid,
};

enum class VideoThumbnailBackendError : std::uint8_t {
    Resource,
    Format,
    Network,
    AccessDenied,
    Other,
};

struct VideoThumbnailBackendMediaFacts
{
    VideoThumbnailBackendMediaStatus status = VideoThumbnailBackendMediaStatus::Pending;
    qint64 durationMsec = 0;
    bool seekable = false;
    bool hasVideo = false;
};

struct VideoThumbnailEmbeddedImages
{
    QImage coverArt;
    QImage thumbnail;
};

struct VideoThumbnailBackendFrame
{
    VideoThumbnailBackendFrame() = default;
    VideoThumbnailBackendFrame(QSize pixelSize, std::function<QImage()> materialize)
        : pixelSize(pixelSize)
        , materialize(std::move(materialize))
    {
    }

    VideoThumbnailBackendFrame(const VideoThumbnailBackendFrame&) = delete;
    auto operator=(const VideoThumbnailBackendFrame&) -> VideoThumbnailBackendFrame& = delete;
    VideoThumbnailBackendFrame(VideoThumbnailBackendFrame&&) noexcept = default;
    auto operator=(VideoThumbnailBackendFrame&&) noexcept -> VideoThumbnailBackendFrame& = default;
    ~VideoThumbnailBackendFrame() = default;

    QSize pixelSize;
    std::function<QImage()> materialize;
};

struct VideoThumbnailBackendCallbacks
{
    std::function<void(VideoThumbnailBackendMediaFacts)> mediaFactsChanged;
    std::function<void(qint64)> positionChanged;
    std::function<void(VideoThumbnailBackendFrame)> frameAvailable;
    std::function<void(VideoThumbnailEmbeddedImages)> metadataAvailable;
    std::function<void(VideoThumbnailBackendError, QString)> errorOccurred;
};

class VideoThumbnailBackend
{
public:
    VideoThumbnailBackend() = default;
    VideoThumbnailBackend(const VideoThumbnailBackend&) = delete;
    auto operator=(const VideoThumbnailBackend&) -> VideoThumbnailBackend& = delete;
    VideoThumbnailBackend(VideoThumbnailBackend&&) = delete;
    auto operator=(VideoThumbnailBackend&&) -> VideoThumbnailBackend& = delete;
    virtual ~VideoThumbnailBackend() = default;

    virtual void setCallbacks(VideoThumbnailBackendCallbacks callbacks) = 0;
    virtual void setSource(const QUrl& sourceUrl) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() noexcept = 0;
    virtual void setPosition(qint64 positionMsec) = 0;
};

class VideoThumbnailDeadline
{
public:
    VideoThumbnailDeadline() = default;
    VideoThumbnailDeadline(const VideoThumbnailDeadline&) = delete;
    auto operator=(const VideoThumbnailDeadline&) -> VideoThumbnailDeadline& = delete;
    VideoThumbnailDeadline(VideoThumbnailDeadline&&) = delete;
    auto operator=(VideoThumbnailDeadline&&) -> VideoThumbnailDeadline& = delete;
    virtual ~VideoThumbnailDeadline() = default;

    virtual void start(std::chrono::milliseconds interval, std::function<void()> expired) = 0;
    virtual void stop() noexcept = 0;
};

using VideoThumbnailBackendFactory = std::function<std::unique_ptr<VideoThumbnailBackend>()>;
using VideoThumbnailDeadlineFactory = std::function<std::unique_ptr<VideoThumbnailDeadline>()>;

struct VideoThumbnailRuntimeDependencies
{
    VideoThumbnailBackendFactory backendFactory;
    VideoThumbnailDeadlineFactory deadlineFactory;
};

enum class VideoThumbnailImageAdmissionStatus : std::uint8_t {
    Ready,
    Missing,
    ResourceLimit,
    ConversionFailure,
};

struct VideoThumbnailImageAdmission
{
    VideoThumbnailImageAdmissionStatus status = VideoThumbnailImageAdmissionStatus::Missing;
    QImage image;
};

struct VideoThumbnailExtractionJobControl
{
    bool active = true;
    std::function<void()> cancel;
};

[[nodiscard]] auto createQtVideoThumbnailBackend() -> std::unique_ptr<VideoThumbnailBackend>;
[[nodiscard]] auto createQtVideoThumbnailDeadline() -> std::unique_ptr<VideoThumbnailDeadline>;

[[nodiscard]] auto isVideoThumbnailExtractionRequestValid(
    const VideoThumbnailExtractionRequest& request) -> bool;
[[nodiscard]] auto admitVideoThumbnailFrameSize(const QSize& size)
    -> VideoThumbnailImageAdmissionStatus;
[[nodiscard]] auto admitVideoThumbnailImage(const QImage& source, int maximumLongEdge)
    -> VideoThumbnailImageAdmission;
[[nodiscard]] auto makeVideoThumbnailReadyResult(QImage image) -> VideoThumbnailExtractionResult;
[[nodiscard]] auto makeVideoThumbnailFailureResult(VideoThumbnailExtractionFailureCause cause)
    -> VideoThumbnailExtractionResult;
[[nodiscard]] auto makeVideoThumbnailBackendFailureResult(VideoThumbnailBackendError error)
    -> VideoThumbnailExtractionResult;

void startVideoThumbnailExtractionOperation(QObject* receiver,
    VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback completion,
    VideoThumbnailRuntimeDependencies dependencies,
    const std::shared_ptr<VideoThumbnailExtractionJobControl>& control);

[[nodiscard]] auto startVideoThumbnailExtractionWithDependencies(QObject* receiver,
    VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback completion,
    VideoThumbnailRuntimeDependencies dependencies) -> VideoThumbnailExtractionJob;

} // namespace kiriview::detail

#endif
