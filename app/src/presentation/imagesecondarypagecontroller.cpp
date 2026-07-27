// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagesecondarypagecontroller.h"

#include "async/imagecallback.h"
#include "document/imageloader.h"
#include "presentation/imagespreadpagecache.h"

#include <utility>

namespace kiriview {
ImageSecondaryPageController::ImageSecondaryPageController(Callbacks callbacks)
    : m_callbacks(std::move(callbacks))
{
    ImageLoader::Callbacks loaderCallbacks;
    loaderCallbacks.error = [this](const ImageLoadSession& session, const ImageLoadFailure&) {
        finishLoadWithError(session);
    };
    loaderCallbacks.findPredecodedImage = [this](const QUrl& url) {
        return m_callbacks.findPredecodedImage ? m_callbacks.findPredecodedImage(url)
                                               : std::optional<PredecodedImage>();
    };
    loaderCallbacks.targetStarted = [](const ImageLoadSession&) { };
    loaderCallbacks.resolvedImage
        = [this](ImageLoadSession session, std::optional<PredecodedImage> predecoded) {
              invokeIfSet(m_callbacks.preparedImage, std::move(session), std::move(predecoded));
          };
    m_imageLoader = std::make_unique<ImageLoader>(std::move(loaderCallbacks));
}

ImageSecondaryPageController::~ImageSecondaryPageController() { cancel(); }

bool ImageSecondaryPageController::visible() const { return m_displayState.visible(); }

DisplayedImageLocation ImageSecondaryPageController::displayedImageLocation() const
{
    return m_displayState.displayedImageLocation();
}

QSize ImageSecondaryPageController::imageSize() const { return m_displayState.imageSize(); }

void ImageSecondaryPageController::startLoad(
    const QUrl& url, const OpenedCollectionScopeLocation& displayedOpenedCollectionScope)
{
    cancel();
    m_imageLoader->start(ImageLoadRequest::fromSameScopePageTarget(
        ImageDocumentPageTarget { url, ImageDocumentPageKind::Image },
        displayedOpenedCollectionScope));
}

void ImageSecondaryPageController::clear()
{
    cancel();
    m_displayState.clear();
}

void ImageSecondaryPageController::cancel()
{
    if (m_imageLoader != nullptr) {
        m_imageLoader->cancel();
    }
}

void ImageSecondaryPageController::finishProviderLoad(
    const ImageLoadSession& session, QSize imageSize)
{
    applyLoadCompletion(m_displayState.finishPresentedLoad(
        session.location(), imageSize, imageSpreadPageIsWide(imageSize)));
}

void ImageSecondaryPageController::finishProviderLoadWithError(const ImageLoadSession& session)
{
    finishLoadWithError(session);
}

void ImageSecondaryPageController::finishLoadWithError(const ImageLoadSession& session)
{
    applyLoadCompletion(m_displayState.finishFailedLoad(session.location()));
}

void ImageSecondaryPageController::applyLoadCompletion(
    const ImageSecondaryPageLoadCompletion& completion)
{
    invokeIfSet(
        m_callbacks.loadFinished, completion.result, completion.location, completion.imageSize);
}
}
