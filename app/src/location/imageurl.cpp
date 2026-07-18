// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "location/imageurl.h"

#include "archive/archiveformat.h"
#include "archive/archivepath.h"
#include "navigation/navigationlogging.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QtGlobal>
#include <cerrno>
#include <optional>
#include <sys/types.h>
#include <sys/xattr.h>
#include <utility>

namespace {
constexpr const char* documentPortalHostPathAttribute = "user.document-portal.host-path";

QString runtimeDirForNavigationSource() { return QFile::decodeName(qgetenv("XDG_RUNTIME_DIR")); }

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

std::optional<QString> documentPortalHostPath(const QUrl& url)
{
    if (!url.isLocalFile()) {
        return std::nullopt;
    }

    const QString localPath = url.toLocalFile();
    const QByteArray encodedLocalPath = QFile::encodeName(localPath);
    if (encodedLocalPath.isEmpty()) {
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
            = getxattr(encodedLocalPath.constData(), documentPortalHostPathAttribute, nullptr, 0);
        const int sizeErrno = errno;
        if (valueSize <= 0) {
            if (valueSize < 0 && !isNegativeErrno(sizeErrno)) {
                qCDebug(kiriviewNavigationLog) << "document portal host path probe failed"
                                               << "url" << url << "errno" << sizeErrno;
            }
            return std::nullopt;
        }

        QByteArray value;
        value.resize(valueSize);
        const ssize_t bytesRead = getxattr(encodedLocalPath.constData(),
            documentPortalHostPathAttribute, value.data(), static_cast<std::size_t>(value.size()));
        const int readErrno = errno;
        if (bytesRead < 0 && readErrno == ERANGE && attempt == 0) {
            continue;
        }
        if (bytesRead <= 0) {
            if (bytesRead < 0 && !isNegativeErrno(readErrno)) {
                qCDebug(kiriviewNavigationLog) << "document portal host path read failed"
                                               << "url" << url << "errno" << readErrno;
            }
            return std::nullopt;
        }
        if (bytesRead > valueSize) {
            if (attempt == 0) {
                continue;
            }
            qCDebug(kiriviewNavigationLog) << "document portal host path read failed"
                                           << "url" << url << "reason"
                                           << "attribute-grew-during-read";
            return std::nullopt;
        }

        value.resize(bytesRead);
        if (value.endsWith('\0')) {
            value.chop(1);
        }

        const QString hostPath = QFile::decodeName(value);
        if (hostPath.isEmpty() || hostPath == localPath) {
            return std::nullopt;
        }

        qCDebug(kiriviewNavigationLog) << "document portal host path resolved"
                                       << "url" << url << "hostPath" << hostPath;
        return hostPath;
    }

    return std::nullopt;
}

kiriview::NavigationSourceEntryFacts collectNavigationSourceEntryFacts(const QUrl& url)
{
    return kiriview::NavigationSourceEntryFacts {
        documentPortalHostPath(url),
        runtimeDirForNavigationSource(),
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

QString normalizedUrlIdentityKey(const QUrl& url, QUrl::ComponentFormattingOptions options)
{
    return normalizedUrlForIdentity(url).toString(options);
}

std::optional<QUrl> normalizedValidUrlForIdentity(const QUrl& url)
{
    const QUrl normalizedUrl = normalizedUrlForIdentity(url);
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
        qCDebug(kiriviewNavigationLog) << "navigation source url resolved"
                                       << "url" << url << "navigationUrl" << source.navigationUrl();
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

bool sameNormalizedUrlOrEmpty(const QUrl& left, const QUrl& right)
{
    if (left.isEmpty() || right.isEmpty()) {
        return left.isEmpty() && right.isEmpty();
    }

    return sameNormalizedUrl(left, right);
}

bool sameContainerNavigationUrl(const QUrl& left, const QUrl& right)
{
    return !left.isEmpty() && !right.isEmpty() && sameNormalizedUrl(left, right);
}
}
