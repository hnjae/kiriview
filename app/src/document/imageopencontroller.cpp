// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageopencontroller.h"

#include "async/imagecallback.h"
#include "imagedocumentstate.h"
#include "imageloader.h"
#include "imageopenapplicationplanapplier.h"
#include "imageopenworkflow.h"
#include "localization/imageerrortext.h"

#include <KLocalizedString>
#include <QtGlobal>
#include <limits>
#include <utility>

namespace {
QString emptyOpenedCollectionErrorMessage()
{
    return kiriview::imageErrorText(kiriview::ImageErrorTextId::EmptyOpenedCollection);
}

QString openedCollectionOpenErrorMessage(const QString& errorString)
{
    return errorString.isEmpty()
        ? kiriview::imageErrorText(kiriview::ImageErrorTextId::OpenOpenedCollection)
        : errorString;
}

QString loadFailureUserMessage(const kiriview::ImageLoadFailure& failure)
{
    return failure.kind == kiriview::ImageLoadFailureKind::EmptyOpenedCollection
        ? emptyOpenedCollectionErrorMessage()
        : failure.userMessage;
}

QString unsupportedOpenedCollectionVideoMessage()
{
    return i18nc("@info:status", "KiriView can’t play this video from the selected collection.");
}

kiriview::ImageLoadFailure imagePresentationFailure(
    const kiriview::ImageLoadSession& session, const QString& message)
{
    return {
        session.imageUrl(),
        session.id(),
        kiriview::ImageLoadFailureKind::Presentation,
        kiriview::DecodedImageFailureRoute::Unknown,
        kiriview::DecodedImageFailureOperation::Unknown,
        message,
        message,
        kiriview::ImageLoadFailureSeverity::Error,
        false,
    };
}
}

namespace kiriview {
ImageOpenController::ImageOpenController(
    ImageDocumentState& state, ImageOpenController::Callbacks callbacks)
    : m_state(state)
    , m_callbacks(std::move(callbacks))
{
    if (!m_callbacks.findPredecodedImage || !m_callbacks.runtimePlan
        || !m_callbacks.openedCollectionVideoPlaybackAvailable
        || !m_callbacks.commitViewportPresentation
        || !m_callbacks.invalidatePendingViewportImageLoad
        || !m_callbacks.ensurePageCandidateSnapshot || !m_callbacks.startViewportImageTarget
        || !m_callbacks.resolveViewportImageTarget || !m_callbacks.firstDisplayDecodeContext
        || !m_callbacks.hasAuthoritativeDisplay) {
        qFatal("Image-open controller requires all workflow callbacks");
    }

    ImageLoader::EnsurePageCandidateSnapshotCallback ensurePageCandidateSnapshot
        = m_callbacks.ensurePageCandidateSnapshot;
    ImageLoader::Callbacks loaderCallbacks;
    loaderCallbacks.error = [this](const ImageLoadSession& session, ImageLoadFailure failure) {
        [[maybe_unused]] auto batch = m_state.beginChangeBatch();
        finishLoadWithError(session, std::move(failure));
    };
    loaderCallbacks.unsupportedOpenedCollectionVideo = [this](const ImageLoadSession& session) {
        [[maybe_unused]] auto batch = m_state.beginChangeBatch();
        finishUnsupportedOpenedCollectionVideoLoad(session);
    };
    loaderCallbacks.findPredecodedImage = [this](const DisplayedImageLocation& location) {
        return m_callbacks.findPredecodedImage(location);
    };
    loaderCallbacks.sourcePrepared = [this](const ImageLoadSession& session) {
        [[maybe_unused]] auto batch = m_state.beginChangeBatch();
        finishSourcePrepared(session);
    };
    loaderCallbacks.ensurePageCandidateSnapshot = std::move(ensurePageCandidateSnapshot);
    loaderCallbacks.targetStarted = [this](const ImageLoadSession& session) {
        [[maybe_unused]] auto batch = m_state.beginChangeBatch();
        finishStartedViewportImageLoad(session);
    };
    loaderCallbacks.resolvedImage
        = [this](const ImageLoadSession& session, std::optional<PredecodedImage> predecoded) {
              [[maybe_unused]] auto batch = m_state.beginChangeBatch();
              finishResolvedViewportImageLoad(session, std::move(predecoded));
          };
    m_imageLoader = std::make_unique<ImageLoader>(std::move(loaderCallbacks));
}

ImageOpenController::~ImageOpenController() { m_imageLoader->cancel(); }

void ImageOpenController::open()
{
    const quint64 revision = beginOperation();
    cancelActiveLoad();
    if (!operationIsCurrent(revision)) {
        return;
    }
    if (m_state.sourceUrl().isEmpty()) {
        finishEmptySourceLoad();
        return;
    }
    if (!m_sourceLoadRequest.has_value()) {
        return;
    }

    ImageLoadRequest request = std::move(*m_sourceLoadRequest);
    m_sourceLoadRequest.reset();
    if (!beginSourceLoad(request.sameScopePageNavigation(), revision)) {
        return;
    }
    const ImageFirstDisplayDecodeContext firstDisplayContext
        = m_callbacks.firstDisplayDecodeContext();
    if (!operationIsCurrent(revision)) {
        return;
    }
    m_imageLoader->start(std::move(request), firstDisplayContext);
}

void ImageOpenController::prepareSourceLoad(const ImageDocumentSourceLoadRequest& request)
{
    m_sourceLoadRequest = request;
}

void ImageOpenController::cancel()
{
    static_cast<void>(beginOperation());
    cancelActiveLoad();
}

quint64 ImageOpenController::beginOperation()
{
    if (m_operationRevision == std::numeric_limits<quint64>::max()) {
        qFatal("Image-open operation revision exhausted");
    }
    return ++m_operationRevision;
}

bool ImageOpenController::operationIsCurrent(quint64 revision) const
{
    return revision == m_operationRevision;
}

void ImageOpenController::cancelActiveLoad()
{
    m_imageLoader->cancel();
    m_callbacks.invalidatePendingViewportImageLoad();
}

bool ImageOpenController::applyAndReportIfCurrent(ImageOpenApplicationPlan plan, quint64 revision)
{
    const ImageDocumentRuntimePlan runtimePlan
        = applyImageOpenApplicationPlan(m_state, std::move(plan));
    if (!operationIsCurrent(revision)) {
        return false;
    }
    reportRuntimePlan(runtimePlan);
    return operationIsCurrent(revision);
}

void ImageOpenController::finishViewportImageLoadReady(
    const ImageLoadSession& session, QSize imageSize, EmbeddedMetadata metadata)
{
    const quint64 revision = m_operationRevision;
    std::optional<ImageLoadSession> currentSession = m_imageLoader->claimCurrentSession(session);
    if (!currentSession.has_value()) {
        return;
    }

    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    if (!m_callbacks.commitViewportPresentation(*currentSession, imageSize)) {
        finishLoadWithError(*currentSession,
            imagePresentationFailure(
                *currentSession, imageErrorText(ImageErrorTextId::DecodeImageAnimation)));
        return;
    }
    if (!operationIsCurrent(revision)) {
        return;
    }
    finishSuccessfulImageLoad(*currentSession, std::move(metadata));
}

void ImageOpenController::finishViewportImageLoadWithError(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    const quint64 revision = m_operationRevision;
    std::optional<ImageLoadSession> currentSession = m_imageLoader->claimCurrentSession(session);
    if (!currentSession.has_value()) {
        return;
    }

    failure.sourceUrl = currentSession->imageUrl();
    failure.sessionId = currentSession->id();
    if (!operationIsCurrent(revision)) {
        return;
    }
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    finishLoadWithError(*currentSession, std::move(failure));
}

void ImageOpenController::finishEmptySourceLoad()
{
    const quint64 revision = m_operationRevision;
    static_cast<void>(
        applyAndReportIfCurrent(ImageOpenWorkflow::finishEmptySourceLoadPlan(), revision));
}

bool ImageOpenController::beginSourceLoad(bool sameScopePageNavigation, quint64 revision)
{
    const bool hasAuthoritativeDisplay = m_callbacks.hasAuthoritativeDisplay();
    if (!operationIsCurrent(revision)) {
        return false;
    }
    return applyAndReportIfCurrent(
        ImageOpenWorkflow::beginSourceLoadPlan(ImageOpenBeginSourceLoadSnapshot {
            hasAuthoritativeDisplay,
            !m_state.loadingContainerNavigationUrl().isEmpty(),
            sameScopePageNavigation,
        }),
        revision);
}

void ImageOpenController::finishContainerNavigationWithEmptyContainer(const QUrl& containerUrl)
{
    finishContainerNavigationLoadWithError(containerUrl, emptyOpenedCollectionErrorMessage());
}

void ImageOpenController::finishContainerNavigationLoadWithError(
    const QUrl& containerUrl, const QString& errorString)
{
    const quint64 revision = beginOperation();
    cancelActiveLoad();
    if (!operationIsCurrent(revision)) {
        return;
    }
    ImageDocumentSelectedTarget selectedTarget = m_state.selectedTarget();
    selectedTarget.url = containerUrl;
    static_cast<void>(applyAndReportIfCurrent(
        ImageOpenWorkflow::finishContainerNavigationLoadWithErrorPlan(
            std::move(selectedTarget), openedCollectionOpenErrorMessage(errorString)),
        revision));
}

void ImageOpenController::finishSourcePrepared(const ImageLoadSession& session)
{
    const quint64 revision = m_operationRevision;
    static_cast<void>(
        applyAndReportIfCurrent(ImageOpenWorkflow::resolveSourceImagePlan(session), revision));
}

void ImageOpenController::finishUnsupportedOpenedCollectionVideoLoad(
    const ImageLoadSession& session)
{
    const quint64 revision = m_operationRevision;
    if (!m_imageLoader->isCurrentSession(session)) {
        return;
    }

    const bool playbackAvailable = m_callbacks.openedCollectionVideoPlaybackAvailable(
        session.openedCollectionScope(), session.imageUrl());
    std::optional<ImageLoadSession> currentSession = m_imageLoader->claimCurrentSession(session);
    if (!currentSession.has_value()) {
        return;
    }

    if (playbackAvailable) {
        finishPlayableOpenedCollectionVideoLoad(*currentSession);
        return;
    }

    const QString message = unsupportedOpenedCollectionVideoMessage();
    if (!applyAndReportIfCurrent(
            ImageOpenWorkflow::finishUnsupportedOpenedCollectionVideoLoadPlan(*currentSession),
            revision)) {
        return;
    }
    invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideoEntered, message);
}

void ImageOpenController::finishPlayableOpenedCollectionVideoLoad(const ImageLoadSession& session)
{
    const quint64 revision = m_operationRevision;
    static_cast<void>(applyAndReportIfCurrent(
        ImageOpenWorkflow::finishPlayableOpenedCollectionVideoLoadPlan(session), revision));
}

void ImageOpenController::finishStartedViewportImageLoad(const ImageLoadSession& session)
{
    if (m_callbacks.startViewportImageTarget(session)) {
        return;
    }
    finishViewportImageLoadWithError(session,
        imagePresentationFailure(session, imageErrorText(ImageErrorTextId::DecodeImageAnimation)));
}

void ImageOpenController::finishResolvedViewportImageLoad(
    const ImageLoadSession& session, std::optional<PredecodedImage> predecoded)
{
    if (m_callbacks.resolveViewportImageTarget(session, std::move(predecoded))) {
        return;
    }
    finishViewportImageLoadWithError(session,
        imagePresentationFailure(session, imageErrorText(ImageErrorTextId::DecodeImageAnimation)));
}

void ImageOpenController::finishLoadWithError(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    const quint64 revision = m_operationRevision;
    failure.userMessage = loadFailureUserMessage(failure);
    static_cast<void>(applyAndReportIfCurrent(
        ImageOpenWorkflow::finishLoadWithErrorPlan(session, failure), revision));
}

void ImageOpenController::finishSuccessfulImageLoad(
    const ImageLoadSession& session, EmbeddedMetadata metadata)
{
    const quint64 revision = m_operationRevision;
    static_cast<void>(applyAndReportIfCurrent(ImageOpenWorkflow::finishSuccessfulImageLoadPlan(
                                                  ImageOpenSuccessfulImageLoadSnapshot {
                                                      session.hasContainerNavigationTarget(),
                                                  },
                                                  session, std::move(metadata)),
        revision));
}

void ImageOpenController::reportRuntimePlan(const ImageDocumentRuntimePlan& plan)
{
    m_callbacks.runtimePlan(plan);
}
}
