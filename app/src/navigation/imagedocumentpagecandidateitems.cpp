// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidateitems.h"

#include "archive/archiveformat.h"
#include "decoding/imageformatregistry.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "imagedocumentpagenavigationpolicy.h"
#include "location/imageurl.h"
#include "mediaformatregistry.h"
#include "navigationlogging.h"

#include <QDebug>
#include <cstddef>
#include <expected>
#include <utility>

namespace {
constexpr qsizetype defaultMaximumOrdinarySiblingEntryCount = 65'536;
constexpr qsizetype defaultMaximumOrdinarySiblingIdentityCodeUnitCount
    = qsizetype { 8 } * 1024 * 1024;

std::expected<void, kiriview::ImageDocumentPageCandidateAdmissionFailure>
admitImageDocumentPageCandidateItems(
    const KFileItemList& items, kiriview::ImageDocumentPageCandidateAdmissionLimits limits)
{
    using Failure = kiriview::ImageDocumentPageCandidateAdmissionFailure;

    if (limits.maximumEntryCount < 0 || limits.maximumRetainedIdentityCodeUnitCount < 0
        || items.size() > limits.maximumEntryCount) {
        return std::unexpected(Failure::ResourceLimitExceeded);
    }

    qsizetype retainedIdentityCodeUnits = 0;
    for (const KFileItem& item : items) {
        const qsizetype nameCodeUnits = item.name().size();
        const qsizetype urlCodeUnits = item.url().toString(QUrl::FullyEncoded).size();
        if (nameCodeUnits < 0 || urlCodeUnits < 0
            || nameCodeUnits
                > limits.maximumRetainedIdentityCodeUnitCount - retainedIdentityCodeUnits) {
            return std::unexpected(Failure::ResourceLimitExceeded);
        }
        retainedIdentityCodeUnits += nameCodeUnits;
        if (urlCodeUnits
            > limits.maximumRetainedIdentityCodeUnitCount - retainedIdentityCodeUnits) {
            return std::unexpected(Failure::ResourceLimitExceeded);
        }
        retainedIdentityCodeUnits += urlCodeUnits;
    }

    return {};
}
}

namespace kiriview {
ImageDocumentPageCandidateAdmissionLimits defaultImageDocumentPageCandidateAdmissionLimits()
{
    return ImageDocumentPageCandidateAdmissionLimits { defaultMaximumOrdinarySiblingEntryCount,
        defaultMaximumOrdinarySiblingIdentityCodeUnitCount };
}

ImageDocumentPageCandidateAdmissionResult imageDocumentPageNavigationCandidates(
    const KFileItemList& items, ImageDocumentPageCandidateAdmissionLimits limits)
{
    if (const auto admitted = admitImageDocumentPageCandidateItems(items, limits); !admitted) {
        return std::unexpected(admitted.error());
    }

    std::vector<ImageDocumentPageCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(items.size()));

    for (const KFileItem& item : items) {
        const QString name = item.name();
        if (!item.isFile() || !kiriview::isSupportedOrdinaryMediaFileName(name)) {
            continue;
        }

        candidates.push_back(ImageDocumentPageCandidate { item.url(), name,
            kiriview::isSupportedDirectVideoFileName(name) ? ImageDocumentPageKind::Video
                                                           : ImageDocumentPageKind::Image });
    }

    sortImageDocumentPageCandidates(&candidates);
    return candidates;
}

DirectMediaNavigationCandidateAdmissionResult directMediaNavigationCandidates(
    const KFileItemList& items, ImageDocumentPageCandidateAdmissionLimits limits)
{
    ImageDocumentPageCandidateAdmissionResult admitted
        = imageDocumentPageNavigationCandidates(items, limits);
    if (!admitted) {
        return std::unexpected(admitted.error());
    }

    std::vector<ImageDocumentPageCandidate> imageDocumentPageCandidates = std::move(*admitted);
    std::vector<DirectMediaNavigationCandidate> candidates;
    candidates.reserve(imageDocumentPageCandidates.size());
    for (ImageDocumentPageCandidate& candidate : imageDocumentPageCandidates) {
        candidates.push_back(
            DirectMediaNavigationCandidate { std::move(candidate.url), std::move(candidate.name) });
    }

    qCDebug(kiriviewNavigationLog)
        << "direct media navigation candidates projected"
        << "items" << items.size() << "supportedCandidates" << candidates.size();
    for (std::size_t index = 0; index < candidates.size() && index < 8; ++index) {
        qCDebug(kiriviewNavigationLog)
            << "direct media navigation candidate"
            << "index" << index << "name" << diagnosticPathReference(candidates.at(index).name)
            << "url" << diagnosticSourceReference(candidates.at(index).url);
    }
    if (candidates.size() > 8) {
        qCDebug(kiriviewNavigationLog) << "direct media navigation candidates omitted"
                                       << "count" << candidates.size() - 8;
    }

    return candidates;
}

std::vector<ContainerNavigationCandidate> containerNavigationCandidates(const KFileItemList& items)
{
    std::vector<ContainerNavigationCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(items.size()));

    for (const KFileItem& item : items) {
        const QString name = item.name();
        if (item.isFile() && item.url().isLocalFile()
            && kiriview::isComicBookArchiveFileName(name)) {
            candidates.push_back(
                ContainerNavigationCandidate { normalizedFileContainerUrl(item.url()), name,
                    ContainerNavigationCandidateType::ComicBookArchive });
        }
    }

    sortContainerNavigationCandidates(&candidates);
    return candidates;
}
}
