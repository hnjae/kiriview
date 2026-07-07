// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagepagesurfacecontroller.h"

#include "async/imagecallback.h"
#include "presentation/animationlogging.h"
#include "presentation/imageanimationplayer.h"
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
    constexpr qsizetype staticDisplayBufferCapacity = 2;
    constexpr qsizetype rasterDisplayRefinementCacheCapacity = 4;
    constexpr qsizetype rasterDisplayRefinementInFlightCapacity = 4;

    struct RasterDisplayRefinementWork
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

    struct RasterDisplayRefinementResult
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

    ImageDisplaySourceStatus displaySourceStatusForLoadOutcome(ImageDisplayLoadOutcome outcome)
    {
        switch (outcome) {
        case ImageDisplayLoadOutcome::Loaded:
            return ImageDisplaySourceStatus::Ready;
        case ImageDisplayLoadOutcome::Error:
            return ImageDisplaySourceStatus::Error;
        case ImageDisplayLoadOutcome::Missing:
            return ImageDisplaySourceStatus::Missing;
        }

        return ImageDisplaySourceStatus::Error;
    }

    ImageDisplaySourceSlot displayErrorSourceSlot(QSize imageSize, quint64 revision)
    {
        return ImageDisplaySourceSlot {
            QUrl(),
            revision,
            QString(),
            imageSize,
            imageSize,
            QSize(),
            DisplayImageQuality::Exact,
            ImageDisplaySourceStatus::Error,
            false,
            false,
            ImageDisplaySourceRetentionStatus::None,
            false,
        };
    }

    ImagePresentationPageSlotSource pageSlotSource(
        bool hasImage, const ImageDisplaySourceSlot& displaySource)
    {
        if (!hasImage) {
            return ImagePresentationPageSlotSource::empty();
        }
        if (displaySource.status == ImageDisplaySourceStatus::Error
            && displaySource.providerUrl.isEmpty()) {
            return ImagePresentationPageSlotSource::displayError(displaySource);
        }

        return ImagePresentationPageSlotSource::providerReady(displaySource);
    }

    StaticDisplayImagePayload refinedDisplayImagePayload(
        RasterDisplayRefinementWork work, QImage image)
    {
        work.currentDisplay.image = displayReadyImage(image);
        work.currentDisplay.quality = work.quality;
        work.currentDisplay.displayPixelsPerSourcePixel = imagePixelsPerSourcePixel(
            work.currentDisplay.originalSize, work.currentDisplay.image.size());
        work.currentDisplay.previewOrigin = DisplayImagePreviewOrigin::None;
        work.currentDisplay.refinementSource = std::move(work.source);
        return std::move(work.currentDisplay);
    }

    RasterDisplayRefinementResult runRasterDisplayRefinement(RasterDisplayRefinementWork work)
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

ImagePageSurfaceController::ImagePageSurfaceController(QObject* context,
    ImagePageSurfaceController::Callbacks callbacks, ImageCacheBudgets cacheBudgets,
    std::shared_ptr<DisplayImageStore> displayImageStore, DisplayedPageRole pageRole,
    ImageWorkerScheduler workerScheduler)
    : m_callbacks(std::move(callbacks))
    , m_context(context)
    , m_predecodeCacheByteBudget(cacheBudgets.predecodeCacheByteBudget)
    , m_displayImageByteBudget(cacheBudgets.displayImageCacheByteBudget)
    , m_displayImageStore(
          displayImageStore == nullptr ? sharedDisplayImageStore() : std::move(displayImageStore))
    , m_pageRole(pageRole)
    , m_workerScheduler(std::move(workerScheduler))
{
    m_animationPlayer = std::make_unique<ImageAnimationPlayer>(
        context,
        [this](const QImage& image) { setAnimationFrame(image, m_animationFrameSourceIdentity); },
        [this](
            const QString& errorString) { invokeIfSet(m_callbacks.animationError, errorString); },
        [this]() { releaseRetainedAnimationFrameEntry(); });
}

ImagePageSurfaceController::~ImagePageSurfaceController()
{
    releaseShadowDisplayEntry();
    releaseRetainedStillImageEntry();
    releaseRetainedAnimationFrameEntry();
    releaseBufferedStaticDisplayEntries();
    releaseCurrentDisplayEntry();
}

QSize ImagePageSurfaceController::imageSize() const { return m_imageSize; }

quint64 ImagePageSurfaceController::imageRevision() const { return m_imageRevision; }

bool ImagePageSurfaceController::hasImage() const { return m_hasImage; }

bool ImagePageSurfaceController::isPredecodeCacheable() const { return m_predecodeCacheable; }

qsizetype ImagePageSurfaceController::predecodeCacheByteBudget() const
{
    return m_predecodeCacheByteBudget;
}

std::optional<StaticDisplayImagePayload> ImagePageSurfaceController::displayImage() const
{
    return m_displayImage;
}

ImagePresentationPageSlotSnapshot ImagePageSurfaceController::snapshot() const
{
    return ImagePresentationPageSlotSnapshot {
        imageRevision(),
        imageSize(),
        pageSlotSource(hasImage(), m_displaySource),
    };
}

void ImagePageSurfaceController::setImage(const QImage& image, bool predecodeCacheable)
{
    cancelRasterDisplayRefinement();
    clearShadowDisplayImage();
    clearDisplaySource();
    releaseBufferedStaticDisplayEntries();
    m_animationFrameSourceIdentity.clear();
    if (!image.isNull()) {
        ++m_displaySourceRevision;
        m_displaySource = displayErrorSourceSlot(image.size(), m_displaySourceRevision);
    }
    acceptImageState(image.size(), predecodeCacheable, std::nullopt);
}

void ImagePageSurfaceController::setAnimationFrame(
    const QImage& image, const QString& sourceIdentity)
{
    cancelRasterDisplayRefinement();
    clearShadowDisplayImage();
    m_animationFrameSourceIdentity = sourceIdentity;

    const QImage displayImage = displayReadyImage(image);
    publishAnimationFrameDisplaySource(displayImage, sourceIdentity);
    acceptImageState(displayImage.size(), false, std::nullopt);
    notify(ImageDocumentChange::DisplaySource);
}

void ImagePageSurfaceController::setStaticDisplayImage(StaticDisplayImagePayload displayImage,
    bool predecodeCacheable, const ImageDocumentRenderContext& renderContext)
{
    cancelRasterDisplayRefinement();
    stopAnimation();
    clearShadowDisplayImage();
    m_animationFrameSourceIdentity.clear();
    publishDisplaySource(displayImage);

    Q_UNUSED(renderContext);
    StaticDisplayImagePayload storedDisplay = std::move(displayImage);
    const QSize imageSize = storedDisplay.originalSize;
    acceptImageState(imageSize, predecodeCacheable, std::move(storedDisplay));
}

void ImagePageSurfaceController::updateDisplayProjection(
    const ImagePresentationRenderProjection& projection)
{
    if (!projection.visible || projection.visibleItemRect.isEmpty()) {
        cancelRasterDisplayRefinement();
        updateDisplaySourceVisibility(false);
        return;
    }

    updateDisplaySourceVisibility(true);
    scheduleRasterDisplayRefinement(projection);
}

void ImagePageSurfaceController::clearImage()
{
    cancelRasterDisplayRefinement();
    stopAnimation();
    clearShadowDisplayImage();
    clearDisplaySource();
    releaseBufferedStaticDisplayEntries();
    m_animationFrameSourceIdentity.clear();
    m_imageSize = {};
    m_hasImage = false;
    m_predecodeCacheable = false;
    m_displayImage = std::nullopt;
    ++m_imageRevision;
}

void ImagePageSurfaceController::startAnimation(ImageAnimationPlaybackRequest request)
{
    m_animationPlayer->start(std::move(request));
}

void ImagePageSurfaceController::stopAnimation() { m_animationPlayer->stop(); }

void ImagePageSurfaceController::acceptImageState(
    QSize imageSize, bool predecodeCacheable, std::optional<StaticDisplayImagePayload> displayImage)
{
    m_imageSize = imageSize;
    m_hasImage = !imageSize.isEmpty();
    m_predecodeCacheable = predecodeCacheable;
    m_displayImage = std::move(displayImage);
    ++m_imageRevision;
}

DisplayImageReuseKey ImagePageSurfaceController::staticDisplayReuseKey(
    const StaticDisplayImagePayload& displayImage) const
{
    const QSize rasterSize = displayImage.image.size();
    return DisplayImageReuseKey {
        displayImage.displayScopeIdentity,
        displayImage.sourceIdentity,
        displayImage.imageReaderTransform.transformations,
        displayImage.originalSize,
        rasterSize,
        displayImage.quality,
        displayImage.previewOrigin,
        m_pageRole,
    };
}

void ImagePageSurfaceController::publishDisplaySource(const StaticDisplayImagePayload& displayImage)
{
    releaseRetainedAnimationFrameEntry();
    clearAnimationFrameLoadContract();
    releaseCurrentDisplayEntry();
    m_animationFrameSourceIdentity.clear();

    ++m_displaySourceRevision;
    const QSize rasterSize = displayImage.image.size();
    const DisplayImageReuseKey reuseKey = staticDisplayReuseKey(displayImage);
    const QString entryId = m_displayImageStore == nullptr
        ? QString()
        : m_displayImageStore->acquireReusable(
              DisplayImageEntry {
                  displayImage.image,
                  displayImage.originalSize,
                  rasterSize,
                  displayImage.sourceIdentity,
                  m_pageRole,
                  displayImage.quality,
                  DisplayImageRetentionPriority::Nearby,
                  m_displaySourceRevision,
                  QStringLiteral("static-display"),
                  displayImage.previewOrigin,
              },
              reuseKey);
    const QUrl providerUrl = displayImageSourceForId(entryId);
    const bool loadAcknowledgmentRequired = m_displayImageStore != nullptr && !entryId.isEmpty()
        && m_displayImageStore->acquirePinLease(entryId, DisplayImagePinKind::PendingLoad);
    const bool retainedReplacement = !m_retainedStillImageEntryId.isEmpty();

    m_displayEntryId = entryId;
    m_displayEntryVisiblePinned = false;
    m_pendingStillImageEntryId = loadAcknowledgmentRequired ? entryId : QString();
    m_pendingStillImageProviderUrl = loadAcknowledgmentRequired ? providerUrl : QUrl();
    m_pendingStillImageRevision = loadAcknowledgmentRequired ? m_displaySourceRevision : 0;
    m_pendingStillImageSourceIdentity
        = loadAcknowledgmentRequired ? displayImage.sourceIdentity : QString();
    m_stillImageDisplayLoadPending = loadAcknowledgmentRequired;
    m_displaySource = ImageDisplaySourceSlot {
        providerUrl,
        m_displaySourceRevision,
        displayImage.sourceIdentity,
        displayImage.originalSize,
        rasterSize,
        rasterSize != displayImage.originalSize ? QSize(rasterSize.width(), 0) : QSize(),
        displayImage.quality,
        entryId.isEmpty() ? ImageDisplaySourceStatus::Error : ImageDisplaySourceStatus::Ready,
        false,
        loadAcknowledgmentRequired,
        retainedReplacement ? ImageDisplaySourceRetentionStatus::StaleRetained
                            : ImageDisplaySourceRetentionStatus::None,
        retainedReplacement && loadAcknowledgmentRequired,
    };
    qCDebug(kiriviewDisplayProviderLog)
        << "static display source published"
        << "providerUrl" << providerUrl << "revision" << m_displaySourceRevision << "entryId"
        << entryId << "sourceIdentity" << displayImage.sourceIdentity << "pageRole"
        << static_cast<int>(m_pageRole) << "originalSize" << displayImage.originalSize
        << "rasterSize" << rasterSize << "sourceSizeHint" << m_displaySource.sourceSizeHint
        << "quality" << static_cast<int>(displayImage.quality) << "previewOrigin"
        << static_cast<int>(displayImage.previewOrigin) << "loadAcknowledgmentRequired"
        << loadAcknowledgmentRequired << "retainedReplacement" << retainedReplacement;
    releaseBufferedStaticDisplayEntriesForSource(reuseKey);
    retainBufferedStaticDisplayEntry(reuseKey, entryId);
}

void ImagePageSurfaceController::publishAnimationFrameDisplaySource(
    const QImage& image, const QString& sourceIdentity)
{
    retainCurrentAnimationFrameEntryForLoad();

    ++m_displaySourceRevision;
    const QSize rasterSize = image.size();
    const QString entryId = m_displayImageStore == nullptr
        ? QString()
        : m_displayImageStore->insert(DisplayImageEntry {
              image,
              rasterSize,
              rasterSize,
              sourceIdentity,
              m_pageRole,
              DisplayImageQuality::Exact,
              DisplayImageRetentionPriority::Nearby,
              m_displaySourceRevision,
              QStringLiteral("animation-frame"),
              DisplayImagePreviewOrigin::None,
          });

    m_displayEntryId = entryId;
    m_displayEntryVisiblePinned = false;
    m_currentDisplayEntryIsAnimationFrame = true;
    const QUrl providerUrl = displayImageSourceForId(entryId);
    const bool loadAcknowledgmentRequired = !entryId.isEmpty();
    m_animationFrameDisplayLoadPending = loadAcknowledgmentRequired;
    m_pendingAnimationFrameProviderUrl = providerUrl;
    m_pendingAnimationFrameRevision = m_displaySourceRevision;
    m_pendingAnimationFrameSourceIdentity = sourceIdentity;
    m_displaySource = ImageDisplaySourceSlot {
        providerUrl,
        m_displaySourceRevision,
        sourceIdentity,
        rasterSize,
        rasterSize,
        {},
        DisplayImageQuality::Exact,
        entryId.isEmpty() ? ImageDisplaySourceStatus::Error : ImageDisplaySourceStatus::Ready,
        false,
        loadAcknowledgmentRequired,
        ImageDisplaySourceRetentionStatus::None,
        false,
    };
    qCDebug(kiriviewAnimationLog) << "animation frame provider source published" << "providerUrl"
                                  << providerUrl << "revision" << m_displaySourceRevision
                                  << "entryId" << entryId << "rasterSize" << rasterSize
                                  << "sourceIdentity" << sourceIdentity
                                  << "loadAcknowledgmentRequired" << loadAcknowledgmentRequired;
}

QString ImagePageSurfaceController::publishShadowDisplayImage(
    StaticDisplayImagePayload displayImage)
{
    releaseShadowDisplayEntry();
    if (m_displayImageStore == nullptr || !displayImage.isValid()) {
        return {};
    }

    const QSize rasterSize = displayImage.image.size();
    const QString entryId = m_displayImageStore->insert(DisplayImageEntry {
        displayImage.image,
        displayImage.originalSize,
        rasterSize,
        displayImage.sourceIdentity,
        m_pageRole,
        displayImage.quality,
        DisplayImageRetentionPriority::Nearby,
        0,
        QStringLiteral("shadow-display"),
        displayImage.previewOrigin,
    });
    m_shadowDisplayEntryId = entryId;
    return entryId;
}

void ImagePageSurfaceController::clearShadowDisplayImage() { releaseShadowDisplayEntry(); }

void ImagePageSurfaceController::retainCurrentStaticDisplayImageForSameScopeNavigation()
{
    if (m_currentDisplayEntryIsAnimationFrame || m_displayEntryId.isEmpty()
        || m_displayImageStore == nullptr) {
        return;
    }
    if (m_retainedStillImageEntryId == m_displayEntryId || !m_retainedStillImageEntryId.isEmpty()) {
        return;
    }
    if (!m_displayImageStore->acquirePinLease(
            m_displayEntryId, DisplayImagePinKind::StaleRetained)) {
        return;
    }

    clearStillImageLoadContract();
    m_retainedStillImageEntryId = m_displayEntryId;
    m_displaySource.loadAcknowledgmentRequired = false;
    m_displaySource.retentionStatus = ImageDisplaySourceRetentionStatus::StaleRetained;
    m_displaySource.retainWhileLoadingEligible = false;
    qCDebug(kiriviewDisplayProviderLog)
        << "static display source retained during same-scope navigation"
        << "entryId" << m_retainedStillImageEntryId << "providerUrl" << m_displaySource.providerUrl
        << "revision" << m_displaySource.revision << "sourceIdentity"
        << m_displaySource.sourceIdentity << "pageRole" << static_cast<int>(m_pageRole);
}

void ImagePageSurfaceController::clearSameScopeImageNavigationRetention()
{
    releaseRetainedStillImageEntry();
    m_displaySource.retentionStatus = ImageDisplaySourceRetentionStatus::None;
    m_displaySource.retainWhileLoadingEligible = false;
}

void ImagePageSurfaceController::clearDisplaySource()
{
    if (m_displaySource.providerUrl.isEmpty() && m_displayEntryId.isEmpty()
        && m_retainedStillImageEntryId.isEmpty() && m_retainedAnimationFrameEntryId.isEmpty()
        && m_displaySource.status == ImageDisplaySourceStatus::Missing
        && !m_stillImageDisplayLoadPending && !m_animationFrameDisplayLoadPending) {
        return;
    }

    clearStillImageLoadContract();
    releaseRetainedStillImageEntry();
    releaseRetainedAnimationFrameEntry();
    clearAnimationFrameLoadContract();
    releaseCurrentDisplayEntry();
    m_displaySource = {};
}

void ImagePageSurfaceController::releaseBufferedStaticDisplayEntriesForSource(
    const DisplayImageReuseKey& reuseKey)
{
    if (m_displayImageStore == nullptr) {
        m_bufferedStaticDisplayEntries.clear();
        return;
    }

    auto entry = m_bufferedStaticDisplayEntries.begin();
    while (entry != m_bufferedStaticDisplayEntries.end()) {
        const DisplayImageReuseKey& bufferedKey = entry->reuseKey;
        if (bufferedKey.locationIdentity == reuseKey.locationIdentity
            && bufferedKey.sourceIdentity == reuseKey.sourceIdentity
            && bufferedKey.pageRole == reuseKey.pageRole && bufferedKey != reuseKey) {
            m_displayImageStore->releasePinLease(
                entry->entryId, DisplayImagePinKind::BufferedDisplay);
            entry = m_bufferedStaticDisplayEntries.erase(entry);
            continue;
        }
        ++entry;
    }
}

void ImagePageSurfaceController::retainBufferedStaticDisplayEntry(
    const DisplayImageReuseKey& reuseKey, const QString& entryId)
{
    if (m_displayImageStore == nullptr || entryId.isEmpty()) {
        return;
    }

    auto existing = std::find_if(m_bufferedStaticDisplayEntries.begin(),
        m_bufferedStaticDisplayEntries.end(), [&reuseKey](const BufferedStaticDisplayEntry& entry) {
            return entry.reuseKey == reuseKey;
        });
    if (existing != m_bufferedStaticDisplayEntries.end()) {
        BufferedStaticDisplayEntry retained = *existing;
        m_bufferedStaticDisplayEntries.erase(existing);
        if (retained.entryId != entryId) {
            m_displayImageStore->releasePinLease(
                retained.entryId, DisplayImagePinKind::BufferedDisplay);
            retained.entryId = entryId;
            if (!m_displayImageStore->acquirePinLease(
                    retained.entryId, DisplayImagePinKind::BufferedDisplay)) {
                trimBufferedStaticDisplayEntries();
                return;
            }
        }
        m_bufferedStaticDisplayEntries.push_back(std::move(retained));
        trimBufferedStaticDisplayEntries();
        return;
    }

    if (!m_displayImageStore->acquirePinLease(entryId, DisplayImagePinKind::BufferedDisplay)) {
        return;
    }

    m_bufferedStaticDisplayEntries.push_back(BufferedStaticDisplayEntry {
        reuseKey,
        entryId,
    });
    trimBufferedStaticDisplayEntries();
}

void ImagePageSurfaceController::trimBufferedStaticDisplayEntries()
{
    if (m_displayImageStore == nullptr) {
        m_bufferedStaticDisplayEntries.clear();
        return;
    }

    while (static_cast<qsizetype>(m_bufferedStaticDisplayEntries.size())
        > staticDisplayBufferCapacity) {
        const QString entryId = m_bufferedStaticDisplayEntries.front().entryId;
        m_displayImageStore->releasePinLease(entryId, DisplayImagePinKind::BufferedDisplay);
        m_bufferedStaticDisplayEntries.erase(m_bufferedStaticDisplayEntries.begin());
    }
}

void ImagePageSurfaceController::releaseBufferedStaticDisplayEntries()
{
    if (m_displayImageStore != nullptr) {
        for (const BufferedStaticDisplayEntry& entry : m_bufferedStaticDisplayEntries) {
            m_displayImageStore->releasePinLease(
                entry.entryId, DisplayImagePinKind::BufferedDisplay);
        }
    }
    m_bufferedStaticDisplayEntries.clear();
}

void ImagePageSurfaceController::cancelRasterDisplayRefinement()
{
    if (m_rasterDisplayRefinementDemand.has_value()) {
        qCDebug(kiriviewDisplayProviderLog)
            << "raster display refinement canceled"
            << "sourceIdentity" << m_rasterDisplayRefinementDemand->sourceIdentity << "pageRole"
            << static_cast<int>(m_rasterDisplayRefinementDemand->pageRole)
            << "displaySourceRevision" << m_rasterDisplayRefinementDemand->displaySourceRevision
            << "renderRevision" << m_rasterDisplayRefinementDemand->renderRevision << "bucketSize"
            << m_rasterDisplayRefinementDemand->bucketKey.rasterSize;
    }
    m_rasterDisplayRefinementTicket.invalidate();
    m_rasterDisplayRefinementDemand = std::nullopt;
}

RasterDisplayRefinementCacheKey ImagePageSurfaceController::rasterDisplayRefinementCacheKey(
    const StaticDisplayImagePayload& displayImage,
    const ImagePresentationRenderProjection& projection, const StaticImageDisplaySource& source,
    const RasterDisplayBucketDecision& decision) const
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

bool ImagePageSurfaceController::promoteCachedRasterDisplayRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey,
    const ImageDocumentRenderContext& renderContext)
{
    auto entry = std::find_if(m_cachedRasterDisplayRefinements.begin(),
        m_cachedRasterDisplayRefinements.end(),
        [&cacheKey](const CachedRasterDisplayRefinement& refinement) {
            return refinement.cacheKey == cacheKey;
        });
    if (entry == m_cachedRasterDisplayRefinements.end()) {
        return false;
    }

    CachedRasterDisplayRefinement cached = *entry;
    m_cachedRasterDisplayRefinements.erase(entry);
    m_cachedRasterDisplayRefinements.push_back(cached);
    qCDebug(kiriviewDisplayProviderLog)
        << "raster display refinement cache hit"
        << "sourceIdentity" << cacheKey.sourceIdentity << "pageRole"
        << static_cast<int>(cacheKey.pageRole) << "bucketSize" << cacheKey.bucketKey.rasterSize
        << "exact" << cacheKey.bucketKey.exact << "quality" << static_cast<int>(cacheKey.quality);

    setStaticDisplayImage(std::move(cached.displayImage), isPredecodeCacheable(), renderContext);
    updateDisplaySourceVisibility(true);
    notify(ImageDocumentChange::DisplaySource);
    return true;
}

void ImagePageSurfaceController::retainCachedRasterDisplayRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey, const StaticDisplayImagePayload& displayImage)
{
    auto entry = m_cachedRasterDisplayRefinements.begin();
    while (entry != m_cachedRasterDisplayRefinements.end()) {
        if (entry->cacheKey == cacheKey) {
            entry = m_cachedRasterDisplayRefinements.erase(entry);
            continue;
        }
        ++entry;
    }

    m_cachedRasterDisplayRefinements.push_back(CachedRasterDisplayRefinement {
        cacheKey,
        displayImage,
    });
    while (static_cast<qsizetype>(m_cachedRasterDisplayRefinements.size())
        > rasterDisplayRefinementCacheCapacity) {
        m_cachedRasterDisplayRefinements.erase(m_cachedRasterDisplayRefinements.begin());
    }
}

std::optional<RasterDisplayRefinementDemandKey>
ImagePageSurfaceController::attachInFlightRasterDisplayRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey, RasterDisplayRefinementDemandKey demandKey)
{
    auto entry = std::find_if(m_inFlightRasterDisplayRefinements.begin(),
        m_inFlightRasterDisplayRefinements.end(),
        [&cacheKey](const InFlightRasterDisplayRefinement& refinement) {
            return refinement.cacheKey == cacheKey;
        });
    if (entry == m_inFlightRasterDisplayRefinements.end()) {
        return std::nullopt;
    }

    demandKey.renderRevision = entry->ticket;
    entry->demandKey = demandKey;
    m_rasterDisplayRefinementDemand = demandKey;
    return demandKey;
}

void ImagePageSurfaceController::retainInFlightRasterDisplayRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey,
    const RasterDisplayRefinementDemandKey& demandKey, quint64 ticket,
    std::shared_ptr<std::atomic_bool> startCanceled)
{
    auto entry = m_inFlightRasterDisplayRefinements.begin();
    while (entry != m_inFlightRasterDisplayRefinements.end()) {
        if (entry->cacheKey == cacheKey || entry->ticket == ticket) {
            entry = m_inFlightRasterDisplayRefinements.erase(entry);
            continue;
        }
        ++entry;
    }

    m_inFlightRasterDisplayRefinements.push_back(InFlightRasterDisplayRefinement {
        cacheKey,
        demandKey,
        ticket,
        std::move(startCanceled),
    });
    while (static_cast<qsizetype>(m_inFlightRasterDisplayRefinements.size())
        > rasterDisplayRefinementInFlightCapacity) {
        InFlightRasterDisplayRefinement evicted = m_inFlightRasterDisplayRefinements.front();
        if (evicted.startCanceled != nullptr) {
            evicted.startCanceled->store(true, std::memory_order_relaxed);
        }
        if (m_rasterDisplayRefinementDemand.has_value()
            && *m_rasterDisplayRefinementDemand == evicted.demandKey) {
            m_rasterDisplayRefinementDemand = std::nullopt;
        }
        m_inFlightRasterDisplayRefinements.erase(m_inFlightRasterDisplayRefinements.begin());
    }
}

std::optional<RasterDisplayRefinementDemandKey>
ImagePageSurfaceController::takeInFlightRasterDisplayRefinement(
    const RasterDisplayRefinementCacheKey& cacheKey, quint64 ticket)
{
    auto entry = std::find_if(m_inFlightRasterDisplayRefinements.begin(),
        m_inFlightRasterDisplayRefinements.end(),
        [&cacheKey, ticket](const InFlightRasterDisplayRefinement& refinement) {
            return refinement.cacheKey == cacheKey && refinement.ticket == ticket;
        });
    if (entry == m_inFlightRasterDisplayRefinements.end()) {
        return std::nullopt;
    }

    RasterDisplayRefinementDemandKey demandKey = entry->demandKey;
    m_inFlightRasterDisplayRefinements.erase(entry);
    return demandKey;
}

void ImagePageSurfaceController::releaseCurrentDisplayEntry()
{
    clearStillImageLoadContract();

    if (m_displayImageStore == nullptr || m_displayEntryId.isEmpty()) {
        m_displayEntryId.clear();
        m_displayEntryVisiblePinned = false;
        m_currentDisplayEntryIsAnimationFrame = false;
        return;
    }

    const QString entryId = m_displayEntryId;
    if (m_displayEntryVisiblePinned) {
        m_displayImageStore->releasePinLease(entryId, DisplayImagePinKind::Visible);
    }
    m_displayImageStore->release(entryId);
    m_displayEntryId.clear();
    m_displayEntryVisiblePinned = false;
    m_currentDisplayEntryIsAnimationFrame = false;
}

void ImagePageSurfaceController::releaseShadowDisplayEntry()
{
    if (m_displayImageStore == nullptr || m_shadowDisplayEntryId.isEmpty()) {
        m_shadowDisplayEntryId.clear();
        return;
    }

    m_displayImageStore->release(m_shadowDisplayEntryId);
    m_shadowDisplayEntryId.clear();
}

void ImagePageSurfaceController::releaseRetainedStillImageEntry()
{
    if (m_displayImageStore == nullptr || m_retainedStillImageEntryId.isEmpty()) {
        m_retainedStillImageEntryId.clear();
        return;
    }

    m_displayImageStore->releasePinLease(
        m_retainedStillImageEntryId, DisplayImagePinKind::StaleRetained);
    m_retainedStillImageEntryId.clear();
}

void ImagePageSurfaceController::retainCurrentAnimationFrameEntryForLoad()
{
    releaseRetainedAnimationFrameEntry();
    clearAnimationFrameLoadContract();

    if (!m_currentDisplayEntryIsAnimationFrame || m_displayEntryId.isEmpty()
        || m_displayImageStore == nullptr) {
        qCDebug(kiriviewAnimationLog)
            << "animation frame retention skipped" << "currentIsAnimationFrame"
            << m_currentDisplayEntryIsAnimationFrame << "entryId" << m_displayEntryId << "hasStore"
            << (m_displayImageStore != nullptr);
        releaseCurrentDisplayEntry();
        return;
    }

    const QString entryId = m_displayEntryId;
    if (m_displayEntryVisiblePinned) {
        qCDebug(kiriviewAnimationLog)
            << "animation frame visible pin released for retention" << "entryId" << entryId;
        m_displayImageStore->releasePinLease(entryId, DisplayImagePinKind::Visible);
    }

    const bool retained
        = m_displayImageStore->acquirePinLease(entryId, DisplayImagePinKind::FrameRetention);
    m_displayImageStore->release(entryId);
    m_displayEntryId.clear();
    m_displayEntryVisiblePinned = false;
    m_currentDisplayEntryIsAnimationFrame = false;
    if (retained) {
        m_retainedAnimationFrameEntryId = entryId;
    }
    qCDebug(kiriviewAnimationLog) << "animation frame retention acquired" << "entryId" << entryId
                                  << "retained" << retained;
}

void ImagePageSurfaceController::releaseRetainedAnimationFrameEntry()
{
    if (m_displayImageStore == nullptr || m_retainedAnimationFrameEntryId.isEmpty()) {
        qCDebug(kiriviewAnimationLog)
            << "animation frame retention release skipped" << "entryId"
            << m_retainedAnimationFrameEntryId << "hasStore" << (m_displayImageStore != nullptr);
        m_retainedAnimationFrameEntryId.clear();
        return;
    }

    qCDebug(kiriviewAnimationLog) << "animation frame retention released" << "entryId"
                                  << m_retainedAnimationFrameEntryId;
    m_displayImageStore->releasePinLease(
        m_retainedAnimationFrameEntryId, DisplayImagePinKind::FrameRetention);
    m_retainedAnimationFrameEntryId.clear();
}

void ImagePageSurfaceController::clearStillImageLoadContract()
{
    if (m_displayImageStore != nullptr && m_stillImageDisplayLoadPending
        && !m_pendingStillImageEntryId.isEmpty()) {
        m_displayImageStore->releasePinLease(
            m_pendingStillImageEntryId, DisplayImagePinKind::PendingLoad);
    }

    m_stillImageDisplayLoadPending = false;
    m_pendingStillImageEntryId.clear();
    m_pendingStillImageProviderUrl = QUrl();
    m_pendingStillImageRevision = 0;
    m_pendingStillImageSourceIdentity.clear();
}

void ImagePageSurfaceController::clearAnimationFrameLoadContract()
{
    m_animationFrameDisplayLoadPending = false;
    m_pendingAnimationFrameProviderUrl = QUrl();
    m_pendingAnimationFrameRevision = 0;
    m_pendingAnimationFrameSourceIdentity.clear();
}

bool ImagePageSurfaceController::acknowledgeDisplayImageLoad(const QUrl& providerUrl,
    quint64 revision, const QString& sourceIdentity, ImageDisplayLoadOutcome outcome)
{
    if (m_currentDisplayEntryIsAnimationFrame) {
        return acknowledgeAnimationFrameDisplayLoad(providerUrl, revision, sourceIdentity, outcome);
    }

    return acknowledgeStillImageDisplayLoad(providerUrl, revision, sourceIdentity, outcome);
}

bool ImagePageSurfaceController::acknowledgeStillImageDisplayLoad(const QUrl& providerUrl,
    quint64 revision, const QString& sourceIdentity, ImageDisplayLoadOutcome outcome)
{
    if (m_currentDisplayEntryIsAnimationFrame || !m_stillImageDisplayLoadPending
        || providerUrl != m_pendingStillImageProviderUrl || revision != m_pendingStillImageRevision
        || sourceIdentity != m_pendingStillImageSourceIdentity) {
        qCDebug(kiriviewDisplayProviderLog)
            << "static display load acknowledgment ignored"
            << "providerUrl" << providerUrl << "revision" << revision << "sourceIdentity"
            << sourceIdentity << "outcome" << static_cast<int>(outcome) << "pageRole"
            << static_cast<int>(m_pageRole) << "pending" << m_stillImageDisplayLoadPending
            << "pendingProviderUrl" << m_pendingStillImageProviderUrl << "pendingRevision"
            << m_pendingStillImageRevision << "pendingSourceIdentity"
            << m_pendingStillImageSourceIdentity << "currentIsAnimationFrame"
            << m_currentDisplayEntryIsAnimationFrame;
        return false;
    }

    clearStillImageLoadContract();
    releaseRetainedStillImageEntry();
    m_displaySource.status = displaySourceStatusForLoadOutcome(outcome);
    m_displaySource.loadAcknowledgmentRequired = false;
    m_displaySource.retentionStatus = ImageDisplaySourceRetentionStatus::None;
    m_displaySource.retainWhileLoadingEligible = false;
    qCDebug(kiriviewDisplayProviderLog)
        << "static display load acknowledgment accepted"
        << "providerUrl" << providerUrl << "revision" << revision << "sourceIdentity"
        << sourceIdentity << "outcome" << static_cast<int>(outcome) << "pageRole"
        << static_cast<int>(m_pageRole) << "status" << static_cast<int>(m_displaySource.status);
    notify(ImageDocumentChange::DisplaySource);
    return true;
}

bool ImagePageSurfaceController::acknowledgeAnimationFrameDisplayLoad(const QUrl& providerUrl,
    quint64 revision, const QString& sourceIdentity, ImageDisplayLoadOutcome outcome)
{
    if (!m_currentDisplayEntryIsAnimationFrame || !m_animationFrameDisplayLoadPending
        || providerUrl != m_pendingAnimationFrameProviderUrl
        || revision != m_pendingAnimationFrameRevision
        || sourceIdentity != m_pendingAnimationFrameSourceIdentity) {
        return false;
    }

    clearAnimationFrameLoadContract();
    releaseRetainedAnimationFrameEntry();
    m_displaySource.status = displaySourceStatusForLoadOutcome(outcome);
    m_displaySource.loadAcknowledgmentRequired = false;
    notify(ImageDocumentChange::DisplaySource);
    return true;
}

void ImagePageSurfaceController::updateDisplaySourceVisibility(bool visible)
{
    if (m_displayImageStore == nullptr || m_displayEntryId.isEmpty()) {
        m_displayEntryVisiblePinned = false;
        return;
    }

    if (visible) {
        if (!m_displayEntryVisiblePinned) {
            m_displayEntryVisiblePinned = m_displayImageStore->acquirePinLease(
                m_displayEntryId, DisplayImagePinKind::Visible);
        }
        m_displayImageStore->updatePriority(
            m_displayEntryId, DisplayImageRetentionPriority::Visible);
        return;
    }

    if (m_displayEntryVisiblePinned) {
        m_displayImageStore->releasePinLease(m_displayEntryId, DisplayImagePinKind::Visible);
        m_displayEntryVisiblePinned = false;
    }
    m_displayImageStore->updatePriority(m_displayEntryId, DisplayImageRetentionPriority::Nearby);
}

void ImagePageSurfaceController::scheduleRasterDisplayRefinement(
    const ImagePresentationRenderProjection& projection)
{
    if (!projection.visible || projection.visibleItemRect.isEmpty()) {
        cancelRasterDisplayRefinement();
        return;
    }

    std::optional<StaticDisplayImagePayload> currentDisplay = displayImage();
    if (!currentDisplay.has_value()) {
        cancelRasterDisplayRefinement();
        return;
    }

    std::shared_ptr<StaticImageDisplaySource> source = currentDisplay->refinementSource;
    if (source == nullptr || !source->supportsRasterDisplayRefinement()) {
        cancelRasterDisplayRefinement();
        return;
    }

    const qsizetype displayImageByteBudget = m_displayImageByteBudget;
    const RasterDisplayBucketPolicyInput policyInput {
        currentDisplay->originalSize,
        currentDisplay->image.size(),
        currentDisplay->quality,
        projection.displaySize,
        projection.visibleItemRect,
        projection.devicePixelRatio,
        projection.rotationDegrees,
        projection.maximumTextureSize,
        displayImageByteBudget,
    };
    const RasterDisplayBucketDecision decision = source->isResolutionIndependent()
        ? svgDisplayBucketDecision(policyInput)
        : rasterDisplayBucketDecision(policyInput);
    if (decision.status != RasterDisplayBucketStatus::RefinementNeeded
        || decision.bucketKey.rasterSize == currentDisplay->image.size()) {
        cancelRasterDisplayRefinement();
        return;
    }

    RasterDisplayRefinementDemandKey demandKey = rasterDemandKey(*currentDisplay, projection,
        m_displaySourceRevision, displayImageByteBudget, 0, decision.bucketKey);
    const ImageDocumentRenderContext renderContext = renderContextForProjection(projection);
    const RasterDisplayRefinementCacheKey cacheKey
        = rasterDisplayRefinementCacheKey(*currentDisplay, projection, *source, decision);
    if (m_rasterDisplayRefinementDemand.has_value()) {
        RasterDisplayRefinementDemandKey pendingDemand = *m_rasterDisplayRefinementDemand;
        pendingDemand.renderRevision = 0;
        if (pendingDemand == demandKey) {
            qCDebug(kiriviewDisplayProviderLog)
                << "raster display refinement demand already pending"
                << "sourceIdentity" << demandKey.sourceIdentity << "pageRole"
                << static_cast<int>(demandKey.pageRole) << "displaySourceRevision"
                << demandKey.displaySourceRevision << "bucketSize" << demandKey.bucketKey.rasterSize
                << "quality" << static_cast<int>(decision.quality);
            return;
        }
    }
    if (promoteCachedRasterDisplayRefinement(cacheKey, renderContext)) {
        return;
    }
    std::optional<RasterDisplayRefinementDemandKey> attachedDemand
        = attachInFlightRasterDisplayRefinement(cacheKey, demandKey);
    if (attachedDemand.has_value()) {
        qCDebug(kiriviewDisplayProviderLog)
            << "raster display refinement demand attached to in-flight work"
            << "ticket" << attachedDemand->renderRevision << "sourceIdentity"
            << attachedDemand->sourceIdentity << "pageRole"
            << static_cast<int>(attachedDemand->pageRole) << "displaySourceRevision"
            << attachedDemand->displaySourceRevision << "zoomGeneration"
            << attachedDemand->zoomGeneration << "renderContextGeneration"
            << attachedDemand->renderContextGeneration << "allocationGeneration"
            << attachedDemand->allocationGeneration << "rotationGeneration"
            << attachedDemand->rotationGeneration << "bucketSize"
            << attachedDemand->bucketKey.rasterSize << "exact" << attachedDemand->bucketKey.exact
            << "maximumTextureSize" << attachedDemand->bucketKey.maximumTextureSize
            << "displayImageByteBudget" << attachedDemand->bucketKey.displayImageByteBudget
            << "quality" << static_cast<int>(decision.quality);
        return;
    }

    const quint64 ticket = m_rasterDisplayRefinementTicket.next();
    demandKey.renderRevision = ticket;
    m_rasterDisplayRefinementDemand = demandKey;
    std::shared_ptr<std::atomic_bool> startCanceled = std::make_shared<std::atomic_bool>(false);
    retainInFlightRasterDisplayRefinement(cacheKey, demandKey, ticket, startCanceled);
    qCDebug(kiriviewDisplayProviderLog)
        << "raster display refinement scheduled"
        << "ticket" << ticket << "sourceIdentity" << demandKey.sourceIdentity << "pageRole"
        << static_cast<int>(demandKey.pageRole) << "displaySourceRevision"
        << demandKey.displaySourceRevision << "zoomGeneration" << demandKey.zoomGeneration
        << "renderContextGeneration" << demandKey.renderContextGeneration << "allocationGeneration"
        << demandKey.allocationGeneration << "rotationGeneration" << demandKey.rotationGeneration
        << "bucketSize" << demandKey.bucketKey.rasterSize << "exact" << demandKey.bucketKey.exact
        << "maximumTextureSize" << demandKey.bucketKey.maximumTextureSize
        << "displayImageByteBudget" << demandKey.bucketKey.displayImageByteBudget << "quality"
        << static_cast<int>(decision.quality);
    m_workerScheduler.run(
        m_context,
        [work = RasterDisplayRefinementWork {
             ticket,
             cacheKey,
             demandKey,
             renderContext,
             std::move(*currentDisplay),
             std::move(source),
             std::move(startCanceled),
             decision.bucketKey.rasterSize,
             decision.quality,
        }]() mutable { return runRasterDisplayRefinement(std::move(work)); },
        [this](RasterDisplayRefinementResult result) mutable {
            std::optional<RasterDisplayRefinementDemandKey> currentDemand
                = takeInFlightRasterDisplayRefinement(result.cacheKey, result.ticket);
            if (!currentDemand.has_value()) {
                qCDebug(kiriviewDisplayProviderLog)
                    << "raster display refinement result dropped"
                    << "ticket" << result.ticket << "sourceIdentity"
                    << result.demandKey.sourceIdentity << "pageRole"
                    << static_cast<int>(result.demandKey.pageRole)
                    << "displaySourceRevision" << result.demandKey.displaySourceRevision
                    << "renderRevision" << result.demandKey.renderRevision << "bucketSize"
                    << result.demandKey.bucketKey.rasterSize << "ready" << result.ready
                    << "hasCurrentDemand" << m_rasterDisplayRefinementDemand.has_value();
                return;
            }

            if (result.ready) {
                retainCachedRasterDisplayRefinement(result.cacheKey, result.displayImage);
            }

            if (!m_rasterDisplayRefinementDemand.has_value()
                || *m_rasterDisplayRefinementDemand != *currentDemand) {
                qCDebug(kiriviewDisplayProviderLog)
                    << "raster display refinement result cached for inactive demand"
                    << "ticket" << result.ticket << "sourceIdentity"
                    << result.demandKey.sourceIdentity << "pageRole"
                    << static_cast<int>(result.demandKey.pageRole)
                    << "displaySourceRevision" << result.demandKey.displaySourceRevision
                    << "renderRevision" << result.demandKey.renderRevision << "bucketSize"
                    << result.demandKey.bucketKey.rasterSize << "ready" << result.ready
                    << "hasCurrentDemand" << m_rasterDisplayRefinementDemand.has_value();
                return;
            }

            m_rasterDisplayRefinementDemand = std::nullopt;
            if (!result.ready) {
                qCDebug(kiriviewDisplayProviderLog)
                    << "raster display refinement result failed"
                    << "ticket" << result.ticket << "sourceIdentity"
                    << result.demandKey.sourceIdentity << "pageRole"
                    << static_cast<int>(result.demandKey.pageRole)
                    << "displaySourceRevision" << result.demandKey.displaySourceRevision
                    << "bucketSize" << result.demandKey.bucketKey.rasterSize;
                return;
            }

            qCDebug(kiriviewDisplayProviderLog)
                << "raster display refinement result accepted"
                << "ticket" << result.ticket << "sourceIdentity"
                << result.demandKey.sourceIdentity << "pageRole"
                << static_cast<int>(result.demandKey.pageRole) << "displaySourceRevision"
                << result.demandKey.displaySourceRevision << "bucketSize"
                << result.demandKey.bucketKey.rasterSize << "rasterSize"
                << result.displayImage.image.size() << "quality"
                << static_cast<int>(result.displayImage.quality);
            setStaticDisplayImage(std::move(result.displayImage), isPredecodeCacheable(),
                result.renderContext);
            updateDisplaySourceVisibility(true);
            notify(ImageDocumentChange::DisplaySource);
        });
}

void ImagePageSurfaceController::notify(ImageDocumentChange change)
{
    invokeIfSet(m_callbacks.change, change);
}
}
