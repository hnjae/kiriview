// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagedisplayentryleasecontroller.h"

#include "rendering/displayproviderlogging.h"

#include <QDebug>
#include <algorithm>
#include <utility>

namespace kiriview {
namespace {
    constexpr qsizetype staticDisplayBufferCapacity = 2;
}

ImageDisplayEntryLeaseController::ImageDisplayEntryLeaseController(
    std::shared_ptr<DisplayImageStore> displayImageStore, DisplayedPageRole pageRole)
    : m_displayImageStore(
          displayImageStore == nullptr ? sharedDisplayImageStore() : std::move(displayImageStore))
    , m_pageRole(pageRole)
{
}

ImageDisplayEntryLeaseController::~ImageDisplayEntryLeaseController()
{
    clearShadowDisplay();
    releaseRetainedStillDisplay();
    releaseRetainedAnimationFrame();
    clearBufferedStaticDisplays();
    releaseCurrentDisplay();
}

DisplayEntryLease ImageDisplayEntryLeaseController::acquireStaticDisplay(
    const StaticDisplayImagePayload& displayImage, quint64 displaySourceRevision)
{
    releaseRetainedAnimationFrame();
    clearAnimationFrameLoadContract();
    releaseCurrentDisplay();

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
                  displaySourceRevision,
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
    m_currentDisplayEntryIsAnimationFrame = false;
    m_pendingStillImageEntryId = loadAcknowledgmentRequired ? entryId : QString();
    m_pendingStillImageProviderUrl = loadAcknowledgmentRequired ? providerUrl : QUrl();
    m_pendingStillImageRevision = loadAcknowledgmentRequired ? displaySourceRevision : 0;
    m_pendingStillImageSourceIdentity
        = loadAcknowledgmentRequired ? displayImage.sourceIdentity : QString();
    m_stillImageDisplayLoadPending = loadAcknowledgmentRequired;
    releaseBufferedStaticDisplaysForSource(reuseKey);
    retainBufferedStaticDisplay(reuseKey, entryId);

    return DisplayEntryLease {
        entryId,
        providerUrl,
        loadAcknowledgmentRequired,
        retainedReplacement,
    };
}

DisplayEntryLease ImageDisplayEntryLeaseController::acquireAnimationFrame(
    const QImage& image, const QString& sourceIdentity, quint64 displaySourceRevision)
{
    retainCurrentAnimationFrameForLoad();

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
              displaySourceRevision,
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
    m_pendingAnimationFrameRevision = displaySourceRevision;
    m_pendingAnimationFrameSourceIdentity = sourceIdentity;

    return DisplayEntryLease {
        entryId,
        providerUrl,
        loadAcknowledgmentRequired,
        false,
    };
}

QString ImageDisplayEntryLeaseController::acquireShadowDisplay(
    const StaticDisplayImagePayload& displayImage)
{
    clearShadowDisplay();
    if (m_displayImageStore == nullptr || !displayImage.isValid()) {
        return {};
    }

    const QSize rasterSize = displayImage.image.size();
    m_shadowDisplayEntryId = m_displayImageStore->insert(DisplayImageEntry {
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
    return m_shadowDisplayEntryId;
}

void ImageDisplayEntryLeaseController::clearShadowDisplay()
{
    if (m_displayImageStore != nullptr && !m_shadowDisplayEntryId.isEmpty()) {
        m_displayImageStore->release(m_shadowDisplayEntryId);
    }
    m_shadowDisplayEntryId.clear();
}

bool ImageDisplayEntryLeaseController::retainCurrentStaticDisplayForSameScopeNavigation()
{
    if (m_currentDisplayEntryIsAnimationFrame || m_displayEntryId.isEmpty()
        || m_displayImageStore == nullptr || m_retainedStillImageEntryId == m_displayEntryId
        || !m_retainedStillImageEntryId.isEmpty()
        || !m_displayImageStore->acquirePinLease(
            m_displayEntryId, DisplayImagePinKind::StaleRetained)) {
        return false;
    }

    clearStillImageLoadContract();
    m_retainedStillImageEntryId = m_displayEntryId;
    return true;
}

void ImageDisplayEntryLeaseController::clearSameScopeImageNavigationRetention()
{
    releaseRetainedStillDisplay();
}

void ImageDisplayEntryLeaseController::clearDisplay()
{
    clearStillImageLoadContract();
    releaseRetainedStillDisplay();
    releaseRetainedAnimationFrame();
    clearAnimationFrameLoadContract();
    releaseCurrentDisplay();
}

void ImageDisplayEntryLeaseController::clearBufferedStaticDisplays()
{
    if (m_displayImageStore != nullptr) {
        for (const BufferedStaticDisplayEntry& entry : m_bufferedStaticDisplayEntries) {
            m_displayImageStore->releasePinLease(
                entry.entryId, DisplayImagePinKind::BufferedDisplay);
        }
    }
    m_bufferedStaticDisplayEntries.clear();
}

void ImageDisplayEntryLeaseController::updateVisibility(bool visible)
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

bool ImageDisplayEntryLeaseController::acknowledgeStillDisplayLoad(
    const QUrl& providerUrl, quint64 revision, const QString& sourceIdentity)
{
    if (m_currentDisplayEntryIsAnimationFrame || !m_stillImageDisplayLoadPending
        || providerUrl != m_pendingStillImageProviderUrl || revision != m_pendingStillImageRevision
        || sourceIdentity != m_pendingStillImageSourceIdentity) {
        return false;
    }

    clearStillImageLoadContract();
    releaseRetainedStillDisplay();
    return true;
}

bool ImageDisplayEntryLeaseController::acknowledgeAnimationFrameDisplayLoad(
    const QUrl& providerUrl, quint64 revision, const QString& sourceIdentity)
{
    if (!m_currentDisplayEntryIsAnimationFrame || !m_animationFrameDisplayLoadPending
        || providerUrl != m_pendingAnimationFrameProviderUrl
        || revision != m_pendingAnimationFrameRevision
        || sourceIdentity != m_pendingAnimationFrameSourceIdentity) {
        return false;
    }

    clearAnimationFrameLoadContract();
    releaseRetainedAnimationFrame();
    return true;
}

bool ImageDisplayEntryLeaseController::currentDisplayIsAnimationFrame() const
{
    return m_currentDisplayEntryIsAnimationFrame;
}

DisplayImageReuseKey ImageDisplayEntryLeaseController::staticDisplayReuseKey(
    const StaticDisplayImagePayload& displayImage) const
{
    return DisplayImageReuseKey {
        displayImage.displayScopeIdentity,
        displayImage.sourceIdentity,
        displayImage.imageReaderTransform.transformations,
        displayImage.originalSize,
        displayImage.image.size(),
        displayImage.quality,
        displayImage.previewOrigin,
        m_pageRole,
    };
}

void ImageDisplayEntryLeaseController::releaseBufferedStaticDisplaysForSource(
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

void ImageDisplayEntryLeaseController::retainBufferedStaticDisplay(
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
                trimBufferedStaticDisplays();
                return;
            }
        }
        m_bufferedStaticDisplayEntries.push_back(std::move(retained));
        trimBufferedStaticDisplays();
        return;
    }

    if (!m_displayImageStore->acquirePinLease(entryId, DisplayImagePinKind::BufferedDisplay)) {
        return;
    }

    m_bufferedStaticDisplayEntries.push_back(BufferedStaticDisplayEntry { reuseKey, entryId });
    trimBufferedStaticDisplays();
}

void ImageDisplayEntryLeaseController::trimBufferedStaticDisplays()
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

void ImageDisplayEntryLeaseController::releaseCurrentDisplay()
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

void ImageDisplayEntryLeaseController::releaseRetainedStillDisplay()
{
    if (m_displayImageStore != nullptr && !m_retainedStillImageEntryId.isEmpty()) {
        m_displayImageStore->releasePinLease(
            m_retainedStillImageEntryId, DisplayImagePinKind::StaleRetained);
    }
    m_retainedStillImageEntryId.clear();
}

void ImageDisplayEntryLeaseController::retainCurrentAnimationFrameForLoad()
{
    releaseRetainedAnimationFrame();
    clearAnimationFrameLoadContract();

    if (!m_currentDisplayEntryIsAnimationFrame || m_displayEntryId.isEmpty()
        || m_displayImageStore == nullptr) {
        releaseCurrentDisplay();
        return;
    }

    const QString entryId = m_displayEntryId;
    if (m_displayEntryVisiblePinned) {
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
}

void ImageDisplayEntryLeaseController::releaseRetainedAnimationFrame()
{
    if (m_displayImageStore != nullptr && !m_retainedAnimationFrameEntryId.isEmpty()) {
        m_displayImageStore->releasePinLease(
            m_retainedAnimationFrameEntryId, DisplayImagePinKind::FrameRetention);
    }
    m_retainedAnimationFrameEntryId.clear();
}

void ImageDisplayEntryLeaseController::clearStillImageLoadContract()
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

void ImageDisplayEntryLeaseController::clearAnimationFrameLoadContract()
{
    m_animationFrameDisplayLoadPending = false;
    m_pendingAnimationFrameProviderUrl = QUrl();
    m_pendingAnimationFrameRevision = 0;
    m_pendingAnimationFrameSourceIdentity.clear();
}
}
