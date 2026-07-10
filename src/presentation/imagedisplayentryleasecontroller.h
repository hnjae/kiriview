// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDISPLAYENTRYLEASECONTROLLER_H
#define KIRIVIEW_IMAGEDISPLAYENTRYLEASECONTROLLER_H

#include "rendering/displayimagestore.h"
#include "rendering/staticimage.h"

#include <QImage>
#include <QString>
#include <QUrl>
#include <memory>
#include <vector>

namespace kiriview {
struct DisplayEntryLease
{
    QString entryId;
    QUrl providerUrl;
    bool loadAcknowledgmentRequired = false;
    bool retainedReplacement = false;
};

class ImageDisplayEntryLeaseController final
{
public:
    ImageDisplayEntryLeaseController(std::shared_ptr<DisplayImageStore> displayImageStore,
        DisplayedPageRole pageRole = DisplayedPageRole::Primary);
    ~ImageDisplayEntryLeaseController();

    DisplayEntryLease acquireStaticDisplay(
        const StaticDisplayImagePayload& displayImage, quint64 displaySourceRevision);
    DisplayEntryLease acquireAnimationFrame(
        const QImage& image, const QString& sourceIdentity, quint64 displaySourceRevision);
    QString acquireShadowDisplay(const StaticDisplayImagePayload& displayImage);
    void clearShadowDisplay();
    bool retainCurrentStaticDisplayForSameScopeNavigation();
    void clearSameScopeImageNavigationRetention();
    void clearDisplay();
    void clearBufferedStaticDisplays();
    void updateVisibility(bool visible);
    bool acknowledgeStillDisplayLoad(
        const QUrl& providerUrl, quint64 revision, const QString& sourceIdentity);
    bool acknowledgeAnimationFrameDisplayLoad(
        const QUrl& providerUrl, quint64 revision, const QString& sourceIdentity);
    bool currentDisplayIsAnimationFrame() const;
    void releaseRetainedAnimationFrame();

private:
    struct BufferedStaticDisplayEntry
    {
        DisplayImageReuseKey reuseKey;
        QString entryId;
    };

    DisplayImageReuseKey staticDisplayReuseKey(const StaticDisplayImagePayload& displayImage) const;
    void releaseBufferedStaticDisplaysForSource(const DisplayImageReuseKey& reuseKey);
    void retainBufferedStaticDisplay(const DisplayImageReuseKey& reuseKey, const QString& entryId);
    void trimBufferedStaticDisplays();
    void releaseCurrentDisplay();
    void releaseRetainedStillDisplay();
    void retainCurrentAnimationFrameForLoad();
    void clearStillImageLoadContract();
    void clearAnimationFrameLoadContract();

    std::shared_ptr<DisplayImageStore> m_displayImageStore;
    DisplayedPageRole m_pageRole = DisplayedPageRole::Primary;
    QString m_displayEntryId;
    QString m_shadowDisplayEntryId;
    QString m_pendingStillImageEntryId;
    QString m_retainedStillImageEntryId;
    QString m_retainedAnimationFrameEntryId;
    QUrl m_pendingStillImageProviderUrl;
    quint64 m_pendingStillImageRevision = 0;
    QString m_pendingStillImageSourceIdentity;
    QUrl m_pendingAnimationFrameProviderUrl;
    quint64 m_pendingAnimationFrameRevision = 0;
    QString m_pendingAnimationFrameSourceIdentity;
    bool m_displayEntryVisiblePinned = false;
    bool m_currentDisplayEntryIsAnimationFrame = false;
    bool m_stillImageDisplayLoadPending = false;
    bool m_animationFrameDisplayLoadPending = false;
    std::vector<BufferedStaticDisplayEntry> m_bufferedStaticDisplayEntries;
};
}

#endif
