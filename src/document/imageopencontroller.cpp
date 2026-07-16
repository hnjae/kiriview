// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageopencontroller.h"

#include "async/imagecallback.h"
#include "imagedocumentstate.h"
#include "imageloader.h"
#include "imageopenapplicationplanapplier.h"
#include "imageopenworkflow.h"
#include "localization/imageerrortext.h"
#include "location/imagedocumentlocation.h"
#include "presentation/imagepagesurfacecontroller.h"
#include "presentation/imagepresentationload.h"
#include "presentation/imagepresentationruntime.h"
#include "rendering/displayproviderlogging.h"

#include <KLocalizedString>
#include <QDebug>
#include <memory>
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
    if (failure.kind == kiriview::ImageLoadFailureKind::EmptyOpenedCollection) {
        return emptyOpenedCollectionErrorMessage();
    }

    return failure.userMessage;
}

QString animationLoadErrorMessage(const QString& errorString)
{
    return errorString.isEmpty()
        ? kiriview::imageErrorText(kiriview::ImageErrorTextId::DecodeImageAnimation)
        : errorString;
}

QString unsupportedOpenedCollectionVideoMessage()
{
    return i18nc("@info:status", "KiriView can’t play this video from the selected collection.");
}

kiriview::ImageLoadFailure imagePresentationFailure(
    const kiriview::ImageLoadSession& session, QString message)
{
    return kiriview::ImageLoadFailure {
        session.imageUrl(),
        session.id(),
        kiriview::ImageLoadFailureKind::Presentation,
        message,
        message,
        kiriview::ImageLoadFailureSeverity::Error,
        false,
    };
}

QString displayLoadOutcomeDiagnostic(kiriview::ImageDisplayLoadOutcome outcome)
{
    switch (outcome) {
    case kiriview::ImageDisplayLoadOutcome::Loaded:
        return QStringLiteral("loaded");
    case kiriview::ImageDisplayLoadOutcome::Error:
        return QStringLiteral("error");
    case kiriview::ImageDisplayLoadOutcome::Missing:
        return QStringLiteral("missing");
    }

    return QStringLiteral("unknown");
}

QString displayContentDiagnostic(kiriview::ImageDisplayContentKind contentKind)
{
    return contentKind == kiriview::ImageDisplayContentKind::AnimationFrame
        ? QStringLiteral("animation-frame")
        : QStringLiteral("still-image");
}

kiriview::ImageLoadFailure withUserMessage(kiriview::ImageLoadFailure failure, QString userMessage)
{
    failure.userMessage = std::move(userMessage);
    return failure;
}
}

namespace kiriview {
ImageOpenController::ImageOpenController(QObject* parent, ImageDocumentState& state,
    ImagePageSurfaceController& pageSurfaceController,
    ImagePresentationRuntime& presentationRuntime, ImageOpenController::Callbacks callbacks,
    ImageDocumentPageCandidateProvider candidateProvider,
    ImageDecodeDependencies decodeDependencies)
    : m_state(state)
    , m_pageSurfaceController(pageSurfaceController)
    , m_presentationRuntime(presentationRuntime)
    , m_callbacks(std::move(callbacks))
{
    m_imageLoader = std::make_unique<ImageLoader>(parent, std::move(candidateProvider),
        std::move(decodeDependencies),
        ImageLoader::Callbacks {
            [this](ImageLoadSession session, ImageLoadFailure failure) {
                [[maybe_unused]] auto batch = m_state.beginChangeBatch();
                finishLoadWithError(session, std::move(failure));
            },
            [this](ImageLoadSession session, DecodedImage image) {
                [[maybe_unused]] auto batch = m_state.beginChangeBatch();
                finishDecodedImageLoad(std::move(session), std::move(image));
            },
            [this](ImageLoadSession session, PredecodedImage image) {
                [[maybe_unused]] auto batch = m_state.beginChangeBatch();
                finishPredecodedImageLoad(std::move(session), std::move(image));
            },
            [this](ImageLoadSession session, StaticDisplayImagePayload preview) {
                [[maybe_unused]] auto batch = m_state.beginChangeBatch();
                finishThumbnailPreviewLoad(std::move(session), std::move(preview));
            },
            [this](ImageLoadSession session) {
                [[maybe_unused]] auto batch = m_state.beginChangeBatch();
                finishUnsupportedOpenedCollectionVideoLoad(std::move(session));
            },
            [this](const QUrl& url) {
                if (!m_callbacks.findPredecodedImage) {
                    return std::optional<PredecodedImage>();
                }

                return m_callbacks.findPredecodedImage(url);
            },
            [this](ImageLoadSession session) {
                [[maybe_unused]] auto batch = m_state.beginChangeBatch();
                finishSourcePrepared(std::move(session));
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
    m_imageLoader->start(std::move(request), m_presentationRuntime.firstDisplayDecodeContext(),
        m_callbacks.pageCandidateSnapshot ? m_callbacks.pageCandidateSnapshot()
                                          : ImageDocumentPageCandidateListSnapshot {});
}

void ImageOpenController::prepareSourceLoad(const ImageDocumentSourceLoadRequest& request)
{
    m_sourceLoadRequest = request;
}

void ImageOpenController::cancel() { m_imageLoader->cancel(); }

void ImageOpenController::finishAnimationLoadWithError(const QString& errorString)
{
    const QString message = animationLoadErrorMessage(errorString);
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::finishPresentationLoadWithErrorPlan(ImageLoadFailure {
            m_state.displayedUrl().isEmpty() ? m_state.sourceUrl() : m_state.displayedUrl(),
            0,
            ImageLoadFailureKind::Presentation,
            message,
            errorString.isEmpty() ? message : errorString,
            ImageLoadFailureSeverity::Error,
            false,
        })));
}

void ImageOpenController::finishDisplayLoadWithError(const ImageDisplayLoadResolution& resolution)
{
    const QString message = imageErrorText(ImageErrorTextId::DisplayImage);
    const QString diagnostic
        = QStringLiteral(
            "display provider load failed: outcome=%1 content=%2 provider=%3 revision=%4 "
            "sourceIdentity=%5")
              .arg(displayLoadOutcomeDiagnostic(resolution.outcome),
                  displayContentDiagnostic(resolution.contentKind),
                  resolution.providerUrl.toString(), QString::number(resolution.revision),
                  resolution.sourceIdentity);
    qCWarning(kiriviewDisplayProviderLog).noquote() << diagnostic;
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::finishPresentationLoadWithErrorPlan(ImageLoadFailure {
            m_state.displayedUrl().isEmpty() ? m_state.sourceUrl() : m_state.displayedUrl(),
            0,
            ImageLoadFailureKind::Presentation,
            message,
            diagnostic,
            ImageLoadFailureSeverity::Error,
            false,
        })));
}

void ImageOpenController::finishEmptySourceLoad()
{
    reportRuntimePlan(
        applyImageOpenApplicationPlan(m_state, ImageOpenWorkflow::finishEmptySourceLoadPlan()));
}

void ImageOpenController::beginSourceLoad()
{
    m_pageSurfaceController.clearShadowDisplayImage();
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::beginSourceLoadPlan(ImageOpenBeginSourceLoadSnapshot {
            m_pageSurfaceController.hasImage(),
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

    const QString message = openedCollectionOpenErrorMessage(errorString);
    reportRuntimePlan(applyImageOpenApplicationPlan(m_state,
        ImageOpenWorkflow::finishContainerNavigationLoadWithErrorPlan(containerUrl, message)));
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
    m_pageSurfaceController.clearImage();
    invokeIfSet(m_callbacks.clearPrimaryPageSlot);
    const ImageDocumentRuntimePlan plan = applyImageOpenApplicationPlan(
        m_state, ImageOpenWorkflow::finishUnsupportedOpenedCollectionVideoLoadPlan(session));
    invokeIfSet(m_callbacks.unsupportedOpenedCollectionVideoEntered, message);
    reportRuntimePlan(plan);
}

void ImageOpenController::finishPlayableOpenedCollectionVideoLoad(ImageLoadSession session)
{
    m_pageSurfaceController.clearImage();
    invokeIfSet(m_callbacks.clearPrimaryPageSlot);
    reportRuntimePlan(applyImageOpenApplicationPlan(
        m_state, ImageOpenWorkflow::finishPlayableOpenedCollectionVideoLoadPlan(session)));
}

void ImageOpenController::finishThumbnailPreviewLoad(
    ImageLoadSession session, StaticDisplayImagePayload preview)
{
    Q_UNUSED(session);
    m_pageSurfaceController.publishShadowDisplayImage(std::move(preview));
}

void ImageOpenController::finishPredecodedImageLoad(ImageLoadSession session, PredecodedImage image)
{
    EmbeddedMetadata metadata = image.embeddedMetadata;
    finishPresentedImageLoad(session,
        presentPredecodedImageLoad(
            m_pageSurfaceController, std::move(image), m_presentationRuntime.renderContext()),
        std::move(metadata));
}

void ImageOpenController::finishDecodedImageLoad(ImageLoadSession session, DecodedImage image)
{
    EmbeddedMetadata metadata = decodedImageEmbeddedMetadata(image);
    finishPresentedImageLoad(session,
        presentDecodedImageLoad(m_pageSurfaceController, std::move(image), session.location(),
            ImagePresentationAnimationHandling::StartAnimation,
            m_presentationRuntime.renderContext()),
        std::move(metadata));
}

void ImageOpenController::finishPresentedImageLoad(
    const ImageLoadSession& session, ImagePresentationLoadResult result, EmbeddedMetadata metadata)
{
    if (!result.presented) {
        finishLoadWithError(session,
            imagePresentationFailure(
                session, imageErrorText(ImageErrorTextId::DecodeImageAnimation)));
        return;
    }

    invokeIfSet(m_callbacks.commitPrimaryPageSlot, session.location());
    finishSuccessfulImageLoad(session, std::move(metadata));
}

void ImageOpenController::finishLoadWithError(
    const ImageLoadSession& session, ImageLoadFailure failure)
{
    m_pageSurfaceController.clearShadowDisplayImage();
    m_pageSurfaceController.clearSameScopeImageNavigationRetention();
    const QString userMessage = loadFailureUserMessage(failure);
    const ImageLoadFailure normalizedFailure = withUserMessage(std::move(failure), userMessage);
    reportRuntimePlan(applyImageOpenApplicationPlan(
        m_state, ImageOpenWorkflow::finishLoadWithErrorPlan(session, normalizedFailure)));
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
