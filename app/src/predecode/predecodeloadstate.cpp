// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "predecodeloadstate.h"

#include "predecodelogging.h"

#include <QDebug>
#include <utility>

namespace {
std::vector<kiriview::DisplayedImageLocation> displayedPredecodeImageLocations(
    const std::vector<kiriview::DisplayedPredecodeImage>& images)
{
    std::vector<kiriview::DisplayedImageLocation> locations;
    locations.reserve(images.size());
    for (const kiriview::DisplayedPredecodeImage& image : images) {
        if (image.hasLocation()) {
            locations.push_back(image.location);
        }
    }

    return locations;
}
}

namespace kiriview {
PredecodeLoadState::PredecodeLoadState(qsizetype cacheByteBudget)
    : m_cache(cacheByteBudget)
{
}

void PredecodeLoadState::cacheDisplayedImages(const std::vector<DisplayedPredecodeImage>& images)
{
    qCDebug(kiriviewPredecodeLog) << "cache displayed images"
                                  << "count" << images.size();
    m_cache.setDisplayedLocations(displayedPredecodeImageLocations(images));
    for (const DisplayedPredecodeImage& image : images) {
        if (!image.isCacheable()) {
            qCDebug(kiriviewPredecodeLog) << "displayed image cache skipped"
                                          << "reason"
                                          << "not-cacheable"
                                          << "url" << image.location.imageUrl();
            continue;
        }

        m_cache.cacheDisplayedImage(
            true, image.location, *image.displayImage, image.embeddedMetadata);
    }
}

void PredecodeLoadState::clearWindow() { m_cache.setWindowLocations({}); }

void PredecodeLoadState::startWindow(
    const PredecodeLoadWindow& window, const PredecodeActiveLoads& activeLoads)
{
    qCDebug(kiriviewPredecodeLog) << "predecode load window"
                                  << "generation" << window.generation << "primaryUrl"
                                  << window.primaryDisplayedLocation.imageUrl() << "locations"
                                  << window.locations.size() << "displayedImages"
                                  << window.displayedImages.size() << "parallelLimit"
                                  << window.parallelLimit;
    cancelBackgroundWork();
    m_activeWindow = ActiveWindow {
        window.firstDisplayContext,
        window.generation,
        window.parallelLimit,
    };
    m_cache.setWindowLocations(window.locations);
    cacheDisplayedImages(window.displayedImages);
    m_cache.enqueueMissingWindowLoads(window.primaryDisplayedLocation, activeLoads);
}

bool PredecodeLoadState::canStartMoreLoads(const PredecodeActiveLoads& activeLoads) const
{
    return m_activeWindow.has_value() && m_activeWindow->parallelLimit > 0
        && activeLoads.size() < m_activeWindow->parallelLimit;
}

std::optional<PredecodeLoadStart> PredecodeLoadState::takeNextLoad(
    const PredecodeActiveLoads& activeLoads)
{
    if (!canStartMoreLoads(activeLoads)) {
        return std::nullopt;
    }

    const std::optional<PredecodeRequest> request = m_cache.takeNextRequest(activeLoads);
    if (!request.has_value()) {
        qCDebug(kiriviewPredecodeLog) << "predecode next load unavailable"
                                      << "activeLoads" << activeLoads.size();
        return std::nullopt;
    }

    qCDebug(kiriviewPredecodeLog) << "predecode next load selected"
                                  << "generation" << m_activeWindow->generation << "url"
                                  << request->location.imageUrl() << "activeLoads"
                                  << activeLoads.size();
    return PredecodeLoadStart {
        ImageDecodeRequest::fromLocation(
            m_activeWindow->generation, request->location, m_activeWindow->firstDisplayContext),
    };
}

void PredecodeLoadState::cacheDecodedImage(
    const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage)
{
    cacheDecodedImage(request, std::move(displayImage), {});
}

void PredecodeLoadState::cacheDecodedImage(const ImageDecodeRequest& request,
    StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata)
{
    qCDebug(kiriviewPredecodeLog) << "cache decoded predecode image"
                                  << "generation" << request.id() << "url" << request.imageUrl();
    m_cache.cacheImage(request.location(), std::move(displayImage), std::move(metadata));
}

void PredecodeLoadState::cancelBackgroundWork()
{
    qCDebug(kiriviewPredecodeLog) << "predecode load state cancel background";
    m_cache.clearQueuedLoads();
    m_activeWindow.reset();
}

void PredecodeLoadState::clear()
{
    cancelBackgroundWork();
    m_cache.clear();
}

std::optional<PredecodedImage> PredecodeLoadState::findPredecodedImage(
    const DisplayedImageLocation& location) const
{
    return m_cache.findImage(location);
}
}
