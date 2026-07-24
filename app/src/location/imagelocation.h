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
    explicit ImageLocation(const QUrl& url)
        : m_source(normalizedSource(url))
    {
    }

    static ImageLocation fromUrl(const QUrl& url) { return ImageLocation(url); }
    static ImageLocation fromResolvedSource(ResolvedNavigationSource source)
    {
        ImageLocation location;
        location.m_source = std::move(source);
        return location;
    }

    [[nodiscard]] const QUrl& url() const { return m_source.requestedUrl(); }
    [[nodiscard]] const ResolvedNavigationSource& source() const { return m_source; }
    [[nodiscard]] bool isEmpty() const { return m_source.isEmpty(); }

    friend bool operator==(const ImageLocation& left, const ImageLocation& right)
    {
        return left.url() == right.url();
    }

private:
    static ResolvedNavigationSource normalizedSource(const QUrl& url)
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
    explicit ContainerLocation(const QUrl& url)
        : m_url(normalizedUrlForIdentity(url))
    {
    }

    static ContainerLocation none() { return ContainerLocation(); }
    static ContainerLocation fromUrl(const QUrl& url) { return ContainerLocation(url); }

    [[nodiscard]] const QUrl& url() const { return m_url; }
    [[nodiscard]] bool isEmpty() const { return m_url.isEmpty(); }

    friend bool operator==(const ContainerLocation& left, const ContainerLocation& right)
    {
        return left.m_url == right.m_url;
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
        ResolvedNavigationSource source, const QUrl& rootUrl, OpenedCollectionScopeKind kind)
        : m_source(std::move(source))
        , m_rootUrl(normalizedUrlForIdentity(rootUrl))
        , m_kind(kind)
    {
    }

    static OpenedCollectionScopeLocation none() { return OpenedCollectionScopeLocation(); }
    static OpenedCollectionScopeLocation fromUrls(
        const QUrl& fileUrl, const QUrl& rootUrl, OpenedCollectionScopeKind kind)
    {
        NavigationSourceEntryFacts facts;
        const QUrl normalizedFileUrl = normalizedUrlForIdentity(fileUrl);
        return OpenedCollectionScopeLocation(
            ResolvedNavigationSource(normalizedFileUrl, std::move(facts), normalizedFileUrl),
            rootUrl, kind);
    }

    static OpenedCollectionScopeLocation fromResolvedSource(
        ResolvedNavigationSource source, const QUrl& rootUrl, OpenedCollectionScopeKind kind)
    {
        return OpenedCollectionScopeLocation(std::move(source), rootUrl, kind);
    }

    [[nodiscard]] const QUrl& fileUrl() const { return m_source.requestedUrl(); }
    [[nodiscard]] const ResolvedNavigationSource& source() const { return m_source; }
    [[nodiscard]] const QUrl& navigationSourceUrl() const { return m_source.navigationUrl(); }
    [[nodiscard]] const QUrl& rootUrl() const { return m_rootUrl; }
    [[nodiscard]] OpenedCollectionScopeKind kind() const { return m_kind; }
    [[nodiscard]] bool isEmpty() const { return m_source.isEmpty() || m_rootUrl.isEmpty(); }
    [[nodiscard]] bool isComicBook() const
    {
        return !isEmpty() && m_kind == OpenedCollectionScopeKind::ComicBookArchive;
    }
    [[nodiscard]] bool isDirectory() const
    {
        return !isEmpty() && m_kind == OpenedCollectionScopeKind::Directory;
    }

    friend bool operator==(
        const OpenedCollectionScopeLocation& left, const OpenedCollectionScopeLocation& right)
    {
        return left.fileUrl() == right.fileUrl() && left.m_rootUrl == right.m_rootUrl
            && left.m_kind == right.m_kind;
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

    static DisplayedImageLocation fromUrl(const QUrl& imageUrl)
    {
        return DisplayedImageLocation { ImageLocation::fromUrl(imageUrl),
            OpenedCollectionScopeLocation::none() };
    }

    static DisplayedImageLocation fromUrl(
        const QUrl& imageUrl, OpenedCollectionScopeLocation openedCollectionScope)
    {
        return DisplayedImageLocation { ImageLocation::fromUrl(imageUrl),
            std::move(openedCollectionScope) };
    }

    static DisplayedImageLocation fromResolvedSource(ResolvedNavigationSource source,
        OpenedCollectionScopeLocation openedCollectionScope = OpenedCollectionScopeLocation::none())
    {
        return DisplayedImageLocation { ImageLocation::fromResolvedSource(std::move(source)),
            std::move(openedCollectionScope) };
    }

    static DisplayedImageLocation fromOpenedCollectionScope(
        const QUrl& imageUrl, OpenedCollectionScopeLocation openedCollectionScope)
    {
        return fromUrl(imageUrl, std::move(openedCollectionScope));
    }

    [[nodiscard]] const QUrl& imageUrl() const { return m_image.url(); }
    [[nodiscard]] const ResolvedNavigationSource& imageSource() const { return m_image.source(); }
    [[nodiscard]] const OpenedCollectionScopeLocation& openedCollectionScope() const
    {
        return m_openedCollectionScope;
    }
    [[nodiscard]] const QUrl& openedCollectionScopeSourceUrl() const
    {
        return m_openedCollectionScope.fileUrl();
    }
    [[nodiscard]] bool isEmpty() const { return m_image.isEmpty(); }
    void setImageUrl(const QUrl& url) { m_image = ImageLocation::fromUrl(url); }
    friend bool operator==(const DisplayedImageLocation& left, const DisplayedImageLocation& right)
    {
        return left.m_image == right.m_image
            && left.m_openedCollectionScope == right.m_openedCollectionScope;
    }

private:
    ImageLocation m_image;
    OpenedCollectionScopeLocation m_openedCollectionScope;
};

QString displayScopeIdentityForLocation(const DisplayedImageLocation& location);

}

#endif
