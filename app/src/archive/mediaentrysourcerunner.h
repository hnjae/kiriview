// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCERUNNER_H
#define KIRIVIEW_MEDIAENTRYSOURCERUNNER_H

#include "mediaentrysourcebackend.h"

#include <mutex>
#include <optional>
#include <vector>

namespace kiriview {
class MediaEntrySourceRunner final
{
public:
    MediaEntrySourceRunner(
        OpenedCollectionScopeLocation openedCollectionScope, MediaEntrySourceFactory sourceFactory);

    [[nodiscard]] const OpenedCollectionScopeLocation& openedCollectionScope() const;

    MediaEntrySourceEntriesResult loadEntries(const MediaEntrySourceOpenContext& context = {});
    MediaEntrySourceImageDataResult loadImageData(
        const QUrl& imageUrl, ImageSourceDataLease lease = {});
    MediaEntrySourceVideoPlaybackDeviceResult loadVideoPlaybackDevice(const QUrl& videoUrl);
    std::optional<std::vector<MediaEntrySourceEntry>> cachedEntries();

private:
    std::optional<MediaEntrySourceError> ensureSource(
        const MediaEntrySourceOpenContext& context = {});

    OpenedCollectionScopeLocation m_openedCollectionScope;
    MediaEntrySourceFactory m_sourceFactory;
    MediaEntrySourcePtr m_source;
    std::optional<MediaEntrySourceError> m_openError;
    std::optional<std::vector<MediaEntrySourceEntry>> m_cachedEntries;
    bool m_openAttempted = false;
    std::mutex m_mutex;
};
}

#endif
