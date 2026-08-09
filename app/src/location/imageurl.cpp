// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imageurl.h"

#include "archive/archiveformat.h"
#include "archive/archivepath.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "location/documentportalpathvalidation_p.h"
#include "navigation/navigationlogging.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <cerrno>
#include <fcntl.h>
#include <linux/magic.h>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <utility>

namespace kiriview::NavigationSourceDetail {
bool isAuthenticatedDocumentPortalMount(const DocumentPortalMountFacts& facts)
{
    return facts.entryFileSystemType == FUSE_SUPER_MAGIC
        && facts.rootFileSystemType == FUSE_SUPER_MAGIC && facts.entryDevice == facts.rootDevice
        && facts.parentDevice != facts.rootDevice;
}

std::optional<QString> validatedDocumentPortalHostPath(
    QByteArray attributeValue, const QString& requestedLocalPath)
{
    if (attributeValue.endsWith('\0')) {
        attributeValue.chop(1);
    }
    if (attributeValue.isEmpty() || attributeValue.contains('\0')) {
        return std::nullopt;
    }

    const QString hostPath = QFile::decodeName(attributeValue);
    if (hostPath.isEmpty() || !hostPath.startsWith(QLatin1Char('/'))
        || !QDir::isAbsolutePath(hostPath) || QDir::cleanPath(hostPath) != hostPath
        || QFile::encodeName(hostPath) != attributeValue
        || hostPath == QDir::cleanPath(requestedLocalPath)) {
        return std::nullopt;
    }

    return hostPath;
}
}

namespace {
constexpr const char* documentPortalHostPathAttribute = "user.document-portal.host-path";
constexpr qsizetype maximumDocumentPortalHostPathBytes = 64 * 1024;

class ScopedFileDescriptor final
{
public:
    ScopedFileDescriptor() = default;

    explicit ScopedFileDescriptor(int descriptor)
        : m_descriptor(descriptor)
    {
    }

    ~ScopedFileDescriptor()
    {
        if (m_descriptor >= 0) {
            ::close(m_descriptor);
        }
    }

    ScopedFileDescriptor(const ScopedFileDescriptor&) = delete;
    ScopedFileDescriptor& operator=(const ScopedFileDescriptor&) = delete;

    ScopedFileDescriptor(ScopedFileDescriptor&& other) noexcept
        : m_descriptor(std::exchange(other.m_descriptor, -1))
    {
    }

    ScopedFileDescriptor& operator=(ScopedFileDescriptor&& other) noexcept
    {
        if (this != &other) {
            if (m_descriptor >= 0) {
                ::close(m_descriptor);
            }
            m_descriptor = std::exchange(other.m_descriptor, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const { return m_descriptor; }
    [[nodiscard]] explicit operator bool() const { return m_descriptor >= 0; }

private:
    int m_descriptor = -1;
};

QString runtimeDirForNavigationSource() { return QFile::decodeName(qgetenv("XDG_RUNTIME_DIR")); }

QStringList documentPortalRoots(const QString& runtimeDir)
{
    QStringList roots;
    if (runtimeDir.startsWith(QLatin1Char('/')) && QDir::isAbsolutePath(runtimeDir)) {
        roots.append(QDir::cleanPath(QDir(runtimeDir).filePath(QStringLiteral("doc"))));
    }
    roots.append(QStringLiteral("/run/flatpak/doc"));
    roots.removeDuplicates();
    return roots;
}

bool isDescendantPath(const QString& localPath, const QString& rootPath)
{
    QString rootPrefix = QDir::cleanPath(rootPath);
    if (!localPath.startsWith(QLatin1Char('/')) || !rootPrefix.startsWith(QLatin1Char('/'))
        || !QDir::isAbsolutePath(localPath) || !QDir::isAbsolutePath(rootPrefix)) {
        return false;
    }
    if (!rootPrefix.endsWith(QLatin1Char('/'))) {
        rootPrefix.append(QLatin1Char('/'));
    }
    return QDir::cleanPath(localPath).startsWith(rootPrefix);
}

QStringList matchingDocumentPortalRoots(const QString& localPath, const QString& runtimeDir)
{
    QStringList matchingRoots;
    for (const QString& rootPath : documentPortalRoots(runtimeDir)) {
        if (isDescendantPath(localPath, rootPath)) {
            matchingRoots.append(rootPath);
        }
    }
    return matchingRoots;
}

ScopedFileDescriptor openLocalPath(const QString& localPath, int flags)
{
    const QByteArray encodedPath = QFile::encodeName(localPath);
    if (encodedPath.isEmpty() || encodedPath.contains('\0')) {
        return {};
    }
    return ScopedFileDescriptor(::open(encodedPath.constData(), flags));
}

bool descriptorBelongsToPortalMount(
    const QString& rootPath, const struct stat& entryStatus, const struct statfs& entryFileSystem)
{
    const ScopedFileDescriptor rootDescriptor
        = openLocalPath(rootPath, O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NONBLOCK);
    if (!rootDescriptor) {
        return false;
    }

    struct stat rootStatus {};
    struct statfs rootFileSystem {};
    if (::fstat(rootDescriptor.get(), &rootStatus) != 0
        || ::fstatfs(rootDescriptor.get(), &rootFileSystem) != 0) {
        return false;
    }

    const ScopedFileDescriptor parentDescriptor = ScopedFileDescriptor(
        ::openat(rootDescriptor.get(), "..", O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NONBLOCK));
    struct stat parentStatus {};
    if (!parentDescriptor || ::fstat(parentDescriptor.get(), &parentStatus) != 0) {
        return false;
    }

    return kiriview::NavigationSourceDetail::isAuthenticatedDocumentPortalMount(
        kiriview::NavigationSourceDetail::DocumentPortalMountFacts {
            entryFileSystem.f_type,
            rootFileSystem.f_type,
            entryStatus.st_dev,
            rootStatus.st_dev,
            parentStatus.st_dev,
        });
}

std::optional<ScopedFileDescriptor> authenticatedDocumentPortalEntry(
    const QString& localPath, const QString& runtimeDir)
{
    const QString cleanLocalPath = QDir::cleanPath(localPath);
    const QStringList matchingRoots = matchingDocumentPortalRoots(cleanLocalPath, runtimeDir);
    if (matchingRoots.isEmpty()) {
        return std::nullopt;
    }

    ScopedFileDescriptor entryDescriptor
        = openLocalPath(cleanLocalPath, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    struct stat entryStatus {};
    struct statfs entryFileSystem {};
    if (!entryDescriptor || ::fstat(entryDescriptor.get(), &entryStatus) != 0
        || ::fstatfs(entryDescriptor.get(), &entryFileSystem) != 0) {
        return std::nullopt;
    }

    for (const QString& rootPath : matchingRoots) {
        if (descriptorBelongsToPortalMount(rootPath, entryStatus, entryFileSystem)) {
            return std::optional<ScopedFileDescriptor>(std::move(entryDescriptor));
        }
    }
    return std::nullopt;
}

QUrl navigationUrlForLocalPath(const QString& localPath, const QString& runtimeDir)
{
    const std::optional<QUrl> kioUrl
        = kiriview::kioFuseArchiveUrlForLocalPath(localPath, runtimeDir);
    if (kioUrl.has_value()) {
        return kioUrl.value();
    }

    return QUrl::fromLocalFile(localPath);
}

QUrl normalizedContainerBaseUrl(const QUrl& url)
{
    QUrl normalizedUrl = url.adjusted(QUrl::NormalizePathSegments);
    normalizedUrl.setQuery(QString());
    normalizedUrl.setFragment(QString());
    return normalizedUrl;
}

std::optional<QString> documentPortalHostPath(const QUrl& url, const QString& runtimeDir)
{
    if (!url.isLocalFile()) {
        return std::nullopt;
    }

    const QString localPath = url.toLocalFile();
    const std::optional<ScopedFileDescriptor> entryDescriptor
        = authenticatedDocumentPortalEntry(localPath, runtimeDir);
    if (!entryDescriptor.has_value()) {
        return std::nullopt;
    }

    auto isNegativeErrno = [](int error) {
        return error == ENODATA
#ifdef ENOATTR
            || error == ENOATTR
#endif
            || error == ENOTSUP
#if EOPNOTSUPP != ENOTSUP
            || error == EOPNOTSUPP
#endif
            ;
    };

    for (int attempt = 0; attempt < 2; ++attempt) {
        // File dialogs can return document-portal URLs; navigation needs the real directory.
        const ssize_t valueSize
            = fgetxattr(entryDescriptor->get(), documentPortalHostPathAttribute, nullptr, 0);
        const int sizeErrno = errno;
        if (valueSize <= 0) {
            if (valueSize < 0 && !isNegativeErrno(sizeErrno)) {
                qCDebug(kiriviewNavigationLog)
                    << "document portal host path probe failed"
                    << "url" << kiriview::diagnosticSourceReference(url) << "errno" << sizeErrno;
            }
            return std::nullopt;
        }
        if (valueSize > maximumDocumentPortalHostPathBytes) {
            qCDebug(kiriviewNavigationLog)
                << "document portal host path read failed"
                << "url" << kiriview::diagnosticSourceReference(url) << "reason"
                << "attribute-too-large";
            return std::nullopt;
        }

        QByteArray value;
        value.resizeForOverwrite(static_cast<qsizetype>(valueSize));
        const ssize_t bytesRead = fgetxattr(entryDescriptor->get(), documentPortalHostPathAttribute,
            value.data(), static_cast<std::size_t>(value.size()));
        const int readErrno = errno;
        if (bytesRead < 0 && readErrno == ERANGE && attempt == 0) {
            continue;
        }
        if (bytesRead <= 0) {
            if (bytesRead < 0 && !isNegativeErrno(readErrno)) {
                qCDebug(kiriviewNavigationLog)
                    << "document portal host path read failed"
                    << "url" << kiriview::diagnosticSourceReference(url) << "errno" << readErrno;
            }
            return std::nullopt;
        }
        if (bytesRead > valueSize) {
            if (attempt == 0) {
                continue;
            }
            qCDebug(kiriviewNavigationLog)
                << "document portal host path read failed"
                << "url" << kiriview::diagnosticSourceReference(url) << "reason"
                << "attribute-grew-during-read";
            return std::nullopt;
        }

        value.resize(bytesRead);
        const std::optional<QString> hostPath
            = kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
                std::move(value), localPath);
        if (!hostPath.has_value()) {
            return std::nullopt;
        }

        qCDebug(kiriviewNavigationLog)
            << "document portal host path resolved"
            << "url" << kiriview::diagnosticSourceReference(url) << "hostPath"
            << kiriview::diagnosticPathReference(*hostPath);
        return hostPath;
    }

    return std::nullopt;
}

kiriview::NavigationSourceEntryFacts collectNavigationSourceEntryFacts(const QUrl& url)
{
    const QString runtimeDir = runtimeDirForNavigationSource();
    return kiriview::NavigationSourceEntryFacts {
        documentPortalHostPath(url, runtimeDir),
        runtimeDir,
        url.isLocalFile() && QFileInfo(QDir::cleanPath(url.toLocalFile())).isDir(),
    };
}

kiriview::NavigationSourceEntryKind navigationSourceEntryKind(
    const QUrl& requestedUrl, const kiriview::NavigationSourceEntryFacts& facts)
{
    if (requestedUrl.isLocalFile() && facts.requestedLocalSourceIsDirectory) {
        return kiriview::NavigationSourceEntryKind::Directory;
    }
    if (kiriview::directArchiveOpenMatchForUrl(requestedUrl).has_value()) {
        return kiriview::NavigationSourceEntryKind::Archive;
    }
    return kiriview::NavigationSourceEntryKind::Direct;
}
}

namespace kiriview::NavigationSourceDetail {
bool isDocumentPortalPathCandidate(const QString& localPath, const QString& runtimeDir)
{
    return !matchingDocumentPortalRoots(localPath, runtimeDir).isEmpty();
}
}

namespace kiriview {
ResolvedNavigationSource::ResolvedNavigationSource(QUrl requestedUrl,
    NavigationSourceEntryFacts facts, QUrl navigationUrl, NavigationSourceEntryKind entryKind)
    : m_requestedUrl(std::move(requestedUrl))
    , m_facts(std::move(facts))
    , m_navigationUrl(std::move(navigationUrl))
    , m_entryKind(entryKind)
{
}

bool DirectoryNavigationLocation::isValid() const
{
    return fileUrl.isValid() && !fileUrl.isEmpty() && directoryUrl.isValid()
        && !directoryUrl.isEmpty();
}

QUrl normalizedUrlForIdentity(const QUrl& url) { return url.adjusted(QUrl::NormalizePathSegments); }

QString userVisibleFileNameForUrl(const QUrl& url) { return url.fileName(QUrl::FullyDecoded); }

QString normalizedUrlIdentityKey(const QUrl& url, QUrl::ComponentFormattingOptions options)
{
    return normalizedUrlForIdentity(url).toString(options);
}

std::optional<QUrl> normalizedValidUrlForIdentity(const QUrl& url)
{
    QUrl normalizedUrl = normalizedUrlForIdentity(url);
    if (!normalizedUrl.isValid() || normalizedUrl.isEmpty()) {
        return std::nullopt;
    }

    return normalizedUrl;
}

QUrl normalizedImageUrl(const QUrl& url) { return normalizedUrlForIdentity(url); }

std::optional<QUrl> normalizedValidImageUrl(const QUrl& url)
{
    return normalizedValidUrlForIdentity(url);
}

QUrl normalizedDirectoryUrlForIdentity(const QUrl& url) { return normalizedUrlForIdentity(url); }

QString directoryUrlIdentityKey(const QUrl& url, QUrl::ComponentFormattingOptions options)
{
    return normalizedUrlIdentityKey(normalizedDirectoryUrlForIdentity(url), options);
}

QUrl normalizedFileContainerUrl(const QUrl& url)
{
    QUrl normalizedUrl = normalizedContainerBaseUrl(url);

    if (normalizedUrl.isLocalFile()) {
        normalizedUrl = QUrl::fromLocalFile(QDir::cleanPath(normalizedUrl.toLocalFile()));
    }
    return normalizedUrl;
}

QUrl normalizedDirectoryContainerUrl(const QUrl& url)
{
    QUrl normalizedUrl = normalizedContainerBaseUrl(url);

    QString path = normalizedUrl.path();
    if (!path.endsWith(QLatin1Char('/'))) {
        path += QLatin1Char('/');
        normalizedUrl.setPath(path);
    }
    return normalizedUrl;
}

QUrl parentDirectoryUrlForFileNavigation(const QUrl& url)
{
    return url.adjusted(QUrl::RemoveFilename | QUrl::NormalizePathSegments);
}

QUrl parentUrlForContainerNavigation(const QUrl& containerUrl)
{
    QUrl parentSourceUrl = containerUrl.adjusted(QUrl::NormalizePathSegments);
    QString path = parentSourceUrl.path();
    if (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
        parentSourceUrl.setPath(path);
    }

    return parentSourceUrl.adjusted(QUrl::RemoveFilename | QUrl::NormalizePathSegments);
}

ResolvedNavigationSource resolvedNavigationSource(
    const QUrl& requestedUrl, const NavigationSourceEntryFacts& facts)
{
    QUrl navigationUrl = requestedUrl;
    if (requestedUrl.isLocalFile() && facts.documentPortalHostPath.has_value()) {
        const QString localPath = requestedUrl.toLocalFile();
        const QString& hostPath = facts.documentPortalHostPath.value();
        if (!hostPath.isEmpty() && hostPath != localPath) {
            navigationUrl = navigationUrlForLocalPath(hostPath, facts.runtimeDir);
        }
    } else if (requestedUrl.isLocalFile()) {
        const std::optional<QUrl> kioUrl
            = kioFuseArchiveUrlForLocalPath(requestedUrl.toLocalFile(), facts.runtimeDir);
        if (kioUrl.has_value()) {
            navigationUrl = *kioUrl;
        }
    }

    return ResolvedNavigationSource(
        requestedUrl, facts, navigationUrl, navigationSourceEntryKind(requestedUrl, facts));
}

NavigationSourceResolver::NavigationSourceResolver()
    : m_provider(collectNavigationSourceEntryFacts)
{
}

NavigationSourceResolver::NavigationSourceResolver(NavigationSourceEntryFactProvider provider)
    : m_provider(std::move(provider))
{
}

ResolvedNavigationSource NavigationSourceResolver::resolveExternalSource(const QUrl& url) const
{
    if (url.isEmpty()) {
        return {};
    }

    const NavigationSourceEntryFacts facts
        = m_provider ? m_provider(url) : NavigationSourceEntryFacts {};
    ResolvedNavigationSource source = resolvedNavigationSource(url, facts);
    if (!sameNormalizedUrl(url, source.navigationUrl())) {
        qCDebug(kiriviewNavigationLog)
            << "navigation source url resolved"
            << "url" << kiriview::diagnosticSourceReference(url) << "navigationUrl"
            << kiriview::diagnosticSourceReference(source.navigationUrl());
    }
    return source;
}

DirectoryNavigationLocation directoryNavigationLocationForSource(
    const ResolvedNavigationSource& source)
{
    const QUrl& fileUrl = source.navigationUrl();
    return DirectoryNavigationLocation {
        fileUrl,
        parentDirectoryUrlForFileNavigation(fileUrl),
    };
}

bool sameNormalizedUrl(const QUrl& left, const QUrl& right)
{
    return left.matches(right, QUrl::NormalizePathSegments);
}

}
