// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageloader.h"

#include "async/imagecallback.h"
#include "predecode/predecodelogging.h"

#include <QDebug>
#include <utility>

namespace {
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
}

namespace kiriview {
ImageLoader::ImageLoader()
    : ImageLoader(Callbacks {})
{
}

ImageLoader::ImageLoader(Callbacks callbacks)
    : m_callbacks(std::move(callbacks))
{
}

void ImageLoader::start(
    ImageLoadRequest request, ImageFirstDisplayDecodeContext firstDisplayContext)
{
    cancel();

    ImageLoadPlan plan = m_sessionTracker.start(std::move(request), firstDisplayContext);
    ImageLoadSession session = std::move(plan.session);
    if (plan.startEffect != ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates
        && tryReportUnsupportedOpenedCollectionVideo(session)) {
        return;
    }
    if (!startProviderTarget(session) || !m_sessionTracker.isCurrent(session)) {
        return;
    }
    if (plan.startEffect == ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates) {
        startOpenedCollectionLoad(session);
        return;
    }

    resolveProviderImage(std::move(session));
}

void ImageLoader::cancel() { m_sessionTracker.cancel(); }

bool ImageLoader::isCurrentSession(const ImageLoadSession& session) const
{
    return m_sessionTracker.isCurrent(session);
}

std::optional<ImageLoadSession> ImageLoader::claimCurrentSession(const ImageLoadSession& session)
{
    return m_sessionTracker.claimCurrent(session);
}

void ImageLoader::startOpenedCollectionLoad(const ImageLoadSession& session)
{
    ImageDocumentPageCandidateListSource candidateSource
        = ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
            session.openedCollectionScope());
    if (!m_callbacks.ensurePageCandidateSnapshot) {
        qCWarning(kiriviewPredecodeLog)
            << "opened collection foreground load rejected without candidate snapshot owner"
            << "sessionId" << session.id() << "imageUrl" << session.imageUrl();
        std::optional<ImageLoadSession> currentSession = m_sessionTracker.claimCurrent(session);
        if (!currentSession.has_value()) {
            return;
        }
        invokeIfSet(m_callbacks.error, *currentSession,
            imageLoadFailure(*currentSession, ImageLoadFailureKind::OpenedCollectionLoad, {},
                QStringLiteral("page candidate snapshot owner is unavailable")));
        return;
    }

    const auto candidateContext
        = ImageDocumentPageCandidateListContext::forSource(session.imageUrl(), candidateSource);
    m_callbacks.ensurePageCandidateSnapshot(candidateContext,
        [this, session, candidateSource = std::move(candidateSource)](
            const ImageDocumentPageCandidateListSnapshotResult& result) mutable {
            finishOpenedCollectionSnapshot(session, candidateSource, result);
        });
}

void ImageLoader::finishOpenedCollectionSnapshot(const ImageLoadSession& session,
    const ImageDocumentPageCandidateListSource& candidateSource,
    const ImageDocumentPageCandidateListSnapshotResult& result)
{
    if (!m_sessionTracker.isCurrent(session)) {
        return;
    }

    if (!result.succeeded) {
        std::optional<ImageLoadSession> currentSession = m_sessionTracker.claimCurrent(session);
        if (currentSession.has_value()) {
            invokeIfSet(m_callbacks.error, *currentSession,
                imageLoadFailure(*currentSession, ImageLoadFailureKind::OpenedCollectionLoad,
                    result.errorString));
        }
        return;
    }

    if (!imageDocumentPageCandidateListSnapshotMatchesSource(result.snapshot, candidateSource)) {
        std::optional<ImageLoadSession> currentSession = m_sessionTracker.claimCurrent(session);
        if (currentSession.has_value()) {
            invokeIfSet(m_callbacks.error, *currentSession,
                imageLoadFailure(*currentSession, ImageLoadFailureKind::OpenedCollectionLoad, {},
                    QStringLiteral(
                        "page candidate snapshot owner returned an unconfirmed or mismatched "
                        "snapshot")));
        }
        return;
    }

    finishOpenedCollectionCandidates(session, imageDocumentPageCandidateRows(result.snapshot));
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
        if (!m_sessionTracker.isCurrent(completion.session)) {
            return;
        }
        invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideo, std::move(completion.session));
        return;
    case OpenedCollectionCandidateCompletionAction::StartImageDecode:
        break;
    }

    invokeIfSet(m_callbacks.sourcePrepared, completion.session);
    resolveProviderImage(std::move(completion.session));
}

bool ImageLoader::tryReportUnsupportedOpenedCollectionVideo(const ImageLoadSession& session)
{
    if (session.openedCollectionScope().isEmpty()
        || session.kind() != ImageDocumentPageKind::Video) {
        return false;
    }

    if (!m_sessionTracker.isCurrent(session)) {
        return false;
    }

    invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideo, session);
    return true;
}

bool ImageLoader::startProviderTarget(const ImageLoadSession& session)
{
    if (!m_sessionTracker.isCurrent(session)) {
        return false;
    }
    if (!m_callbacks.targetStarted) {
        std::optional<ImageLoadSession> currentSession = m_sessionTracker.claimCurrent(session);
        if (currentSession.has_value()) {
            invokeIfSet(m_callbacks.error, *currentSession,
                imageLoadFailure(*currentSession, ImageLoadFailureKind::Presentation, {},
                    QStringLiteral("viewport provider target owner is unavailable")));
        }
        return false;
    }

    invokeIfSet(m_callbacks.targetStarted, session);
    return m_sessionTracker.isCurrent(session);
}

void ImageLoader::resolveProviderImage(ImageLoadSession session)
{
    if (!m_sessionTracker.isCurrent(session)) {
        return;
    }
    if (!m_callbacks.resolvedImage) {
        std::optional<ImageLoadSession> currentSession = m_sessionTracker.claimCurrent(session);
        if (currentSession.has_value()) {
            invokeIfSet(m_callbacks.error, *currentSession,
                imageLoadFailure(*currentSession, ImageLoadFailureKind::Presentation, {},
                    QStringLiteral("viewport provider target resolver is unavailable")));
        }
        return;
    }
    std::optional<PredecodedImage> predecoded = matchingPredecodedImage(session);
    if (!m_sessionTracker.isCurrent(session)) {
        return;
    }
    invokeIfSet(m_callbacks.resolvedImage, std::move(session), std::move(predecoded));
}

std::optional<PredecodedImage> ImageLoader::matchingPredecodedImage(
    const ImageLoadSession& session) const
{
    if (!m_callbacks.findPredecodedImage) {
        return std::nullopt;
    }

    std::optional<PredecodedImage> predecoded = m_callbacks.findPredecodedImage(session.imageUrl());
    if (!predecoded.has_value() || predecoded->location != session.location()) {
        return std::nullopt;
    }
    return predecoded;
}
}
