// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGELOCATION_H
#define KIRIVIEW_IMAGELOCATION_H

#include "location/imageurl.h"

#include <QString>
#include <QUrl>
#include <utility>

namespace kiriview {
class ImageLocation
{
public:
    ImageLocation() = default;
    explicit ImageLocation(QUrl url)
        : m_source(normalizedSource(std::move(url)))
    {
    }

    static ImageLocation fromUrl(QUrl url) { return ImageLocation(std::move(url)); }
    static ImageLocation fromResolvedSource(ResolvedNavigationSource source)
    {
        ImageLocation location;
        location.m_source = std::move(source);
        return location;
    }

    const QUrl& url() const { return m_source.requestedUrl(); }
    const ResolvedNavigationSource& source() const { return m_source; }
    bool isEmpty() const { return m_source.isEmpty(); }

    friend bool operator==(const ImageLocation& left, const ImageLocation& right)
    {
        return left.url() == right.url();
    }
    friend bool operator!=(const ImageLocation& left, const ImageLocation& right)
    {
        return !(left == right);
    }

private:
    static ResolvedNavigationSource normalizedSource(QUrl url)
    {
        const QUrl normalizedUrl = normalizedUrlForIdentity(url);
        return ResolvedNavigationSource(
            normalizedUrl, NavigationSourceEntryFacts {}, normalizedUrl);
    }

    ResolvedNavigationSource m_source;
};

class ContainerLocation
{
public:
    ContainerLocation() = default;
    explicit ContainerLocation(QUrl url)
        : m_url(normalizedUrlForIdentity(url))
    {
    }

    static ContainerLocation none() { return ContainerLocation(); }
    static ContainerLocation fromUrl(QUrl url) { return ContainerLocation(std::move(url)); }

    const QUrl& url() const { return m_url; }
    bool isEmpty() const { return m_url.isEmpty(); }

    friend bool operator==(const ContainerLocation& left, const ContainerLocation& right)
    {
        return left.m_url == right.m_url;
    }
    friend bool operator!=(const ContainerLocation& left, const ContainerLocation& right)
    {
        return !(left == right);
    }

private:
    QUrl m_url;
};

enum class OpenedCollectionScopeKind {
    ComicBookArchive,
    GeneralArchive,
    Directory,
};

class OpenedCollectionScopeLocation
{
public:
    OpenedCollectionScopeLocation() = default;
    OpenedCollectionScopeLocation(
        ResolvedNavigationSource source, QUrl rootUrl, OpenedCollectionScopeKind kind)
        : m_source(std::move(source))
        , m_rootUrl(normalizedUrlForIdentity(rootUrl))
        , m_kind(kind)
    {
    }

    static OpenedCollectionScopeLocation none() { return OpenedCollectionScopeLocation(); }
    static OpenedCollectionScopeLocation fromUrls(
        QUrl fileUrl, QUrl rootUrl, OpenedCollectionScopeKind kind)
    {
        NavigationSourceEntryFacts facts;
        const QUrl normalizedFileUrl = normalizedUrlForIdentity(fileUrl);
        return OpenedCollectionScopeLocation(
            ResolvedNavigationSource(normalizedFileUrl, std::move(facts), normalizedFileUrl),
            std::move(rootUrl), kind);
    }

    static OpenedCollectionScopeLocation fromResolvedSource(
        ResolvedNavigationSource source, QUrl rootUrl, OpenedCollectionScopeKind kind)
    {
        return OpenedCollectionScopeLocation(std::move(source), std::move(rootUrl), kind);
    }

    const QUrl& fileUrl() const { return m_source.requestedUrl(); }
    const ResolvedNavigationSource& source() const { return m_source; }
    const QUrl& navigationSourceUrl() const { return m_source.navigationUrl(); }
    const QUrl& rootUrl() const { return m_rootUrl; }
    OpenedCollectionScopeKind kind() const { return m_kind; }
    bool isEmpty() const { return m_source.isEmpty() || m_rootUrl.isEmpty(); }
    bool isComicBook() const
    {
        return !isEmpty() && m_kind == OpenedCollectionScopeKind::ComicBookArchive;
    }
    bool isDirectory() const
    {
        return !isEmpty() && m_kind == OpenedCollectionScopeKind::Directory;
    }

    friend bool operator==(
        const OpenedCollectionScopeLocation& left, const OpenedCollectionScopeLocation& right)
    {
        return left.fileUrl() == right.fileUrl() && left.m_rootUrl == right.m_rootUrl
            && left.m_kind == right.m_kind;
    }
    friend bool operator!=(
        const OpenedCollectionScopeLocation& left, const OpenedCollectionScopeLocation& right)
    {
        return !(left == right);
    }

private:
    ResolvedNavigationSource m_source;
    QUrl m_rootUrl;
    OpenedCollectionScopeKind m_kind = OpenedCollectionScopeKind::GeneralArchive;
};

bool sameOpenedCollectionScopeLocation(
    const OpenedCollectionScopeLocation& left, const OpenedCollectionScopeLocation& right);

class DisplayedImageLocation
{
public:
    DisplayedImageLocation() = default;
    DisplayedImageLocation(ImageLocation image, OpenedCollectionScopeLocation openedCollectionScope)
        : m_image(std::move(image))
        , m_openedCollectionScope(std::move(openedCollectionScope))
    {
    }

    static DisplayedImageLocation fromUrl(QUrl imageUrl)
    {
        return DisplayedImageLocation { ImageLocation::fromUrl(std::move(imageUrl)),
            OpenedCollectionScopeLocation::none() };
    }

    static DisplayedImageLocation fromUrl(
        QUrl imageUrl, OpenedCollectionScopeLocation openedCollectionScope)
    {
        return DisplayedImageLocation { ImageLocation::fromUrl(std::move(imageUrl)),
            std::move(openedCollectionScope) };
    }

    static DisplayedImageLocation fromResolvedSource(ResolvedNavigationSource source,
        OpenedCollectionScopeLocation openedCollectionScope = OpenedCollectionScopeLocation::none())
    {
        return DisplayedImageLocation { ImageLocation::fromResolvedSource(std::move(source)),
            std::move(openedCollectionScope) };
    }

    static DisplayedImageLocation fromOpenedCollectionScope(
        QUrl imageUrl, OpenedCollectionScopeLocation openedCollectionScope)
    {
        return fromUrl(std::move(imageUrl), std::move(openedCollectionScope));
    }

    const QUrl& imageUrl() const { return m_image.url(); }
    const ResolvedNavigationSource& imageSource() const { return m_image.source(); }
    const OpenedCollectionScopeLocation& openedCollectionScope() const
    {
        return m_openedCollectionScope;
    }
    const QUrl& openedCollectionScopeSourceUrl() const { return m_openedCollectionScope.fileUrl(); }
    const QUrl& openedCollectionScopeRootUrl() const { return m_openedCollectionScope.rootUrl(); }
    bool isEmpty() const { return m_image.isEmpty(); }
    void setImageUrl(QUrl url) { m_image = ImageLocation::fromUrl(std::move(url)); }
    void setOpenedCollectionScope(OpenedCollectionScopeLocation openedCollectionScope)
    {
        m_openedCollectionScope = std::move(openedCollectionScope);
    }
    void clearOpenedCollectionScope()
    {
        m_openedCollectionScope = OpenedCollectionScopeLocation::none();
    }

    friend bool operator==(const DisplayedImageLocation& left, const DisplayedImageLocation& right)
    {
        return left.m_image == right.m_image
            && left.m_openedCollectionScope == right.m_openedCollectionScope;
    }
    friend bool operator!=(const DisplayedImageLocation& left, const DisplayedImageLocation& right)
    {
        return !(left == right);
    }

private:
    ImageLocation m_image;
    OpenedCollectionScopeLocation m_openedCollectionScope;
};

QString displayScopeIdentityForLocation(const DisplayedImageLocation& location);

}

#endif
