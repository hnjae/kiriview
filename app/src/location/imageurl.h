// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEURL_H
#define KIRIVIEW_IMAGEURL_H

#include <QString>
#include <QUrl>
#include <functional>
#include <optional>

namespace kiriview {
struct DirectoryNavigationLocation
{
    QUrl fileUrl;
    QUrl directoryUrl;

    bool isValid() const;
};

enum class NavigationSourceEntryKind {
    Direct,
    Directory,
    Archive,
};

struct NavigationSourceEntryFacts
{
    std::optional<QString> documentPortalHostPath;
    QString runtimeDir;
    bool requestedLocalSourceIsDirectory = false;
};

using NavigationSourceEntryFactProvider = std::function<NavigationSourceEntryFacts(const QUrl&)>;

class ResolvedNavigationSource;

class NavigationSourceResolver
{
public:
    NavigationSourceResolver();
    explicit NavigationSourceResolver(NavigationSourceEntryFactProvider provider);
    ResolvedNavigationSource resolveExternalSource(const QUrl& url) const;

private:
    NavigationSourceEntryFactProvider m_provider;
};

class ResolvedNavigationSource
{
public:
    ResolvedNavigationSource() = default;
    ResolvedNavigationSource(QUrl requestedUrl, NavigationSourceEntryFacts facts,
        QUrl navigationUrl,
        NavigationSourceEntryKind entryKind = NavigationSourceEntryKind::Direct);

    const QUrl& requestedUrl() const { return m_requestedUrl; }
    const NavigationSourceEntryFacts& facts() const { return m_facts; }
    const QUrl& navigationUrl() const { return m_navigationUrl; }
    NavigationSourceEntryKind entryKind() const { return m_entryKind; }
    bool isEmpty() const { return m_requestedUrl.isEmpty(); }

private:
    QUrl m_requestedUrl;
    NavigationSourceEntryFacts m_facts;
    QUrl m_navigationUrl;
    NavigationSourceEntryKind m_entryKind = NavigationSourceEntryKind::Direct;
};

QUrl normalizedUrlForIdentity(const QUrl& url);
QString normalizedUrlIdentityKey(
    const QUrl& url, QUrl::ComponentFormattingOptions options = QUrl::PrettyDecoded);
std::optional<QUrl> normalizedValidUrlForIdentity(const QUrl& url);
QUrl normalizedImageUrl(const QUrl& url);
std::optional<QUrl> normalizedValidImageUrl(const QUrl& url);
QUrl normalizedDirectoryUrlForIdentity(const QUrl& url);
QString directoryUrlIdentityKey(
    const QUrl& url, QUrl::ComponentFormattingOptions options = QUrl::FullyEncoded);
QUrl normalizedFileContainerUrl(const QUrl& url);
QUrl normalizedDirectoryContainerUrl(const QUrl& url);
QUrl parentDirectoryUrlForFileNavigation(const QUrl& url);
QUrl parentUrlForContainerNavigation(const QUrl& containerUrl);
ResolvedNavigationSource resolvedNavigationSource(
    const QUrl& requestedUrl, const NavigationSourceEntryFacts& facts);
DirectoryNavigationLocation directoryNavigationLocationForSource(
    const ResolvedNavigationSource& source);
bool sameNormalizedUrl(const QUrl& left, const QUrl& right);
}

#endif
