// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcebackend_p.h"

#include "scopedfiledescriptor_p.h"

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QIODevice>
#include <QList>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <sys/stat.h>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace {
namespace Backend = kiriview::MediaEntrySourceBackendDetail;
using Backend::ScopedFileDescriptor;

constexpr int maximumSymlinkResolutionCount = 40;
constexpr int maximumPathResolutionStepCount = 4096;
constexpr qsizetype maximumSymlinkTargetSize = qsizetype { 64 } * 1024;

#ifdef O_CLOEXEC
constexpr int closeOnExecFlag = O_CLOEXEC;
#else
constexpr int closeOnExecFlag = 0;
#endif

#ifdef O_NONBLOCK
constexpr int nonBlockingFlag = O_NONBLOCK;
#else
constexpr int nonBlockingFlag = 0;
#endif

struct FileIdentity
{
    dev_t device = 0;
    ino_t inode = 0;
    mode_t type = 0;
};

struct ResolvedDirectoryEntry
{
    ScopedFileDescriptor fileDescriptor;
    FileIdentity identity;
    qint64 size = -1;
};

struct DirectoryPathResolution
{
    std::optional<ResolvedDirectoryEntry> entry;
    QString diagnosticDetail;
};

struct DirectoryEntryAuthority
{
    QByteArray rawRelativePath;
    FileIdentity identity;
};

struct DirectoryCollectionMetadata
{
    std::vector<kiriview::ImageDocumentPageCandidate> candidates;
    std::map<QString, DirectoryEntryAuthority> authorityByPath;
};

struct DirectoryCollectionScan
{
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope;
    int rootFileDescriptor = -1;
    DirectoryCollectionMetadata metadata;
    std::set<std::pair<dev_t, ino_t>> activeDirectories;
    QString diagnosticDetail;
};

struct OpenDirectoryRootResult
{
    ScopedFileDescriptor fileDescriptor;
    FileIdentity identity;
    QString diagnosticDetail;
};

class DirectoryStreamCloser final
{
public:
    void operator()(DIR* stream) const
    {
        if (stream != nullptr) {
            ::closedir(stream);
        }
    }
};

using DirectoryStream = std::unique_ptr<DIR, DirectoryStreamCloser>;

QString systemErrorString(int errorNumber)
{
    return QString::fromLocal8Bit(std::strerror(errorNumber));
}

FileIdentity fileIdentity(const struct stat& status)
{
    return FileIdentity {
        status.st_dev,
        status.st_ino,
        status.st_mode & S_IFMT,
    };
}

bool sameFileIdentity(const FileIdentity& left, const FileIdentity& right)
{
    return left.device == right.device && left.inode == right.inode && left.type == right.type;
}

bool pathResolutionFailureIsSystemic(int errorNumber)
{
    return errorNumber == EIO || errorNumber == EMFILE || errorNumber == ENFILE
        || errorNumber == ENOMEM;
}

DirectoryPathResolution pathResolutionFailure(int errorNumber)
{
    if (!pathResolutionFailureIsSystemic(errorNumber)) {
        return {};
    }
    return DirectoryPathResolution { {}, systemErrorString(errorNumber) };
}

ScopedFileDescriptor duplicateFileDescriptor(int fileDescriptor)
{
#ifdef F_DUPFD_CLOEXEC
    return ScopedFileDescriptor(::fcntl(fileDescriptor, F_DUPFD_CLOEXEC, 0));
#else
    const int duplicate = ::dup(fileDescriptor);
    if (duplicate >= 0) {
        ::fcntl(duplicate, F_SETFD, FD_CLOEXEC);
    }
    return ScopedFileDescriptor(duplicate);
#endif
}

std::optional<QByteArray> readSymbolicLinkTarget(
    int directoryFileDescriptor, const QByteArray& name, qint64 sizeHint, int* errorNumber)
{
    if (sizeHint >= maximumSymlinkTargetSize) {
        *errorNumber = ENAMETOOLONG;
        return std::nullopt;
    }
    qsizetype capacity = sizeHint > 0 ? static_cast<qsizetype>(sizeHint) + 1 : 256;

    while (capacity <= maximumSymlinkTargetSize) {
        QByteArray target(capacity, '\0');
        const ssize_t size
            = ::readlinkat(directoryFileDescriptor, name.constData(), target.data(), target.size());
        if (size < 0) {
            *errorNumber = errno;
            return std::nullopt;
        }
        if (size < target.size()) {
            target.resize(static_cast<qsizetype>(size));
            return target;
        }

        if (capacity == maximumSymlinkTargetSize) {
            break;
        }
        capacity = std::min(capacity * 2, maximumSymlinkTargetSize);
    }

    *errorNumber = ENAMETOOLONG;
    return std::nullopt;
}

void prependPathComponents(std::deque<QByteArray>* pending, const QByteArray& path)
{
    const QList<QByteArray> components = path.split('/');
    for (const QByteArray& component : components | std::views::reverse) {
        pending->push_front(component);
    }
}

DirectoryPathResolution resolveDirectoryRelativePath(
    int rootFileDescriptor, const QByteArray& rawRelativePath)
{
    if (rootFileDescriptor < 0 || rawRelativePath.isEmpty() || rawRelativePath.startsWith('/')) {
        return {};
    }

    std::deque<QByteArray> pending;
    prependPathComponents(&pending, rawRelativePath);
    std::vector<ScopedFileDescriptor> directoryStack;
    directoryStack.push_back(duplicateFileDescriptor(rootFileDescriptor));
    if (!directoryStack.back()) {
        return pathResolutionFailure(errno);
    }

    bool followedSymbolicLink = false;
    int symlinkResolutionCount = 0;
    int resolutionStepCount = 0;
    while (resolutionStepCount < maximumPathResolutionStepCount) {
        ++resolutionStepCount;

        if (pending.empty()) {
            if (!followedSymbolicLink) {
                return {};
            }

            ScopedFileDescriptor currentDirectory
                = duplicateFileDescriptor(directoryStack.back().get());
            if (!currentDirectory) {
                return pathResolutionFailure(errno);
            }
            struct stat status {};
            if (::fstat(currentDirectory.get(), &status) != 0) {
                return pathResolutionFailure(errno);
            }
            return DirectoryPathResolution {
                ResolvedDirectoryEntry {
                    std::move(currentDirectory), fileIdentity(status), status.st_size },
                {},
            };
        }

        QByteArray component = std::move(pending.front());
        pending.pop_front();
        if (component.isEmpty() || component == QByteArrayLiteral(".")) {
            continue;
        }
        if (component == QByteArrayLiteral("..")) {
            if (directoryStack.size() == 1) {
                return {};
            }
            directoryStack.pop_back();
            continue;
        }

        struct stat observedStatus {};
        if (::fstatat(directoryStack.back().get(), component.constData(), &observedStatus,
                AT_SYMLINK_NOFOLLOW)
            != 0) {
            return pathResolutionFailure(errno);
        }

        if (S_ISLNK(observedStatus.st_mode)) {
            ++symlinkResolutionCount;
            if (symlinkResolutionCount > maximumSymlinkResolutionCount) {
                return {};
            }

            int errorNumber = 0;
            const std::optional<QByteArray> target = readSymbolicLinkTarget(
                directoryStack.back().get(), component, observedStatus.st_size, &errorNumber);
            if (!target.has_value() || target->isEmpty() || target->startsWith('/')) {
                return pathResolutionFailure(errorNumber);
            }

            prependPathComponents(&pending, *target);
            followedSymbolicLink = true;
            continue;
        }

        const FileIdentity observedIdentity = fileIdentity(observedStatus);
        const bool finalComponent = pending.empty();
        int openFlags = O_RDONLY | closeOnExecFlag | O_NOFOLLOW | nonBlockingFlag;
        if (!finalComponent || S_ISDIR(observedStatus.st_mode)) {
            if (!S_ISDIR(observedStatus.st_mode)) {
                return {};
            }
            openFlags |= O_DIRECTORY;
        } else if (!S_ISREG(observedStatus.st_mode)) {
            return {};
        }

        ScopedFileDescriptor opened(
            ::openat(directoryStack.back().get(), component.constData(), openFlags));
        if (!opened) {
            return pathResolutionFailure(errno);
        }

        struct stat openedStatus {};
        if (::fstat(opened.get(), &openedStatus) != 0) {
            return pathResolutionFailure(errno);
        }
        if (!sameFileIdentity(observedIdentity, fileIdentity(openedStatus))) {
            return {};
        }

        if (!finalComponent) {
            directoryStack.push_back(std::move(opened));
            continue;
        }

        return DirectoryPathResolution {
            ResolvedDirectoryEntry {
                std::move(opened), fileIdentity(openedStatus), openedStatus.st_size },
            {},
        };
    }

    return {};
}

bool scanDirectoryCollection(ScopedFileDescriptor directoryFileDescriptor,
    const QByteArray& rawRelativeDirectoryPath, DirectoryCollectionScan* scan)
{
    DIR* rawStream = ::fdopendir(directoryFileDescriptor.get());
    if (rawStream == nullptr) {
        scan->diagnosticDetail = systemErrorString(errno);
        return false;
    }
    static_cast<void>(directoryFileDescriptor.release());
    DirectoryStream stream(rawStream);

    while (true) {
        errno = 0;
        const dirent* directoryEntry = ::readdir(stream.get());
        if (directoryEntry == nullptr) {
            if (errno != 0) {
                scan->diagnosticDetail = systemErrorString(errno);
                return false;
            }
            return true;
        }

        const QByteArray name(directoryEntry->d_name);
        if (name == QByteArrayLiteral(".") || name == QByteArrayLiteral("..")) {
            continue;
        }
        const QByteArray rawRelativePath
            = rawRelativeDirectoryPath.isEmpty() ? name : rawRelativeDirectoryPath + '/' + name;
        DirectoryPathResolution resolution
            = resolveDirectoryRelativePath(scan->rootFileDescriptor, rawRelativePath);
        if (!resolution.diagnosticDetail.isEmpty()) {
            scan->diagnosticDetail = std::move(resolution.diagnosticDetail);
            return false;
        }
        if (!resolution.entry.has_value()) {
            continue;
        }

        ResolvedDirectoryEntry resolved = std::move(*resolution.entry);
        if (resolved.identity.type == S_IFDIR) {
            const std::pair identity { resolved.identity.device, resolved.identity.inode };
            if (scan->activeDirectories.contains(identity)) {
                continue;
            }

            scan->activeDirectories.insert(identity);
            const bool completed = scanDirectoryCollection(
                std::move(resolved.fileDescriptor), rawRelativePath, scan);
            scan->activeDirectories.erase(identity);
            if (!completed) {
                return false;
            }
            continue;
        }
        if (resolved.identity.type != S_IFREG) {
            continue;
        }

        std::optional<kiriview::ImageDocumentPageCandidate> candidate
            = Backend::openedCollectionImageDocumentPageCandidate(
                scan->openedCollectionScope, QFile::decodeName(rawRelativePath));
        if (!candidate.has_value()) {
            continue;
        }

        auto [authority, inserted] = scan->metadata.authorityByPath.emplace(
            candidate->name, DirectoryEntryAuthority { rawRelativePath, resolved.identity });
        if (inserted) {
            scan->metadata.candidates.push_back(std::move(*candidate));
        }
    }
}

OpenDirectoryRootResult openDirectoryCollectionRoot(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    if (!openedCollectionScope.isDirectory()) {
        return OpenDirectoryRootResult {
            {},
            {},
            QStringLiteral("opened collection is not a directory"),
        };
    }

    const QString path = openedCollectionScope.fileUrl().toLocalFile();
    if (path.isEmpty()) {
        return OpenDirectoryRootResult {
            {},
            {},
            QStringLiteral("directory collection path is empty"),
        };
    }

    const QByteArray encodedPath = QFile::encodeName(path);
    ScopedFileDescriptor fileDescriptor(
        ::open(encodedPath.constData(), O_RDONLY | O_DIRECTORY | closeOnExecFlag));
    if (!fileDescriptor) {
        return OpenDirectoryRootResult { {}, {}, systemErrorString(errno) };
    }

    struct stat status {};
    if (::fstat(fileDescriptor.get(), &status) != 0) {
        return OpenDirectoryRootResult { {}, {}, systemErrorString(errno) };
    }
    if (!S_ISDIR(status.st_mode)) {
        return OpenDirectoryRootResult {
            {},
            {},
            QStringLiteral("directory collection root is not a directory"),
        };
    }

    return OpenDirectoryRootResult {
        std::move(fileDescriptor),
        fileIdentity(status),
        {},
    };
}

std::optional<DirectoryCollectionMetadata> scanDirectoryCollectionMetadata(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope, int rootFileDescriptor,
    const FileIdentity& rootIdentity, QString* diagnosticDetail)
{
    ScopedFileDescriptor scanRoot = duplicateFileDescriptor(rootFileDescriptor);
    if (!scanRoot) {
        *diagnosticDetail = systemErrorString(errno);
        return std::nullopt;
    }

    DirectoryCollectionScan scan {
        openedCollectionScope,
        rootFileDescriptor,
    };
    scan.activeDirectories.emplace(rootIdentity.device, rootIdentity.inode);
    if (!scanDirectoryCollection(std::move(scanRoot), {}, &scan)) {
        *diagnosticDetail = std::move(scan.diagnosticDetail);
        return std::nullopt;
    }

    return std::move(scan.metadata);
}

class DirectoryCollectionMediaEntrySource final
    : public Backend::MediaEntrySourceWithCandidateSnapshot
{
public:
    DirectoryCollectionMediaEntrySource(
        kiriview::OpenedCollectionScopeLocation openedCollectionScope,
        ScopedFileDescriptor rootFileDescriptor, DirectoryCollectionMetadata metadata)
        : Backend::MediaEntrySourceWithCandidateSnapshot(std::move(openedCollectionScope),
              kiriview::MediaEntrySourceBackendKind::Directory, std::move(metadata.candidates))
        , m_rootFileDescriptor(std::move(rootFileDescriptor))
        , m_authorityByPath(std::move(metadata.authorityByPath))
    {
    }
    ~DirectoryCollectionMediaEntrySource() override = default;

protected:
    kiriview::MediaEntrySourceImageDataResult loadAuthorizedImageData(
        const kiriview::ImageDocumentPageCandidate& candidate) override
    {
        const auto authority = m_authorityByPath.find(candidate.name);
        if (authority == m_authorityByPath.cend()) {
            return entryNotFound<kiriview::MediaEntrySourceImageDataResult>(
                kiriview::MediaEntrySourceOperation::ReadImageData, candidate.name);
        }

        DirectoryPathResolution resolution = resolveDirectoryRelativePath(
            m_rootFileDescriptor.get(), authority->second.rawRelativePath);
        if (!resolution.diagnosticDetail.isEmpty()) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::Directory,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                    std::move(resolution.diagnosticDetail), candidate.name));
        }
        if (!resolution.entry.has_value()
            || !sameFileIdentity(resolution.entry->identity, authority->second.identity)
            || resolution.entry->identity.type != S_IFREG) {
            return entryNotFound<kiriview::MediaEntrySourceImageDataResult>(
                kiriview::MediaEntrySourceOperation::ReadImageData, candidate.name);
        }

        ResolvedDirectoryEntry resolved = std::move(*resolution.entry);
        QFile file;
        if (!file.open(
                resolved.fileDescriptor.get(), QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::Directory,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                    file.errorString(), candidate.name));
        }
        static_cast<void>(resolved.fileDescriptor.release());

        QByteArray data = file.readAll();
        if (file.error() != QFile::NoError || data.size() != resolved.size) {
            return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceImageDataResult>(
                Backend::mediaEntrySourceError(
                    kiriview::MediaEntrySourceErrorCause::EntryReadFailed,
                    kiriview::MediaEntrySourceBackendKind::Directory,
                    kiriview::MediaEntrySourceOperation::ReadImageData, openedCollectionScope(),
                    file.errorString(), candidate.name));
        }

        return Backend::mediaEntrySourceImageDataResult(std::move(data));
    }

    kiriview::MediaEntrySourceVideoPlaybackDeviceResult loadAuthorizedVideoPlaybackDevice(
        const kiriview::ImageDocumentPageCandidate& candidate) override
    {
        const auto authority = m_authorityByPath.find(candidate.name);
        if (authority == m_authorityByPath.cend()) {
            return entryNotFound<kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(
                kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice, candidate.name);
        }

        DirectoryPathResolution resolution = resolveDirectoryRelativePath(
            m_rootFileDescriptor.get(), authority->second.rawRelativePath);
        if (!resolution.diagnosticDetail.isEmpty()) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::VideoPlaybackUnsupported,
                kiriview::MediaEntrySourceBackendKind::Directory,
                kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice,
                openedCollectionScope(), std::move(resolution.diagnosticDetail), candidate.name));
        }
        if (!resolution.entry.has_value()
            || !sameFileIdentity(resolution.entry->identity, authority->second.identity)
            || resolution.entry->identity.type != S_IFREG) {
            return entryNotFound<kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(
                kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice, candidate.name);
        }

        ResolvedDirectoryEntry resolved = std::move(*resolution.entry);
        auto file = std::make_unique<QFile>();
        if (!file->open(
                resolved.fileDescriptor.get(), QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            return Backend::mediaEntrySourceErrorResult<
                kiriview::MediaEntrySourceVideoPlaybackDeviceResult>(Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::VideoPlaybackUnsupported,
                kiriview::MediaEntrySourceBackendKind::Directory,
                kiriview::MediaEntrySourceOperation::OpenVideoPlaybackDevice,
                openedCollectionScope(), file->errorString(), candidate.name));
        }
        static_cast<void>(resolved.fileDescriptor.release());

        return Backend::mediaEntrySourceVideoPlaybackDeviceResult(std::move(file));
    }

private:
    template <typename Result>
    Result entryNotFound(kiriview::MediaEntrySourceOperation operation, const QString& entryPath)
    {
        return Backend::mediaEntrySourceErrorResult<Result>(
            Backend::mediaEntrySourceError(kiriview::MediaEntrySourceErrorCause::EntryNotFound,
                kiriview::MediaEntrySourceBackendKind::Directory, operation,
                openedCollectionScope(), {}, entryPath));
    }

    ScopedFileDescriptor m_rootFileDescriptor;
    std::map<QString, DirectoryEntryAuthority> m_authorityByPath;
    Q_DISABLE_COPY_MOVE(DirectoryCollectionMediaEntrySource)
};

kiriview::MediaEntrySourceOpenResult openDirectoryCollectionMediaEntrySource(
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope)
{
    OpenDirectoryRootResult root = openDirectoryCollectionRoot(openedCollectionScope);
    if (!root.fileDescriptor) {
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::CollectionOpenFailed,
                kiriview::MediaEntrySourceBackendKind::Directory,
                kiriview::MediaEntrySourceOperation::OpenCollection, openedCollectionScope,
                root.diagnosticDetail));
    }

    QString diagnosticDetail;
    std::optional<DirectoryCollectionMetadata> metadata = scanDirectoryCollectionMetadata(
        openedCollectionScope, root.fileDescriptor.get(), root.identity, &diagnosticDetail);
    if (!metadata.has_value()) {
        return Backend::mediaEntrySourceErrorResult<kiriview::MediaEntrySourceOpenResult>(
            Backend::mediaEntrySourceError(
                kiriview::MediaEntrySourceErrorCause::CandidateListingFailed,
                kiriview::MediaEntrySourceBackendKind::Directory,
                kiriview::MediaEntrySourceOperation::ListCandidates, openedCollectionScope,
                diagnosticDetail));
    }

    return kiriview::MediaEntrySourcePtr(std::make_shared<DirectoryCollectionMediaEntrySource>(
        openedCollectionScope, std::move(root.fileDescriptor), std::move(*metadata)));
}
}

namespace kiriview::MediaEntrySourceBackendDetail {
const MediaEntrySourceBackendOperations* directoryCollectionMediaEntrySourceBackendOperations()
{
    static const MediaEntrySourceBackendOperations operations {
        openDirectoryCollectionMediaEntrySource,
    };
    return &operations;
}
}
