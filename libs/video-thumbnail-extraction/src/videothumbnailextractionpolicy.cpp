// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "videothumbnailextraction_p.h"

#include <QChar>
#include <QList>
#include <QSize>
#include <QString>
#include <Qt>
#include <QtTypes>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr int maximumInputLongEdge
    = kiriview::VideoThumbnailExtractionLimits::maximumOutputLongEdge * 4;
constexpr qsizetype maximumInputBytes
    = kiriview::VideoThumbnailExtractionLimits::maximumOutputBytes * 4;
constexpr qsizetype maximumMaterializedBytesPerPixel = 16;

auto rgbaBytesForSize(const QSize& size) -> std::optional<qsizetype>
{
    if (size.isEmpty() || size.width() <= 0 || size.height() <= 0) {
        return std::nullopt;
    }

    constexpr qsizetype bytesPerPixel = 4;
    const qsizetype width = size.width();
    const qsizetype height = size.height();
    const qsizetype maximum = std::numeric_limits<qsizetype>::max();
    if (width > maximum / bytesPerPixel || height > maximum / (width * bytesPerPixel)) {
        return std::nullopt;
    }
    return width * height * bytesPerPixel;
}

auto sumWithin(qsizetype left, qsizetype right, qsizetype maximum) -> std::optional<qsizetype>
{
    if (left < 0 || right < 0 || left > maximum || right > maximum - left) {
        return std::nullopt;
    }
    return left + right;
}

auto normalizedDiagnostic(const QString& diagnostic) -> QString
{
    const QList<uint> scalars = diagnostic.toUcs4();
    std::vector<char32_t> normalized;
    normalized.reserve(std::min(static_cast<std::size_t>(scalars.size()),
        static_cast<std::size_t>(
            kiriview::VideoThumbnailExtractionLimits::maximumDiagnosticCharacters)));

    bool previousWasSpace = true;
    for (uint scalar : scalars) {
        const bool control = scalar <= 0x1f || (scalar >= 0x7f && scalar <= 0x9f)
            || scalar == 0x2028 || scalar == 0x2029;
        const bool space = control || QChar::isSpace(static_cast<char32_t>(scalar));
        if (space) {
            if (!previousWasSpace
                && normalized.size() < static_cast<std::size_t>(
                       kiriview::VideoThumbnailExtractionLimits::maximumDiagnosticCharacters)) {
                normalized.push_back(U' ');
            }
            previousWasSpace = true;
            continue;
        }
        if (normalized.size() >= static_cast<std::size_t>(
                kiriview::VideoThumbnailExtractionLimits::maximumDiagnosticCharacters)) {
            break;
        }
        normalized.push_back(static_cast<char32_t>(scalar));
        previousWasSpace = false;
    }
    if (!normalized.empty() && normalized.back() == U' ') {
        normalized.pop_back();
    }
    return QString::fromUcs4(normalized.data(), static_cast<qsizetype>(normalized.size()));
}

auto diagnosticFor(kiriview::VideoThumbnailExtractionFailureCause cause) -> QString
{
    using Cause = kiriview::VideoThumbnailExtractionFailureCause;
    switch (cause) {
    case Cause::InvalidRequest:
        return QStringLiteral("The video thumbnail extraction request is invalid.");
    case Cause::SourceUnavailable:
        return QStringLiteral("The video source could not be opened.");
    case Cause::UnsupportedMedia:
        return QStringLiteral("The video source is not supported by the multimedia backend.");
    case Cause::BackendFailure:
        return QStringLiteral("The multimedia backend could not extract a thumbnail.");
    case Cause::TimedOut:
        return QStringLiteral("Video thumbnail extraction exceeded its deadline.");
    case Cause::NoRepresentativeImage:
        return QStringLiteral("The video source produced no representative image.");
    case Cause::ResourceLimit:
        return QStringLiteral("The video thumbnail candidate exceeded a resource limit.");
    }
    return {};
}

auto failureCause(kiriview::detail::VideoThumbnailBackendError error)
    -> kiriview::VideoThumbnailExtractionFailureCause
{
    using BackendError = kiriview::detail::VideoThumbnailBackendError;
    using FailureCause = kiriview::VideoThumbnailExtractionFailureCause;

    switch (error) {
    case BackendError::Resource:
    case BackendError::Network:
    case BackendError::AccessDenied:
        return FailureCause::SourceUnavailable;
    case BackendError::Format:
        return FailureCause::UnsupportedMedia;
    case BackendError::Other:
        return FailureCause::BackendFailure;
    }
    return FailureCause::BackendFailure;
}

} // namespace

namespace kiriview::detail {

auto isVideoThumbnailExtractionRequestValid(const VideoThumbnailExtractionRequest& request) -> bool
{
    return !request.sourceUrl.isEmpty() && request.sourceUrl.isValid()
        && request.maximumLongEdge > 0
        && request.maximumLongEdge <= VideoThumbnailExtractionLimits::maximumOutputLongEdge;
}

auto admitVideoThumbnailFrameSize(const QSize& size) -> VideoThumbnailImageAdmissionStatus
{
    if (size.isEmpty() || size.width() <= 0 || size.height() <= 0) {
        return VideoThumbnailImageAdmissionStatus::Missing;
    }

    const int sourceLongEdge = std::max(size.width(), size.height());
    const qsizetype width = size.width();
    const qsizetype height = size.height();
    const qsizetype maximumInputPixels = maximumInputBytes / maximumMaterializedBytesPerPixel;
    if (sourceLongEdge > maximumInputLongEdge || width > maximumInputPixels / height) {
        return VideoThumbnailImageAdmissionStatus::ResourceLimit;
    }
    return VideoThumbnailImageAdmissionStatus::Ready;
}

auto admitVideoThumbnailImageResources(const VideoThumbnailImageResources& resources,
    int maximumLongEdge, qsizetype retainedSourceBytes) -> VideoThumbnailImageAdmissionStatus
{
    const VideoThumbnailImageAdmissionStatus sizeStatus
        = admitVideoThumbnailFrameSize(resources.pixelSize);
    if (sizeStatus != VideoThumbnailImageAdmissionStatus::Ready) {
        return sizeStatus;
    }

    const int sourceLongEdge = std::max(resources.pixelSize.width(), resources.pixelSize.height());
    if (resources.sourceBytes <= 0 || resources.sourceBytes > maximumInputBytes
        || retainedSourceBytes < resources.sourceBytes) {
        return VideoThumbnailImageAdmissionStatus::ResourceLimit;
    }

    QSize outputSize = resources.pixelSize;
    if (sourceLongEdge > maximumLongEdge) {
        outputSize = resources.pixelSize.scaled(
            QSize(maximumLongEdge, maximumLongEdge), Qt::KeepAspectRatio);
    }
    if (outputSize.isEmpty() || outputSize.width() <= 0 || outputSize.height() <= 0) {
        return VideoThumbnailImageAdmissionStatus::ConversionFailure;
    }

    const std::optional<qsizetype> convertedBytes = rgbaBytesForSize(resources.pixelSize);
    const std::optional<qsizetype> outputBytes = rgbaBytesForSize(outputSize);
    if (!convertedBytes.has_value() || !outputBytes.has_value()
        || *outputBytes > VideoThumbnailExtractionLimits::maximumOutputBytes) {
        return VideoThumbnailImageAdmissionStatus::ResourceLimit;
    }

    qsizetype additionalPeak = 0;
    const bool requiresConversion = resources.format != QImage::Format_RGBA8888;
    const bool requiresScaling = outputSize != resources.pixelSize;
    if (requiresConversion) {
        additionalPeak = *convertedBytes;
    }
    if (requiresScaling || !requiresConversion) {
        const auto peak = sumWithin(
            additionalPeak, *outputBytes, VideoThumbnailExtractionLimits::maximumWorkingBytes);
        if (!peak.has_value()) {
            return VideoThumbnailImageAdmissionStatus::ResourceLimit;
        }
        additionalPeak = *peak;
    }

    if (!sumWithin(retainedSourceBytes, additionalPeak,
            VideoThumbnailExtractionLimits::maximumWorkingBytes)
            .has_value()) {
        return VideoThumbnailImageAdmissionStatus::ResourceLimit;
    }
    return VideoThumbnailImageAdmissionStatus::Ready;
}

auto admitVideoThumbnailImage(const QImage& source, int maximumLongEdge,
    qsizetype retainedSourceBytes) -> VideoThumbnailImageAdmission
{
    if (source.isNull()) {
        return {};
    }

    const VideoThumbnailImageResources resources { source.size(), source.format(),
        source.sizeInBytes() };
    const VideoThumbnailImageAdmissionStatus resourceStatus
        = admitVideoThumbnailImageResources(resources, maximumLongEdge, retainedSourceBytes);
    if (resourceStatus != VideoThumbnailImageAdmissionStatus::Ready) {
        return { resourceStatus, {} };
    }

    const int sourceLongEdge = std::max(source.width(), source.height());
    QSize outputSize = source.size();
    if (sourceLongEdge > maximumLongEdge) {
        outputSize
            = source.size().scaled(QSize(maximumLongEdge, maximumLongEdge), Qt::KeepAspectRatio);
    }

    const bool requiresConversion = source.format() != QImage::Format_RGBA8888;
    const bool requiresScaling = outputSize != source.size();
    QImage converted
        = requiresConversion ? source.convertToFormat(QImage::Format_RGBA8888) : source;
    if (converted.isNull()) {
        return { VideoThumbnailImageAdmissionStatus::ConversionFailure, {} };
    }

    QImage owned;
    if (requiresScaling) {
        owned = converted.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        converted = {};
    } else if (requiresConversion) {
        owned = std::move(converted);
    } else {
        owned = source.copy();
    }
    if (owned.isNull()) {
        return { VideoThumbnailImageAdmissionStatus::ConversionFailure, {} };
    }

    const int outputLongEdge = std::max(owned.width(), owned.height());
    if (owned.width() <= 0 || owned.height() <= 0 || outputLongEdge > maximumLongEdge
        || owned.sizeInBytes() <= 0
        || owned.sizeInBytes() > VideoThumbnailExtractionLimits::maximumOutputBytes) {
        return { VideoThumbnailImageAdmissionStatus::ResourceLimit, {} };
    }
    return { VideoThumbnailImageAdmissionStatus::Ready, std::move(owned) };
}

auto makeVideoThumbnailReadyResult(QImage image) -> VideoThumbnailExtractionResult
{
    return { VideoThumbnailExtractionStatus::Ready, std::move(image), std::nullopt };
}

auto makeVideoThumbnailFailureResult(VideoThumbnailExtractionFailureCause cause)
    -> VideoThumbnailExtractionResult
{
    return {
        VideoThumbnailExtractionStatus::Failed,
        {},
        VideoThumbnailExtractionFailure { cause, normalizedDiagnostic(diagnosticFor(cause)) },
    };
}

auto makeVideoThumbnailBackendFailureResult(VideoThumbnailBackendError error)
    -> VideoThumbnailExtractionResult
{
    return makeVideoThumbnailFailureResult(failureCause(error));
}

} // namespace kiriview::detail
