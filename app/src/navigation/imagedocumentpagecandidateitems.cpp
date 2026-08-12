// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagedocumentpagecandidateitems.h"

#include "archive/archiveformat.h"
#include "decoding/imageformatregistry.h"
#include "diagnostics/diagnosticlogprojection.h"
#include "imagedocumentpagenavigationpolicy.h"
#include "location/imageurl.h"
#include "location/sourcekey.h"
#include "mediaformatregistry.h"
#include "navigationlogging.h"

#include <QDebug>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <utility>

namespace {
constexpr qsizetype defaultMaximumSiblingEntryCount = 65'536;
constexpr qsizetype defaultMaximumSiblingIdentityCodeUnitCount = qsizetype { 8 } * 1024 * 1024;

std::expected<void, kiriview::ImageDocumentPageCandidateAdmissionFailure>
admitSiblingCandidateItems(
    const KFileItemList& items, kiriview::SiblingCandidateAdmissionLimits limits)
{
    using Failure = kiriview::ImageDocumentPageCandidateAdmissionFailure;

    if (limits.maximumEntryCount < 0 || limits.maximumIdentityCodeUnitCount < 0
        || items.size() > limits.maximumEntryCount) {
        return std::unexpected(Failure::ResourceLimitExceeded);
    }

    qsizetype retainedIdentityCodeUnits = 0;
    for (const KFileItem& item : items) {
        const qsizetype nameCodeUnits = item.name().size();
        const qsizetype urlCodeUnits = item.url().toString(QUrl::FullyEncoded).size();
        if (nameCodeUnits < 0 || urlCodeUnits < 0
            || nameCodeUnits > limits.maximumIdentityCodeUnitCount - retainedIdentityCodeUnits) {
            return std::unexpected(Failure::ResourceLimitExceeded);
        }
        retainedIdentityCodeUnits += nameCodeUnits;
        if (urlCodeUnits > limits.maximumIdentityCodeUnitCount - retainedIdentityCodeUnits) {
            return std::unexpected(Failure::ResourceLimitExceeded);
        }
        retainedIdentityCodeUnits += urlCodeUnits;
    }

    return {};
}
}

namespace kiriview {
SiblingCandidateAdmissionLimits defaultSiblingCandidateAdmissionLimits()
{
    return SiblingCandidateAdmissionLimits { defaultMaximumSiblingEntryCount,
        defaultMaximumSiblingIdentityCodeUnitCount };
}

ImageDocumentPageCandidateAdmissionResult imageDocumentPageNavigationCandidates(
    const QUrl& directoryUrl, const KFileItemList& items, SiblingCandidateAdmissionLimits limits)
{
    if (const auto admitted = admitSiblingCandidateItems(items, limits); !admitted) {
        return std::unexpected(admitted.error());
    }

    std::vector<ImageDocumentPageCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(items.size()));

    for (const KFileItem& item : items) {
        const QString name = item.name();
        if (!item.isFile() || !kiriview::isSupportedOrdinaryMediaFileName(name)) {
            continue;
        }
        if (!sourceBelongsToDirectoryScope(item.url(), directoryUrl)) {
            return std::unexpected(ImageDocumentPageCandidateAdmissionFailure::ScopeViolation);
        }

        candidates.push_back(ImageDocumentPageCandidate { item.url(), name,
            kiriview::isSupportedDirectVideoFileName(name) ? ImageDocumentPageKind::Video
                                                           : ImageDocumentPageKind::Image });
    }

    sortImageDocumentPageCandidates(&candidates);
    return candidates;
}

DirectMediaNavigationCandidateAdmissionResult directMediaNavigationCandidates(
    const QUrl& directoryUrl, const KFileItemList& items, SiblingCandidateAdmissionLimits limits)
{
    ImageDocumentPageCandidateAdmissionResult admitted
        = imageDocumentPageNavigationCandidates(directoryUrl, items, limits);
    if (!admitted) {
        return std::unexpected(admitted.error());
    }

    std::vector<ImageDocumentPageCandidate> imageDocumentPageCandidates = std::move(*admitted);
    std::vector<DirectMediaNavigationCandidate> candidates;
    candidates.reserve(imageDocumentPageCandidates.size());
    for (ImageDocumentPageCandidate& candidate : imageDocumentPageCandidates) {
        candidates.push_back(DirectMediaNavigationCandidate {
            std::move(candidate.url), std::move(candidate.name), candidate.sourceFreshness });
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

ContainerNavigationCandidateAdmissionResult containerNavigationCandidates(
    const QUrl& directoryUrl, const KFileItemList& items, SiblingCandidateAdmissionLimits limits)
{
    if (const auto admitted = admitSiblingCandidateItems(items, limits); !admitted) {
        return std::unexpected(admitted.error());
    }

    std::vector<ContainerNavigationCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(items.size()));

    for (const KFileItem& item : items) {
        const QString name = item.name();
        if (item.isFile() && item.url().isLocalFile()
            && kiriview::isComicBookArchiveFileName(name)) {
            if (!sourceBelongsToDirectoryScope(item.url(), directoryUrl)) {
                return std::unexpected(ImageDocumentPageCandidateAdmissionFailure::ScopeViolation);
            }
            candidates.push_back(
                ContainerNavigationCandidate { normalizedFileContainerUrl(item.url()), name,
                    ContainerNavigationCandidateType::ComicBookArchive });
        }
    }

    sortContainerNavigationCandidates(&candidates);
    return candidates;
}

bool imageDocumentPageCandidatesBelongToDirectoryScope(
    const std::vector<ImageDocumentPageCandidate>& candidates, const QUrl& directoryUrl)
{
    return std::ranges::all_of(
        candidates, [&directoryUrl](const ImageDocumentPageCandidate& candidate) {
            return sourceBelongsToDirectoryScope(candidate.url, directoryUrl);
        });
}

bool containerNavigationCandidatesBelongToDirectoryScope(
    const std::vector<ContainerNavigationCandidate>& candidates, const QUrl& directoryUrl)
{
    return std::ranges::all_of(
        candidates, [&directoryUrl](const ContainerNavigationCandidate& candidate) {
            QUrl candidateUrl = candidate.url;
            if (candidate.type == ContainerNavigationCandidateType::Directory) {
                QString path = candidateUrl.path();
                while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
                    path.chop(1);
                }
                candidateUrl.setPath(path);
            }
            return sourceBelongsToDirectoryScope(candidateUrl, directoryUrl);
        });
}
}
