// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcebackend.h"

#include "archiveformat.h"
#include "archivepath.h"
#include "decoding/imageformatregistry.h"
#include "mediaentrysourcebackend_p.h"
#include "navigation/imagedocumentpagenavigationpolicy.h"
#include "navigation/mediaformatregistry.h"

#include <QDebug>
#include <algorithm>
#include <optional>
#include <utility>

namespace {
namespace Backend = kiriview::MediaEntrySourceBackendDetail;

QString defaultMediaEntrySourceDiagnostic(kiriview::MediaEntrySourceErrorCause cause)
{
    using Cause = kiriview::MediaEntrySourceErrorCause;

    switch (cause) {
    case Cause::CollectionOpenFailed:
        return QStringLiteral("collection access backend could not open the collection");
    case Cause::UnsupportedCollection:
        return QStringLiteral("no collection access backend supports the collection");
    case Cause::CandidateListingFailed:
        return QStringLiteral("collection access backend could not list media entries");
    case Cause::EntryNotFound:
        return QStringLiteral("requested collection entry was not found");
    case Cause::EntryReadFailed:
        return QStringLiteral("collection access backend could not read the image entry");
    case Cause::VideoPlaybackUnsupported:
        return QStringLiteral("collection access backend cannot provide a video playback device");
    case Cause::ThumbnailMetadataUnsupported:
        return QStringLiteral("collection entry thumbnail metadata is unavailable");
    case Cause::ProviderUnavailable:
        return QStringLiteral("media entry source provider is unavailable");
    }

    return QStringLiteral("unknown collection access failure");
}

const Backend::MediaEntrySourceBackendOperations*
mediaEntrySourceBackendOperationsForOpenedCollection(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    if (openedCollectionScope.isDirectory()) {
        return Backend::directoryCollectionMediaEntrySourceBackendOperations();
    }

    switch (
        kiriview::archiveStorageBackendForRootScheme(openedCollectionScope.rootUrl().scheme())) {
    case kiriview::ArchiveStorageBackend::KZip:
    case kiriview::ArchiveStorageBackend::KTar:
    case kiriview::ArchiveStorageBackend::K7Zip:
        return Backend::kArchiveMediaEntrySourceBackendOperations();
    case kiriview::ArchiveStorageBackend::LibArchive:
        return Backend::libArchiveMediaEntrySourceBackendOperations();
    case kiriview::ArchiveStorageBackend::None:
        return nullptr;
    }

    return nullptr;
}

kiriview::MediaEntrySourceOpenResult openWithMediaEntrySourceBackend(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    const Backend::MediaEntrySourceBackendOperations* backend
        = mediaEntrySourceBackendOperationsForOpenedCollection(openedCollectionScope);
    if (backend == nullptr) {
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::UnsupportedCollection,
                kiriview::MediaEntrySourceBackendKind::Unsupported,
                kiriview::MediaEntrySourceOperation::OpenCollection, openedCollectionScope));
    }

    return backend->openSource(openedCollectionScope);
}

}

namespace kiriview {
QDebug operator<<(QDebug debug, const MediaEntrySourceError& error)
{
    QDebugStateSaver stateSaver(debug);
    debug.noquote() << "cause" << static_cast<int>(error.cause) << "backend"
                    << static_cast<int>(error.backend) << "operation"
                    << static_cast<int>(error.operation) << "collection" << error.collectionUrl
                    << "entry" << error.entryPath << "diagnostic" << error.diagnosticDetail;
    return debug;
}

MediaEntrySourceVideoPlaybackDeviceResult MediaEntrySource::loadVideoPlaybackDevice(
    const QUrl& videoUrl)
{
    Q_UNUSED(videoUrl)
    return MediaEntrySourceBackendDetail::mediaEntrySourceErrorResult<
        MediaEntrySourceVideoPlaybackDeviceResult>(
        MediaEntrySourceBackendDetail::mediaEntrySourceError(
            MediaEntrySourceErrorCause::VideoPlaybackUnsupported,
            MediaEntrySourceBackendKind::Unknown,
            MediaEntrySourceOperation::OpenVideoPlaybackDevice, {}));
}

MediaEntrySourceThumbnailMetadataResult MediaEntrySource::loadThumbnailMetadata(
    const QUrl& imageUrl)
{
    Q_UNUSED(imageUrl)
    return MediaEntrySourceBackendDetail::mediaEntrySourceErrorResult<
        MediaEntrySourceThumbnailMetadataResult>(
        MediaEntrySourceBackendDetail::mediaEntrySourceError(
            MediaEntrySourceErrorCause::ThumbnailMetadataUnsupported,
            MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::LoadThumbnailMetadata,
            {}));
}
}

namespace kiriview::MediaEntrySourceBackendDetail {
MediaEntrySourceWithCandidateSnapshot::MediaEntrySourceWithCandidateSnapshot(
    OpenedCollectionScopeLocation openedCollectionScope, MediaEntrySourceBackendKind backend,
    std::vector<ImageDocumentPageCandidate> candidates)
    : m_openedCollectionScope(std::move(openedCollectionScope))
    , m_backend(backend)
    , m_candidates(std::move(candidates))
{
    sortImageDocumentPageCandidates(&m_candidates);
}

MediaEntrySourceCandidatesResult
MediaEntrySourceWithCandidateSnapshot::loadImageDocumentPageCandidates()
{
    return MediaEntrySourceCandidates { m_candidates };
}

MediaEntrySourceImageDataResult MediaEntrySourceWithCandidateSnapshot::loadImageData(
    const QUrl& imageUrl)
{
    const ImageDocumentPageCandidate* candidate
        = authorizedCandidate(imageUrl, ImageDocumentPageKind::Image);
    if (candidate == nullptr) {
        return mediaEntrySourceErrorResult<MediaEntrySourceImageDataResult>(
            mediaEntrySourceError(MediaEntrySourceErrorCause::EntryNotFound, m_backend,
                MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope, {},
                rejectedSelectorEntryPath(imageUrl)));
    }

    return loadAuthorizedImageData(*candidate);
}

MediaEntrySourceVideoPlaybackDeviceResult
MediaEntrySourceWithCandidateSnapshot::loadVideoPlaybackDevice(const QUrl& videoUrl)
{
    const ImageDocumentPageCandidate* candidate
        = authorizedCandidate(videoUrl, ImageDocumentPageKind::Video);
    if (candidate == nullptr) {
        return mediaEntrySourceErrorResult<MediaEntrySourceVideoPlaybackDeviceResult>(
            mediaEntrySourceError(MediaEntrySourceErrorCause::EntryNotFound, m_backend,
                MediaEntrySourceOperation::OpenVideoPlaybackDevice, m_openedCollectionScope, {},
                rejectedSelectorEntryPath(videoUrl)));
    }

    return loadAuthorizedVideoPlaybackDevice(*candidate);
}

MediaEntrySourceThumbnailMetadataResult
MediaEntrySourceWithCandidateSnapshot::loadThumbnailMetadata(const QUrl& imageUrl)
{
    const ImageDocumentPageCandidate* candidate
        = authorizedCandidate(imageUrl, ImageDocumentPageKind::Image);
    if (candidate == nullptr) {
        return mediaEntrySourceErrorResult<MediaEntrySourceThumbnailMetadataResult>(
            mediaEntrySourceError(MediaEntrySourceErrorCause::EntryNotFound, m_backend,
                MediaEntrySourceOperation::LoadThumbnailMetadata, m_openedCollectionScope, {},
                rejectedSelectorEntryPath(imageUrl)));
    }

    return loadAuthorizedThumbnailMetadata(*candidate);
}

const OpenedCollectionScopeLocation&
MediaEntrySourceWithCandidateSnapshot::openedCollectionScope() const
{
    return m_openedCollectionScope;
}

MediaEntrySourceVideoPlaybackDeviceResult
MediaEntrySourceWithCandidateSnapshot::loadAuthorizedVideoPlaybackDevice(
    const ImageDocumentPageCandidate& candidate)
{
    return mediaEntrySourceErrorResult<MediaEntrySourceVideoPlaybackDeviceResult>(
        mediaEntrySourceError(MediaEntrySourceErrorCause::VideoPlaybackUnsupported, m_backend,
            MediaEntrySourceOperation::OpenVideoPlaybackDevice, m_openedCollectionScope, {},
            candidate.name));
}

MediaEntrySourceThumbnailMetadataResult
MediaEntrySourceWithCandidateSnapshot::loadAuthorizedThumbnailMetadata(
    const ImageDocumentPageCandidate& candidate)
{
    return mediaEntrySourceErrorResult<MediaEntrySourceThumbnailMetadataResult>(
        mediaEntrySourceError(MediaEntrySourceErrorCause::ThumbnailMetadataUnsupported, m_backend,
            MediaEntrySourceOperation::LoadThumbnailMetadata, m_openedCollectionScope, {},
            candidate.name));
}

const ImageDocumentPageCandidate* MediaEntrySourceWithCandidateSnapshot::authorizedCandidate(
    const QUrl& url, ImageDocumentPageKind expectedKind) const
{
    const QString entryPath = openedCollectionEntryPathForUrl(m_openedCollectionScope, url);
    if (entryPath.isEmpty()) {
        return nullptr;
    }

    const auto candidate
        = std::ranges::find_if(m_candidates, [&](const ImageDocumentPageCandidate& item) {
              return item.kind == expectedKind && item.name == entryPath && item.url == url;
          });
    return candidate == m_candidates.cend() ? nullptr : &*candidate;
}

QString MediaEntrySourceWithCandidateSnapshot::rejectedSelectorEntryPath(const QUrl& url) const
{
    const QString entryPath = openedCollectionEntryPathForUrl(m_openedCollectionScope, url);
    return entryPath.isEmpty() ? url.toString() : entryPath;
}

std::optional<ImageDocumentPageCandidate> openedCollectionImageDocumentPageCandidate(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath)
{
    const QString candidateName = normalizedArchiveEntryPath(entryPath);
    if (candidateName.isEmpty() || !isSupportedOrdinaryMediaFileName(candidateName)) {
        return std::nullopt;
    }

    const QUrl url = openedCollectionEntryUrl(openedCollectionScope, candidateName);
    if (url.isEmpty()) {
        return std::nullopt;
    }

    return ImageDocumentPageCandidate { url, candidateName,
        isSupportedDirectVideoFileName(candidateName) ? ImageDocumentPageKind::Video
                                                      : ImageDocumentPageKind::Image };
}

MediaEntrySourceError mediaEntrySourceError(MediaEntrySourceErrorCause cause,
    MediaEntrySourceBackendKind backend, MediaEntrySourceOperation operation,
    const OpenedCollectionScopeLocation& openedCollectionScope, QString diagnosticDetail,
    QString entryPath)
{
    if (diagnosticDetail.isEmpty()) {
        diagnosticDetail = defaultMediaEntrySourceDiagnostic(cause);
    }
    return MediaEntrySourceError { cause, backend, operation, openedCollectionScope.fileUrl(),
        std::move(entryPath), std::move(diagnosticDetail) };
}

MediaEntrySourceCandidatesResult mediaEntrySourceCandidatesResult(
    std::vector<ImageDocumentPageCandidate> candidates)
{
    sortImageDocumentPageCandidates(&candidates);
    return MediaEntrySourceCandidates { std::move(candidates) };
}

MediaEntrySourceImageDataResult mediaEntrySourceImageDataResult(QByteArray data)
{
    return MediaEntrySourceImageData { std::move(data) };
}

MediaEntrySourceVideoPlaybackDeviceResult mediaEntrySourceVideoPlaybackDeviceResult(
    std::unique_ptr<QIODevice> device, MediaEntrySourcePtr sourceOwner)
{
    return MediaEntrySourceVideoPlaybackDevice { std::move(sourceOwner), std::move(device) };
}

MediaEntrySourceThumbnailMetadataResult mediaEntrySourceThumbnailMetadataResult(
    MediaEntrySourceThumbnailMetadata metadata)
{
    return metadata;
}
}

namespace kiriview {
MediaEntrySourceCandidatesResult loadMediaEntrySourceCandidates(
    const OpenedCollectionScopeLocation& openedCollectionScope)
{
    MediaEntrySourceOpenResult opened = openMediaEntrySource(openedCollectionScope);
    if (const auto* error = kiriview::mediaEntrySourceResultError(opened)) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceCandidatesResult>(*error);
    }

    const auto* source = kiriview::mediaEntrySourceResultValue(opened);
    if (source == nullptr || *source == nullptr) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceCandidatesResult>(
            Backend::mediaEntrySourceError(MediaEntrySourceErrorCause::ProviderUnavailable,
                MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::OpenCollection,
                openedCollectionScope));
    }

    return (*source)->loadImageDocumentPageCandidates();
}

MediaEntrySourceImageDataResult loadMediaEntrySourceImageData(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& imageUrl)
{
    MediaEntrySourceOpenResult opened = openMediaEntrySource(openedCollectionScope);
    if (const auto* error = kiriview::mediaEntrySourceResultError(opened)) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceImageDataResult>(*error);
    }

    const auto* source = kiriview::mediaEntrySourceResultValue(opened);
    if (source == nullptr || *source == nullptr) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceImageDataResult>(
            Backend::mediaEntrySourceError(MediaEntrySourceErrorCause::ProviderUnavailable,
                MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::OpenCollection,
                openedCollectionScope));
    }

    return (*source)->loadImageData(imageUrl);
}

MediaEntrySourceThumbnailMetadataResult loadMediaEntrySourceThumbnailMetadata(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& imageUrl)
{
    MediaEntrySourceOpenResult opened = openMediaEntrySource(openedCollectionScope);
    if (const auto* error = kiriview::mediaEntrySourceResultError(opened)) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceThumbnailMetadataResult>(
            *error);
    }

    const auto* source = kiriview::mediaEntrySourceResultValue(opened);
    if (source == nullptr || *source == nullptr) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceThumbnailMetadataResult>(
            Backend::mediaEntrySourceError(MediaEntrySourceErrorCause::ProviderUnavailable,
                MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::OpenCollection,
                openedCollectionScope));
    }

    return (*source)->loadThumbnailMetadata(imageUrl);
}

MediaEntrySourceOpenResult openMediaEntrySource(
    const OpenedCollectionScopeLocation& openedCollectionScope)
{
    if (openedCollectionScope.isEmpty()) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceError(MediaEntrySourceErrorCause::CollectionOpenFailed,
                MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::OpenCollection,
                openedCollectionScope, QStringLiteral("opened collection scope is empty")));
    }

    return openWithMediaEntrySourceBackend(openedCollectionScope);
}
}
