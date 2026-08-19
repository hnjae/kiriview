// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcebackend_p.h"

#include "archivepath.h"
#include "decoding/imagesourcedata.h"
#include "scopedfiledescriptor_p.h"

#include <QFile>
#include <archive.h>
#include <archive_entry.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <memory>
#include <optional>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace {
namespace Backend = kiriview::MediaEntrySourceBackendDetail;
using LibArchiveReader = std::unique_ptr<archive, int (*)(archive*)>;
using Backend::ScopedFileDescriptor;

struct OpenArchiveFileResult
{
    ScopedFileDescriptor fileDescriptor;
    QString diagnosticDetail;
};

QString libArchiveErrorString(archive* reader, const QString& fallback)
{
    const char* message = reader == nullptr ? nullptr : archive_error_string(reader);
    if (message == nullptr || message[0] == '\0') {
        return fallback;
    }

    return QString::fromLocal8Bit(message);
}

LibArchiveReader makeLibArchiveReader()
{
    return LibArchiveReader(archive_read_new(), archive_read_free);
}

OpenArchiveFileResult openArchiveFileDescriptor(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    const QString filePath = openedCollectionScope.fileUrl().toLocalFile();
    if (filePath.isEmpty()) {
        return OpenArchiveFileResult {
            {},
            QStringLiteral("libarchive collection path is empty"),
        };
    }

#ifdef O_CLOEXEC
    constexpr int openFlags = O_RDONLY | O_CLOEXEC;
#else
    constexpr int openFlags = O_RDONLY;
#endif

    const QByteArray encodedFilePath = QFile::encodeName(filePath);
    const int fileDescriptor = ::open(encodedFilePath.constData(), openFlags);
    if (fileDescriptor < 0) {
        return OpenArchiveFileResult {
            {},
            QString::fromLocal8Bit(std::strerror(errno)),
        };
    }

    return OpenArchiveFileResult { ScopedFileDescriptor(fileDescriptor), QString() };
}

bool configureLibArchiveReader(archive* reader, QString* diagnosticDetail)
{
    if (archive_read_support_filter_all(reader) != ARCHIVE_OK
        || archive_read_support_format_rar(reader) != ARCHIVE_OK
        || archive_read_support_format_rar5(reader) != ARCHIVE_OK) {
        *diagnosticDetail = libArchiveErrorString(
            reader, QStringLiteral("libarchive reader configuration failed"));
        return false;
    }

    return true;
}

LibArchiveReader openLibArchiveReaderOnFd(int fileDescriptor, QString* diagnosticDetail)
{
    if (fileDescriptor < 0 || ::lseek(fileDescriptor, 0, SEEK_SET) < 0) {
        *diagnosticDetail = QStringLiteral("libarchive file descriptor could not seek to start");
        return LibArchiveReader(nullptr, archive_read_free);
    }

    LibArchiveReader reader = makeLibArchiveReader();
    if (reader == nullptr) {
        *diagnosticDetail = QStringLiteral("libarchive reader allocation failed");
        return reader;
    }

    if (!configureLibArchiveReader(reader.get(), diagnosticDetail)) {
        return LibArchiveReader(nullptr, archive_read_free);
    }

    if (archive_read_open_fd(reader.get(), fileDescriptor, 10240) != ARCHIVE_OK) {
        *diagnosticDetail = libArchiveErrorString(
            reader.get(), QStringLiteral("libarchive could not open the file descriptor"));
        return LibArchiveReader(nullptr, archive_read_free);
    }

    return reader;
}

QString libArchiveEntryPath(archive_entry* entry)
{
    const char* utf8Path = archive_entry_pathname_utf8(entry);
    if (utf8Path != nullptr) {
        return QString::fromUtf8(utf8Path);
    }

    const char* path = archive_entry_pathname(entry);
    if (path == nullptr) {
        return {};
    }

    return QFile::decodeName(path);
}

int archiveEntryNestingDepth(QStringView entryPath)
{
    int depth = 1;
    for (const QChar character : entryPath) {
        if (character == QLatin1Char('/')) {
            ++depth;
        }
    }
    return depth;
}

bool skipLibArchiveEntry(archive* reader, QString* diagnosticDetail)
{
    if (archive_read_data_skip(reader) == ARCHIVE_OK) {
        return true;
    }

    *diagnosticDetail
        = libArchiveErrorString(reader, QStringLiteral("libarchive could not skip an entry"));
    return false;
}

kiriview::MediaEntrySourceImageDataResult readLibArchiveEntryData(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, const QString& entryPath,
    archive* reader, archive_entry* entry, kiriview::ImageSourceDataLease lease)
{
    kiriview::ImageSourceData sourceData({}, std::move(lease));
    if (archive_entry_size_is_set(entry)) {
        const la_int64_t expectedSize = archive_entry_size(entry);
        if (expectedSize >= 0 && !sourceData.tryReserveExpectedByteCount(expectedSize)) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::ResourceLimitExceeded,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope,
                    kiriview::imageSourceDataResourceLimitDiagnostic(), entryPath));
        }
    }

    char buffer[64 * 1024];
    while (true) {
        const la_ssize_t bytesRead = archive_read_data(reader, buffer, sizeof(buffer));
        if (bytesRead == 0) {
            break;
        }
        if (bytesRead < 0) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope,
                    libArchiveErrorString(
                        reader, QStringLiteral("libarchive could not read entry data")),
                    entryPath));
        }

        if (!sourceData.tryAppend(QByteArrayView(buffer, static_cast<qsizetype>(bytesRead)))) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::ResourceLimitExceeded,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope,
                    kiriview::imageSourceDataResourceLimitDiagnostic(), entryPath));
        }
    }

    if (archive_entry_size_is_set(entry)) {
        const la_int64_t expectedSize = archive_entry_size(entry);
        if (expectedSize >= 0 && static_cast<qint64>(sourceData.data.size()) != expectedSize) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope,
                    QStringLiteral("libarchive entry size did not match the declared size"),
                    entryPath));
        }
    }

    return Backend::mediaEntrySourceImageDataResult(std::move(sourceData));
}

struct LibArchiveMediaEntrySourceMetadata
{
    std::vector<kiriview::MediaEntrySourceEntry> entries;
    std::map<QString, int> entryOrderByPath;
};

std::optional<LibArchiveMediaEntrySourceMetadata> scanLibArchiveMediaEntrySourceMetadata(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, archive* reader,
    const kiriview::MediaEntrySourceOpenContext& context,
    std::optional<Backend::MediaEntrySourceEnumerationFailure>* enumerationFailure,
    QString* diagnosticDetail)
{
    LibArchiveMediaEntrySourceMetadata metadata;
    Backend::MediaEntrySourceEnumerationBudget budget(context);
    archive_entry* archiveEntry = nullptr;
    int entryOrder = 0;

    while (true) {
        if (const auto current = budget.checkpoint(); !current) {
            *enumerationFailure = current.error();
            return std::nullopt;
        }
        const int status = archive_read_next_header(reader, &archiveEntry);
        if (status == ARCHIVE_EOF) {
            return metadata;
        }
        if (status != ARCHIVE_OK) {
            *diagnosticDetail = libArchiveErrorString(
                reader, QStringLiteral("libarchive could not read the next archive header"));
            return std::nullopt;
        }

        const QString entryPath = libArchiveEntryPath(archiveEntry);
        if (const auto admitted
            = budget.admitEntry(entryPath.size(), archiveEntryNestingDepth(entryPath));
            !admitted) {
            *enumerationFailure = admitted.error();
            return std::nullopt;
        }
        const int currentEntryOrder = entryOrder;
        ++entryOrder;

        if (archive_entry_filetype(archiveEntry) == AE_IFREG) {
            std::optional<kiriview::MediaEntrySourceEntry> mediaEntry
                = Backend::openedCollectionMediaEntry(openedCollectionScope, entryPath);
            if (mediaEntry.has_value()) {
                metadata.entryOrderByPath[mediaEntry->name] = currentEntryOrder;
                metadata.entries.push_back(std::move(*mediaEntry));
            }
        }

        if (!skipLibArchiveEntry(reader, diagnosticDetail)) {
            return std::nullopt;
        }
    }
}

class LibArchiveMediaEntrySource final : public Backend::MediaEntrySourceWithEntrySnapshot
{
public:
    static kiriview::MediaEntrySourceOpenResult create(
        const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
        const kiriview::MediaEntrySourceOpenContext& context)
    {
        if (context.stopToken.stop_requested()) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                Backend::mediaEntrySourceEnumerationError(
                    Backend::MediaEntrySourceEnumerationFailure::OperationCancelled,
                    kiriview::MediaEntrySourceBackendKind::LibArchive, openedCollectionScope));
        }
        OpenArchiveFileResult opened = openArchiveFileDescriptor(openedCollectionScope);
        if (!opened.fileDescriptor) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::CollectionOpenFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::OpenCollection, openedCollectionScope,
                    opened.diagnosticDetail));
        }

        std::optional<Backend::MediaEntrySourceEnumerationFailure> enumerationFailure;
        QString diagnosticDetail;
        LibArchiveReader reader
            = openLibArchiveReaderOnFd(opened.fileDescriptor.get(), &diagnosticDetail);
        if (reader == nullptr) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::CollectionOpenFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::OpenCollection, openedCollectionScope,
                    diagnosticDetail));
        }

        std::optional<LibArchiveMediaEntrySourceMetadata> metadata
            = scanLibArchiveMediaEntrySourceMetadata(openedCollectionScope, reader.get(), context,
                &enumerationFailure, &diagnosticDetail);
        if (!metadata.has_value()) {
            if (enumerationFailure.has_value()) {
                return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                    Backend::mediaEntrySourceEnumerationError(*enumerationFailure,
                        kiriview::MediaEntrySourceBackendKind::LibArchive, openedCollectionScope));
            }
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryListingFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ListEntries, openedCollectionScope,
                    diagnosticDetail));
        }

        reader.reset();
        return kiriview::MediaEntrySourcePtr(
            std::shared_ptr<LibArchiveMediaEntrySource>(new LibArchiveMediaEntrySource(
                openedCollectionScope, std::move(opened.fileDescriptor), std::move(*metadata))));
    }

protected:
    kiriview::MediaEntrySourceImageDataResult loadAuthorizedImageData(
        const kiriview::MediaEntrySourceEntry& entry, kiriview::ImageSourceDataLease lease) override
    {
        const auto entryOrder = m_entryOrderByPath.find(entry.name);
        if (entryOrder == m_entryOrderByPath.cend()) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(), {},
                    entry.name));
        }

        return readImageDataAtOrder(entryOrder->second, entry.name, std::move(lease));
    }

    kiriview::MediaEntrySourceVideoPlaybackDeviceResult loadAuthorizedVideoPlaybackDevice(
        const kiriview::MediaEntrySourceEntry& entry) override
    {
        const auto entryOrder = m_entryOrderByPath.find(entry.name);
        if (entryOrder == m_entryOrderByPath.cend()) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice,
                    openedCollectionScope(), {}, entry.name));
        }

        return Backend::mediaEntrySourceErrorResult<
            kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(Backend::mediaEntrySourceError(
            kiriview::MediaEntrySourceErrorCause::VideoPlaybackUnsupported,
            kiriview::MediaEntrySourceBackendKind::LibArchive,
            kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice, openedCollectionScope(),
            {}, entry.name));
    }

private:
    LibArchiveMediaEntrySource(kiriview::OpenedCollectionScopeLocation openedCollectionScope,
        ScopedFileDescriptor archiveFile, LibArchiveMediaEntrySourceMetadata metadata)
        : Backend::MediaEntrySourceWithEntrySnapshot(std::move(openedCollectionScope),
              kiriview::MediaEntrySourceBackendKind::LibArchive, std::move(metadata.entries))
        , m_archiveFile(std::move(archiveFile))
        , m_entryOrderByPath(std::move(metadata.entryOrderByPath))
    {
    }

    kiriview::MediaEntrySourceImageDataResult readImageDataAtOrder(
        int targetEntryOrder, const QString& targetEntryPath, kiriview::ImageSourceDataLease lease)
    {
        QString diagnosticDetail;
        if (!prepareReaderForEntry(targetEntryOrder, &diagnosticDetail)) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                    diagnosticDetail, targetEntryPath));
        }

        archive_entry* entry = nullptr;
        while (m_nextEntryOrder <= targetEntryOrder) {
            const int currentEntryOrder = m_nextEntryOrder;
            const int status = archive_read_next_header(m_reader.get(), &entry);
            if (status == ARCHIVE_EOF) {
                m_readerExhausted = true;
                return Backend::mediaEntrySourceErrorResult<
                    kiriview::MediaEntrySourceImageDataResult>(Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                    QStringLiteral("libarchive reached end of archive before the requested entry"),
                    targetEntryPath));
            }
            if (status != ARCHIVE_OK) {
                return Backend::mediaEntrySourceErrorResult<
                    kiriview::MediaEntrySourceImageDataResult>(Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                    libArchiveErrorString(
                        m_reader.get(), QStringLiteral("libarchive could not read entry header")),
                    targetEntryPath));
            }

            ++m_nextEntryOrder;
            if (currentEntryOrder == targetEntryOrder) {
                if (archive_entry_filetype(entry) != AE_IFREG) {
                    return Backend::mediaEntrySourceErrorResult<
                        kiriview::MediaEntrySourceImageDataResult>(Backend::mediaEntrySourceError(
                        kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                        kiriview::MediaEntrySourceBackendKind::LibArchive,
                        kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                        {}, targetEntryPath));
                }

                const QString currentEntryPath
                    = kiriview::normalizedArchiveEntryPath(libArchiveEntryPath(entry));
                if (currentEntryPath != targetEntryPath) {
                    return Backend::mediaEntrySourceErrorResult<
                        kiriview::MediaEntrySourceImageDataResult>(Backend::mediaEntrySourceError(
                        kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                        kiriview::MediaEntrySourceBackendKind::LibArchive,
                        kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                        QStringLiteral("libarchive entry identity did not match the snapshot"),
                        targetEntryPath));
                }

                return readLibArchiveEntryData(openedCollectionScope(), currentEntryPath,
                    m_reader.get(), entry, std::move(lease));
            }

            if (!skipLibArchiveEntry(m_reader.get(), &diagnosticDetail)) {
                return Backend::mediaEntrySourceErrorResult<
                    kiriview::MediaEntrySourceImageDataResult>(Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                    diagnosticDetail, targetEntryPath));
            }
        }

        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
            Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                kiriview::MediaEntrySourceBackendKind::LibArchive,
                kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(), {},
                targetEntryPath));
    }

    bool prepareReaderForEntry(int targetEntryOrder, QString* diagnosticDetail)
    {
        if (m_reader == nullptr || m_readerExhausted || targetEntryOrder < m_nextEntryOrder) {
            return resetReader(diagnosticDetail);
        }

        return true;
    }

    bool resetReader(QString* diagnosticDetail)
    {
        m_reader.reset();
        m_nextEntryOrder = 0;
        m_readerExhausted = false;

        m_reader = openLibArchiveReaderOnFd(m_archiveFile.get(), diagnosticDetail);
        if (m_reader == nullptr) {
            m_readerExhausted = true;
            return false;
        }

        return true;
    }

    ScopedFileDescriptor m_archiveFile;
    std::map<QString, int> m_entryOrderByPath;
    LibArchiveReader m_reader { nullptr, archive_read_free };
    int m_nextEntryOrder = 0;
    bool m_readerExhausted = true;
};

kiriview::MediaEntrySourceOpenResult openLibArchiveMediaEntrySource(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
    const kiriview::MediaEntrySourceOpenContext& context)
{
    return LibArchiveMediaEntrySource::create(openedCollectionScope, context);
}
}

namespace kiriview::MediaEntrySourceBackendDetail {
const MediaEntrySourceBackendOperations* libArchiveMediaEntrySourceBackendOperations()
{
    static const MediaEntrySourceBackendOperations operations {
        openLibArchiveMediaEntrySource,
    };
    return &operations;
}
}
