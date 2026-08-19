// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcebackend.h"

#include "archiveformat.h"
#include "archivepath.h"
#include "decoding/imageformatregistry.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "format/supportedmediaformats.h"
#include "mediaentrysourcebackend_p.h"

#include <QDebug>
#include <algorithm>
#include <optional>
#include <utility>

namespace {
namespace Backend = kiriview::MediaEntrySourceBackendDetail;
constexpr qsizetype defaultMaximumCollectionEntryCount = 65'536;
constexpr qsizetype defaultMaximumCollectionPathCodeUnitCount = qsizetype { 8 } * 1024 * 1024;
constexpr int defaultMaximumCollectionNestingDepth = 256;

QString defaultMediaEntrySourceDiagnostic(kiriview::MediaEntrySourceErrorCause cause)
{
    using Cause = kiriview::MediaEntrySourceErrorCause;

    switch (cause) {
    case Cause::CollectionOpenFailed:
        return QStringLiteral("collection access backend could not open the collection");
    case Cause::UnsupportedCollection:
        return QStringLiteral("no collection access backend supports the collection");
    case Cause::EntryListingFailed:
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
    case Cause::ResourceLimitExceeded:
        return kiriview::imageSourceDataResourceLimitDiagnostic();
    case Cause::OperationCancelled:
        return QStringLiteral("collection access operation was cancelled");
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
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
    const kiriview::MediaEntrySourceOpenContext& context)
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

    return backend->openSource(openedCollectionScope, context);
}

}

namespace kiriview {
MediaEntrySourceEnumerationLimits defaultMediaEntrySourceEnumerationLimits()
{
    return MediaEntrySourceEnumerationLimits { defaultMaximumCollectionEntryCount,
        defaultMaximumCollectionPathCodeUnitCount, defaultMaximumCollectionNestingDepth };
}

QDebug operator<<(QDebug debug, const MediaEntrySourceError& error)
{
    QDebugStateSaver stateSaver(debug);
    debug.noquote() << "cause" << static_cast<int>(error.cause) << "backend"
                    << static_cast<int>(error.backend) << "operation"
                    << static_cast<int>(error.operation) << "collection"
                    << diagnosticSourceReference(error.collectionUrl) << "entry"
                    << diagnosticPathReference(error.entryPath) << "diagnostic"
                    << diagnosticDetailReference(error.diagnosticDetail);
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
MediaEntrySourceEnumerationBudget::MediaEntrySourceEnumerationBudget(
    const MediaEntrySourceOpenContext& context)
    : m_stopToken(context.stopToken)
    , m_limits(context.enumerationLimits)
{
}

std::expected<void, MediaEntrySourceEnumerationFailure>
MediaEntrySourceEnumerationBudget::checkpoint() const
{
    if (m_stopToken.stop_requested()) {
        return std::unexpected(MediaEntrySourceEnumerationFailure::OperationCancelled);
    }
    return {};
}

std::expected<void, MediaEntrySourceEnumerationFailure>
MediaEntrySourceEnumerationBudget::admitEntry(qsizetype pathCodeUnitCount, int nestingDepth)
{
    if (const auto current = checkpoint(); !current) {
        return current;
    }
    if (pathCodeUnitCount < 0 || nestingDepth <= 0 || m_entryCount >= m_limits.maximumEntryCount
        || nestingDepth > m_limits.maximumNestingDepth
        || pathCodeUnitCount > m_limits.maximumPathCodeUnitCount - m_pathCodeUnitCount) {
        return std::unexpected(MediaEntrySourceEnumerationFailure::ResourceLimitExceeded);
    }

    ++m_entryCount;
    m_pathCodeUnitCount += pathCodeUnitCount;
    return {};
}

MediaEntrySourceWithEntrySnapshot::MediaEntrySourceWithEntrySnapshot(
    OpenedCollectionScopeLocation openedCollectionScope, MediaEntrySourceBackendKind backend,
    std::vector<MediaEntrySourceEntry> entries)
    : m_openedCollectionScope(std::move(openedCollectionScope))
    , m_backend(backend)
    , m_entries(std::move(entries))
{
}

MediaEntrySourceEntriesResult MediaEntrySourceWithEntrySnapshot::loadEntries()
{
    return MediaEntrySourceEntries { m_entries };
}

MediaEntrySourceImageDataResult MediaEntrySourceWithEntrySnapshot::loadImageData(
    const QUrl& imageUrl, ImageSourceDataLease lease)
{
    const MediaEntrySourceEntry* entry
        = authorizedEntry(imageUrl, MediaEntrySourceEntryKind::Image);
    if (entry == nullptr) {
        return mediaEntrySourceErrorResult<MediaEntrySourceImageDataResult>(
            mediaEntrySourceError(MediaEntrySourceErrorCause::EntryNotFound, m_backend,
                MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope, {},
                rejectedSelectorEntryPath(imageUrl)));
    }

    if (!lease.isManaged()) {
        lease = defaultImageSourceDataBudget()->startLease();
    }
    return loadAuthorizedImageData(*entry, std::move(lease));
}

MediaEntrySourceVideoPlaybackDeviceResult
MediaEntrySourceWithEntrySnapshot::loadVideoPlaybackDevice(const QUrl& videoUrl)
{
    const MediaEntrySourceEntry* entry
        = authorizedEntry(videoUrl, MediaEntrySourceEntryKind::Video);
    if (entry == nullptr) {
        return mediaEntrySourceErrorResult<MediaEntrySourceVideoPlaybackDeviceResult>(
            mediaEntrySourceError(MediaEntrySourceErrorCause::EntryNotFound, m_backend,
                MediaEntrySourceOperation::OpenVideoPlaybackDevice, m_openedCollectionScope, {},
                rejectedSelectorEntryPath(videoUrl)));
    }

    return loadAuthorizedVideoPlaybackDevice(*entry);
}

MediaEntrySourceThumbnailMetadataResult MediaEntrySourceWithEntrySnapshot::loadThumbnailMetadata(
    const QUrl& imageUrl)
{
    const MediaEntrySourceEntry* entry
        = authorizedEntry(imageUrl, MediaEntrySourceEntryKind::Image);
    if (entry == nullptr) {
        return mediaEntrySourceErrorResult<MediaEntrySourceThumbnailMetadataResult>(
            mediaEntrySourceError(MediaEntrySourceErrorCause::EntryNotFound, m_backend,
                MediaEntrySourceOperation::LoadThumbnailMetadata, m_openedCollectionScope, {},
                rejectedSelectorEntryPath(imageUrl)));
    }

    return loadAuthorizedThumbnailMetadata(*entry);
}

const OpenedCollectionScopeLocation&
MediaEntrySourceWithEntrySnapshot::openedCollectionScope() const
{
    return m_openedCollectionScope;
}

MediaEntrySourceVideoPlaybackDeviceResult
MediaEntrySourceWithEntrySnapshot::loadAuthorizedVideoPlaybackDevice(
    const MediaEntrySourceEntry& entry)
{
    return mediaEntrySourceErrorResult<MediaEntrySourceVideoPlaybackDeviceResult>(
        mediaEntrySourceError(MediaEntrySourceErrorCause::VideoPlaybackUnsupported, m_backend,
            MediaEntrySourceOperation::OpenVideoPlaybackDevice, m_openedCollectionScope, {},
            entry.name));
}

MediaEntrySourceThumbnailMetadataResult
MediaEntrySourceWithEntrySnapshot::loadAuthorizedThumbnailMetadata(
    const MediaEntrySourceEntry& entry)
{
    return mediaEntrySourceErrorResult<MediaEntrySourceThumbnailMetadataResult>(
        mediaEntrySourceError(MediaEntrySourceErrorCause::ThumbnailMetadataUnsupported, m_backend,
            MediaEntrySourceOperation::LoadThumbnailMetadata, m_openedCollectionScope, {},
            entry.name));
}

const MediaEntrySourceEntry* MediaEntrySourceWithEntrySnapshot::authorizedEntry(
    const QUrl& url, MediaEntrySourceEntryKind expectedKind) const
{
    const QString entryPath = openedCollectionEntryPathForUrl(m_openedCollectionScope, url);
    if (entryPath.isEmpty()) {
        return nullptr;
    }

    const auto entry = std::ranges::find_if(m_entries, [&](const MediaEntrySourceEntry& item) {
        return item.kind == expectedKind && item.name == entryPath && item.url == url;
    });
    return entry == m_entries.cend() ? nullptr : &*entry;
}

QString MediaEntrySourceWithEntrySnapshot::rejectedSelectorEntryPath(const QUrl& url) const
{
    const QString entryPath = openedCollectionEntryPathForUrl(m_openedCollectionScope, url);
    return entryPath.isEmpty() ? url.toString() : entryPath;
}

std::optional<MediaEntrySourceEntry> openedCollectionMediaEntry(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath)
{
    const QString entryName = normalizedArchiveEntryPath(entryPath);
    if (entryName.isEmpty()
        || !SupportedMediaFormats::isSupportedOrdinaryMediaFileName(entryName)) {
        return std::nullopt;
    }

    const QUrl url = openedCollectionEntryUrl(openedCollectionScope, entryName);
    if (url.isEmpty()) {
        return std::nullopt;
    }

    return MediaEntrySourceEntry { url, entryName,
        SupportedMediaFormats::isSupportedDirectVideoFileName(entryName)
            ? MediaEntrySourceEntryKind::Video
            : MediaEntrySourceEntryKind::Image };
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

MediaEntrySourceError mediaEntrySourceEnumerationError(MediaEntrySourceEnumerationFailure failure,
    MediaEntrySourceBackendKind backend, const OpenedCollectionScopeLocation& openedCollectionScope)
{
    if (failure == MediaEntrySourceEnumerationFailure::OperationCancelled) {
        return mediaEntrySourceError(MediaEntrySourceErrorCause::OperationCancelled, backend,
            MediaEntrySourceOperation::ListEntries, openedCollectionScope,
            QStringLiteral("collection enumeration was cancelled"));
    }
    return mediaEntrySourceError(MediaEntrySourceErrorCause::ResourceLimitExceeded, backend,
        MediaEntrySourceOperation::ListEntries, openedCollectionScope,
        QStringLiteral("collection enumeration exceeds the configured resource limits"));
}

MediaEntrySourceEntriesResult mediaEntrySourceEntriesResult(
    std::vector<MediaEntrySourceEntry> entries)
{
    return MediaEntrySourceEntries { std::move(entries) };
}

MediaEntrySourceImageDataResult mediaEntrySourceImageDataResult(QByteArray data)
{
    return MediaEntrySourceImageData { std::move(data), {} };
}

MediaEntrySourceImageDataResult mediaEntrySourceImageDataResult(ImageSourceData sourceData)
{
    return MediaEntrySourceImageData { std::move(sourceData.data), std::move(sourceData.lease) };
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
MediaEntrySourceEntriesResult loadMediaEntrySourceEntries(
    const OpenedCollectionScopeLocation& openedCollectionScope)
{
    MediaEntrySourceOpenResult opened = openMediaEntrySource(openedCollectionScope);
    if (const auto* error = kiriview::mediaEntrySourceResultError(opened)) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceEntriesResult>(*error);
    }

    const auto* source = kiriview::mediaEntrySourceResultValue(opened);
    if (source == nullptr || *source == nullptr) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceEntriesResult>(
            Backend::mediaEntrySourceError(MediaEntrySourceErrorCause::ProviderUnavailable,
                MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::OpenCollection,
                openedCollectionScope));
    }

    return (*source)->loadEntries();
}

MediaEntrySourceImageDataResult loadMediaEntrySourceImageData(
    const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& imageUrl,
    ImageSourceDataLease lease)
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

    return (*source)->loadImageData(imageUrl, std::move(lease));
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
    const OpenedCollectionScopeLocation& openedCollectionScope,
    const MediaEntrySourceOpenContext& context)
{
    if (openedCollectionScope.isEmpty()) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceError(MediaEntrySourceErrorCause::CollectionOpenFailed,
                MediaEntrySourceBackendKind::Unknown, MediaEntrySourceOperation::OpenCollection,
                openedCollectionScope, QStringLiteral("opened collection scope is empty")));
    }

    if (context.stopToken.stop_requested()) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceEnumerationError(
                Backend::MediaEntrySourceEnumerationFailure::OperationCancelled,
                MediaEntrySourceBackendKind::Unknown, openedCollectionScope));
    }

    return openWithMediaEntrySourceBackend(openedCollectionScope, context);
}
}
