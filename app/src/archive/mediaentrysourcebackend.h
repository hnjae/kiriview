// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCEBACKEND_H
#define KIRIVIEW_MEDIAENTRYSOURCEBACKEND_H

#include "decoding/imagesourcedata.h"
#include "location/imagelocation.h"
#include "mediaentrysourceerror.h"
#include "navigation/imagedocumentpagenavigationtypes.h"

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <expected>
#include <functional>
#include <memory>
#include <stop_token>
#include <vector>

namespace kiriview {
class MediaEntrySource;
using MediaEntrySourcePtr = std::shared_ptr<MediaEntrySource>;

struct MediaEntrySourceCandidates
{
    std::vector<ImageDocumentPageCandidate> candidates;
};

struct MediaEntrySourceImageData
{
    QByteArray data;
    ImageSourceDataLease lease;
};

struct MediaEntrySourceVideoPlaybackDevice
{
    MediaEntrySourcePtr sourceOwner;
    std::unique_ptr<QIODevice> device;
};

enum class MediaEntryContentChecksumAlgorithm {
    Crc32,
};

struct MediaEntryContentChecksum
{
    MediaEntryContentChecksumAlgorithm algorithm = MediaEntryContentChecksumAlgorithm::Crc32;
    quint64 value = 0;
};

struct MediaEntrySourceThumbnailMetadata
{
    MediaEntryContentChecksum checksum;
    qint64 uncompressedSize = -1;
};

template <typename Value>
using MediaEntrySourceResult = std::expected<Value, MediaEntrySourceError>;
using MediaEntrySourceCandidatesResult = MediaEntrySourceResult<MediaEntrySourceCandidates>;
using MediaEntrySourceImageDataResult = MediaEntrySourceResult<MediaEntrySourceImageData>;
using MediaEntrySourceVideoPlaybackDeviceResult
    = MediaEntrySourceResult<MediaEntrySourceVideoPlaybackDevice>;
using MediaEntrySourceThumbnailMetadataResult
    = MediaEntrySourceResult<MediaEntrySourceThumbnailMetadata>;

template <typename Value>
const Value* mediaEntrySourceResultValue(const MediaEntrySourceResult<Value>& result)
{
    return result ? &*result : nullptr;
}

template <typename Value> Value* mediaEntrySourceResultValue(MediaEntrySourceResult<Value>& result)
{
    return result ? &*result : nullptr;
}

template <typename Value>
const MediaEntrySourceError* mediaEntrySourceResultError(
    const MediaEntrySourceResult<Value>& result)
{
    return result ? nullptr : &result.error();
}

template <typename Value>
MediaEntrySourceError* mediaEntrySourceResultError(MediaEntrySourceResult<Value>& result)
{
    return result ? nullptr : &result.error();
}

class MediaEntrySource
{
public:
    MediaEntrySource() = default;
    virtual ~MediaEntrySource() = default;

    virtual MediaEntrySourceCandidatesResult loadImageDocumentPageCandidates() = 0;
    virtual MediaEntrySourceImageDataResult loadImageData(
        const QUrl& imageUrl, ImageSourceDataLease lease = {})
        = 0;
    virtual MediaEntrySourceVideoPlaybackDeviceResult loadVideoPlaybackDevice(const QUrl& videoUrl);
    virtual MediaEntrySourceThumbnailMetadataResult loadThumbnailMetadata(const QUrl& imageUrl);
    Q_DISABLE_COPY_MOVE(MediaEntrySource)
};

using MediaEntrySourceOpenResult = MediaEntrySourceResult<MediaEntrySourcePtr>;

struct MediaEntrySourceEnumerationLimits
{
    qsizetype maximumEntryCount = 0;
    qsizetype maximumPathCodeUnitCount = 0;
    int maximumNestingDepth = 0;
};

MediaEntrySourceEnumerationLimits defaultMediaEntrySourceEnumerationLimits();

struct MediaEntrySourceOpenContext
{
    std::stop_token stopToken;
    MediaEntrySourceEnumerationLimits enumerationLimits
        = defaultMediaEntrySourceEnumerationLimits();
};

using MediaEntrySourceFactory = std::function<MediaEntrySourceOpenResult(
    const OpenedCollectionScopeLocation&, const MediaEntrySourceOpenContext&)>;

MediaEntrySourceCandidatesResult loadMediaEntrySourceCandidates(
    const OpenedCollectionScopeLocation& openedCollectionScope);
MediaEntrySourceImageDataResult loadMediaEntrySourceImageData(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& imageUrl,
    ImageSourceDataLease lease = {});
MediaEntrySourceThumbnailMetadataResult loadMediaEntrySourceThumbnailMetadata(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& imageUrl);
MediaEntrySourceOpenResult openMediaEntrySource(
    const OpenedCollectionScopeLocation& openedCollectionScope,
    const MediaEntrySourceOpenContext& context = {});
}

#endif
