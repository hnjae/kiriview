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
        || !m_callbacks.openedCollectionVideoPlaybackAvailable || !m_callbacks.commitPrimaryPageSlot
        || !m_callbacks.invalidatePendingViewportImageLoad
        || !m_callbacks.ensurePageCandidateSnapshot || !m_callbacks.prepareViewportImageTarget
        || !m_callbacks.firstDisplayDecodeContext || !m_callbacks.hasCommittedImage) {
        qFatal("Image-open controller requires all workflow callbacks");
    }

    ImageLoader::EnsurePageCandidateSnapshotCallback ensurePageCandidateSnapshot
        = m_callbacks.ensurePageCandidateSnapshot;
    m_imageLoader = std::make_unique<ImageLoader>(ImageLoader::Callbacks {
        [this](const ImageLoadSession& session, ImageLoadFailure failure) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishLoadWithError(session, std::move(failure));
        },
        [this](const ImageLoadSession& session) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishUnsupportedOpenedCollectionVideoLoad(session);
        },
        [this](const QUrl& url) { return m_callbacks.findPredecodedImage(url); },
        [this](const ImageLoadSession& session) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishSourcePrepared(session);
        },
        std::move(ensurePageCandidateSnapshot),
        [this](const ImageLoadSession& session, std::optional<PredecodedImage> predecoded) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishPreparedViewportImageLoad(session, std::move(predecoded));
        },
    });
}

ImageOpenController::~ImageOpenController() { m_imageLoader->cancel(); }

void ImageOpenController::open()
{
    cancel();
    if (m_state.sourceUrl().isEmpty()) {
        finishEmptySourceLoad();
        return;
    }
    if (!m_sourceLoadRequest.has_value()) {
        return;
    }

    ImageLoadRequest request = std::move(*m_sourceLoadRequest);
    m_sourceLoadRequest.reset();
    beginSourceLoad(request.sameScopePageNavigation());
    const ImageFirstDisplayDecodeContext firstDisplayContext
        = m_callbacks.firstDisplayDecodeContext();
    m_imageLoader->start(std::move(request), firstDisplayContext);
}

void ImageOpenController::prepareSourceLoad(const ImageDocumentSourceLoadRequest& request)
{
    m_sourceLoadRequest = request;
}

void ImageOpenController::cancel()
{
    m_imageLoader->cancel();
    m_callbacks.invalidatePendingViewportImageLoad();
}

void ImageOpenController::finishViewportImageLoadReady(
    const ImageLoadSession& session, QSize imageSize, EmbeddedMetadata metadata)
{
    std::optional<ImageLoadSession> currentSession = m_imageLoader->claimCurrentSession(session);
    if (!currentSession.has_value()) {
        return;
    }

    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    m_callbacks.commitPrimaryPageSlot(currentSession->location(), imageSize);
    finishSuccessfulImageLoad(*currentSession, std::move(metadata));
}

void ImageOpenController::finishViewportImageLoadWithError(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    std::optional<ImageLoadSession> currentSession = m_imageLoader->claimCurrentSession(session);
    if (!currentSession.has_value()) {
        return;
    }

    failure.sourceUrl = currentSession->imageUrl();
    failure.sessionId = currentSession->id();
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    finishLoadWithError(*currentSession, std::move(failure));
}

void ImageOpenController::finishEmptySourceLoad()
{
    reportRuntimePlan(
        applyImageOpenApplicationPlan(m_state, ImageOpenWorkflow::finishEmptySourceLoadPlan()));
}

void ImageOpenController::beginSourceLoad(bool sameScopePageNavigation)
{
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::beginSourceLoadPlan(ImageOpenBeginSourceLoadSnapshot {
            m_callbacks.hasCommittedImage(),
            !m_state.loadingContainerNavigationUrl().isEmpty(),
            sameScopePageNavigation,
        })));
}

void ImageOpenController::finishContainerNavigationWithEmptyContainer(const QUrl& containerUrl)
{
    finishContainerNavigationLoadWithError(containerUrl, emptyOpenedCollectionErrorMessage());
}

void ImageOpenController::finishContainerNavigationLoadWithError(
    const QUrl& containerUrl, const QString& errorString)
{
    cancel();
    ImageDocumentSelectedTarget selectedTarget = m_state.selectedTarget();
    selectedTarget.url = containerUrl;
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::finishContainerNavigationLoadWithErrorPlan(
            std::move(selectedTarget), openedCollectionOpenErrorMessage(errorString))));
}

void ImageOpenController::finishSourcePrepared(const ImageLoadSession& session)
{
    reportRuntimePlan(
        applyImageOpenApplicationPlan(m_state, ImageOpenWorkflow::resolveSourceImagePlan(session)));
}

void ImageOpenController::finishUnsupportedOpenedCollectionVideoLoad(
    const ImageLoadSession& session)
{
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
    const ImageDocumentRuntimePlan plan = applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::finishUnsupportedOpenedCollectionVideoLoadPlan(*currentSession));
    reportRuntimePlan(plan);
    invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideoEntered, message);
}

void ImageOpenController::finishPlayableOpenedCollectionVideoLoad(const ImageLoadSession& session)
{
    reportRuntimePlan(applyImageOpenApplicationPlan(
        m_state, ImageOpenWorkflow::finishPlayableOpenedCollectionVideoLoadPlan(session)));
}

void ImageOpenController::finishPreparedViewportImageLoad(
    const ImageLoadSession& session, std::optional<PredecodedImage> predecoded)
{
    if (m_callbacks.prepareViewportImageTarget(session, std::move(predecoded))) {
        return;
    }
    finishViewportImageLoadWithError(session,
        imagePresentationFailure(session, imageErrorText(ImageErrorTextId::DecodeImageAnimation)));
}

void ImageOpenController::finishLoadWithError(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    failure.userMessage = loadFailureUserMessage(failure);
    reportRuntimePlan(applyImageOpenApplicationPlan(
        m_state, ImageOpenWorkflow::finishLoadWithErrorPlan(session, failure)));
}

void ImageOpenController::finishSuccessfulImageLoad(
    const ImageLoadSession& session, EmbeddedMetadata metadata)
{
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::finishSuccessfulImageLoadPlan(
            ImageOpenSuccessfulImageLoadSnapshot {
                session.hasContainerNavigationTarget(),
            },
            session, std::move(metadata))));
}

void ImageOpenController::reportRuntimePlan(const ImageDocumentRuntimePlan& plan)
{
    m_callbacks.runtimePlan(plan);
}
}
