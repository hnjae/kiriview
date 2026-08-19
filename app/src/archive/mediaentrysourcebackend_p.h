// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCEBACKEND_P_H
#define KIRIVIEW_MEDIAENTRYSOURCEBACKEND_P_H

#include "location/imagelocation.h"
#include "mediaentrysourcebackend.h"

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <optional>
#include <utility>
#include <vector>

namespace kiriview::MediaEntrySourceBackendDetail {
using MediaEntrySourceOpener = MediaEntrySourceOpenResult (*)(
    const OpenedCollectionScopeLocation&, const MediaEntrySourceOpenContext&);

enum class MediaEntrySourceEnumerationFailure {
    ResourceLimitExceeded,
    OperationCancelled,
};

class MediaEntrySourceEnumerationBudget final
{
public:
    explicit MediaEntrySourceEnumerationBudget(const MediaEntrySourceOpenContext& context);

    [[nodiscard]] std::expected<void, MediaEntrySourceEnumerationFailure> checkpoint() const;
    std::expected<void, MediaEntrySourceEnumerationFailure> admitEntry(
        qsizetype pathCodeUnitCount, int nestingDepth);

private:
    std::stop_token m_stopToken;
    MediaEntrySourceEnumerationLimits m_limits;
    qsizetype m_entryCount = 0;
    qsizetype m_pathCodeUnitCount = 0;
};

struct MediaEntrySourceBackendOperations
{
    MediaEntrySourceOpener openSource;
};

class MediaEntrySourceWithEntrySnapshot : public MediaEntrySource
{
public:
    MediaEntrySourceWithEntrySnapshot(OpenedCollectionScopeLocation openedCollectionScope,
        MediaEntrySourceBackendKind backend, std::vector<MediaEntrySourceEntry> entries);
    ~MediaEntrySourceWithEntrySnapshot() override = default;

    MediaEntrySourceEntriesResult loadEntries() final;
    MediaEntrySourceImageDataResult loadImageData(
        const QUrl& imageUrl, ImageSourceDataLease lease = {}) final;
    MediaEntrySourceVideoPlaybackDeviceResult loadVideoPlaybackDevice(const QUrl& videoUrl) final;
    MediaEntrySourceThumbnailMetadataResult loadThumbnailMetadata(const QUrl& imageUrl) final;

protected:
    [[nodiscard]] const OpenedCollectionScopeLocation& openedCollectionScope() const;

    virtual MediaEntrySourceImageDataResult loadAuthorizedImageData(
        const MediaEntrySourceEntry& entry, ImageSourceDataLease lease)
        = 0;
    virtual MediaEntrySourceVideoPlaybackDeviceResult loadAuthorizedVideoPlaybackDevice(
        const MediaEntrySourceEntry& entry);
    virtual MediaEntrySourceThumbnailMetadataResult loadAuthorizedThumbnailMetadata(
        const MediaEntrySourceEntry& entry);

private:
    [[nodiscard]] const MediaEntrySourceEntry* authorizedEntry(
        const QUrl& url, MediaEntrySourceEntryKind expectedKind) const;
    [[nodiscard]] QString rejectedSelectorEntryPath(const QUrl& url) const;

    OpenedCollectionScopeLocation m_openedCollectionScope;
    MediaEntrySourceBackendKind m_backend = MediaEntrySourceBackendKind::Unknown;
    std::vector<MediaEntrySourceEntry> m_entries;
    Q_DISABLE_COPY_MOVE(MediaEntrySourceWithEntrySnapshot)
};

std::optional<MediaEntrySourceEntry> openedCollectionMediaEntry(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath);

MediaEntrySourceError mediaEntrySourceError(MediaEntrySourceErrorCause cause,
    MediaEntrySourceBackendKind backend, MediaEntrySourceOperation operation,
    const OpenedCollectionScopeLocation& openedCollectionScope,
    QString diagnosticDetail = QString(), QString entryPath = QString());
MediaEntrySourceError mediaEntrySourceEnumerationError(MediaEntrySourceEnumerationFailure failure,
    MediaEntrySourceBackendKind backend,
    const OpenedCollectionScopeLocation& openedCollectionScope);

template <typename Result> Result mediaEntrySourceErrorResult(MediaEntrySourceError error)
{
    return std::unexpected(std::move(error));
}

MediaEntrySourceEntriesResult mediaEntrySourceEntriesResult(
    std::vector<MediaEntrySourceEntry> entries);
MediaEntrySourceImageDataResult mediaEntrySourceImageDataResult(QByteArray data);
MediaEntrySourceImageDataResult mediaEntrySourceImageDataResult(ImageSourceData sourceData);
MediaEntrySourceVideoPlaybackDeviceResult mediaEntrySourceVideoPlaybackDeviceResult(
    std::unique_ptr<QIODevice> device, MediaEntrySourcePtr sourceOwner = {});
MediaEntrySourceThumbnailMetadataResult mediaEntrySourceThumbnailMetadataResult(
    MediaEntrySourceThumbnailMetadata metadata);

const MediaEntrySourceBackendOperations* kArchiveMediaEntrySourceBackendOperations();
const MediaEntrySourceBackendOperations* libArchiveMediaEntrySourceBackendOperations();
const MediaEntrySourceBackendOperations* directoryCollectionMediaEntrySourceBackendOperations();
}

#endif
