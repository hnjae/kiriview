// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationthumbnailprojection.h"

#include "navigation/mediaformatregistry.h"

#include <cstddef>
#include <optional>

namespace {
kiriview::ActiveNavigationThumbnailKind thumbnailKindForDirectMediaNavigationCandidate(
    const kiriview::DirectMediaNavigationCandidate& candidate)
{
    return kiriview::isSupportedDirectVideoFileName(candidate.name)
            || kiriview::isSupportedDirectVideoUrl(candidate.url)
        ? kiriview::ActiveNavigationThumbnailKind::Video
        : kiriview::ActiveNavigationThumbnailKind::Image;
}

kiriview::ActiveNavigationThumbnailKind thumbnailKindForImageDocumentPageCandidate(
    const kiriview::ImageDocumentPageCandidate& candidate)
{
    return candidate.kind == kiriview::ImageDocumentPageKind::Video
        ? kiriview::ActiveNavigationThumbnailKind::Video
        : kiriview::ActiveNavigationThumbnailKind::Image;
}

QString thumbnailLabel(const QString& candidateName, const QUrl& url)
{
    return candidateName.isEmpty() ? url.fileName(QUrl::PrettyDecoded) : candidateName;
}

std::vector<kiriview::ActiveNavigationThumbnailRow> thumbnailRowsForDirectMediaNavigationCandidates(
    const std::vector<kiriview::DirectMediaNavigationCandidate>& candidates, int currentNumber)
{
    std::vector<kiriview::ActiveNavigationThumbnailRow> rows;
    rows.reserve(candidates.size());

    int number = 1;
    for (const kiriview::DirectMediaNavigationCandidate& candidate : candidates) {
        const kiriview::ActiveNavigationThumbnailKind kind
            = thumbnailKindForDirectMediaNavigationCandidate(candidate);
        rows.push_back(kiriview::ActiveNavigationThumbnailRow {
            number,
            candidate.url,
            thumbnailLabel(candidate.name, candidate.url),
            kind,
            kind == kiriview::ActiveNavigationThumbnailKind::Video
                ? kiriview::ActiveNavigationThumbnailSourceKind::DirectVideo
                : kiriview::ActiveNavigationThumbnailSourceKind::DirectImage,
            number == currentNumber,
        });
        ++number;
    }

    return rows;
}

std::vector<kiriview::ActiveNavigationThumbnailRow>
thumbnailRowsForImageDocumentPageCandidateListSnapshot(
    const kiriview::ImageDocumentPageCandidateListSnapshot& snapshot, int currentNumber)
{
    const kiriview::ImageDocumentPageCandidateRows& candidates
        = kiriview::imageDocumentPageCandidateRows(snapshot);
    std::vector<kiriview::ActiveNavigationThumbnailRow> rows;
    rows.reserve(candidates.size());

    int number = 1;
    for (const kiriview::ImageDocumentPageCandidate& candidate : candidates) {
        const kiriview::ActiveNavigationThumbnailKind kind
            = thumbnailKindForImageDocumentPageCandidate(candidate);
        rows.push_back(kiriview::ActiveNavigationThumbnailRow {
            number,
            candidate.url,
            thumbnailLabel(candidate.name, candidate.url),
            kind,
            kind == kiriview::ActiveNavigationThumbnailKind::Video
                ? kiriview::ActiveNavigationThumbnailSourceKind::ImageDocumentPageVideo
                : kiriview::ActiveNavigationThumbnailSourceKind::ImageDocumentPageImage,
            number == currentNumber,
        });
        ++number;
    }

    return rows;
}

bool activeNavigationThumbnailNavigationAvailable(kiriview::ActiveNavigationSnapshot navigation)
{
    return navigation.available && navigation.known && navigation.count > 0;
}
}

namespace kiriview {
QString activeNavigationThumbnailPageKindIdentity(ActiveNavigationThumbnailKind kind)
{
    switch (kind) {
    case ActiveNavigationThumbnailKind::Image:
        return QStringLiteral("image");
    case ActiveNavigationThumbnailKind::Video:
        return QStringLiteral("video");
    }

    return QStringLiteral("image");
}

QString activeNavigationThumbnailSourceKindIdentity(ActiveNavigationThumbnailSourceKind sourceKind)
{
    switch (sourceKind) {
    case ActiveNavigationThumbnailSourceKind::DirectImage:
        return QStringLiteral("direct-image");
    case ActiveNavigationThumbnailSourceKind::DirectVideo:
        return QStringLiteral("direct-video");
    case ActiveNavigationThumbnailSourceKind::ImageDocumentPageImage:
        return QStringLiteral("image-document-page-image");
    case ActiveNavigationThumbnailSourceKind::ImageDocumentPageVideo:
        return QStringLiteral("image-document-page-video");
    }

    return QStringLiteral("direct-image");
}

bool sameActiveNavigationThumbnailRowSetIdentity(
    const ActiveNavigationThumbnailRowSetIdentity& left,
    const ActiveNavigationThumbnailRowSetIdentity& right)
{
    if (left.known != right.known || left.sourceKind != right.sourceKind
        || left.candidateRevision != right.candidateRevision || left.count != right.count) {
        return false;
    }

    if (!left.known) {
        return true;
    }

    switch (left.sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia:
        return left.directMediaSource == right.directMediaSource;
    case ActiveNavigationSourceKind::ImageDocumentPages:
        return left.imageDocumentPageSource.has_value() && right.imageDocumentPageSource.has_value()
            && sameImageDocumentPageCandidateListSource(
                *left.imageDocumentPageSource, *right.imageDocumentPageSource);
    case ActiveNavigationSourceKind::None:
        return true;
    }

    return false;
}

std::optional<ActiveNavigationThumbnailRowSetIdentity> activeNavigationThumbnailRowSetIdentity(
    ActiveNavigationSourceKind sourceKind, ActiveNavigationSnapshot navigation,
    const DirectMediaNavigationCandidateSnapshot& directMediaNavigationCandidateSnapshot,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
{
    if (!activeNavigationThumbnailNavigationAvailable(navigation)) {
        return std::nullopt;
    }

    switch (sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia: {
        const DirectMediaNavigationCandidateRows& rows
            = directMediaNavigationCandidateRows(directMediaNavigationCandidateSnapshot);
        if (!directMediaNavigationCandidateSnapshot.known
            || !directMediaNavigationCandidateSnapshot.source.has_value()
            || static_cast<int>(rows.size()) != navigation.count) {
            return std::nullopt;
        }

        return ActiveNavigationThumbnailRowSetIdentity {
            sourceKind,
            directMediaNavigationCandidateSnapshot.source,
            std::nullopt,
            directMediaNavigationCandidateSnapshot.revision,
            navigation.count,
            true,
        };
    }
    case ActiveNavigationSourceKind::ImageDocumentPages: {
        const ImageDocumentPageCandidateRows& rows
            = imageDocumentPageCandidateRows(imageDocumentPageCandidateSnapshot);
        if (!imageDocumentPageCandidateSnapshot.known
            || !imageDocumentPageCandidateSnapshot.source.has_value()
            || static_cast<int>(rows.size()) != navigation.count) {
            return std::nullopt;
        }

        return ActiveNavigationThumbnailRowSetIdentity {
            sourceKind,
            {},
            imageDocumentPageCandidateSnapshot.source,
            imageDocumentPageCandidateSnapshot.revision,
            navigation.count,
            true,
        };
    }
    case ActiveNavigationSourceKind::None:
        return std::nullopt;
    }

    return std::nullopt;
}

std::vector<ActiveNavigationThumbnailRow> projectActiveNavigationThumbnailRows(
    ActiveNavigationSourceKind sourceKind, ActiveNavigationSnapshot navigation,
    const DirectMediaNavigationCandidateSnapshot& directMediaNavigationCandidateSnapshot,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
{
    if (!activeNavigationThumbnailRowSetIdentity(sourceKind, navigation,
            directMediaNavigationCandidateSnapshot, imageDocumentPageCandidateSnapshot)
            .has_value()) {
        return {};
    }

    std::vector<ActiveNavigationThumbnailRow> rows;
    switch (sourceKind) {
    case ActiveNavigationSourceKind::OrdinaryDirectMedia:
        rows = thumbnailRowsForDirectMediaNavigationCandidates(
            directMediaNavigationCandidateRows(directMediaNavigationCandidateSnapshot),
            navigation.currentNumber);
        break;
    case ActiveNavigationSourceKind::ImageDocumentPages:
        rows = thumbnailRowsForImageDocumentPageCandidateListSnapshot(
            imageDocumentPageCandidateSnapshot, navigation.currentNumber);
        break;
    case ActiveNavigationSourceKind::None:
        break;
    }

    if (static_cast<int>(rows.size()) != navigation.count) {
        return {};
    }

    return rows;
}
}
