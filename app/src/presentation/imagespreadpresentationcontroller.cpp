// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadpresentationcontroller.h"

#include "async/imagecallback.h"
#include "document/imagedocumentnotifications.h"
#include "presentation/imagesecondarypagecontroller.h"
#include "presentation/imagespreadmodepolicy.h"
#include "presentation/imagespreadpagecache.h"

#include <utility>

namespace kiriview {
ImageSpreadPresentationController::ImageSpreadPresentationController(
    ImageDocumentState& state, Callbacks callbacks)
    : m_state(state)
    , m_callbacks(std::move(callbacks))
{
    m_secondaryPageController
        = std::make_unique<ImageSecondaryPageController>(ImageSecondaryPageController::Callbacks {
            [this](ImageSecondaryPageLoadResult result, const DisplayedImageLocation& location,
                const QSize& imageSize) {
                handleSecondaryPageLoadFinished(result, location, imageSize);
            },
            [this](const QUrl& url) {
                return m_callbacks.findPredecodedImage ? m_callbacks.findPredecodedImage(url)
                                                       : std::optional<PredecodedImage>();
            },
            [this](ImageLoadSession session, std::optional<PredecodedImage> predecoded) {
                const bool lifecycleAlreadyAdvanced = m_pendingShapeChange;
                m_pendingShapeChange = false;
                if (!lifecycleAlreadyAdvanced) {
                    m_state.advancePresentationLifecycle();
                }
                invokeIfSet(
                    m_callbacks.secondaryImagePrepared, std::move(session), std::move(predecoded));
            },
        });
}

ImageSpreadPresentationController::~ImageSpreadPresentationController() { shutdown(); }

int ImageSpreadPresentationController::currentLastPageNumber() const
{
    return m_secondaryPageRefresh.currentLastPageNumber(pageNavigationContext());
}

ImageDocumentPageActiveNavigationSnapshot
ImageSpreadPresentationController::activeNavigationSnapshot() const
{
    return m_secondaryPageRefresh.activeNavigationSnapshot(pageNavigationContext());
}

ImageSpreadPageNavigationTarget
ImageSpreadPresentationController::imageDocumentPageNavigationTarget(
    NavigationDirection direction) const
{
    return m_secondaryPageRefresh.pageNavigationTarget(direction, pageNavigationContext());
}

int ImageSpreadPresentationController::relativePageNavigationTarget(int offset) const
{
    return m_secondaryPageRefresh.relativePageNavigationTarget(offset, pageNavigationContext());
}

bool ImageSpreadPresentationController::twoPageModeEnabled() const { return m_twoPageModeEnabled; }

void ImageSpreadPresentationController::setTwoPageModeEnabled(bool enabled)
{
    if (m_twoPageModeEnabled == enabled) {
        return;
    }

    m_twoPageModeEnabled = enabled;
    m_state.advancePresentationLifecycle();
    m_pendingShapeChange = true;
    if (enabled) {
        refreshSecondaryPage();
    } else {
        discardSecondaryPage(true);
    }
    notifyTwoPageModeChanged();
}

bool ImageSpreadPresentationController::twoPageModeAvailable() const
{
    return readingControlsAvailable();
}

bool ImageSpreadPresentationController::twoPageModeActive() const
{
    return m_twoPageModeEnabled && twoPageModeAvailable();
}

bool ImageSpreadPresentationController::rightToLeftReadingEnabled() const
{
    return m_rightToLeftReadingEnabled;
}

void ImageSpreadPresentationController::setRightToLeftReadingEnabled(bool enabled)
{
    if (m_rightToLeftReadingEnabled == enabled) {
        return;
    }
    m_rightToLeftReadingEnabled = enabled;
    notifyRightToLeftReadingChanged();
}

bool ImageSpreadPresentationController::rightToLeftReadingAvailable() const
{
    return readingControlsAvailable();
}

bool ImageSpreadPresentationController::rightToLeftReadingActive() const
{
    return m_rightToLeftReadingEnabled && rightToLeftReadingAvailable();
}

bool ImageSpreadPresentationController::secondaryPageVisible() const
{
    return m_secondaryPageController != nullptr && m_secondaryPageController->visible();
}

std::optional<DisplayedPredecodeImage>
ImageSpreadPresentationController::secondaryDisplayedPredecodeImage() const
{
    if (!secondaryPageVisible()) {
        return std::nullopt;
    }
    return DisplayedPredecodeImage {
        m_secondaryPageController->displayedImageLocation(),
        true,
        m_callbacks.secondaryDisplayImage ? m_callbacks.secondaryDisplayImage() : std::nullopt,
        {},
    };
}

void ImageSpreadPresentationController::commitPrimaryPageSlot(
    const DisplayedImageLocation& location, QSize imageSize)
{
    m_committedPrimaryImageSize = imageSize;
    m_secondaryPageRefresh.cachePageSize(location.imageUrl(), imageSize);
}

void ImageSpreadPresentationController::clearPrimaryPageSlot()
{
    m_committedPrimaryImageSize = {};
    discardSecondaryPage(false);
}

void ImageSpreadPresentationController::refreshSecondaryPage()
{
    const ImageSpreadSecondaryPageRefreshResult result
        = m_secondaryPageRefresh.planRefresh(ImageSpreadSecondaryPageRefreshRequest {
            twoPageModeActive(),
            primaryPageIsWide(),
            secondaryPageVisible(),
            m_secondaryPageController->displayedImageLocation().imageUrl(),
            pageNavigationSnapshot(),
        });
    if (result.action == ImageSpreadSecondaryPageRefreshAction::PrimaryOnly) {
        discardSecondaryPage(true);
        scheduleAdjacentPredecode();
        return;
    }
    if (result.action == ImageSpreadSecondaryPageRefreshAction::KeepCurrentSecondary) {
        return;
    }
    if (result.targetUrl.isEmpty()) {
        discardSecondaryPage(true);
        return;
    }
    startSecondaryPageLoad(result.targetUrl);
}

void ImageSpreadPresentationController::handleDocumentChange(ImageDocumentChange change)
{
    if (change != ImageDocumentChange::PageNavigation) {
        return;
    }
    if (m_secondaryPageRefresh.primarySelectionMatchesDisplayed(
            pageNavigationSnapshot(), m_state.displayedUrl())) {
        refreshSecondaryPage();
    }
    notifyRightToLeftReadingChanged();
}

void ImageSpreadPresentationController::clearSecondaryPage() { discardSecondaryPage(true); }

void ImageSpreadPresentationController::shutdown()
{
    if (m_secondaryPageController != nullptr) {
        m_secondaryPageController->cancel();
    }
}

void ImageSpreadPresentationController::finishViewportSecondaryPageLoad(
    const ImageLoadSession& session, QSize imageSize)
{
    m_secondaryPageController->finishProviderLoad(session, imageSize);
}

void ImageSpreadPresentationController::finishViewportSecondaryPageLoadWithError(
    const ImageLoadSession& session)
{
    m_secondaryPageController->finishProviderLoadWithError(session);
}

void ImageSpreadPresentationController::resetRightToLeftReading()
{
    m_rightToLeftReadingEnabled = false;
}

void ImageSpreadPresentationController::notifyRightToLeftReadingChanged()
{
    notifyChanges(imageDocumentRightToLeftReadingNotifications(secondaryPageVisible()));
}

void ImageSpreadPresentationController::startSecondaryPageLoad(const QUrl& url)
{
    m_secondaryPageController->startLoad(url, m_state.displayedOpenedCollectionScope());
}

void ImageSpreadPresentationController::handleSecondaryPageLoadFinished(
    ImageSecondaryPageLoadResult result, const DisplayedImageLocation& location, QSize imageSize)
{
    if (result != ImageSecondaryPageLoadResult::Failed) {
        m_secondaryPageRefresh.cachePageSize(location.imageUrl(), imageSize);
    }
    if (result == ImageSecondaryPageLoadResult::Visible) {
        finishSecondaryPageVisible();
    } else {
        finishSecondaryPageAsPrimaryOnly();
    }
}

void ImageSpreadPresentationController::discardSecondaryPage(bool submitShapeChange)
{
    const bool hadSecondary = secondaryPageVisible();
    m_secondaryPageController->clear();
    if (submitShapeChange && (hadSecondary || m_pendingShapeChange)) {
        const bool lifecycleAlreadyAdvanced = m_pendingShapeChange;
        m_pendingShapeChange = false;
        if (!lifecycleAlreadyAdvanced) {
            m_state.advancePresentationLifecycle();
        }
        invokeIfSet(m_callbacks.secondaryImageCleared);
    }
}

void ImageSpreadPresentationController::finishSecondaryPageAsPrimaryOnly()
{
    m_secondaryPageController->clear();
    notifyTwoPageModeChanged();
    scheduleAdjacentPredecode();
}

void ImageSpreadPresentationController::finishSecondaryPageVisible()
{
    notifyTwoPageModeChanged();
    scheduleAdjacentPredecode();
}

bool ImageSpreadPresentationController::primaryPageIsWide() const
{
    return imageSpreadPageIsWide(m_committedPrimaryImageSize);
}

bool ImageSpreadPresentationController::readingControlsAvailable() const
{
    const DisplayedImageLocation& location = m_state.displayedImageLocation();
    return imageSpreadReadingControlsAvailable(ImageSpreadReadingAvailability {
        !m_committedPrimaryImageSize.isEmpty(),
        !location.isEmpty(),
        location.openedCollectionScope().isComicBook(),
    });
}

bool ImageSpreadPresentationController::secondaryPageVisibleForNavigation() const
{
    return secondaryPageVisible();
}

ImageSpreadPageNavigationContext ImageSpreadPresentationController::pageNavigationContext() const
{
    return ImageSpreadPageNavigationContext {
        twoPageModeActive(),
        secondaryPageVisibleForNavigation(),
        pageNavigationSnapshot(),
    };
}

void ImageSpreadPresentationController::scheduleAdjacentPredecode()
{
    invokeIfSet(m_callbacks.scheduleAdjacentPredecode);
}

ImageDocumentPageNavigationSnapshot
ImageSpreadPresentationController::pageNavigationSnapshot() const
{
    return m_callbacks.pageNavigationSnapshot ? m_callbacks.pageNavigationSnapshot()
                                              : ImageDocumentPageNavigationSnapshot {};
}

void ImageSpreadPresentationController::notifyTwoPageModeChanged()
{
    notifyChanges(imageDocumentTwoPageModeNotifications());
}

void ImageSpreadPresentationController::notifyChanges(
    const std::vector<ImageDocumentChange>& changes)
{
    invokeIfSet(m_callbacks.changes, changes);
}
}
