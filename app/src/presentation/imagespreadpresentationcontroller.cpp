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
            [this](const DisplayedImageLocation& location) {
                return m_callbacks.findPredecodedImage ? m_callbacks.findPredecodedImage(location)
                                                       : std::optional<PredecodedImage> {};
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
            [this](quint64 primarySessionId, ImageLoadSession session,
                std::optional<PredecodedImage> predecoded) {
                if (!m_pendingPageReplacement.has_value()
                    || m_pendingPageReplacement->primarySession.id() != primarySessionId
                    || m_pendingPageReplacement->phase
                        != PageReplacementPhase::PreparingSecondary) {
                    return;
                }
                invokeIfSet(m_callbacks.navigationSecondaryImagePrepared, primarySessionId,
                    std::move(session), std::move(predecoded));
            },
            [this](quint64 primarySessionId, ImageLoadSession session, ImageLoadFailure failure) {
                if (!m_pendingPageReplacement.has_value()
                    || m_pendingPageReplacement->primarySession.id() != primarySessionId) {
                    return;
                }
                m_pendingPageReplacement.reset();
                invokeIfSet(m_callbacks.navigationSecondaryImagePreparationFailed, primarySessionId,
                    std::move(session), std::move(failure));
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

    const bool pageReplacementWasPending = pageReplacementPairingPending();
    cancelPageReplacementPairing();
    m_twoPageModeEnabled = enabled;
    m_state.advancePresentationLifecycle();
    m_pendingShapeChange = true;
    if (enabled && !pageReplacementWasPending) {
        refreshSecondaryPage();
    } else {
        discardSecondaryPage(SecondaryPageDiscardIntent::PresentationShapeChange);
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
        true,
    };
}

void ImageSpreadPresentationController::commitPrimaryPageSlot(
    const DisplayedImageLocation& location, QSize imageSize)
{
    cancelPageReplacementPairing();
    m_committedPrimaryImageSize = imageSize;
    m_secondaryPageRefresh.cachePageSize(location.imageUrl(), imageSize);
}

ImageSpreadPageReplacementPairingResult
ImageSpreadPresentationController::beginPageReplacementPairing(
    const ImageLoadSession& primarySession, QSize primaryImageSize)
{
    cancelPageReplacementPairing();
    if (primarySession.id() == 0 || primarySession.location().isEmpty()
        || primarySession.kind() != ImageDocumentPageKind::Image
        || !primarySession.request().sameScopePageNavigation() || primaryImageSize.isEmpty()) {
        return ImageSpreadPageReplacementPairingResult::Stale;
    }

    const ImageDocumentPageNavigationSnapshot navigation = pageNavigationSnapshot();
    if (!m_secondaryPageRefresh.primarySelectionMatchesDisplayed(
            navigation, primarySession.imageUrl())) {
        return ImageSpreadPageReplacementPairingResult::Stale;
    }

    m_pendingPageReplacement = PendingPageReplacement {
        primarySession,
        primaryImageSize,
        PageReplacementPhase::PrimaryOnly,
        std::nullopt,
    };
    m_secondaryPageController->beginPageReplacement(primarySession.id());

    const ImageSpreadSecondaryPageRefreshResult plan
        = m_secondaryPageRefresh.planRefresh(ImageSpreadSecondaryPageRefreshRequest {
            m_twoPageModeEnabled
                && primaryPageSupportsSpread(primarySession.location(), primaryImageSize),
            imageSpreadPageIsWide(primaryImageSize),
            false,
            {},
            navigation,
        });
    if (plan.action != ImageSpreadSecondaryPageRefreshAction::LoadTarget
        || plan.targetUrl.isEmpty()) {
        return ImageSpreadPageReplacementPairingResult::PrimaryOnly;
    }

    m_pendingPageReplacement->phase = PageReplacementPhase::PreparingSecondary;
    if (!m_secondaryPageController->startPageReplacementLoad(
            primarySession.id(), plan.targetUrl, primarySession.openedCollectionScope())) {
        cancelPageReplacementPairing(primarySession.id());
        return ImageSpreadPageReplacementPairingResult::Stale;
    }
    return ImageSpreadPageReplacementPairingResult::PreparingSecondary;
}

ImageSpreadPageReplacementSecondaryMetadataResult
ImageSpreadPresentationController::finishPageReplacementSecondaryMetadata(
    quint64 primarySessionId, const ImageLoadSession& secondarySession, QSize secondaryImageSize)
{
    if (!m_pendingPageReplacement.has_value() || primarySessionId == 0
        || m_pendingPageReplacement->primarySession.id() != primarySessionId
        || m_pendingPageReplacement->phase != PageReplacementPhase::PreparingSecondary
        || secondaryImageSize.isEmpty()) {
        return ImageSpreadPageReplacementSecondaryMetadataResult::Stale;
    }

    const ImageSecondaryPageReplacementMetadataResult result
        = m_secondaryPageController->finishPageReplacementProviderLoad(
            primarySessionId, secondarySession, secondaryImageSize);
    if (result == ImageSecondaryPageReplacementMetadataResult::Stale) {
        return ImageSpreadPageReplacementSecondaryMetadataResult::Stale;
    }

    m_secondaryPageRefresh.cachePageSize(secondarySession.imageUrl(), secondaryImageSize);
    if (result == ImageSecondaryPageReplacementMetadataResult::PrimaryOnly) {
        m_pendingPageReplacement->phase = PageReplacementPhase::PrimaryOnly;
        m_pendingPageReplacement->secondary.reset();
        return ImageSpreadPageReplacementSecondaryMetadataResult::PrimaryOnly;
    }

    m_pendingPageReplacement->phase = PageReplacementPhase::Secondary;
    m_pendingPageReplacement->secondary
        = ImageSpreadPreparedSecondaryPage { secondarySession, secondaryImageSize };
    return ImageSpreadPageReplacementSecondaryMetadataResult::Secondary;
}

bool ImageSpreadPresentationController::commitPageReplacementPresentation(
    const ImageLoadSession& primarySession, QSize primaryImageSize,
    const std::optional<ImageSpreadPreparedSecondaryPage>& secondary)
{
    if (!m_pendingPageReplacement.has_value()
        || !m_pendingPageReplacement->primarySession.sameSession(primarySession)
        || m_pendingPageReplacement->primarySession.location() != primarySession.location()
        || m_pendingPageReplacement->primaryImageSize != primaryImageSize
        || primaryImageSize.isEmpty()
        || m_pendingPageReplacement->phase == PageReplacementPhase::PreparingSecondary) {
        return false;
    }

    if (m_pendingPageReplacement->phase == PageReplacementPhase::PrimaryOnly
        && secondary.has_value()) {
        return false;
    }
    if (m_pendingPageReplacement->phase == PageReplacementPhase::Secondary
        && (!secondary.has_value() || !m_pendingPageReplacement->secondary.has_value()
            || !m_pendingPageReplacement->secondary->session.sameSession(secondary->session)
            || m_pendingPageReplacement->secondary->session.location()
                != secondary->session.location()
            || m_pendingPageReplacement->secondary->imageSize != secondary->imageSize
            || secondary->imageSize.isEmpty())) {
        return false;
    }

    const std::optional<ImageSecondaryPageReplacementCommit> secondaryCommit = secondary.has_value()
        ? std::optional<ImageSecondaryPageReplacementCommit> { ImageSecondaryPageReplacementCommit {
              secondary->session, secondary->imageSize } }
        : std::nullopt;
    if (!m_secondaryPageController->commitPageReplacement(primarySession.id(), secondaryCommit)) {
        return false;
    }

    m_committedPrimaryImageSize = primaryImageSize;
    m_secondaryPageRefresh.cachePageSize(primarySession.imageUrl(), primaryImageSize);
    m_pendingPageReplacement.reset();
    m_pendingShapeChange = false;
    notifyTwoPageModeChanged();
    scheduleAdjacentPredecode();
    return true;
}

void ImageSpreadPresentationController::cancelPageReplacementPairing(quint64 primarySessionId)
{
    if (!m_pendingPageReplacement.has_value()
        || (primarySessionId != 0
            && m_pendingPageReplacement->primarySession.id() != primarySessionId)) {
        return;
    }
    const quint64 pendingPrimarySessionId = m_pendingPageReplacement->primarySession.id();
    m_pendingPageReplacement.reset();
    m_secondaryPageController->cancelPageReplacement(pendingPrimarySessionId);
}

bool ImageSpreadPresentationController::pageReplacementPairingPending() const
{
    return m_pendingPageReplacement.has_value();
}

void ImageSpreadPresentationController::clearPrimaryPageSlot()
{
    cancelPageReplacementPairing();
    m_committedPrimaryImageSize = {};
    discardSecondaryPage(SecondaryPageDiscardIntent::Silent);
}

void ImageSpreadPresentationController::refreshSecondaryPage()
{
    if (pageReplacementPairingPending()) {
        return;
    }
    const ImageSpreadSecondaryPageRefreshResult result
        = m_secondaryPageRefresh.planRefresh(ImageSpreadSecondaryPageRefreshRequest {
            twoPagePresentationActive(),
            primaryPageIsWide(),
            secondaryPageVisible(),
            m_secondaryPageController->displayedImageLocation().imageUrl(),
            pageNavigationSnapshot(),
        });
    if (result.action == ImageSpreadSecondaryPageRefreshAction::PrimaryOnly) {
        discardSecondaryPage(SecondaryPageDiscardIntent::PresentationShapeChange);
        scheduleAdjacentPredecode();
        return;
    }
    if (result.action == ImageSpreadSecondaryPageRefreshAction::KeepCurrentSecondary) {
        return;
    }
    if (result.targetUrl.isEmpty()) {
        discardSecondaryPage(SecondaryPageDiscardIntent::PresentationShapeChange);
        return;
    }
    startSecondaryPageLoad(result.targetUrl);
}

void ImageSpreadPresentationController::handleDocumentChange(ImageDocumentChange change)
{
    if (change != ImageDocumentChange::PageNavigation) {
        return;
    }
    if (!pageReplacementPairingPending()
        && m_secondaryPageRefresh.primarySelectionMatchesDisplayed(
            pageNavigationSnapshot(), m_state.displayedUrl())) {
        refreshSecondaryPage();
    }
    notifyRightToLeftReadingChanged();
}

void ImageSpreadPresentationController::clearSecondaryPage()
{
    cancelPageReplacementPairing();
    discardSecondaryPage(SecondaryPageDiscardIntent::PresentationTeardown);
}

void ImageSpreadPresentationController::shutdown()
{
    if (m_secondaryPageController != nullptr) {
        cancelPageReplacementPairing();
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

void ImageSpreadPresentationController::discardSecondaryPage(SecondaryPageDiscardIntent intent)
{
    const bool hadSecondary = secondaryPageVisible();
    m_secondaryPageController->clear();
    if (intent != SecondaryPageDiscardIntent::Silent && (hadSecondary || m_pendingShapeChange)) {
        const bool lifecycleAlreadyAdvanced = m_pendingShapeChange;
        m_pendingShapeChange = false;
        if (!lifecycleAlreadyAdvanced) {
            m_state.advancePresentationLifecycle();
        }
        if (intent == SecondaryPageDiscardIntent::PresentationShapeChange) {
            invokeIfSet(m_callbacks.secondaryImageCleared);
        } else {
            invokeIfSet(m_callbacks.secondaryPresentationTeardown);
        }
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

bool ImageSpreadPresentationController::primaryPageSupportsSpread(
    const DisplayedImageLocation& location, QSize imageSize) const
{
    return imageSpreadPrimaryPageEligible(ImageSpreadPrimaryPageEligibility {
        !imageSize.isEmpty(),
        !location.isEmpty(),
        location.openedCollectionScope().isComicBook(),
    });
}

bool ImageSpreadPresentationController::twoPagePresentationActive() const
{
    return m_twoPageModeEnabled
        && primaryPageSupportsSpread(m_state.displayedImageLocation(), m_committedPrimaryImageSize);
}

bool ImageSpreadPresentationController::readingControlsAvailable() const
{
    return m_state.selectedOpenedCollectionScope().isComicBook();
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
