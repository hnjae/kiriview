// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATELISTSOURCE_H
#define KIRIVIEW_IMAGEDOCUMENTPAGECANDIDATELISTSOURCE_H

#include "imagedocumentpagenavigationtypes.h"
#include "location/imagelocation.h"

#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace kiriview {
class ImageDocumentPageCandidateListSource
{
public:
    struct Directory
    {
        QUrl directoryUrl;
    };

    struct OpenedCollectionScope
    {
        OpenedCollectionScopeLocation openedCollectionScope;
    };

    static ImageDocumentPageCandidateListSource forDirectory(QUrl directoryUrl);
    static ImageDocumentPageCandidateListSource forOpenedCollectionScope(
        OpenedCollectionScopeLocation openedCollectionScope);

    [[nodiscard]] OpenedCollectionScopeLocation openedCollectionScope() const;

    template <typename Visitor> decltype(auto) visit(Visitor&& visitor) const
    {
        return std::visit(std::forward<Visitor>(visitor), m_source);
    }

private:
    using Payload = std::variant<Directory, OpenedCollectionScope>;

    explicit ImageDocumentPageCandidateListSource(Payload source);

    Payload m_source;
};

bool sameImageDocumentPageCandidateListSource(const ImageDocumentPageCandidateListSource& left,
    const ImageDocumentPageCandidateListSource& right);

class ImageDocumentPageCandidateListContext
{
public:
    using DirectoryContext = ImageDocumentPageCandidateListSource::Directory;
    using OpenedCollectionScopeContext
        = ImageDocumentPageCandidateListSource::OpenedCollectionScope;

    static ImageDocumentPageCandidateListContext forDirectory(QUrl currentUrl, QUrl directoryUrl);
    static ImageDocumentPageCandidateListContext forOpenedCollectionScope(
        QUrl currentUrl, OpenedCollectionScopeLocation openedCollectionScope);
    static ImageDocumentPageCandidateListContext forSource(
        QUrl currentUrl, ImageDocumentPageCandidateListSource source);

    [[nodiscard]] const QUrl& currentUrl() const;
    [[nodiscard]] const ImageDocumentPageCandidateListSource& source() const;
    [[nodiscard]] OpenedCollectionScopeLocation openedCollectionScope() const;

    template <typename Visitor> decltype(auto) visit(Visitor&& visitor) const
    {
        return m_source.visit(std::forward<Visitor>(visitor));
    }

private:
    explicit ImageDocumentPageCandidateListContext(
        QUrl currentUrl, ImageDocumentPageCandidateListSource source);

    QUrl m_currentUrl;
    ImageDocumentPageCandidateListSource m_source;
};

using ImageDocumentPageCandidateRows = std::vector<ImageDocumentPageCandidate>;

struct ImageDocumentPageCandidateListSnapshot
{
    std::optional<ImageDocumentPageCandidateListSource> source;
    quint64 revision = 0;
    std::shared_ptr<const ImageDocumentPageCandidateRows> candidates
        = std::make_shared<const ImageDocumentPageCandidateRows>();
    bool known = false;
};

struct ImageDocumentPageCandidateListSnapshotResult
{
    ImageDocumentPageCandidateListSnapshot snapshot;
    bool succeeded = false;
    QString errorString;
};

using ImageDocumentPageCandidateListSnapshotCallback
    = std::function<void(ImageDocumentPageCandidateListSnapshotResult)>;

bool imageDocumentPageCandidateListSnapshotMatchesSource(
    const ImageDocumentPageCandidateListSnapshot& snapshot,
    const ImageDocumentPageCandidateListSource& source);
const ImageDocumentPageCandidateRows& imageDocumentPageCandidateRows(
    const ImageDocumentPageCandidateListSnapshot& snapshot);
std::optional<ImageDocumentPageCandidateListContext>
imageDocumentPageCandidateListContextForDisplayedImage(const DisplayedImageLocation& location);
}

#endif
