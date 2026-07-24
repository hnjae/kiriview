// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEOTHUMBNAILEXTRACTIONWORKFLOW_H
#define KIRIVIEW_VIDEOTHUMBNAILEXTRACTIONWORKFLOW_H

#include "thumbnail/videothumbnailbackend.h"

#include <QImage>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QtGlobal>
#include <variant>
#include <vector>

namespace kiriview {
enum class VideoThumbnailExtractionStatus {
    Ready,
    Failed,
};

struct VideoThumbnailExtractionRequest
{
    QUrl sourceUrl;
    int maximumLongEdge = 0;
};

struct VideoThumbnailExtractionResult
{
    VideoThumbnailExtractionStatus status = VideoThumbnailExtractionStatus::Failed;
    QImage image;
    QString errorString;
};

struct StartVideoThumbnailTimeout
{
    int intervalMsec = 0;
};
struct StopVideoThumbnailTimeout
{
};
struct SetVideoThumbnailSource
{
    QUrl sourceUrl;
};
struct SeekVideoThumbnail
{
    qint64 positionMsec = 0;
};
struct PlayVideoThumbnailBackend
{
};
struct PauseVideoThumbnailBackend
{
};
struct StopVideoThumbnailBackend
{
};
struct CompleteVideoThumbnailExtraction
{
    VideoThumbnailExtractionResult result;
};

using VideoThumbnailExtractionOperation
    = std::variant<StartVideoThumbnailTimeout, StopVideoThumbnailTimeout, SetVideoThumbnailSource,
        SeekVideoThumbnail, PlayVideoThumbnailBackend, PauseVideoThumbnailBackend,
        StopVideoThumbnailBackend, CompleteVideoThumbnailExtraction>;
using VideoThumbnailExtractionPlan = std::vector<VideoThumbnailExtractionOperation>;

QImage videoThumbnailImageFromFrameImage(
    const QImage& image, int maximumLongEdge, QString* errorString = nullptr);
QImage videoThumbnailImageFromEmbeddedImages(
    VideoThumbnailEmbeddedImages images, int maximumLongEdge, QString* errorString = nullptr);
QVector<qint64> videoThumbnailCandidatePositions(qint64 durationMsec);
bool videoThumbnailFrameIsInteresting(const QImage& image);

class VideoThumbnailExtractionWorkflow final
{
public:
    VideoThumbnailExtractionPlan start(VideoThumbnailExtractionRequest request);
    VideoThumbnailExtractionPlan handleMediaFacts(VideoThumbnailBackendMediaFacts facts);
    VideoThumbnailExtractionPlan handlePositionChanged(qint64 positionMsec);
    VideoThumbnailExtractionPlan handleFrame(QImage image);
    VideoThumbnailExtractionPlan handleMetadata(VideoThumbnailEmbeddedImages images);
    VideoThumbnailExtractionPlan handleBackendError(QString errorString);
    VideoThumbnailExtractionPlan handleTimeout();
    VideoThumbnailExtractionPlan cancel();

private:
    VideoThumbnailExtractionPlan startFirstFrameFallback();
    VideoThumbnailExtractionPlan seekToNextCandidate();
    VideoThumbnailExtractionPlan acceptCandidateFrame(QImage image);
    VideoThumbnailExtractionPlan advanceToNextCandidate();
    VideoThumbnailExtractionPlan finishCandidateExtraction();
    VideoThumbnailExtractionPlan finishReadyFromImage(
        const QImage& image, QString fallbackErrorString);
    VideoThumbnailExtractionPlan finish(VideoThumbnailExtractionResult result);

    VideoThumbnailExtractionRequest m_request;
    QVector<qint64> m_candidatePositions;
    qsizetype m_candidateIndex = 0;
    QImage m_lastCapturedCandidate;
    bool m_started = false;
    bool m_finished = false;
    bool m_frameExtractionStarted = false;
    bool m_awaitingCandidatePosition = false;
    bool m_awaitingCandidateFrame = false;
    bool m_firstFrameFallback = false;
};
}

#endif
