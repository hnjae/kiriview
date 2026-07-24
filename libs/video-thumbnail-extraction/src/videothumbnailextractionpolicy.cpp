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
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr int maximumInputLongEdge
    = kiriview::VideoThumbnailExtractionLimits::maximumOutputLongEdge * 4;
constexpr qsizetype maximumInputBytes
    = kiriview::VideoThumbnailExtractionLimits::maximumOutputBytes * 4;

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

auto admitVideoThumbnailImage(const QImage& source, int maximumLongEdge)
    -> VideoThumbnailImageAdmission
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return {};
    }

    const int sourceLongEdge = std::max(source.width(), source.height());
    const qsizetype sourceBytes = source.sizeInBytes();
    if (sourceLongEdge > maximumInputLongEdge || sourceBytes <= 0
        || sourceBytes > maximumInputBytes) {
        return { VideoThumbnailImageAdmissionStatus::ResourceLimit, {} };
    }

    QSize outputSize = source.size();
    if (sourceLongEdge > maximumLongEdge) {
        outputSize
            = source.size().scaled(QSize(maximumLongEdge, maximumLongEdge), Qt::KeepAspectRatio);
    }
    if (outputSize.isEmpty() || outputSize.width() <= 0 || outputSize.height() <= 0) {
        return { VideoThumbnailImageAdmissionStatus::ConversionFailure, {} };
    }

    QImage scaled = outputSize == source.size()
        ? source
        : source.scaled(outputSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (scaled.isNull()) {
        return { VideoThumbnailImageAdmissionStatus::ConversionFailure, {} };
    }

    QImage converted = scaled.convertToFormat(QImage::Format_RGBA8888);
    if (converted.isNull()) {
        return { VideoThumbnailImageAdmissionStatus::ConversionFailure, {} };
    }
    QImage owned = converted.copy();
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
