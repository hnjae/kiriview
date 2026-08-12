// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_H
#define KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_H

#include <QImage>
#include <QString>
#include <QUrl>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

class QObject;

namespace kiriview {

struct VideoThumbnailExtractionLimits
{
    static constexpr int maximumOutputLongEdge = 4096;
    static constexpr qsizetype maximumOutputBytes = 67'108'864;
    static constexpr qsizetype maximumWorkingBytes = 536'870'912;
    static constexpr qsizetype maximumDiagnosticCharacters = 1024;
};

struct VideoThumbnailExtractionRequest
{
    QUrl sourceUrl;
    int maximumLongEdge = 0;
};

enum class VideoThumbnailExtractionFailureCause : std::uint8_t {
    InvalidRequest,
    SourceUnavailable,
    UnsupportedMedia,
    BackendFailure,
    TimedOut,
    NoRepresentativeImage,
    ResourceLimit,
};

struct VideoThumbnailExtractionFailure
{
    VideoThumbnailExtractionFailureCause cause
        = VideoThumbnailExtractionFailureCause::BackendFailure;
    QString diagnostic;
};

enum class VideoThumbnailExtractionStatus : std::uint8_t {
    Ready,
    Failed,
};

struct VideoThumbnailExtractionResult
{
    VideoThumbnailExtractionStatus status = VideoThumbnailExtractionStatus::Failed;
    QImage image;
    std::optional<VideoThumbnailExtractionFailure> failure;
};

class VideoThumbnailExtractionJob final
{
public:
    VideoThumbnailExtractionJob() noexcept = default;
    VideoThumbnailExtractionJob(const VideoThumbnailExtractionJob&) = delete;
    auto operator=(const VideoThumbnailExtractionJob&) -> VideoThumbnailExtractionJob& = delete;
    VideoThumbnailExtractionJob(VideoThumbnailExtractionJob&& other) noexcept;
    auto operator=(VideoThumbnailExtractionJob&& other) noexcept -> VideoThumbnailExtractionJob&;
    ~VideoThumbnailExtractionJob();

    void cancel() noexcept;
    [[nodiscard]] auto isActive() const noexcept -> bool;

private:
    class State;

    explicit VideoThumbnailExtractionJob(std::shared_ptr<State> state) noexcept;

    std::shared_ptr<State> state_;

    friend auto startVideoThumbnailExtraction(QObject*, VideoThumbnailExtractionRequest,
        std::function<void(VideoThumbnailExtractionResult)>) -> VideoThumbnailExtractionJob;
};

using VideoThumbnailExtractionCallback = std::function<void(VideoThumbnailExtractionResult)>;

[[nodiscard]] auto startVideoThumbnailExtraction(QObject* receiver,
    VideoThumbnailExtractionRequest request, VideoThumbnailExtractionCallback completion)
    -> VideoThumbnailExtractionJob;

} // namespace kiriview

#endif
