// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidatelistsource.h"

#include "location/imagedocumentlocation.h"
#include "location/imageurl.h"

#include <memory>
#include <optional>
#include <utility>

namespace {
bool sameImageDocumentPageCandidateListSourcePayload(
    const kiriview::ImageDocumentPageCandidateListSource::Directory& left,
    const kiriview::ImageDocumentPageCandidateListSource::Directory& right)
{
    const kiriview::SourceKey leftKey = kiriview::sourceKeyForUrl(left.directoryUrl);
    const kiriview::SourceKey rightKey = kiriview::sourceKeyForUrl(right.directoryUrl);
    if (leftKey.valid || rightKey.valid) {
        return kiriview::sameSourceKey(leftKey, rightKey);
    }
    return kiriview::sameNormalizedUrl(left.directoryUrl, right.directoryUrl);
}

bool sameImageDocumentPageCandidateListSourcePayload(
    const kiriview::ImageDocumentPageCandidateListSource::OpenedCollectionScope& left,
    const kiriview::ImageDocumentPageCandidateListSource::OpenedCollectionScope& right)
{
    return kiriview::sameOpenedCollectionScopeLocation(
        left.openedCollectionScope, right.openedCollectionScope);
}

template <typename Left, typename Right>
bool sameImageDocumentPageCandidateListSourcePayload(const Left&, const Right&)
{
    return false;
}
}

namespace kiriview {
ImageDocumentPageCandidateListSource ImageDocumentPageCandidateListSource::forDirectory(
    QUrl directoryUrl)
{
    return ImageDocumentPageCandidateListSource { Directory { std::move(directoryUrl) } };
}

ImageDocumentPageCandidateListSource ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
    OpenedCollectionScopeLocation openedCollectionScope)
{
    return ImageDocumentPageCandidateListSource { OpenedCollectionScope {
        std::move(openedCollectionScope) } };
}

OpenedCollectionScopeLocation ImageDocumentPageCandidateListSource::openedCollectionScope() const
{
    const auto* archiveSource = std::get_if<OpenedCollectionScope>(&m_source);
    return archiveSource == nullptr ? OpenedCollectionScopeLocation::none()
                                    : archiveSource->openedCollectionScope;
}

ImageDocumentPageCandidateListSource::ImageDocumentPageCandidateListSource(Payload source)
    : m_source(std::move(source))
{
}

bool sameImageDocumentPageCandidateListSource(const ImageDocumentPageCandidateListSource& left,
    const ImageDocumentPageCandidateListSource& right)
{
    return left.visit([&right](const auto& leftSource) {
        return right.visit([&leftSource](const auto& rightSource) {
            return sameImageDocumentPageCandidateListSourcePayload(leftSource, rightSource);
        });
    });
}

ImageDocumentPageCandidateListContext ImageDocumentPageCandidateListContext::forDirectory(
    QUrl currentUrl, QUrl directoryUrl)
{
    return ImageDocumentPageCandidateListContext {
        std::move(currentUrl),
        ImageDocumentPageCandidateListSource::forDirectory(std::move(directoryUrl)),
    };
}

ImageDocumentPageCandidateListContext
ImageDocumentPageCandidateListContext::forOpenedCollectionScope(
    QUrl currentUrl, OpenedCollectionScopeLocation openedCollectionScope)
{
    return ImageDocumentPageCandidateListContext {
        std::move(currentUrl),
        ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
            std::move(openedCollectionScope)),
    };
}

ImageDocumentPageCandidateListContext ImageDocumentPageCandidateListContext::forSource(
    QUrl currentUrl, ImageDocumentPageCandidateListSource source)
{
    return ImageDocumentPageCandidateListContext {
        std::move(currentUrl),
        std::move(source),
    };
}

const QUrl& ImageDocumentPageCandidateListContext::currentUrl() const { return m_currentUrl; }

const ImageDocumentPageCandidateListSource& ImageDocumentPageCandidateListContext::source() const
{
    return m_source;
}

OpenedCollectionScopeLocation ImageDocumentPageCandidateListContext::openedCollectionScope() const
{
    return m_source.openedCollectionScope();
}

ImageDocumentPageCandidateListContext::ImageDocumentPageCandidateListContext(
    QUrl currentUrl, ImageDocumentPageCandidateListSource source)
    : m_currentUrl(std::move(currentUrl))
    , m_source(std::move(source))
{
}

bool imageDocumentPageCandidateListSnapshotMatchesSource(
    const ImageDocumentPageCandidateListSnapshot& snapshot,
    const ImageDocumentPageCandidateListSource& source)
{
    return snapshot.known && snapshot.source.has_value()
        && sameImageDocumentPageCandidateListSource(*snapshot.source, source);
}

const ImageDocumentPageCandidateRows& imageDocumentPageCandidateRows(
    const ImageDocumentPageCandidateListSnapshot& snapshot)
{
    static const ImageDocumentPageCandidateRows emptyRows;
    return snapshot.candidates != nullptr ? *snapshot.candidates : emptyRows;
}

std::optional<ImageDocumentPageCandidateListContext>
imageDocumentPageCandidateListContextForDisplayedImage(const DisplayedImageLocation& location)
{
    const QUrl& displayedUrl = location.imageUrl();
    if (displayedUrl.isEmpty()) {
        return std::nullopt;
    }

    if (displayedLocationIsInsideOpenedCollectionScope(location)) {
        const QUrl currentUrl = normalizedImageUrl(displayedUrl);
        if (!currentUrl.isValid()) {
            return std::nullopt;
        }

        return ImageDocumentPageCandidateListContext::forOpenedCollectionScope(
            currentUrl, location.openedCollectionScope());
    }

    if (location.directMediaPageScopeIdentity().has_value()) {
        const DirectMediaPageScopeIdentity& identity = *location.directMediaPageScopeIdentity();
        return ImageDocumentPageCandidateListContext::forDirectory(
            identity.currentKey().normalizedUrl, identity.parentNavigationUrl());
    }

    const DirectoryNavigationLocation navigationLocation
        = directoryNavigationLocationForSource(location.imageSource());
    if (!navigationLocation.isValid()) {
        return std::nullopt;
    }

    return ImageDocumentPageCandidateListContext::forDirectory(
        navigationLocation.fileUrl, navigationLocation.directoryUrl);
}
}
