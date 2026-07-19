// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpredecodecontroller.h"

#include "imagedocumentstate.h"
#include "location/imagedocumentlocation.h"
#include "predecode/imagepredecodecoordinator.h"

#include <optional>
#include <utility>
#include <vector>

namespace kiriview {
namespace {
    std::optional<std::vector<DisplayedPredecodeImage>> displayedPredecodeImages(
        const ImageDocumentPredecodeController::PrimaryDisplayedImageCallback& primaryImage,
        std::optional<DisplayedPredecodeImage> secondaryImage)
    {
        std::optional<DisplayedPredecodeImage> primary
            = primaryImage ? primaryImage() : std::nullopt;
        if (!primary.has_value() || !primary->hasLocation()) {
            return std::nullopt;
        }

        std::vector<DisplayedPredecodeImage> displayedImages;
        displayedImages.push_back(std::move(*primary));
        if (secondaryImage.has_value()) {
            displayedImages.push_back(std::move(*secondaryImage));
        }

        return displayedImages;
    }

    std::optional<DisplayedImageLocation> predecodeLocationForTarget(
        const ImageDocumentPageTarget& target, const DisplayedImageLocation& currentLocation)
    {
        if (target.kind != ImageDocumentPageKind::Image || target.url.isEmpty()) {
            return std::nullopt;
        }

        if (displayedLocationIsInsideOpenedCollectionScope(currentLocation)
            && openedCollectionScopeContainsUrl(
                currentLocation.openedCollectionScope(), target.url)) {
            return DisplayedImageLocation::fromOpenedCollectionScope(
                target.url, currentLocation.openedCollectionScope());
        }

        return DisplayedImageLocation::fromUrl(target.url);
    }

    bool predecodeScopeAllowed(
        const DisplayedImageLocation& location, bool ordinaryDirectMediaPredecodeEnabled)
    {
        return ordinaryDirectMediaPredecodeEnabled
            || displayedLocationIsInsideOpenedCollectionScope(location);
    }
}

ImageDocumentPredecodeController::ImageDocumentPredecodeController(QObject* parent,
    ImageDocumentState& state, PrimaryDisplayedImageCallback primaryDisplayedImage,
    FirstDisplayDecodeContextCallback firstDisplayDecodeContext,
    ImageDecodeDependencies decodeDependencies, qsizetype cacheByteBudget,
    CurrentPageNumberCallback currentPageNumber,
    EnsurePageCandidateSnapshotCallback ensurePageCandidateSnapshot,
    PowerSaverProvider powerSaverProvider, bool ordinaryDirectMediaPredecodeEnabled,
    TimerScheduler timerScheduler, PredecodeThreadCountProvider threadCountProvider)
    : m_state(state)
    , m_primaryDisplayedImage(std::move(primaryDisplayedImage))
    , m_firstDisplayDecodeContext(std::move(firstDisplayDecodeContext))
    , m_coordinator(std::make_unique<ImagePredecodeCoordinator>(parent,
          std::move(decodeDependencies), std::move(powerSaverProvider), cacheByteBudget,
          std::move(timerScheduler), std::move(threadCountProvider)))
    , m_currentPageNumber(std::move(currentPageNumber))
    , m_ensurePageCandidateSnapshot(std::move(ensurePageCandidateSnapshot))
    , m_ordinaryDirectMediaPredecodeEnabled(ordinaryDirectMediaPredecodeEnabled)
{
}

ImageDocumentPredecodeController::~ImageDocumentPredecodeController()
{
    m_callbackLifetime.reset();
}

void ImageDocumentPredecodeController::scheduleAdjacentImagePredecode(
    std::optional<DisplayedPredecodeImage> secondaryImage)
{
    std::optional<std::vector<DisplayedPredecodeImage>> displayedImages
        = displayedPredecodeImages(m_primaryDisplayedImage, std::move(secondaryImage));
    if (!displayedImages.has_value()) {
        m_coordinator->cancel();
        return;
    }

    const DisplayedImageLocation currentLocation = m_state.displayedImageLocation();
    if (!predecodeScopeAllowed(currentLocation, m_ordinaryDirectMediaPredecodeEnabled)) {
        m_coordinator->cancel();
        return;
    }

    ImagePredecodeCoordinator::Context context {
        currentLocation,
        std::move(*displayedImages),
        m_firstDisplayDecodeContext ? m_firstDisplayDecodeContext()
                                    : ImageFirstDisplayDecodeContext {},
        m_currentPageNumber ? m_currentPageNumber() - 1 : -1,
        {},
        false,
        ImageDocumentPageCandidateListSnapshot {},
    };
    scheduleWithConfirmedCandidateSnapshot(std::move(context));
}

void ImageDocumentPredecodeController::scheduleImageNavigationTargetPredecode(
    const ImageDocumentPageTarget& target, int targetPageIndex,
    std::optional<DisplayedPredecodeImage> secondaryImage)
{
    const std::optional<DisplayedImageLocation> targetLocation
        = predecodeLocationForTarget(target, m_state.displayedImageLocation());
    if (!targetLocation.has_value()) {
        return;
    }

    if (!predecodeScopeAllowed(*targetLocation, m_ordinaryDirectMediaPredecodeEnabled)) {
        return;
    }

    std::optional<std::vector<DisplayedPredecodeImage>> displayedImages
        = displayedPredecodeImages(m_primaryDisplayedImage, std::move(secondaryImage));
    if (!displayedImages.has_value()) {
        return;
    }

    ImagePredecodeCoordinator::Context context {
        *targetLocation,
        std::move(*displayedImages),
        m_firstDisplayDecodeContext ? m_firstDisplayDecodeContext()
                                    : ImageFirstDisplayDecodeContext {},
        targetPageIndex,
        {},
        true,
        ImageDocumentPageCandidateListSnapshot {},
    };
    scheduleWithConfirmedCandidateSnapshot(std::move(context));
}

void ImageDocumentPredecodeController::scheduleWithConfirmedCandidateSnapshot(
    PredecodeScheduleContext context)
{
    const std::optional<ImageDocumentPageCandidateListContext> candidateContext
        = imageDocumentPageCandidateListContextForDisplayedImage(context.currentLocation);
    if (!candidateContext.has_value() || !m_ensurePageCandidateSnapshot) {
        m_coordinator->schedule(std::move(context));
        return;
    }

    const quint64 requestId = ++m_candidateSnapshotRequestId;
    const std::weak_ptr<int> lifetime = m_callbackLifetime;
    m_ensurePageCandidateSnapshot(*candidateContext,
        [this, lifetime, requestId, context = std::move(context)](
            ImageDocumentPageCandidateListSnapshotResult result) mutable {
            if (lifetime.expired() || requestId != m_candidateSnapshotRequestId) {
                return;
            }
            if (result.succeeded) {
                context.candidateSnapshot = std::move(result.snapshot);
            }
            m_coordinator->schedule(std::move(context));
        });
}

void ImageDocumentPredecodeController::cancel()
{
    ++m_candidateSnapshotRequestId;
    m_coordinator->cancel();
}

void ImageDocumentPredecodeController::clear() { m_coordinator->clear(); }

std::optional<PredecodedImage> ImageDocumentPredecodeController::findPredecodedImage(
    const QUrl& url) const
{
    return m_coordinator->findPredecodedImage(url);
}
}
