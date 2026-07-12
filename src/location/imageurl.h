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

struct NavigationSourceFacts
{
    std::optional<QString> documentPortalHostPath;
    QString runtimeDir;
};

using NavigationSourceFactProvider = std::function<NavigationSourceFacts(const QUrl&)>;

class ResolvedNavigationSource
{
public:
    ResolvedNavigationSource() = default;
    ResolvedNavigationSource(QUrl requestedUrl, NavigationSourceFacts facts, QUrl navigationUrl);

    const QUrl& requestedUrl() const { return m_requestedUrl; }
    const NavigationSourceFacts& facts() const { return m_facts; }
    const QUrl& navigationUrl() const { return m_navigationUrl; }
    bool isEmpty() const { return m_requestedUrl.isEmpty(); }

private:
    QUrl m_requestedUrl;
    NavigationSourceFacts m_facts;
    QUrl m_navigationUrl;
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
QUrl navigationSourceUrlForFacts(const QUrl& url, const NavigationSourceFacts& facts);
NavigationSourceFacts collectNavigationSourceFacts(const QUrl& url);
ResolvedNavigationSource resolveNavigationSource(
    const QUrl& url, const NavigationSourceFactProvider& provider = {});
QUrl navigationSourceUrl(const QUrl& url);
DirectoryNavigationLocation directoryNavigationLocationForSource(
    const ResolvedNavigationSource& source);
DirectoryNavigationLocation directoryNavigationLocationForFileUrl(const QUrl& url);
bool sameNormalizedUrl(const QUrl& left, const QUrl& right);
bool sameNormalizedUrlOrEmpty(const QUrl& left, const QUrl& right);
bool sameContainerNavigationUrl(const QUrl& left, const QUrl& right);
}

#endif
