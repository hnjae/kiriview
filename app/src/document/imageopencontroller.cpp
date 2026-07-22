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
    const kiriview::ImageLoadSession& session, QString message)
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
    ImageLoader::EnsurePageCandidateSnapshotCallback ensurePageCandidateSnapshot
        = m_callbacks.ensurePageCandidateSnapshot;
    m_imageLoader = std::make_unique<ImageLoader>(ImageLoader::Callbacks {
        [this](ImageLoadSession session, ImageLoadFailure failure) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishLoadWithError(session, std::move(failure));
        },
        [this](ImageLoadSession session) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishUnsupportedOpenedCollectionVideoLoad(std::move(session));
        },
        [this](const QUrl& url) {
            return m_callbacks.findPredecodedImage ? m_callbacks.findPredecodedImage(url)
                                                   : std::optional<PredecodedImage>();
        },
        [this](ImageLoadSession session) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishSourcePrepared(std::move(session));
        },
        std::move(ensurePageCandidateSnapshot),
        [this](ImageLoadSession session, std::optional<PredecodedImage> predecoded) {
            [[maybe_unused]] auto batch = m_state.beginChangeBatch();
            finishPreparedViewportImageLoad(std::move(session), std::move(predecoded));
        },
    });
}

ImageOpenController::~ImageOpenController() { cancel(); }

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
    beginSourceLoad();
    const ImageFirstDisplayDecodeContext firstDisplayContext = m_callbacks.firstDisplayDecodeContext
        ? m_callbacks.firstDisplayDecodeContext()
        : ImageFirstDisplayDecodeContext {};
    m_imageLoader->start(std::move(request), firstDisplayContext);
}

void ImageOpenController::prepareSourceLoad(const ImageDocumentSourceLoadRequest& request)
{
    m_sourceLoadRequest = request;
}

void ImageOpenController::cancel() { m_imageLoader->cancel(); }

void ImageOpenController::finishViewportImageLoadReady(
    const ImageLoadSession& session, QSize imageSize, EmbeddedMetadata metadata)
{
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    invokeIfSet(m_callbacks.commitPrimaryPageSlot, session.location(), imageSize);
    finishSuccessfulImageLoad(session, std::move(metadata));
}

void ImageOpenController::finishViewportImageLoadWithError(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    [[maybe_unused]] auto batch = m_state.beginChangeBatch();
    finishLoadWithError(session, std::move(failure));
}

void ImageOpenController::finishEmptySourceLoad()
{
    invokeIfSet(m_callbacks.clearViewportTarget);
    reportRuntimePlan(
        applyImageOpenApplicationPlan(m_state, ImageOpenWorkflow::finishEmptySourceLoadPlan()));
}

void ImageOpenController::beginSourceLoad()
{
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::beginSourceLoadPlan(ImageOpenBeginSourceLoadSnapshot {
            m_callbacks.hasCommittedImage && m_callbacks.hasCommittedImage(),
            !m_state.loadingContainerNavigationUrl().isEmpty(),
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
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::finishContainerNavigationLoadWithErrorPlan(
            containerUrl, openedCollectionOpenErrorMessage(errorString))));
}

void ImageOpenController::finishSourcePrepared(ImageLoadSession session)
{
    reportRuntimePlan(
        applyImageOpenApplicationPlan(m_state, ImageOpenWorkflow::resolveSourceImagePlan(session)));
}

void ImageOpenController::finishUnsupportedOpenedCollectionVideoLoad(ImageLoadSession session)
{
    if (m_callbacks.openedCollectionVideoPlaybackAvailable
        && m_callbacks.openedCollectionVideoPlaybackAvailable(
            session.openedCollectionScope(), session.imageUrl())) {
        finishPlayableOpenedCollectionVideoLoad(std::move(session));
        return;
    }

    const QString message = unsupportedOpenedCollectionVideoMessage();
    invokeIfSet(m_callbacks.clearViewportTarget);
    invokeIfSet(m_callbacks.clearPrimaryPageSlot);
    const ImageDocumentRuntimePlan plan = applyImageOpenApplicationPlan(
        m_state, ImageOpenWorkflow::finishUnsupportedOpenedCollectionVideoLoadPlan(session));
    invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideoEntered, message);
    reportRuntimePlan(plan);
}

void ImageOpenController::finishPlayableOpenedCollectionVideoLoad(ImageLoadSession session)
{
    invokeIfSet(m_callbacks.clearViewportTarget);
    invokeIfSet(m_callbacks.clearPrimaryPageSlot);
    reportRuntimePlan(applyImageOpenApplicationPlan(
        m_state, ImageOpenWorkflow::finishPlayableOpenedCollectionVideoLoadPlan(session)));
}

void ImageOpenController::finishPreparedViewportImageLoad(
    ImageLoadSession session, std::optional<PredecodedImage> predecoded)
{
    if (m_callbacks.prepareViewportImageTarget
        && m_callbacks.prepareViewportImageTarget(session, std::move(predecoded))) {
        return;
    }
    finishLoadWithError(session,
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

void ImageOpenController::reportRuntimePlan(ImageDocumentRuntimePlan plan)
{
    invokeIfSet(m_callbacks.runtimePlan, plan);
}
}
