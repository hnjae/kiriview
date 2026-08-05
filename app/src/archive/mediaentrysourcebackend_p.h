// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCEBACKEND_P_H
#define KIRIVIEW_MEDIAENTRYSOURCEBACKEND_P_H

#include "location/imagelocation.h"
#include "mediaentrysourcebackend.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

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

class MediaEntrySourceWithCandidateSnapshot : public MediaEntrySource
{
public:
    MediaEntrySourceWithCandidateSnapshot(OpenedCollectionScopeLocation openedCollectionScope,
        MediaEntrySourceBackendKind backend, std::vector<ImageDocumentPageCandidate> candidates);
    ~MediaEntrySourceWithCandidateSnapshot() override = default;

    MediaEntrySourceCandidatesResult loadImageDocumentPageCandidates() final;
    MediaEntrySourceImageDataResult loadImageData(
        const QUrl& imageUrl, ImageSourceDataLease lease = {}) final;
    MediaEntrySourceVideoPlaybackDeviceResult loadVideoPlaybackDevice(const QUrl& videoUrl) final;
    MediaEntrySourceThumbnailMetadataResult loadThumbnailMetadata(const QUrl& imageUrl) final;

protected:
    [[nodiscard]] const OpenedCollectionScopeLocation& openedCollectionScope() const;

    virtual MediaEntrySourceImageDataResult loadAuthorizedImageData(
        const ImageDocumentPageCandidate& candidate, ImageSourceDataLease lease)
        = 0;
    virtual MediaEntrySourceVideoPlaybackDeviceResult loadAuthorizedVideoPlaybackDevice(
        const ImageDocumentPageCandidate& candidate);
    virtual MediaEntrySourceThumbnailMetadataResult loadAuthorizedThumbnailMetadata(
        const ImageDocumentPageCandidate& candidate);

private:
    [[nodiscard]] const ImageDocumentPageCandidate* authorizedCandidate(
        const QUrl& url, ImageDocumentPageKind expectedKind) const;
    [[nodiscard]] QString rejectedSelectorEntryPath(const QUrl& url) const;

    OpenedCollectionScopeLocation m_openedCollectionScope;
    MediaEntrySourceBackendKind m_backend = MediaEntrySourceBackendKind::Unknown;
    std::vector<ImageDocumentPageCandidate> m_candidates;
    Q_DISABLE_COPY_MOVE(MediaEntrySourceWithCandidateSnapshot)
};

std::optional<ImageDocumentPageCandidate> openedCollectionImageDocumentPageCandidate(
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

MediaEntrySourceCandidatesResult mediaEntrySourceCandidatesResult(
    std::vector<ImageDocumentPageCandidate> candidates);
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
