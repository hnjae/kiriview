// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageloader.h"

#include "async/imagecallback.h"
#include "decoding/decodedimageresult.h"
#include "decoding/imagedecodelogging.h"
#include "predecode/predecodelogging.h"
#include <QDebug>
#include <optional>
#include <utility>
#include <vector>

namespace {
kiriview::ImageLoadFailureSeverity imageLoadFailureSeverity(
    kiriview::DecodedImageFailureSeverity severity)
{
    switch (severity) {
    case kiriview::DecodedImageFailureSeverity::Error:
        return kiriview::ImageLoadFailureSeverity::Error;
    }

    return kiriview::ImageLoadFailureSeverity::Error;
}

kiriview::ImageLoadFailure imageLoadFailure(const kiriview::ImageLoadSession& session,
    kiriview::ImageLoadFailureKind kind, QString userMessage, QString diagnosticDetail)
{
    return kiriview::ImageLoadFailure {
        session.imageUrl(),
        session.id(),
        kind,
        kiriview::DecodedImageFailureRoute::Unknown,
        kiriview::DecodedImageFailureOperation::Unknown,
        std::move(userMessage),
        std::move(diagnosticDetail),
        kiriview::ImageLoadFailureSeverity::Error,
        false,
    };
}

kiriview::ImageLoadFailure imageLoadFailure(const kiriview::ImageLoadSession& session,
    kiriview::ImageLoadFailureKind kind, const QString& errorString)
{
    return imageLoadFailure(session, kind, errorString, errorString);
}

kiriview::ImageLoadFailure imageLoadFailure(
    const kiriview::ImageLoadSession& session, const kiriview::DecodedImageFailure& failure)
{
    return kiriview::ImageLoadFailure {
        session.imageUrl(),
        session.id(),
        kiriview::ImageLoadFailureKind::Decode,
        failure.route,
        failure.operation,
        failure.errorString,
        failure.diagnosticDetail,
        imageLoadFailureSeverity(failure.severity),
        failure.retryable,
    };
}
}

namespace kiriview {
ImageLoader::ImageLoader(QObject* parent)
    : ImageLoader(parent, ImageDocumentPageCandidateProvider {}, ImageDecodeDependencies {})
{
}

ImageLoader::ImageLoader(QObject* parent, Callbacks callbacks)
    : ImageLoader(parent, ImageDocumentPageCandidateProvider {}, ImageDecodeDependencies {},
          std::move(callbacks))
{
}

ImageLoader::ImageLoader(QObject* parent, ImageDocumentPageCandidateProvider candidateProvider,
    ImageDecodeDependencies decodeDependencies)
    : ImageLoader(parent, std::move(candidateProvider), std::move(decodeDependencies), {})
{
}

ImageLoader::ImageLoader(QObject* parent, ImageDocumentPageCandidateProvider candidateProvider,
    ImageDecodeDependencies decodeDependencies, Callbacks callbacks)
    : QObject(parent)
    , m_callbacks(std::move(callbacks))
    , m_decodeJob(this, std::move(decodeDependencies),
          ImageDecodeJob::Callbacks {
              [this](ImageDecodeRequest request, DecodedImageResult result) {
                  finishDecodeResult(std::move(request), std::move(result));
              },
              [this](const ImageDecodeRequest& request, const QString& errorString) {
                  finishImageLoadError(request, errorString);
              },
              [this](const ImageDecodeRequest& request, StaticDisplayImagePayload preview) {
                  finishThumbnailPreview(request, std::move(preview));
              },
          })
    , m_candidateRepository(std::move(candidateProvider))
{
}

void ImageLoader::finishDecodeResult(ImageDecodeRequest request, DecodedImageResult result)
{
    if (const DecodedImageFailure* failure = result.failure()) {
        finishDecodeRequestWithFailure(request, *failure);
        return;
    }

    std::optional<DecodedImage> image = std::move(result).takeImage();
    if (!image.has_value()) {
        return;
    }

    std::optional<ImageLoadSession> session
        = m_sessionTracker.claimCurrentForDecodeRequest(request);
    if (!session.has_value()) {
        return;
    }

    finishDecodedImage(std::move(*session), std::move(*image));
}

void ImageLoader::finishImageLoadError(
    const ImageDecodeRequest& request, const QString& errorString)
{
    finishDecodeRequestWithError(request, ImageLoadFailureKind::DataLoad, errorString);
}

void ImageLoader::finishThumbnailPreview(
    const ImageDecodeRequest& request, StaticDisplayImagePayload preview)
{
    std::optional<ImageLoadSession> session = m_sessionTracker.currentForDecodeRequest(request);
    if (!session.has_value()) {
        return;
    }

    finishThumbnailPreview(std::move(*session), std::move(preview));
}

void ImageLoader::start(ImageLoadRequest request,
    ImageFirstDisplayDecodeContext firstDisplayContext,
    ImageDocumentPageCandidateListSnapshot candidateSnapshot)
{
    cancel();

    ImageLoadPlan plan = m_sessionTracker.start(std::move(request), firstDisplayContext);
    const ImageLoadSession session = std::move(plan.session);
    switch (plan.startEffect) {
    case ImageLoadStartEffect::DecodeImage:
        break;
    case ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates:
        startOpenedCollectionLoad(session, candidateSnapshot);
        return;
    }

    if (tryReportUnsupportedOpenedCollectionVideo(session)) {
        return;
    }

    if (tryDisplayPredecodedImage(session)) {
        return;
    }

    startImageLoad(session);
}

void ImageLoader::startImageLoad(ImageLoadSession session)
{
    if (!m_sessionTracker.isCurrent(session)) {
        return;
    }

    m_decodeJob.start(session.decodeRequest());
}

void ImageLoader::startOpenedCollectionLoad(
    ImageLoadSession session, const ImageDocumentPageCandidateListSnapshot& candidateSnapshot)
{
    const ImageDocumentPageCandidateListSource candidateSource
        = ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
            session.openedCollectionScope());
    if (imageDocumentPageCandidateListSnapshotMatchesSource(candidateSnapshot, candidateSource)) {
        const ImageDocumentPageCandidateRows& candidates
            = imageDocumentPageCandidateRows(candidateSnapshot);
        qCDebug(kiriviewPredecodeLog)
            << "opened collection candidate snapshot reused for foreground load"
            << "sessionId" << session.id() << "imageUrl" << session.imageUrl()
            << "openedCollectionRoot" << session.openedCollectionScope().rootUrl()
            << "candidateCount" << static_cast<qsizetype>(candidates.size());
        finishOpenedCollectionCandidates(session, candidates);
        return;
    }

    qCDebug(kiriviewPredecodeLog) << "opened collection candidates listed for foreground load"
                                  << "sessionId" << session.id() << "imageUrl" << session.imageUrl()
                                  << "openedCollectionRoot"
                                  << session.openedCollectionScope().rootUrl() << "snapshotKnown"
                                  << candidateSnapshot.known;
    m_openedCollectionCandidateLoadJob = m_candidateRepository.loadImages(
        this, candidateSource,
        [this, session](std::vector<ImageDocumentPageCandidate> candidates) mutable {
            finishOpenedCollectionCandidates(session, candidates);
        },
        [this, session](const QString& errorString) {
            std::optional<ImageLoadSession> currentSession = m_sessionTracker.claimCurrent(session);
            if (!currentSession.has_value()) {
                return;
            }

            invokeIfSet(m_callbacks.error, *currentSession,
                imageLoadFailure(
                    *currentSession, ImageLoadFailureKind::OpenedCollectionLoad, errorString));
        });
}

void ImageLoader::finishOpenedCollectionCandidates(
    const ImageLoadSession& session, const std::vector<ImageDocumentPageCandidate>& candidates)
{
    OpenedCollectionCandidateCompletion completion
        = m_sessionTracker.completeOpenedCollectionCandidates(session, candidates);
    switch (completion.action) {
    case OpenedCollectionCandidateCompletionAction::Ignored:
        return;
    case OpenedCollectionCandidateCompletionAction::ReportEmptyOpenedCollection:
        invokeIfSet(m_callbacks.error, completion.session,
            imageLoadFailure(completion.session, ImageLoadFailureKind::EmptyOpenedCollection,
                QString(), QString()));
        return;
    case OpenedCollectionCandidateCompletionAction::ReportUnsupportedOpenedCollectionVideo:
        invokeIfSet(m_callbacks.sourcePrepared, completion.session);
        invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideo, std::move(completion.session));
        return;
    case OpenedCollectionCandidateCompletionAction::StartImageDecode:
        break;
    }

    invokeIfSet(m_callbacks.sourcePrepared, completion.session);
    startImageLoad(std::move(completion.session));
}

void ImageLoader::cancel()
{
    m_sessionTracker.cancel();
    m_decodeJob.cancel();
    m_openedCollectionCandidateLoadJob.cancel();
}

bool ImageLoader::tryReportUnsupportedOpenedCollectionVideo(ImageLoadSession session)
{
    if (session.openedCollectionScope().isEmpty()) {
        return false;
    }

    if (session.kind() != ImageDocumentPageKind::Video) {
        return false;
    }

    std::optional<ImageLoadSession> currentSession = m_sessionTracker.claimCurrent(session);
    if (!currentSession.has_value()) {
        return false;
    }

    invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideo, std::move(*currentSession));
    return true;
}

bool ImageLoader::tryDisplayPredecodedImage(ImageLoadSession session)
{
    if (!m_callbacks.findPredecodedImage) {
        qCDebug(kiriviewPredecodeLog)
            << "foreground predecode lookup skipped without lookup port"
            << "sessionId" << session.id() << "imageUrl" << session.imageUrl();
        return false;
    }

    std::optional<PredecodedImage> predecoded = m_callbacks.findPredecodedImage(session.imageUrl());
    if (!predecoded.has_value()) {
        qCDebug(kiriviewPredecodeLog)
            << "foreground predecode lookup miss"
            << "sessionId" << session.id() << "imageUrl" << session.imageUrl();
        return false;
    }
    if (predecoded->location != session.location()) {
        qCDebug(kiriviewPredecodeLog)
            << "foreground predecode lookup rejected for location mismatch"
            << "sessionId" << session.id() << "imageUrl" << session.imageUrl() << "predecodedUrl"
            << predecoded->location.imageUrl();
        return false;
    }

    std::optional<ImageLoadSession> predecodedSession
        = m_sessionTracker.claimPredecodedImage(session, predecoded->location);
    if (!predecodedSession.has_value()) {
        qCDebug(kiriviewPredecodeLog)
            << "foreground predecode lookup rejected for stale session"
            << "sessionId" << session.id() << "imageUrl" << session.imageUrl();
        return false;
    }

    qCDebug(kiriviewPredecodeLog) << "foreground predecode lookup hit"
                                  << "sessionId" << predecodedSession->id() << "imageUrl"
                                  << predecodedSession->imageUrl() << "sourceIdentity"
                                  << predecoded->displayImage.sourceIdentity << "originalSize"
                                  << predecoded->displayImage.originalSize << "rasterSize"
                                  << predecoded->displayImage.image.size() << "quality"
                                  << static_cast<int>(predecoded->displayImage.quality)
                                  << "previewOrigin"
                                  << static_cast<int>(predecoded->displayImage.previewOrigin);
    finishPredecodedImage(std::move(*predecodedSession), std::move(*predecoded));
    return true;
}

void ImageLoader::finishDecodeRequestWithFailure(
    const ImageDecodeRequest& request, const DecodedImageFailure& failure)
{
    std::optional<ImageLoadSession> session
        = m_sessionTracker.claimCurrentForDecodeRequest(request);
    if (!session.has_value()) {
        return;
    }

    qCWarning(kiriviewDecodeLog).noquote()
        << "image decode failed"
        << "sourceUrl" << session->imageUrl() << "sessionId" << session->id() << "route"
        << static_cast<int>(failure.route) << "operation" << static_cast<int>(failure.operation)
        << "detail" << failure.diagnosticDetail << "retryable" << failure.retryable;
    invokeIfSet(m_callbacks.error, *session, imageLoadFailure(*session, failure));
}

void ImageLoader::finishDecodeRequestWithError(
    const ImageDecodeRequest& request, ImageLoadFailureKind kind, const QString& errorString)
{
    std::optional<ImageLoadSession> session
        = m_sessionTracker.claimCurrentForDecodeRequest(request);
    if (!session.has_value()) {
        return;
    }

    invokeIfSet(m_callbacks.error, *session, imageLoadFailure(*session, kind, errorString));
}

void ImageLoader::finishDecodedImage(ImageLoadSession session, DecodedImage image)
{
    invokeIfSet(m_callbacks.decodedImage, std::move(session), std::move(image));
}

void ImageLoader::finishPredecodedImage(ImageLoadSession session, PredecodedImage image)
{
    invokeIfSet(m_callbacks.predecodedImage, std::move(session), std::move(image));
}

void ImageLoader::finishThumbnailPreview(
    ImageLoadSession session, StaticDisplayImagePayload preview)
{
    invokeIfSet(m_callbacks.thumbnailPreview, std::move(session), std::move(preview));
}
}
