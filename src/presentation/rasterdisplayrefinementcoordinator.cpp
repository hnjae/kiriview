// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/rasterdisplayrefinementcoordinator.h"

#include "async/imagecallback.h"
#include "rendering/displayproviderlogging.h"
#include "rendering/imagerendering.h"

#include <QDebug>
#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
namespace {
    constexpr qsizetype refinementCacheCapacity = 4;
    constexpr qsizetype refinementInFlightCapacity = 4;

    struct RefinementWork
    {
        quint64 ticket = 0;
        RasterDisplayRefinementCacheKey cacheKey;
        RasterDisplayRefinementDemandKey demandKey;
        ImageDocumentRenderContext renderContext;
        StaticDisplayImagePayload currentDisplay;
        std::shared_ptr<StaticImageDisplaySource> source;
        std::shared_ptr<std::atomic_bool> startCanceled;
        QSize rasterSize;
        DisplayImageQuality quality = DisplayImageQuality::Exact;
    };

    struct RefinementResult
    {
        quint64 ticket = 0;
        RasterDisplayRefinementCacheKey cacheKey;
        RasterDisplayRefinementDemandKey demandKey;
        ImageDocumentRenderContext renderContext;
        StaticDisplayImagePayload displayImage;
        bool ready = false;
    };

    RasterDisplayRefinementDemandKey rasterDemandKey(const StaticDisplayImagePayload& displayImage,
        const ImagePresentationRenderProjection& projection, quint64 displaySourceRevision,
        qsizetype displayImageByteBudget, quint64 renderRevision,
        const RasterDisplayBucketKey& bucketKey)
    {
        return RasterDisplayRefinementDemandKey {
            displayImage.sourceIdentity,
            displayImage.originalSize,
            projection.pageRole,
            displaySourceRevision,
            projection.imageRevision,
            projection.renderContextGeneration,
            projection.renderContextGeneration,
            static_cast<quint64>(std::max<qsizetype>(0, displayImageByteBudget)),
            static_cast<quint64>(std::max(0, projection.rotationDegrees)),
            renderRevision,
            bucketKey,
        };
    }

    ImageDocumentRenderContext renderContextForProjection(
        const ImagePresentationRenderProjection& projection)
    {
        return ImageDocumentRenderContext {
            projection.devicePixelRatio,
            projection.maximumTextureSize,
            projection.renderContextGeneration,
        };
    }

    RasterDisplayRefinementCacheKey refinementCacheKey(
        const StaticDisplayImagePayload& displayImage,
        const ImagePresentationRenderProjection& projection, const StaticImageDisplaySource& source,
        const RasterDisplayBucketDecision& decision)
    {
        return RasterDisplayRefinementCacheKey {
            displayImage.displayScopeIdentity,
            displayImage.sourceIdentity,
            displayImage.imageReaderTransform.transformations,
            displayImage.originalSize,
            projection.pageRole,
            source.isResolutionIndependent(),
            decision.quality,
            decision.bucketKey,
        };
    }

    StaticDisplayImagePayload refinedDisplayImagePayload(RefinementWork work, QImage image)
    {
        work.currentDisplay.image = displayReadyImage(image);
        work.currentDisplay.quality = work.quality;
        work.currentDisplay.displayPixelsPerSourcePixel = imagePixelsPerSourcePixel(
            work.currentDisplay.originalSize, work.currentDisplay.image.size());
        work.currentDisplay.previewOrigin = DisplayImagePreviewOrigin::None;
        work.currentDisplay.refinementSource = std::move(work.source);
        return std::move(work.currentDisplay);
    }

    RefinementResult runRefinement(RefinementWork work)
    {
        if (work.startCanceled != nullptr && work.startCanceled->load(std::memory_order_relaxed)) {
            return { work.ticket, std::move(work.cacheKey), std::move(work.demandKey),
                work.renderContext, {}, false };
        }
        if (work.source == nullptr || work.rasterSize.isEmpty()) {
            return { work.ticket, std::move(work.cacheKey), std::move(work.demandKey),
                work.renderContext, {}, false };
        }

        StaticImageDisplayDecodeResult decodeResult
            = work.source->decodeRasterDisplayImageWithDiagnostics(work.rasterSize);
        Q_UNUSED(decodeResult.diagnostics);
        QImage image = std::move(decodeResult.image);
        if (image.isNull()) {
            return { work.ticket, std::move(work.cacheKey), std::move(work.demandKey),
                work.renderContext, {}, false };
        }

        return { work.ticket, work.cacheKey, work.demandKey, work.renderContext,
            refinedDisplayImagePayload(std::move(work), std::move(image)), true };
    }
}

RasterDisplayRefinementCoordinator::RasterDisplayRefinementCoordinator(QObject* context,
    qsizetype displayImageByteBudget, ImageWorkerScheduler workerScheduler,
    AcceptedCallback acceptedCallback)
    : m_context(context)
    , m_displayImageByteBudget(displayImageByteBudget)
    , m_workerScheduler(std::move(workerScheduler))
    , m_acceptedCallback(std::move(acceptedCallback))
{
}

RasterDisplayRefinementCoordinator::~RasterDisplayRefinementCoordinator() { cancel(); }

void RasterDisplayRefinementCoordinator::request(StaticDisplayImagePayload currentDisplay,
    const ImagePresentationRenderProjection& projection, quint64 displaySourceRevision)
{
    if (!projection.visible || projection.visibleItemRect.isEmpty()
        || currentDisplay.refinementSource == nullptr
        || !currentDisplay.refinementSource->supportsRasterDisplayRefinement()) {
        cancel();
        return;
    }

    std::shared_ptr<StaticImageDisplaySource> source = currentDisplay.refinementSource;
    const RasterDisplayBucketPolicyInput policyInput {
        currentDisplay.originalSize,
        currentDisplay.image.size(),
        currentDisplay.quality,
        projection.displaySize,
        projection.visibleItemRect,
        projection.devicePixelRatio,
        projection.rotationDegrees,
        projection.maximumTextureSize,
        m_displayImageByteBudget,
    };
    const RasterDisplayBucketDecision decision = source->isResolutionIndependent()
        ? svgDisplayBucketDecision(policyInput)
        : rasterDisplayBucketDecision(policyInput);
    if (decision.status != RasterDisplayBucketStatus::RefinementNeeded
        || decision.bucketKey.rasterSize == currentDisplay.image.size()) {
        cancel();
        return;
    }

    RasterDisplayRefinementDemandKey demandKey = rasterDemandKey(currentDisplay, projection,
        displaySourceRevision, m_displayImageByteBudget, 0, decision.bucketKey);
    const ImageDocumentRenderContext renderContext = renderContextForProjection(projection);
    const RasterDisplayRefinementCacheKey cacheKey
        = refinementCacheKey(currentDisplay, projection, *source, decision);
    if (m_activeDemand.has_value()) {
        RasterDisplayRefinementDemandKey pendingDemand = *m_activeDemand;
        pendingDemand.renderRevision = 0;
        if (pendingDemand == demandKey) {
            return;
        }
    }
    if (promoteCachedRefinement(cacheKey, renderContext)) {
        return;
    }
    std::optional<RasterDisplayRefinementDemandKey> attachedDemand
        = attachInFlightRefinement(cacheKey, demandKey);
    if (attachedDemand.has_value()) {
        return;
    }

    const quint64 ticket = m_ticket.next();
    demandKey.renderRevision = ticket;
    m_activeDemand = demandKey;
    std::shared_ptr<std::atomic_bool> startCanceled = std::make_shared<std::atomic_bool>(false);
    retainInFlightRefinement(cacheKey, demandKey, ticket, startCanceled);
    m_workerScheduler.run(
        m_context,
        [work = RefinementWork {
             ticket,
             cacheKey,
             demandKey,
             renderContext,
             std::move(currentDisplay),
             std::move(source),
             std::move(startCanceled),
             decision.bucketKey.rasterSize,
             decision.quality,
        }]() mutable { return runRefinement(std::move(work)); },
        [this](RefinementResult result) mutable {
            std::optional<RasterDisplayRefinementDemandKey> currentDemand
                = takeInFlightRefinement(result.cacheKey, result.ticket);
            if (!currentDemand.has_value()) {
                return;
            }
            if (result.ready) {
                retainCachedRefinement(result.cacheKey, result.displayImage);
            }
            if (!m_activeDemand.has_value() || *m_activeDemand != *currentDemand) {
                return;
            }

            m_activeDemand = std::nullopt;
            if (!result.ready) {
                return;
            }
            invokeIfSet(
                m_acceptedCallback, std::move(result.displayImage), result.renderContext);
        });
}

void RasterDisplayRefinementCoordinator::cancel()
{
    m_ticket.invalidate();
    m_activeDemand = std::nullopt;
}

bool RasterDisplayRefinementCoordinator::promoteCachedRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey,
    const ImageDocumentRenderContext& renderContext)
{
    auto entry = std::find_if(m_cachedRefinements.begin(), m_cachedRefinements.end(),
        [&cacheKey](
            const CachedRefinement& refinement) { return refinement.cacheKey == cacheKey; });
    if (entry == m_cachedRefinements.end()) {
        return false;
    }

    CachedRefinement cached = *entry;
    m_cachedRefinements.erase(entry);
    m_cachedRefinements.push_back(cached);
    invokeIfSet(m_acceptedCallback, std::move(cached.displayImage), renderContext);
    return true;
}

void RasterDisplayRefinementCoordinator::retainCachedRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey, const StaticDisplayImagePayload& displayImage)
{
    auto entry = m_cachedRefinements.begin();
    while (entry != m_cachedRefinements.end()) {
        if (entry->cacheKey == cacheKey) {
            entry = m_cachedRefinements.erase(entry);
            continue;
        }
        ++entry;
    }
    m_cachedRefinements.push_back(CachedRefinement { cacheKey, displayImage });
    while (static_cast<qsizetype>(m_cachedRefinements.size()) > refinementCacheCapacity) {
        m_cachedRefinements.erase(m_cachedRefinements.begin());
    }
}

std::optional<RasterDisplayRefinementDemandKey>
RasterDisplayRefinementCoordinator::attachInFlightRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey, RasterDisplayRefinementDemandKey demandKey)
{
    auto entry = std::find_if(m_inFlightRefinements.begin(), m_inFlightRefinements.end(),
        [&cacheKey](
            const InFlightRefinement& refinement) { return refinement.cacheKey == cacheKey; });
    if (entry == m_inFlightRefinements.end()) {
        return std::nullopt;
    }

    demandKey.renderRevision = entry->ticket;
    entry->demandKey = demandKey;
    m_activeDemand = demandKey;
    return demandKey;
}

void RasterDisplayRefinementCoordinator::retainInFlightRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey,
    const RasterDisplayRefinementDemandKey& demandKey, quint64 ticket,
    std::shared_ptr<std::atomic_bool> startCanceled)
{
    auto entry = m_inFlightRefinements.begin();
    while (entry != m_inFlightRefinements.end()) {
        if (entry->cacheKey == cacheKey || entry->ticket == ticket) {
            entry = m_inFlightRefinements.erase(entry);
            continue;
        }
        ++entry;
    }
    m_inFlightRefinements.push_back(
        InFlightRefinement { cacheKey, demandKey, ticket, std::move(startCanceled) });
    while (static_cast<qsizetype>(m_inFlightRefinements.size()) > refinementInFlightCapacity) {
        InFlightRefinement evicted = m_inFlightRefinements.front();
        if (evicted.startCanceled != nullptr) {
            evicted.startCanceled->store(true, std::memory_order_relaxed);
        }
        if (m_activeDemand.has_value() && *m_activeDemand == evicted.demandKey) {
            m_activeDemand = std::nullopt;
        }
        m_inFlightRefinements.erase(m_inFlightRefinements.begin());
    }
}

std::optional<RasterDisplayRefinementDemandKey>
RasterDisplayRefinementCoordinator::takeInFlightRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey, quint64 ticket)
{
    auto entry = std::find_if(m_inFlightRefinements.begin(), m_inFlightRefinements.end(),
        [&cacheKey, ticket](const InFlightRefinement& refinement) {
            return refinement.cacheKey == cacheKey && refinement.ticket == ticket;
        });
    if (entry == m_inFlightRefinements.end()) {
        return std::nullopt;
    }

    RasterDisplayRefinementDemandKey demandKey = entry->demandKey;
    m_inFlightRefinements.erase(entry);
    return demandKey;
}
}
