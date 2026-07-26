// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcebackend_p.h"

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

class ScopedFileDescriptor final
{
public:
    ScopedFileDescriptor() = default;

    explicit ScopedFileDescriptor(int fileDescriptor)
        : m_fileDescriptor(fileDescriptor)
    {
    }

    ~ScopedFileDescriptor() { close(); }

    ScopedFileDescriptor(const ScopedFileDescriptor&) = delete;
    ScopedFileDescriptor& operator=(const ScopedFileDescriptor&) = delete;
    ScopedFileDescriptor(ScopedFileDescriptor&& other) noexcept
        : m_fileDescriptor(std::exchange(other.m_fileDescriptor, -1))
    {
    }
    ScopedFileDescriptor& operator=(ScopedFileDescriptor&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        close();
        m_fileDescriptor = std::exchange(other.m_fileDescriptor, -1);
        return *this;
    }

    [[nodiscard]] int get() const { return m_fileDescriptor; }

    explicit operator bool() const { return m_fileDescriptor >= 0; }

private:
    void close()
    {
        if (m_fileDescriptor >= 0) {
            ::close(m_fileDescriptor);
            m_fileDescriptor = -1;
        }
    }

    int m_fileDescriptor = -1;
};

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
    archive* reader, archive_entry* entry)
{
    QByteArray data;
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

        data.append(buffer, static_cast<qsizetype>(bytesRead));
    }

    if (archive_entry_size_is_set(entry)) {
        const la_int64_t expectedSize = archive_entry_size(entry);
        if (expectedSize >= 0 && static_cast<qint64>(data.size()) != expectedSize) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope,
                    QStringLiteral("libarchive entry size did not match the declared size"),
                    entryPath));
        }
    }

    return Backend::mediaEntrySourceImageDataResult(std::move(data));
}

struct LibArchiveMediaEntrySourceMetadata
{
    std::vector<kiriview::ImageDocumentPageCandidate> candidates;
    std::map<QString, int> entryOrderByPath;
};

std::optional<LibArchiveMediaEntrySourceMetadata> scanLibArchiveMediaEntrySourceMetadata(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, archive* reader,
    QString* diagnosticDetail)
{
    LibArchiveMediaEntrySourceMetadata metadata;
    archive_entry* entry = nullptr;
    int entryOrder = 0;

    while (true) {
        const int status = archive_read_next_header(reader, &entry);
        if (status == ARCHIVE_EOF) {
            return metadata;
        }
        if (status != ARCHIVE_OK) {
            *diagnosticDetail = libArchiveErrorString(
                reader, QStringLiteral("libarchive could not read the next archive header"));
            return std::nullopt;
        }

        const int currentEntryOrder = entryOrder;
        ++entryOrder;

        if (archive_entry_filetype(entry) == AE_IFREG) {
            std::optional<kiriview::ImageDocumentPageCandidate> candidate
                = Backend::openedCollectionImageDocumentPageCandidate(
                    openedCollectionScope, libArchiveEntryPath(entry));
            if (candidate.has_value()) {
                metadata.entryOrderByPath[candidate->name] = currentEntryOrder;
                metadata.candidates.push_back(std::move(*candidate));
            }
        }

        if (!skipLibArchiveEntry(reader, diagnosticDetail)) {
            return std::nullopt;
        }
    }
}

class LibArchiveMediaEntrySource final : public Backend::MediaEntrySourceWithCandidateSnapshot
{
public:
    static kiriview::MediaEntrySourceOpenResult create(
        const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
    {
        OpenArchiveFileResult opened = openArchiveFileDescriptor(openedCollectionScope);
        if (!opened.fileDescriptor) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::CollectionOpenFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::OpenCollection, openedCollectionScope,
                    opened.diagnosticDetail));
        }

        auto source = std::shared_ptr<LibArchiveMediaEntrySource>(new LibArchiveMediaEntrySource(
            openedCollectionScope, std::move(opened.fileDescriptor)));
        QString diagnosticDetail;
        LibArchiveReader reader
            = openLibArchiveReaderOnFd(source->m_archiveFile.get(), &diagnosticDetail);
        if (reader == nullptr) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::CollectionOpenFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::OpenCollection, openedCollectionScope,
                    diagnosticDetail));
        }

        std::optional<LibArchiveMediaEntrySourceMetadata> metadata
            = scanLibArchiveMediaEntrySourceMetadata(
                openedCollectionScope, reader.get(), &diagnosticDetail);
        if (!metadata.has_value()) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::CandidateListingFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ListCandidates, openedCollectionScope,
                    diagnosticDetail));
        }

        source->m_entryOrderByPath = std::move(metadata->entryOrderByPath);
        source->replaceCandidateSnapshot(std::move(metadata->candidates));
        return kiriview::MediaEntrySourcePtr(std::move(source));
    }

    kiriview::MediaEntrySourceImageDataResult loadImageData(const QUrl& imageUrl) override
    {
        const std::optional<QString> entryPath
            = Backend::openedCollectionImageEntryPathForRead(m_openedCollectionScope, imageUrl);
        if (!entryPath.has_value()) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope,
                    imageUrl.toString()));
        }

        const auto entryOrder = m_entryOrderByPath.find(*entryPath);
        if (entryOrder == m_entryOrderByPath.cend()) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope, {},
                    *entryPath));
        }

        return readImageDataAtOrder(entryOrder->second, *entryPath);
    }

    kiriview::MediaEntrySourceVideoPlaybackDeviceResult loadVideoPlaybackDevice(
        const QUrl& videoUrl) override
    {
        const std::optional<QString> entryPath
            = Backend::openedCollectionVideoEntryPathForRead(m_openedCollectionScope, videoUrl);
        if (!entryPath.has_value()) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice,
                    m_openedCollectionScope, videoUrl.toString()));
        }

        const auto entryOrder = m_entryOrderByPath.find(*entryPath);
        if (entryOrder == m_entryOrderByPath.cend()) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(
                Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice,
                    m_openedCollectionScope, {}, *entryPath));
        }

        return Backend::mediaEntrySourceErrorResult<
            kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(Backend::mediaEntrySourceError(
            kiriview::MediaEntrySourceErrorCause::VideoPlaybackUnsupported,
            kiriview::MediaEntrySourceBackendKind::LibArchive,
            kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice, m_openedCollectionScope,
            {}, *entryPath));
    }

private:
    LibArchiveMediaEntrySource(kiriview::OpenedCollectionScopeLocation openedCollectionScope,
        ScopedFileDescriptor archiveFile)
        : Backend::MediaEntrySourceWithCandidateSnapshot({})
        , m_openedCollectionScope(std::move(openedCollectionScope))
        , m_archiveFile(std::move(archiveFile))
    {
    }

    kiriview::MediaEntrySourceImageDataResult readImageDataAtOrder(
        int targetEntryOrder, const QString& targetEntryPath)
    {
        QString diagnosticDetail;
        if (!prepareReaderForEntry(targetEntryOrder, &diagnosticDetail)) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope,
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
                    kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope,
                    QStringLiteral("libarchive reached end of archive before the requested entry"),
                    targetEntryPath));
            }
            if (status != ARCHIVE_OK) {
                return Backend::mediaEntrySourceErrorResult<
                    kiriview::MediaEntrySourceImageDataResult>(Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope,
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
                        kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope,
                        {}, targetEntryPath));
                }

                return readLibArchiveEntryData(
                    m_openedCollectionScope, libArchiveEntryPath(entry), m_reader.get(), entry);
            }

            if (!skipLibArchiveEntry(m_reader.get(), &diagnosticDetail)) {
                return Backend::mediaEntrySourceErrorResult<
                    kiriview::MediaEntrySourceImageDataResult>(Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::LibArchive,
                    kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope,
                    diagnosticDetail, targetEntryPath));
            }
        }

        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
            Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                kiriview::MediaEntrySourceBackendKind::LibArchive,
                kiriview::MediaEntrySourceOperation::ReadImageData, m_openedCollectionScope, {},
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

    kiriview::OpenedCollectionScopeLocation m_openedCollectionScope;
    ScopedFileDescriptor m_archiveFile;
    std::map<QString, int> m_entryOrderByPath;
    LibArchiveReader m_reader { nullptr, archive_read_free };
    int m_nextEntryOrder = 0;
    bool m_readerExhausted = true;
};

kiriview::MediaEntrySourceOpenResult openLibArchiveMediaEntrySource(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    return LibArchiveMediaEntrySource::create(openedCollectionScope);
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
