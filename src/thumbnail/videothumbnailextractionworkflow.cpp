// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnail/videothumbnailextractionworkflow.h"

#include <QColor>
#include <QSize>
#include <Qt>
#include <algorithm>
#include <utility>

namespace {
constexpr int extractionTimeoutMsec = 10000;
constexpr double boringFrameVarianceThreshold = 256.0;

QSize boundedSize(QSize size, int maximumLongEdge)
{
    if (size.isEmpty() || maximumLongEdge <= 0) {
        return {};
    }
    const int longEdge = std::max(size.width(), size.height());
    return longEdge <= maximumLongEdge
        ? size
        : size.scaled(QSize(maximumLongEdge, maximumLongEdge), Qt::KeepAspectRatio);
}

kiriview::VideoThumbnailExtractionResult failedExtraction(QString errorString)
{
    return { kiriview::VideoThumbnailExtractionStatus::Failed, {}, std::move(errorString) };
}
}

namespace kiriview {
QImage videoThumbnailImageFromFrameImage(QImage image, int maximumLongEdge, QString* errorString)
{
    if (image.isNull()) {
        if (errorString != nullptr) {
            *errorString = QStringLiteral("video frame produced no image");
        }
        return {};
    }
    const QSize targetSize = boundedSize(image.size(), maximumLongEdge);
    if (targetSize.isEmpty()) {
        if (errorString != nullptr) {
            *errorString = QStringLiteral("video thumbnail requires a valid size");
        }
        return {};
    }
    return targetSize == image.size()
        ? image
        : image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage videoThumbnailImageFromEmbeddedImages(
    VideoThumbnailEmbeddedImages images, int maximumLongEdge, QString* errorString)
{
    QImage image
        = images.coverArt.isNull() ? std::move(images.thumbnail) : std::move(images.coverArt);
    if (image.isNull()) {
        if (errorString != nullptr) {
            *errorString = QStringLiteral("video metadata produced no embedded image");
        }
        return {};
    }
    return videoThumbnailImageFromFrameImage(std::move(image), maximumLongEdge, errorString);
}

QVector<qint64> videoThumbnailCandidatePositions(qint64 durationMsec)
{
    if (durationMsec <= 0) {
        return {};
    }
    QVector<qint64> positions;
    const auto appendPosition = [&positions, durationMsec](qint64 numerator, qint64 denominator) {
        const qint64 position = std::clamp((durationMsec / denominator) * numerator
                + ((durationMsec % denominator) * numerator) / denominator,
            qint64(0), durationMsec);
        if (!positions.contains(position)) {
            positions.append(position);
        }
    };
    appendPosition(1, 3);
    appendPosition(2, 3);
    appendPosition(1, 10);
    appendPosition(9, 10);
    appendPosition(1, 2);
    return positions;
}

bool videoThumbnailFrameIsInteresting(const QImage& image)
{
    if (image.isNull() || image.size().isEmpty()) {
        return false;
    }
    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    if (rgbImage.isNull() || rgbImage.size().isEmpty()) {
        return false;
    }
    const qsizetype bytesPerRow = static_cast<qsizetype>(rgbImage.width()) * 3;
    const qsizetype byteCount = bytesPerRow * rgbImage.height();
    if (byteCount <= 1) {
        return false;
    }
    double count = 0.0;
    double mean = 0.0;
    double sumOfSquaredDifferences = 0.0;
    for (int y = 0; y < rgbImage.height(); ++y) {
        const uchar* line = rgbImage.constScanLine(y);
        for (qsizetype x = 0; x < bytesPerRow; ++x) {
            count += 1.0;
            const double value = line[x];
            const double delta = value - mean;
            mean += delta / count;
            sumOfSquaredDifferences += delta * (value - mean);
        }
    }
    return sumOfSquaredDifferences / (count - 1.0) > boringFrameVarianceThreshold;
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::start(
    VideoThumbnailExtractionRequest request)
{
    if (m_started) {
        return {};
    }
    m_started = true;
    m_request = std::move(request);
    if (m_request.sourceUrl.isEmpty()) {
        return finish(failedExtraction(QStringLiteral("video thumbnail source URL is empty")));
    }
    if (m_request.maximumLongEdge <= 0) {
        return finish(failedExtraction(QStringLiteral("video thumbnail requires a size bucket")));
    }
    return { StartVideoThumbnailTimeout { extractionTimeoutMsec },
        SetVideoThumbnailSource { m_request.sourceUrl } };
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::handleMediaFacts(
    VideoThumbnailBackendMediaFacts facts)
{
    if (m_finished) {
        return {};
    }
    switch (facts.status) {
    case VideoThumbnailBackendMediaStatus::Ready:
        if (!m_frameExtractionStarted) {
            m_frameExtractionStarted = true;
            if (facts.durationMsec > 0 && facts.seekable) {
                m_candidatePositions = videoThumbnailCandidatePositions(facts.durationMsec);
                if (!m_candidatePositions.isEmpty()) {
                    return seekToNextCandidate();
                }
            }
            return startFirstFrameFallback();
        }
        break;
    case VideoThumbnailBackendMediaStatus::EndOfMedia:
        if (m_frameExtractionStarted) {
            if (m_firstFrameFallback) {
                return finish(
                    failedExtraction(QStringLiteral("video thumbnail decode produced no frame")));
            }
            if (m_awaitingCandidatePosition || m_awaitingCandidateFrame) {
                m_awaitingCandidatePosition = false;
                m_awaitingCandidateFrame = false;
                return advanceToNextCandidate();
            }
        }
        break;
    case VideoThumbnailBackendMediaStatus::Invalid:
        return finish(failedExtraction(QStringLiteral("video thumbnail media is invalid")));
    case VideoThumbnailBackendMediaStatus::Pending:
        break;
    }
    return {};
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::handlePositionChanged(qint64)
{
    if (m_finished || !m_awaitingCandidatePosition) {
        return {};
    }
    m_awaitingCandidatePosition = false;
    m_awaitingCandidateFrame = true;
    return {};
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::handleFrame(QImage image)
{
    if (m_finished || (!m_firstFrameFallback && !m_awaitingCandidateFrame)) {
        return {};
    }
    if (m_firstFrameFallback) {
        return finishReadyFromImage(
            std::move(image), QStringLiteral("video thumbnail frame conversion failed"));
    }
    return acceptCandidateFrame(std::move(image));
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::handleMetadata(
    VideoThumbnailEmbeddedImages images)
{
    if (m_finished) {
        return {};
    }
    QString errorString;
    QImage image = videoThumbnailImageFromEmbeddedImages(
        std::move(images), m_request.maximumLongEdge, &errorString);
    if (image.isNull()) {
        return {};
    }
    return finish({ VideoThumbnailExtractionStatus::Ready, std::move(image), {} });
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::handleBackendError(
    QString errorString)
{
    if (m_finished) {
        return {};
    }
    return finish(
        failedExtraction(errorString.isEmpty() ? QStringLiteral("video thumbnail decode failed")
                                               : std::move(errorString)));
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::handleTimeout()
{
    if (m_finished) {
        return {};
    }
    if (!m_firstFrameFallback && !m_lastCapturedCandidate.isNull()) {
        return finishReadyFromImage(std::move(m_lastCapturedCandidate),
            QStringLiteral("video thumbnail candidate conversion failed"));
    }
    return finish(failedExtraction(QStringLiteral("video thumbnail extraction timed out")));
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::cancel()
{
    if (m_finished) {
        return {};
    }
    m_finished = true;
    return { StopVideoThumbnailTimeout {}, StopVideoThumbnailBackend {} };
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::startFirstFrameFallback()
{
    m_firstFrameFallback = true;
    m_awaitingCandidatePosition = false;
    m_awaitingCandidateFrame = false;
    m_candidatePositions.clear();
    return { SeekVideoThumbnail { 0 }, PlayVideoThumbnailBackend {} };
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::seekToNextCandidate()
{
    if (m_candidateIndex >= m_candidatePositions.size()) {
        return finishCandidateExtraction();
    }
    m_awaitingCandidatePosition = true;
    m_awaitingCandidateFrame = false;
    return { PauseVideoThumbnailBackend {},
        SeekVideoThumbnail { m_candidatePositions.at(m_candidateIndex) },
        PlayVideoThumbnailBackend {} };
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::acceptCandidateFrame(QImage image)
{
    m_awaitingCandidateFrame = false;
    VideoThumbnailExtractionPlan plan { PauseVideoThumbnailBackend {} };
    if (!image.isNull() && videoThumbnailFrameIsInteresting(image)) {
        VideoThumbnailExtractionPlan completion = finishReadyFromImage(
            std::move(image), QStringLiteral("video thumbnail candidate conversion failed"));
        plan.insert(plan.end(), std::make_move_iterator(completion.begin()),
            std::make_move_iterator(completion.end()));
        return plan;
    }
    if (!image.isNull()) {
        m_lastCapturedCandidate = std::move(image);
    }
    VideoThumbnailExtractionPlan next = advanceToNextCandidate();
    plan.insert(
        plan.end(), std::make_move_iterator(next.begin()), std::make_move_iterator(next.end()));
    return plan;
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::advanceToNextCandidate()
{
    ++m_candidateIndex;
    return seekToNextCandidate();
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::finishCandidateExtraction()
{
    if (m_lastCapturedCandidate.isNull()) {
        return finish(failedExtraction(
            QStringLiteral("video thumbnail decode produced no usable candidate frame")));
    }
    return finishReadyFromImage(std::move(m_lastCapturedCandidate),
        QStringLiteral("video thumbnail candidate conversion failed"));
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::finishReadyFromImage(
    QImage image, QString fallbackErrorString)
{
    QString errorString;
    QImage thumbnail = videoThumbnailImageFromFrameImage(
        std::move(image), m_request.maximumLongEdge, &errorString);
    if (thumbnail.isNull()) {
        return finish(failedExtraction(
            errorString.isEmpty() ? std::move(fallbackErrorString) : std::move(errorString)));
    }
    return finish({ VideoThumbnailExtractionStatus::Ready, std::move(thumbnail), {} });
}

VideoThumbnailExtractionPlan VideoThumbnailExtractionWorkflow::finish(
    VideoThumbnailExtractionResult result)
{
    if (m_finished) {
        return {};
    }
    m_finished = true;
    return { StopVideoThumbnailTimeout {}, StopVideoThumbnailBackend {},
        CompleteVideoThumbnailExtraction { std::move(result) } };
}
}
