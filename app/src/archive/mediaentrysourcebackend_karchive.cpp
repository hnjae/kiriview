// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcebackend_p.h"

#include "archiveformat.h"
#include "decoding/imagesourcedata.h"
#include "openedcollectionthumbnailpolicy.h"

#include <K7Zip>
#include <KArchive>
#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KTar>
#include <KZip>
#include <KZipFileEntry>
#include <QIODevice>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
namespace Backend = kiriview::MediaEntrySourceBackendDetail;

class ScopedKArchive final
{
public:
    ScopedKArchive() = default;

    explicit ScopedKArchive(std::unique_ptr<KArchive> archive)
        : m_archive(std::move(archive))
    {
    }

    ~ScopedKArchive() { close(); }

    ScopedKArchive(const ScopedKArchive&) = delete;
    ScopedKArchive& operator=(const ScopedKArchive&) = delete;
    ScopedKArchive(ScopedKArchive&&) noexcept = default;
    ScopedKArchive& operator=(ScopedKArchive&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        close();
        m_archive = std::move(other.m_archive);
        return *this;
    }

    [[nodiscard]] const KArchiveDirectory* directory() const
    {
        return m_archive == nullptr ? nullptr : m_archive->directory();
    }

    explicit operator bool() const { return m_archive != nullptr; }

private:
    void close()
    {
        if (m_archive != nullptr) {
            m_archive->close();
        }
    }

    std::unique_ptr<KArchive> m_archive;
};

struct OpenKArchiveResult
{
    ScopedKArchive archive;
    QString diagnosticDetail;
};

std::unique_ptr<KArchive> createArchive(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    const QString filePath = openedCollectionScope.fileUrl().toLocalFile();
    if (filePath.isEmpty()) {
        return nullptr;
    }

    switch (
        kiriview::archiveStorageBackendForRootScheme(openedCollectionScope.rootUrl().scheme())) {
    case kiriview::ArchiveStorageBackend::KZip:
        return std::make_unique<KZip>(filePath);
    case kiriview::ArchiveStorageBackend::KTar:
        return std::make_unique<KTar>(filePath);
    case kiriview::ArchiveStorageBackend::K7Zip:
        return std::make_unique<K7Zip>(filePath);
    case kiriview::ArchiveStorageBackend::LibArchive:
    case kiriview::ArchiveStorageBackend::None:
        return nullptr;
    }

    return nullptr;
}

std::expected<void, Backend::MediaEntrySourceEnumerationFailure> appendArchiveDirectoryMediaEntries(
    std::vector<kiriview::MediaEntrySourceEntry>* resultEntries, const KArchiveDirectory* directory,
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, const QString& prefix,
    int nestingDepth, Backend::MediaEntrySourceEnumerationBudget* budget)
{
    if (directory == nullptr) {
        return {};
    }
    if (const auto current = budget->checkpoint(); !current) {
        return current;
    }

    const QStringList entryNames = directory->entries();
    for (const QString& entryName : entryNames) {
        const QString entryPath
            = prefix.isEmpty() ? entryName : prefix + QLatin1Char('/') + entryName;
        if (const auto admitted = budget->admitEntry(entryPath.size(), nestingDepth); !admitted) {
            return admitted;
        }
        const KArchiveEntry* archiveEntry = directory->entry(entryName);
        if (archiveEntry == nullptr) {
            continue;
        }

        if (archiveEntry->isDirectory()) {
            if (const auto nested = appendArchiveDirectoryMediaEntries(resultEntries,
                    static_cast<const KArchiveDirectory*>(archiveEntry), openedCollectionScope,
                    entryPath, nestingDepth + 1, budget);
                !nested) {
                return nested;
            }
            continue;
        }

        if (!archiveEntry->isFile()) {
            continue;
        }

        std::optional<kiriview::MediaEntrySourceEntry> mediaEntry
            = Backend::openedCollectionMediaEntry(openedCollectionScope, entryPath);
        if (mediaEntry.has_value()) {
            resultEntries->push_back(std::move(*mediaEntry));
        }
    }
    return {};
}

std::expected<std::vector<kiriview::MediaEntrySourceEntry>,
    Backend::MediaEntrySourceEnumerationFailure>
archiveDirectoryMediaEntries(const KArchiveDirectory* directory,
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
    const kiriview::MediaEntrySourceOpenContext& context)
{
    std::vector<kiriview::MediaEntrySourceEntry> entries;
    Backend::MediaEntrySourceEnumerationBudget budget(context);
    if (const auto result = appendArchiveDirectoryMediaEntries(
            &entries, directory, openedCollectionScope, QString(), 1, &budget);
        !result) {
        return std::unexpected(result.error());
    }
    return entries;
}

OpenKArchiveResult openKArchiveCollection(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    std::unique_ptr<KArchive> archive = createArchive(openedCollectionScope);
    if (archive == nullptr) {
        return OpenKArchiveResult {
            {},
            QStringLiteral("KArchive backend could not create an archive reader"),
        };
    }

    if (!archive->open(QIODevice::ReadOnly)) {
        const QString diagnosticDetail = archive->errorString().isEmpty()
            ? QStringLiteral("KArchive could not open the collection")
            : archive->errorString();
        return OpenKArchiveResult { {}, diagnosticDetail };
    }

    return OpenKArchiveResult { ScopedKArchive(std::move(archive)), QString() };
}

kiriview::MediaEntrySourceImageDataResult readKArchiveFileData(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath,
    const KArchiveFile& file, kiriview::ImageSourceDataLease lease)
{
    std::unique_ptr<QIODevice> device(file.createDevice());
    if (device == nullptr) {
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
            Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                kiriview::MediaEntrySourceBackendKind::KArchive,
                kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope, {},
                entryPath));
    }

    kiriview::ImageSourceDataReadResult readResult
        = kiriview::readImageSourceData(*device, std::move(lease), file.size());
    if (readResult.status != kiriview::ImageSourceDataReadStatus::Ready) {
        const kiriview::MediaEntrySourceErrorCause cause
            = readResult.status == kiriview::ImageSourceDataReadStatus::ResourceLimitExceeded
            ? kiriview::MediaEntrySourceErrorCause::ResourceLimitExceeded
            : kiriview::MediaEntrySourceErrorCause::EntryReadFailed;
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
            Backend::mediaEntrySourceError(cause, kiriview::MediaEntrySourceBackendKind::KArchive,
                kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope,
                std::move(readResult.diagnosticDetail), entryPath));
    }

    return Backend::mediaEntrySourceImageDataResult(std::move(readResult.sourceData));
}

std::optional<kiriview::MediaEntrySourceThumbnailMetadata> thumbnailMetadataForKArchiveFile(
    const KArchiveFile& file)
{
    const auto* zipFile = dynamic_cast<const KZipFileEntry*>(&file);
    if (zipFile == nullptr || file.size() < 0) {
        return std::nullopt;
    }

    return kiriview::MediaEntrySourceThumbnailMetadata {
        kiriview::MediaEntryContentChecksum {
            kiriview::MediaEntryContentChecksumAlgorithm::Crc32,
            zipFile->crc32(),
        },
        file.size(),
    };
}

bool kArchiveFileSupportsVideoPlayback(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, const KArchiveFile& file)
{
    switch (
        kiriview::archiveStorageBackendForRootScheme(openedCollectionScope.rootUrl().scheme())) {
    case kiriview::ArchiveStorageBackend::KZip: {
        constexpr int zipStoredCompressionMethod = 0;
        const auto* zipFile = dynamic_cast<const KZipFileEntry*>(&file);
        return zipFile != nullptr && zipFile->encoding() == zipStoredCompressionMethod;
    }
    case kiriview::ArchiveStorageBackend::KTar:
        return true;
    case kiriview::ArchiveStorageBackend::K7Zip:
    case kiriview::ArchiveStorageBackend::LibArchive:
    case kiriview::ArchiveStorageBackend::None:
        return false;
    }

    return false;
}

kiriview::MediaEntrySourceVideoPlaybackDeviceResult openKArchiveFileVideoPlaybackDevice(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath,
    const KArchiveFile& file)
{
    if (!kArchiveFileSupportsVideoPlayback(openedCollectionScope, file)) {
        return Backend::mediaEntrySourceErrorResult<
            kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(Backend::mediaEntrySourceError(
            kiriview::MediaEntrySourceErrorCause::VideoPlaybackUnsupported,
            kiriview::MediaEntrySourceBackendKind::KArchive,
            kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice, openedCollectionScope, {},
            entryPath));
    }

    std::unique_ptr<QIODevice> device(file.createDevice());
    if (device == nullptr) {
        return Backend::mediaEntrySourceErrorResult<
            kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(Backend::mediaEntrySourceError(
            kiriview::MediaEntrySourceErrorCause::VideoPlaybackUnsupported,
            kiriview::MediaEntrySourceBackendKind::KArchive,
            kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice, openedCollectionScope,
            QStringLiteral("KArchive could not create an entry device"), entryPath));
    }

    return Backend::mediaEntrySourceVideoPlaybackDeviceResult(std::move(device));
}

class KArchiveMediaEntrySource final : public Backend::MediaEntrySourceWithEntrySnapshot
{
public:
    KArchiveMediaEntrySource(kiriview::OpenedCollectionScopeLocation openedCollectionScope,
        ScopedKArchive archive, std::vector<kiriview::MediaEntrySourceEntry> entries)
        : Backend::MediaEntrySourceWithEntrySnapshot(std::move(openedCollectionScope),
              kiriview::MediaEntrySourceBackendKind::KArchive, std::move(entries))
        , m_archive(std::move(archive))
    {
    }

protected:
    kiriview::MediaEntrySourceImageDataResult loadAuthorizedImageData(
        const kiriview::MediaEntrySourceEntry& entry, kiriview::ImageSourceDataLease lease) override
    {
        const KArchiveDirectory* directory = m_archive.directory();
        const KArchiveFile* file = directory == nullptr ? nullptr : directory->file(entry.name);
        if (file == nullptr) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::KArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(), {},
                    entry.name));
        }

        return readKArchiveFileData(openedCollectionScope(), entry.name, *file, std::move(lease));
    }

    kiriview::MediaEntrySourceVideoPlaybackDeviceResult loadAuthorizedVideoPlaybackDevice(
        const kiriview::MediaEntrySourceEntry& entry) override
    {
        const KArchiveDirectory* directory = m_archive.directory();
        const KArchiveFile* file = directory == nullptr ? nullptr : directory->file(entry.name);
        if (file == nullptr) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::KArchive,
                    kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice,
                    openedCollectionScope(), {}, entry.name));
        }

        return openKArchiveFileVideoPlaybackDevice(openedCollectionScope(), entry.name, *file);
    }

    kiriview::MediaEntrySourceThumbnailMetadataResult loadAuthorizedThumbnailMetadata(
        const kiriview::MediaEntrySourceEntry& entry) override
    {
        if (!kiriview::openedCollectionEntryPathSupportsThumbnailContentIdentity(
                openedCollectionScope(), entry.name)) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceThumbnailMetadataResult>(Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::ThumbnailMetadataUnsupported,
                kiriview::MediaEntrySourceBackendKind::KArchive,
                kiriview::MediaEntrySourceOperation::LoadThumbnailMetadata, openedCollectionScope(),
                {}, entry.name));
        }

        const KArchiveDirectory* directory = m_archive.directory();
        const KArchiveFile* file = directory == nullptr ? nullptr : directory->file(entry.name);
        if (file == nullptr) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceThumbnailMetadataResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::KArchive,
                    kiriview::MediaEntrySourceOperation::LoadThumbnailMetadata,
                    openedCollectionScope(), {}, entry.name));
        }

        const std::optional<kiriview::MediaEntrySourceThumbnailMetadata> metadata
            = thumbnailMetadataForKArchiveFile(*file);
        if (!metadata.has_value()) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceThumbnailMetadataResult>(Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::ThumbnailMetadataUnsupported,
                kiriview::MediaEntrySourceBackendKind::KArchive,
                kiriview::MediaEntrySourceOperation::LoadThumbnailMetadata, openedCollectionScope(),
                {}, entry.name));
        }

        return Backend::mediaEntrySourceThumbnailMetadataResult(*metadata);
    }

private:
    ScopedKArchive m_archive;
};

kiriview::MediaEntrySourceOpenResult openKArchiveMediaEntrySource(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
    const kiriview::MediaEntrySourceOpenContext& context)
{
    if (context.stopToken.stop_requested()) {
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceEnumerationError(
                Backend::MediaEntrySourceEnumerationFailure::OperationCancelled,
                kiriview::MediaEntrySourceBackendKind::KArchive, openedCollectionScope));
    }
    OpenKArchiveResult opened = openKArchiveCollection(openedCollectionScope);
    if (!opened.archive) {
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::CollectionOpenFailed,
                kiriview::MediaEntrySourceBackendKind::KArchive,
                kiriview::MediaEntrySourceOperation::OpenCollection, openedCollectionScope,
                opened.diagnosticDetail));
    }

    auto entries
        = archiveDirectoryMediaEntries(opened.archive.directory(), openedCollectionScope, context);
    if (!entries) {
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceEnumerationError(entries.error(),
                kiriview::MediaEntrySourceBackendKind::KArchive, openedCollectionScope));
    }
    return kiriview::MediaEntrySourcePtr(std::make_shared<KArchiveMediaEntrySource>(
        openedCollectionScope, std::move(opened.archive), std::move(*entries)));
}

}

namespace kiriview::MediaEntrySourceBackendDetail {
const MediaEntrySourceBackendOperations* kArchiveMediaEntrySourceBackendOperations()
{
    static const MediaEntrySourceBackendOperations operations {
        openKArchiveMediaEntrySource,
    };
    return &operations;
}
}
