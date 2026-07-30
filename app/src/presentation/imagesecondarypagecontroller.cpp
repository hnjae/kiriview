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
    loaderCallbacks.error = [this](ImageLoadSession session, ImageLoadFailure failure) {
        if (pageReplacementPending()) {
            const quint64 primarySessionId = m_pageReplacementPrimarySessionId;
            cancelPageReplacement(primarySessionId);
            invokeIfSet(m_callbacks.pageReplacementFailed, primarySessionId, std::move(session),
                std::move(failure));
            return;
        }
        finishClaimedLoadWithError(session);
    };
    loaderCallbacks.findPredecodedImage = [this](const DisplayedImageLocation& location) {
        return m_callbacks.findPredecodedImage ? m_callbacks.findPredecodedImage(location)
                                               : std::optional<PredecodedImage> {};
    };
    loaderCallbacks.targetStarted = [](const ImageLoadSession&) { };
    loaderCallbacks.resolvedImage
        = [this](ImageLoadSession session, std::optional<PredecodedImage> predecoded) {
              if (pageReplacementPending()) {
                  if (m_pageReplacementPhase != PageReplacementPhase::PreparingSecondary
                      || session.id() == 0 || session.location().isEmpty()) {
                      return;
                  }
                  m_pageReplacementPreparedSession = session;
                  invokeIfSet(m_callbacks.pageReplacementPreparedImage,
                      m_pageReplacementPrimarySessionId, std::move(session), std::move(predecoded));
                  return;
              }
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

void ImageSecondaryPageController::beginPageReplacement(quint64 primarySessionId)
{
    cancel();
    if (primarySessionId == 0) {
        return;
    }
    m_pageReplacementPrimarySessionId = primarySessionId;
    m_pageReplacementPhase = PageReplacementPhase::PrimaryOnly;
}

bool ImageSecondaryPageController::startPageReplacementLoad(quint64 primarySessionId,
    const QUrl& url, const OpenedCollectionScopeLocation& openedCollectionScope)
{
    if (primarySessionId == 0 || primarySessionId != m_pageReplacementPrimarySessionId
        || m_pageReplacementPhase != PageReplacementPhase::PrimaryOnly || url.isEmpty()
        || openedCollectionScope.isEmpty()) {
        return false;
    }

    m_pageReplacementPhase = PageReplacementPhase::PreparingSecondary;
    m_pageReplacementPreparedSession.reset();
    m_imageLoader->start(ImageLoadRequest::fromSameScopePageTarget(
        ImageDocumentPageTarget { url, ImageDocumentPageKind::Image }, openedCollectionScope));
    return true;
}

ImageSecondaryPageReplacementMetadataResult
ImageSecondaryPageController::finishPageReplacementProviderLoad(
    quint64 primarySessionId, const ImageLoadSession& session, QSize imageSize)
{
    if (primarySessionId == 0 || primarySessionId != m_pageReplacementPrimarySessionId
        || m_pageReplacementPhase != PageReplacementPhase::PreparingSecondary
        || !m_pageReplacementPreparedSession.has_value()
        || !m_pageReplacementPreparedSession->sameSession(session)
        || m_pageReplacementPreparedSession->location() != session.location()
        || imageSize.isEmpty()) {
        return ImageSecondaryPageReplacementMetadataResult::Stale;
    }

    const std::optional<ImageLoadSession> claimedSession
        = m_imageLoader->claimCurrentSession(session);
    if (!claimedSession.has_value() || claimedSession->location() != session.location()) {
        return ImageSecondaryPageReplacementMetadataResult::Stale;
    }

    m_pageReplacementSecondarySession.reset();
    m_pageReplacementPreparedSession.reset();
    m_pageReplacementSecondaryImageSize = {};
    m_displayState.discardStagedPageReplacement();
    if (imageSpreadPageIsWide(imageSize)) {
        m_pageReplacementPhase = PageReplacementPhase::PrimaryOnly;
        return ImageSecondaryPageReplacementMetadataResult::PrimaryOnly;
    }

    m_pageReplacementPhase = PageReplacementPhase::Secondary;
    m_pageReplacementSecondarySession = *claimedSession;
    m_pageReplacementSecondaryImageSize = imageSize;
    m_displayState.stagePageReplacement(claimedSession->location(), imageSize);
    return ImageSecondaryPageReplacementMetadataResult::Secondary;
}

bool ImageSecondaryPageController::commitPageReplacement(
    quint64 primarySessionId, const std::optional<ImageSecondaryPageReplacementCommit>& secondary)
{
    if (primarySessionId == 0 || primarySessionId != m_pageReplacementPrimarySessionId
        || m_pageReplacementPhase == PageReplacementPhase::PreparingSecondary) {
        return false;
    }

    const bool includeSecondary = secondary.has_value();
    if (m_pageReplacementPhase == PageReplacementPhase::PrimaryOnly && includeSecondary) {
        return false;
    }
    if (m_pageReplacementPhase == PageReplacementPhase::Secondary
        && (!includeSecondary || !m_pageReplacementSecondarySession.has_value()
            || !m_pageReplacementSecondarySession->sameSession(secondary->session)
            || m_pageReplacementSecondarySession->location() != secondary->session.location()
            || m_pageReplacementSecondaryImageSize != secondary->imageSize
            || !m_displayState.stagedPageReplacementMatches(
                secondary->session.location(), secondary->imageSize))) {
        return false;
    }

    m_displayState.commitStagedPageReplacement(includeSecondary);
    m_pageReplacementPrimarySessionId = 0;
    m_pageReplacementPhase = PageReplacementPhase::PrimaryOnly;
    m_pageReplacementPreparedSession.reset();
    m_pageReplacementSecondarySession.reset();
    m_pageReplacementSecondaryImageSize = {};
    return true;
}

void ImageSecondaryPageController::cancelPageReplacement(quint64 primarySessionId)
{
    if (!pageReplacementPending()
        || (primarySessionId != 0 && primarySessionId != m_pageReplacementPrimarySessionId)) {
        return;
    }

    m_imageLoader->cancel();
    m_displayState.discardStagedPageReplacement();
    m_pageReplacementPrimarySessionId = 0;
    m_pageReplacementPhase = PageReplacementPhase::PrimaryOnly;
    m_pageReplacementPreparedSession.reset();
    m_pageReplacementSecondarySession.reset();
    m_pageReplacementSecondaryImageSize = {};
}

bool ImageSecondaryPageController::pageReplacementPending() const
{
    return m_pageReplacementPrimarySessionId != 0;
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
    m_displayState.discardStagedPageReplacement();
    m_pageReplacementPrimarySessionId = 0;
    m_pageReplacementPhase = PageReplacementPhase::PrimaryOnly;
    m_pageReplacementPreparedSession.reset();
    m_pageReplacementSecondarySession.reset();
    m_pageReplacementSecondaryImageSize = {};
}

void ImageSecondaryPageController::finishProviderLoad(
    const ImageLoadSession& session, QSize imageSize)
{
    if (pageReplacementPending()) {
        return;
    }
    const std::optional<ImageLoadSession> claimedSession
        = m_imageLoader->claimCurrentSession(session);
    if (!claimedSession.has_value()) {
        return;
    }
    applyLoadCompletion(m_displayState.finishPresentedLoad(
        claimedSession->location(), imageSize, imageSpreadPageIsWide(imageSize)));
}

void ImageSecondaryPageController::finishProviderLoadWithError(const ImageLoadSession& session)
{
    if (pageReplacementPending()) {
        return;
    }
    const std::optional<ImageLoadSession> claimedSession
        = m_imageLoader->claimCurrentSession(session);
    if (!claimedSession.has_value()) {
        return;
    }
    finishClaimedLoadWithError(*claimedSession);
}

void ImageSecondaryPageController::finishClaimedLoadWithError(const ImageLoadSession& session)
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
