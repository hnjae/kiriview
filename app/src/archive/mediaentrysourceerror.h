// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_MEDIAENTRYSOURCEERROR_H
#define KIRIVIEW_MEDIAENTRYSOURCEERROR_H

#include <QString>
#include <QUrl>
#include <functional>

class QDebug;

namespace kiriview {
enum class MediaEntrySourceBackendKind {
    Unknown,
    Unsupported,
    Directory,
    KArchive,
    LibArchive,
};

enum class MediaEntrySourceOperation {
    OpenCollection,
    ListCandidates,
    ReadImageData,
    LoadThumbnailMetadata,
    OpenVideoPlaybackDevice,
};

enum class MediaEntrySourceErrorCause {
    CollectionOpenFailed,
    UnsupportedCollection,
    CandidateListingFailed,
    EntryNotFound,
    EntryReadFailed,
    VideoPlaybackUnsupported,
    ThumbnailMetadataUnsupported,
    ProviderUnavailable,
    ResourceLimitExceeded,
    OperationCancelled,
};

struct MediaEntrySourceError
{
    MediaEntrySourceErrorCause cause = MediaEntrySourceErrorCause::CollectionOpenFailed;
    MediaEntrySourceBackendKind backend = MediaEntrySourceBackendKind::Unknown;
    MediaEntrySourceOperation operation = MediaEntrySourceOperation::OpenCollection;
    QUrl collectionUrl;
    QString entryPath;
    QString diagnosticDetail;
};

QDebug operator<<(QDebug debug, const MediaEntrySourceError& error);

using MediaEntrySourceErrorCallback = std::function<void(MediaEntrySourceError)>;
}

#endif
