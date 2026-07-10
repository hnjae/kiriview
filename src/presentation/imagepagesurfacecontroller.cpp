// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagepagesurfacecontroller.h"

#include "async/imagecallback.h"
#include "presentation/animationlogging.h"
#include "presentation/imageanimationplayer.h"
#include "presentation/imagedisplayentryleasecontroller.h"
#include "presentation/rasterdisplayrefinementcoordinator.h"
#include "rendering/displayproviderlogging.h"
#include "rendering/imagerendering.h"

#include <QDebug>
#include <memory>
#include <optional>
#include <utility>

namespace kiriview {
namespace {
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
}

ImagePageSurfaceController::ImagePageSurfaceController(QObject* context,
    ImagePageSurfaceController::Callbacks callbacks, ImageCacheBudgets cacheBudgets,
    std::shared_ptr<DisplayImageStore> displayImageStore, DisplayedPageRole pageRole,
    ImageWorkerScheduler workerScheduler)
    : m_callbacks(std::move(callbacks))
    , m_predecodeCacheByteBudget(cacheBudgets.predecodeCacheByteBudget)
    , m_pageRole(pageRole)
    , m_displayEntryLeases(std::make_unique<ImageDisplayEntryLeaseController>(
          std::move(displayImageStore), pageRole))
    , m_refinementCoordinator(std::make_unique<RasterDisplayRefinementCoordinator>(context,
          cacheBudgets.displayImageCacheByteBudget, std::move(workerScheduler),
          [this](StaticDisplayImagePayload displayImage,
              const ImageDocumentRenderContext& renderContext) {
              setStaticDisplayImage(std::move(displayImage), isPredecodeCacheable(), renderContext);
              m_displayEntryLeases->updateVisibility(true);
              notify(ImageDocumentChange::DisplaySource);
          }))
{
    m_animationPlayer = std::make_unique<ImageAnimationPlayer>(
        context,
        [this](const QImage& image) { setAnimationFrame(image, m_animationFrameSourceIdentity); },
        [this](
            const QString& errorString) { invokeIfSet(m_callbacks.animationError, errorString); },
        [this]() { m_displayEntryLeases->releaseRetainedAnimationFrame(); });
}

ImagePageSurfaceController::~ImagePageSurfaceController() = default;

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
    m_refinementCoordinator->cancel();
    clearShadowDisplayImage();
    clearDisplaySource();
    m_displayEntryLeases->clearBufferedStaticDisplays();
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
    m_refinementCoordinator->cancel();
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
    m_refinementCoordinator->cancel();
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
        m_refinementCoordinator->cancel();
        m_displayEntryLeases->updateVisibility(false);
        return;
    }

    m_displayEntryLeases->updateVisibility(true);
    if (!m_displayImage.has_value()) {
        m_refinementCoordinator->cancel();
        return;
    }
    m_refinementCoordinator->request(*m_displayImage, projection, m_displaySourceRevision);
}

void ImagePageSurfaceController::clearImage()
{
    m_refinementCoordinator->cancel();
    stopAnimation();
    clearShadowDisplayImage();
    clearDisplaySource();
    m_displayEntryLeases->clearBufferedStaticDisplays();
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

void ImagePageSurfaceController::publishDisplaySource(const StaticDisplayImagePayload& displayImage)
{
    m_animationFrameSourceIdentity.clear();
    ++m_displaySourceRevision;
    const QSize rasterSize = displayImage.image.size();
    const DisplayEntryLease lease
        = m_displayEntryLeases->acquireStaticDisplay(displayImage, m_displaySourceRevision);
    m_displaySource = ImageDisplaySourceSlot {
        lease.providerUrl,
        m_displaySourceRevision,
        displayImage.sourceIdentity,
        displayImage.originalSize,
        rasterSize,
        rasterSize != displayImage.originalSize ? QSize(rasterSize.width(), 0) : QSize(),
        displayImage.quality,
        lease.entryId.isEmpty() ? ImageDisplaySourceStatus::Error : ImageDisplaySourceStatus::Ready,
        false,
        lease.loadAcknowledgmentRequired,
        lease.retainedReplacement ? ImageDisplaySourceRetentionStatus::StaleRetained
                                  : ImageDisplaySourceRetentionStatus::None,
        lease.retainedReplacement && lease.loadAcknowledgmentRequired,
    };
    qCDebug(kiriviewDisplayProviderLog)
        << "static display source published"
        << "providerUrl" << lease.providerUrl << "revision" << m_displaySourceRevision << "entryId"
        << lease.entryId << "sourceIdentity" << displayImage.sourceIdentity << "pageRole"
        << static_cast<int>(m_pageRole) << "originalSize" << displayImage.originalSize
        << "rasterSize" << rasterSize << "sourceSizeHint" << m_displaySource.sourceSizeHint
        << "quality" << static_cast<int>(displayImage.quality) << "previewOrigin"
        << static_cast<int>(displayImage.previewOrigin) << "loadAcknowledgmentRequired"
        << lease.loadAcknowledgmentRequired << "retainedReplacement" << lease.retainedReplacement;
}

void ImagePageSurfaceController::publishAnimationFrameDisplaySource(
    const QImage& image, const QString& sourceIdentity)
{
    ++m_displaySourceRevision;
    const QSize rasterSize = image.size();
    const DisplayEntryLease lease = m_displayEntryLeases->acquireAnimationFrame(
        image, sourceIdentity, m_displaySourceRevision);
    m_displaySource = ImageDisplaySourceSlot {
        lease.providerUrl,
        m_displaySourceRevision,
        sourceIdentity,
        rasterSize,
        rasterSize,
        {},
        DisplayImageQuality::Exact,
        lease.entryId.isEmpty() ? ImageDisplaySourceStatus::Error : ImageDisplaySourceStatus::Ready,
        false,
        lease.loadAcknowledgmentRequired,
        ImageDisplaySourceRetentionStatus::None,
        false,
    };
    qCDebug(kiriviewAnimationLog) << "animation frame provider source published" << "providerUrl"
                                  << lease.providerUrl << "revision" << m_displaySourceRevision
                                  << "entryId" << lease.entryId << "rasterSize" << rasterSize
                                  << "sourceIdentity" << sourceIdentity
                                  << "loadAcknowledgmentRequired"
                                  << lease.loadAcknowledgmentRequired;
}

QString ImagePageSurfaceController::publishShadowDisplayImage(
    StaticDisplayImagePayload displayImage)
{
    return m_displayEntryLeases->acquireShadowDisplay(displayImage);
}

void ImagePageSurfaceController::clearShadowDisplayImage()
{
    m_displayEntryLeases->clearShadowDisplay();
}

void ImagePageSurfaceController::retainCurrentStaticDisplayImageForSameScopeNavigation()
{
    if (!m_displayEntryLeases->retainCurrentStaticDisplayForSameScopeNavigation()) {
        return;
    }
    m_displaySource.loadAcknowledgmentRequired = false;
    m_displaySource.retentionStatus = ImageDisplaySourceRetentionStatus::StaleRetained;
    m_displaySource.retainWhileLoadingEligible = false;
}

void ImagePageSurfaceController::clearSameScopeImageNavigationRetention()
{
    m_displayEntryLeases->clearSameScopeImageNavigationRetention();
    m_displaySource.retentionStatus = ImageDisplaySourceRetentionStatus::None;
    m_displaySource.retainWhileLoadingEligible = false;
}

void ImagePageSurfaceController::clearDisplaySource()
{
    m_displayEntryLeases->clearDisplay();
    m_displaySource = {};
}

bool ImagePageSurfaceController::acknowledgeDisplayImageLoad(const QUrl& providerUrl,
    quint64 revision, const QString& sourceIdentity, ImageDisplayLoadOutcome outcome)
{
    if (m_displayEntryLeases->currentDisplayIsAnimationFrame()) {
        return acknowledgeAnimationFrameDisplayLoad(providerUrl, revision, sourceIdentity, outcome);
    }
    return acknowledgeStillImageDisplayLoad(providerUrl, revision, sourceIdentity, outcome);
}

bool ImagePageSurfaceController::acknowledgeStillImageDisplayLoad(const QUrl& providerUrl,
    quint64 revision, const QString& sourceIdentity, ImageDisplayLoadOutcome outcome)
{
    if (!m_displayEntryLeases->acknowledgeStillDisplayLoad(providerUrl, revision, sourceIdentity)) {
        return false;
    }

    m_displaySource.status = displaySourceStatusForLoadOutcome(outcome);
    m_displaySource.loadAcknowledgmentRequired = false;
    m_displaySource.retentionStatus = ImageDisplaySourceRetentionStatus::None;
    m_displaySource.retainWhileLoadingEligible = false;
    notify(ImageDocumentChange::DisplaySource);
    return true;
}

bool ImagePageSurfaceController::acknowledgeAnimationFrameDisplayLoad(const QUrl& providerUrl,
    quint64 revision, const QString& sourceIdentity, ImageDisplayLoadOutcome outcome)
{
    if (!m_displayEntryLeases->acknowledgeAnimationFrameDisplayLoad(
            providerUrl, revision, sourceIdentity)) {
        return false;
    }

    m_displaySource.status = displaySourceStatusForLoadOutcome(outcome);
    m_displaySource.loadAcknowledgmentRequired = false;
    notify(ImageDocumentChange::DisplaySource);
    return true;
}

void ImagePageSurfaceController::notify(ImageDocumentChange change)
{
    invokeIfSet(m_callbacks.change, change);
}
}
